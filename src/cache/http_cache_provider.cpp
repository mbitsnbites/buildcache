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

#include <base/compressor.hpp>
#include <base/debug_utils.hpp>
#include <base/file_utils.hpp>
#include <cache/http_cache_provider.hpp>
#include <config/configuration.hpp>

#ifdef _WIN32
#undef NOMINMAX
#include <HTTPRequest.hpp>
#undef log
#undef ERROR
#else
#include <HTTPRequest.hpp>
#endif

#include <sstream>
#include <stdexcept>

namespace bcache {

namespace {
// Name of the cache entry file.
const std::string CACHE_ENTRY_FILE_NAME = ".entry";

// The prefix (namespace) for BuildCache keys.
const std::string KEY_PREFIX = "buildcache";

std::string remote_key_name(const std::string& hash_str, const std::string& file) {
  return KEY_PREFIX + "_" + hash_str + "_" + file;
}

}  // namespace

http_cache_provider_t::http_cache_provider_t() {
}

http_cache_provider_t::~http_cache_provider_t() {
  disconnect();
}

bool http_cache_provider_t::connect(const std::string& host_description) {
  // Decode the host description.
  if (!parse_host_description(host_description, m_host, m_port, m_path)) {
    return false;
  }

  // The default port for HTTP is 80 (other services, such as MinIO, may use other ports).
  if (m_port < 0) {
    m_port = 80;
  }

  // If the path is non-empty, it needs to start with a slash.
  if ((!m_path.empty()) && m_path[0] != '/') {
    m_path = "/" + m_path;
  }

  m_ready_for_action = true;
  return true;
}

bool http_cache_provider_t::is_connected() const {
  return m_ready_for_action;
}

void http_cache_provider_t::disconnect() {
  m_ready_for_action = false;
}

std::string http_cache_provider_t::get_object_url(const std::string& key) {
  std::ostringstream ss;
  ss << "http://" << m_host << ":" << m_port << m_path << "/" << key;
  return ss.str();
}

cache_entry_t http_cache_provider_t::lookup(const hasher_t::hash_t& hash) {
  const auto key = remote_key_name(hash.as_string(), CACHE_ENTRY_FILE_NAME);
  try {
    // Try to get the cache entry item from the remote cache.
    return cache_entry_t::deserialize(get_data(key));
  } catch (const std::exception& e) {
    // We most likely had a cache miss.
    debug::log(debug::log_level_t::DEBUG) << e.what();
    return cache_entry_t();
  }
}

void http_cache_provider_t::add(const hasher_t::hash_t& hash,
                                const cache_entry_t& entry,
                                const std::map<std::string, expected_file_t>& expected_files) {
  const auto hash_str = hash.as_string();

  // Upload (and optinally compress) the files to the remote cache.
  for (const auto& file_id : entry.file_ids()) {
    const auto& source_path = expected_files.at(file_id).path();

    // Read the data from the source file.
    auto data = file::read(source_path);

    // Compress?
    if (entry.compression_mode() == cache_entry_t::comp_mode_t::ALL) {
      debug::log(debug::DEBUG) << "Compressing " << source_path << "...";
      data = comp::compress(data);
    }

    // Upload the data.
    const auto key = remote_key_name(hash_str, file_id);
    set_data(key, data);
  }

  // Create a cache entry file.
  const auto key = remote_key_name(hash_str, CACHE_ENTRY_FILE_NAME);
  set_data(key, entry.serialize());
}

void http_cache_provider_t::get_file(const hasher_t::hash_t& hash,
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

std::string http_cache_provider_t::get_path() const {
  return m_path;
}

std::vector<std::string> http_cache_provider_t::get_header(const std::string& /* method */,
                                                           const std::string& /* key */) const {
  const std::string content_type = "application/octet-stream";
  return {"Content-Type: " + content_type};
}

std::string http_cache_provider_t::get_data(const std::string& key) {
  if (!is_connected()) {
    throw std::runtime_error("Can't GET from a disconnected context");
  }

  // Gather information for this request.
  const std::string method = "GET";
  const auto http_header = get_header(method, key);

  // Perform the HTTP request.
  const auto url = get_object_url(key);
  http::Request request(url);
  http::Response response = request.send(method, "", http_header);

  // A successful response must have the code 200.
  if (response.status != http::Response::Ok) {
    std::ostringstream ss;
    if (response.status == http::Response::NotFound) {
      ss << "File not found on HTTP remote: " << key;
    } else {
      ss << "HTTP remote responded (" << response.status << "): " << response.body.data()
         << " (URL: " << url << ")";
    }
    throw std::runtime_error(ss.str());
  }

  std::string data(response.body.begin(), response.body.end());
  debug::log(debug::DEBUG) << "Completed HTTP GET request: " << url << " (" << data.size()
                           << " bytes)";

  return data;
}

void http_cache_provider_t::set_data(const std::string& key, const std::string& data) {
  if (!is_connected()) {
    throw std::runtime_error("Can't PUT to a disconnected context");
  }

  // Gather information for this request.
  const std::string method = "PUT";
  auto http_header = get_header(method, key);

  // Perform the HTTP request.
  const auto url = get_object_url(key);
  http::Request request(url);
  http::Response response = request.send(method, data, http_header);

  // A successful response must have the code 200 or 201.
  if (response.status != http::Response::Ok && response.status != http::Response::Created) {
    std::ostringstream ss;
    ss << "HTTP remote responded (" << response.status << "): " << response.body.data()
       << " (URL: " << url << ")";
    throw std::runtime_error(ss.str());
  }

  debug::log(debug::DEBUG) << "Completed HTTP PUT request: " << url;
}

}  // namespace bcache
