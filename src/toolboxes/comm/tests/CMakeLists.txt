# toolboxes/comm/tests/CMakeLists.txt
#
# Communications Toolbox unit tests. Each new function in /loop adds a
# TU here via target_sources(numkit_gtest PRIVATE ...). Hand-runnable
# .m smokes live in tests/smoke/ (next to this file) and are not compiled.

target_sources(numkit_gtest PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/comm_modulation_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/comm_pulse_scrambler_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/comm_fading_test.cpp
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
    ${CMAKE_CURRENT_SOURCE_DIR}/mil188_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/arith_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mskmod_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mskdemod_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/base_conversions_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/pammod_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/poly2trellis_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/convenc_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/vitdec_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/istrellis_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gen2par_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/hammgen_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cyclpoly_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cyclgen_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/encode_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/decode_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/snr_ber_metrics_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/syndtable_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/analog_demod_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/known_bugs_test.cpp
)
