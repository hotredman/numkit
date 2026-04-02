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
        vm.setCompiledFuncs(&compiler.compiledFuncs());
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
    // Debug dump
    {
        Lexer lexer("2 + 3;");
        auto tokens = lexer.tokenize();
        Parser parser(tokens);
        auto ast = parser.parse();
        auto chunk = compiler.compile(ast.get());
        std::cerr << Compiler::disassemble(chunk);
        auto result = vm.execute(chunk);
        std::cerr << "Result register value: " << result.toScalar() << "\n";
    }
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

// ============================================================
// Phase 2: if/elseif/else
// ============================================================

TEST_F(VMTest, IfTrue)
{
    EXPECT_DOUBLE_EQ(runScalar("x = 0; if 1; x = 42; end; x;"), 42.0);
}

TEST_F(VMTest, IfFalse)
{
    EXPECT_DOUBLE_EQ(runScalar("x = 0; if 0; x = 42; end; x;"), 0.0);
}

TEST_F(VMTest, IfElse)
{
    EXPECT_DOUBLE_EQ(runScalar("x = 0; if 0; x = 1; else; x = 2; end; x;"), 2.0);
}

TEST_F(VMTest, IfElseTrue)
{
    EXPECT_DOUBLE_EQ(runScalar("x = 0; if 1; x = 1; else; x = 2; end; x;"), 1.0);
}

TEST_F(VMTest, IfElseifElse)
{
    EXPECT_DOUBLE_EQ(runScalar("x = 0; if 0; x = 1; elseif 1; x = 2; else; x = 3; end; x;"), 2.0);
}

TEST_F(VMTest, IfElseifFallthrough)
{
    EXPECT_DOUBLE_EQ(runScalar("x = 0; if 0; x = 1; elseif 0; x = 2; else; x = 3; end; x;"), 3.0);
}

TEST_F(VMTest, IfWithExpression)
{
    EXPECT_DOUBLE_EQ(runScalar("a = 5; b = 3; if a > b; x = a - b; else; x = b - a; end; x;"), 2.0);
}

TEST_F(VMTest, NestedIf)
{
    EXPECT_DOUBLE_EQ(runScalar("x = 0; if 1; if 0; x = 1; else; x = 2; end; end; x;"), 2.0);
}

// ============================================================
// Phase 2: while
// ============================================================

TEST_F(VMTest, WhileBasic)
{
    EXPECT_DOUBLE_EQ(runScalar("x = 0; while x < 5; x = x + 1; end; x;"), 5.0);
}

TEST_F(VMTest, WhileNever)
{
    EXPECT_DOUBLE_EQ(runScalar("x = 10; while 0; x = 0; end; x;"), 10.0);
}

TEST_F(VMTest, WhileCountdown)
{
    EXPECT_DOUBLE_EQ(runScalar("x = 10; while x > 0; x = x - 1; end; x;"), 0.0);
}

TEST_F(VMTest, WhileAccumulate)
{
    EXPECT_DOUBLE_EQ(runScalar("s = 0; i = 1; while i <= 10; s = s + i; i = i + 1; end; s;"), 55.0);
}

TEST_F(VMTest, NestedWhile)
{
    EXPECT_DOUBLE_EQ(runScalar("s = 0; i = 0; while i < 3; j = 0; while j < 4; s = s + 1; j = j + "
                               "1; end; i = i + 1; end; s;"),
                     12.0);
}

// ============================================================
// Phase 2: break and continue
// ============================================================

TEST_F(VMTest, WhileBreak)
{
    EXPECT_DOUBLE_EQ(runScalar("x = 0; while 1; x = x + 1; if x == 5; break; end; end; x;"), 5.0);
}

TEST_F(VMTest, WhileContinue)
{
    // Skip iteration when x == 3
    EXPECT_DOUBLE_EQ(
        runScalar(
            "s = 0; i = 0; while i < 5; i = i + 1; if i == 3; continue; end; s = s + i; end; s;"),
        12.0);
}

TEST_F(VMTest, WhileBreakNested)
{
    // Break only exits inner loop
    EXPECT_DOUBLE_EQ(runScalar("s = 0; i = 0; while i < 3; i = i + 1; j = 0; while j < 10; j = j + "
                               "1; if j == 2; break; end; s = s + 1; end; end; s;"),
                     3.0);
}

// ============================================================
// Phase 3: for-loop
// ============================================================

TEST_F(VMTest, ForBasicRange)
{
    EXPECT_DOUBLE_EQ(runScalar("s = 0; for i = 1:5; s = s + i; end; s;"), 15.0);
}

TEST_F(VMTest, ForStepRange)
{
    EXPECT_DOUBLE_EQ(runScalar("s = 0; for i = 1:2:9; s = s + i; end; s;"), 25.0);
}

TEST_F(VMTest, ForSingleElement)
{
    EXPECT_DOUBLE_EQ(runScalar("s = 0; for i = 5; s = s + i; end; s;"), 5.0);
}

