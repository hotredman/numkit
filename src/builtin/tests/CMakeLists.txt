# src/builtin/tests/CMakeLists.txt

target_sources(numkit_gtest PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/ops_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/elmat_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/elfun_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/matfun_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/datafun_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/specfun_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/polyfun_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/strfun_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/timefun_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/datatypes_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/iofun_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/general_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/lang_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/builtin_test.cpp
)
