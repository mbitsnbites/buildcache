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

#include <base/debug_utils.hpp>
#include <base/env_utils.hpp>
#include <base/unicode_utils.hpp>
#include <config/configuration.hpp>
#include <sys/filetracker.hpp>
#include <sys/sys_utils.hpp>
#include <wrappers/msvc_wrapper.hpp>

#include <codecvt>
#include <cstdlib>

#include <fstream>
#include <locale>
#include <stdexcept>

namespace bcache {
namespace {
// Tick this to a new number if the format has changed in a non-backwards-compatible way.
const std::string HASH_VERSION = "1";

// When cl.exe is started from Visual Studio, it explicitly sends certain output to the IDE
// process. This prevents capturing output otherwise written to stderr or stdout. The
// redirection is controlled by the VS_UNICODE_OUTPUT environment variable.
const std::string ENV_VS_OUTPUT_REDIRECTION = "VS_UNICODE_OUTPUT";

bool is_source_file(const std::string& arg) {
  const auto ext = lower_case(file::get_extension(arg));
  return ((ext == ".cpp") || (ext == ".cc") || (ext == ".cxx") || (ext == ".c"));
}

bool is_object_file(const std::string& file_ext) {
  const auto ext = lower_case(file_ext);
  return ((ext == ".obj") || (ext == ".o"));
}

bool arg_starts_with(const std::string& str, const std::string& sub_str) {
  const auto size = sub_str.size();
  const auto is_flag = (size >= 1) && ((str[0] == '/') || (str[0] == '-'));
  return is_flag && ((str.size() >= (size + 1)) && (str.substr(1, size) == sub_str));
}

bool arg_equals(const std::string& str, const std::string& sub_str) {
  const auto size = sub_str.size();
  const auto is_flag = (size >= 1) && ((str[0] == '/') || (str[0] == '-'));
  return is_flag && ((str.size() >= (size + 1)) && (str.substr(1) == sub_str));
}

// Apparently some cl.exe arguments can be specified with an optional colon separator (e.g.
// both "/Fooutput.obj" and "/Fo:output.obj" are valid).
std::string drop_leading_colon(const std::string& s) {
  if (s.length() > 0 && s[0] == ':') {
    return s.substr(1);
  } else {
    return s;
  }
}

string_list_t make_preprocessor_cmd(const string_list_t& args) {
  string_list_t preprocess_args;

  // Drop arguments that we do not want/need, and check if the build will produce debug/coverage
  // info.
  bool has_debug_symbols = false;
  bool has_coverage_output = false;
  for (auto it = args.begin(); it != args.end(); ++it) {
    auto arg = *it;
    bool drop_this_arg = false;
    if (arg_equals(arg, "c") || arg_starts_with(arg, "Fo") || arg_equals(arg, "C") ||
        arg_equals(arg, "E") || arg_equals(arg, "EP")) {
      drop_this_arg = true;
    }
    if (arg_equals(arg, "Z7") || arg_equals(arg, "Zi") || arg_equals(arg, "ZI")) {
      has_debug_symbols = true;
    }
    if (arg_equals(arg, "DEBUG") || arg_equals(arg, "DEBUG:FULL") || arg_equals(arg, "Zi") ||
        arg_equals(arg, "ZI")) {
      has_coverage_output = true;
    }
    if (!drop_this_arg) {
      preprocess_args += arg;
    }
  }

  // Should we inhibit line info in the preprocessed output?
  const bool debug_symbols_required =
      has_debug_symbols && (config::accuracy() >= config::cache_accuracy_t::STRICT);
  const bool coverage_symbols_required =
      has_coverage_output && (config::accuracy() >= config::cache_accuracy_t::DEFAULT);
  const bool inhibit_line_info = !(debug_symbols_required || coverage_symbols_required);

  // Append the required arguments for producing preprocessed output.
  if (inhibit_line_info) {
    preprocess_args += std::string("/EP");
  } else {
    preprocess_args += std::string("/E");
  }

  return preprocess_args;
}
}  // namespace

msvc_wrapper_t::msvc_wrapper_t(const string_list_t& args) : program_wrapper_t(args) {
}

void msvc_wrapper_t::resolve_args() {
  // Iterate over all args and load any response files that we encounter.
  m_resolved_args.clear();
  for (const auto& arg : m_args) {
    if (arg.substr(0, 1) == "@") {
      std::ifstream file(arg.substr(1));
      if (file.is_open()) {
        // Look for UTF-16 BOM.
        int byte0 = file.get();
        int byte1 = file.get();
        if ((byte0 == 0xff && byte1 == 0xfe) || (byte0 == 0xfe && byte1 == 0xff)) {
          // Reopen stream knowing the file is UTF-16 encoded.
          file.close();
          std::wifstream wfile(arg.substr(1), std::ios::binary);
          wfile.imbue(std::locale(wfile.getloc(),
                                  new std::codecvt_utf16<wchar_t, 0x10ffff, std::consume_header>));
          std::wstring wline;
          while (std::getline(wfile, wline)) {
            m_resolved_args += string_list_t::split_args(ucs2_to_utf8(wline));
          }
        } else {
          // Assume UTF-8.
          file.clear();
          file.seekg(0);
          std::string line;
          while (std::getline(file, line)) {
            m_resolved_args += string_list_t::split_args(line);
          }
        }
      }
    } else {
      m_resolved_args += arg;
    }
  }
}

bool msvc_wrapper_t::can_handle_command() {
  // Is this the right compiler?
  const auto cmd = lower_case(file::get_file_part(m_args[0], false));
  return (cmd == "cl");
}

string_list_t msvc_wrapper_t::get_capabilities() {
  // We can use hard links with MSVC since it will never overwrite already existing files.
  return string_list_t{"hard_links"};
}

std::string msvc_wrapper_t::preprocess_source() {
  // Check if this is a compilation command that we support.
  auto is_object_compilation = false;
  auto has_object_output = false;
  for (const auto& arg : m_resolved_args) {
    if (arg_equals(arg, "c")) {
      is_object_compilation = true;
    } else if (arg_starts_with(arg, "Fo") && (is_object_file(file::get_extension(arg)))) {
      has_object_output = true;
    } else if (arg_equals(arg, "Zi") || arg_equals(arg, "ZI")) {
      throw std::runtime_error("PDB generation is not supported.");
    }
  }
  if ((!is_object_compilation) || (!has_object_output)) {
    throw std::runtime_error("Unsupported complation command.");
  }

  // Disable unwanted printing of source file name in Visual Studio.
  scoped_unset_env_t scoped_off(ENV_VS_OUTPUT_REDIRECTION);

  // Run the preprocessor step.
  const auto preprocessor_args = make_preprocessor_cmd(m_resolved_args);
  auto result = sys::run(preprocessor_args);
  if (result.return_code != 0) {
    throw std::runtime_error("Preprocessing command was unsuccessful.");
  }

  // Return the preprocessed file (from stdout).
  return result.std_out;
}

string_list_t msvc_wrapper_t::get_relevant_arguments() {
  string_list_t filtered_args;

  // The first argument is the compiler binary without the path.
  filtered_args += file::get_file_part(m_resolved_args[0]);

  // Note: We always skip the first arg since we have handled it already.
  bool skip_next_arg = true;
  for (const auto& arg : m_resolved_args) {
    if (!skip_next_arg) {
      // Generally unwanted argument (things that will not change how we go from preprocessed code
      // to binary object files)?
      const auto first_two_chars = arg.substr(0, 2);
      const bool is_unwanted_arg = ((arg_equals(first_two_chars, "F") && !arg_equals(arg, "F")) ||
                                    arg_equals(first_two_chars, "I") ||
                                    arg_equals(first_two_chars, "D") || is_source_file(arg));

      if (!is_unwanted_arg) {
        filtered_args += arg;
      }
    } else {
      skip_next_arg = false;
    }
  }

  debug::log(debug::DEBUG) << "Filtered arguments: " << filtered_args.join(" ", true);

  return filtered_args;
}

std::map<std::string, std::string> msvc_wrapper_t::get_relevant_env_vars() {
  // According to this: https://msdn.microsoft.com/en-us/library/kezkeayy.aspx
  // ...the following environment variables are relevant for compilation results: CL, _CL_
  static const std::string CL_ENV_VARS[] = {"CL", "_CL_"};
  std::map<std::string, std::string> env_vars;
  for (const auto& key : CL_ENV_VARS) {
    const auto* value = std::getenv(key.c_str());
    if (value != nullptr) {
      env_vars[key] = std::string(value);
    }
  }
  return env_vars;
}

std::string msvc_wrapper_t::get_program_id() {
  // TODO(m): Add things like executable file size too.

  // Get the version string for the compiler.
  // Just calling "cl.exe" will return the version information. Note, though, that the version
  // information is given on stderr.
  scoped_unset_env_t scoped_off(ENV_VS_OUTPUT_REDIRECTION);

  string_list_t version_args;
  version_args += m_args[0];

  const auto result = sys::run(version_args, true);
  if (result.std_err.empty()) {
    throw std::runtime_error("Unable to get the compiler version information string.");
  }

  return HASH_VERSION + result.std_err;
}

std::map<std::string, expected_file_t> msvc_wrapper_t::get_build_files() {
  std::map<std::string, expected_file_t> files;
  auto found_object_file = false;
  for (const auto& arg : m_resolved_args) {
    if (arg_starts_with(arg, "Fo") && (is_object_file(file::get_extension(arg)))) {
      if (found_object_file) {
        throw std::runtime_error("Only a single target object file can be specified.");
      }
      files["object"] = {drop_leading_colon(arg.substr(3)), true};
      found_object_file = true;
    }
  }
  if (!found_object_file) {
    throw std::runtime_error("Unable to get the target object file.");
  }
  return files;
}

sys::run_result_t msvc_wrapper_t::run_for_miss() {
  // Capture printed source file name (stdout) in cache entry.
  scoped_unset_env_t scoped_off(ENV_VS_OUTPUT_REDIRECTION);
  // Allow FileTracker for the duration of the compilation command.
  filetracker::resume();
  auto result = program_wrapper_t::run_for_miss();
  filetracker::suspend();
  return result;
}

}  // namespace bcache
