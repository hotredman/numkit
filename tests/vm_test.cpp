// tests/vm_test.cpp
//
// Tests for Bytecode Compiler + VM
// Phase 1: scalar arithmetic, variables, display

#include "MLabCompiler.hpp"
#include "MLabEngine.hpp"
#include "MLabLexer.hpp"
#include "MLabParser.hpp"
#include "MLabStdLibrary.hpp"
#include "MLabVM.hpp"
#include <cmath>
#include <gtest/gtest.h>

using namespace mlab;

class VMTest : public ::testing::Test
{
public:
    Engine engine;
    Compiler compiler{engine};
    VM vm{engine};
    std::string capturedOutput;

    void SetUp() override
    {
        StdLibrary::install(engine);
        capturedOutput.clear();
        engine.setOutputFunc([this](const std::string &s) { capturedOutput += s; });
    }

    // Parse + compile + execute
    MValue run(const std::string &code)
    {
        Lexer lexer(code);
        auto tokens = lexer.tokenize();
        Parser parser(tokens);
        auto ast = parser.parse();
        auto chunk = compiler.compile(ast.get());
        return vm.execute(chunk);
    }

    double runScalar(const std::string &code) { return run(code).toScalar(); }
};

// ============================================================
// Number literals
// ============================================================

TEST_F(VMTest, IntegerLiteral)
{
    EXPECT_DOUBLE_EQ(runScalar("42;"), 42.0);
}

TEST_F(VMTest, FloatLiteral)
{
    EXPECT_DOUBLE_EQ(runScalar("3.14;"), 3.14);
}

TEST_F(VMTest, NegativeLiteral)
{
    EXPECT_DOUBLE_EQ(runScalar("-5;"), -5.0);
}

// ============================================================
// Basic arithmetic
// ============================================================

TEST_F(VMTest, Addition)
{
    EXPECT_DOUBLE_EQ(runScalar("2 + 3;"), 5.0);
}

TEST_F(VMTest, Subtraction)
{
    EXPECT_DOUBLE_EQ(runScalar("10 - 7;"), 3.0);
}

TEST_F(VMTest, Multiplication)
{
    EXPECT_DOUBLE_EQ(runScalar("4 * 5;"), 20.0);
}

TEST_F(VMTest, Division)
{
    EXPECT_DOUBLE_EQ(runScalar("15 / 3;"), 5.0);
}

TEST_F(VMTest, Power)
{
    EXPECT_DOUBLE_EQ(runScalar("2 ^ 10;"), 1024.0);
}

TEST_F(VMTest, ComplexExpression)
{
    EXPECT_DOUBLE_EQ(runScalar("(2 + 3) * 4;"), 20.0);
}

TEST_F(VMTest, OperatorPrecedence)
{
    EXPECT_DOUBLE_EQ(runScalar("2 + 3 * 4;"), 14.0);
}

TEST_F(VMTest, ChainedOps)
{
    EXPECT_DOUBLE_EQ(runScalar("((2 + 3) * 4 - 6) / 7;"), 2.0);
}

// ============================================================
// Unary operators
// ============================================================

TEST_F(VMTest, UnaryMinus)
{
    EXPECT_DOUBLE_EQ(runScalar("-5;"), -5.0);
}

TEST_F(VMTest, UnaryPlus)
{
    EXPECT_DOUBLE_EQ(runScalar("+5;"), 5.0);
}

TEST_F(VMTest, DoubleNegation)
{
    EXPECT_DOUBLE_EQ(runScalar("-(-3);"), 3.0);
}

// ============================================================
// Variables and assignment
// ============================================================

TEST_F(VMTest, SimpleAssign)
{
    EXPECT_DOUBLE_EQ(runScalar("x = 42;"), 42.0);
}

TEST_F(VMTest, VariableReuse)
{
    EXPECT_DOUBLE_EQ(runScalar("x = 5; y = x + 3;"), 8.0);
}

TEST_F(VMTest, VariableReassign)
{
    EXPECT_DOUBLE_EQ(runScalar("x = 1; x = x + 1; x = x * 3;"), 6.0);
}

TEST_F(VMTest, MultipleVariables)
{
    EXPECT_DOUBLE_EQ(runScalar("a = 2; b = 3; c = a * b + 1;"), 7.0);
}

// ============================================================
// Comparison operators
// ============================================================

