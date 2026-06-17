# src/bundle/tests/CMakeLists.txt
#
# Cross-cutting tests that exercise the assembled standard library (functions
# spanning many layers) + the base-layer known_bugs regression catalog. Each
# adds a TU via target_sources(numkit_gtest PRIVATE ...). Smokes in smoke/.
target_sources(numkit_gtest PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/builtins_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cell_struct_numtheory_batch_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/construct_search_batch_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/known_bugs_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/misc2_batch_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/misc3_batch_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/misc4_batch_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/misc5_batch_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/misc6_batch_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/misc_batch_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/predicates2_batch_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/quickwins5_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/simd_parity_test.cpp
)