TEST_F(VMTest, ForEmptyRange)
{
    EXPECT_DOUBLE_EQ(runScalar("s = 99; for i = 5:1; s = 0; end; s;"), 99.0);
}

TEST_F(VMTest, ForBreak)
{
    EXPECT_DOUBLE_EQ(runScalar("s = 0; for i = 1:10; if i == 4; break; end; s = s + i; end; s;"),
                     6.0);
}

TEST_F(VMTest, ForContinue)
{
    // Skip i == 3
    EXPECT_DOUBLE_EQ(runScalar("s = 0; for i = 1:5; if i == 3; continue; end; s = s + i; end; s;"),
                     12.0);
}

TEST_F(VMTest, ForNested)
{
    EXPECT_DOUBLE_EQ(runScalar("s = 0; for i = 1:3; for j = 1:4; s = s + 1; end; end; s;"), 12.0);
}

// ============================================================
// Phase 3: colon expressions
// ============================================================

TEST_F(VMTest, ColonBasic)
{
    auto result = run("x = 1:5;");
    EXPECT_EQ(result.numel(), 5u);
    EXPECT_DOUBLE_EQ(result.doubleData()[0], 1.0);
    EXPECT_DOUBLE_EQ(result.doubleData()[4], 5.0);
}

TEST_F(VMTest, ColonStep)
{
    auto result = run("x = 0:0.5:2;");
    EXPECT_EQ(result.numel(), 5u);
    EXPECT_DOUBLE_EQ(result.doubleData()[0], 0.0);
    EXPECT_DOUBLE_EQ(result.doubleData()[4], 2.0);
}

TEST_F(VMTest, ColonEmpty)
{
    auto result = run("x = 10:1;");
    EXPECT_EQ(result.numel(), 0u);
}

// ============================================================
// Phase 3: matrix literals
// ============================================================

TEST_F(VMTest, RowVector)
{
    auto result = run("x = [1, 2, 3];");
    EXPECT_EQ(result.numel(), 3u);
    EXPECT_DOUBLE_EQ(result.doubleData()[0], 1.0);
    EXPECT_DOUBLE_EQ(result.doubleData()[2], 3.0);
}

TEST_F(VMTest, ColumnVector)
{
    auto result = run("x = [1; 2; 3];");
    EXPECT_EQ(result.dims().rows(), 3u);
    EXPECT_EQ(result.dims().cols(), 1u);
}

TEST_F(VMTest, Matrix2x3)
{
    auto result = run("x = [1, 2, 3; 4, 5, 6];");
    EXPECT_EQ(result.dims().rows(), 2u);
    EXPECT_EQ(result.dims().cols(), 3u);
    // Column-major: x(1,1)=1, x(2,1)=4, x(1,2)=2, x(2,2)=5, ...
    const double *d = result.doubleData();
    EXPECT_DOUBLE_EQ(d[0], 1.0); // (1,1)
    EXPECT_DOUBLE_EQ(d[1], 4.0); // (2,1)
    EXPECT_DOUBLE_EQ(d[2], 2.0); // (1,2)
    EXPECT_DOUBLE_EQ(d[3], 5.0); // (2,2)
}

TEST_F(VMTest, EmptyMatrix)
{
    auto result = run("x = [];");
    EXPECT_TRUE(result.isEmpty());
}

// ============================================================
// Phase 3: array indexing
// ============================================================

TEST_F(VMTest, IndexGet1D)
{
    EXPECT_DOUBLE_EQ(runScalar("x = [10, 20, 30]; x(2);"), 20.0);
}

TEST_F(VMTest, IndexSet1D)
{
    EXPECT_DOUBLE_EQ(runScalar("x = [10, 20, 30]; x(2) = 99; x(2);"), 99.0);
}

TEST_F(VMTest, IndexGet2D)
{
    EXPECT_DOUBLE_EQ(runScalar("x = [1, 2, 3; 4, 5, 6]; x(2, 3);"), 6.0);
}

TEST_F(VMTest, IndexSet2D)
{
    EXPECT_DOUBLE_EQ(runScalar("x = [1, 2, 3; 4, 5, 6]; x(1, 2) = 99; x(1, 2);"), 99.0);
}

TEST_F(VMTest, IndexAutoGrow)
{
    EXPECT_DOUBLE_EQ(runScalar("x = [1, 2, 3]; x(5) = 10; x(5);"), 10.0);
}

// ============================================================
// Phase 3: combined for + array
// ============================================================

TEST_F(VMTest, ForOverArray)
{
    // for i = [10, 20, 30] iterates over columns
    EXPECT_DOUBLE_EQ(runScalar("s = 0; for i = [10, 20, 30]; s = s + i; end; s;"), 60.0);
}

TEST_F(VMTest, ForFillArray)
{
    auto result = run("x = [0, 0, 0, 0, 0]; for i = 1:5; x(i) = i * i; end; x;");
    EXPECT_EQ(result.numel(), 5u);
    const double *d = result.doubleData();
    EXPECT_DOUBLE_EQ(d[0], 1.0);
    EXPECT_DOUBLE_EQ(d[1], 4.0);
    EXPECT_DOUBLE_EQ(d[2], 9.0);
    EXPECT_DOUBLE_EQ(d[3], 16.0);
    EXPECT_DOUBLE_EQ(d[4], 25.0);
}

