# CMakeLists.txt
cmake_minimum_required(VERSION 3.21)
project(numkit VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(NUMKIT_BUILD_EXAMPLE    "Build example executable"              ON)
option(NUMKIT_BUILD_TESTS      "Build tests"                           ON)
option(NUMKIT_BUILD_BENCHMARKS "Build Google Benchmark micro-benches"  OFF)
option(NUMKIT_BUILD_SHARED     "Build shared library"                  OFF)
option(NUMKIT_INSTALL          "Generate install target"               ON)

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
    set(NUMKIT_BUILD_REPL OFF CACHE BOOL "Build WebAssembly REPL" FORCE)
else()
    option(NUMKIT_BUILD_REPL "Build WebAssembly module" ON)
endif()

# ──────────────────────────────────────────────
# Основная библиотека
# ──────────────────────────────────────────────

set(NUMKIT_CORE_SOURCES
    # value / environment / allocator
    core/src/allocator.cpp
    core/src/value_type.cpp
    core/src/dims.cpp
    core/src/data_buffer.cpp
    core/src/heap_object.cpp
    core/src/value.cpp
    core/src/environment.cpp
    core/src/engine.cpp
    core/src/vfs.cpp
    # lexer / parser / AST / compiler
    core/src/lexer.cpp
    core/src/parser.cpp
    core/src/ast.cpp
    core/src/compiler.cpp
    # execution backends
    core/src/tree_walker.cpp
    core/src/vm.cpp
    # debugger
    core/src/debugger.cpp
    core/src/debug_session.cpp
    core/src/debug_workspace.cpp
)

if(NUMKIT_WITH_THREADS)
    list(APPEND NUMKIT_CORE_SOURCES core/src/thread_pool.cpp)
endif()

# Each library defines its own source list (STDLIB_SOURCES, SIGNALLIB_SOURCES, ...)
add_subdirectory(libs/builtin/src)
add_subdirectory(libs/signal/src)
add_subdirectory(libs/stats/src)
add_subdirectory(libs/graphics/src)

set(NUMKIT_SOURCES
    ${NUMKIT_CORE_SOURCES}
    ${STDLIB_SOURCES}
    ${SIGNALLIB_SOURCES}
    ${STATSLIB_SOURCES}
    ${GRAPHICSLIB_SOURCES}
)

set(NUMKIT_HEADERS
    # core (runtime + frontend + backend + debug all live here)
    core/include/numkit/core/allocator.hpp
    core/include/numkit/core/ast.hpp
    core/include/numkit/core/branding.hpp
    core/include/numkit/core/bytecode.hpp
    core/include/numkit/core/compiler.hpp
    core/include/numkit/core/data_buffer.hpp
    core/include/numkit/core/debugger.hpp
    core/include/numkit/core/debug_session.hpp
    core/include/numkit/core/debug_workspace.hpp
    core/include/numkit/core/dims.hpp
    core/include/numkit/core/engine.hpp
    core/include/numkit/core/environment.hpp
    core/include/numkit/core/figure_manager.hpp
    core/include/numkit/core/heap_object.hpp
    core/include/numkit/core/lexer.hpp
    core/include/numkit/core/parser.hpp
    core/include/numkit/core/parallel_for.hpp
    core/include/numkit/core/shape_ops.hpp
    core/include/numkit/core/span.hpp
    core/include/numkit/core/thread_pool.hpp
    core/include/numkit/core/tree_walker.hpp
    core/include/numkit/core/value_type.hpp
    core/include/numkit/core/types.hpp
    core/include/numkit/core/vm.hpp
    core/include/numkit/core/value.hpp
    core/include/numkit/core/vfs.hpp
    # libs
    libs/builtin/include/numkit/builtin/library.hpp
    libs/builtin/include/numkit/builtin/math/elementary/trigonometry.hpp
    libs/builtin/include/numkit/builtin/math/elementary/exponents.hpp
    libs/builtin/include/numkit/builtin/math/elementary/rounding.hpp
    libs/builtin/include/numkit/builtin/math/elementary/misc.hpp
    libs/builtin/include/numkit/builtin/math/elementary/special.hpp
    libs/builtin/include/numkit/builtin/math/elementary/reductions.hpp
    libs/builtin/include/numkit/builtin/math/elementary/discrete.hpp
    libs/builtin/include/numkit/builtin/datatypes/cell/cell.hpp
    libs/builtin/include/numkit/builtin/datatypes/struct/struct.hpp
    libs/builtin/include/numkit/builtin/datatypes/strings/strings.hpp
    libs/builtin/include/numkit/builtin/datatypes/strings/regex.hpp
    libs/builtin/include/numkit/builtin/math/integration/integration.hpp
    libs/builtin/include/numkit/builtin/math/optim/fzero.hpp
    libs/builtin/include/numkit/builtin/lang/commands/env.hpp
    libs/builtin/include/numkit/builtin/lang/operators/binary_ops.hpp
    libs/builtin/include/numkit/builtin/lang/operators/unary_ops.hpp
    libs/builtin/include/numkit/builtin/lang/arrays/matrix.hpp
    libs/builtin/include/numkit/builtin/lang/arrays/manip.hpp
    libs/builtin/include/numkit/builtin/lang/arrays/nd_manip.hpp
    libs/builtin/include/numkit/builtin/lang/arrays/accum.hpp
    libs/builtin/include/numkit/builtin/math/random/rng.hpp
    libs/builtin/include/numkit/builtin/math/elementary/complex.hpp
    libs/builtin/include/numkit/builtin/math/elementary/polynomials.hpp
    libs/builtin/include/numkit/builtin/data_io/csv.hpp
    libs/builtin/include/numkit/builtin/data_io/fileio.hpp
    libs/builtin/include/numkit/builtin/data_io/saveload.hpp
    libs/builtin/include/numkit/builtin/datatypes/numeric/types.hpp
    libs/builtin/include/numkit/builtin/datatypes/numeric/int_math.hpp
    libs/builtin/include/numkit/builtin/datatypes/strings/format.hpp
    libs/builtin/include/numkit/builtin/datatypes/strings/print.hpp
    libs/builtin/include/numkit/builtin/datatypes/strings/scan.hpp
    libs/builtin/include/numkit/builtin/programming/errors/diagnostics.hpp
    libs/builtin/include/numkit/builtin/data_analysis/descriptive_statistics/stats.hpp
    libs/signal/include/numkit/signal/library.hpp
    libs/signal/include/numkit/signal/waveform_generation/waveform_generation.hpp
    libs/signal/include/numkit/signal/filter_design/filter_design.hpp
    libs/signal/include/numkit/signal/filter_analysis/frequency_response.hpp
    libs/signal/include/numkit/signal/filter_analysis/unwrap.hpp
    libs/signal/include/numkit/signal/digital_filtering/filter.hpp
    libs/signal/include/numkit/signal/digital_filtering/sosfilt.hpp
    libs/signal/include/numkit/signal/filter_implementation/conversions.hpp
    libs/signal/include/numkit/signal/multirate/multirate.hpp
    libs/signal/include/numkit/signal/convolution/convolution.hpp
    libs/signal/include/numkit/signal/transforms/fft.hpp
    libs/signal/include/numkit/signal/transforms/hilbert.hpp
    libs/signal/include/numkit/signal/transforms/dct.hpp
    libs/signal/include/numkit/signal/transforms/goertzel.hpp
    libs/signal/include/numkit/signal/transforms/transform_helpers.hpp
    libs/signal/include/numkit/signal/smoothing/sgolay.hpp
    libs/signal/include/numkit/signal/smoothing/medfilt.hpp
    libs/signal/include/numkit/signal/measurements/findpeaks.hpp
    libs/signal/include/numkit/signal/spectral_analysis/periodogram_pwelch.hpp
    libs/signal/include/numkit/signal/time_frequency/spectrogram.hpp
    libs/signal/include/numkit/signal/windows/windows.hpp
    libs/builtin/include/numkit/builtin/math/interpolation/interp.hpp
    libs/stats/include/numkit/stats/library.hpp
    libs/stats/include/numkit/stats/moments/moments.hpp
    libs/stats/include/numkit/stats/nan_aware/nan_aware.hpp
    libs/graphics/include/numkit/graphics/library.hpp
)

if(NUMKIT_BUILD_SHARED)
    add_library(numkit SHARED ${NUMKIT_SOURCES} ${NUMKIT_HEADERS})
else()
    add_library(numkit STATIC ${NUMKIT_SOURCES} ${NUMKIT_HEADERS})
endif()

# Match install-tree NAMESPACE so add_subdirectory consumers can use the
# same `numkit::numkit` target as find_package() consumers do.
add_library(numkit::numkit ALIAS numkit)

target_include_directories(numkit
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/core/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/libs/builtin/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/libs/signal/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/libs/stats/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/libs/graphics/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        # Shared library-internal helpers (helpers.hpp under libs/*/src/)
        # TODO: formalize as public "detail/" headers or common utility lib
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/builtin/src
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/signal/src
)

target_compile_features(numkit PUBLIC cxx_std_17)

if(EMSCRIPTEN)
    target_compile_options(numkit PRIVATE -fexceptions)
    target_link_options(numkit PUBLIC "SHELL:-s DISABLE_EXCEPTION_CATCHING=0")
endif()

if(NUMKIT_WITH_SIMD)
    # PRIVATE — our public headers never mention Highway types; it's a pure
    # implementation detail confined to libs/*/src/backends/*_simd.cpp.
    # BUILD_INTERFACE genex keeps the link dep out of the exported target
    # set (install(EXPORT) would otherwise complain that hwy / hwy_contrib
    # aren't themselves exported).
    target_link_libraries(numkit PRIVATE
        $<BUILD_INTERFACE:hwy>
        $<BUILD_INTERFACE:hwy_contrib>)
    # PUBLIC — backends/FftKernels.hpp declares SoA / Stockham dispatchers
    # whose definitions are only built when SIMD is on. Direct callers
    # (benchmarks, kernel-parity tests) need to gate themselves on this
    # macro to avoid link errors against missing symbols on portable.
    target_compile_definitions(numkit PUBLIC NUMKIT_WITH_SIMD=1)
endif()

if(NUMKIT_WITH_THREADS)
    # std::thread support. The pool itself lives in core/src/thread_pool.cpp
    # and is only compiled when this option is ON. Without the option, the
    # parallel_for helper falls through to a sequential `fn(0, n)` call
    # (header-only inline) and pthread/Threads is not linked.
    find_package(Threads REQUIRED)
    target_link_libraries(numkit PUBLIC Threads::Threads)
    target_compile_definitions(numkit PUBLIC NUMKIT_WITH_THREADS=1)
endif()

# ──────────────────────────────────────────────
# Example
# ──────────────────────────────────────────────

if(NUMKIT_BUILD_EXAMPLE)
    add_executable(numkit_example example/main.cpp)
    target_link_libraries(numkit_example PRIVATE numkit)

    if(EMSCRIPTEN)
        # Emscripten produces a Node-runnable .js loader + .wasm sidecar
        # when the output ends in .js. NODERAWFS lets argv paths resolve
        # against the real host filesystem:
        #   node build-browser/example/numkit_example.js path/to/script.m
        set_target_properties(numkit_example PROPERTIES SUFFIX ".js")
        target_compile_options(numkit_example PRIVATE -fexceptions)
        target_link_options(numkit_example PRIVATE
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

if(NUMKIT_BUILD_TESTS AND NOT EMSCRIPTEN)
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

    # tests/ creates the numkit_tests target; per-component tests/ dirs
    # contribute their sources via target_sources(numkit_tests PRIVATE ...)
    add_subdirectory(tests)
    add_subdirectory(core/tests)
    add_subdirectory(libs/builtin/tests)
    add_subdirectory(libs/signal/tests)
    add_subdirectory(libs/graphics/tests)
endif()

# ──────────────────────────────────────────────
# Benchmarks (Google Benchmark)
# ──────────────────────────────────────────────

if(NUMKIT_BUILD_BENCHMARKS)
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

if(NUMKIT_BUILD_REPL)
    add_subdirectory(wasm)
endif()

# ──────────────────────────────────────────────
# Install
# ──────────────────────────────────────────────

if(NUMKIT_INSTALL AND NOT EMSCRIPTEN)
    include(GNUInstallDirs)
    include(CMakePackageConfigHelpers)

    install(TARGETS numkit
        EXPORT numkitTargets
        LIBRARY  DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE  DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME  DESTINATION ${CMAKE_INSTALL_BINDIR}
    )

    # Public headers — every component ships its own numkit/<name>/ subtree
    install(DIRECTORY core/include/numkit          DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
    install(DIRECTORY libs/builtin/include/numkit  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
    install(DIRECTORY libs/signal/include/numkit   DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
    install(DIRECTORY libs/stats/include/numkit    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
    install(DIRECTORY libs/graphics/include/numkit DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

    install(EXPORT numkitTargets
        FILE        numkitTargets.cmake
        NAMESPACE   numkit::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/numkit
    )

    configure_package_config_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/numkitConfig.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/numkitConfig.cmake"
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/numkit
    )

    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/numkitConfigVersion.cmake"
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion
    )

    install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/numkitConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/numkitConfigVersion.cmake"
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/numkit
    )
endif()
