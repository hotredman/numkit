target_sources(numkit_gtest PRIVATE
    # lexer / parser / compiler
    ${CMAKE_CURRENT_SOURCE_DIR}/lexer_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/parser_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/command_syntax_test.cpp
    # Stack safety — watermark guard mechanism (dev-docs/memory/stack_safety.md layer 2)
    ${CMAKE_CURRENT_SOURCE_DIR}/stack_guard_test.cpp
    # TreeWalker / VM backends
    ${CMAKE_CURRENT_SOURCE_DIR}/tree_walker_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/vm_test.cpp
    # Object model (dev-docs/OBJECT_MODEL.md) — P1 infra: type + registry + clone
    ${CMAKE_CURRENT_SOURCE_DIR}/object_model_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/vm_benchmark.cpp
    # Thread pool — no-op file body when NUMKIT_WITH_THREADS is OFF
    ${CMAKE_CURRENT_SOURCE_DIR}/thread_pool_test.cpp
    # ND shape ops — broadcasting, strides, coord-walking
    ${CMAKE_CURRENT_SOURCE_DIR}/shape_ops_test.cpp
    # Fused element-wise kernels (ops::fused*) — Stage 0 of VM fusion
    ${CMAKE_CURRENT_SOURCE_DIR}/fused_kernels_test.cpp
    # Fusion rule registry + gate (Engine) — Stage 1 of VM fusion
    ${CMAKE_CURRENT_SOURCE_DIR}/fusion_registry_test.cpp
    # Fusion parity (fused == unfused, both engines) — Stage 2 of VM fusion
    ${CMAKE_CURRENT_SOURCE_DIR}/fusion_parity_test.cpp
    # Complex floor/ceil/round/fix (component-wise) + expm1 (=exp(z)-1)
    ${CMAKE_CURRENT_SOURCE_DIR}/complex_math_test.cpp
    # ScratchArena — inline-buffer / upstream-spill / value-init
    ${CMAKE_CURRENT_SOURCE_DIR}/scratch_arena_test.cpp
    # Namespace resolver (Phase 6) — `import` integration tests
    ${CMAKE_CURRENT_SOURCE_DIR}/namespace_resolver_test.cpp
    # VFS Phase-8 extension — listDir / stat / mkdir / rmdir / unlink / tempArea
    ${CMAKE_CURRENT_SOURCE_DIR}/vfs_extension_test.cpp
    # M-file resolver (Phase 9a) — addpath + .m loading via VFS
    ${CMAKE_CURRENT_SOURCE_DIR}/mfile_resolver_test.cpp
    # Folder + path-utility builtins (Phase 9c)
    ${CMAKE_CURRENT_SOURCE_DIR}/folder_builtins_test.cpp
    # Package namespaces (Phase 10) — +pkg/ folder resolution
    ${CMAKE_CURRENT_SOURCE_DIR}/package_namespace_test.cpp
    # Variable-viewer integer/single serialization (numericCellJSON + stats)
    ${CMAKE_CURRENT_SOURCE_DIR}/value_json_test.cpp
    # Value::deepBytes() — whos-correct deep struct/cell sizing (Bytes column)
    ${CMAKE_CURRENT_SOURCE_DIR}/value_deepbytes_test.cpp
    # First-class CSL (ValueType::CSL) foundation + collapseCsl (value-level)
    ${CMAKE_CURRENT_SOURCE_DIR}/value_csl_test.cpp
    # Line-series decimation (M4 / LTTB) — twin of decimate.js
    ${CMAKE_CURRENT_SOURCE_DIR}/decimate_test.cpp
)

# Relocated from tests/builtin (layer split 2026-06-17).
target_sources(numkit_gtest PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/advanced_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/commands_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/comments_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/controlflow_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/dynvars_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/frame_introspection_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/functions_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/programs_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/scope_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/variables_test.cpp
)
