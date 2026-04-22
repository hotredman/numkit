# CMakeLists.txt
cmake_minimum_required(VERSION 3.21)
project(m VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(NUMKIT_M_BUILD_EXAMPLE    "Build example executable"              ON)
option(NUMKIT_M_BUILD_TESTS      "Build tests"                           ON)
option(NUMKIT_M_BUILD_BENCHMARKS "Build Google Benchmark micro-benches"  OFF)
option(NUMKIT_M_BUILD_SHARED     "Build shared library"                  OFF)
option(NUMKIT_M_INSTALL          "Generate install target"               ON)

# Feature flags (NUMKIT_WITH_SIMD)
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
include(NumkitOptions)

# ──────────────────────────────────────────────
# Google Highway (SIMD-intrinsics abstraction)
# ──────────────────────────────────────────────
#
# Pulled only when NUMKIT_WITH_SIMD=ON. Dispatch strategy is dynamic —
# the library ships with backends for every ISA Highway supports and
# picks one at first call (see HWY_DYNAMIC_DISPATCH in the _simd.cpp
# files under libs/*/src/backends/).

if(NUMKIT_WITH_SIMD)
    include(FetchContent)

    FetchContent_Declare(
        highway
        GIT_REPOSITORY https://github.com/google/highway.git
        GIT_TAG        1.1.0
    )

    # Don't pull Highway's own tests / examples into our tree. contrib is
    # enabled because Phase 8b uses hwy/contrib/math (Sin/Cos/Exp/Log).
    set(HWY_ENABLE_TESTS    OFF CACHE BOOL "" FORCE)
    set(HWY_ENABLE_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(HWY_ENABLE_INSTALL  OFF CACHE BOOL "" FORCE)
    set(HWY_ENABLE_CONTRIB  ON  CACHE BOOL "" FORCE)

    FetchContent_MakeAvailable(highway)
endif()

if(NOT EMSCRIPTEN)
    set(NUMKIT_M_BUILD_REPL OFF CACHE BOOL "Build WebAssembly REPL" FORCE)
else()
    option(NUMKIT_M_BUILD_REPL "Build WebAssembly module" ON)
endif()

# ──────────────────────────────────────────────
# Основная библиотека
# ──────────────────────────────────────────────

set(NUMKIT_M_CORE_SOURCES
    # value / environment / allocator
    core/src/MAllocator.cpp
    core/src/MType.cpp
    core/src/MDims.cpp
    core/src/MDataBuffer.cpp
    core/src/MHeapObject.cpp
    core/src/MValue.cpp
    core/src/MEnvironment.cpp
    core/src/MEngine.cpp
    core/src/MVfs.cpp
    # lexer / parser / AST / compiler
    core/src/MLexer.cpp
    core/src/MParser.cpp
    core/src/MAst.cpp
    core/src/MCompiler.cpp
    # execution backends
    core/src/MTreeWalker.cpp
    core/src/MVM.cpp
    # debugger
    core/src/MDebugger.cpp
    core/src/MDebugSession.cpp
    core/src/MDebugWorkspace.cpp
)

if(NUMKIT_WITH_THREADS)
    list(APPEND NUMKIT_M_CORE_SOURCES core/src/MThreadPool.cpp)
endif()

# Each library defines its own source list (STDLIB_SOURCES, DSPLIB_SOURCES, ...)
add_subdirectory(libs/builtin/src)
add_subdirectory(libs/dsp/src)
add_subdirectory(libs/plt/src)
add_subdirectory(libs/fit/src)

set(NUMKIT_M_SOURCES
    ${NUMKIT_M_CORE_SOURCES}
    ${STDLIB_SOURCES}
    ${DSPLIB_SOURCES}
    ${PLTLIB_SOURCES}
    ${FITLIB_SOURCES}
)

set(NUMKIT_M_HEADERS
    # core (runtime + frontend + backend + debug all live here)
    core/include/numkit/m/core/MAllocator.hpp
    core/include/numkit/m/core/MAst.hpp
    core/include/numkit/m/core/MBranding.hpp
    core/include/numkit/m/core/MBytecode.hpp
    core/include/numkit/m/core/MCompiler.hpp
    core/include/numkit/m/core/MDataBuffer.hpp
    core/include/numkit/m/core/MDebugger.hpp
    core/include/numkit/m/core/MDebugSession.hpp
    core/include/numkit/m/core/MDebugWorkspace.hpp
    core/include/numkit/m/core/MDims.hpp
    core/include/numkit/m/core/MEngine.hpp
    core/include/numkit/m/core/MEnvironment.hpp
    core/include/numkit/m/core/MFigureManager.hpp
    core/include/numkit/m/core/MHeapObject.hpp
    core/include/numkit/m/core/MLexer.hpp
    core/include/numkit/m/core/MParser.hpp
    core/include/numkit/m/core/MParallelFor.hpp
    core/include/numkit/m/core/MSpan.hpp
    core/include/numkit/m/core/MThreadPool.hpp
    core/include/numkit/m/core/MTreeWalker.hpp
    core/include/numkit/m/core/MType.hpp
    core/include/numkit/m/core/MTypes.hpp
    core/include/numkit/m/core/MVM.hpp
    core/include/numkit/m/core/MValue.hpp
    core/include/numkit/m/core/MVfs.hpp
    # libs
    libs/builtin/include/numkit/m/builtin/MStdLibrary.hpp
    libs/builtin/include/numkit/m/builtin/MStdMath.hpp
    libs/builtin/include/numkit/m/builtin/MStdMatrix.hpp
    libs/builtin/include/numkit/m/builtin/MStdBinaryOps.hpp
    libs/builtin/include/numkit/m/builtin/MStdUnaryOps.hpp
    libs/builtin/include/numkit/m/builtin/MStdComplex.hpp
    libs/builtin/include/numkit/m/builtin/MStdStrings.hpp
    libs/builtin/include/numkit/m/builtin/MStdTypes.hpp
    libs/builtin/include/numkit/m/builtin/MStdCellStruct.hpp
    libs/builtin/include/numkit/m/builtin/MStdFormat.hpp
    libs/builtin/include/numkit/m/builtin/MStdDiagnostics.hpp
    libs/builtin/include/numkit/m/builtin/MStdPrint.hpp
    libs/builtin/include/numkit/m/builtin/MStdFileIO.hpp
    libs/builtin/include/numkit/m/builtin/MStdScan.hpp
    libs/builtin/include/numkit/m/builtin/MStdCsv.hpp
    libs/builtin/include/numkit/m/builtin/MStdEnv.hpp
    libs/builtin/include/numkit/m/builtin/MStdSaveLoad.hpp
    libs/dsp/include/numkit/m/dsp/MDspLibrary.hpp
    libs/dsp/include/numkit/m/dsp/MDspFft.hpp
    libs/dsp/include/numkit/m/dsp/MDspConv.hpp
    libs/dsp/include/numkit/m/dsp/MDspFilter.hpp
    libs/dsp/include/numkit/m/dsp/MDspFilterDesign.hpp
    libs/dsp/include/numkit/m/dsp/MDspResample.hpp
    libs/dsp/include/numkit/m/dsp/MDspSpectral.hpp
    libs/dsp/include/numkit/m/dsp/MDspWindows.hpp
    libs/dsp/include/numkit/m/dsp/MDspTransform.hpp
    libs/dsp/include/numkit/m/dsp/MDspSignalCore.hpp
    libs/fit/include/numkit/m/fit/MFitLibrary.hpp
    libs/fit/include/numkit/m/fit/MFitInterp.hpp
    libs/plt/include/numkit/m/plt/MPltLibrary.hpp
)

if(NUMKIT_M_BUILD_SHARED)
    add_library(m SHARED ${NUMKIT_M_SOURCES} ${NUMKIT_M_HEADERS})
else()
    add_library(m STATIC ${NUMKIT_M_SOURCES} ${NUMKIT_M_HEADERS})
endif()

add_library(numkit::m ALIAS m)

target_include_directories(m
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/core/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/libs/builtin/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/libs/dsp/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/libs/fit/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/libs/plt/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        # Shared library-internal helpers (MStdHelpers.hpp, MDspHelpers.hpp)
        # TODO: formalize as public "detail/" headers or common utility lib
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/builtin/src
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/dsp/src
)

target_compile_features(m PUBLIC cxx_std_17)

if(EMSCRIPTEN)
    target_compile_options(m PRIVATE -fexceptions)
    target_link_options(m PUBLIC "SHELL:-s DISABLE_EXCEPTION_CATCHING=0")
endif()

if(NUMKIT_WITH_SIMD)
    # PRIVATE — our public headers never mention Highway types; it's a pure
    # implementation detail confined to libs/*/src/backends/*_simd.cpp.
    # BUILD_INTERFACE genex keeps the link dep out of the exported target
    # set (install(EXPORT) would otherwise complain that hwy / hwy_contrib
    # aren't themselves exported).
    target_link_libraries(m PRIVATE
        $<BUILD_INTERFACE:hwy>
        $<BUILD_INTERFACE:hwy_contrib>)
    target_compile_definitions(m PRIVATE NUMKIT_WITH_SIMD=1)
endif()

if(NUMKIT_WITH_THREADS)
    # std::thread support. The pool itself lives in core/src/MThreadPool.cpp
    # and is only compiled when this option is ON. Without the option, the
    # parallel_for helper falls through to a sequential `fn(0, n)` call
    # (header-only inline) and pthread/Threads is not linked.
    find_package(Threads REQUIRED)
    target_link_libraries(m PUBLIC Threads::Threads)
    target_compile_definitions(m PUBLIC NUMKIT_WITH_THREADS=1)
endif()

# ──────────────────────────────────────────────
# Example
# ──────────────────────────────────────────────

if(NUMKIT_M_BUILD_EXAMPLE)
    add_executable(m_example example/main.cpp)
    target_link_libraries(m_example PRIVATE numkit::m)

    if(EMSCRIPTEN)
        # Emscripten produces a Node-runnable .js loader + .wasm sidecar
        # when the output ends in .js. NODERAWFS lets argv paths resolve
        # against the real host filesystem:
        #   node build-browser/example/m_example.js path/to/script.m
        set_target_properties(m_example PROPERTIES SUFFIX ".js")
        target_compile_options(m_example PRIVATE -fexceptions)
        target_link_options(m_example PRIVATE
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

# ──────────────────────────────────────────────
# Тесты (Google Test)
# ──────────────────────────────────────────────

if(NUMKIT_M_BUILD_TESTS AND NOT EMSCRIPTEN)
    include(FetchContent)

    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG        v1.14.0
    )

    # Windows: не ломать runtime library
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

    FetchContent_MakeAvailable(googletest)

    enable_testing()

    # tests/ creates the m_tests target; per-component tests/ dirs
    # contribute their sources via target_sources(m_tests PRIVATE ...)
    add_subdirectory(tests)
    add_subdirectory(core/tests)
    add_subdirectory(libs/builtin/tests)
    add_subdirectory(libs/dsp/tests)
    add_subdirectory(libs/fit/tests)
    add_subdirectory(libs/plt/tests)
endif()

# ──────────────────────────────────────────────
# Benchmarks (Google Benchmark)
# ──────────────────────────────────────────────

if(NUMKIT_M_BUILD_BENCHMARKS)
    include(FetchContent)

    FetchContent_Declare(
        benchmark
        GIT_REPOSITORY https://github.com/google/benchmark.git
        GIT_TAG        v1.8.3
    )

    # Don't build Google Benchmark's own tests or docs inside our tree.
    set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)
    # v1.8.3 enables -Werror by default; newer clang (emscripten upstream)
    # flags __COUNTER__ as a C2y-extension and fails the build. Our code
    # doesn't depend on strict-warnings inside the benchmark lib itself.
    set(BENCHMARK_ENABLE_WERROR OFF CACHE BOOL "" FORCE)

    FetchContent_MakeAvailable(benchmark)

    # Benchmark's own flag list also includes -pedantic-errors, which turns
    # the C2y __COUNTER__ extension warning into an error on upstream clang
    # (emscripten). Scoped to the two benchmark libs only — and only on
    # clang/gcc, since MSVC doesn't understand the flag.
    if(NOT MSVC OR CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "GNU")
        foreach(_t benchmark benchmark_main)
            if(TARGET ${_t})
                target_compile_options(${_t} PRIVATE -Wno-c2y-extensions)
            endif()
        endforeach()
    endif()

    add_subdirectory(benchmarks)
endif()

# ──────────────────────────────────────────────
# REPL subdirectory
# ──────────────────────────────────────────────

if(NUMKIT_M_BUILD_REPL)
    add_subdirectory(wasm)
endif()

# ──────────────────────────────────────────────
# Install
# ──────────────────────────────────────────────

if(NUMKIT_M_INSTALL AND NOT EMSCRIPTEN)
    include(GNUInstallDirs)
    include(CMakePackageConfigHelpers)

    install(TARGETS m
        EXPORT mTargets
        LIBRARY  DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE  DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME  DESTINATION ${CMAKE_INSTALL_BINDIR}
    )

    # Public headers — every component ships its own numkit/m/<name>/ subtree
    install(DIRECTORY core/include/numkit         DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
    install(DIRECTORY libs/builtin/include/numkit DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
    install(DIRECTORY libs/dsp/include/numkit     DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
    install(DIRECTORY libs/fit/include/numkit     DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
    install(DIRECTORY libs/plt/include/numkit     DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

    install(EXPORT mTargets
        FILE        mTargets.cmake
        NAMESPACE   numkit::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/m
    )

    configure_package_config_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/mConfig.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/mConfig.cmake"
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/m
    )

    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/mConfigVersion.cmake"
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion
    )

    install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/mConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/mConfigVersion.cmake"
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/m
    )
endif()
