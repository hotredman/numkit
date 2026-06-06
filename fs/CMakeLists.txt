# fs/CMakeLists.txt
#
# numkit_fs — the filesystem abstraction (VirtualFS interface + NativeFS +
# CallbackFS). An L0 foundation module: it depends ONLY on the C++ standard
# library — never on Value, the engine, the parser, or any toolbox. Both
# `core` and the pure `libs/*` build on it WITHOUT pulling each other in.
#
# Concrete non-native backends (a DB/IndexedDB-backed temp store, an Electron
# mount) are supplied by the embedder via CallbackFS hooks — they live outside
# this module and connect through the abstract interface (dependency
# inversion), so numkit_fs stays standard-library-only.

add_library(numkit_fs STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/vfs.cpp
)

target_include_directories(numkit_fs PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_compile_features(numkit_fs PUBLIC cxx_std_17)

# Linked into the (possibly SHARED) numkit aggregate → must be PIC.
set_target_properties(numkit_fs PROPERTIES POSITION_INDEPENDENT_CODE ON)

if(EMSCRIPTEN)
    target_compile_options(numkit_fs PRIVATE -fexceptions)
endif()
