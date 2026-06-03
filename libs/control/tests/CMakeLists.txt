# libs/control/tests/CMakeLists.txt
#
# Control Toolbox unit tests. Each new function in /loop adds a TU here
# via target_sources(numkit_gtest PRIVATE ...). Hand-runnable .m smokes
# live in tests/smoke/ (next to this file) and are not compiled.

target_sources(numkit_gtest PRIVATE
    # Add per-function gtest TUs here.
    ${CMAKE_CURRENT_SOURCE_DIR}/response_xtraj_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/c2d_foh_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/known_bugs_test.cpp
)
