# CMakeLists.txt
cmake_minimum_required(VERSION 3.14)
project(matlab_interp VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(MLAB_BUILD_EXAMPLE "Build example executable" ON)
option(MLAB_BUILD_SHARED  "Build shared library"     OFF)
option(MLAB_INSTALL       "Generate install target"  ON)

if(NOT EMSCRIPTEN)
    set(MLAB_BUILD_REPL OFF CACHE BOOL "Build WebAssembly REPL" FORCE)
else()
    option(MLAB_BUILD_REPL "Build WebAssembly REPL" ON)
endif()

set(MLAB_SOURCES
    src/MLabAllocator.cpp
    src/MLabValue.cpp
    src/MLabLexer.cpp
    src/MLabParser.cpp
    src/MLabEnvironment.cpp
    src/MLabEngine.cpp
    src/MLabStdLibrary.cpp
)

set(MLAB_HEADERS
    include/MLabAllocator.hpp
    include/MLabValue.hpp
    include/MLabLexer.hpp
    include/MLabAst.hpp
    include/MLabParser.hpp
    include/MLabEnvironment.hpp
    include/MLabEngine.hpp
    include/MLabStdLibrary.hpp
)

if(MLAB_BUILD_SHARED)
    add_library(mlab SHARED ${MLAB_SOURCES} ${MLAB_HEADERS})
else()
    add_library(mlab STATIC ${MLAB_SOURCES} ${MLAB_HEADERS}
        src/MLabAst.cpp)
endif()

add_library(mlab::mlab ALIAS mlab)

target_include_directories(mlab
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(mlab PUBLIC cxx_std_17)

if(MLAB_BUILD_EXAMPLE AND NOT EMSCRIPTEN)
    add_executable(mlab_example example/main.cpp)
    target_link_libraries(mlab_example PRIVATE mlab::mlab)
endif()

# ──────────────────────────────────────────────
# REPL subdirectory
# ──────────────────────────────────────────────
if(MLAB_BUILD_REPL)
    add_subdirectory(repl)
endif()

if(MLAB_INSTALL AND NOT EMSCRIPTEN)
    include(GNUInstallDirs)
    include(CMakePackageConfigHelpers)

    install(TARGETS mlab
        EXPORT mlabTargets
        LIBRARY  DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE  DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME  DESTINATION ${CMAKE_INSTALL_BINDIR}
    )

    install(FILES ${MLAB_HEADERS}
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/mlab
    )

    install(EXPORT mlabTargets
        FILE        mlabTargets.cmake
        NAMESPACE   mlab::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/mlab
    )

    configure_package_config_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/mlabConfig.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/mlabConfig.cmake"
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/mlab
    )

    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/mlabConfigVersion.cmake"
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion
    )

    install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/mlabConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/mlabConfigVersion.cmake"
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/mlab
    )
endif()
