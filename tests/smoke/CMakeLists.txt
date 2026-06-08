# tests/smoke/CMakeLists.txt — owner of the numkit_smoke binary.
#
# Runs `.m` smoke scripts via the numkit engine. Used by
#   toolboxes/<lib>/tests/smoke/*_smoke.m
# scripts that are hand-runnable demos with `fprintf` of expected
# values inline. Not a "demo app" — it's a thin runner for the
# MATLAB-flavoured smoke files that ship per-library.

if(NUMKIT_BUILD_EXAMPLE)
    add_executable(numkit_smoke main.cpp)
    target_link_libraries(numkit_smoke PRIVATE numkit)

    if(EMSCRIPTEN)
        # Emscripten produces a Node-runnable .js loader + .wasm sidecar
        # when the output ends in .js. NODERAWFS lets argv paths resolve
        # against the real host filesystem:
        #   node build/browser/tests/smoke/numkit_smoke.js path/to/script.m
        set_target_properties(numkit_smoke PROPERTIES SUFFIX ".js")
        target_compile_options(numkit_smoke PRIVATE -fexceptions)
        target_link_options(numkit_smoke PRIVATE
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
