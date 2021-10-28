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

#ifndef BUILDCACHE_GHS_WRAPPER_HPP_
#define BUILDCACHE_GHS_WRAPPER_HPP_

#include <wrappers/gcc_wrapper.hpp>

namespace bcache {
/// @brief This is a wrapper for the Green Hills Software C/C++ compiler.
///
/// The GHS compiler is fairly compatible with the GCC wrapper, so we derive from it.
class ghs_wrapper_t : public gcc_wrapper_t {
public:
  ghs_wrapper_t(const file::exe_path_t& exe_path, const string_list_t& args);

  bool can_handle_command() override;

private:
  std::set<std::string> get_include_files(const std::string& std_err) const override;

  string_list_t get_capabilities() override;
  std::string get_program_id() override;
  string_list_t get_relevant_arguments() override;
  std::map<std::string, std::string> get_relevant_env_vars() override;
};
}  // namespace bcache

#endif  // BUILDCACHE_GHS_WRAPPER_HPP_