// ============================================================
// Phase 4: builtin function calls
// ============================================================

TEST_F(VMTest, CallFloor)
{
    EXPECT_DOUBLE_EQ(runScalar("floor(3.7);"), 3.0);
}

TEST_F(VMTest, CallCeil)
{
    EXPECT_DOUBLE_EQ(runScalar("ceil(3.2);"), 4.0);
}

TEST_F(VMTest, CallAbs)
{
    EXPECT_DOUBLE_EQ(runScalar("abs(-5);"), 5.0);
}

TEST_F(VMTest, CallSqrt)
{
    EXPECT_DOUBLE_EQ(runScalar("sqrt(16);"), 4.0);
}

TEST_F(VMTest, CallMod)
{
    EXPECT_DOUBLE_EQ(runScalar("mod(7, 3);"), 1.0);
}

TEST_F(VMTest, CallMax)
{
    EXPECT_DOUBLE_EQ(runScalar("max(3, 7);"), 7.0);
}

TEST_F(VMTest, CallMin)
{
    EXPECT_DOUBLE_EQ(runScalar("min(3, 7);"), 3.0);
}

TEST_F(VMTest, CallZeros)
{
    auto result = run("x = zeros(1, 3);");
    EXPECT_EQ(result.numel(), 3u);
    EXPECT_DOUBLE_EQ(result.doubleData()[0], 0.0);
}

TEST_F(VMTest, CallOnes)
{
    auto result = run("x = ones(1, 4);");
    EXPECT_EQ(result.numel(), 4u);
    EXPECT_DOUBLE_EQ(result.doubleData()[3], 1.0);
}

TEST_F(VMTest, CallLength)
{
    EXPECT_DOUBLE_EQ(runScalar("length([1, 2, 3, 4]);"), 4.0);
}

TEST_F(VMTest, CallNested)
{
    EXPECT_DOUBLE_EQ(runScalar("abs(floor(-3.7));"), 4.0);
}

TEST_F(VMTest, CallInExpr)
{
    EXPECT_DOUBLE_EQ(runScalar("x = 2 + sqrt(9);"), 5.0);
}

TEST_F(VMTest, CallInLoop)
{
    EXPECT_DOUBLE_EQ(runScalar("s = 0; for i = 1:5; s = s + abs(3 - i); end; s;"), 6.0);
}

// ============================================================
// Phase 5a: user-defined functions
// ============================================================

TEST_F(VMTest, UserFuncSimple)
{
    EXPECT_DOUBLE_EQ(runScalar("function r = double_it(x); r = x * 2; end; double_it(5);"), 10.0);
}

TEST_F(VMTest, UserFuncNoArgs)
{
    EXPECT_DOUBLE_EQ(runScalar("function r = seven(); r = 7; end; seven();"), 7.0);
}

TEST_F(VMTest, UserFuncMultipleArgs)
{
    EXPECT_DOUBLE_EQ(runScalar("function r = add3(a, b, c); r = a + b + c; end; add3(1, 2, 3);"),
                     6.0);
}

TEST_F(VMTest, UserFuncWithLocals)
{
    EXPECT_DOUBLE_EQ(runScalar(
                         "function r = hyp(a, b); c2 = a*a + b*b; r = sqrt(c2); end; hyp(3, 4);"),
                     5.0);
}

TEST_F(VMTest, UserFuncCalledInLoop)
{
    EXPECT_DOUBLE_EQ(
        runScalar("function r = sq(x); r = x * x; end; s = 0; for i = 1:5; s = s + sq(i); end; s;"),
        55.0);
}

TEST_F(VMTest, UserFuncCalledMultipleTimes)
{
    EXPECT_DOUBLE_EQ(
        runScalar("function r = inc(x); r = x + 1; end; a = inc(1); b = inc(a); c = inc(b); c;"),
        4.0);
}

TEST_F(VMTest, UserFuncRecursiveFib)
{
    EXPECT_DOUBLE_EQ(runScalar("function r = fib(n); if n <= 1; r = n; else; r = fib(n-1) + "
                               "fib(n-2); end; end; fib(10);"),
                     55.0);
}

TEST_F(VMTest, UserFuncRecursiveFactorial)
{
    EXPECT_DOUBLE_EQ(
        runScalar(
            "function r = fact(n); if n <= 1; r = 1; else; r = n * fact(n-1); end; end; fact(6);"),
        720.0);
}

TEST_F(VMTest, UserFuncWithReturn)
{
    EXPECT_DOUBLE_EQ(
        runScalar(
            "function r = early(x); r = 0; if x > 0; r = x; return; end; r = -1; end; early(5);"),
        5.0);
}

TEST_F(VMTest, UserFuncCallsBuiltin)
{
    EXPECT_DOUBLE_EQ(runScalar("function r = myabs(x); r = abs(x); end; myabs(-42);"), 42.0);
}