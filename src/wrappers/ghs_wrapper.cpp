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

ghs_wrapper_t::ghs_wrapper_t(const string_list_t& args) : gcc_wrapper_t(args) {
}

bool ghs_wrapper_t::can_handle_command() {
  // Is this the right compiler?
  const auto cmd = lower_case(file::get_file_part(m_args[0], false));
  return (cmd.find("ccarm") != std::string::npos) || (cmd.find("cxarm") != std::string::npos) ||
         (cmd.find("ccthumb") != std::string::npos) || (cmd.find("cxthumb") != std::string::npos) ||
         (cmd.find("ccintarm") != std::string::npos) || (cmd.find("cxintarm") != std::string::npos);
}

string_list_t ghs_wrapper_t::get_relevant_arguments() {
  string_list_t filtered_args;

  // The first argument is the compiler binary without the path.
  filtered_args += file::get_file_part(m_args[0]);

  // Note: We always skip the first arg since we have handled it already.
  bool skip_next_arg = true;
  for (auto arg : m_args) {
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

std::string ghs_wrapper_t::get_program_id() {
  // Getting a version string from the GHS compiler by passing "--version" is less than trivial. For
  // instance you need to pass valid -bsp and -os_dir arguments, and a dummy source file that does
  // not exist (otherwise it will fail or actually perform the compilation), and then the output is
  // sent to stderr instead of stdout.

  // Instead, we fall back to the default program ID logic (i.e. hash the program binary).
  const auto program_version_info = program_wrapper_t::get_program_id();

  // Try to retrieve the version information for the OS files.
  std::string os_dir;
  for (auto arg : m_args) {
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
}  // namespace bcache
