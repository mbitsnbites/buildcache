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

#ifndef BUILDCACHE_REDIS_CACHE_PROVIDER_HPP_
#define BUILDCACHE_REDIS_CACHE_PROVIDER_HPP_

#include <cache/remote_cache_provider.hpp>

// Forward declaration.
typedef struct redisContext redisContext;

namespace bcache {

class redis_cache_provider_t : public remote_cache_provider_t {
public:
  redis_cache_provider_t();
  ~redis_cache_provider_t() override;

  // Implementation of the remote_cache_provider_t interface.
  bool connect(const std::string& host_description) override;
  bool is_connected() const override;
  cache_entry_t lookup(const hasher_t::hash_t& hash) override;
  void add(const hasher_t::hash_t& hash,
           const cache_entry_t& entry,
           const std::map<std::string, expected_file_t>& expected_files) override;
  void get_file(const hasher_t::hash_t& hash,
                const std::string& source_id,
                const std::string& target_path,
                const bool is_compressed) override;

private:
  /// @brief Disconnect (usually as a result of an error).
  void disconnect();

  /// @brief Get a binary data blob from the remote cache.
  /// @param key The unique key that identifies the data.
  /// @returns the data as a string object.
  /// @throws runtime_error if the data could not be retrieved (e.g. the key does not exist in the
  /// remote cache).
  std::string get_data(const std::string& key);

  /// @brief Set a binary data blob in the remote cache.
  /// @param key The unique key that identifies the data.
  /// @param data The data as a string object.
  /// @throws runtime_error if the data could not be retrieved (e.g. the key does not exist in the
  /// remote cache).
  void set_data(const std::string& key, const std::string& data);

  redisContext* m_ctx = nullptr;
};

}  // namespace bcache

#endif  // BUILDCACHE_REDIS_CACHE_PROVIDER_HPP_
