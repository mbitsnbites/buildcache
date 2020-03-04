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

#include <base/env_utils.hpp>
#include <base/unicode_utils.hpp>

#include <algorithm>
#include <stdlib.h>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#undef ERROR
#undef log
#endif

namespace bcache {
namespace {
std::string to_lower(const std::string& str) {
  std::string str_lower(str.size(), ' ');
  std::transform(str.begin(), str.end(), str_lower.begin(), ::tolower);
  return str_lower;
}
}  // namespace

env_var_t::env_var_t(const std::string& name) : m_defined(false) {
  if (env_defined(name)) {
    m_value = get_env(name);
    m_defined = true;
  }
}

int64_t env_var_t::as_int64() const {
  return std::stoll(m_value);
}

bool env_var_t::as_bool() const {
  const auto value_lower = to_lower(m_value);
  return m_defined && (!m_value.empty()) && (value_lower != "false") && (value_lower != "no") &&
      (value_lower != "off") && (value_lower != "0");
}

scoped_set_env_t::scoped_set_env_t(const std::string& name, const std::string& value)
    : m_name(name), m_old_env_var(name) {
  set_env(name, value);
}

scoped_set_env_t::~scoped_set_env_t() {
  if (!m_old_env_var) {
    unset_env(m_name);
  } else {
    set_env(m_name, m_old_env_var.as_string());
  }
}

bool env_defined(const std::string& env_var) {
#if defined(_WIN32)
  const auto env_var_w = utf8_to_ucs2(env_var);
  return ::_wgetenv(env_var_w.c_str()) != nullptr;
#else
  return ::getenv(env_var.c_str()) != nullptr;
#endif
}

const std::string get_env(const std::string& env_var) {
#if defined(_WIN32)
  const auto env_var_w = utf8_to_ucs2(env_var);
  const auto* value_w = ::_wgetenv(env_var_w.c_str());
  return value_w != nullptr ? ucs2_to_utf8(std::wstring(value_w)) : std::string();
#else
  const auto* env = ::getenv(env_var.c_str());
  return env != nullptr ? std::string(env) : std::string();
#endif
}

void set_env(const std::string& env_var, const std::string& value) {
#if defined(_WIN32)
  const auto env_var_w = utf8_to_ucs2(env_var);
  const auto value_w = utf8_to_ucs2(value);
  (void)::_wputenv_s(env_var_w.c_str(), value_w.c_str());
#else
  (void)::setenv(env_var.c_str(), value.c_str(), 1);
#endif
}

void unset_env(const std::string& env_var) {
#if defined(_WIN32)
  // According to MSDN: "You can remove a variable from the environment by specifying an empty
  // string".
  const auto env_var_w = utf8_to_ucs2(env_var);
  (void)::_wputenv_s(env_var_w.c_str(), L"");
#else
  (void)::unsetenv(env_var.c_str());
#endif
}

}  // namespace bcache
