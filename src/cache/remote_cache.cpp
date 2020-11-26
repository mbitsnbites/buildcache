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

#include <cache/remote_cache.hpp>

#include <base/debug_utils.hpp>
#include <cache/http_cache_provider.hpp>
#include <cache/redis_cache_provider.hpp>
#include <config/configuration.hpp>

#ifdef ENABLE_S3
#include <cache/s3_cache_provider.hpp>
#endif

namespace bcache {
namespace {
bool get_host_description(std::string& protocol, std::string& host_description) {
  const auto& remote_address = config::remote();
  if (remote_address.empty()) {
    return false;
  }

  // The format of the remote address string is as follows: "protocol://host_description"
  const auto protocol_end = remote_address.find("://", 0, 3);
  if (protocol_end == std::string::npos) {
    debug::log(debug::log_level_t::ERROR) << "Invalid remote address: \"" << remote_address << "\"";
    return false;
  }

  // Split the address into protocol and host_description.
  protocol = remote_address.substr(0, protocol_end);
  const auto host_description_start = protocol_end + 3;
  host_description = remote_address.substr(host_description_start);

  return true;
}
}  // namespace

remote_cache_t::~remote_cache_t() {
  if (m_provider != nullptr) {
    delete m_provider;
  }
}

bool remote_cache_t::connect() {
  if (is_connected()) {
    return true;
  }

  // Get remote cache configuration.
  std::string protocol;
  std::string host_description;
  if (!get_host_description(protocol, host_description)) {
    return false;
  }

  // Select an apropriate cache provider.
  m_provider = nullptr;
  if (protocol == "http") {
    m_provider = new http_cache_provider_t();
  } else if (protocol == "redis") {
    m_provider = new redis_cache_provider_t();
#ifdef ENABLE_S3
  } else if (protocol == "s3") {
    m_provider = new s3_cache_provider_t();
#endif
  }
  if (m_provider == nullptr) {
    debug::log(debug::log_level_t::ERROR) << "Unsupported remote protocol: " << protocol;
    return false;
  }

  // Connect to the remote cache instance.
  if (!m_provider->connect(host_description)) {
    return false;
  }

  return true;
}

bool remote_cache_t::is_connected() const {
  return (m_provider != nullptr) && m_provider->is_connected();
}

cache_entry_t bcache::remote_cache_t::lookup(const hasher_t::hash_t& hash) {
  if (m_provider != nullptr) {
    return m_provider->lookup(hash);
  }
  return cache_entry_t();
}

void remote_cache_t::add(const hasher_t::hash_t& hash,
                         const cache_entry_t& entry,
                         const std::map<std::string, expected_file_t>& expected_files) {
  if (m_provider != nullptr) {
    try {
      m_provider->add(hash, entry, expected_files);
    } catch (std::exception& e) {
      debug::log(debug::ERROR) << e.what();
    }
  }
}

void remote_cache_t::get_file(const hasher_t::hash_t& hash,
                              const std::string& source_id,
                              const std::string& target_path,
                              const bool is_compressed) {
  if (m_provider != nullptr) {
    m_provider->get_file(hash, source_id, target_path, is_compressed);
  }
}

}  // namespace bcache
