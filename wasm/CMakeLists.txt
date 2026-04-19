cmake_minimum_required(VERSION 3.14)

add_executable(numkit_ide_wasm src/repl_bindings.cpp)
target_link_libraries(numkit_ide_wasm PRIVATE numkit::numkit)

if(EMSCRIPTEN)
    set_target_properties(numkit_ide_wasm PROPERTIES
        OUTPUT_NAME "numkit_ide"
        SUFFIX ".js"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/dist"
    )

    target_link_options(numkit_ide_wasm PRIVATE
        --bind
        "SHELL:-s MODULARIZE=1"
        "SHELL:-s EXPORT_NAME=createNumkitModule"
        "SHELL:-s ALLOW_MEMORY_GROWTH=1"
        "SHELL:-s INITIAL_MEMORY=67108864"
        "SHELL:-s STACK_SIZE=4194304"
        "SHELL:-s EXPORTED_RUNTIME_METHODS=[FS]"
        "SHELL:-s DISABLE_EXCEPTION_CATCHING=0"
        -O2
    )

    target_compile_options(numkit_ide_wasm PRIVATE
        -fexceptions
        -O2
    )

else()
    message(WARNING "wasm/ is designed for Emscripten. Use: emcmake cmake .. -DNUMKIT_BUILD_REPL=ON")
endif()
