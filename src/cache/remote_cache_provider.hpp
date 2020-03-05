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

#ifndef BUILDCACHE_REMOTE_CACHE_PROVIDER_HPP_
#define BUILDCACHE_REMOTE_CACHE_PROVIDER_HPP_

#include <base/hasher.hpp>
#include <cache/cache_entry.hpp>
#include <cache/expected_file.hpp>

#include <string>

namespace bcache {

class remote_cache_provider_t {
public:
  /// @brief De-initialzie the remote cache object.
  virtual ~remote_cache_provider_t();

  /// @brief Connect to the remote cache.
  /// @param host_description A string describing the host to connect to (excluding protocol).
  /// @returns true if the connection was successful.
  virtual bool connect(const std::string& host_description) = 0;

  /// @brief Check if we have a connection to the remote cache.
  /// @returns true if we have a connection to the remote cache.
  virtual bool is_connected() const = 0;

  /// @brief Check if an entry exists in the cache.
  /// @returns A cache entry struct. If there was no cache hit the entry will be empty.
  virtual cache_entry_t lookup(const hasher_t::hash_t& hash) = 0;

  /// @brief Adds a set of files to the cache.
  /// @param hash The cache entry identifier.
  /// @param entry The cache entry data (files, stdout, etc).
  /// @param expected_files Paths to the actual files in the local file system (map from file ID to
  /// an expected file descriptor).
  virtual void add(const hasher_t::hash_t& hash,
                   const cache_entry_t& entry,
                   const std::map<std::string, expected_file_t>& expected_files) = 0;

  /// @brief Copy a cached file to the local file system.
  /// @param hash The cache entry identifier.
  /// @param source_id The ID of the remote file to copy.
  /// @param target_path The path to the local file.
  /// @param is_compressed True if the remote data is compressed.
  virtual void get_file(const hasher_t::hash_t& hash,
                        const std::string& source_id,
                        const std::string& target_path,
                        const bool is_compressed) = 0;

protected:
  // Constructor called by child classes.
  remote_cache_provider_t();

  /// @brief Parse a host description string.
  /// @param host_description The host description string.
  /// @param[out] host The host name.
  /// @param[out] port The port (optional - defaults to -1).
  /// @param[out] path The path (optional - defaults to "").
  /// @returns true if the parser was successful, or false if it failed.
  static bool parse_host_description(const std::string& host_description,
                                     std::string& host,
                                     int& port,
                                     std::string& path);

  /// @brief Get the timeout for remote connections.
  /// @returns the timeout, in milliseconds.
  static int connection_timeout_ms();

  /// @brief Get the timeout for remote transfers.
  /// @returns the timeout, in milliseconds.
  static int transfer_timeout_ms();

private:
  // Prohibit copy & assignment.
  remote_cache_provider_t(const remote_cache_provider_t&) = delete;
  remote_cache_provider_t& operator=(const remote_cache_provider_t&) = delete;
};

}  // namespace bcache

#endif  // BUILDCACHE_REMOTE_CACHE_PROVIDER_HPP_
