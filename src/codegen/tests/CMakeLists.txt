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
    ${CMAKE_CURRENT_SOURCE_DIR}/driver_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/bridge_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tiered_test.cpp
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
# Bridge header dir, so the CodegenTiered test's generated plugin TU can
# #include "<dir>/nk_plugin.h" with no -I flag.
get_filename_component(NK_BRIDGE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../bridge" ABSOLUTE)

# ---- nk_codegen_rt as a standalone shared library (DESIGN.md §6a brick 3) ---
# The Value-ABI bridge runtime, packaged as a DLL that EMBEDS the numkit
# engine and EXPORTS only the opaque C-ABI (dllexport via NK_RT_BUILDING_DLL).
# A bridged AOT artifact links THIS thin, stable C-ABI instead of the whole
# numkit graph; CMake resolves the numkit deps for us.
add_library(nk_codegen_rt_shared SHARED ${CMAKE_CURRENT_SOURCE_DIR}/../bridge/nk_codegen_rt.cpp)
target_include_directories(nk_codegen_rt_shared PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../bridge)
target_compile_definitions(nk_codegen_rt_shared PRIVATE NK_RT_BUILDING_DLL)
target_link_libraries(nk_codegen_rt_shared PRIVATE numkit)
set_target_properties(nk_codegen_rt_shared PROPERTIES
    OUTPUT_NAME                      nk_codegen_rt
    RUNTIME_OUTPUT_DIRECTORY         ${CMAKE_CURRENT_BINARY_DIR}
    RUNTIME_OUTPUT_DIRECTORY_DEBUG   ${CMAKE_CURRENT_BINARY_DIR}
    RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_CURRENT_BINARY_DIR}
    ARCHIVE_OUTPUT_DIRECTORY         ${CMAKE_CURRENT_BINARY_DIR}
    ARCHIVE_OUTPUT_DIRECTORY_DEBUG   ${CMAKE_CURRENT_BINARY_DIR}
    ARCHIVE_OUTPUT_DIRECTORY_RELEASE ${CMAKE_CURRENT_BINARY_DIR})
# Import library (to link) + the runtime DLL (the e2e copies it beside the
# generated artifact so it loads). Paths are deterministic from the pinned
# output dir, so configure_file can bake them in.
set(NK_RT_IMPORT_LIB
    "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_IMPORT_LIBRARY_PREFIX}nk_codegen_rt${CMAKE_IMPORT_LIBRARY_SUFFIX}")
set(NK_RT_SHARED_DLL
    "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_SHARED_LIBRARY_PREFIX}nk_codegen_rt${CMAKE_SHARED_LIBRARY_SUFFIX}")

# ---- nk_ops_kernels: the public ops kernel facade as a shared lib -----------
# Same pattern as nk_codegen_rt, for the OTHER lowering tier: a codegen artifact
# that uses an ops kernel (matmul, …) links THIS one import lib + copies the DLL
# beside it, instead of statically enumerating numkit_ops + its transitive deps
# (numkit_value, Highway). The DLL compiles the facade (kernels.cpp) with
# dllexport (NK_OPS_BUILDING_DLL) and embeds numkit_ops; CMake resolves the deps.
add_library(nk_ops_kernels SHARED ${CMAKE_CURRENT_SOURCE_DIR}/../../ops/src/kernels.cpp)
target_include_directories(nk_ops_kernels PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../ops/include)
target_compile_definitions(nk_ops_kernels PRIVATE NK_OPS_BUILDING_DLL)
target_link_libraries(nk_ops_kernels PRIVATE numkit_ops)
set_target_properties(nk_ops_kernels PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY         ${CMAKE_CURRENT_BINARY_DIR}
    RUNTIME_OUTPUT_DIRECTORY_DEBUG   ${CMAKE_CURRENT_BINARY_DIR}
    RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_CURRENT_BINARY_DIR}
    ARCHIVE_OUTPUT_DIRECTORY         ${CMAKE_CURRENT_BINARY_DIR}
    ARCHIVE_OUTPUT_DIRECTORY_DEBUG   ${CMAKE_CURRENT_BINARY_DIR}
    ARCHIVE_OUTPUT_DIRECTORY_RELEASE ${CMAKE_CURRENT_BINARY_DIR})
get_filename_component(NK_OPS_INCLUDE_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/../../ops/include" ABSOLUTE)
set(NK_OPS_IMPORT_LIB
    "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_IMPORT_LIBRARY_PREFIX}nk_ops_kernels${CMAKE_IMPORT_LIBRARY_SUFFIX}")
set(NK_OPS_SHARED_DLL
    "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_SHARED_LIBRARY_PREFIX}nk_ops_kernels${CMAKE_SHARED_LIBRARY_SUFFIX}")

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/nk_codegen_test_paths.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/nk_codegen_test_paths.h @ONLY)
target_include_directories(numkit_gtest PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

# Ensure all fixtures exist before the test binary runs.
add_dependencies(numkit_gtest nk_sample_plugin nk_codegen_rt_shared nk_ops_kernels)
