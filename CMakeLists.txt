# CMakeLists.txt
cmake_minimum_required(VERSION 3.21)
project(nk VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# MSVC: file-level parallel compilation inside one vcxproj. MSBuild's /m
# only parallelises across vcxproj — without /MP, cl.exe processes the
# .cpp files of one project sequentially, even with `cmake --parallel`.
# Combined with `"jobs": 0` in CMakePresets.json this gives both project-
# level (MSBuild /m) and file-level (cl /MP) parallelism.
# Skip clang-cl (MSVC is true for it too): it ignores /MP and, under -Werror,
# treats the unused flag as a fatal -Wunused-command-line-argument. clang-cl
# builds (bench-clang, coverage) parallelise via the build tool's -j instead.
if(MSVC AND NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_options(/MP)
endif()

option(NK_BUILD_CLI        "Build the nk interpreter CLI"        ON)
option(NK_BUILD_TESTS      "Build tests"                           ON)
option(NK_BUILD_BENCHMARKS "Build Google Benchmark micro-benches"  OFF)
option(NK_BUILD_SHARED     "Build shared library"                  OFF)
option(NK_INSTALL          "Generate install target"               ON)

# Feature flags (NK_WITH_SIMD)
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
include(NKOptions)

# ──────────────────────────────────────────────
# Google Highway (SIMD-intrinsics abstraction)
# ──────────────────────────────────────────────
#
# Pulled only when NK_WITH_SIMD=ON. Dispatch strategy is dynamic —
# the library ships with backends for every ISA Highway supports and
# picks one at first call (see HWY_DYNAMIC_DISPATCH in the _simd.cpp
# files under toolboxes/*/src/backends/).

if(NK_WITH_SIMD)
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
    set(NK_BUILD_REPL OFF CACHE BOOL "Build WebAssembly REPL" FORCE)
else()
    option(NK_BUILD_REPL "Build WebAssembly module" ON)
endif()

# ──────────────────────────────────────────────
# Sources — assembled from per-area CMakeLists
# ──────────────────────────────────────────────
#
# core/CMakeLists.txt   exports NK_CORE_SOURCES / NK_CORE_HEADERS
#                              / NK_CORE_INCLUDE_DIRS
# toolboxes/CMakeLists.txt   exports NK_LIBS_SOURCES / NK_LIBS_HEADERS
#                              / NK_LIBS_INCLUDE_DIRS
# Each lib has its own toolboxes/<name>/CMakeLists.txt for per-lib worker
# ownership (see dev-docs/handbook/coordination.md).

# ── Coverage (NK_COVERAGE) — arch-review #6 ─────────────────────
# Source-based coverage via llvm-cov (clang / clang-cl only). Declared HERE,
# after the FetchContent third-party targets (highway/matio/zlib, created
# above) but before the nk add_subdirectory()s below — so add_compile_options
# instruments only our own TUs, leaving third_party uninstrumented (less noise,
# no risk of a third_party build break). The link flag pulls clang_rt.profile
# into every executable created afterwards (nk_gtest). Use the `coverage`
# preset (Ninja + clang-cl); run scripts/coverage.ps1 to emit a report.
option(NK_COVERAGE "Instrument nk for llvm-cov source coverage (clang/clang-cl only)" OFF)
if(NK_COVERAGE)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        message(FATAL_ERROR "NK_COVERAGE requires a Clang/clang-cl toolchain — use the 'coverage' preset.")
    endif()
    add_compile_options(-fprofile-instr-generate -fcoverage-mapping)
    add_link_options(-fprofile-instr-generate)
    message(STATUS "nk: source-coverage instrumentation ON (llvm-cov)")
endif()

add_subdirectory(src/value)   # L0:   Value substrate (nk_value) — STL-only
add_subdirectory(src/fs)      # L0:   filesystem abstraction (nk_fs) — STL-only
add_subdirectory(src/ops)     # L0.5: numeric primitives + threading (nk_ops) → value (+Highway)
add_subdirectory(src/figure)  # L0.5 header-only: figure/plot session state (FigureManager) — below core, used by core+graphics
add_subdirectory(src/core)
add_subdirectory(src/builtin)  # L2: MATLAB built-in standard library (elmat, elfun, matfun, datafun, ...)
add_subdirectory(src/graphics)   # L2 service: plotting (figure/plot/…) — depends on no toolbox; toolboxes MAY depend on it
add_subdirectory(src/scriptgraph) # L2 IDE-analysis: AST→NodeGraph→JSON (buildScriptGraph) — core-coupled (parser), registers no builtin
add_subdirectory(src/codegen)    # L2 codegen: AST→typed IR→C++ transpiler (type inference) — core-coupled, registers no builtin
add_subdirectory(src/toolboxes)  # L2: MATLAB toolboxes (pure compute + file I/O via fs/)
add_subdirectory(src/runtime)  # L2: language runtime extracted from builtin (eval-family; who/clear/import to follow)
add_subdirectory(src/bundle)  # L3: installStandardLibrary / makeStandardEngine / StandardEngine

# Fail loudly if any aggregated layer include-dir variable came back empty — the owning
# layer's CMakeLists forgot its `set(NK_<L>_INCLUDE_DIRS <dir> PARENT_SCOPE)` export.
# These are consumed unconditionally by the include-dir foreach below; a silent empty
# would drop that layer's headers from every target's include path and surface only as a
# confusing downstream "file not found" (architecture review §5 — CMake aggregation).
foreach(_inc_var NK_CORE_INCLUDE_DIRS NK_FIGURE_INCLUDE_DIRS NK_LIBS_INCLUDE_DIRS
                 NK_RUNTIME_INCLUDE_DIRS NK_BUILTIN_INCLUDE_DIRS NK_BUNDLE_INCLUDE_DIRS)
    if(NOT ${_inc_var})
        message(FATAL_ERROR
            "Layer include-dir variable ${_inc_var} is empty — the owning layer's "
            "CMakeLists likely missed its `set(${_inc_var} <dir> PARENT_SCOPE)` export.")
    endif()
endforeach()

# G1 (target-split): every layer compiles as its own OBJECT library (nk_<L>_obj,
# defined at the end of this file once the dependency targets exist) carrying the
# monolith compile config via nk_configure_layer(). The final `nk` aggregates
# them through $<TARGET_OBJECTS> — same object set as the old monolith, grouped.
set(NK_SOURCES )
set(NK_LAYER_OBJECTS
    $<TARGET_OBJECTS:nk_core_obj>
    $<TARGET_OBJECTS:nk_toolboxes_obj>
    $<TARGET_OBJECTS:nk_runtime_obj>
    $<TARGET_OBJECTS:nk_builtin_obj>
    $<TARGET_OBJECTS:nk_bundle_obj>)
set(NK_HEADERS ${NK_CORE_HEADERS} ${NK_LIBS_HEADERS} ${NK_BUILTIN_HEADERS} ${NK_RUNTIME_HEADERS})

if(NK_BUILD_SHARED)
    add_library(nk SHARED ${NK_SOURCES} ${NK_HEADERS} ${NK_LAYER_OBJECTS})
else()
    add_library(nk STATIC ${NK_SOURCES} ${NK_HEADERS} ${NK_LAYER_OBJECTS})
endif()

# Match install-tree NAMESPACE so add_subdirectory consumers can use the
# same `nk::nk` target as find_package() consumers do.
add_library(nk::nk ALIAS nk)

# L0 foundation modules (Value substrate + filesystem abstraction). The
# BUILD_INTERFACE genex keeps them out of the exported target set (mirrors the
# matio / hwy pattern) — their PUBLIC include dirs + objects propagate to
# nk (and to build-tree consumers like the tests) at build time, which is
# all we need.
target_link_libraries(nk PUBLIC
    $<BUILD_INTERFACE:nk_value>
    $<BUILD_INTERFACE:nk_ops>
    $<BUILD_INTERFACE:nk_fs>)

# Product executables (apps/): the `nk` interpreter CLI and the
# `nk_codegen` AOT transpiler CLI (codegen driver, DESIGN.md §8 M4). Both
# are thin wrappers over the `nk` lib, so built after it is defined.
# nk_codegen is skipped under Emscripten (no native C++ compiler in a WASM
# build); the interpreter still builds as a Node-runnable .js. See apps/CMakeLists.txt.
add_subdirectory(apps)

# ── Architectural layering guard ────────────────────────────────────
#
# Fails the build if value/ fs/ ops/ core/ gain a forbidden *upward*
# dependency (e.g. core including a toolbox, ops including core). Runs on every
# build so the three parallel sessions can't silently re-couple the layers.
# Skipped when Python is unavailable so it never blocks a build it can't
# perform — it is a guard, not a hard build dependency.
if(NK_BUILD_TESTS)
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_FOUND)
        add_custom_target(nk_layering_check ALL
            COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_CURRENT_SOURCE_DIR}/tools/check_layering.py
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            COMMENT "Checking architectural layering (value/fs/ops/core)"
            VERBATIM)
    endif()
