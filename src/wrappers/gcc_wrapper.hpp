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

#ifndef BUILDCACHE_GCC_WRAPPER_HPP_
#define BUILDCACHE_GCC_WRAPPER_HPP_

#include <wrappers/program_wrapper.hpp>

namespace bcache {
/// @brief A program wrapper for GCC and GCC-like C/C++ compilers.
class gcc_wrapper_t : public program_wrapper_t {
public:
  /// @brief Compatible mode.
  gcc_wrapper_t(const file::exe_path_t& exe_path, const string_list_t& args);

  bool can_handle_command() override;

protected:
  string_list_t get_capabilities() override;
  std::map<std::string, expected_file_t> get_build_files() override;
  std::string get_program_id() override;
  string_list_t get_relevant_arguments() override;
  std::map<std::string, std::string> get_relevant_env_vars() override;
  string_list_t get_input_files() override;
  std::string preprocess_source() override;
  string_list_t get_implicit_input_files() override;

  /// @brief Get whether -D flags are consumed during preprocess step
  ///
  /// Some compilers support an optimization to not expand macro definitions
  /// during a preprocess step. When determining relevent arguments for hashing,
  /// it is important to note whether the -D command line flag will be consumed
  /// during preprocess and therefore result in changes to the preprocess output
  /// or not.
  /// @returns true if preprocess step will consume -D arguments
  ///
  /// @note Only used in preprocess mode
  virtual bool uses_defines_in_preprocess() const;

  string_list_t m_resolved_args;
  string_list_t m_implicit_input_files;

private:
  enum class compatible_mode_t {
    NOT_SPECIFIED = 0,  ///< Don't use any specific flags
    GCC = 1,            ///< Can use GCC compatible flags
    CLANG = 2,          ///< Can use Clang compatible flags
  };
  compatible_mode_t m_compatible_mode = compatible_mode_t::NOT_SPECIFIED;

  void resolve_args() override;
  string_list_t parse_args(const string_list_t& args);
  string_list_t parse_response_file(const std::string& filename);
  virtual string_list_t get_include_files(const std::string& std_err) const;
  virtual string_list_t make_preprocessor_cmd(const std::string& preprocessed_file) const;
};
}  // namespace bcache
#endif  // BUILDCACHE_GCC_WRAPPER_HPP_
