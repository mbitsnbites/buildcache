//--------------------------------------------------------------------------------------------------
// Copyright (c) 2020 Marcus Geelnard
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

#ifndef BUILDCACHE_ENV_UTILS_HPP_
#define BUILDCACHE_ENV_UTILS_HPP_

#include <string>

namespace bcache {
/// @brief A helper class for reading and parsing environment variables.
class env_var_t {
public:
  /// @brief Read an environment variable.
  /// @param name The name of the environment variable.
  env_var_t(const std::string& name);

  /// @returns true if the environment variable was defined.
  operator bool() const {
    return m_defined;
  }

  /// @returns the environment variable value as a string.
  const std::string& as_string() const {
    return m_value;
  }

  /// @returns the environment variable value as an integer.
  int64_t as_int64() const;

  /// @returns the environment variable value as a boolean value.
  bool as_bool() const;

private:
  std::string m_value;
  bool m_defined;
};

/// @brief A class for temporarily modifying an environment variable.
class scoped_set_env_t {
public:
  /// @brief Temporarily set an environment variable.
  /// @param name The name of the environment variable.
  /// @param value The value for the environment variable.
  scoped_set_env_t(const std::string& name, const std::string& value);

  /// @brief Restore the environment variable to its old value.
  ~scoped_set_env_t();

private:
  std::string m_name;
  env_var_t m_old_env_var;
};

/// @brief Check if the named environment variable is defined.
/// @param env_var Name of the environment variable.
/// @returns true if the environment variable was defined, otherwise false.
bool env_defined(const std::string& env_var);

/// @brief Get the named environment variable for this process.
/// @param env_var Name of the environment variable.
/// @returns the value as a string, or an empty string if the environment variable was not defined.
/// @note Use @link env_defined @endlink to check if an environment variable is defined.
const std::string get_env(const std::string& env_var);

/// @brief Set the named environment variable for this process.
/// @param env_var Name of the environment variable.
/// @param value Value of the environment variable.
void set_env(const std::string& env_var, const std::string& value);

/// @brief Unset the named environment variable for this process.
/// @param env_var Name of the environment variable.
void unset_env(const std::string& env_var);

}  // namespace bcache

#endif  // BUILDCACHE_ENV_UTILS_HPP_
