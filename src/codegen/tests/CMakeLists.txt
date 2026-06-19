target_sources(numkit_gtest PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/type_lattice_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/transfer_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/transfer_diff_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/inference_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/indexing_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/emitter_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/aot_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/monomorphize_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/classinfo_test.cpp
)
