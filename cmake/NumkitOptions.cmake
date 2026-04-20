# cmake/NumkitOptions.cmake
#
# Central declaration of NUMKIT_WITH_* feature flags.
# Toggling a flag here (via CMake preset, -D arg, or GUI cache) controls
# which optimized backend .cpp files get compiled into the library.
#
# All flags default to OFF — the reference/portable build always works.
# Optimized backends get wired up in Phase 5+ as each library gets its
# public C++ API and backends/ structure.

# ── SIMD / vectorization ────────────────────────────────────────────────
option(NUMKIT_WITH_SIMD
    "Enable Google Highway dynamic-dispatch SIMD backends (pulls hwy dep)"
    OFF)

# ── Math / DSP optimized backends ───────────────────────────────────────
option(NUMKIT_WITH_POCKETFFT
    "Use pocketfft (BSD, header-only) for FFT instead of portable Cooley-Tukey"
    OFF)

# ── Linear algebra ──────────────────────────────────────────────────────
option(NUMKIT_WITH_BLAS
    "Link against a BLAS implementation (OpenBLAS preferred) for matmul/solve"
    OFF)

option(NUMKIT_WITH_ACCELERATE
    "Use Apple Accelerate.framework on ARM64 macOS (free, pre-installed, optimal)"
    OFF)

# ── Threading ───────────────────────────────────────────────────────────
option(NUMKIT_WITH_OPENMP
    "Parallelize hot loops via OpenMP"
    OFF)

# ── Summary print ───────────────────────────────────────────────────────
message(STATUS "numkit-m feature flags:")
message(STATUS "  NUMKIT_WITH_SIMD       = ${NUMKIT_WITH_SIMD}")
message(STATUS "  NUMKIT_WITH_POCKETFFT  = ${NUMKIT_WITH_POCKETFFT}")
message(STATUS "  NUMKIT_WITH_BLAS       = ${NUMKIT_WITH_BLAS}")
message(STATUS "  NUMKIT_WITH_ACCELERATE = ${NUMKIT_WITH_ACCELERATE}")
message(STATUS "  NUMKIT_WITH_OPENMP     = ${NUMKIT_WITH_OPENMP}")
