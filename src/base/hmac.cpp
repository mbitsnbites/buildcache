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

#ifdef HAS_OPENSSL
#include <openssl/hmac.h>
#elif defined(__APPLE__) && defined(__MACH__)
#include <CommonCrypto/CommonHMAC.h>
#define HAS_APPLE_CRYPTO
#elif defined(_WIN32)
#undef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#undef NOMINMAX
#define NOMINMAX
// Include order matters.
// clang-format off
#include <windows.h>
#include <wincrypt.h>
#include <cstring>
// clang-format on
#define HAS_WIN32_CRYPTO
#endif
#if !(defined(HAS_APPLE_CRYPTO) || defined(HAS_WIN32_CRYPTO))
#include <cstring>
#define USE_CUSTOM_HMAC
#endif

#include <array>
#include <stdexcept>
#include <stdint.h>
#include <vector>

namespace bcache {

namespace {
// Number of bytes in a SHA1 hash (160 bits = 20 bytes).
const int SHA1_BYTES = 20;
using sha1_hash_t = std::array<uint8_t, SHA1_BYTES>;

#ifdef USE_CUSTOM_HMAC
// Read a big endian 32-bit word from a byte array.
uint32_t get_uint32_be(const uint8_t* ptr) {
  return (static_cast<uint32_t>(ptr[0]) << 24) | (static_cast<uint32_t>(ptr[1]) << 16) |
         (static_cast<uint32_t>(ptr[2]) << 8) | static_cast<uint32_t>(ptr[3]);
}

// Write a big endian 32-bit word to a byte array.
void set_uint32_be(const uint32_t x, uint8_t* ptr) {
  ptr[0] = static_cast<uint8_t>(x >> 24);
  ptr[1] = static_cast<uint8_t>(x >> 16);
  ptr[2] = static_cast<uint8_t>(x >> 8);
  ptr[3] = static_cast<uint8_t>(x);
}

// Calculate the SHA1 hash for a message.
// Based on pseudocode from Wikipedia: https://en.wikipedia.org/wiki/SHA-1#SHA-1_pseudocode
sha1_hash_t sha1(const uint8_t* msg, size_t msg_size) {
  // The original message size, in bits.
  const uint64_t original_size_bits = static_cast<uint64_t>(msg_size) * 8U;

  // The maximum number of extra bytes required for padding and meta data.
  const size_t MAX_EXTRA_BYTES = 129U;

  // Make a copy of the message into a new buffer.
  std::vector<uint8_t> message(msg_size + MAX_EXTRA_BYTES);
  std::copy(msg, msg + msg_size, message.data());

  // Set the first bit after the message to 1.
  message[msg_size++] = 0x80U;

  // Pad the message to a multiple of 512 bits (i.e. 64 characters), minus the 64-bit size (see
  // below).
  size_t padding = 64U - (msg_size % 64U);
  padding = (padding >= 8U) ? (padding - 8U) : (padding + 64U - 8U);
  if (padding > 0U) {
    std::memset(&message[msg_size], 0, padding);
    msg_size += padding;
  }

  // Append the original size as a 64-bit big endian number.
  for (int i = 0; i < 8; ++i) {
    message[msg_size++] = static_cast<uint8_t>(original_size_bits >> (56 - 8 * i));
  }

  // Initial state of the hash.
  uint32_t h0 = 0x67452301U;
  uint32_t h1 = 0xEFCDAB89U;
  uint32_t h2 = 0x98BADCFEU;
  uint32_t h3 = 0x10325476U;
  uint32_t h4 = 0xC3D2E1F0U;

  // Work buffer for each chunk.
  uint32_t w[80];

  // Loop over all 512-bit chunks.
  const size_t num_chunks = msg_size / 64U;
  for (size_t j = 0U; j < num_chunks; ++j) {
    // Extract the chunk as sixteen 32-bit words.
    for (size_t i = 0U; i < 16U; ++i) {
      w[i] = get_uint32_be(&message[(j * 64) + (i * 4)]);
    }

    // Extend the sixteen 32-bit words into eighty 32-bit words.
    for (size_t i = 16U; i < 80U; ++i) {
      uint32_t temp = w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16];
      temp = (temp << 1) + (temp >> 31);
      w[i] = temp;
    }

    // Initialize hash value for this chunk.
    uint32_t a = h0;
    uint32_t b = h1;
    uint32_t c = h2;
    uint32_t d = h3;
    uint32_t e = h4;

    // Main loop.
    for (size_t i = 0U; i < 80U; ++i) {
      uint32_t f;
      uint32_t k;
      if (i < 20U) {
        f = (b & c) | ((~b) & d);
        k = 0x5A827999U;
      } else if (i < 40U) {
        f = b ^ c ^ d;
        k = 0x6ED9EBA1U;
      } else if (i < 60U) {
        f = (b & c) | (b & d) | (c & d);
        k = 0x8F1BBCDCU;
      } else {
        f = b ^ c ^ d;
        k = 0xCA62C1D6U;
      }

      f = ((a << 5) | (a >> 27)) + f + e + k + w[i];
      e = d;
      d = c;
      c = (b << 30) | (b >> 2);
      b = a;
      a = f;
    }

    // Add this chunk's hash to result so far.
    h0 = h0 + a;
    h1 = h1 + b;
    h2 = h2 + c;
    h3 = h3 + d;
    h4 = h4 + e;
  }

