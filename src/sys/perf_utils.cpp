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

#include <sys/perf_utils.hpp>

#include <base/unicode_utils.hpp>
#include <config/configuration.hpp>

#include <chrono>
#include <iomanip>
#include <iostream>

namespace bcache {
namespace perf {
namespace {
int64_t s_perf_log[NUM_PERF_IDS] = {};

int64_t get_time_in_us() {
  const auto t =
      std::chrono::time_point_cast<std::chrono::microseconds>(std::chrono::system_clock::now())
          .time_since_epoch()
          .count();
  return t;
}

struct perf_us_t {
  perf_us_t(const int id) : value(s_perf_log[id]) {
  }
  const int64_t value;
};

struct perf_ms_t {
  perf_ms_t(const int id) : value(static_cast<double>(s_perf_log[id]) / 1000.0) {
  }
  const double value;
};

std::ostream& operator<<(std::ostream& out, const perf_us_t& p) {
  out << std::setw(10) << p.value << " us";
  return out;
}

std::ostream& operator<<(std::ostream& out, const perf_ms_t& p) {
  out << std::setw(10) << p.value << " ms";
  return out;
}
}  // namespace

int64_t start() {
  return get_time_in_us();
}

void stop(const int64_t start_time, const id_t id) {
  const auto dt = get_time_in_us() - start_time;
  s_perf_log[id] += dt;
}

void report() {
  if (config::perf()) {
    // Set format.
    std::ios old_fmt(nullptr);
    old_fmt.copyfmt(std::cerr);
    std::cerr << std::setiosflags(std::ios::fixed) << std::setprecision(1);

    std::cerr << "Find exectuable:       " << perf_us_t(ID_FIND_EXECUTABLE) << "\n";
    std::cerr << "Find wrapper:          " << perf_us_t(ID_FIND_WRAPPER) << "\n";
    std::cerr << "Lua - Init:            " << perf_us_t(ID_LUA_INIT) << "\n";
    std::cerr << "Lua - Load script:     " << perf_us_t(ID_LUA_LOAD_SCRIPT) << "\n";
    std::cerr << "Lua - Run:             " << perf_us_t(ID_LUA_RUN) << "\n";
    std::cerr << "Resolve args:          " << perf_us_t(ID_RESOLVE_ARGS) << "\n";
    std::cerr << "Get capabilities:      " << perf_us_t(ID_GET_CAPABILITIES) << "\n";
    std::cerr << "Preprocess:            " << perf_us_t(ID_PREPROCESS) << "\n";
    std::cerr << "Filter arguments:      " << perf_us_t(ID_FILTER_ARGS) << "\n";
    std::cerr << "Get program id:        " << perf_us_t(ID_GET_PRG_ID) << "\n";
    std::cerr << "Hash extra files:      " << perf_us_t(ID_HASH_EXTRA_FILES) << "\n";
    std::cerr << "Cache lookup:          " << perf_us_t(ID_CACHE_LOOKUP) << "\n";
    std::cerr << "Retreive cached files: " << perf_us_t(ID_RETRIEVE_CACHED_FILES) << "\n";
    std::cerr << "Get build files:       " << perf_us_t(ID_GET_BUILD_FILES) << "\n";
    std::cerr << "Run cmd (miss):        " << perf_us_t(ID_RUN_FOR_MISS) << "\n";
    std::cerr << "Add to cache:          " << perf_us_t(ID_ADD_TO_CACHE) << "\n";
    std::cerr << "Run cmd (fallback):    " << perf_us_t(ID_RUN_FOR_FALLBACK) << "\n";
    std::cerr << "Update stats:          " << perf_us_t(ID_UPDATE_STATS) << "\n";
    std::cerr << "\n";
    std::cerr << "TOTAL:                 " << perf_ms_t(ID_TOTAL) << "\n";

    // Restore format.
    std::cerr.copyfmt(old_fmt);
  }
}
}  // namespace perf
}  // namespace bcache
