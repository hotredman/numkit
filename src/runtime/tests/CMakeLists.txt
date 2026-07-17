# src/runtime/tests/CMakeLists.txt
#
# runtime-layer unit tests (cellfun / accumarray / splitapply / group*). Each
# adds a TU via target_sources(numkit_gtest PRIVATE ...). Smokes in smoke/.
target_sources(numkit_gtest PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/accum_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/accumarray_integer_vals_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cellfun_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cellfun_inputforms_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/coder_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/group_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/group_public_api_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/groupfilter_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/groupsummary_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/grouptransform_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/splitapply_test.cpp
)
