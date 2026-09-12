# apps/nk/CMakeLists.txt — owner of the `nk_repl` interpreter binary.
#
# The CMake TARGET is `nk_cli` (the name `nk` is already taken by the
# engine static library defined in the root CMakeLists.txt — CMake forbids two
# targets of the same name regardless of type). The OUTPUT_NAME property below
# makes the produced binary `nk_repl.exe` / `nk_repl.js` — the product
# command users type is `nk_repl`, the target name is an internal detail.
#
# `nk_repl` is the product CLI for the interpreter: an interactive REPL with
# multi-line input (no args) and a batch `.m` runner (`nk_repl script.m`).
# It is also the harness that runs the hand-runnable `*_smoke.m` demos that ship
# per-library under `src/toolboxes/<lib>/tests/smoke/` — smoke files are just
# `.m` scripts, so the interpreter runs them unchanged.

if(NK_BUILD_CLI)
    add_executable(nk_cli main.cpp)
    target_link_libraries(nk_cli PRIVATE nk)
    set_target_properties(nk_cli PROPERTIES OUTPUT_NAME nk_repl)

    if(EMSCRIPTEN)
        # Emscripten produces a Node-runnable .js loader + .wasm sidecar
        # when the output ends in .js. NODERAWFS lets argv paths resolve
        # against the real host filesystem:
        #   node build/browser/apps/nk/nk_repl.js path/to/script.m
        set_target_properties(nk_cli PROPERTIES SUFFIX ".js")
        target_compile_options(nk_cli PRIVATE -fexceptions)
        target_link_options(nk_cli PRIVATE
            -fexceptions
            "SHELL:-s ALLOW_MEMORY_GROWTH=1"
            "SHELL:-s INITIAL_MEMORY=67108864"
            "SHELL:-s STACK_SIZE=4194304"
            "SHELL:-s EXIT_RUNTIME=1"
            "SHELL:-s NODERAWFS=1"
            "SHELL:-s DISABLE_EXCEPTION_CATCHING=0"
        )
    endif()
endif()