endif()

# ── Build-info header (regenerated on every build) ──────────────────
#
# Writes <build>/generated/nk/core/build_info.hpp containing a
# NK_BUILD_TIMESTAMP macro with current wall-clock time. The
# `nk_build_info` custom target is part of ALL so it runs on
# every build; bundle/src/version_string.cpp depends on the
# generated header so only that tiny TU recompiles per build, not
# the huge library.cpp.
set(NK_BUILD_INFO_DIR    "${CMAKE_BINARY_DIR}/generated")
set(NK_BUILD_INFO_HEADER "${NK_BUILD_INFO_DIR}/nk/core/build_info.hpp")
# Generate once at CONFIGURE time too — the custom target below
# refreshes it on every build, but the first-ever build needs the
# header to exist when version_string.cpp compiles.
execute_process(COMMAND ${CMAKE_COMMAND}
    "-DOUT=${NK_BUILD_INFO_HEADER}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/gen_build_info.cmake")
add_custom_target(nk_build_info ALL
    COMMAND ${CMAKE_COMMAND}
        "-DOUT=${NK_BUILD_INFO_HEADER}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/gen_build_info.cmake"
    BYPRODUCTS "${NK_BUILD_INFO_HEADER}"
    COMMENT    "Generating nk build timestamp"
    VERBATIM
)
add_dependencies(nk nk_build_info)
target_include_directories(nk PRIVATE "${NK_BUILD_INFO_DIR}")
set_source_files_properties(
    "${CMAKE_CURRENT_SOURCE_DIR}/src/bundle/src/version_string.cpp"
    PROPERTIES OBJECT_DEPENDS "${NK_BUILD_INFO_HEADER}"
)

