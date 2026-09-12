# apps/CMakeLists.txt — product executables (the nk CLI surface).
#
# Two siblings, both thin argv/embind wrappers over the `nk` engine lib
# (defined above this add_subdirectory in the root CMakeLists.txt):
#   apps/nk/            nk_repl   — the interpreter / REPL / .m runner.
#   apps/nk_codegen/    nk_codegen — the AOT transpiler CLI (codegen
#                                            driver, DESIGN.md §8 M4); native
#                                            only — a C++ compiler is not
#                                            available under Emscripten.
#
# The IDE is NOT here: it is an Electron + React shell (ide/) that loads the
# `nk_ide` WASM module (wasm/); the IDE's "generate C++" demo reaches the
# codegen transpile core already linked into that WASM module via embind.
#
# Built after the `nk` target is defined, so both apps can link it.

add_subdirectory(nk)
if(NOT EMSCRIPTEN)
    add_subdirectory(nk_codegen)
endif()
