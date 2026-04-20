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

message(STATUS "numkit-m feature flags:")
message(STATUS "  NUMKIT_WITH_SIMD = ${NUMKIT_WITH_SIMD}")
