# CMakeLists.txt
cmake_minimum_required(VERSION 3.21)
project(m VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(NUMKIT_M_BUILD_EXAMPLE "Build example executable" ON)
option(NUMKIT_M_BUILD_TESTS   "Build tests"              ON)
option(NUMKIT_M_BUILD_SHARED  "Build shared library"     OFF)
option(NUMKIT_M_INSTALL       "Generate install target"  ON)

# Feature flags (NUMKIT_WITH_SIMD)
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
include(NumkitOptions)

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
    core/include/numkit/m/core/MSpan.hpp
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

# ──────────────────────────────────────────────
# Example
# ──────────────────────────────────────────────

if(NUMKIT_M_BUILD_EXAMPLE AND NOT EMSCRIPTEN)
    add_executable(m_example example/main.cpp)
    target_link_libraries(m_example PRIVATE numkit::m)
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
