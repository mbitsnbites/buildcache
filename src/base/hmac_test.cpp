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

#include <base/hmac.hpp>

#include <doctest/doctest.h>

#include <string>

// Workaround for macOS build errors.
// See: https://github.com/onqtam/doctest/issues/126
#include <iostream>

using namespace bcache;

namespace {

std::string to_hex(const std::string& digest) {
  static const char HEX_CHARS[] = "0123456789abcdef";
  std::string result;
  for (const auto x : digest) {
    const auto byte = static_cast<unsigned int>(x);
    result += HEX_CHARS[(byte >> 4) & 15];
    result += HEX_CHARS[byte & 15];
  }
  return result;
}

}  // namespace

TEST_CASE("sha1_hmac() produces expected results") {
  SUBCASE("Case 1 - hello world") {
    const std::string key = "012345678";
    const std::string data = "hello world";
    const auto result = to_hex(sha1_hmac(key, data));
    CHECK_EQ(result, "e19e220122b37b708bfb95aca2577905acabf0c0");
  }

  SUBCASE("Case 2 - The quick brown fox") {
    const std::string key = "reb6780rewbo214";
    const std::string data = "The quick brown fox jumps over the lazy dog! {0/1/2/3/4/5/6/7/8/9}";
    const auto result = to_hex(sha1_hmac(key, data));
    CHECK_EQ(result, "b7af6ab52472997028e498bb264663fcb5a2183e");
  }

  SUBCASE("Case 3 - Empty data") {
    const std::string key = "abcdefghijklmnopqrstuvwxyz";
    const std::string data;
    const auto result = to_hex(sha1_hmac(key, data));
    CHECK_EQ(result, "28cfb82af65df022e08fa1a67121068c1d480bc8");
  }

  SUBCASE("Case 4 - Key length == 64 chars") {
    const std::string key = "0123456789012345678901234567890123456789012345678901234567890123";
    const std::string data = "This is a string";
    const auto result = to_hex(sha1_hmac(key, data));
    CHECK_EQ(result, "59ba27cc1f0079294ddfc9cce4b507020fb3f91f");
  }

  SUBCASE("Case 5 - Key length > 64 chars") {
    const std::string key = "0123456789012345678901234567890123456789012345678901234567890123abcd";
    const std::string data = "This is a string";
    const auto result = to_hex(sha1_hmac(key, data));
    CHECK_EQ(result, "b5442dce0788560bdc2548180868e5cb3c76ecea");
  }
}