TEST_F(VMTest, Equal)
{
    EXPECT_DOUBLE_EQ(runScalar("3 == 3;"), 1.0);
    EXPECT_DOUBLE_EQ(runScalar("3 == 4;"), 0.0);
}

TEST_F(VMTest, NotEqual)
{
    EXPECT_DOUBLE_EQ(runScalar("3 ~= 4;"), 1.0);
    EXPECT_DOUBLE_EQ(runScalar("3 ~= 3;"), 0.0);
}

TEST_F(VMTest, LessThan)
{
    EXPECT_DOUBLE_EQ(runScalar("2 < 3;"), 1.0);
    EXPECT_DOUBLE_EQ(runScalar("3 < 2;"), 0.0);
}

TEST_F(VMTest, GreaterThan)
{
    EXPECT_DOUBLE_EQ(runScalar("5 > 3;"), 1.0);
}

TEST_F(VMTest, LessEqual)
{
    EXPECT_DOUBLE_EQ(runScalar("3 <= 3;"), 1.0);
    EXPECT_DOUBLE_EQ(runScalar("4 <= 3;"), 0.0);
}

TEST_F(VMTest, GreaterEqual)
{
    EXPECT_DOUBLE_EQ(runScalar("3 >= 3;"), 1.0);
    EXPECT_DOUBLE_EQ(runScalar("2 >= 3;"), 0.0);
}

// ============================================================
// Logical operators
// ============================================================

TEST_F(VMTest, LogicalAnd)
{
    EXPECT_DOUBLE_EQ(runScalar("1 & 1;"), 1.0);
    EXPECT_DOUBLE_EQ(runScalar("1 & 0;"), 0.0);
}

TEST_F(VMTest, LogicalOr)
{
    EXPECT_DOUBLE_EQ(runScalar("0 | 1;"), 1.0);
    EXPECT_DOUBLE_EQ(runScalar("0 | 0;"), 0.0);
}

TEST_F(VMTest, LogicalNot)
{
    EXPECT_DOUBLE_EQ(runScalar("~0;"), 1.0);
    EXPECT_DOUBLE_EQ(runScalar("~1;"), 0.0);
}

TEST_F(VMTest, ShortCircuitAnd)
{
    EXPECT_DOUBLE_EQ(runScalar("1 && 1;"), 1.0);
    EXPECT_DOUBLE_EQ(runScalar("0 && 1;"), 0.0);
}

TEST_F(VMTest, ShortCircuitOr)
{
    EXPECT_DOUBLE_EQ(runScalar("0 || 1;"), 1.0);
    EXPECT_DOUBLE_EQ(runScalar("1 || 0;"), 1.0);
}

// ============================================================
// Display (no semicolon)
// ============================================================

TEST_F(VMTest, SuppressOutput)
{
    capturedOutput.clear();
    run("42;");
    EXPECT_TRUE(capturedOutput.empty());
}

TEST_F(VMTest, ShowOutput)
{
    capturedOutput.clear();
    run("42");
    EXPECT_FALSE(capturedOutput.empty());
    EXPECT_NE(capturedOutput.find("42"), std::string::npos);
}

TEST_F(VMTest, AssignShowOutput)
{
    capturedOutput.clear();
    run("x = 42");
    EXPECT_FALSE(capturedOutput.empty());
    EXPECT_NE(capturedOutput.find("x"), std::string::npos);
    EXPECT_NE(capturedOutput.find("42"), std::string::npos);
}

TEST_F(VMTest, AssignSuppressOutput)
{
    capturedOutput.clear();
    run("x = 42;");
    EXPECT_TRUE(capturedOutput.empty());
}

// ============================================================
// Edge cases
// ============================================================

TEST_F(VMTest, DivisionByZero)
{
    double result = runScalar("1 / 0;");
    EXPECT_TRUE(std::isinf(result));
}

TEST_F(VMTest, ZeroDivZero)
{
    double result = runScalar("0 / 0;");
    EXPECT_TRUE(std::isnan(result));
}

TEST_F(VMTest, NegPowerPrecedence)
{
    // -2^2 = -(2^2) = -4
    EXPECT_DOUBLE_EQ(runScalar("-2^2;"), -4.0);
}

TEST_F(VMTest, PowerRightAssoc)
{
    // 2^3^2 = 2^(3^2) = 512
    EXPECT_DOUBLE_EQ(runScalar("2^3^2;"), 512.0);
}