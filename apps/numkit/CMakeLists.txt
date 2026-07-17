# apps/numkit/CMakeLists.txt — owner of the `numkit` interpreter binary.
#
# The CMake TARGET is `numkit_cli` (the name `numkit` is already taken by the
# engine static library defined in the root CMakeLists.txt — CMake forbids two
# targets of the same name regardless of type). The OUTPUT_NAME property below
# makes the produced binary `numkit.exe` / `numkit.js` — the product command
# users type is `numkit`, the target name is an internal build-time detail.
#
# `numkit` is the product CLI for the interpreter: an interactive REPL with
# multi-line input (no args) and a batch `.m` runner (`numkit script.m`). It
# is also the harness that runs the hand-runnable `*_smoke.m` demos that ship
# per-library under `src/toolboxes/<lib>/tests/smoke/` — smoke files are just
# `.m` scripts, so the interpreter runs them unchanged.

if(NUMKIT_BUILD_CLI)
    add_executable(numkit_cli main.cpp)
    target_link_libraries(numkit_cli PRIVATE numkit)
    set_target_properties(numkit_cli PROPERTIES OUTPUT_NAME numkit)

    if(EMSCRIPTEN)
        # Emscripten produces a Node-runnable .js loader + .wasm sidecar
        # when the output ends in .js. NODERAWFS lets argv paths resolve
        # against the real host filesystem:
        #   node build/browser/apps/numkit/numkit.js path/to/script.m
        set_target_properties(numkit_cli PROPERTIES SUFFIX ".js")
        target_compile_options(numkit_cli PRIVATE -fexceptions)
        target_link_options(numkit_cli PRIVATE
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
