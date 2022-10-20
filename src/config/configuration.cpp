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

#include <config/configuration.hpp>

#include <base/env_utils.hpp>
#include <base/file_utils.hpp>

#include <cjson/cJSON.h>

#include <algorithm>
#include <sstream>
#include <stdexcept>

namespace bcache {
namespace {
// Various constants.
const std::string ROOT_FOLDER_NAME = ".buildcache";
const std::string CONFIGURATION_FILE_NAME = "config.json";
const int64_t DEFAULT_MAX_CACHE_SIZE = 5368709120L;        // 5 GiB
const int64_t DEFAULT_MAX_LOCAL_ENTRY_SIZE = 134217728L;   // 128 MiB
const int64_t DEFAULT_MAX_REMOTE_ENTRY_SIZE = 134217728L;  // 128 MiB

// Delimiter character for the LUA_PATH environment variable.
#ifdef _WIN32
const char PATH_DELIMITER_CHR = ';';
#else
const char PATH_DELIMITER_CHR = ':';
#endif
const auto PATH_DELIMITER = std::string(1, PATH_DELIMITER_CHR);

// The configuration file for this configuration.
std::string s_config_file;

// Configuration options.
config::cache_accuracy_t s_accuracy;
bool s_cache_link_commands;
bool s_compress;
config::compress_format_t s_compress_format;
int32_t s_compress_level;
int32_t s_debug;
bool s_disable;
std::string s_dir;
bool s_direct_mode;
bool s_hard_links;
string_list_t s_hash_extra_files;
std::string s_impersonate;
std::string s_log_file;
string_list_t s_lua_paths;
int64_t s_max_cache_size;
int64_t s_max_local_entry_size;
int64_t s_max_remote_entry_size;
bool s_perf;
std::string s_prefix;
bool s_read_only;
bool s_read_only_remote;
std::string s_redis_username;
std::string s_redis_password;
bool s_remote_locks;
std::string s_remote;
std::string s_s3_access;
std::string s_s3_secret;
bool s_terminate_on_miss;

std::string to_lower(const std::string& str) {
  std::string str_lower(str.size(), ' ');
  std::transform(str.begin(), str.end(), str_lower.begin(), ::tolower);
  return str_lower;
}

std::string get_dir() {
  // Is the environment variable BUILDCACHE_DIR defined?
  {
    const env_var_t dir_env("BUILDCACHE_DIR");
    if (dir_env) {
      return dir_env.as_string();
    }
  }

  // Use the user home directory if possible.
  {
    auto home = file::get_user_home_dir();
    if (!home.empty()) {
      // TODO(m): Should use ".cache/buildcache".
      return file::append_path(home, ROOT_FOLDER_NAME);
    }
  }

  // We failed.
  throw std::runtime_error("Unable to determine a home directory for BuildCache.");
}

std::string get_config_file(const std::string& dir) {
  return file::append_path(dir, CONFIGURATION_FILE_NAME);
}

config::cache_accuracy_t to_cache_accuracy(const std::string& str) {
  const auto s = to_lower(str);
  if (s == "strict") {
    return config::cache_accuracy_t::STRICT;
  }
  if (s == "sloppy") {
    return config::cache_accuracy_t::SLOPPY;
  }
  return config::cache_accuracy_t::DEFAULT;
}

config::compress_format_t to_compress_format(const std::string& str) {
  const auto s = to_lower(str);
  if (s == "lz4") {
    return config::compress_format_t::LZ4;
  }
  if (s == "zstd") {
    return config::compress_format_t::ZSTD;
  }
  return config::compress_format_t::DEFAULT;
}

void set_defaults() noexcept {
  s_accuracy = config::cache_accuracy_t::DEFAULT;
  s_cache_link_commands = false;
  s_compress = true;
  s_compress_format = config::compress_format_t::DEFAULT;
  s_compress_level = -1;
  s_debug = -1;
  s_disable = false;
  s_dir = std::string();
  s_direct_mode = false;
  s_hard_links = false;
  s_hash_extra_files = string_list_t();
  s_impersonate = std::string();
  s_log_file = std::string();
  s_lua_paths = string_list_t();
  s_max_cache_size = DEFAULT_MAX_CACHE_SIZE;
  s_max_local_entry_size = DEFAULT_MAX_LOCAL_ENTRY_SIZE;
  s_max_remote_entry_size = DEFAULT_MAX_REMOTE_ENTRY_SIZE;
  s_perf = false;
  s_prefix = std::string();
  s_read_only = false;
  s_read_only_remote = false;
  s_redis_username = std::string();
  s_redis_password = std::string();
  s_remote_locks = false;
  s_remote = std::string();
  s_s3_access = std::string();
  s_s3_secret = std::string();
  s_terminate_on_miss = false;
}

void load_from_file(const std::string& file_name) {
  // Load the configuration file.
  if (!file::file_exists(file_name)) {
    // Nothing to do.
    return;
  }
  const auto data = file::read(file_name);

  // Parse the JSON data.
  auto* root = cJSON_Parse(data.data());
  if (root == nullptr) {
    std::ostringstream ss;
    ss << "Configuration file JSON parse error before:\n";
    const auto* json_error = cJSON_GetErrorPtr();
    if (json_error != nullptr) {
      ss << json_error;
    } else {
      ss << "(N/A)";
    }
    throw std::runtime_error(ss.str());
  }

  {
    const auto* node = cJSON_GetObjectItemCaseSensitive(root, "accuracy");
    if ((cJSON_IsString(node) != 0) && node->valuestring != nullptr) {
      s_accuracy = to_cache_accuracy(std::string(node->valuestring));
    }
  }

  {
    const auto* node = cJSON_GetObjectItemCaseSensitive(root, "cache_link_commands");
    if (cJSON_IsBool(node) != 0) {
      s_cache_link_commands = (cJSON_IsTrue(node) != 0);
    }
  }

  {
    const auto* node = cJSON_GetObjectItemCaseSensitive(root, "compress");
    if (cJSON_IsBool(node) != 0) {
      s_compress = (cJSON_IsTrue(node) != 0);
    }
  }

  {
    const auto* node = cJSON_GetObjectItemCaseSensitive(root, "compress_format");
    if ((cJSON_IsString(node) != 0) && node->valuestring != nullptr) {
      s_compress_format = to_compress_format(std::string(node->valuestring));
    }
  }

  {
    const auto* node = cJSON_GetObjectItemCaseSensitive(root, "compress_level");
    if (cJSON_IsNumber(node) != 0) {
      s_compress_level = static_cast<int32_t>(node->valueint);
    }
  }

  {
    const auto* node = cJSON_GetObjectItemCaseSensitive(root, "debug");
    if (cJSON_IsNumber(node) != 0) {
      s_debug = static_cast<int32_t>(node->valueint);
    }
  }

  {
    const auto* node = cJSON_GetObjectItemCaseSensitive(root, "direct_mode");
    if (cJSON_IsBool(node) != 0) {
      s_direct_mode = (cJSON_IsTrue(node) != 0);
    }
  }

  {
    const auto* node = cJSON_GetObjectItemCaseSensitive(root, "disable");
    if (cJSON_IsBool(node) != 0) {
      s_disable = (cJSON_IsTrue(node) != 0);
    }
  }

  {
    const auto* node = cJSON_GetObjectItemCaseSensitive(root, "hard_links");
    if (cJSON_IsBool(node) != 0) {
      s_hard_links = (cJSON_IsTrue(node) != 0);
    }
  }

  {
    const auto* node = cJSON_GetObjectItemCaseSensitive(root, "hash_extra_files");
    cJSON* child_node;
    cJSON_ArrayForEach(child_node, node) {
      const auto str = std::string(child_node->valuestring);
      s_hash_extra_files += str;
    }
  }

  {
    const auto* node = cJSON_GetObjectItemCaseSensitive(root, "impersonate");
    if ((cJSON_IsString(node) != 0) && node->valuestring != nullptr) {
      s_impersonate = std::string(node->valuestring);
    }
  }

  {
    const auto* node = cJSON_GetObjectItemCaseSensitive(root, "log_file");
    if ((cJSON_IsString(node) != 0) && node->valuestring != nullptr) {
      s_log_file = std::string(node->valuestring);
    }
  }

  {
    const auto* node = cJSON_GetObjectItemCaseSensitive(root, "lua_paths");
    cJSON* child_node;
    cJSON_ArrayForEach(child_node, node) {
      const auto str = std::string(child_node->valuestring);
      s_lua_paths += str;
    }
  }

  {
    const auto* node = cJSON_GetObjectItemCaseSensitive(root, "max_cache_size");
    if (cJSON_IsNumber(node) != 0) {
      s_max_cache_size = static_cast<int64_t>(node->valuedouble);
    }
  }

  {
    const auto* node = cJSON_GetObjectItemCaseSensitive(root, "max_local_entry_size");
    if (cJSON_IsNumber(node) != 0) {
      s_max_local_entry_size = static_cast<int64_t>(node->valuedouble);
    }
  }

  {
    const auto* node = cJSON_GetObjectItemCaseSensitive(root, "max_remote_entry_size");
    if (cJSON_IsNumber(node) != 0) {
      s_max_remote_entry_size = static_cast<int64_t>(node->valuedouble);
    }
  }

  {
    const auto* node = cJSON_GetObjectItemCaseSensitive(root, "perf");
    if (cJSON_IsBool(node) != 0) {
      s_perf = (cJSON_IsTrue(node) != 0);
    }
  }

  {
    const auto* node = cJSON_GetObjectItemCaseSensitive(root, "prefix");
    if ((cJSON_IsString(node) != 0) && node->valuestring != nullptr) {
      s_prefix = std::string(node->valuestring);
    }
  }

  {
    const auto* node = cJSON_GetObjectItemCaseSensitive(root, "read_only");
    if (cJSON_IsBool(node) != 0) {
      s_read_only = (cJSON_IsTrue(node) != 0);
    }
  }

  {
    const auto* node = cJSON_GetObjectItemCaseSensitive(root, "read_only_remote");
    if (cJSON_IsBool(node) != 0) {
      s_read_only_remote = (cJSON_IsTrue(node) != 0);
    }
  }

  {
    const auto* node = cJSON_GetObjectItemCaseSensitive(root, "redis_username");
    if ((cJSON_IsString(node) != 0) && node->valuestring != nullptr) {
      s_redis_username = std::string(node->valuestring);
    }
  }

  {
    const auto* node = cJSON_GetObjectItemCaseSensitive(root, "redis_password");
    if ((cJSON_IsString(node) != 0) && node->valuestring != nullptr) {
      s_redis_password = std::string(node->valuestring);
    }
  }

  {
    const auto* node = cJSON_GetObjectItemCaseSensitive(root, "remote");
    if ((cJSON_IsString(node) != 0) && node->valuestring != nullptr) {
      s_remote = std::string(node->valuestring);
    }
  }

  {
    const auto* node = cJSON_GetObjectItemCaseSensitive(root, "remote_locks");
    if (cJSON_IsBool(node) != 0) {
      s_remote_locks = (cJSON_IsTrue(node) != 0);
    }
  }

  {
    const auto* node = cJSON_GetObjectItemCaseSensitive(root, "s3_access");
    if ((cJSON_IsString(node) != 0) && node->valuestring != nullptr) {
      s_s3_access = std::string(node->valuestring);
    }
  }

  {
    const auto* node = cJSON_GetObjectItemCaseSensitive(root, "s3_secret");
    if ((cJSON_IsString(node) != 0) && node->valuestring != nullptr) {
      s_s3_secret = std::string(node->valuestring);
    }
  }

  {
    const auto* node = cJSON_GetObjectItemCaseSensitive(root, "terminate_on_miss");
    if (cJSON_IsBool(node) != 0) {
      s_terminate_on_miss = (cJSON_IsTrue(node) != 0);
    }
  }

  cJSON_Delete(root);
}
}  // namespace

namespace config {
std::string to_string(const cache_accuracy_t accuracy) {
  switch (accuracy) {
    case cache_accuracy_t::STRICT:
      return "STRICT";
    case cache_accuracy_t::DEFAULT:
      return "DEFAULT";
    case cache_accuracy_t::SLOPPY:
      return "SLOPPY";
    default:
      return "?";
  }
}

std::string to_string(const compress_format_t format) {
  switch (format) {
    case compress_format_t::LZ4:
      return "LZ4";
    case compress_format_t::ZSTD:
      return "ZSTD";
    case compress_format_t::DEFAULT:
      return "DEFAULT";
    default:
      return "?";
  }
}

void init(const char* bcache_dir) {
  // Initialize the configuration to default values. Note that we MUST guarantee that this is
  // performed before any configuration value is queried in order to avoid using undefined values.
  set_defaults();

  try {
    // Get the BuildCache home directory.
    s_dir = bcache_dir != nullptr ? std::string(bcache_dir) : get_dir();

    // Load any paramaters from the user configuration file.
    // Note: We do this before reading the configuration from the environment, so that the
    // environment overrides the configuration file.
    s_config_file = get_config_file(s_dir);
    load_from_file(s_config_file);

    {
      const env_var_t env("BUILDCACHE_ACCURACY");
      if (env) {
        s_accuracy = to_cache_accuracy(env.as_string());
      }
    }

    {
      const env_var_t env("BUILDCACHE_CACHE_LINK_COMMANDS");
      if (env) {
        s_cache_link_commands = env.as_bool();
      }
    }

    {
      const env_var_t env("BUILDCACHE_COMPRESS");
      if (env) {
        s_compress = env.as_bool();
      }
    }

    {
      const env_var_t env("BUILDCACHE_COMPRESS_FORMAT");
      if (env) {
        s_compress_format = to_compress_format(env.as_string());
      }
    }

    {
      const env_var_t env("BUILDCACHE_COMPRESS_LEVEL");
      if (env) {
        try {
          s_compress_level = static_cast<int32_t>(env.as_int64());
        } catch (...) {
          // Ignore...
        }
      }
    }

    {
      const env_var_t env("BUILDCACHE_DEBUG");
      if (env) {
        try {
          s_debug = static_cast<int32_t>(env.as_int64());
        } catch (...) {
          // Ignore...
        }
      }
    }

    {
      const env_var_t env("BUILDCACHE_DIRECT_MODE");
      if (env) {
        s_direct_mode = env.as_bool();
      }
    }

    {
      const env_var_t env("BUILDCACHE_DISABLE");
      if (env) {
        s_disable = env.as_bool();
      }
    }

    {
      const env_var_t env("BUILDCACHE_HARD_LINKS");
      if (env) {
        s_hard_links = env.as_bool();
      }
    }

    {
      const env_var_t env("BUILDCACHE_HASH_EXTRA_FILES");
      if (env) {
        s_hash_extra_files = string_list_t(env.as_string(), PATH_DELIMITER) + s_hash_extra_files;
      }
    }

    {
      const env_var_t env("BUILDCACHE_IMPERSONATE");
      if (env) {
        s_impersonate = env.as_string();
      }
    }

    {
      const env_var_t env("BUILDCACHE_LOG_FILE");
      if (env) {
        s_log_file = env.as_string();
      }
    }

    {
      const env_var_t env("BUILDCACHE_LUA_PATH");
      if (env) {
        s_lua_paths = string_list_t(env.as_string(), PATH_DELIMITER) + s_lua_paths;
      }
    }

    {
      const env_var_t env("BUILDCACHE_MAX_CACHE_SIZE");
      if (env) {
        try {
          s_max_cache_size = env.as_int64();
        } catch (...) {
          // Ignore...
        }
      }
    }

    {
      const env_var_t env("BUILDCACHE_MAX_LOCAL_ENTRY_SIZE");
      if (env) {
        try {
          s_max_local_entry_size = env.as_int64();
        } catch (...) {
          // Ignore...
        }
      }
    }

    {
      const env_var_t env("BUILDCACHE_MAX_REMOTE_ENTRY_SIZE");
      if (env) {
        try {
          s_max_remote_entry_size = env.as_int64();
        } catch (...) {
          // Ignore...
        }
      }
    }

    {
      const env_var_t env("BUILDCACHE_PERF");
      if (env) {
        s_perf = env.as_bool();
      }
    }

    {
      const env_var_t env("BUILDCACHE_PREFIX");
      if (env) {
        s_prefix = env.as_string();
      }
    }

    {
      const env_var_t env("BUILDCACHE_READ_ONLY");
      if (env) {
        s_read_only = env.as_bool();
      }
    }

    {
      const env_var_t env("BUILDCACHE_READ_ONLY_REMOTE");
      if (env) {
        s_read_only_remote = env.as_bool();
      }
    }

    {
      const env_var_t env("BUILDCACHE_REDIS_USERNAME");
      if (env) {
        s_redis_username = env.as_string();
      }
    }

    {
      const env_var_t env("BUILDCACHE_REDIS_PASSWORD");
      if (env) {
        s_redis_password = env.as_string();
      }
    }

    {
      const env_var_t env("BUILDCACHE_REMOTE");
      if (env) {
        s_remote = env.as_string();
      }
    }

    {
      const env_var_t env("BUILDCACHE_REMOTE_LOCKS");
      if (env) {
        s_remote_locks = env.as_bool();
      }
    }

    {
      const env_var_t env("BUILDCACHE_S3_ACCESS");
      if (env) {
        s_s3_access = env.as_string();
      }
    }

    {
      const env_var_t env("BUILDCACHE_S3_SECRET");
      if (env) {
        s_s3_secret = env.as_string();
      }
    }

    {
      const env_var_t env("BUILDCACHE_TERMINATE_ON_MISS");
      if (env) {
        s_terminate_on_miss = env.as_bool();
      }
    }

    // We also look for Lua files in the cache root dir (i.e. ${BUILDCACHE_DIR}/lua).
    // Note: We give the default Lua path the lowest priority.
    s_lua_paths += file::append_path(s_dir, "lua");
  } catch (...) {
    // If we could not initialize the configuration, we can't proceed. We need to disable the cache.
    s_disable = true;
    throw;
  }
}

const std::string& config_file() {
  return s_config_file;
}

cache_accuracy_t accuracy() {
  return s_accuracy;
}

bool cache_link_commands() {
  return s_cache_link_commands;
}

bool compress() {
  return s_compress;
}

compress_format_t compress_format() {
  return s_compress_format;
}

int32_t compress_level() {
  return s_compress_level;
}

int32_t debug() {
  return s_debug;
}

const std::string& dir() {
  return s_dir;
}

bool direct_mode() {
  return s_direct_mode;
}

bool disable() {
  return s_disable;
}

bool hard_links() {
  return s_hard_links;
}

string_list_t hash_extra_files() {
  return s_hash_extra_files;
}

const std::string& impersonate() {
  return s_impersonate;
}

const std::string& log_file() {
  return s_log_file;
}

const string_list_t& lua_paths() {
  return s_lua_paths;
}

int64_t max_cache_size() {
  return s_max_cache_size;
}

int64_t max_local_entry_size() {
  return s_max_local_entry_size;
}

int64_t max_remote_entry_size() {
  return s_max_remote_entry_size;
}

bool perf() {
  return s_perf;
}

const std::string& prefix() {
  return s_prefix;
}

bool read_only() {
  return s_read_only;
}

const std::string& redis_username() {
  return s_redis_username;
}

const std::string& redis_password() {
  return s_redis_password;
}

bool read_only_remote() {
  return s_read_only_remote;
}

const std::string& remote() {
  return s_remote;
}

bool remote_locks() {
  return s_remote_locks;
}

const std::string& s3_access() {
  return s_s3_access;
}

const std::string& s3_secret() {
  return s_s3_secret;
}

bool terminate_on_miss() {
  return s_terminate_on_miss;
}

}  // namespace config
}  // namespace bcache
