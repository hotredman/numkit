cmake_minimum_required(VERSION 3.14)

add_executable(matlab_repl src/repl_bindings.cpp)
target_link_libraries(matlab_repl PRIVATE mlab::mlab)

if(EMSCRIPTEN)
    set_target_properties(matlab_repl PROPERTIES
        OUTPUT_NAME "matlab_repl"
        SUFFIX ".js"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/dist"
    )

    target_link_options(matlab_repl PRIVATE
        --bind
        "SHELL:-s MODULARIZE=1"
        "SHELL:-s EXPORT_NAME=createMatlabModule"
        "SHELL:-s ALLOW_MEMORY_GROWTH=1"
        "SHELL:-s INITIAL_MEMORY=67108864"
        "SHELL:-s STACK_SIZE=4194304"
        "SHELL:-s EXPORTED_RUNTIME_METHODS=[FS]"
        "SHELL:-s DISABLE_EXCEPTION_CATCHING=0"
        -O2
    )

    target_compile_options(matlab_repl PRIVATE
        -fexceptions
        -O2
    )

    set(DIST_DIR "${CMAKE_CURRENT_BINARY_DIR}/dist")

    add_custom_command(TARGET matlab_repl POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${DIST_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/web/index.html"
            "${CMAKE_CURRENT_SOURCE_DIR}/web/repl.css"
            "${CMAKE_CURRENT_SOURCE_DIR}/web/repl.js"
            "${CMAKE_CURRENT_SOURCE_DIR}/web/terminal.js"
            "${CMAKE_CURRENT_SOURCE_DIR}/web/examples.js"
            "${DIST_DIR}/"
        COMMENT "Copying web assets to dist/"
    )

    add_custom_target(serve
        COMMAND ${CMAKE_COMMAND} -E echo "Open http://localhost:8080"
        COMMAND python3 -m http.server 8080 --directory "${DIST_DIR}"
        DEPENDS matlab_repl
        WORKING_DIRECTORY "${DIST_DIR}"
        COMMENT "Starting local dev server..."
    )

else()
    message(WARNING "repl/ is designed for Emscripten. Use: emcmake cmake .. -DMLAB_BUILD_REPL=ON")
endif()