  // Write the hash to the output buffer.
  sha1_hash_t hash;
  auto* hash_bytes = hash.data();
  set_uint32_be(h0, hash_bytes);
  set_uint32_be(h1, hash_bytes + 4);
  set_uint32_be(h2, hash_bytes + 8);
  set_uint32_be(h3, hash_bytes + 12);
  set_uint32_be(h4, hash_bytes + 16);
  return hash;
}

std::array<uint8_t, 64> prepare_hmac_sha1_key(const std::string& key) {
  std::array<uint8_t, 64> key_pad;
  if (key.size() > 64U) {
    // Keys longer than 64 characters are shortened by hashing them.
    const auto hash = sha1(reinterpret_cast<const uint8_t*>(key.data()), key.size());
    std::memcpy(key_pad.data(), hash.data(), hash.size());
    std::memset(&key_pad[hash.size()], 0, 64 - hash.size());
  } else {
    // Keys shorter than 64 characters are padded with zeros on the right.
    std::memcpy(key_pad.data(), key.data(), key.size());
    if (key.size() < 64) {
      std::memset(&key_pad[key.size()], 0, 64 - key.size());
    }
  }
  return key_pad;
}
#endif  // USE_CUSTOM_HMAC

std::string to_string(const sha1_hash_t& hash) {
  std::string result;
  result.reserve(SHA1_BYTES);
  for (const auto& e : hash) {
    result += static_cast<char>(e);
  }
  return result;
}
}  // namespace

