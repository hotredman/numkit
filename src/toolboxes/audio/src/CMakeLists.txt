set(AUDIOLIB_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/library.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/scale/freq_scales.cpp               # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/spectral/shape_descriptors.cpp      # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/spectral/melspec_delta.cpp          # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/spectral/cepstral.cpp            # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/features/pitch_harmonics.cpp     # compute (engine-free)
)

set(AUDIOLIB_SOURCES ${AUDIOLIB_SOURCES} PARENT_SCOPE)
