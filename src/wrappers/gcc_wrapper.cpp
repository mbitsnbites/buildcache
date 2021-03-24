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

#include <wrappers/gcc_wrapper.hpp>

#include <base/debug_utils.hpp>
#include <base/unicode_utils.hpp>
#include <config/configuration.hpp>
#include <sys/sys_utils.hpp>

#include <fstream>
#include <regex>
#include <set>
#include <stdexcept>

namespace bcache {
namespace {
// Tick this to a new number if the format has changed in a non-backwards-compatible way.
const std::string HASH_VERSION = "3";

bool is_arg_plus_file_name(const std::string& arg) {
  // Is this an argument that is followed by a file path?
  static const std::set<std::string> path_args = {"-I", "-MF", "-MT", "-MQ", "-o"};
  return path_args.find(arg) != path_args.end();
}

bool is_arg_pair(const std::string& arg) {
  // Is this an argument that is followed by an option?
  // TODO(m): Recognize more arg pairs.
  return is_arg_plus_file_name(arg);
}

bool is_source_file(const std::string& arg) {
  const auto ext = lower_case(file::get_extension(arg));
  return ((ext == ".cpp") || (ext == ".cc") || (ext == ".cxx") || (ext == ".c"));
}

bool has_debug_symbols(const string_list_t& args) {
  // TODO(m): Handle more debug options (e.g. -g0, -gxcoff3, ...).
  const std::set<std::string> debug_options = {"-g",
                                               "-ggdb",
                                               "-gdwarf",
                                               "-gdwarf-2",
                                               "-gdwarf-3",
                                               "-gdwarf-4",
                                               "-gdwarf-5",
                                               "-gstabs",
                                               "-gstabs+",
                                               "-gxcoff",
                                               "-gxcoff+",
                                               "-gvms"};

  for (const auto& arg : args) {
    if (debug_options.find(arg) != debug_options.end()) {
      return true;
    }
  }
  return false;
}

bool has_coverage_output(const string_list_t& args) {
  const std::set<std::string> coverage_options = {
      "-ftest-coverage", "-fprofile-arcs", "--coverage"};

  for (const auto& arg : args) {
    if (coverage_options.find(arg) != coverage_options.end()) {
      return true;
    }
  }
  return false;
}

string_list_t make_preprocessor_cmd(const string_list_t& args,
                                    const std::string& preprocessed_file) {
  string_list_t preprocess_args;

  // Drop arguments that we do not want/need.
  bool drop_next_arg = false;
  for (auto it = args.begin(); it != args.end(); ++it) {
    auto arg = *it;
    auto drop_this_arg = drop_next_arg;
    drop_next_arg = false;
    if (arg == "-c") {
      drop_this_arg = true;
    } else if (arg == "-o") {
      drop_this_arg = true;
      drop_next_arg = true;
    } else if (arg == "-MF") {
      drop_this_arg = true;
      drop_next_arg = true;
    }
    if (!drop_this_arg) {
      preprocess_args += arg;
    }
  }

  // Should we inhibit line info in the preprocessed output?
  const bool debug_symbols_required =
      has_debug_symbols(args) && (config::accuracy() >= config::cache_accuracy_t::STRICT);
  const bool coverage_symbols_required =
      has_coverage_output(args) && (config::accuracy() >= config::cache_accuracy_t::DEFAULT);
  const bool inhibit_line_info = !(debug_symbols_required || coverage_symbols_required);

  // Append the required arguments for producing preprocessed output.
  preprocess_args += std::string("-E");
  if (inhibit_line_info) {
    preprocess_args += std::string("-P");
  }
  preprocess_args += std::string("-o");
  preprocess_args += preprocessed_file;

  // Add argument for listing include files (used for direct mode).
  preprocess_args += std::string("-H");  // Supported by gcc, clang and ghc

  return preprocess_args;
}
}  // namespace

gcc_wrapper_t::gcc_wrapper_t(const file::exe_path_t& exe_path, const string_list_t& args)
    : program_wrapper_t(exe_path, args) {
}

void gcc_wrapper_t::resolve_args() {
  // Iterate over all args and load any response files that we encounter.
  m_resolved_args.clear();
  m_resolved_args += parse_args(m_args);
}

string_list_t gcc_wrapper_t::parse_args(const string_list_t& args) {
  string_list_t parsed_args;

  for (const auto& arg : args) {
    if (arg.substr(0, 1) == "@") {
      parsed_args += parse_response_file(arg.substr(1));
    } else {
      parsed_args += arg;
    }
  }

  return parsed_args;
}

string_list_t gcc_wrapper_t::parse_response_file(const std::string& filename) {
  string_list_t parsed_file_contents;

  std::ifstream response_file(filename);
  if (response_file.is_open()) {
    std::string line;
    while (std::getline(response_file, line)) {
      parsed_file_contents += parse_args(string_list_t::split_args(line));
    }
  } else {
    // Unable to open the specified file.  GCC says to leave the argument parameter as-is.
    parsed_file_contents += (std::string("@") + filename);
  }

  return parsed_file_contents;
}

string_list_t gcc_wrapper_t::get_include_files(const std::string& std_err) const {
  // Turn the std_err string into a list of strings.
  // TODO(m): Is this correct on Windows for instance?
  string_list_t lines(std_err, "\n");

  // Extract all unique include paths. Include path references in std_err start with one or more
  // periods (.) followed by a single space character, and finally the full path. In the regex we
  // also trim leading and trailing whitespaces from the path, just for good measure.
  const std::regex incpath_re("\\.+\\s+(.*[^\\s])\\s*");
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

bool gcc_wrapper_t::can_handle_command() {
  // Is this the right compiler?
  // Note: We keep the file extension part to support version strings in the executable file name,
  // such as "aarch64-unknown-nto-qnx7.0.0-g++".
  const auto cmd = lower_case(file::get_file_part(m_exe_path.real_path(), true));

  // gcc?
  if ((cmd.find("gcc") != std::string::npos) || (cmd.find("g++") != std::string::npos)) {
    return true;
  }

  // clang?
  {
    // We can't handle clang-cl style arguments (it's handled by the MSVC wrapper). We check the
    // virtual_path rather than the real path, since clang-cl may be invoked as a symlink to clang.
    const auto virt_cmd = lower_case(file::get_file_part(m_exe_path.virtual_path(), false));
    if (virt_cmd == "clang-cl") {
      return false;
    }

    // We allow things like "clang", "clang++", "clang-5", "x86-clang-6.0", but not "clang-tidy"
    // and similar.
    const std::regex clang_re(".*clang(\\+\\+|-cpp)?(-[1-9][0-9]*(\\.[0-9]+)*)?(\\.exe)?");
    if (std::regex_match(cmd, clang_re)) {
      return true;
    }
  }

  return false;
}

string_list_t gcc_wrapper_t::get_capabilities() {
  // direct_mode - We support direct mode.
  // hard_links  - We can use hard links since GCC will never overwrite already existing files.
  return string_list_t{"direct_mode", "hard_links"};
}

std::map<std::string, expected_file_t> gcc_wrapper_t::get_build_files() {
  std::map<std::string, expected_file_t> files;
  auto found_object_file = false;
  for (size_t i = 0U; i < m_resolved_args.size(); ++i) {
    const auto next_idx = i + 1U;
    if ((m_resolved_args[i] == "-o") && (next_idx < m_resolved_args.size())) {
      if (found_object_file) {
        throw std::runtime_error("Only a single target object file can be specified.");
      }
      files["object"] = {m_resolved_args[next_idx], true};
      found_object_file = true;
    }
  }
  if (!found_object_file) {
    throw std::runtime_error("Unable to get the target object file.");
  }
  if (has_coverage_output(m_resolved_args)) {
    files["coverage"] = {file::change_extension(files["object"].path(), ".gcno"), true};
  }
  return files;
}

std::string gcc_wrapper_t::get_program_id() {
  // TODO(m): Add things like executable file size too.

  // Get the version string for the compiler.
  string_list_t version_args;
  version_args += m_args[0];
  version_args += "--version";
  const auto result = sys::run(version_args);
  if (result.return_code != 0) {
    throw std::runtime_error("Unable to get the compiler version information string.");
  }

  // Prepend the hash format version.
  auto id = HASH_VERSION + result.std_out;

  return id;
}

string_list_t gcc_wrapper_t::get_relevant_arguments() {
  string_list_t filtered_args;

  // The first argument is the compiler binary without the path.
  filtered_args += file::get_file_part(m_args[0]);

  // Note: We always skip the first arg since we have handled it already.
  bool skip_next_arg = true;
  for (const auto& arg : m_resolved_args) {
    if (!skip_next_arg) {
      // Generally unwanted argument (things that will not change how we go from preprocessed code
      // to binary object files)?
      const auto first_two_chars = arg.substr(0, 2);
      const bool is_unwanted_arg =
          ((first_two_chars == "-I") || (first_two_chars == "-D") || (first_two_chars == "-M") ||
           (arg.substr(0, 10) == "--sysroot=") || is_source_file(arg));

      if (is_arg_plus_file_name(arg)) {
        // We don't want to hash file paths.
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

std::map<std::string, std::string> gcc_wrapper_t::get_relevant_env_vars() {
  // TODO(m): What environment variables can affect the build result?
  std::map<std::string, std::string> env_vars;
  return env_vars;
}

string_list_t gcc_wrapper_t::get_input_files() {
  string_list_t input_files;

  // Iterate over the command line arguments to find input files.
  // Note: We always skip the first arg (it's the program executable).
  bool skip_next_arg = true;
  for (const auto& arg : m_resolved_args) {
    if (!skip_next_arg) {
      if (is_arg_pair(arg)) {
        skip_next_arg = true;
      } else if (is_source_file(arg)) {
        input_files += file::resolve_path(arg);
      }
    } else {
      skip_next_arg = false;
    }
  }
  return input_files;
}

std::string gcc_wrapper_t::preprocess_source() {
  // Check if this is a compilation command that we support.
  auto is_object_compilation = false;
  auto has_object_output = false;
  for (const auto& arg : m_resolved_args) {
    if (arg == "-c") {
      is_object_compilation = true;
    } else if (arg == "-o") {
      has_object_output = true;
    }
  }
  if ((!is_object_compilation) || (!has_object_output)) {
    throw std::runtime_error("Unsupported complation command.");
  }

  // Run the preprocessor step.
  file::tmp_file_t preprocessed_file(sys::get_local_temp_folder(), ".i");
  const auto preprocessor_args = make_preprocessor_cmd(m_resolved_args, preprocessed_file.path());
  auto result = sys::run(preprocessor_args);
  if (result.return_code != 0) {
    throw std::runtime_error("Preprocessing command was unsuccessful.");
  }

  // Collect all the input files. They are reported in std_err.
  m_implicit_input_files = get_include_files(result.std_err);

  // Read and return the preprocessed file.
  return file::read(preprocessed_file.path());
}

string_list_t gcc_wrapper_t::get_implicit_input_files() {
  return m_implicit_input_files;
}
}  // namespace bcache
