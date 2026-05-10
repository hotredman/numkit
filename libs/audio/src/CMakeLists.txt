set(AUDIOLIB_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/library.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/scale/freq_scales.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/spectral/shape_descriptors.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/spectral/melspec_delta.cpp
)

set(AUDIOLIB_SOURCES ${AUDIOLIB_SOURCES} PARENT_SCOPE)
