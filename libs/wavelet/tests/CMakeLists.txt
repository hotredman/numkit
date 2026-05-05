# libs/wavelet/tests/CMakeLists.txt
#
# Wavelet Toolbox unit tests. Each new function in /loop adds a TU here
# via target_sources(numkit_tests PRIVATE ...). Hand-runnable .m smokes
# live in tests/smoke/ (next to this file) and are not compiled.

target_sources(numkit_tests PRIVATE
    # ── DWT 1-D ─────────────────────────────────────────────────
    # Add per-function gtest TUs here as they're written / backfilled.
)
