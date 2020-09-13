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
  gcc_wrapper_t(const string_list_t& args);

  bool can_handle_command() override;

protected:
  string_list_t get_capabilities() override;
  pp_sources_t preprocess_source() override;
  string_list_t get_relevant_arguments() override;
  std::map<std::string, std::string> get_relevant_env_vars() override;
  std::string get_program_id() override;
  build_files_t get_build_files(const pp_key_t& key) override;
};
}  // namespace bcache
#endif  // BUILDCACHE_GCC_WRAPPER_HPP_
