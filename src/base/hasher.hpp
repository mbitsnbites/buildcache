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

#ifndef BUILDCACHE_HASHER_HPP_
#define BUILDCACHE_HASHER_HPP_

#include <base/string_list.hpp>

#include <cstdint>
#include <map>
#include <string>

extern "C" {
#include <md4/md4.h>
}

namespace bcache {

/// @brief A class for hashing data.
class hasher_t {
public:
  /// @brief A helper class for storing the data hash.
  class hash_t {
  public:
    uint8_t* data() {
      return &m_data[0];
    }

    const uint8_t* data() const {
      return &m_data[0];
    }

    /// @brief Convert a hash to a hexadecimal string.
    /// @returns A hexadecimal string representation of the given hash.
    const std::string as_string() const;

  private:
    // The hash size is 128 bits.
    static const size_t SIZE = 16u;

    uint8_t m_data[SIZE];
  };

  hasher_t() {
    MD4_Init(&m_ctx);
  }

  /// @brief Update the hash with more data.
  /// @param data Pointer to the data to hash.
  /// @param size The number of bytes to hash.
  void update(const void* data, const size_t size) {
    MD4_Update(&m_ctx, data, static_cast<unsigned long>(size));
  }

  /// @brief Update the hash with more data.
  /// @param text The data to hash.
  void update(const std::string& text) {
    update(text.data(), text.size());
  }

  /// @brief Update the hash with more data.
  /// @param data The data to hash.
  void update(const std::map<std::string, std::string>& data);

  /// @brief Update the hash with more data.
  /// @param path Path to a file that contains the data to hash.
  /// @throws runtime_error if the operation could not be completed.
  void update_from_file(const std::string& path);

  /// @brief Update the hash with more data.
  /// @param paths list of paths to a files that contains the data to hash.
  /// @throws runtime_error if the operation could not be completed.
  void update_from_files(const string_list_t& paths);

  /// @brief Update the hash with more data.
  ///
  /// This method tries to produce a deterministic hash by employing file format specific heuristics
  /// to exclude things like time stamps.
  /// @param path Path to a file that contains the data to hash.
  /// @throws runtime_error if the operation could not be completed.
  void update_from_file_deterministic(const std::string& path);

  /// @brief Finalize the hash calculation.
  /// @returns the result of the hash.
  /// @note This method must only be called once.
  hash_t final() {
    hash_t result;
    MD4_Final(result.data(), &m_ctx);
    return result;
  }

private:
  /// @brief Update the hash with data from an AR archive.
  /// @param data The raw AR data.
  void update_from_ar_data(const std::string &data);

  MD4_CTX m_ctx;
};

}  // namespace bcache

#endif  // BUILDCACHE_HASHER_HPP_
