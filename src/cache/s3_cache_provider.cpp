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

#include <base/debug_utils.hpp>
#include <base/hmac.hpp>
#include <cache/s3_cache_provider.hpp>
#include <config/configuration.hpp>

#include <cpp-base64/base64.h>
#include <ctime>

#include <sstream>
#include <stdexcept>

namespace bcache {

std::string get_date_rfc2616_gmt() {
  // TODO(m): setlocale() is not guaranteed to be thread safe. Can we do this in a more thread safe
  // manner?

  // Set the locale to "C" (and save old locale).
  const auto* old_locale_ptr = ::setlocale(LC_ALL, nullptr);
  const auto old_locale =
      old_locale_ptr ? std::string(::setlocale(LC_ALL, nullptr)) : std::string();
  ::setlocale(LC_ALL, "C");

  // Get the current date & time.
  time_t now = ::time(0);
  struct tm tm = *::gmtime(&now);

  // Format the date & time according to RFC2616.
  char buf[100];
  if (::strftime(buf, sizeof(buf), "%a, %d %b %Y %H:%M:%S GMT", &tm) == 0) {
    throw std::runtime_error("strftime failed");
  }

  // Restore the old locale.
  ::setlocale(LC_ALL, old_locale.c_str());

  return std::string(&buf[0]);
}

bool s3_cache_provider_t::connect(const std::string& host_description) {
  // Get the access keys.
  if (config::s3_access().empty() || config::s3_secret().empty()) {
    debug::log(debug::ERROR)
        << "Missing S3 credentials (define BUILDCACHE_S3_ACCESS and BUILDCACHE_S3_SECRET)";
    return false;
  }
  m_access = config::s3_access();
  m_secret = config::s3_secret();

  return http_cache_provider_t::connect(host_description);
}

std::vector<std::string> s3_cache_provider_t::get_header(const std::string& method,
                                                         const std::string& key) const {
  // Gather information for this request.
  const std::string date_formatted = get_date_rfc2616_gmt();
  const std::string relative_path = get_path() + "/" + key;
  const std::string content_type = "application/octet-stream";

  // Generate a signature based on the S3 secret key.
  const std::string string_to_sign =
      method + "\n\n" + content_type + "\n" + date_formatted + "\n" + relative_path;
  const auto signature = sign_string(string_to_sign);

  return {"Date: " + date_formatted,
          "Content-Type: " + content_type,
          "Authorization: AWS " + m_access + ":" + signature};
}

std::string s3_cache_provider_t::sign_string(const std::string& str) const {
  const auto hmac = sha1_hmac(m_secret, str);
  return base64_encode(reinterpret_cast<const unsigned char*>(hmac.data()),
                       static_cast<unsigned int>(hmac.size()));
}

}  // namespace bcache
