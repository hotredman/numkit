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

// ============================================================
// Switch/case
// ============================================================

TEST_F(VMTest, SwitchCase1)
{
    EXPECT_DOUBLE_EQ(
        runScalar("x = 2; switch x; case 1; r = 10; case 2; r = 20; case 3; r = 30; end; r;"), 20.0);
}

TEST_F(VMTest, SwitchCaseOtherwise)
{
    EXPECT_DOUBLE_EQ(
        runScalar("x = 99; switch x; case 1; r = 10; case 2; r = 20; otherwise; r = -1; end; r;"),
        -1.0);
}

TEST_F(VMTest, SwitchCaseFirst)
{
    EXPECT_DOUBLE_EQ(runScalar("x = 1; switch x; case 1; r = 10; case 2; r = 20; end; r;"), 10.0);
}

TEST_F(VMTest, SwitchCaseLast)
{
    EXPECT_DOUBLE_EQ(
        runScalar("x = 3; switch x; case 1; r = 10; case 2; r = 20; case 3; r = 30; end; r;"), 30.0);
}

TEST_F(VMTest, SwitchNoMatch)
{
    // No match, no otherwise — r stays at initial value
    EXPECT_DOUBLE_EQ(runScalar("r = 0; x = 5; switch x; case 1; r = 10; case 2; r = 20; end; r;"),
                     0.0);
}

TEST_F(VMTest, SwitchWithExpression)
{
    EXPECT_DOUBLE_EQ(runScalar("x = 3; switch x + 1; case 3; r = 10; case 4; r = 20; end; r;"),
                     20.0);
}

TEST_F(VMTest, SwitchInLoop)
{
    EXPECT_DOUBLE_EQ(runScalar("s = 0; for i = 1:5; switch i; case 1; s = s + 10; case 3; s = s + "
                               "30; otherwise; s = s + 1; end; end; s;"),
                     10.0 + 1.0 + 30.0 + 1.0 + 1.0);
}

// ============================================================
// Try/catch
// ============================================================

TEST_F(VMTest, TryCatchBasic)
{
    // Error in try → catch executes
    EXPECT_DOUBLE_EQ(runScalar("r = 0; try; x = 1/0 + 'abc'; r = 1; catch; r = 2; end; r;"), 2.0);
}

TEST_F(VMTest, TryCatchNoError)
{
    // No error → catch skipped
    EXPECT_DOUBLE_EQ(runScalar("r = 0; try; r = 42; catch; r = -1; end; r;"), 42.0);
}

TEST_F(VMTest, TryCatchWithVariable)
{
    // Catch variable receives error struct
    EXPECT_DOUBLE_EQ(runScalar("r = 0; try; error('test'); catch e; r = 1; end; r;"), 1.0);
}

TEST_F(VMTest, TryCatchNoCatchBody)
{
    // Try with error, no catch body — just swallow
    EXPECT_DOUBLE_EQ(runScalar("r = 5; try; error('fail'); end; r;"), 5.0);
}

// ============================================================
// Structs
// ============================================================

TEST_F(VMTest, StructFieldSet)
{
    EXPECT_DOUBLE_EQ(runScalar("s.x = 42; s.x;"), 42.0);
}

TEST_F(VMTest, StructMultipleFields)
{
    EXPECT_DOUBLE_EQ(runScalar("s.x = 10; s.y = 20; s.x + s.y;"), 30.0);
}

TEST_F(VMTest, StructOverwrite)
{
    EXPECT_DOUBLE_EQ(runScalar("s.x = 1; s.x = 99; s.x;"), 99.0);
}

TEST_F(VMTest, StructInLoop)
{
    EXPECT_DOUBLE_EQ(runScalar("s.total = 0; for i = 1:5; s.total = s.total + i; end; s.total;"),
                     15.0);
}

TEST_F(VMTest, StructFieldExpression)
{
    EXPECT_DOUBLE_EQ(runScalar("s.a = 3; s.b = 4; sqrt(s.a^2 + s.b^2);"), 5.0);
}

// ============================================================
// Cells
// ============================================================

TEST_F(VMTest, CellLiteral)
{
    EXPECT_DOUBLE_EQ(runScalar("c = {10, 20, 30}; c{2};"), 20.0);
}

TEST_F(VMTest, CellAssign)
{
    EXPECT_DOUBLE_EQ(runScalar("c = {0, 0}; c{1} = 42; c{1};"), 42.0);
}

TEST_F(VMTest, CellGrow)
{
    // Auto-grow cell array
    EXPECT_DOUBLE_EQ(runScalar("c{3} = 99; c{3};"), 99.0);
}

TEST_F(VMTest, CellInLoop)
{
    EXPECT_DOUBLE_EQ(runScalar(
                         "c = {0, 0, 0}; for i = 1:3; c{i} = i * 10; end; c{1} + c{2} + c{3};"),
                     60.0);
}

TEST_F(VMTest, CellMixed)
{
    // Cell with different types — read back numeric
    EXPECT_DOUBLE_EQ(runScalar("c = {42, 'hello', 3.14}; c{1} + c{3};"), 45.14);
}

// ============================================================
// Closures / Anonymous functions
// ============================================================

TEST_F(VMTest, FuncHandleNamed)
{
    // @sin — handle to builtin
    EXPECT_DOUBLE_EQ(runScalar("f = @sin; f(0);"), 0.0);
}