# Public include paths — collected from per-area CMakeLists. Wrap each
# in BUILD_INTERFACE so the export set stays clean for find_package.
foreach(_dir ${NK_CORE_INCLUDE_DIRS} ${NK_FIGURE_INCLUDE_DIRS} ${NK_LIBS_INCLUDE_DIRS} ${NK_MATH_INCLUDE_DIRS} ${NK_LANG_INCLUDE_DIRS} ${NK_RUNTIME_INCLUDE_DIRS} ${NK_BUILTIN_INCLUDE_DIRS} ${NK_BUNDLE_INCLUDE_DIRS})
    target_include_directories(nk PUBLIC $<BUILD_INTERFACE:${_dir}>)
endforeach()
target_include_directories(nk PUBLIC $<INSTALL_INTERFACE:include>)

target_include_directories(nk
    PRIVATE
        # Shared library-internal helpers (helpers.hpp under toolboxes/*/src/)
        # AND the resolution root for every Highway HWY_TARGET_INCLUDE string
        # (e.g. "transforms/backends/fft_r2_simd.cpp", relative to toolboxes/<lib>/src/).
        # TODO: formalize the helpers.hpp leak as public "detail/" headers
        #       or a common utility lib.
        # math/ relocated from builtin (Phase 3-A step C) — same role: shared
        # math helpers + the Highway HWY_TARGET_INCLUDE resolution root for the
        # relocated SIMD backends under math/src/<area>/.
        ${CMAKE_CURRENT_SOURCE_DIR}/src/math/src
        # lang/ relocated from builtin (Phase 3-A step C2) — same role: Highway
        # HWY_TARGET_INCLUDE resolution root for the relocated casts SIMD backend
        # under lang/src/types/.
        ${CMAKE_CURRENT_SOURCE_DIR}/src/lang/src
        ${CMAKE_CURRENT_SOURCE_DIR}/src/toolboxes/signal/src
        ${CMAKE_CURRENT_SOURCE_DIR}/src/toolboxes/stats/src
        ${CMAKE_CURRENT_SOURCE_DIR}/src/toolboxes/io/src
        # C5: register adapters relocated to bundle/src/register/<T>/ keep their
        # area-own ("<subarea>/X_detail.hpp") includes resolvable via the toolbox
        # src root on the -I path.
        ${CMAKE_CURRENT_SOURCE_DIR}/src/toolboxes/image/src
        ${CMAKE_CURRENT_SOURCE_DIR}/src/toolboxes/comm/src
        ${CMAKE_CURRENT_SOURCE_DIR}/src/toolboxes/control/src
        ${CMAKE_CURRENT_SOURCE_DIR}/src/toolboxes/audio/src
        ${CMAKE_CURRENT_SOURCE_DIR}/src/toolboxes/wavelet/src
        ${CMAKE_CURRENT_SOURCE_DIR}/src/toolboxes/linalg/src
)

