# libs/comm/tests/CMakeLists.txt
#
# Communications Toolbox unit tests. Each new function in /loop adds a
# TU here via target_sources(numkit_gtest PRIVATE ...). Hand-runnable
# .m smokes live in tests/smoke/ (next to this file) and are not compiled.

target_sources(numkit_gtest PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/biterr_symerr_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/compand_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/pmmod_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ammod_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/fmmod_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ssbmod_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/randsrc_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/randerr_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/genqam_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/huffmandict_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/huffman_codec_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/dpcm_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/quantiz_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/lloyds_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/dpcmopt_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/apsk_test.cpp
)
