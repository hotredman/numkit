# libs/linalg/tests/CMakeLists.txt
#
# Gtest unit tests for libs/linalg. Each *_test.cpp is appended to
# the global numkit_gtest target. Smoke .m files in tests/smoke/ are
# hand-runnable demos for numkit_smoke (not built into gtest).

target_sources(numkit_gtest PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/condeig_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/condest_test.cpp
)
