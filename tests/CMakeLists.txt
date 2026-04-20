# tests/CMakeLists.txt — owner of the single m_tests binary
#
# After this runs, each component can contribute its unit tests via
#   target_sources(m_tests PRIVATE ...)
# from its own <component>/tests/CMakeLists.txt. The root CMakeLists
# invokes add_subdirectory(<component>/tests) for each.
#
# Layout:
#   tests/main.cpp            gtest_main replacement (if any), always included
#   tests/support/            shared fixtures (dual_engine_fixture.hpp, ...)
#   tests/integration/        cross-cutting tests — debugger, diagnostics, matlab-parity
#   tests/compat/             non-C++ MATLAB parity scripts (not compiled)
#
# Run examples:
#   ./m_tests                                   # all tests
#   ./m_tests --gtest_filter="TW_VM/*"          # dual-backend only
#   ./m_tests --gtest_filter="CommandSyntax*"   # parser tests

# ── Cross-cutting / integration tests (debugger, diagnostics, parity) ─────
set(INTEGRATION_TESTS
    integration/error_diagnostics_test.cpp
    integration/debugger_phase1_test.cpp
    integration/debugger_phase234_test.cpp
    integration/debugger_diagnostic_test.cpp
    integration/debug_session_test.cpp
    integration/debug_session_test_eval_inject.cpp
    integration/who_diagnostic_test.cpp
    integration/compiler_assigned_vars_test.cpp
    integration/matlab_parity_test.cpp
    integration/register_constant_test.cpp
    integration/known_issues_test.cpp
)

# ── The one test binary ───────────────────────────────────────────────────
add_executable(m_tests
    main.cpp
    ${INTEGRATION_TESTS}
)

# Each component's tests/CMakeLists.txt appends its sources via
# target_sources(m_tests PRIVATE ...). That happens after this file runs.

target_include_directories(m_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/support   # dual_engine_fixture.hpp
)

target_link_libraries(m_tests
    PRIVATE
        numkit::m
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(m_tests DISCOVERY_MODE PRE_TEST)
