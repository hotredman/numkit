# libs/wavelet/tests/CMakeLists.txt
#
# Wavelet Toolbox unit tests. Each new function in /loop adds a TU here
# via target_sources(numkit_tests PRIVATE ...). Hand-runnable .m smokes
# live in tests/smoke/ (next to this file) and are not compiled.

target_sources(numkit_tests PRIVATE
    # ── filter helpers ──────────────────────────────────────────
    ${CMAKE_CURRENT_SOURCE_DIR}/wrev_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/qmf_test.cpp
    # ── dwt helpers ─────────────────────────────────────────────
    ${CMAKE_CURRENT_SOURCE_DIR}/dyaddown_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/dyadup_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/wmaxlev_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/wkeep_test.cpp
)
