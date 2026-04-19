# CMakeLists.txt
cmake_minimum_required(VERSION 3.14)
project(m VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(NUMKIT_M_BUILD_EXAMPLE "Build example executable" ON)
option(NUMKIT_M_BUILD_TESTS   "Build tests"              ON)
option(NUMKIT_M_BUILD_SHARED  "Build shared library"     OFF)
option(NUMKIT_M_INSTALL       "Generate install target"  ON)

if(NOT EMSCRIPTEN)
    set(NUMKIT_M_BUILD_REPL OFF CACHE BOOL "Build WebAssembly REPL" FORCE)
else()
    option(NUMKIT_M_BUILD_REPL "Build WebAssembly module" ON)
endif()

# ──────────────────────────────────────────────
# Основная библиотека
# ──────────────────────────────────────────────

set(NUMKIT_M_CORE_SOURCES
    src/MAllocator.cpp
    src/MType.cpp
    src/MDims.cpp
    src/MDataBuffer.cpp
    src/MHeapObject.cpp
    src/MValue.cpp
    src/MLexer.cpp
    src/MParser.cpp
    src/MEnvironment.cpp
    src/MEngine.cpp
    src/MAst.cpp
    src/MTreeWalker.cpp
    src/MCompiler.cpp
    src/MVM.cpp
    src/MDebugger.cpp
    src/MDebugSession.cpp
    src/MDebugWorkspace.cpp
    src/MVfs.cpp
)

# Each subdirectory defines its own source list
add_subdirectory(src/stdlib)
add_subdirectory(src/dsplib)
add_subdirectory(src/pltlib)
add_subdirectory(src/fitlib)

set(NUMKIT_M_SOURCES
    ${NUMKIT_M_CORE_SOURCES}
    ${STDLIB_SOURCES}
    ${DSPLIB_SOURCES}
    ${PLTLIB_SOURCES}
    ${FITLIB_SOURCES}
)

set(NUMKIT_M_HEADERS
    include/MTypes.hpp
    include/MTreeWalker.hpp
    include/MAllocator.hpp
    include/MType.hpp
    include/MDims.hpp
    include/MDataBuffer.hpp
    include/MHeapObject.hpp
    include/MValue.hpp
    include/MLexer.hpp
    include/MAst.hpp
    include/MParser.hpp
    include/MEnvironment.hpp
    include/MEngine.hpp
    include/MStdLibrary.hpp
    include/MDspLibrary.hpp
    include/MPltLibrary.hpp
    include/MFitLibrary.hpp
    include/MFigureManager.hpp
    include/MSpan.hpp
    include/MBytecode.hpp
    include/MCompiler.hpp
    include/MVM.hpp
    include/MDebugger.hpp
)

if(NUMKIT_M_BUILD_SHARED)
    add_library(m SHARED ${NUMKIT_M_SOURCES} ${NUMKIT_M_HEADERS})
else()
    add_library(m STATIC ${NUMKIT_M_SOURCES} ${NUMKIT_M_HEADERS})
endif()

add_library(numkit::m ALIAS m)

target_include_directories(m
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/stdlib
        ${CMAKE_CURRENT_SOURCE_DIR}/src/dsplib
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

    add_subdirectory(tests)
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

    install(FILES ${NUMKIT_M_HEADERS}
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/numkit
    )

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
