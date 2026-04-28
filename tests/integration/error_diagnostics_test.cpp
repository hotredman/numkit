// tests/test_error_diagnostics.cpp — Error diagnostics: evalSafe, line/col, context
// Parameterized: runs on both TreeWalker and VM backends

#include "dual_engine_fixture.hpp"

using namespace m_test;
using namespace numkit;

class ErrorDiagnosticsTest : public DualEngineTest
{};

// ============================================================
// 1. evalSafe basics
// ============================================================

TEST_P(ErrorDiagnosticsTest, EvalSafeSuccessReturnsOk)
{
    auto r = engine.evalSafe("x = 2 + 3;");
    EXPECT_TRUE(r.ok);
    EXPECT_EQ(r.errorMessage, "");
    EXPECT_EQ(r.errorLine, 0);
    EXPECT_FALSE(r.debugStop);
}

TEST_P(ErrorDiagnosticsTest, EvalSafeReturnsValue)
{
    auto r = engine.evalSafe("42;");
    EXPECT_TRUE(r.ok);
    EXPECT_DOUBLE_EQ(r.value.toScalar(), 42.0);
}

TEST_P(ErrorDiagnosticsTest, EvalSafeErrorReturnsNotOk)
{
    auto r = engine.evalSafe("undefined_var_xyz");
    EXPECT_FALSE(r.ok);
    EXPECT_FALSE(r.errorMessage.empty());
}

TEST_P(ErrorDiagnosticsTest, EvalSafeDoesNotThrow)
{
    // Should never throw, even on errors
    EXPECT_NO_THROW({
        auto r = engine.evalSafe("x{1}");
        (void)r;
    });
}

// ============================================================
// 2. Error line numbers
// ============================================================

TEST_P(ErrorDiagnosticsTest, ErrorOnCorrectLine)
{
    // Use runtime error (index out of bounds) — VM can locate these.
    // "undefined variable" is a compile-time error in VM and has no line info.
    auto r = engine.evalSafe("x = [1 2];\ny = 2;\nz = x(100);\n");
    EXPECT_FALSE(r.ok);
    EXPECT_EQ(r.errorLine, 3);
}

TEST_P(ErrorDiagnosticsTest, ErrorOnFirstLine)
{
    // Use runtime error — cell indexing on non-cell
    auto r = engine.evalSafe("x = 5; x{1};");
    EXPECT_FALSE(r.ok);
    EXPECT_GE(r.errorLine, 1);
}

TEST_P(ErrorDiagnosticsTest, ErrorOnMiddleLine)
{
    auto r = engine.evalSafe("a = 1;\nb = 2;\nc = a{1};\nd = 4;\n");
    EXPECT_FALSE(r.ok);
    EXPECT_EQ(r.errorLine, 3);
}

TEST_P(ErrorDiagnosticsTest, IndexOutOfBoundsHasLine)
{
    auto r = engine.evalSafe("x = [1 2 3];\ny = x(100);\n");
    EXPECT_FALSE(r.ok);
    EXPECT_EQ(r.errorLine, 2);
}

// ============================================================
// 3. Error column precision
// ============================================================

TEST_P(ErrorDiagnosticsTest, ColumnPointsToOperator)
{
    // "a = 5; b = a{2};" — error on cell indexing of 'a', not on 'b = ...'
    auto r = engine.evalSafe("a = 5; b = a{2};");
    EXPECT_FALSE(r.ok);
    // Column should be past the first statement (a = 5; is 7 chars)
    EXPECT_GT(r.errorCol, 7);
}

TEST_P(ErrorDiagnosticsTest, ColumnOnMultiStatementLine)
{
    auto r = engine.evalSafe("x = 1; y = 2; z = x{1};");
    EXPECT_FALSE(r.ok);
    // 'x{1}' starts well after column 1
    EXPECT_GT(r.errorCol, 14);
}

// ============================================================
// 4. Error context (operator description)
// ============================================================

TEST_P(ErrorDiagnosticsTest, ContextForFunctionCall)
{
    auto r = engine.evalSafe("x = 'hello'; sin(x);");
    EXPECT_FALSE(r.ok);
    EXPECT_NE(r.errorContext.find("sin"), std::string::npos)
        << "context should mention function name, got: " << r.errorContext;
}

