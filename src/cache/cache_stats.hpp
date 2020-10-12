//--------------------------------------------------------------------------------------------------
// Copyright (c) 2019 Alexey Sheplyakov
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

#ifndef BUILDCACHE_CACHE_STATS_HPP_
#define BUILDCACHE_CACHE_STATS_HPP_
#include <iosfwd>
#include <string>

struct cJSON;

namespace bcache {

class cache_stats_t {
  int m_local_miss_count{0};
  int m_local_hit_count{0};
  int m_remote_hit_count{0};
  int m_remote_miss_count{0};

public:
  bool from_file(const std::string& path) noexcept;
  bool from_json(cJSON const* obj) noexcept;
  bool to_json(cJSON* obj) const noexcept;
  bool to_file(const std::string& path) const noexcept;

  cache_stats_t& operator+=(const cache_stats_t& other) noexcept {
    m_local_hit_count += other.m_local_hit_count;
    m_local_miss_count += other.m_local_miss_count;
    m_remote_hit_count += other.m_remote_hit_count;
    m_remote_miss_count += other.m_remote_miss_count;
    return *this;
  }

  double local_hit_ratio() const noexcept {
    int total = m_local_hit_count + m_local_miss_count;
    if (total != 0) {
      return (100.0 * m_local_hit_count) / total;
    } else {
      return 0.0;
    }
  }

  double remote_hit_ratio() const noexcept {
    int total = m_remote_hit_count + m_remote_miss_count;
    if (total != 0) {
      return (100.0 * m_remote_hit_count) / total;
    } else {
      return 0.0;
    }
  }

  /// @brief Hit either the local or the remote cache
  int global_hit_count() const noexcept {
    // buildcache does not use the remote cache on a local hit
    return m_local_hit_count + m_remote_hit_count;
  }

  /// @brief Missed both local and remote cache
  int global_miss_count() const noexcept {
    // Every local miss which has been satisfied from the remote cache is a global hit
    return m_local_miss_count - m_remote_hit_count;
  }

  double global_hit_ratio() const noexcept {
    int total = global_hit_count() + global_miss_count();
    if (total != 0) {
      return (100.0 * global_hit_count()) / total;
    } else {
      return 0.0;
    }
  }

  static cache_stats_t local_hit() noexcept {
    cache_stats_t st;
    st.m_local_hit_count = 1;
    st.m_local_miss_count = 0;
    return st;
  }
  static cache_stats_t local_miss() noexcept {
    cache_stats_t st;
    st.m_local_hit_count = 0;
    st.m_local_miss_count = 1;
    return st;
  }
  static cache_stats_t remote_miss() noexcept {
    cache_stats_t st;
    st.m_remote_hit_count = 0;
    st.m_remote_miss_count = 1;
    return st;
  }
  static cache_stats_t remote_hit() noexcept {
    cache_stats_t st;
    st.m_remote_hit_count = 1;
    st.m_remote_miss_count = 0;
    return st;
  }
  void dump(std::ostream& os, const std::string& prefix) const;

  int local_hit_count() const {
    return m_local_hit_count;
  }
  int local_miss_count() const {
    return m_local_miss_count;
  }
  int remote_hit_count() const {
    return m_remote_hit_count;
  }
  int remote_miss_count() const {
    return m_remote_miss_count;
  }
  void set_local_hit_count(int value) {
    m_local_hit_count = value;
  }
  void set_local_miss_count(int value) {
    m_local_miss_count = value;
  }
  void set_remote_hit_count(int value) {
    m_remote_hit_count = value;
  }
  void set_remote_miss_value(int value) {
    m_remote_miss_count = value;
  }
};

}  // namespace bcache

#endif /* BUILDCACHE_CACHE_STATS_HPP_ */
