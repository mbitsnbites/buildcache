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

#ifndef BUILDCACHE_REMOTE_CACHE_HPP_
#define BUILDCACHE_REMOTE_CACHE_HPP_

#include <base/hasher.hpp>
#include <cache/cache_entry.hpp>
#include <cache/expected_file.hpp>
#include <cache/remote_cache_provider.hpp>

#include <string>

namespace bcache {

class remote_cache_t {
public:
  /// @brief Initialize the remote cache object.
  remote_cache_t() {
  }

  /// @brief De-initialzie the remote cache object.
  ~remote_cache_t();

  /// @brief Connect to the remote cache.
  /// @returns true if the connection was successful.
  bool connect();

  /// @brief Check if we have a connection to the remote cache.
  /// @returns true if we have a connection to the remote cache.
  bool is_connected() const;

  /// @brief Check if an entry exists in the cache.
  /// @returns A cache entry struct. If there was no cache hit the entry will be empty.
  cache_entry_t lookup(const hasher_t::hash_t& hash);

  /// @brief Adds a set of files to the cache.
  /// @param hash The cache entry identifier.
  /// @param entry The cache entry data (files, stdout, etc).
  /// @param expected_files Paths to the actual files in the local file system (map from file ID to
  /// an expected file descriptor).
  void add(const hasher_t::hash_t& hash,
           const cache_entry_t& entry,
           const std::map<std::string, expected_file_t>& expected_files);

  /// @brief Copy a cached file to the local file system.
  /// @param hash The cache entry identifier.
  /// @param source_id The ID of the remote file to copy.
  /// @param target_path The path to the local file.
  /// @param is_compressed True if the remote data is compressed.
  void get_file(const hasher_t::hash_t& hash,
                const std::string& source_id,
                const std::string& target_path,
                const bool is_compressed);

private:
  remote_cache_provider_t* m_provider = nullptr;
};

}  // namespace bcache

#endif  // BUILDCACHE_REMOTE_CACHE_HPP_
