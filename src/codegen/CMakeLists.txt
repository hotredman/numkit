# codegen/CMakeLists.txt
#
# nk code generation — a static type-inference pass over a parsed
# AST (AST -> typed IR) and a transpiler that emits C++ calling the
# existing nk runtime, for AOT compilation of nk code via an
# external compiler.
#
# Pure C++ analysis/codegen pass: core-coupled (parser / AST), registers
# NO engine builtin. Architectural sibling of scriptgraph (L2 analysis
# layer). Compiled into nk_toolboxes_obj via NK_LIBS_SOURCES.

add_subdirectory(src)  # exports CODEGENLIB_SOURCES via PARENT_SCOPE

# AOT harness config (brick 4): capture the external C++ compiler the
# emitted code is compiled with at run time. Paths use forward slashes
# (valid in a C++ string literal); aot.cpp converts to native separators.
set(NK_CODEGEN_AOT_AVAILABLE 0)
set(NK_CODEGEN_AOT_IS_MSVC 0)
set(NK_CODEGEN_AOT_VCVARS "")
set(NK_CODEGEN_AOT_CXX "${CMAKE_CXX_COMPILER}")
if(MSVC AND CMAKE_GENERATOR_INSTANCE)
    set(_nk_vcvars "${CMAKE_GENERATOR_INSTANCE}/VC/Auxiliary/Build/vcvars64.bat")
    if(EXISTS "${_nk_vcvars}")
        set(NK_CODEGEN_AOT_AVAILABLE 1)
        set(NK_CODEGEN_AOT_IS_MSVC 1)
        set(NK_CODEGEN_AOT_VCVARS "${_nk_vcvars}")
    endif()
elseif(NOT MSVC AND CMAKE_CXX_COMPILER)
    # A non-MSVC compiler is assumed self-sufficient (headers/libs on its
    # own search paths) — invoked directly, no vcvars shell.
    set(NK_CODEGEN_AOT_AVAILABLE 1)
endif()
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/aot_config.hpp.in
    ${CMAKE_CURRENT_BINARY_DIR}/generated/nk/codegen/aot_config.hpp
    @ONLY)

list(APPEND NK_LIBS_SOURCES ${CODEGENLIB_SOURCES})
list(APPEND NK_LIBS_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/include)
list(APPEND NK_LIBS_INCLUDE_DIRS ${CMAKE_CURRENT_BINARY_DIR}/generated)

list(APPEND NK_LIBS_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/codegen/type_lattice.hpp
)

set(NK_LIBS_SOURCES      ${NK_LIBS_SOURCES}      PARENT_SCOPE)
set(NK_LIBS_HEADERS      ${NK_LIBS_HEADERS}      PARENT_SCOPE)
set(NK_LIBS_INCLUDE_DIRS ${NK_LIBS_INCLUDE_DIRS} PARENT_SCOPE)
