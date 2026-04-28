set(STATSLIB_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/library.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/moments/moments.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/nan_aware/nan_aware.cpp
)

# Backend-split functions: one definition per TU, picked at configure
# time via NUMKIT_WITH_SIMD. nan_reductions kernels feed nansum / nanmean
# in nan_aware.cpp.
if(NUMKIT_WITH_SIMD)
    list(APPEND STATSLIB_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/nan_aware/backends/nan_reductions_simd.cpp)
else()
    list(APPEND STATSLIB_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/nan_aware/backends/nan_reductions_portable.cpp)
endif()

set(STATSLIB_SOURCES ${STATSLIB_SOURCES} PARENT_SCOPE)
