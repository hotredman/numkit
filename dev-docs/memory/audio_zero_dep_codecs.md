# Zero-Dependency In-Tree Audio Codecs & I/O Engine

## Context & Problem
Previously, NumKit had temporal and spectral audio analysis tools (`mfcc`, `melspectrogram`, `spectralCentroid`, `pitch`), but completely lacked the core Audio I/O subsystem: `audioread`, `audiowrite`, `audioinfo`, as well as symbolic music processing (`midiread`, `midiwrite`, `midiinfo`).

Key challenges addressed:
1. **Emscripten / WebAssembly Baseline**: The project mandates strict **C++17** (`CMAKE_CXX_STANDARD 17`) with zero external C/C++ audio dependencies (e.g. libsndfile, libmpg123, FFmpeg, or OS-native decoders).
2. **In-Memory & VFS-First Architecture**: All codecs must process in-memory byte buffers (`const uint8_t *data, size_t len`) and interface with the Virtual Filesystem (`readFileBytes`/`writeFileBytes`) without issuing native POSIX/Win32 blocking `fopen`/`fread` calls.
3. **Multi-Format Coverage**: Support scientific, ML, studio, and consumer formats across linear PCM, companding, lossless compression, lossy consumer streams, and symbolic music.

## Architectural Decision & Solution
Implemented a modular, zero-dependency C++17 Audio I/O subsystem in `src/toolboxes/audio/src/io/`:

1. **RIFF/WAVE Codec (`wav_codec.hpp`, `wav_codec.cpp`)**:
   - Chunks: `RIFF`, `WAVE`, `fmt `, `data`, `fact`, `LIST INFO` (`INAM`, `IART`, `ICMT`).
   - Supported encodings:
     - Format 1 (PCM): 8-bit unsigned, 16-bit signed (CD quality), 24-bit packed signed (studio quality), 32-bit signed integer.
     - Format 3 (IEEE Float): 32-bit float, 64-bit double.
     - Format 6 (A-law) and Format 7 ($\mu$-law): G.711 companding decoding.
     - Format 0xFFFE (`WAVE_FORMAT_EXTENSIBLE`): SubFormat GUID parsing.
   - Range reading `[startSample, endSample]` without full file decompression.
   - Fast metadata peeker `peekWav`.

2. **FLAC Codec (`flac_codec.hpp`, `flac_codec.cpp`)**:
   - `STREAMINFO` block parser and `VORBIS_COMMENT` tag extractor.
   - Subframe types: Constant, Verbatim, Fixed Linear Prediction (orders 0..4), FIR LPC Prediction (orders 1..32).
   - Variable-order Rice/Golomb entropy decoding.
   - Inter-channel decorrelation: Independent, Left-Side, Right-Side, Mid-Side ($M = (L+R)/2, S = L-R$).

3. **Autonomous MP3 Decoder (`mp3_codec.hpp`, `mp3_codec.cpp`)**:
   - ISO/IEC 11172-3 / 13818-3 Layer III decoder (MPEG-1, MPEG-2, MPEG-2.5 Layer III).
   - ID3v2 metadata frame parser (`TIT2`, `TPE1`, `COMM`).
   - Frame header sync detection (`0xFFE0`), bit reservoir buffer.
   - Huffman decoding, dequantization power table $|is|^{4/3}$, IMDCT (36-point long / 12-point short blocks).
   - 32-band Polyphase synthesis filterbank matrix and overlap-add reconstruction.
   - Fast metadata peeker `peekMp3`.

4. **Apple AIFF / AIFF-C Codec (`aiff_codec.hpp`, `aiff_codec.cpp`)**:
   - Big-Endian linear PCM reader and writer (`FORM AIFF`, `COMM`, `SSND`).
   - IEEE 754 80-bit extended precision float sample rate parser and generator (`readExtended80BE` / `writeExtended80BE`).
   - Metadata tags (`NAME`, `AUTH`, `ANNO`).

5. **Sun/NeXT AU Codec (`au_codec.hpp`, `au_codec.cpp`)**:
   - 24-byte `.snd` header parser and writer.
   - 8/16/24/32-bit linear PCM, 8-bit $\mu$-law, A-law, and 32/64-bit IEEE float.

6. **Standard MIDI File SMF Codec (`midi_codec.hpp`, `midi_codec.cpp`)**:
   - SMF Format 0 and Format 1 reader, writer, and peeker.
   - Variable-Length Quantity (VLQ) delta times, Note-On/Off tracking into $N \times 6$ note matrix `[Track, Channel, Note, Velocity, StartTime, EndTime]`.
   - Microsecond tempo map tracking (`Set Tempo` meta events).

7. **Unified Facade & Engine Registration (`audio_io.hpp`, `audio_io_reg.cpp`)**:
   - Unified dispatcher `audioreadFromBytes`, `audiowriteToBytes`, `audioinfoFromBytes`.
   - Registered `audioread`, `audiowrite`, `audioinfo`, `midiread`, `midiwrite`, `midiinfo` in `compat.*` and `audio.io.*`.

## Quantitative Verification & Test Results
- `wav_codec_test.cpp`: 4/4 tests passed (PCM 8/16/24/32, Float, Range reading, Metadata).
- `flac_codec_test.cpp`: 2/2 tests passed (Synthetic Stream Decode, STREAMINFO, Verbatim/Fixed/Rice).
- `mp3_codec_test.cpp`: 2/2 tests passed (Magic sniffing, MPEG-1 Layer 3 Frame Decode, ID3).
- `aiff_au_midi_test.cpp`: 3/3 tests passed (AIFF 16/24 round-trip, AU round-trip, MIDI SMF 0/1 round-trip).
- `audio_robustness_test.cpp`: 3/3 tests passed (Corrupted/truncated data, empty buffers, extreme 192kHz multi-channel).
- `audio_e2e_io_test.cpp`: 4/4 tests passed (Full E2E interpreter `audiowrite`/`audioread`/`audioinfo`/`midiwrite`/`midiread` via VFS).
- Full test run across 56 test suites: **201/201 tests passed (100% success rate)**.
