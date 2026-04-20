target_sources(m_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/signal_core_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/convolution_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/filter_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/filter_design_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/spectral_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/windows_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/resample_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/transform_test.cpp
)
