# toolboxes/optim/tests/CMakeLists.txt
#
# Optimization Toolbox unit tests. Each new function adds a TU here via
# target_sources(numkit_gtest PRIVATE ...). Hand-runnable .m smokes live
# in tests/smoke/ (next to this file) and are not compiled.

target_sources(numkit_gtest PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/optim_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/fsolve_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/lsqnonlin_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/fminunc_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/quadprog_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/linprog_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/fmincon_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/known_bugs_test.cpp
)
