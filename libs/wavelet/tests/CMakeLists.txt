# libs/wavelet/tests/CMakeLists.txt
#
# Wavelet Toolbox unit tests. Each new function in /loop adds a TU here
# via target_sources(numkit_gtest PRIVATE ...). Hand-runnable .m smokes
# live in tests/smoke/ (next to this file) and are not compiled.

target_sources(numkit_gtest PRIVATE
    # ── filter helpers ──────────────────────────────────────────
    ${CMAKE_CURRENT_SOURCE_DIR}/wrev_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/qmf_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/dbwavf_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/coifwavf_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/symwavf_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/orthfilt_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/families_public_api_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/qmf_wrev_public_api_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/wfilters_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/dwt_idwt_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/appcoef_test.cpp
    # ── dwt helpers ─────────────────────────────────────────────
    ${CMAKE_CURRENT_SOURCE_DIR}/dyaddown_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/dyadup_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/wmaxlev_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/dyad_public_api_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/wkeep_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/wextend_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/detcoef_test.cpp
    # ── shapes ──────────────────────────────────────────────────
    ${CMAKE_CURRENT_SOURCE_DIR}/gauswavf_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cgauwavf_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mexihat_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/morlet_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cmorwavf_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/shanwavf_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/fbspwavf_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/meyeraux_test.cpp
    # ── haar transform ──────────────────────────────────────────
    ${CMAKE_CURRENT_SOURCE_DIR}/haart_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ihaart_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/wrcoef_test.cpp
    # ── stationary / MODWT ──────────────────────────────────────
    ${CMAKE_CURRENT_SOURCE_DIR}/swt_modwt_test.cpp
)