target_compile_features(nk PUBLIC cxx_std_17)

if(EMSCRIPTEN)
    target_compile_options(nk PRIVATE -fexceptions)
    target_link_options(nk PUBLIC "SHELL:-s DISABLE_EXCEPTION_CATCHING=0")
endif()



if(NK_WITH_SIMD)
    # PRIVATE — our public headers never mention Highway types; it's a pure
    # implementation detail confined to toolboxes/*/src/backends/*_simd.cpp.
    # BUILD_INTERFACE genex keeps the link dep out of the exported target
    # set (install(EXPORT) would otherwise complain that hwy / hwy_contrib
    # aren't themselves exported).
    target_link_libraries(nk PRIVATE
        $<BUILD_INTERFACE:hwy>
        $<BUILD_INTERFACE:hwy_contrib>)
    # PUBLIC — backends/FftKernels.hpp declares SoA / Stockham dispatchers
    # whose definitions are only built when SIMD is on. Direct callers
    # (benchmarks, kernel-parity tests) need to gate themselves on this
    # macro to avoid link errors against missing symbols on portable.
    target_compile_definitions(nk PUBLIC NK_WITH_SIMD=1)
endif()

if(NK_WITH_THREADS)
    # std::thread support. The pool itself lives in core/src/thread_pool.cpp
    # and is only compiled when this option is ON. Without the option, the
    # parallel_for helper falls through to a sequential `fn(0, n)` call
    # (header-only inline) and pthread/Threads is not linked.
    find_package(Threads REQUIRED)
    target_link_libraries(nk PUBLIC Threads::Threads)
    target_compile_definitions(nk PUBLIC NK_WITH_THREADS=1)
endif()

