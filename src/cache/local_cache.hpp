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

#ifndef BUILDCACHE_LOCAL_CACHE_HPP_
#define BUILDCACHE_LOCAL_CACHE_HPP_

#include <base/file_utils.hpp>
#include <base/hasher.hpp>
#include <base/lock_file.hpp>
#include <cache/cache_entry.hpp>
#include <cache/cache_stats.hpp>
#include <cache/expected_file.hpp>

#include <string>
#include <utility>

namespace bcache {

class local_cache_t {
public:
  /// @brief Initialize the cache object.
  /// @throws runtime_error if the cache could not be initialized.
  local_cache_t();

  /// @brief De-initialzie the cache object.
  ~local_cache_t();

  /// @brief Clear all entries in the cache.
  void clear();

  /// @brief Show cache statistics (print to standard out).
  void show_stats();

  /// @brief Clear the cache statistics.
  void zero_stats();

  /// @brief Add a set of files to the cache
  /// @param hash The cache entry identifier.
  /// @param entry The cache entry data (files, stdout, etc).
  /// @param expected_files Paths to the actual files in the local file system (map from file ID to
  /// an expected file descriptor).
  /// @param allow_hard_links Whether or not to allow hard links to be used when caching files.
  void add(const hasher_t::hash_t& hash,
           const cache_entry_t& entry,
           const std::map<std::string, expected_file_t>& expected_files,
           const bool allow_hard_links);

  /// @brief Check if an entry exists in the cache.
  /// @returns A pair of a cache entry struct and a lock file object. If there was no cache hit,
  /// the entry will be empty, and the lock file object will not hold any lock.
  std::pair<cache_entry_t, file::lock_file_t> lookup(const hasher_t::hash_t& hash);

  /// @brief Copy a cached file to the local file system.
  /// @param hash The cache entry identifier.
  /// @param source_id The ID of the cached file to copy.
  /// @param target_path The path to the local file.
  /// @param is_compressed True if the cached data is compressed.
  /// @param allow_hard_links True if hard links are allowed.
  void get_file(const hasher_t::hash_t& hash,
                const std::string& source_id,
                const std::string& target_path,
                const bool is_compressed,
                const bool allow_hard_links);

  /// @brief Update statistics associated with the given entry
  bool update_stats(const hasher_t::hash_t& hash, const cache_stats_t& delta) const noexcept;

private:
  const std::string hash_to_cache_entry_path(const hasher_t::hash_t& hash) const;
  const std::string get_cache_files_folder() const;

  void perform_housekeeping();
};

}  // namespace bcache

#endif  // BUILDCACHE_LOCAL_CACHE_HPP_
