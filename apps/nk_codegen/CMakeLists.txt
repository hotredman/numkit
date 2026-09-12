# apps/nk_codegen/CMakeLists.txt
#
# nk_codegen — the `nk build` CLI front end (DESIGN.md §8 M4). It
# transpiles a nk source file's entry function to C++. The driver core
# (parseTypeSpec / transpileSource) lives in the codegen lib (carried by
# nk); this is a thin argv wrapper. Processed after the `nk` target
# is defined, so it can link it.
add_executable(nk_codegen ${CMAKE_CURRENT_SOURCE_DIR}/nk_codegen_main.cpp)
target_link_libraries(nk_codegen PRIVATE nk)
target_include_directories(nk_codegen PRIVATE ${CMAKE_SOURCE_DIR}/src/codegen/include)
