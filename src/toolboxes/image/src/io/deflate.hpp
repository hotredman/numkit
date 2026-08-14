// toolboxes/image/src/io/deflate.hpp
//
// In-tree RFC 1951 Deflate/Inflate, RFC 1950 ZLIB wrapper, and
// CRC-32 / Adler-32 checksums. Zero external dependencies.
//
// Forwards to shared ops/deflate.hpp implementation.

#pragma once

#include <numkit/ops/deflate.hpp>

namespace numkit::image {

using ops::crc32;
using ops::adler32;
using ops::inflateRaw;
using ops::deflateRaw;
using ops::zlibDecompress;
using ops::zlibCompress;

} // namespace numkit::image