# ── G1 (target-split): per-layer OBJECT libraries ───────────────────
#
# Each compute/engine layer compiles into its own OBJECT library that the final
# `nk` aggregates via $<TARGET_OBJECTS:...>. The final binary is identical to
# the old monolith (same object set, grouped). nk_configure_layer() applies
# the exact compile config the monolith used (include roots + SIMD/threads/matio
# definitions + usage-requirement links for include dirs + the WASM -fexceptions).
# Defined here so the dependency targets (hwy/matio/Threads) already exist.
function(nk_configure_layer tgt)
    target_compile_features(${tgt} PRIVATE cxx_std_17)
    target_include_directories(${tgt} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/math/src
        ${CMAKE_CURRENT_SOURCE_DIR}/src/lang/src
        ${CMAKE_CURRENT_SOURCE_DIR}/src/toolboxes/signal/src
        ${CMAKE_CURRENT_SOURCE_DIR}/src/toolboxes/stats/src
        ${CMAKE_CURRENT_SOURCE_DIR}/src/toolboxes/io/src
        ${CMAKE_CURRENT_SOURCE_DIR}/src/toolboxes/image/src
        ${CMAKE_CURRENT_SOURCE_DIR}/src/toolboxes/comm/src
        ${CMAKE_CURRENT_SOURCE_DIR}/src/toolboxes/control/src
        ${CMAKE_CURRENT_SOURCE_DIR}/src/toolboxes/audio/src
        ${CMAKE_CURRENT_SOURCE_DIR}/src/toolboxes/wavelet/src
        ${CMAKE_CURRENT_SOURCE_DIR}/src/toolboxes/linalg/src
        ${NK_BUILD_INFO_DIR})
    foreach(_dir ${NK_CORE_INCLUDE_DIRS} ${NK_FIGURE_INCLUDE_DIRS} ${NK_LIBS_INCLUDE_DIRS} ${NK_MATH_INCLUDE_DIRS} ${NK_LANG_INCLUDE_DIRS} ${NK_RUNTIME_INCLUDE_DIRS} ${NK_BUILTIN_INCLUDE_DIRS} ${NK_BUNDLE_INCLUDE_DIRS})
        target_include_directories(${tgt} PRIVATE $<BUILD_INTERFACE:${_dir}>)
    endforeach()
    target_link_libraries(${tgt} PRIVATE
        $<BUILD_INTERFACE:nk_value> $<BUILD_INTERFACE:nk_ops> $<BUILD_INTERFACE:nk_fs>)
    if(NK_WITH_SIMD)
        target_link_libraries(${tgt} PRIVATE $<BUILD_INTERFACE:hwy> $<BUILD_INTERFACE:hwy_contrib>)
        target_compile_definitions(${tgt} PRIVATE NK_WITH_SIMD=1)
    endif()
    if(NK_WITH_THREADS)
        find_package(Threads REQUIRED)
        target_link_libraries(${tgt} PRIVATE Threads::Threads)
        target_compile_definitions(${tgt} PRIVATE NK_WITH_THREADS=1)
    endif()
    if(EMSCRIPTEN)
        target_compile_options(${tgt} PRIVATE -fexceptions)
    endif()
endfunction()

add_library(nk_core_obj      OBJECT ${NK_CORE_SOURCES})
add_library(nk_toolboxes_obj OBJECT ${NK_LIBS_SOURCES})
add_library(nk_runtime_obj   OBJECT ${NK_RUNTIME_SOURCES})
add_library(nk_builtin_obj   OBJECT ${NK_BUILTIN_SOURCES})
add_library(nk_bundle_obj    OBJECT ${NK_BUNDLE_SOURCES})
foreach(_obj nk_core_obj nk_toolboxes_obj nk_runtime_obj nk_builtin_obj nk_bundle_obj)
    nk_configure_layer(${_obj})
endforeach()

# The `nk` interpreter CLI is owned by apps/nk/CMakeLists.txt (pulled
# in via add_subdirectory(apps) above); tests/ owns only nk_gtest now.

# ──────────────────────────────────────────────
# Тесты (Google Test)
# ──────────────────────────────────────────────

