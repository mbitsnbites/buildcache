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

#include <base/compressor.hpp>
#include <base/file_utils.hpp>
#include <config/configuration.hpp>

#include <cstdint>
#include <lz4/lz4.h>
#include <zstd/zstd.h>

#include <algorithm>
#include <functional>
#include <limits>
#include <stdexcept>
#include <vector>

namespace bcache {
namespace comp {
namespace {
// Format of the compressed data:
//
// +-----------+------------------------+-------------------------------------------------+
// | Offset    | Data type              | Description                                     |
// +-----------+------------------------+-------------------------------------------------+
// | 0         | uint32 (little endian) | Compression format                              |
// |           |                        |   0x00345a4c = LZ4                              |
// |           |                        |   0x4454535a = ZSTD                             |
// | 4         | uint32 (little endian) | Original (uncompressed) size, in bytes          |
// | 8         | (algorithm dependent)  | Compressed data                                 |
// +-----------+------------------------+-------------------------------------------------+
const int COMPR_HEADER_SIZE = 8;
const uint32_t COMPR_FORMAT_LZ4 = 0x00345a4cu;
const uint32_t COMPR_FORMAT_ZSTD = 0x4454535au;

void encode_uint32(std::vector<char>& buffer, const int offset, const uint32_t value) {
  buffer[offset + 0] = static_cast<char>(value);
  buffer[offset + 1] = static_cast<char>(value >> 8);
  buffer[offset + 2] = static_cast<char>(value >> 16);
  buffer[offset + 3] = static_cast<char>(value >> 24);
}

uint32_t decode_uint32(const std::string& buffer, const int offset) {
  return static_cast<uint32_t>(static_cast<uint8_t>(buffer[offset + 0])) |
         (static_cast<uint32_t>(static_cast<uint8_t>(buffer[offset + 1])) << 8) |
         (static_cast<uint32_t>(static_cast<uint8_t>(buffer[offset + 2])) << 16) |
         (static_cast<uint32_t>(static_cast<uint8_t>(buffer[offset + 3])) << 24);
}

}  // namespace

std::string compress(const std::string& str) {
  const uint32_t compress_format = (config::compress_format() == config::compress_format_t::ZSTD)
                                       ? COMPR_FORMAT_ZSTD
                                       : COMPR_FORMAT_LZ4;
  int32_t compress_level = config::compress_level();

  std::function<size_t(size_t)> bound_func;

  // Can this data be compressed?
  if (compress_format == COMPR_FORMAT_LZ4) {
    if (str.size() > static_cast<std::string::size_type>(LZ4_MAX_INPUT_SIZE)) {
      throw std::runtime_error("Unable to compress the data: Too large data buffer.");
    }

    bound_func = LZ4_compressBound;

    if (compress_level == -1) {
      compress_level = 1;  // "acceleration" in LZ4
    }
  } else if (compress_format == COMPR_FORMAT_ZSTD) {
    bound_func = ZSTD_compressBound;

    if (compress_level != -1) {
      // Ensure that we set a valid compression level
      ZSTD_bounds b = ZSTD_cParam_getBounds(ZSTD_c_compressionLevel);
      compress_level = std::max(b.lowerBound, std::min(compress_level, b.upperBound));
    } else {
      compress_level = ZSTD_CLEVEL_DEFAULT;
    }
  }

  // Allocate memory for the destination buffer.
  const auto original_size = static_cast<int>(str.size());
  int compressed_size = 0;

  const auto max_compressed_size = bound_func(original_size);
  if (max_compressed_size == 0) {
    throw std::runtime_error("Unable to compress the data.");
  }

  std::vector<char> compressed_data(COMPR_HEADER_SIZE + max_compressed_size);

  // Perform the compression.
  if (compress_format == COMPR_FORMAT_LZ4) {
    compressed_size = LZ4_compress_fast(str.c_str(),
                                        &compressed_data[COMPR_HEADER_SIZE],
                                        original_size,
                                        max_compressed_size,
                                        compress_level);

    if (compressed_size == 0) {
      throw std::runtime_error("Unable to compress the data.");
    }
  } else if (compress_format == COMPR_FORMAT_ZSTD) {
    compressed_size = ZSTD_compress(&compressed_data[COMPR_HEADER_SIZE],
                                    max_compressed_size,
                                    str.c_str(),
                                    original_size,
                                    compress_level);

    if (ZSTD_isError(compressed_size)) {
      throw std::runtime_error("An error occurred while compressing the data.");
    } else {
      if (compressed_size == 0) {
        throw std::runtime_error("Unable to compress the data.");
      }
    }
  }

  // Write data to the header.
  encode_uint32(compressed_data, 0, compress_format);
  encode_uint32(compressed_data, 4, static_cast<uint32_t>(original_size));

  // Convert the buffer to a string (and truncate it to the compressed size).
  return std::string(compressed_data.data(), COMPR_HEADER_SIZE + compressed_size);
}

std::string decompress(const std::string& compressed_str) {
  // Sanity check: Is there a header in the compressed string?
  if (compressed_str.size() < static_cast<std::string::size_type>(COMPR_HEADER_SIZE)) {
    throw std::runtime_error("Missing header in compressed data.");
  }
  if (compressed_str.size() > std::numeric_limits<int>::max()) {
    throw std::runtime_error("Too large input buffer for decompression.");
  }

  // Read data from the header.
  const auto format = decode_uint32(compressed_str, 0);
  const auto original_size_u32 = decode_uint32(compressed_str, 4);
  if (original_size_u32 > static_cast<uint32_t>(std::numeric_limits<int32_t>::max())) {
    throw std::runtime_error("Too large output buffer for decompression.");
  }
  const auto original_size = static_cast<int>(static_cast<int32_t>(original_size_u32));

  // Sanity check: Is the header data reasonable?
  if (format != COMPR_FORMAT_LZ4 && format != COMPR_FORMAT_ZSTD) {
    throw std::runtime_error("Unrecognized compression format.");
  }
  if (original_size < 0) {
    throw std::runtime_error("Invalid uncompressed data size.");
  }

  // Allocate memory for the destination buffer.
  std::vector<char> decompressed_data(original_size);

  // Perform the decompression.
  const auto compressed_size = static_cast<int>(compressed_str.size()) - COMPR_HEADER_SIZE;
  size_t size = 0;

  if (format == COMPR_FORMAT_LZ4) {
    size = LZ4_decompress_safe(compressed_str.data() + COMPR_HEADER_SIZE,
                               decompressed_data.data(),
                               compressed_size,
                               original_size);
  } else if (format == COMPR_FORMAT_ZSTD) {
    size = ZSTD_decompress(decompressed_data.data(),
                           original_size,
                           compressed_str.data() + COMPR_HEADER_SIZE,
                           compressed_size);

    if (ZSTD_isError(size)) {
      throw std::runtime_error("An error occurred while decompressing the data.");
    }
  }

  if (size != original_size) {
    throw std::runtime_error("Unable to decompress the data.");
  }

  // Convert the buffer to a string.
  return std::string(decompressed_data.data(), decompressed_data.size());
}

void compress_file(const std::string& from_path, const std::string& to_path) {
  // Create a temporary file first and once the copy has succeeded, move it to the target file.
  // This should prevent half-finished copies if the process is terminated prematurely (e.g.
  // CTRL+C).
  const auto base_path = file::get_dir_part(to_path);
  auto tmp_file = file::tmp_file_t(base_path, ".tmp");

  // Compress the source file and write it to the temporary file.
  // TODO(m): Do buffered/streaming compression.
  file::write(compress(file::read(from_path)), tmp_file.path());

  // Move the temporary file to its target name.
  file::move(tmp_file.path(), to_path);
}

void decompress_file(const std::string& from_path, const std::string& to_path) {
  // Create a temporary file first and once the copy has succeeded, move it to the target file.
  // This should prevent half-finished copies if the process is terminated prematurely (e.g.
  // CTRL+C).
  const auto base_path = file::get_dir_part(to_path);
  auto tmp_file = file::tmp_file_t(base_path, ".tmp");

  // Decompress the source file and write it to the temporary file.
  // TODO(m): Do buffered/streaming decompression.
  file::write(decompress(file::read(from_path)), tmp_file.path());

  // Move the temporary file to its target name.
  file::move(tmp_file.path(), to_path);
}

}  // namespace comp
}  // namespace bcache
