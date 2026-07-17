# toolboxes/ode/tests/CMakeLists.txt
#
# Gtest unit tests for toolboxes/ode. Each *_test.cpp is appended to the
# global numkit_gtest target. Smoke .m files in tests/smoke/ are
# hand-runnable demos for numkit (not built into gtest).

target_sources(numkit_gtest PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/ode45_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ode23_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/odeset_odeget_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ode_cpp_api_test.cpp  # Engine-free FnHandle C++ API
)
