target_sources(numkit_gtest PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/io_extras_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/saveload_mat_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/csv_cpp_api_test.cpp  # Engine-free csvreadFromString/csvwriteToString
    ${CMAKE_CURRENT_SOURCE_DIR}/extras_cpp_api_test.cpp  # Engine-free readmatrixFromString/writematrixToString
)
