//--------------------------------------------------------------------------------------------------
// Copyright (c) 2019 Marcus Geelnard
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

#ifndef BUILDCACHE_S3_CACHE_PROVIDER_HPP_
#define BUILDCACHE_S3_CACHE_PROVIDER_HPP_

#include <cache/http_cache_provider.hpp>

namespace bcache {

class s3_cache_provider_t : public http_cache_provider_t {
public:
  // Override S3 specific parts of the http_cache_provider_t.
  bool connect(const std::string& host_description) override;

private:
  /// @brief Sign a string (to create an AWS authorization string).
  /// @param str The string to sign.
  /// @returns the signature of the string.
  std::string sign_string(const std::string& str) const;

  std::vector<std::string> get_header(const std::string& method,
                                      const std::string& key) const override;

  std::string m_access;
  std::string m_secret;
};

}  // namespace bcache

#endif  // BUILDCACHE_S3_CACHE_PROVIDER_HPP_
