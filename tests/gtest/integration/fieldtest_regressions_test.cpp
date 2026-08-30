// tests/gtest/integration/fieldtest_regressions_test.cpp
//
// Regression guards for bugs found by the REAL-WORLD differential corpus
// (fieldtest/ — GitHub MATLAB code run through MATLAB R2025b vs numkit).
// Each test asserts the MATLAB-observed behaviour; DISABLED_ per the bug
// protocol until its bug is fixed (then the prefix is removed and the test
// goes live). Companion bug files: bugs/lang/complex-relational-ops.md,
// bugs/lang/command-syntax-url-args.md.

#include "dual_engine_fixture.hpp"

#include <filesystem>
#include <fstream>

using namespace m_test;
using namespace numkit;

class FieldTestRegression : public DualEngineTest {};

// ── bugs/lang/complex-relational-ops ─────────────────────────────────────
// MATLAB R2025b: `< > <= >=` on complex compare REAL parts and return a
// logical — (0+1i) < 2 is 1. numkit currently throws "Operator '<' is not
// supported for complex operands" (found via the AHP.m corpus script).
TEST_P(FieldTestRegression, ComplexRelationalComparesRealPart)
{
    EXPECT_TRUE(engine.evalSafe("w = (complex(1,0) < 2);").ok)
        << "complex(1,0) < 2 must compare real parts (MATLAB returns 1)";
    EXPECT_DOUBLE_EQ(engine.eval("w").toScalar(), 1.0);

    EXPECT_TRUE(engine.evalSafe("w2 = ((0+1i) < 2);").ok)
        << "even a nonzero imaginary part is ignored by MATLAB orderings";
    EXPECT_DOUBLE_EQ(engine.eval("w2").toScalar(), 1.0);

    EXPECT_TRUE(engine.evalSafe("w3 = ((3+1i) < 2);").ok);
    EXPECT_DOUBLE_EQ(engine.eval("w3").toScalar(), 0.0);
}

// ── bugs/lang/command-syntax-url-args ────────────────────────────────────
// Command syntax takes everything after the head as whitespace-split char
// literals — ':' '/' '.' are NOT operators there. MATLAB `disp a//b:c`
// prints "a//b:c"; `foo -bar http://x.y/z` may fail at the FUNCTION level
// but never with a parse/token error.
TEST_P(FieldTestRegression, CommandSyntaxUrlArgs)
{
    // Silent truncation is the worse half: numkit today prints "a//b".
    EXPECT_TRUE(engine.evalSafe("s = 'a//b:c'; disp2 = s;").ok); // sanity

    auto r1 = engine.evalSafe("disp a//b:c");
    EXPECT_TRUE(r1.ok) << "command args must not be token-lexed";

    // Parse-level failure on a URL argument.
    auto r2 = engine.evalSafe("foo -bar http://x.y/z");
    EXPECT_FALSE(r2.ok); // foo is undefined — an error is correct…
    EXPECT_EQ(r2.errorMessage.find("Unexpected token"), std::string::npos)
        << "…but a FUNCTION-level error, never a token-level parse error";
}

// ── bugs/opened/lang/command-syntax-quoted-arg ────────────────────────────
// A quoted string as the command argument is silently dropped (numkit prints
// nothing; MATLAB prints the string). Engine-observable guard: a QUOTED
// filename passed to `load` in command form must load the file.
TEST_P(FieldTestRegression, DISABLED_CommandSyntaxQuotedArg)
{
    namespace fs = std::filesystem;
    fs::path dir = fs::temp_directory_path() / "numkit_cmd_quoted_arg";
    fs::create_directories(dir);
    fs::path mat = dir / "qtest.mat";
    {
        StandardEngine se;
        se.eval("qv = 42; save('" + mat.generic_string() + "', 'qv');");
    }
    engine.eval("clearvars;");
    engine.eval("load '" + mat.generic_string() + "'");   // command form, QUOTED arg
    Value *v = engine.getVariable("qv");
    ASSERT_NE(v, nullptr) << "quoted command arg must reach load";
    EXPECT_DOUBLE_EQ(v->toScalar(), 42.0);
    fs::remove_all(dir);
}

// ── bugs/closed/lang/run-invokes-nullary-function-file ─────────────────────
// MATLAB run() semantics: a FUNCTION file EXECUTES its primary function.
// Observable via a global set inside the function (round-trip verified).
TEST_P(FieldTestRegression, FileRunInvokesNullaryFunction)
{
    namespace fs = std::filesystem;
    fs::path dir = fs::temp_directory_path() / "numkit_run_nullary_ft";
    fs::create_directories(dir);
    fs::path m = dir / "ft_hello.m";
    {
        std::ofstream f(m);
        f << "function ft_hello\nglobal FT_QV\nFT_QV = 42;\nend\n";
    }
    engine.eval("global FT_QV; FT_QV = -1;");
    engine.eval("run('" + m.generic_string() + "');");
    Value *v = engine.getVariable("FT_QV");
    ASSERT_NE(v, nullptr);
    EXPECT_DOUBLE_EQ(v->toScalar(), 42.0);
    fs::remove_all(dir);
}

INSTANTIATE_DUAL(FieldTestRegression);
