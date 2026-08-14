// toolboxes/image/src/io/deflate.hpp
//
// In-tree RFC 1951 Deflate/Inflate, RFC 1950 ZLIB wrapper, and
// CRC-32 / Adler-32 checksums. Zero external dependencies.
//
// Used by:
//   - png_codec (PNG IDAT chunks)
//   - tiff_reader / tiff_writer (Deflate compression scheme 8 / 32946)

#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace numkit::image {

/// Compute IEEE 802.3 CRC-32 (used by PNG chunks).
std::uint32_t crc32(const std::uint8_t *data, std::size_t len);

/// Compute Adler-32 checksum (used by ZLIB RFC 1950).
std::uint32_t adler32(const std::uint8_t *data, std::size_t len);

/// Decompress raw RFC 1951 Deflate bitstream.
/// @param src               Compressed bytes.
/// @param len               Length of compressed bytes.
/// @param expectedSizeHint  Optional hint for output vector reservation.
/// @return                  Decompressed byte vector.
/// @throws                  numkit::Error on corrupted / truncated stream.
std::vector<std::uint8_t> inflateRaw(const std::uint8_t *src, std::size_t len,
                                    std::size_t expectedSizeHint = 0);

/// Compress uncompressed bytes using raw RFC 1951 Deflate.
/// @param src    Input bytes.
/// @param len    Length of input bytes.
/// @param level  Compression level: 0 = store uncompressed, 1..9 = LZ77 + Huffman.
/// @return       Compressed Deflate bitstream.
std::vector<std::uint8_t> deflateRaw(const std::uint8_t *src, std::size_t len,
                                    int level = 6);

/// Decompress RFC 1950 ZLIB stream (2-byte header + Deflate stream + 4-byte Adler-32).
/// @param src               ZLIB stream bytes.
/// @param len               Length of stream bytes.
/// @param expectedSizeHint  Optional output size hint.
/// @return                  Decompressed payload bytes.
/// @throws                  numkit::Error on bad header, corrupted data, or Adler-32 mismatch.
std::vector<std::uint8_t> zlibDecompress(const std::uint8_t *src, std::size_t len,
                                         std::size_t expectedSizeHint = 0);

/// Compress payload into an RFC 1950 ZLIB stream.
/// @param src    Input payload.
/// @param len    Length of payload.
/// @param level  Compression level (0..9).
/// @return       Complete ZLIB stream (header + Deflate + Adler-32).
std::vector<std::uint8_t> zlibCompress(const std::uint8_t *src, std::size_t len,
                                       int level = 6);

} // namespace numkit::image
