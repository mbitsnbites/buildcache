//--------------------------------------------------------------------------------------------------
// Copyright (c) 2018 Marcus Geelnard
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

#ifndef BUILDCACHE_PERF_UTILS_HPP_
#define BUILDCACHE_PERF_UTILS_HPP_

#include <cstdint>

namespace bcache {
namespace perf {
/// @brief Recognized instrumentation IDs.
enum id_t {
  ID_FIND_EXECUTABLE = 0,
  ID_FIND_WRAPPER = 1,
  ID_LUA_INIT = 2,
  ID_LUA_LOAD_SCRIPT = 3,
  ID_LUA_RUN = 4,
  ID_RESOLVE_ARGS = 5,
  ID_GET_CAPABILITIES = 6,
  ID_PREPROCESS = 7,
  ID_FILTER_ARGS = 8,
  ID_GET_PRG_ID = 9,
  ID_CACHE_LOOKUP = 10,
  ID_RETRIEVE_CACHED_FILES = 11,
  ID_GET_BUILD_FILES = 12,
  ID_RUN_FOR_MISS = 13,
  ID_ADD_TO_CACHE = 14,
  ID_RUN_FOR_FALLBACK = 15,
  ID_UPDATE_STATS = 16,
  ID_TOTAL = 17,
  ID_HASH_EXTRA_FILES = 18,
  NUM_PERF_IDS
};

/// @brief Start measuring time.
/// @returns a starting time point.
int64_t start();

/// @brief Stop measuring time.
/// @param start_time The starting time for the measurment.
/// @param id The id of the measurment.
void stop(const int64_t start_time, const id_t id);

/// @brief Report the results.
void report();

/// @brief A scoped perf logger.
class perf_scope_t {
public:
  perf_scope_t(const id_t id) : m_id(id), m_t0(start()) {
  }

  ~perf_scope_t() {
    stop(m_t0, m_id);
  }

private:
  const id_t m_id;
  const int64_t m_t0;
};
}  // namespace perf
}  // namespace bcache

// Convenience macros.
#define PERF_START(id) const auto t0_ID_##id = bcache::perf::start()
#define PERF_STOP(id) bcache::perf::stop(t0_ID_##id, bcache::perf::ID_##id);
#define PERF_SCOPE(id) bcache::perf::perf_scope_t p_ID_##id(bcache::perf::ID_##id);

#endif  // BUILDCACHE_PERF_UTILS_HPP_
