# libs/image/tests/CMakeLists.txt
#
# Image Toolbox unit tests. Each new function in /loop adds a TU here
# via target_sources(numkit_gtest PRIVATE ...). Hand-runnable .m smokes
# live in tests/smoke/ (next to this file) and are not compiled.

target_sources(numkit_gtest PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/wcodemat_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/image_batch1_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/image_batch2_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/image_batch3_test.cpp
)
