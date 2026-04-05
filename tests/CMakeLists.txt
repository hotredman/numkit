# tests/CMakeLists.txt — Test suite
#
# Included from root CMakeLists.txt via add_subdirectory(tests)
#
# Run examples:
#   ./mlab_tests                                  # all tests
#   ./mlab_tests --gtest_filter="TW_VM/*"         # dual-backend only
#   ./mlab_tests --gtest_filter="TW_VM/*VM*"      # VM-only runs
#   ./mlab_tests --gtest_filter="TW_VM/*TW*"      # TW-only runs
#   ./mlab_tests --gtest_filter="CommandStyle*"    # parser tests

# ── Dual-backend parameterized tests (TW + VM) ──────────────
set(DUAL_BACKEND_TESTS
    test_arithmetic.cpp
    test_variables.cpp
    test_matrices.cpp
    test_controlflow.cpp
    test_functions.cpp
    test_builtins.cpp
    test_commands.cpp
    test_comments.cpp
    test_programs.cpp
    test_advanced.cpp
    test_known_issues.cpp
    test_debugger_phase1.cpp
    test_debugger_phase234.cpp
    test_debugger_diagnostic.cpp
    test_who_diagnostic.cpp
    test_error_diagnostics.cpp
)

# ── Frontend tests (lexer, parser, command-style AST) ────────
set(FRONTEND_TESTS
    lexer_test.cpp
    parser_test.cpp
    command_style_test.cpp
)

# ── Domain-specific tests (signal processing, plotting) ─────
set(DOMAIN_TESTS
    figure_test.cpp
    signal_core_test.cpp
    convolution_test.cpp
    windows_test.cpp
    filter_test.cpp
    filter_design_test.cpp
    spectral_test.cpp
    resample_test.cpp
    transform_test.cpp
    interp_test.cpp
)

# ── Legacy / standalone backend tests ────────────────────────
set(LEGACY_TESTS
    engine_test.cpp
    engine_advanced_test.cpp
    tree_walker_test.cpp
    vm_test.cpp
)

# ── Benchmarks ───────────────────────────────────────────────
set(BENCHMARK_SOURCES
    vm_benchmark.cpp
)

# ── Assemble the test executable ─────────────────────────────
add_executable(mlab_tests
    main.cpp
    ${DUAL_BACKEND_TESTS}
    ${FRONTEND_TESTS}
    ${DOMAIN_TESTS}
    ${LEGACY_TESTS}
    ${BENCHMARK_SOURCES}
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
gtest_discover_tests(mlab_tests)
