# toolboxes/control/tests/CMakeLists.txt
#
# Control Toolbox unit tests. Each new function in /loop adds a TU here
# via target_sources(numkit_gtest PRIVATE ...). Hand-runnable .m smokes
# live in tests/smoke/ (next to this file) and are not compiled.

target_sources(numkit_gtest PRIVATE
    # Add per-function gtest TUs here.
    ${CMAKE_CURRENT_SOURCE_DIR}/control_core_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/control_freq_lti_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/response_xtraj_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/c2d_foh_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/connect_numeric_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/lti_introspection_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/care_dare_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/hinfnorm_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/initial_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/known_bugs_test.cpp
)
