# Zero-Dependency In-Tree Image Codecs & Compression Engine

## Context & Problem
Previously, image I/O (`imread`, `imwrite`, `imfinfo`) relied on third-party vendored C headers (`stb_image.h` and `stb_image_write.h` under `third_party/stb/`). This had several drawbacks:
1. External dependency clutter in the codebase.
2. Truncation of 16-bit scientific imagery down to 8-bit quantization in PNG.
3. Lack of modular in-memory buffer encode/decode primitives for cross-platform and WebAssembly targets.
4. Missing support for Netpbm 16-bit binary streams (P5/P6) and fine-grained chunk introspection.

## Architectural Decision & Solution
Replaced `stb_image` and `stb_image_write` entirely with a set of autonomous, zero-dependency C++20 image codecs and compression engines:

1. **In-Tree Deflate/Inflate & Checksum Engine (`deflate.hpp`, `deflate.cpp`)**:
   - RFC 1951 Deflate compressor (fixed Huffman codes, dynamic LZ77 sliding window with 32K hash table, uncompressed blocks).
   - RFC 1951 Inflate decompressor (fixed/dynamic canonical Huffman table decoders, length-distance back-references).
   - RFC 1950 Zlib wrapper (`zlibCompress`, `zlibDecompress`).
   - Fast CRC-32 (polynomial `0xEDB88320`) and Adler-32 checksum implementations.

2. **Windows Bitmap (`bmp_codec.hpp`, `bmp_codec.cpp`)**:
   - Headers: `BITMAPINFOHEADER` (40 bytes), `BITMAPCOREHEADER` (12 bytes), `BITMAPV4HEADER`, `BITMAPV5HEADER`.
   - Bit depths: 1, 4, 8 (indexed/palette), 16 (RGB555), 24 (BGR), 32 (BGRA).
   - Compressions: `BI_RGB` (0), `BI_RLE8` (1), `BI_BITFIELDS` (3).
   - Top-down ($H < 0$) and bottom-up ($H > 0$) scanning with 4-byte row padding.

3. **Truevision TGA (`tga_codec.hpp`, `tga_codec.cpp`)**:
   - Supports 8-bit Grayscale, 24-bit BGR, 32-bit BGRA, palette/colormap.
   - Packet-based RLE encoder and decoder.
   - Origin descriptors (top-left vs bottom-left).

4. **Netpbm PNM (`pnm_codec.hpp`, `pnm_codec.cpp`)**:
   - Full support for P1 (PBM ASCII), P2 (PGM ASCII), P3 (PPM ASCII), P4 (PBM binary), P5 (PGM binary), P6 (PPM binary).
   - Preserves 8-bit `uint8` and 16-bit `uint16` depths (Big-Endian per Netpbm spec).

5. **Portable Network Graphics PNG (`png_codec.hpp`, `png_codec.cpp`)**:
   - Chunk parsing: `IHDR`, `PLTE`, `tRNS`, `IDAT`, `IEND` with CRC-32 integrity validation.
   - Scanline filters: `None` (0), `Sub` (1), `Up` (2), `Average` (3), `Paeth` (4).
   - High dynamic range: Full 16-bit Grayscale, RGB, and RGBA (`uint16`) support without precision loss.
   - Palette color mapping: `[A, map] = readPngWithMap(...)`.

6. **Baseline JPEG (`jpeg_codec.hpp`, `jpeg_codec.cpp`)**:
   - JFIF marker parsing (`SOI`, `APP0`, `DQT`, `SOF0`, `SOF2`, `DHT`, `SOS`, `EOI`) and byte stuffing (`0xFF 0x00`).
   - 2D AAN forward and inverse discrete cosine transform (FDCT / IDCT).
   - Quantization table scaling for arbitrary quality levels $Q \in [1, 100]$.
   - Full baseline sequential Huffman entropy decoding and encoding with canonical codebook tree reconstruction.
   - MCU grid dequantization, IDCT, and chroma sub-sampling reconstruction for Grayscale (1-channel) and YCbCr (4:2:0, 4:2:2, 4:4:4).

7. **TIFF & BigTIFF Codec (`tiff_codec.hpp`, `tiff_codec.cpp`)**:
   - Consolidated classic TIFF (magic 42) and BigTIFF (magic 43, 64-bit offsets) reader, writer, and peeker.
   - Fully zero-dependency: switched Deflate (8 / 32946) to in-tree `deflate.hpp` engine alongside LZW (5) and PackBits (32773).
   - Multi-page IFD navigation and append mode.

8. **Integration & Cleanup (`io.cpp`)**:
   - Unified byte-oriented dispatcher `imreadFromBytes`, `imwriteToBytes`, and `imfinfoFromBytes`.
   - Complete removal and deletion of `third_party/stb/` and `src/toolboxes/image/src/io/stb_impl.cpp`.

## Quantitative Verification & Results
- `deflate_test.cpp`: 5/5 tests passed in 18 ms.
- `bmp_tga_pnm_test.cpp`: 6/6 tests passed in 0 ms.
- `png_codec_test.cpp`: 3/3 tests passed in 1 ms.
- `jpeg_codec_test.cpp`: 2/2 tests passed in 6 ms.
- `tiff_codec_test.cpp`: 30/30 tests passed in 2.5 s.
- `codecs_robustness_test.cpp`: 6/6 tests passed in 2 ms.
- `image_e2e_io_test.cpp`: 5/5 tests passed in 456 ms.
- Full image test suites (`*Image*:*Imread*:*Deflate*:*Bmp*:*Tga*:*Pnm*:*Png*:*Jpeg*:*Tiff*`): 100% pass rate.