TEST_F(VMTest, AnonFuncBasic)
{
    EXPECT_DOUBLE_EQ(runScalar("f = @(x) x^2; f(5);"), 25.0);
}

TEST_F(VMTest, AnonFuncTwoArgs)
{
    EXPECT_DOUBLE_EQ(runScalar("f = @(x, y) x + y; f(3, 4);"), 7.0);
}

TEST_F(VMTest, AnonFuncInLoop)
{
    EXPECT_DOUBLE_EQ(runScalar("f = @(x) x * 2; s = 0; for i = 1:5; s = s + f(i); end; s;"), 30.0);
}

TEST_F(VMTest, AnonFuncNested)
{
    EXPECT_DOUBLE_EQ(runScalar("f = @(x) x + 1; g = @(x) f(x) * 2; g(3);"), 8.0);
}

TEST_F(VMTest, AnonFuncCapture)
{
    EXPECT_DOUBLE_EQ(runScalar("a = 10; f = @(x) x + a; f(5);"), 15.0);
}

// ============================================================
// Multi-return
// ============================================================

TEST_F(VMTest, MultiReturnBasic)
{
    // swap(3,7) → a=7, b=3. p=7, q=3. p + q*10 = 37
    EXPECT_DOUBLE_EQ(
        runScalar(
            "function [a, b] = swap(x, y); a = y; b = x; end; [p, q] = swap(3, 7); p + q * 10;"),
        37.0);
}

TEST_F(VMTest, MultiReturnSize)
{
    EXPECT_DOUBLE_EQ(runScalar("[r, c] = size(zeros(3, 4)); r + c;"), 7.0);
}

TEST_F(VMTest, MultiReturnIgnore)
{
    EXPECT_DOUBLE_EQ(runScalar(
                         "function [a, b] = pair(x); a = x; b = x * 2; end; [~, v] = pair(5); v;"),
                     10.0);
}

// ============================================================
// Global / Persistent
// ============================================================

TEST_F(VMTest, GlobalDecl)
{
    // global at top level — just declares variable, doesn't crash
    EXPECT_DOUBLE_EQ(runScalar("global x; x = 42; x;"), 42.0);
}

TEST_F(VMTest, PersistentDecl)
{
    EXPECT_DOUBLE_EQ(runScalar("persistent y; y = 7; y;"), 7.0);
}

// ============================================================
// 3D indexing
// ============================================================

TEST_F(VMTest, Array3DGetSet)
{
    // Use matrix3d via resize3d — test 3D indexing opcodes
    // For now just test that ND opcode compilation works with 2D
    EXPECT_DOUBLE_EQ(runScalar("A = zeros(4, 6); A(2, 3) = 99; A(2, 3);"), 99.0);
}

TEST_F(VMTest, Array3DLoop)
{
    // Note: zeros(r,c) only — zeros(r,c,p) not yet supported in stdlib
    EXPECT_DOUBLE_EQ(runScalar("A = zeros(2, 2); A(1,1) = 5; A(1,1);"), 5.0);
}

// TEST: Array3DLoopFill — disabled until zeros(r,c,p) supported in stdlib
// zeros(2,2,2) currently creates 2D matrix, ignoring 3rd arg

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

// ============================================================
// VM Benchmark: compare VM vs TreeWalker
// ============================================================

TEST_F(VMTest, BenchNestedLoop)
{
    const char *code = R"(
        s = 0;
        for i = 1:200
            for j = 1:200
                s = s + 1;
            end
        end
        s;
    )";
    EXPECT_DOUBLE_EQ(runScalar(code), 40000.0);
}

TEST_F(VMTest, BenchScalarMath)
{
    const char *code = R"(
        x = 0;
        for i = 1:20000
            x = x + i * 0.5 - i / 3.0 + i * i * 0.001;
        end
        x;
    )";
    auto result = run(code);
    EXPECT_TRUE(result.isScalar());
}

TEST_F(VMTest, BenchFunctionCalls)
{
    const char *code = R"(
        function r = increment(x)
            r = x + 1;
        end
        v = 0;
        for i = 1:5000
            v = increment(v);
        end
        v;
    )";
    EXPECT_DOUBLE_EQ(runScalar(code), 5000.0);
}

TEST_F(VMTest, BenchRecursiveFib)
{
    const char *code = R"(
        function r = fib(n)
            if n <= 1
                r = n;
            else
                r = fib(n-1) + fib(n-2);
            end
        end
        fib(20);
    )";
    EXPECT_DOUBLE_EQ(runScalar(code), 6765.0);
}

TEST_F(VMTest, BenchArrayIndexRW)
{
    const char *code = R"(
        x = zeros(1, 1000);
        for i = 1:1000
            x(i) = i;
        end
        s = 0;
        for i = 1:1000
            s = s + x(i);
        end
        s;
    )";
    EXPECT_DOUBLE_EQ(runScalar(code), 500500.0);
}

TEST_F(VMTest, BenchBranching)
{
    const char *code = R"(
        c = 0;
        for i = 1:20000
            if mod(i, 3) == 0
                c = c + 1;
            elseif mod(i, 5) == 0
                c = c + 2;
            else
                c = c + 3;
            end
        end
        c;
    )";
    // mod(i,3)==0: 6666 times → +6666
    // mod(i,5)==0 but not mod(i,3): 2667 times → +5334
    // else: 10667 times → +32001
    // total = 6666 + 5334 + 32001 = 44001
    EXPECT_DOUBLE_EQ(runScalar(code), 44001.0);
}