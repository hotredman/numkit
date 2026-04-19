cmake_minimum_required(VERSION 3.14)

add_executable(numkit_mide_wasm src/repl_bindings.cpp)
target_link_libraries(numkit_mide_wasm PRIVATE numkit::m)

if(EMSCRIPTEN)
    set_target_properties(numkit_mide_wasm PROPERTIES
        OUTPUT_NAME "numkit_mide"
        SUFFIX ".js"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/dist"
    )

    target_link_options(numkit_mide_wasm PRIVATE
        --bind
        "SHELL:-s MODULARIZE=1"
        "SHELL:-s EXPORT_NAME=createNumkitMideModule"
        "SHELL:-s ALLOW_MEMORY_GROWTH=1"
        "SHELL:-s INITIAL_MEMORY=67108864"
        "SHELL:-s STACK_SIZE=4194304"
        "SHELL:-s EXPORTED_RUNTIME_METHODS=[FS]"
        "SHELL:-s DISABLE_EXCEPTION_CATCHING=0"
        -O2
    )

    target_compile_options(numkit_mide_wasm PRIVATE
        -fexceptions
        -O2
    )

else()
    message(WARNING "wasm/ is designed for Emscripten. Use: emcmake cmake .. -DNUMKIT_M_BUILD_REPL=ON")
endif()
