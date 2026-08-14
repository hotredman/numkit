# Autonomous Zero-Dependency MAT-File Codec & Third-Party Elimination

## Context and Motivation
Prior to this task, NumKit relied on two third-party dependencies:
1. `matio` (v1.5.30) via CMake `FetchContent` for MATLAB binary `.mat` file serialization/deserialization.
2. `zlib` (v1.3.1) via CMake `FetchContent` for Deflate decompression in TIFF and compression in Level 7 `.mat` files.

This introduced multiple issues:
- FetchContent network download overhead and build-system bloat during clean builds / offline builds.
- C-library impedance mismatch with NumKit's C++17 `Value`, `Dims`, `VirtualFS`, and `std::pmr::memory_resource`.
- Memory leak hazards and temporary file staging (`makeTempMatPath()`) because `matio` required POSIX file paths on disk rather than working in-memory with NumKit's `VirtualFS` (`CallbackFS`, `MemoryFS`).
- Portability / cross-compilation risks on WebAssembly / Emscripten.

## Solution & Architecture
We eliminated `matio` and `zlib` entirely and replaced them with an autonomous, high-performance, in-tree C++17 MAT-file codec:

1. **Shared In-Tree Deflate/Inflate Subsystem (`src/ops/include/numkit/ops/deflate.hpp`, `src/ops/src/compression/deflate.cpp`):**
   - Implements RFC 1951 Deflate/Inflate engine and RFC 1950 Zlib container format (`zlibCompress`, `zlibDecompress`).
   - Implements hardware-efficient CRC-32 and Adler-32 checksums.
   - Shared between Image I/O (PNG, TIFF Deflate/ZIP), Audio I/O (FLAC container Deflate blocks), and MAT-file compression (`-v7`).

2. **In-Tree MAT-File Codec (`src/runtime/src/saveload_mat.cpp`):**
   - **MAT Level 5 (`-v6` uncompressed, `-v7` zlib compressed):**
     - Full 128-byte header parsing/generation with Little-Endian `"IM"` magic.
     - Small Data Element Format (SDEF) & standard 8-byte aligned tags.
     - Full `miMATRIX` encoding & decoding for all NumKit types:
       - Real & complex matrices (`DOUBLE`, `SINGLE`, `INT8`..`INT64`, `UINT8`..`UINT64`).
       - `LOGICAL` matrices (with `0x02` logical flag).
       - `CHAR` arrays (1D string row vectors, 2D char matrices, and ND char tensors encoded as UTF-16).
       - `CELL` arrays (arbitrary shape and recursive nesting).
       - `STRUCT` & struct arrays (field name table, nested field matrices, insertion order tracking).
       - Function handles (stored as 0×0 DOUBLE placeholders matching MATLAB v5 convention).
     - Compressed `-v7` stream encoding & decoding via in-tree `numkit::ops::zlibCompress` / `zlibDecompress` wrapped in `miCOMPRESSED` tags.
   - **MAT Level 4 (`-v4`):**
     - 20-byte matrix header (`MOPT`, `mrows`, `ncols`, `imagf`, `namlen`), ASCII name, column-major real/imag payloads.
     - Full validation of header parameters to reject garbage/corrupted files cleanly.
   - **In-Memory & VFS-First:**
     - Directly reads `resolved.fs->readFile(resolved.path)` and writes `resolved.fs->writeFile(resolved.path, bytes)`.
     - Zero temporary file creation; 100% compatible with NativeFS, MemoryFS, and CallbackFS.

3. **Build System Streamlining (`CMakeLists.txt`, `cmake/NumkitOptions.cmake`, `src/runtime/CMakeLists.txt`):**
   - Completely removed `matio` and `zlib_pkg` `FetchContent` blocks.
   - Removed `NUMKIT_WITH_MATIO` and `NUMKIT_WITH_ZLIB` conditional defines.
   - The entire NumKit runtime and all toolboxes (Image, Audio, Workspace I/O) now have **zero external C/C++ third-party library dependencies** (only Google Highway for optional SIMD and Google Test for unit test binaries).

## Verification & Test Coverage
- **100% Pass Rate across all 34 `SaveLoadMatTest` cases:**
  - `RoundTripDoubleMatrix`, `RoundTripSingle`, `RoundTripAllIntegerTypes`, `RoundTripComplexMatrix`, `RoundTripPureImaginary`, `RoundTripLogical`, `RoundTripCharRow`, `RoundTripCharMatrix`, `RoundTrip3DDouble`, `RoundTrip3DComplex`, `RoundTripEmpty`, `RoundTripRowOfZeroCols`, `RoundTripScalar`, `SpecialFloatsPreserved`, `RoundTripCell`, `RoundTripStruct`, `RoundTripStructFieldOrder`, `RoundTripStructArray`, `NestedStructAndCell`, `CellOfCells`, `RoundTripEmptyStructAndCell`, `RoundTripMultipleVarsAndStructForm`, `SaveWholeWorkspaceWithoutVarnames`, `OverwriteReplacesEntireFile`, `UppercaseMatExtensionDispatchesBinary`, `ExplicitMatFlagOverridesAsciiExtension`, `V6AndV7FlagsAccepted`, `V73Rejected`, `FileBeginsWithV5Magic`, `LoadNonexistentFileThrows`, `LoadGarbageFileThrows`, `SaveMissingVariableThrows`, `FunctionHandleStoredAsEmptyPlaceholder`, `MixedTypeBundleSurvivesRoundTrip`.
- **100% Pass Rate across Deflate, Codecs, and Audio test suites.**
