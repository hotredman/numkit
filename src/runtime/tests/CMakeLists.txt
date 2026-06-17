# Auto-grouped from tests/builtin (layer split 2026-06-17). Attaches to numkit_gtest.
target_sources(numkit_gtest PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/accum_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/accumarray_integer_vals_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cellfun_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cellfun_inputforms_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/group_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/group_public_api_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/groupfilter_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/groupsummary_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/grouptransform_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/splitapply_test.cpp
)
