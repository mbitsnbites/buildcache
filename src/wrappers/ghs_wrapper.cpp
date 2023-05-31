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

#include <wrappers/ghs_wrapper.hpp>

#include <base/debug_utils.hpp>
#include <base/file_utils.hpp>
#include <base/unicode_utils.hpp>

#include <algorithm>
#include <list>
#include <regex>
#include <set>
#include <sstream>
#include <stdexcept>

namespace bcache {
namespace {
// Tick this to a new number if the format has changed in a non-backwards-compatible way.
const std::string HASH_VERSION = "2";

bool is_source_file(const std::string& arg) {
  const auto ext = lower_case(file::get_extension(arg));
  return ((ext == ".cpp") || (ext == ".cc") || (ext == ".cxx") || (ext == ".c"));
}

std::string get_nonexisting_source_file_path() {
  // Try a few different paths until we find one that represents a non-existing file...
  for (int i = 0; i < 100; ++i) {
    std::ostringstream ss;
    ss << "_nofile_" << i << ".cpp";
    auto path = ss.str();
    if (!bcache::file::file_exists(path)) {
      return path;
    }
  }

  // Bail at this point.
  // TODO(m): Can we do better?
  throw std::runtime_error("Unable to produce a path to a non-existing file");
}

std::string extract_version_info(const std::string& version_str) {
  // We do not want all lines in the version_str (e.g. any error messages), so we extract lines that
  // include "version" or "revision" (case insensitive).
  std::string result;
  string_list_t lines(version_str, "\n");
  const std::regex pattern("(version|revision)", std::regex_constants::icase);
  for (const auto& line : lines) {
    if (std::regex_search(line, pattern)) {
      result += line + "\n";
    }
  }
  return result;
}
}  // namespace

ghs_wrapper_t::ghs_wrapper_t(const file::exe_path_t& exe_path, const string_list_t& args)
    : gcc_wrapper_t(exe_path, args) {
}

string_list_t ghs_wrapper_t::get_include_files(const std::string& std_err) const {
  // Turn the std_err string into a list of strings.
  // TODO(m): Is this correct on Windows for instance?
  string_list_t lines(std_err, "\n");

  // Extract all unique include paths. Include path references in std_err start with zero or more
  // spaces followed by the full path. In the regex we also trim leading and trailing whitespaces
  // from the path, just for good measure.
  const std::regex incpath_re(R"(\s*(.*[^\s])\s*)");
  std::set<std::string> includes;
  for (const auto& line : lines) {
    std::smatch match;
    if (std::regex_match(line, match, incpath_re)) {
      if (match.size() == 2) {
        const auto& include = match[1].str();
        includes.insert(file::resolve_path(include));
      }
    }
  }

  // Convert the set of includes to a list of strings.
  string_list_t result;
  for (const auto& include : includes) {
    result += include;
  }
  return result;
}

bool ghs_wrapper_t::can_handle_command() {
  // Is this the right compiler?

  static const std::list<std::string> supported = {"ccarm",
                                                   "ccintarm",
                                                   "cxarm",
                                                   "cxintarm",
                                                   "ccthumb",
                                                   "cxthumb",
                                                   "ccrh850",
                                                   "ccintrh850",
                                                   "cxrh850",
                                                   "cxintrh850"};

  const auto cmd = lower_case(file::get_file_part(m_exe_path.real_path(), false));

  return std::find_if(supported.begin(), supported.end(), [&cmd](const std::string& s) {
           return cmd.find(s) != std::string::npos;
         }) != supported.end();
}

string_list_t ghs_wrapper_t::get_capabilities() {
  // GHS compilers implicitly create target directories for object files.
  return gcc_wrapper_t::get_capabilities() + "create_target_dirs";
}

std::string ghs_wrapper_t::get_program_id() {
  // We start by using the default program ID logic (i.e. hash the program binary).
  const auto program_version_info = program_wrapper_t::get_program_id();

  // Try to retrieve the version information for the OS files.
  std::string os_dir;
  for (const auto& arg : m_args) {
    if (arg.substr(0, 8) == "-os_dir=") {
      os_dir = arg.substr(8);
    }
  }
  std::string os_version_info;
  if (!os_dir.empty()) {
    // There should be a header file called ${OS_DIR}/INTEGRITY-include/INTEGRITY_version.h that
    // contains version information.
    const auto os_version_file =
        file::append_path(file::append_path(os_dir, "INTEGRITY-include"), "INTEGRITY_version.h");
    if (file::file_exists(os_version_file)) {
      os_version_info = file::read(os_version_file);
    }
  }

  // Try to retrieve the version information by querying the version info from the compiler.
  // Getting a version string from the GHS compiler by passing "--version" is less than trivial. You
  // need to pass valid -bsp and -os_dir arguments, and a dummy source file that does not exist
  // (otherwise it will fail or actually perform the compilation). Finally, the output is sent to
  // stderr instead of stdout, along with the compilation error message.
  std::string cmd_line_version;
  try {
    if (!os_dir.empty()) {
      std::string bsp;
      for (const auto& arg : m_args) {
        if (arg.substr(0, 5) == "-bsp=") {
          bsp = arg.substr(5);
        }
      }
      if (!bsp.empty()) {
        string_list_t version_args;
        version_args += m_args[0];
        version_args += "-os_dir=" + os_dir;
        version_args += "-bsp=" + bsp;
        version_args += "--version";
        version_args += get_nonexisting_source_file_path();
        const auto result = sys::run(version_args);
        cmd_line_version = extract_version_info(result.std_err);
      }
    }
  } catch (std::exception& e) {
    debug::log(debug::ERROR) << "ghs version info: " << e.what();
  }

  return HASH_VERSION + program_version_info + os_version_info + cmd_line_version;
}

string_list_t ghs_wrapper_t::get_relevant_arguments() {
  string_list_t filtered_args;

  // The first argument is the compiler binary without the path.
  filtered_args += file::get_file_part(m_args[0]);

  // Note: We always skip the first arg since we have handled it already.
  bool skip_next_arg = true;
  for (const auto& arg : m_args) {
    if (!skip_next_arg) {
      // Does this argument specify a file (we don't want to hash those).
      const bool is_arg_plus_file_name =
          ((arg == "-I") || (arg == "-MF") || (arg == "-MT") || (arg == "-MQ") || (arg == "-o"));

      // Generally unwanted argument (things that will not change how we go from preprocessed code
      // to binary object files)?
      const auto first_two_chars = arg.substr(0, 2);
      const bool is_unwanted_arg =
          ((first_two_chars == "-I") || (first_two_chars == "-D") || (first_two_chars == "-M") ||
           (arg.substr(0, 8) == "-os_dir=") || is_source_file(arg));

      if (is_arg_plus_file_name) {
        skip_next_arg = true;
      } else if (!is_unwanted_arg) {
        filtered_args += arg;
      }
    } else {
      skip_next_arg = false;
    }
  }

  debug::log(debug::DEBUG) << "Filtered arguments: " << filtered_args.join(" ", true);

  return filtered_args;
}

std::map<std::string, std::string> ghs_wrapper_t::get_relevant_env_vars() {
  // TODO(m): What environment variables can affect the build result?
  std::map<std::string, std::string> env_vars;
  return env_vars;
}
}  // namespace bcache