if(NK_BUILD_TESTS AND NOT EMSCRIPTEN)
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

    # tests/ owns nk_gtest (in tests/gtest/). Per-component tests/ dirs
    # contribute their gtest sources via target_sources(nk_gtest PRIVATE ...).
    add_subdirectory(tests)
    add_subdirectory(src/core/tests)
    add_subdirectory(src/runtime/tests)
    add_subdirectory(src/builtin/tests)
    add_subdirectory(src/bundle/tests)
    if(EXISTS ${CMAKE_SOURCE_DIR}/src/graphics/tests/CMakeLists.txt)
        add_subdirectory(src/graphics/tests)   # graphics is a top-level service lib now
    endif()
    if(EXISTS ${CMAKE_SOURCE_DIR}/src/scriptgraph/tests/CMakeLists.txt)
        add_subdirectory(src/scriptgraph/tests)   # scriptgraph is a top-level analysis lib now
    endif()
    if(EXISTS ${CMAKE_SOURCE_DIR}/src/codegen/tests/CMakeLists.txt)
        add_subdirectory(src/codegen/tests)   # codegen is a top-level analysis lib too
    endif()
    foreach(_test_lib linalg signal stats wavelet comm control image audio optim ode io)
        if(EXISTS ${CMAKE_SOURCE_DIR}/src/toolboxes/${_test_lib}/tests/CMakeLists.txt)
            add_subdirectory(src/toolboxes/${_test_lib}/tests)
        endif()
    endforeach()
endif()

# ──────────────────────────────────────────────
# Benchmarks (Google Benchmark)
# ──────────────────────────────────────────────

if(NK_BUILD_BENCHMARKS)
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

if(NK_BUILD_REPL)
    add_subdirectory(wasm)
endif()

# ──────────────────────────────────────────────
# Install
# ──────────────────────────────────────────────

if(NK_INSTALL AND NOT EMSCRIPTEN)
    include(GNUInstallDirs)
    include(CMakePackageConfigHelpers)

    install(TARGETS nk
        EXPORT nkTargets
        LIBRARY  DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE  DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME  DESTINATION ${CMAKE_INSTALL_BINDIR}
    )

    # Public headers — install each per-area include-dir's nk/ subtree
    foreach(_dir ${NK_CORE_INCLUDE_DIRS} ${NK_FIGURE_INCLUDE_DIRS} ${NK_LIBS_INCLUDE_DIRS} ${NK_MATH_INCLUDE_DIRS} ${NK_LANG_INCLUDE_DIRS} ${NK_RUNTIME_INCLUDE_DIRS})
        install(DIRECTORY ${_dir}/nk DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
    endforeach()

    install(EXPORT nkTargets
        FILE        nkTargets.cmake
        NAMESPACE   nk::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/nk
    )

    configure_package_config_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/nkConfig.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/nkConfig.cmake"
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/nk
    )

    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/nkConfigVersion.cmake"
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion
    )

    install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/nkConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/nkConfigVersion.cmake"
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/nk
    )
endif()

# ── API documentation (Doxygen) ──────────────────────────────────────
# Adds `docs` / `docs-pdf` / `docs-clean` targets when Doxygen is on
# PATH. PDF target additionally requires pdflatex (MiKTeX / TeX Live).
# Safe no-op if Doxygen is not installed.
include(cmake/Doxygen.cmake)

# ── API lint ─────────────────────────────────────────────────────────
# `lint` target — checks toolboxes/* public headers against the mechanically
# checkable subset of dev-docs/handbook/library_api.md (no Engine in signatures, mr
# by pointer, no const Value*). Not wired into the default build; run
# explicitly or from CI. Safe no-op if Python 3 is not found.
find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_FOUND)
    add_custom_target(lint
        COMMAND ${Python3_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/tools/lint/check_api.py
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Linting toolboxes/* public API headers (dev-docs/handbook/library_api.md)"
        VERBATIM)
endif()
