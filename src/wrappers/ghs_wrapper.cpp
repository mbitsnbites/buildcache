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
#include <stdexcept>

namespace bcache {
namespace {
// Tick this to a new number if the format has changed in a non-backwards-compatible way.
const std::string HASH_VERSION = "1";

bool is_source_file(const std::string& arg) {
  const auto ext = lower_case(file::get_extension(arg));
  return ((ext == ".cpp") || (ext == ".cc") || (ext == ".cxx") || (ext == ".c"));
}
}  // namespace

ghs_wrapper_t::ghs_wrapper_t(const file::exe_path_t& exe_path, const string_list_t& args)
    : gcc_wrapper_t(exe_path, args) {
}

std::set<std::string> ghs_wrapper_t::get_include_files(const std::string& std_err) const {
  // Turn the std_err string into a list of strings.
  // TODO(m): Is this correct on Windows for instance?
  string_list_t lines(std_err, "\n");

  // Extract all unique include paths. Include path references in std_err start with zero or more
  // spaces followed by the full path. In the regex we also trim leading and trailing whitespaces
  // from the path, just for good measure.
  const std::regex incpath_re("\\s*(.*[^\\s])\\s*");
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

  return includes;
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
  // Getting a version string from the GHS compiler by passing "--version" is less than trivial. For
  // instance you need to pass valid -bsp and -os_dir arguments, and a dummy source file that does
  // not exist (otherwise it will fail or actually perform the compilation), and then the output is
  // sent to stderr instead of stdout.

  // Instead, we fall back to the default program ID logic (i.e. hash the program binary).
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

  return HASH_VERSION + program_version_info + os_version_info;
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
