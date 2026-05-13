# CMakeLists.txt
cmake_minimum_required(VERSION 3.21)
project(numkit VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# MSVC: file-level parallel compilation inside one vcxproj. MSBuild's /m
# only parallelises across vcxproj — without /MP, cl.exe processes the
# .cpp files of one project sequentially, even with `cmake --parallel`.
# Combined with `"jobs": 0` in CMakePresets.json this gives both project-
# level (MSBuild /m) and file-level (cl /MP) parallelism.
if(MSVC)
    add_compile_options(/MP)
endif()

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
# matio (MATLAB .mat file I/O) — gated by NUMKIT_WITH_MATIO
# ──────────────────────────────────────────────
#
# Used by libs/io/src/workspace/saveload_mat.cpp to back save/load on
# .mat files (v4 / v5 / v7). HDF5 backend (v7.3) is intentionally
# disabled — adds a heavy dependency we don't need yet. ZLIB compression
# (for `-v7`) is also off for the same reason; matio silently falls back
# to uncompressed v5 in that case, so `-v7` still produces a valid file
# but isn't actually smaller until we flip MATIO_WITH_ZLIB and pull
# zlib via FetchContent (planned).
#
# Target: `matio` (static, PIC). Linked PRIVATE to numkit so matio.h
# never leaks into our public headers (only the matio-backed cpp under
# libs/io/src/ includes it).
if(NUMKIT_WITH_MATIO)
    include(FetchContent)

    set(MATIO_SHARED        OFF CACHE BOOL "" FORCE)
    set(MATIO_WITH_HDF5     OFF CACHE BOOL "" FORCE)
    set(MATIO_WITH_ZLIB     OFF CACHE BOOL "" FORCE)
    set(MATIO_MAT73         OFF CACHE BOOL "" FORCE)
    set(MATIO_BUILD_TESTING OFF CACHE BOOL "" FORCE)
    set(MATIO_USE_CONAN     OFF CACHE BOOL "" FORCE)
    set(MATIO_EXTENDED_SPARSE OFF CACHE BOOL "" FORCE)

    FetchContent_Declare(
        matio
        GIT_REPOSITORY https://github.com/tbeu/matio.git
        GIT_TAG        v1.5.30
    )
    FetchContent_MakeAvailable(matio)
endif()

# ──────────────────────────────────────────────
# Sources — assembled from per-area CMakeLists
# ──────────────────────────────────────────────
#
# core/CMakeLists.txt   exports NUMKIT_CORE_SOURCES / NUMKIT_CORE_HEADERS
#                              / NUMKIT_CORE_INCLUDE_DIRS
# libs/CMakeLists.txt   exports NUMKIT_LIBS_SOURCES / NUMKIT_LIBS_HEADERS
#                              / NUMKIT_LIBS_INCLUDE_DIRS
# Each lib has its own libs/<name>/CMakeLists.txt for per-lib worker
# ownership (see COORDINATION.md).

add_subdirectory(core)
add_subdirectory(libs)

set(NUMKIT_SOURCES ${NUMKIT_CORE_SOURCES} ${NUMKIT_LIBS_SOURCES})
set(NUMKIT_HEADERS ${NUMKIT_CORE_HEADERS} ${NUMKIT_LIBS_HEADERS})

if(NUMKIT_BUILD_SHARED)
    add_library(numkit SHARED ${NUMKIT_SOURCES} ${NUMKIT_HEADERS})
else()
    add_library(numkit STATIC ${NUMKIT_SOURCES} ${NUMKIT_HEADERS})
endif()

# Match install-tree NAMESPACE so add_subdirectory consumers can use the
# same `numkit::numkit` target as find_package() consumers do.
add_library(numkit::numkit ALIAS numkit)

# Public include paths — collected from per-area CMakeLists. Wrap each
# in BUILD_INTERFACE so the export set stays clean for find_package.
foreach(_dir ${NUMKIT_CORE_INCLUDE_DIRS} ${NUMKIT_LIBS_INCLUDE_DIRS})
    target_include_directories(numkit PUBLIC $<BUILD_INTERFACE:${_dir}>)
endforeach()
target_include_directories(numkit PUBLIC $<INSTALL_INTERFACE:include>)

target_include_directories(numkit
    PRIVATE
        # Shared library-internal helpers (helpers.hpp under libs/*/src/)
        # AND the resolution root for every Highway HWY_TARGET_INCLUDE string
        # (e.g. "transforms/backends/fft_r2_simd.cpp", relative to libs/<lib>/src/).
        # TODO: formalize the helpers.hpp leak as public "detail/" headers
        #       or a common utility lib.
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/builtin/src
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/signal/src
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/stats/src
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/io/src
        # Vendored single-header libs (stb_image, stb_image_write).
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/stb
)

target_compile_features(numkit PUBLIC cxx_std_17)

if(EMSCRIPTEN)
    target_compile_options(numkit PRIVATE -fexceptions)
    target_link_options(numkit PUBLIC "SHELL:-s DISABLE_EXCEPTION_CATCHING=0")
endif()

# matio is linked when NUMKIT_WITH_MATIO is ON — libs/io's .mat backend
# (saveload_mat.cpp) pulls in matio.h directly. PRIVATE: matio's header
# never appears in any of our public include/ trees. BUILD_INTERFACE
# genex keeps the link dep out of the exported target set — matio isn't
# exported via install(EXPORT) on our side. The compile-definition lets
# saveload.cpp #ifdef-out the binary-.mat dispatch when matio is absent.
if(NUMKIT_WITH_MATIO)
    target_link_libraries(numkit PRIVATE $<BUILD_INTERFACE:matio>)
    target_compile_definitions(numkit PRIVATE NUMKIT_WITH_MATIO)
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

# Smoke binary (numkit_smoke) is now owned by tests/smoke/CMakeLists.txt;
# add_subdirectory(tests) below pulls it in via tests/CMakeLists.txt.

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

    # tests/ owns numkit_gtest (in tests/gtest/) and numkit_smoke (in
    # tests/smoke/). Per-component tests/ dirs contribute their gtest
    # sources via target_sources(numkit_gtest PRIVATE ...).
    add_subdirectory(tests)
    add_subdirectory(core/tests)
    foreach(_test_lib builtin signal stats graphics io wavelet comm control image audio)
        if(EXISTS ${CMAKE_SOURCE_DIR}/libs/${_test_lib}/tests/CMakeLists.txt)
            add_subdirectory(libs/${_test_lib}/tests)
        endif()
    endforeach()
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

    # Public headers — install each per-area include-dir's numkit/ subtree
    foreach(_dir ${NUMKIT_CORE_INCLUDE_DIRS} ${NUMKIT_LIBS_INCLUDE_DIRS})
        install(DIRECTORY ${_dir}/numkit DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
    endforeach()

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

# ── API documentation (Doxygen) ──────────────────────────────────────
# Adds `docs` / `docs-pdf` / `docs-clean` targets when Doxygen is on
# PATH. PDF target additionally requires pdflatex (MiKTeX / TeX Live).
# Safe no-op if Doxygen is not installed.
include(cmake/Doxygen.cmake)
