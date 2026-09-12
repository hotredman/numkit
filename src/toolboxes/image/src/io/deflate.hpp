// toolboxes/image/src/io/deflate.hpp
//
// In-tree RFC 1951 Deflate/Inflate, RFC 1950 ZLIB wrapper, and
// CRC-32 / Adler-32 checksums. Zero external dependencies.
//
// Forwards to shared ops/deflate.hpp implementation.

#pragma once

#include <nk/ops/deflate.hpp>

namespace nk::image {

using ops::crc32;
using ops::adler32;
using ops::inflateRaw;
using ops::deflateRaw;
using ops::zlibDecompress;
using ops::zlibCompress;

} // namespace nk::image
