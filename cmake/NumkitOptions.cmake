# cmake/NumkitOptions.cmake
#
# Central declaration of NUMKIT_WITH_* feature flags.
# Toggling a flag here (via CMake preset, -D arg, or GUI cache) controls
# which optimized backend .cpp files get compiled into the library.
#
# All flags default to OFF — the reference/portable build always works.
# Optimized backends get wired up in Phase 5+ as each library gets its
# public C++ API and backends/ structure.
#
# Dependency policy (project decision — see project_architecture memory):
#   numkit-m writes its own numerical algorithms. No third-party
#   numerical libs (no FFTW, no pocketfft, no OpenBLAS, no Accelerate).
#   The one exception is Google Highway — it is a SIMD-intrinsics
#   abstraction, not an algorithm library. Its only job is to let us
#   write one kernel that compiles for SSE/AVX/AVX-512/NEON/WASM SIMD128.
#   Threading, when needed, goes through std::thread / std::async.

option(NUMKIT_WITH_SIMD
    "Enable Google Highway dynamic-dispatch SIMD backends (pulls hwy dep)"
    OFF)

option(NUMKIT_WITH_THREADS
    "Enable multi-threaded SIMD kernels via a persistent std::thread pool. \
Above per-kernel thresholds the work is split across hardware_concurrency() \
workers; below them the kernel runs single-threaded as before. Bit-identical \
to the single-threaded path for the supported elementwise ops (+ - .* ./ \
abs sin cos exp log)."
    OFF)

# Binary .mat (MATLAB save/load) support via matio. Pulled in via
# FetchContent — no system packages, no vcpkg. Disabled by default on
# Emscripten until matio's autoconf-style feature detection is validated
# under emcc cross-compile.
#
# Sole exception to the "no third-party numerical libs" rule (matio is a
# file-format library — parses/emits Mathworks' MAT5 binary container,
# not a numerical kernel).
#
# Toggling OFF: saveload_mat.cpp is excluded, matio FetchContent is
# skipped, and save/load throw a clear "binary .mat support not compiled
# in" message on `-mat` / `-v4` / `-v6` / `-v7` paths. ASCII mode keeps
# working.
if(EMSCRIPTEN)
    set(_numkit_matio_default OFF)
else()
    set(_numkit_matio_default ON)
endif()
option(NUMKIT_WITH_MATIO
    "Enable binary .mat file support (MAT4 / MAT5 / MAT7) via matio"
    ${_numkit_matio_default})

message(STATUS "numkit-m feature flags:")
message(STATUS "  NUMKIT_WITH_SIMD    = ${NUMKIT_WITH_SIMD}")
message(STATUS "  NUMKIT_WITH_THREADS = ${NUMKIT_WITH_THREADS}")
message(STATUS "  NUMKIT_WITH_MATIO   = ${NUMKIT_WITH_MATIO}")
