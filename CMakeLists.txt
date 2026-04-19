# CMakeLists.txt
cmake_minimum_required(VERSION 3.14)
project(numkit VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(NUMKIT_BUILD_EXAMPLE "Build example executable" ON)
option(NUMKIT_BUILD_TESTS   "Build tests"              ON)
option(NUMKIT_BUILD_SHARED  "Build shared library"     OFF)
option(NUMKIT_INSTALL       "Generate install target"  ON)

if(NOT EMSCRIPTEN)
    set(NUMKIT_BUILD_REPL OFF CACHE BOOL "Build WebAssembly REPL" FORCE)
else()
    option(NUMKIT_BUILD_REPL "Build WebAssembly module" ON)
endif()

# ──────────────────────────────────────────────
# Основная библиотека
# ──────────────────────────────────────────────

set(NUMKIT_CORE_SOURCES
    src/MAllocator.cpp
    src/MMType.cpp
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

set(NUMKIT_SOURCES
    ${NUMKIT_CORE_SOURCES}
    ${STDLIB_SOURCES}
    ${DSPLIB_SOURCES}
    ${PLTLIB_SOURCES}
    ${FITLIB_SOURCES}
)

set(NUMKIT_HEADERS
    include/MTypes.hpp
    include/MTreeWalker.hpp
    include/MAllocator.hpp
    include/MMType.hpp
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

if(NUMKIT_BUILD_SHARED)
    add_library(numkit SHARED ${NUMKIT_SOURCES} ${NUMKIT_HEADERS})
else()
    add_library(numkit STATIC ${NUMKIT_SOURCES} ${NUMKIT_HEADERS})
endif()

add_library(numkit::numkit ALIAS numkit)

target_include_directories(numkit
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/stdlib
        ${CMAKE_CURRENT_SOURCE_DIR}/src/dsplib
)

target_compile_features(numkit PUBLIC cxx_std_17)

if(EMSCRIPTEN)
    target_compile_options(numkit PRIVATE -fexceptions)
    target_link_options(numkit PUBLIC "SHELL:-s DISABLE_EXCEPTION_CATCHING=0")
endif()

# ──────────────────────────────────────────────
# Example
# ──────────────────────────────────────────────

if(NUMKIT_BUILD_EXAMPLE AND NOT EMSCRIPTEN)
    add_executable(numkit_example example/main.cpp)
    target_link_libraries(numkit_example PRIVATE numkit::numkit)
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

    add_subdirectory(tests)
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

    install(FILES ${NUMKIT_HEADERS}
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/numkit
    )

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