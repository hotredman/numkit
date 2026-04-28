set(SIGNALLIB_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/MSignalLibrary.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/MDspSignalCore.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/MDspWindows.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/MDspFilter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/MDspFilterDesign.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/MDspSpectral.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/MDspResample.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/MDspTransform.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/MDspFft.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/MDspConv.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/MDspGaps.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/MDspSos.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/MDspSgolay.cpp
)

# Backend-split FFT kernel — the SIMD path itself splits across
# several TUs (one per radix family) so MSVC's per-TU inliner
# budget isn't shared across butterflies; see
# feedback_fft_msvc_limits memory for the rationale.
if(NUMKIT_WITH_SIMD)
    list(APPEND SIGNALLIB_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/backends/MDspFft_simd.cpp)
    list(APPEND SIGNALLIB_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/backends/MDspFft_r2_simd.cpp)
    list(APPEND SIGNALLIB_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/backends/MDspFft_r4_simd.cpp)
    list(APPEND SIGNALLIB_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/backends/MDspFft_stockham_simd.cpp)
    list(APPEND SIGNALLIB_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/backends/MDspFft_r2_soa_simd.cpp)
    list(APPEND SIGNALLIB_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/backends/MDspFft_r4_soa_simd.cpp)
else()
    list(APPEND SIGNALLIB_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/backends/MDspFft_portable.cpp)
endif()

set(SIGNALLIB_SOURCES ${SIGNALLIB_SOURCES} PARENT_SCOPE)
