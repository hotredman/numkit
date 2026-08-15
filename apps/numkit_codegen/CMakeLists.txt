# apps/numkit_codegen/CMakeLists.txt
#
# numkit_codegen — the `numkit build` CLI front end (DESIGN.md §8 M4). It
# transpiles a numkit source file's entry function to C++. The driver core
# (parseTypeSpec / transpileSource) lives in the codegen lib (carried by
# numkit); this is a thin argv wrapper. Processed after the `numkit` target
# is defined, so it can link it.
add_executable(numkit_codegen ${CMAKE_CURRENT_SOURCE_DIR}/numkit_codegen_main.cpp)
target_link_libraries(numkit_codegen PRIVATE numkit)
target_include_directories(numkit_codegen PRIVATE ${CMAKE_SOURCE_DIR}/src/codegen/include)
