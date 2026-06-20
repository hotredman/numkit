target_sources(numkit_gtest PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/type_lattice_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/transfer_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/transfer_diff_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/inference_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/indexing_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/emitter_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/aot_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/monomorphize_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/classinfo_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/bridge_test.cpp
    # The Value-ABI bridge runtime (DESIGN.md §6a). Compiled into the test
    # binary for brick ①; packaged as the standalone nk_codegen_rt shared
    # lib for AOT-compiled artifacts in a later brick.
    ${CMAKE_CURRENT_SOURCE_DIR}/../bridge/nk_codegen_rt.cpp
)
target_include_directories(numkit_gtest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../bridge)

# ---- Plugin e2e fixture (DESIGN.md §6b) ------------------------------------
# A standalone sample plugin, built as a shared library and dlopen'd by the
# plugin e2e test (Plugin.LoadSharedLibraryAndCall). It links against nothing
# from the host — only nk_plugin.h. We pin its output dir per-config (no VS
# config subdir) so a configure-time configure_file can hand the test an
# absolute, forward-slashed path with no generator-expression gymnastics.
add_library(nk_sample_plugin SHARED ${CMAKE_CURRENT_SOURCE_DIR}/sample_plugin.cpp)
target_include_directories(nk_sample_plugin PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../bridge)
set_target_properties(nk_sample_plugin PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY         ${CMAKE_CURRENT_BINARY_DIR}
    RUNTIME_OUTPUT_DIRECTORY_DEBUG   ${CMAKE_CURRENT_BINARY_DIR}
    RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_CURRENT_BINARY_DIR}
    LIBRARY_OUTPUT_DIRECTORY         ${CMAKE_CURRENT_BINARY_DIR}
    LIBRARY_OUTPUT_DIRECTORY_DEBUG   ${CMAKE_CURRENT_BINARY_DIR}
    LIBRARY_OUTPUT_DIRECTORY_RELEASE ${CMAKE_CURRENT_BINARY_DIR})

set(NK_SAMPLE_PLUGIN_FILE
    "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_SHARED_LIBRARY_PREFIX}nk_sample_plugin${CMAKE_SHARED_LIBRARY_SUFFIX}")
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/nk_sample_plugin_path.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/nk_sample_plugin_path.h @ONLY)
target_include_directories(numkit_gtest PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

# Ensure the plugin .dll exists before the test binary runs.
add_dependencies(numkit_gtest nk_sample_plugin)
