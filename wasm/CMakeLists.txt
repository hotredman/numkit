cmake_minimum_required(VERSION 3.14)

add_executable(nk_ide_wasm src/repl_bindings.cpp)
target_link_libraries(nk_ide_wasm PRIVATE nk::nk)

if(EMSCRIPTEN)
    set_target_properties(nk_ide_wasm PROPERTIES
        OUTPUT_NAME "nk_ide"
        SUFFIX ".js"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/dist"
    )

    target_link_options(nk_ide_wasm PRIVATE
        --bind
        "SHELL:-s MODULARIZE=1"
        "SHELL:-s EXPORT_NAME=createNkIdeModule"
        "SHELL:-s ALLOW_MEMORY_GROWTH=1"
        "SHELL:-s INITIAL_MEMORY=67108864"
        "SHELL:-s STACK_SIZE=4194304"
        "SHELL:-s EXPORTED_RUNTIME_METHODS=[FS]"
        "SHELL:-s DISABLE_EXCEPTION_CATCHING=0"
        -O2
    )

    target_compile_options(nk_ide_wasm PRIVATE
        -fexceptions
        -O2
    )

else()
    message(WARNING "wasm/ is designed for Emscripten. Use: emcmake cmake .. -DNK_BUILD_REPL=ON")
endif()
