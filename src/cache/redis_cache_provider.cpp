//--------------------------------------------------------------------------------------------------
// Copyright (c) 2019 Marcus Geelnard
//
// This software is provided 'as-is', without any express or implied warranty. In no event will the
// authors be held liable for any damages arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose, including commercial
// applications, and to alter it and redistribute it freely, subject to the following restrictions:
//
//  1. The origin of this software must not be misrepresented; you must not claim that you wrote
//     the original software. If you use this software in a product, an acknowledgment in the
//     product documentation would be appreciated but is not required.
//
//  2. Altered source versions must be plainly marked as such, and must not be misrepresented as
//     being the original software.
//
//  3. This notice may not be removed or altered from any source distribution.
//--------------------------------------------------------------------------------------------------

#include <cache/redis_cache_provider.hpp>

#include <base/compressor.hpp>
#include <base/debug_utils.hpp>
#include <base/file_utils.hpp>
#include <base/string_list.hpp>
#include <config/configuration.hpp>

#include <hiredis/hiredis.h>

#include <stdexcept>

namespace bcache {
namespace {
// Name of the cache entry file.
const std::string CACHE_ENTRY_FILE_NAME = ".entry";

// The prefix (namespace) for BuildCache database keys.
const std::string DB_PREFIX = "buildcache";

bool get_remote_server(const std::string& host_description, std::string& host, int& port) {
  // Split the host_id into host and port.
  auto parts = string_list_t(host_description, ":");
  if (parts.size() != 2) {
    debug::log(debug::log_level_t::ERROR)
        << "Invalid remote address: \"" << host_description << "\"";
    return false;
  }

  // Extract the host name / IP.
  host = std::move(parts[0]);
  if (host.size() < 1) {
    debug::log(debug::log_level_t::ERROR) << "Invalid remote host name: \"" << host << "\"";
    return false;
  }

  // Extract the port.
  try {
    port = std::stoi(parts[1]);
  } catch (std::exception& e) {
    debug::log(debug::log_level_t::ERROR)
        << "Invalid remote address port: \"" << parts[1] << "\" (" << e.what() << ")";
    return false;
  }

  return true;
}

std::string remote_key_name(const std::string& hash_str, const std::string& file) {
  return DB_PREFIX + "_" + hash_str + "_" + file;
}
}  // namespace

redis_cache_provider_t::redis_cache_provider_t() : remote_cache_provider_t() {
}

redis_cache_provider_t::~redis_cache_provider_t() {
  disconnect();
}

bool redis_cache_provider_t::connect(const std::string& host_description) {
  if (is_connected()) {
    return true;
  }

  // Decode the host description.
  std::string host;
  int port;
  if (!get_remote_server(host_description, host, port)) {
    return false;
  }

  // Connect to the remote Redis instance.
  m_ctx = redisConnect(host.c_str(), port);
  if (m_ctx == nullptr || m_ctx->err) {
    if (m_ctx != nullptr) {
      debug::log(debug::log_level_t::ERROR) << "Failed Connection: " << m_ctx->errstr;
      disconnect();
    } else {
      debug::log(debug::log_level_t::ERROR) << "Unable to allocate redis context";
    }
    return false;
  }

  return true;
}

bool redis_cache_provider_t::is_connected() const {
  return m_ctx != nullptr;
}

void redis_cache_provider_t::disconnect() {
  if (m_ctx != nullptr) {
    redisFree(m_ctx);
    m_ctx = nullptr;
  }
}

cache_t::entry_t bcache::redis_cache_provider_t::lookup(const hasher_t::hash_t& hash) {
  const auto key = remote_key_name(hash.as_string(), CACHE_ENTRY_FILE_NAME);
  try {
    // Try to get the cache entry item from the remote cache.
    const auto data = get_data(key);
    return cache_t::deserialize_entry(data);
  } catch (const std::exception& e) {
    // We most likely had a cache miss.
    debug::log(debug::log_level_t::DEBUG) << e.what();
    return cache_t::entry_t();
  }
}

void redis_cache_provider_t::add(const hasher_t::hash_t& hash, const cache_t::entry_t& entry) {
  const auto hash_str = hash.as_string();

  // Upload (and optinally compress) the files to the remote cache.
  for (const auto& file : entry.files) {
    const auto& file_id = file.first;
    const auto& source_path = file.second;

    // Read the data from the source file.
    auto data = file::read(source_path);

    // Compress?
    if (entry.compression_mode == cache_t::entry_t::comp_mode_t::ALL) {
      debug::log(debug::DEBUG) << "Compressing " << source_path << "...";
      data = comp::compress(data);
    }

    // Upload the data.
    const auto key = remote_key_name(hash_str, file_id);
    set_data(key, data);
  }

  // Create a cache entry file.
  const auto key = remote_key_name(hash_str, CACHE_ENTRY_FILE_NAME);
  set_data(key, cache_t::serialize_entry(entry));
}

void redis_cache_provider_t::get_file(const hasher_t::hash_t& hash,
                                      const std::string& source_id,
                                      const std::string& target_path,
                                      const bool is_compressed) {
  const auto key = remote_key_name(hash.as_string(), source_id);
  auto data = get_data(key);
  if (is_compressed) {
    data = comp::decompress(data);
  }
  file::write(data, target_path);
}

std::string redis_cache_provider_t::get_data(const std::string& key) {
  if (!is_connected()) {
    throw std::runtime_error("Can't GET from a disconnected context");
  }

  // Make a synchronous GET request.
  auto* reply_ptr = redisCommand(m_ctx, "GET %s", key.c_str());

  std::string data;
  bool success = false;
  if (reply_ptr != nullptr) {
    // Interpret the result.
    auto* reply = reinterpret_cast<redisReply*>(reply_ptr);

    if (reply->type == REDIS_REPLY_STRING) {
      data = std::string(reply->str, reply->len);
      debug::log(debug::log_level_t::DEBUG)
          << "Downloaded " << data.size() << " bytes from the remote cache";
      success = true;
    } else if (reply->type == REDIS_REPLY_ERROR) {
      data = std::string("Remote cache reply error: ") + std::string(reply->str, reply->len);
    } else if (reply->type == REDIS_REPLY_NIL) {
      // This is what happens if we have a cache miss.
      data = std::string("Remote cache miss: ") + key;
    } else {
      data = std::string("Unexpected remote cache reply type: ") + std::to_string(reply->type);
    }

    freeReplyObject(reply);
  } else {
    // The command failed.
    data = std::string("Remote cache GET error: ") + std::string(m_ctx->errstr);
    disconnect();
  }

  if (!success) {
    throw std::runtime_error(data);
  }
  return data;
}

void redis_cache_provider_t::set_data(const std::string& key, const std::string& data) {
  if (!is_connected()) {
    throw std::runtime_error("Can't SET to a disconnected context");
  }

  // Make a synchronous SET request.
  auto* reply_ptr = redisCommand(m_ctx, "SET %s %b", key.c_str(), data.c_str(), data.size());

  bool success = false;
  std::string err;
  if (reply_ptr != nullptr) {
    // Interpret the result.
    auto* reply = reinterpret_cast<redisReply*>(reply_ptr);
    if (reply->type == REDIS_REPLY_STATUS) {
      debug::log(debug::log_level_t::DEBUG)
          << "Uploaded " << data.size() << " bytes to the remote cache";
      success = true;
    } else if (reply->type == REDIS_REPLY_ERROR) {
      err = std::string("Remote cache reply error: ") + std::string(reply->str, reply->len);
    } else {
      err = std::string("Unexpected remote cache reply type: ") + std::to_string(reply->type);
    }

    freeReplyObject(reply);
  } else {
    // The command failed.
    err = std::string("Remote cache GET error: ") + std::string(m_ctx->errstr);
    disconnect();
  }

  if (!success) {
    throw std::runtime_error(err);
  }
}
}  // namespace bcache