std::string sha1_hmac(const std::string& key, const std::string& data) {
  sha1_hash_t digest;

#if defined(HAS_APPLE_CRYPTO)
  CCHmac(kCCHmacAlgSHA1, key.data(), key.size(), data.data(), data.size(), digest.data());
#elif defined(HAS_WIN32_CRYPTO)
  // Windows implementation inspired by:
  // https://docs.microsoft.com/en-us/windows/desktop/seccrypto/example-c-program--creating-an-hmac
  HCRYPTPROV crypt_prov = 0;
  HCRYPTKEY crypt_key = 0;
  HCRYPTHASH crypt_hash = 0;

  // Acquire a handle to the default RSA cryptographic service provider.
  if (!CryptAcquireContext(&crypt_prov, nullptr, nullptr, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) {
    throw std::runtime_error("Unable to acquire a crtypto context");
  }

  try {
    {
      // Import the key as a plain text key blob. This is dirty.
      // See: https://stackoverflow.com/a/32365048
      using plain_text_key_blob_t = struct {
        BLOBHEADER hdr;
        DWORD key_length;
      };

      std::vector<BYTE> key_blob(sizeof(plain_text_key_blob_t) + key.size());
      auto* kb = reinterpret_cast<plain_text_key_blob_t*>(key_blob.data());
      std::memset(kb, 0, sizeof(plain_text_key_blob_t));
      kb->hdr.aiKeyAlg = CALG_RC2;
      kb->hdr.bType = PLAINTEXTKEYBLOB;
      kb->hdr.bVersion = CUR_BLOB_VERSION;
      kb->hdr.reserved = 0;
      kb->key_length = static_cast<DWORD>(key.size());
      std::memcpy(&key_blob[sizeof(plain_text_key_blob_t)], key.data(), key.size());
      if (CryptImportKey(crypt_prov,
                         key_blob.data(),
                         static_cast<DWORD>(key_blob.size()),
                         0,
                         CRYPT_IPSEC_HMAC_KEY,
                         &crypt_key) == 0) {
        throw std::runtime_error("Unable to import the key");
      }
    }

    if (CryptCreateHash(crypt_prov, CALG_HMAC, crypt_key, 0, &crypt_hash) == 0) {
      throw std::runtime_error("Unable to create the hash object");
    }

    HMAC_INFO hmac_info;
    ZeroMemory(&hmac_info, sizeof(hmac_info));
    hmac_info.HashAlgid = CALG_SHA1;
    if (CryptSetHashParam(crypt_hash, HP_HMAC_INFO, reinterpret_cast<const BYTE*>(&hmac_info), 0) ==
        0) {
      throw std::runtime_error("Unable to set the hash parameters");
    }

    if (CryptHashData(crypt_hash,
                      reinterpret_cast<const BYTE*>(data.data()),
                      static_cast<DWORD>(data.size()),
                      0) == 0) {
      throw std::runtime_error("Unable to hash the data");
    }

    DWORD hash_len = 0;
    if (CryptGetHashParam(crypt_hash, HP_HASHVAL, nullptr, &hash_len, 0) == 0) {
      throw std::runtime_error("Unable to retrieve the hashed data");
    }
    if (hash_len != SHA1_BYTES) {
      throw std::runtime_error("Invalid hash size");
    }
    if (CryptGetHashParam(crypt_hash, HP_HASHVAL, digest.data(), &hash_len, 0) == 0) {
      throw std::runtime_error("Unable to retrieve the hashed data");
    }

    // Release resources.
    CryptDestroyHash(crypt_hash);
    CryptDestroyKey(crypt_key);
    CryptReleaseContext(crypt_prov, 0);
  } catch (...) {
    // Release resources...
    if (crypt_hash != 0U) {
      CryptDestroyHash(crypt_hash);
    }
    if (crypt_key != 0U) {
      CryptDestroyKey(crypt_key);
    }
    CryptReleaseContext(crypt_prov, 0);

    // ...and re-throw the exception.
    throw;
  }
#else  // USE_CUSTOM_HMAC
  // Based on pseudocode from Wikipedia: https://en.wikipedia.org/wiki/HMAC#Implementation

  // Prepare the key (make it exactly 64 characters long).
  const auto key_pad = prepare_hmac_sha1_key(key);

  // Inner and outer keys, padded.
  std::array<uint8_t, 64> inner_key_pad;
  std::array<uint8_t, 64> outer_key_pad;
  for (int i = 0; i < 64; ++i) {
    inner_key_pad[i] = key_pad[i] ^ 0x36U;
    outer_key_pad[i] = key_pad[i] ^ 0x5CU;
  }

  // Inner hash.
  sha1_hash_t inner_hash;
  {
    // Concatenate inner_key_pad + data.
    std::vector<uint8_t> msg(inner_key_pad.size() + data.size());
    std::memcpy(msg.data(), inner_key_pad.data(), inner_key_pad.size());
    if (!data.empty()) {
      std::memcpy(&msg[inner_key_pad.size()], data.data(), data.size());
    }

    // Calculate the inner hash.
    inner_hash = sha1(msg.data(), msg.size());
  }

  // Outer hash (i.e. the result)
  {
    // Concatenate outer_key_pad + inner_hash.
    std::vector<uint8_t> msg(outer_key_pad.size() + inner_hash.size());
    std::memcpy(msg.data(), outer_key_pad.data(), outer_key_pad.size());
    std::memcpy(&msg[outer_key_pad.size()], inner_hash.data(), inner_hash.size());

    // Calculate the outer hash.
    digest = sha1(msg.data(), msg.size());
  }
#endif

  return to_string(digest);
}

}  // namespace bcache
