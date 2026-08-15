target_sources(numkit_gtest PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/freq_scales_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/spectral_shape_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/melspec_delta_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cepstral_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/pitch_harmonics_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/wav_codec_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/flac_codec_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mp3_codec_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/aiff_au_midi_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/audio_robustness_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/audio_e2e_io_test.cpp
)
