# tests/gtest/CMakeLists.txt — owner of the single numkit_gtest binary
#
# After this runs, each component can contribute its unit tests via
#   target_sources(numkit_gtest PRIVATE ...)
# from its own <component>/tests/CMakeLists.txt. The root CMakeLists
# invokes add_subdirectory(<component>/tests) for each.
#
# Layout:
#   tests/gtest/main.cpp        gtest_main replacement (if any), always included
#   tests/gtest/support/        shared fixtures (dual_engine_fixture.hpp, ...)
#   tests/gtest/integration/    cross-cutting tests — debugger, diagnostics, parity
#   tests/gtest/compat/         non-C++ MATLAB parity scripts (not compiled)
#
# Run examples:
#   ./numkit_gtest                                   # all tests
#   ./numkit_gtest --gtest_filter="TW_VM/*"          # dual-backend only
#   ./numkit_gtest --gtest_filter="CommandSyntax*"   # parser tests

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
    integration/try_first_block_bug_test.cpp
    integration/multi_assign_lvalue_test.cpp
    integration/compound_lvalue_test.cpp
    integration/multi_output_args_test.cpp
    integration/containers_test.cpp
    integration/string_indexing_test.cpp
)

# ── The one test binary ───────────────────────────────────────────────────
add_executable(numkit_gtest
    main.cpp
    ${INTEGRATION_TESTS}
)

# Each component's tests/CMakeLists.txt appends its sources via
# target_sources(numkit_gtest PRIVATE ...). That happens after this file runs.

target_include_directories(numkit_gtest
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/support   # dual_engine_fixture.hpp
)

target_link_libraries(numkit_gtest
    PRIVATE
        numkit::numkit
        GTest::gtest_main
)

# Force-include StandardEngine into every test TU, so the suite's `StandardEngine engine;`
# (a fully-loaded engine — the standard library auto-installed) needs no
# per-file include. Resolved via numkit's public bundle/ include dir, which the
# link above propagates. (core's Engine ctor no longer installs libraries.)
if(MSVC)
    target_compile_options(numkit_gtest PRIVATE /FInumkit/bundle/standard_engine.hpp)
else()
    target_compile_options(numkit_gtest PRIVATE -include numkit/bundle/standard_engine.hpp)
endif()

include(GoogleTest)
gtest_discover_tests(numkit_gtest DISCOVERY_MODE PRE_TEST)
