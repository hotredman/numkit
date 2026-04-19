# tests/CMakeLists.txt — Test suite
#
# Included from root CMakeLists.txt via add_subdirectory(tests)
#
# Run examples:
#   ./mlab_tests                                  # all tests
#   ./mlab_tests --gtest_filter="TW_VM/*"         # dual-backend only
#   ./mlab_tests --gtest_filter="TW_VM/*VM*"      # VM-only runs
#   ./mlab_tests --gtest_filter="TW_VM/*TW*"      # TW-only runs
#   ./mlab_tests --gtest_filter="CommandSyntax*"   # parser tests

# ── Core: lexer, parser, command syntax ─────────────────────
set(CORE_TESTS
    core/lexer_test.cpp
    core/parser_test.cpp
    core/command_syntax_test.cpp
)

# ── Stdlib: language + standard library ─────────────────────
set(STDLIB_TESTS
    stdlib/arithmetic_test.cpp
    stdlib/variables_test.cpp
    stdlib/controlflow_test.cpp
    stdlib/functions_test.cpp
    stdlib/commands_test.cpp
    stdlib/comments_test.cpp
    stdlib/builtins_test.cpp
    stdlib/advanced_test.cpp
    stdlib/programs_test.cpp
    stdlib/matrix_test.cpp
    stdlib/indexing_test.cpp
    stdlib/concat_test.cpp
    stdlib/scope_test.cpp
    stdlib/types_test.cpp
    stdlib/integer_types_test.cpp
    stdlib/dynvars_test.cpp
    stdlib/integration_test.cpp
    stdlib/csv_test.cpp
    stdlib/vfs_test.cpp
)

# ── DSP library ─────────────────────────────────────────────
set(DSPLIB_TESTS
    dsplib/signal_core_test.cpp
    dsplib/convolution_test.cpp
    dsplib/filter_test.cpp
    dsplib/filter_design_test.cpp
    dsplib/spectral_test.cpp
    dsplib/windows_test.cpp
    dsplib/resample_test.cpp
    dsplib/transform_test.cpp
)

# ── Plot library ────────────────────────────────────────────
set(PLTLIB_TESTS
    pltlib/figure_test.cpp
)

# ── Fit library ─────────────────────────────────────────────
set(FITLIB_TESTS
    fitlib/interp_test.cpp
)

# ── Backend: VM, TreeWalker, benchmarks ─────────────────────
set(BACKEND_TESTS
    backend/vm_test.cpp
    backend/tree_walker_test.cpp
    backend/vm_benchmark.cpp
)

# ── Diagnostics: errors, debugger ───────────────────────────
set(DIAGNOSTICS_TESTS
    diagnostics/error_diagnostics_test.cpp
    diagnostics/debugger_phase1_test.cpp
    diagnostics/debugger_phase234_test.cpp
    diagnostics/debugger_diagnostic_test.cpp
    diagnostics/debug_session_test.cpp
    diagnostics/debug_session_test_eval_inject.cpp
    diagnostics/who_diagnostic_test.cpp
    diagnostics/compiler_assigned_vars_test.cpp
    diagnostics/matlab_parity_test.cpp
    diagnostics/register_constant_test.cpp
    diagnostics/known_issues_test.cpp
)

# ── Assemble the test executable ─────────────────────────────
add_executable(mlab_tests
    main.cpp
    ${CORE_TESTS}
    ${STDLIB_TESTS}
    ${DSPLIB_TESTS}
    ${PLTLIB_TESTS}
    ${FITLIB_TESTS}
    ${BACKEND_TESTS}
    ${DIAGNOSTICS_TESTS}
)

target_include_directories(mlab_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}   # dual_engine_fixture.hpp
)

target_link_libraries(mlab_tests
    PRIVATE
        mlab::mlab
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(mlab_tests DISCOVERY_MODE PRE_TEST)