TEST_P(ErrorDiagnosticsTest, ContextForCellIndexing)
{
    auto r = engine.evalSafe("a = [1,2]; a{1};");
    EXPECT_FALSE(r.ok);
    EXPECT_NE(r.errorContext.find("cell"), std::string::npos)
        << "context should mention cell indexing, got: " << r.errorContext;
}

TEST_P(ErrorDiagnosticsTest, ContextForBinaryOp)
{
    auto r = engine.evalSafe("a = 'hello' + [1,2,3];");
    EXPECT_FALSE(r.ok);
    EXPECT_NE(r.errorContext.find("+"), std::string::npos)
        << "context should mention operator, got: " << r.errorContext;
}

TEST_P(ErrorDiagnosticsTest, ContextForFieldAccess)
{
    auto r = engine.evalSafe("a = 5; a.x;");
    EXPECT_FALSE(r.ok);
    EXPECT_NE(r.errorContext.find("field"), std::string::npos)
        << "context should mention field access, got: " << r.errorContext;
}

TEST_P(ErrorDiagnosticsTest, ContextForColonExpr)
{
    auto r = engine.evalSafe("x = 1:0:10;");
    EXPECT_FALSE(r.ok);
    EXPECT_NE(r.errorContext.find("colon"), std::string::npos)
        << "context should mention colon expression, got: " << r.errorContext;
}

TEST_P(ErrorDiagnosticsTest, ContextForMatrixConcat)
{
    auto r = engine.evalSafe("x = [1 2; 3 4 5];");
    EXPECT_FALSE(r.ok);
    EXPECT_NE(r.errorContext.find("matrix"), std::string::npos)
        << "context should mention matrix construction, got: " << r.errorContext;
}

// ============================================================
// 5. what() stays clean for MATLAB try/catch
// ============================================================

TEST_P(ErrorDiagnosticsTest, TryCatchGetsCleanMessage)
{
    eval("try\n  error('test message');\ncatch e\n  msg = e.message;\nend\n");
    auto *v = engine.getVariable("msg");
    ASSERT_NE(v, nullptr);
    EXPECT_EQ(v->toString(), "test message");
}

TEST_P(ErrorDiagnosticsTest, TryCatchMessageNoContext)
{
    // error() message should NOT contain context description
    eval("try\n  error('clean msg');\ncatch e\n  msg = e.message;\nend\n");
    auto *v = engine.getVariable("msg");
    ASSERT_NE(v, nullptr);
    std::string msg = v->toString();
    EXPECT_EQ(msg.find("in call"), std::string::npos)
        << "try/catch message should not contain context, got: " << msg;
}

// ============================================================
// 6. Complex scenarios
// ============================================================

TEST_P(ErrorDiagnosticsTest, FFTComplexIndexingError)
{
    auto r = engine.evalSafe(
        "Fs = 256;\n"
        "t = 0:1/Fs:1-1/Fs;\n"
        "N = length(t);\n"
        "x = 0.7*sin(2*pi*30*t);\n"
        "X = fft(x);\n"
        "mag = abs(X(1:N/2+1)) / N;\n"
        "mag(2:end-1) = 2 * mag(2:end-1);\n"
    );
    // This may succeed (fft of real input) or fail depending on implementation
    // If it fails, error should have line info
    if (!r.ok) {
        EXPECT_GT(r.errorLine, 0) << "error should have line number";
    }
}

TEST_P(ErrorDiagnosticsTest, NestedFunctionErrorHasLine)
{
    auto r = engine.evalSafe(
        "function y = foo(x)\n"
        "  y = x{1};\n"
        "end\n"
        "a = 5;\n"
        "foo(a);\n"
    );
    EXPECT_FALSE(r.ok);
    EXPECT_GT(r.errorLine, 0);
}

TEST_P(ErrorDiagnosticsTest, MultipleErrorsIndependent)
{
    // First error
    auto r1 = engine.evalSafe("x = [1,2]; x{1};");
    EXPECT_FALSE(r1.ok);

    // Second error — should not be affected by first
    auto r2 = engine.evalSafe("y = 5; sin(y);");
    EXPECT_TRUE(r2.ok) << "second eval should succeed independently";
}

TEST_P(ErrorDiagnosticsTest, DebugStopIsNotError)
{
    // Without debug observer, debugStop should never be true
    auto r = engine.evalSafe("x = 1;");
    EXPECT_TRUE(r.ok);
    EXPECT_FALSE(r.debugStop);
}

INSTANTIATE_DUAL(ErrorDiagnosticsTest);
