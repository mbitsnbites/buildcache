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

#include <wrappers/program_wrapper.hpp>

#include <base/compressor.hpp>
#include <base/debug_utils.hpp>
#include <base/hasher.hpp>
#include <cache/cache_entry.hpp>
#include <config/configuration.hpp>
#include <sys/perf_utils.hpp>
#include <sys/sys_utils.hpp>

#include <iostream>
#include <map>

namespace bcache {
namespace {
/// @brief A helper class for managing wrapper capabilities.
class capabilities_t {
public:
  capabilities_t(const string_list_t& cap_strings);

  bool hard_links() const {
    return m_hard_links;
  }

  bool create_target_dirs() const {
    return m_create_target_dirs;
  }

private:
  bool m_hard_links = false;
  bool m_create_target_dirs = false;
};

capabilities_t::capabilities_t(const string_list_t& cap_strings) {
  for (const auto& str : cap_strings) {
    if (str == "hard_links") {
      m_hard_links = true;
    } else if (str == "create_target_dirs") {
      m_create_target_dirs = true;
    } else {
      debug::log(debug::ERROR) << "Invalid capability string: " << str;
    }
  }
}
}  // namespace

program_wrapper_t::program_wrapper_t(const string_list_t& args) : m_args(args) {
}

program_wrapper_t::~program_wrapper_t() {
}

bool program_wrapper_t::handle_command(int& return_code) {
  return_code = 1;

  try {
    // Begin by resolving any response files.
    PERF_START(RESOLVE_ARGS);
    resolve_args();
    PERF_STOP(RESOLVE_ARGS);

    // Get wrapper capabilities.
    PERF_START(GET_CAPABILITIES);
    const auto capabilites = capabilities_t(get_capabilities());
    PERF_STOP(GET_CAPABILITIES);

    // Start a hash.
    hasher_t hasher;

    // Hash the preprocessed file contents.
    PERF_START(PREPROCESS);
    hasher.update(preprocess_source());
    PERF_STOP(PREPROCESS);

    // Hash the (filtered) command line flags and environment variables.
    PERF_START(FILTER_ARGS);
    hasher.update(get_relevant_arguments().join(" ", true));
    hasher.update(get_relevant_env_vars());
    PERF_STOP(FILTER_ARGS);

    // Hash the program identification (version string or similar).
    PERF_START(GET_PRG_ID);
    hasher.update(get_program_id());
    PERF_STOP(GET_PRG_ID);

    // Finalize the hash.
    const auto hash = hasher.final();

    // Check if we can use hard links.
    const auto allow_hard_links = config::hard_links() && capabilites.hard_links();

    // Get the list of files that are expected to be generated by the command. This is in fact a
    // map of file ID:s to their corresponding file path.
    PERF_START(GET_BUILD_FILES);
    const auto expected_files = get_build_files();
    PERF_STOP(GET_BUILD_FILES);

    // Look up the entry in the cache(s).
    if (m_cache.lookup(hash,
                       expected_files,
                       allow_hard_links,
                       capabilites.create_target_dirs(),
                       return_code)) {
      return true;
    } else {
      debug::log(debug::INFO) << "Cache miss (" << hash.as_string() << ")";

      //If the "terminate on a miss" mode is enabled and we didn't find
      //an entry in the cache, we exit
      if(config::terminate_on_miss()) {
        for (const auto& file : expected_files) {
          std::cout << file.second.path() << '\n';
        }
        std::cout << "Terminate on a miss!\n";
        std::exit(0);
      }
    }

    // Run the actual program command to produce the build file(s).
    PERF_START(RUN_FOR_MISS);
    const auto result = run_for_miss();
    PERF_STOP(RUN_FOR_MISS);

    // Extract only the file ID:s (and filter out missing optional files).
    std::vector<std::string> file_ids;
    for (const auto& file : expected_files) {
      const auto& expected_file = file.second;
      if (expected_file.required() || file::file_exists(expected_file.path())) {
        file_ids.emplace_back(file.first);
      }
    }

    // Create a new entry in the cache.
    // Note: We do not want to create cache entries for failed program runs. We could, but that
    // would run the risk of caching intermittent faults for instance.
    if (result.return_code == 0) {
      // Add the entry to the cache.
      const cache_entry_t entry(
          file_ids,
          config::compress() ? cache_entry_t::comp_mode_t::ALL : cache_entry_t::comp_mode_t::NONE,
          result.std_out,
          result.std_err,
          result.return_code);
      m_cache.add(hash, entry, expected_files, allow_hard_links);
    }

    // Everything's ok!
    // Note: Even if the program failed, we've done the expected job (running the program again
    // would just take twice the time and give the same errors).
    return_code = result.return_code;
    return true;
  } catch (std::exception& e) {
    debug::log(debug::DEBUG) << "Exception: " << e.what();
  } catch (...) {
    // Catch-all in order to not propagate exceptions any higher up (we'll return false).
    debug::log(debug::ERROR) << "UNEXPECTED EXCEPTION";
  }

  return false;
}

//--------------------------------------------------------------------------------------------------
// Default wrapper interface implementation. Wrappers are expected to override the parts that are
// relevant.
//--------------------------------------------------------------------------------------------------

void program_wrapper_t::resolve_args() {
  // Default: Do nothing.
}

string_list_t program_wrapper_t::get_capabilities() {
  // Default: No capabilities are supported.
  string_list_t capabilites;
  return capabilites;
}

std::string program_wrapper_t::preprocess_source() {
  // Default: There is no prepocessing step.
  return std::string();
}

string_list_t program_wrapper_t::get_relevant_arguments() {
  // Default: All arguments are relevant.
  return m_args;
}

std::map<std::string, std::string> program_wrapper_t::get_relevant_env_vars() {
  // Default: There are no relevant environment variables.
  std::map<std::string, std::string> env_vars;
  return env_vars;
}

std::string program_wrapper_t::get_program_id() {
  // Default: The hash of the program binary serves as the program identification.
  const auto& program_exe = m_args[0];
  hasher_t hasher;
  hasher.update_from_file(program_exe);
  return hasher.final().as_string();
}

std::map<std::string, expected_file_t> program_wrapper_t::get_build_files() {
  // Default: There are no build files generated by the command.
  std::map<std::string, expected_file_t> result;
  return result;
}

sys::run_result_t program_wrapper_t::run_for_miss() {
  // Default: Run the program with the configured prefix.
  return sys::run_with_prefix(m_args, false);
}

}  // namespace bcache
