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

#ifndef BUILDCACHE_FILE_UTILS_HPP_
#define BUILDCACHE_FILE_UTILS_HPP_

#include <cstdint>
#include <string>
#include <vector>

namespace bcache {
namespace file {
/// @brief A helper class for handling temporary files and directories.
///
/// When the temp file object is created, a temporary file name is generated. Once the object goes
/// out of scope, it removes the file or directory from disk.
class tmp_file_t {
public:
  /// @brief Construct a temporary file name.
  /// @param dir The base directory in which the temporary file will be located.
  /// @param extension The file name extension (including the leading dot).
  tmp_file_t(const std::string& dir, const std::string& extension);

  /// @brief Remove the temporary file or directory (if any).
  ~tmp_file_t();

  const std::string& path() const {
    return m_path;
  }

private:
  std::string m_path;
};

/// @brief Information about a file.
class file_info_t {
public:
  /// @brief File time (seconds since the Unix epoch, i.e. 00:00:00 UTC on 1 January 1970).
  using time_t = int64_t;

  file_info_t(const std::string& path,
              const time_t modify_time,
              const time_t access_time,
              const int64_t size,
              const bool is_dir);

  /// @returns the full path to the file.
  const std::string& path() const {
    return m_path;
  }

  /// @returns the last modification time of the file, or the most recent modification time for any
  /// of the recursively contained files if this is a directory.
  time_t modify_time() const {
    return m_modify_time;
  }

  /// @returns the last access time of the file, or the most recent access time for any of the
  /// recursively contained files if this is a directory.
  time_t access_time() const {
    return m_access_time;
  }

  /// @returns the size of the file (in bytes), or the size of all the recursively contained files
  /// if this is a directory.
  int64_t size() const {
    return m_size;
  }

  /// @returns true if file is a directory.
  int64_t is_dir() const {
    return m_is_dir;
  }

private:
  std::string m_path;
  time_t m_modify_time;
  time_t m_access_time;
  int64_t m_size;
  bool m_is_dir;
};

///@{
/// @brief Append two paths.
/// @param path The base path.
/// @param append The path to be appended (e.g. a file name).
/// @returns the concatenated paths, using the system path separator.
/// @note If @c path is empty or @c append is empty, the result will not contain any path separator.
std::string append_path(const std::string& path, const std::string& append);
std::string append_path(const std::string& path, const char* append);
///@}

/// @brief Get the file extension of a path.
/// @param path The path to a file.
/// @returns The file extension of the file (including the leading period), or an empty string if
/// the file does not have an extension.
std::string get_extension(const std::string& path);

/// @brief Change the file extension of a path.
/// @param path The path to a file.
/// @param new_ext The new file extension (including the leading period) for the file.
/// @returns The path to the file that has the new file extension.
std::string change_extension(const std::string& path, const std::string& new_ext);

/// @brief Get the file name part of a path.
/// @param path The path to a file.
/// @param include_ext Include the file extension in the file name.
/// @returns The part of the path after the final path separator. If the path does not contain a
/// separator, the entire path is returned.
std::string get_file_part(const std::string& path, const bool include_ext = true);

/// @brief Get the directory part of a path.
/// @param path The path to a file.
/// @returns The part of the path before the final path separator. If the path does not contain a
/// separator, an empty string is returned.
std::string get_dir_part(const std::string& path);

/// @brief Get a temporary directory for this user and process.
/// @returns the full path to the temporary directory.
std::string get_temp_dir();

/// @brief Get the user home directory.
/// @returns the full path to the user home directory.
std::string get_user_home_dir();

/// @brief Resolve a path.
///
/// Relative paths are converted into absolute paths, and symbolic links are resolved.
/// @param path The path to resolve.
/// @returns an absolute path to a regular file, or an empty string if the path could not be
/// resolved.
std::string resolve_path(const std::string& path);

/// @brief Find the true path to an executable file.
/// @param path The file to find.
/// @param exclude A file name to exclude (excluding the file extension).
/// @returns an absolute path to the true executable file (symlinks resolved and all).
/// @throws runtime_error if the file could not be found.
std::string find_executable(const std::string& path, const std::string& exclude = std::string());

/// @brief Get file information about a single file or directory.
/// @param path The path to the file (or directory).
/// @returns a file information object.
file_info_t get_file_info(const std::string& path);

/// @brief Convert a size to a human readable string.
/// @param byte_size The size (number of bytes).
/// @returns a string containing a human readable version of the size, e.g. "4.7 MiB".
std::string human_readable_size(const int64_t byte_size);

/// @brief Walk a directory and its subdirectories.
/// @param path The path to the directory.
/// @returns a vector of file information objects.
/// @note Directories are listed after any files that are contained within the directories.
std::vector<file_info_t> walk_directory(const std::string& path);

/// @brief Create a directory.
/// @param path The path to the directory.
/// @throws runtime_error if the directory could not be created.
void create_dir(const std::string& path);

/// @brief Create a directory and its parent directories.
///
/// This function will create parent directories if required (similar to mkdir -p), and if the
/// specified directory does not already exist, it is created (unlike @c create_dir, which will
/// always try to create the directory).
/// @param path The path to the directory.
/// @throws runtime_error if the directory could not be created.
void create_dir_with_parents(const std::string& path);

/// @brief Remove an existing file.
/// @param path The path to the file.
/// @param ignore_errors Set this to true to ignore errors related to removing files.
/// @throws runtime_error if the file could not be removed.
void remove_file(const std::string& path, const bool ignore_errors = false);

/// @brief Remove a directory and all its contents (recursively).
/// @param path The path to the dir.
/// @param ignore_errors Set this to true to ignore errors related to removing files.
/// @throws runtime_error if the dir could not be removed.
void remove_dir(const std::string& path, const bool ignore_errors = false);

/// @brief Check if a directory exists.
/// @param path The path to the directory.
/// @returns true if the directory exists.
bool dir_exists(const std::string& path);

/// @brief Check if a file exists.
/// @param path The path to the file.
/// @returns true if the file exists.
bool file_exists(const std::string& path);

/// @brief Move a file from an old location to a new location.
/// @param from_path The source file.
/// @param to_path The destination file.
/// @throws runtime_error if the operation could not be completed.
void move(const std::string& from_path, const std::string& to_path);

/// @brief Make a full copy of a file.
/// @param from_path The source file.
/// @param to_path The destination file.
/// @throws runtime_error if the operation could not be completed.
void copy(const std::string& from_path, const std::string& to_path);

/// @brief Make a hard link or a full copy of a file.
///
/// A hard link will be performed if possible. Otherwise a full copy will be made.
/// @param from_path The source file.
/// @param to_path The destination file.
/// @throws runtime_error if the operation could not be completed.
void link_or_copy(const std::string& from_path, const std::string& to_path);

/// @brief Read a file into a string.
/// @param path The path to the file.
/// @returns the contents of the file as a string.
/// @throws runtime_error if the operation could not be completed.
std::string read(const std::string& path);

/// @brief Write a string to a file.
/// @param data The data string to write.
/// @param path The path to the file.
/// @throws runtime_error if the operation could not be completed.
void write(const std::string& data, const std::string& path);

/// @brief Append a string to a file.
/// @param data The data string to write.
/// @param path The path to the file.
/// @throws runtime_error if the operation could not be completed.
/// @note Multiple processes may append to the same file concurrently.
void append(const std::string& data, const std::string& path);

/// @brief Create a unique ID string.
/// @returns a string that contains a unique ID.
std::string get_unique_id();

}  // namespace file
}  // namespace bcache

#endif  // BUILDCACHE_FILE_UTILS_HPP_
