// tests/engine_test.cpp — Тесты для MLabEngine
// Проверяют runtime-поведение интерпретатора

#include "MLabEngine.hpp"
#include "MLabStdLibrary.hpp"
#include <cmath>
#include <gtest/gtest.h>
#include <string>
#include <vector>

using namespace mlab;

// ============================================================
// Test fixture — создаёт engine с StdLibrary для каждого теста
// ============================================================

class EngineTest : public ::testing::Test
{
public:
    Engine engine;
    std::string capturedOutput;

    void SetUp() override
    {
        StdLibrary::install(engine);
        capturedOutput.clear();
        engine.setOutputFunc([this](const std::string &s) { capturedOutput += s; });
    }

    MValue eval(const std::string &code) { return engine.eval(code); }

    double evalScalar(const std::string &code) { return eval(code).toScalar(); }

    bool evalBool(const std::string &code) { return eval(code).toBool(); }

    std::string evalString(const std::string &code) { return eval(code).toString(); }

    double getVar(const std::string &name) { return engine.getVariable(name)->toScalar(); }

    MValue *getVarPtr(const std::string &name) { return engine.getVariable(name); }
};

// ============================================================
// Базовые литералы и арифметика
// ============================================================

class EngineArithmeticTest : public EngineTest
{};

TEST_F(EngineArithmeticTest, IntegerLiteral)
{
    EXPECT_DOUBLE_EQ(evalScalar("42;"), 42.0);
}

TEST_F(EngineArithmeticTest, FloatLiteral)
{
    EXPECT_DOUBLE_EQ(evalScalar("3.14;"), 3.14);
}

TEST_F(EngineArithmeticTest, Addition)
{
    EXPECT_DOUBLE_EQ(evalScalar("2 + 3;"), 5.0);
}

TEST_F(EngineArithmeticTest, Subtraction)
{
    EXPECT_DOUBLE_EQ(evalScalar("10 - 7;"), 3.0);
}

TEST_F(EngineArithmeticTest, Multiplication)
{
    EXPECT_DOUBLE_EQ(evalScalar("4 * 5;"), 20.0);
}

TEST_F(EngineArithmeticTest, Division)
{
    EXPECT_DOUBLE_EQ(evalScalar("15 / 3;"), 5.0);
}

TEST_F(EngineArithmeticTest, Power)
{
    EXPECT_DOUBLE_EQ(evalScalar("2 ^ 10;"), 1024.0);
}

TEST_F(EngineArithmeticTest, UnaryMinus)
{
    EXPECT_DOUBLE_EQ(evalScalar("-5;"), -5.0);
}

TEST_F(EngineArithmeticTest, UnaryPlus)
{
    // FIX #2: unary + теперь работает (identity)
    EXPECT_DOUBLE_EQ(evalScalar("+5;"), 5.0);
}

TEST_F(EngineArithmeticTest, UnaryPlusOnVariable)
{
    eval("x = 42;");
    EXPECT_DOUBLE_EQ(evalScalar("+x;"), 42.0);
}

TEST_F(EngineArithmeticTest, Precedence)
{
    EXPECT_DOUBLE_EQ(evalScalar("2 + 3 * 4;"), 14.0);
    EXPECT_DOUBLE_EQ(evalScalar("(2 + 3) * 4;"), 20.0);
}

TEST_F(EngineArithmeticTest, NegPowerPrecedence)
{
    // -2^2 = -(2^2) = -4
    EXPECT_DOUBLE_EQ(evalScalar("-2^2;"), -4.0);
}

TEST_F(EngineArithmeticTest, PowerRightAssoc)
{
    // 2^3^2 = 2^(3^2) = 2^9 = 512
    EXPECT_DOUBLE_EQ(evalScalar("2^3^2;"), 512.0);
}

TEST_F(EngineArithmeticTest, HexLiteral)
{
    EXPECT_DOUBLE_EQ(evalScalar("0xFF;"), 255.0);
}

TEST_F(EngineArithmeticTest, BinaryLiteral)
{
    // FIX #2 (parser): 0b1010 = 10
    EXPECT_DOUBLE_EQ(evalScalar("0b1010;"), 10.0);
}

TEST_F(EngineArithmeticTest, OctalLiteral)
{
    EXPECT_DOUBLE_EQ(evalScalar("0o17;"), 15.0);
}

TEST_F(EngineArithmeticTest, UnderscoreLiteral)
{
    EXPECT_DOUBLE_EQ(evalScalar("1_000;"), 1000.0);
}

TEST_F(EngineArithmeticTest, ImaginaryLiteral)
{
    auto v = eval("3i;");
    EXPECT_TRUE(v.isComplex());
    EXPECT_DOUBLE_EQ(v.toComplex().imag(), 3.0);
    EXPECT_DOUBLE_EQ(v.toComplex().real(), 0.0);
}

TEST_F(EngineArithmeticTest, ComplexArithmetic)
{
    auto v = eval("(2 + 3i) + (1 - 1i);");
    EXPECT_DOUBLE_EQ(v.toComplex().real(), 3.0);
    EXPECT_DOUBLE_EQ(v.toComplex().imag(), 2.0);
}

// ============================================================
// Логические операторы — FIX #3, #4
// ============================================================

class EngineLogicalTest : public EngineTest
{};

TEST_F(EngineLogicalTest, ScalarAnd)
{
    EXPECT_TRUE(evalBool("true & true;"));
    EXPECT_FALSE(evalBool("true & false;"));
}

TEST_F(EngineLogicalTest, ScalarOr)
{
    EXPECT_TRUE(evalBool("true | false;"));
    EXPECT_FALSE(evalBool("false | false;"));
}

TEST_F(EngineLogicalTest, ShortCircuitAnd)
{
    EXPECT_TRUE(evalBool("true && true;"));
    EXPECT_FALSE(evalBool("true && false;"));
}

TEST_F(EngineLogicalTest, ShortCircuitOr)
{
    EXPECT_TRUE(evalBool("false || true;"));
    EXPECT_FALSE(evalBool("false || false;"));
}

TEST_F(EngineLogicalTest, ElementWiseAndArray)
{
    // FIX #3: [1 0 1] & [1 1 0] → logical [1 0 0]
    eval("r = [1 0 1] & [1 1 0];");
    auto *r = getVarPtr("r");
    ASSERT_TRUE(r != nullptr);
    EXPECT_TRUE(r->isLogical());
    ASSERT_EQ(r->numel(), 3u);
    const uint8_t *d = r->logicalData();
    EXPECT_EQ(d[0], 1);
    EXPECT_EQ(d[1], 0);
    EXPECT_EQ(d[2], 0);
}

TEST_F(EngineLogicalTest, ElementWiseOrArray)
{
    // FIX #3: [1 0 1] | [0 1 0] → logical [1 1 1]
    eval("r = [1 0 1] | [0 1 0];");
    auto *r = getVarPtr("r");
    ASSERT_TRUE(r != nullptr);
    EXPECT_TRUE(r->isLogical());
    ASSERT_EQ(r->numel(), 3u);
    const uint8_t *d = r->logicalData();
    EXPECT_EQ(d[0], 1);
    EXPECT_EQ(d[1], 1);
    EXPECT_EQ(d[2], 1);
}

TEST_F(EngineLogicalTest, ElementWiseAndScalarBroadcast)
{
    // scalar & array → broadcast
    eval("r = 1 & [1 0 1];");
    auto *r = getVarPtr("r");
    ASSERT_TRUE(r != nullptr);
    EXPECT_EQ(r->numel(), 3u);
    const uint8_t *d = r->logicalData();
    EXPECT_EQ(d[0], 1);
    EXPECT_EQ(d[1], 0);
    EXPECT_EQ(d[2], 1);
}

TEST_F(EngineLogicalTest, ElementWiseNotArray)
{
    // FIX #4: ~[1 0 1] → logical [0 1 0]
    eval("r = ~[1 0 1];");
    auto *r = getVarPtr("r");
    ASSERT_TRUE(r != nullptr);
    EXPECT_TRUE(r->isLogical());
    ASSERT_EQ(r->numel(), 3u);
    const uint8_t *d = r->logicalData();
    EXPECT_EQ(d[0], 0);
    EXPECT_EQ(d[1], 1);
    EXPECT_EQ(d[2], 0);
}

TEST_F(EngineLogicalTest, ElementWiseNotScalar)
{
    EXPECT_TRUE(evalBool("~false;"));
    EXPECT_FALSE(evalBool("~true;"));
}

TEST_F(EngineLogicalTest, PrecedenceAllFourLevels)
{
    // a || b && c | d & e
    // С правильными приоритетами: || < && < | < &
    // 1 || 0 && 0 | 0 & 1 = 1 || (0 && (0 | (0 & 1))) = 1 || (0 && (0 | 0)) = 1 || 0 = 1
    EXPECT_TRUE(evalBool("1 || 0 && 0 | 0 & 1;"));

    // 0 || 1 && 1 | 0 & 0 = 0 || (1 && (1 | (0 & 0))) = 0 || (1 && (1 | 0)) = 0 || (1 && 1) = 1
    EXPECT_TRUE(evalBool("0 || 1 && 1 | 0 & 0;"));
}

// ============================================================
// Присваивание и переменные
// ============================================================

class EngineAssignTest : public EngineTest
{};

TEST_F(EngineAssignTest, SimpleAssign)
{
    eval("x = 42;");
    EXPECT_DOUBLE_EQ(getVar("x"), 42.0);
}

TEST_F(EngineAssignTest, ChainedAssign)
{
    eval("x = 2; y = x + 3;");
    EXPECT_DOUBLE_EQ(getVar("y"), 5.0);
}

TEST_F(EngineAssignTest, FieldAssign)
{
    eval("s.name = 'hello';");
    auto *s = getVarPtr("s");
    ASSERT_TRUE(s != nullptr);
    EXPECT_TRUE(s->isStruct());
    EXPECT_EQ(s->field("name").toString(), "hello");
}

TEST_F(EngineAssignTest, NestedFieldAssign)
{
    eval("s.a.b = 42;");
    auto *s = getVarPtr("s");
    EXPECT_DOUBLE_EQ(s->field("a").field("b").toScalar(), 42.0);
}

TEST_F(EngineAssignTest, IndexedAssign)
{
    eval("A = zeros(3,3); A(2,2) = 99;");
    auto *A = getVarPtr("A");
    EXPECT_DOUBLE_EQ((*A)(1, 1), 99.0);
    EXPECT_DOUBLE_EQ((*A)(0, 0), 0.0);
}

TEST_F(EngineAssignTest, EmptyMatrixAssign)
{
    // FIX #12 (parser): x = [] creates ASSIGN(x, MATRIX_LITERAL[])
    eval("x = [];");
    auto *x = getVarPtr("x");
    ASSERT_TRUE(x != nullptr);
    EXPECT_TRUE(x->isEmpty());
}

// ============================================================
// Multi-assign и tilde — FIX #6
// ============================================================

class EngineMultiAssignTest : public EngineTest
{};

TEST_F(EngineMultiAssignTest, BasicMultiAssign)
{
    // User-defined function that returns multiple values
    eval("function [a, b] = myfun()\n  a = 10;\n  b = 20;\nend");
    eval("[m, n] = myfun();");
    EXPECT_DOUBLE_EQ(getVar("m"), 10.0);
    EXPECT_DOUBLE_EQ(getVar("n"), 20.0);
}

TEST_F(EngineMultiAssignTest, TildeIgnoresOutput)
{
    // FIX #6: ~ не создаёт переменную
    eval("function [a, b] = myfun()\n  a = 10;\n  b = 20;\nend");
    eval("[~, n] = myfun();");
    EXPECT_DOUBLE_EQ(getVar("n"), 20.0);
    // Переменная ~ не должна существовать
    EXPECT_EQ(getVarPtr("~"), nullptr);
}

// ============================================================
// Матрицы
// ============================================================

class EngineMatrixTest : public EngineTest
{};

TEST_F(EngineMatrixTest, RowVector)
{
    eval("v = [1 2 3];");
    auto *v = getVarPtr("v");
    EXPECT_EQ(v->dims().rows(), 1u);
    EXPECT_EQ(v->dims().cols(), 3u);
}

TEST_F(EngineMatrixTest, ColumnVector)
{
    eval("v = [1; 2; 3];");
    auto *v = getVarPtr("v");
    EXPECT_EQ(v->dims().rows(), 3u);
    EXPECT_EQ(v->dims().cols(), 1u);
}

TEST_F(EngineMatrixTest, Matrix2x3)
{
    eval("M = [1 2 3; 4 5 6];");
    auto *M = getVarPtr("M");
    EXPECT_EQ(M->dims().rows(), 2u);
    EXPECT_EQ(M->dims().cols(), 3u);
    EXPECT_DOUBLE_EQ((*M)(0, 0), 1.0);
    EXPECT_DOUBLE_EQ((*M)(1, 2), 6.0);
}

TEST_F(EngineMatrixTest, Transpose)
{
    eval("v = [1 2 3]; w = v';");
    auto *w = getVarPtr("w");
    EXPECT_EQ(w->dims().rows(), 3u);
    EXPECT_EQ(w->dims().cols(), 1u);
    EXPECT_DOUBLE_EQ((*w)(2, 0), 3.0);
}

TEST_F(EngineMatrixTest, MatrixMultiply)
{
    eval("A = [1 2; 3 4]; B = [5; 6]; C = A * B;");
    auto *C = getVarPtr("C");
    EXPECT_EQ(C->dims().rows(), 2u);
    EXPECT_EQ(C->dims().cols(), 1u);
    EXPECT_DOUBLE_EQ((*C)(0, 0), 17.0); // 1*5+2*6
    EXPECT_DOUBLE_EQ((*C)(1, 0), 39.0); // 3*5+4*6
}

TEST_F(EngineMatrixTest, ElementWiseMul)
{
    eval("r = [1 2 3] .* [4 5 6];");
    auto *r = getVarPtr("r");
    EXPECT_DOUBLE_EQ(r->doubleData()[0], 4.0);
    EXPECT_DOUBLE_EQ(r->doubleData()[1], 10.0);
    EXPECT_DOUBLE_EQ(r->doubleData()[2], 18.0);
}

TEST_F(EngineMatrixTest, ElementWisePow)
{
    eval("r = [2 3] .^ [3 2];");
    auto *r = getVarPtr("r");
    EXPECT_DOUBLE_EQ(r->doubleData()[0], 8.0);
    EXPECT_DOUBLE_EQ(r->doubleData()[1], 9.0);
}

TEST_F(EngineMatrixTest, StringConcatInMatrix)
{
    eval("s = ['hello' ' ' 'world'];");
    auto *s = getVarPtr("s");
    EXPECT_EQ(s->toString(), "hello world");
}

// ============================================================
// Bounds checking — FIX #7
// ============================================================

class EngineBoundsTest : public EngineTest
{};

TEST_F(EngineBoundsTest, OutOfBoundsLinear)
{
    eval("v = [1 2 3];");
    EXPECT_THROW(eval("v(5);"), std::runtime_error);
}

TEST_F(EngineBoundsTest, OutOfBoundsRow)
{
    eval("A = [1 2; 3 4];");
    EXPECT_THROW(eval("A(3, 1);"), std::runtime_error);
}

TEST_F(EngineBoundsTest, OutOfBoundsCol)
{
    eval("A = [1 2; 3 4];");
    EXPECT_THROW(eval("A(1, 5);"), std::runtime_error);
}

TEST_F(EngineBoundsTest, ValidBoundsOK)
{
    eval("v = [10 20 30];");
    EXPECT_DOUBLE_EQ(evalScalar("v(3);"), 30.0);
}

TEST_F(EngineBoundsTest, IndexZeroError)
{
    eval("v = [1 2 3];");
    EXPECT_THROW(eval("v(0);"), std::runtime_error);
}

// ============================================================
// Chain calls — FIX #1
// ============================================================

class EngineChainCallTest : public EngineTest
{};

TEST_F(EngineChainCallTest, FuncHandleCall)
{
    eval("f = @sin; r = f(0);");
    EXPECT_NEAR(getVar("r"), 0.0, 1e-12);
}

TEST_F(EngineChainCallTest, FuncHandleCallPi)
{
    eval("f = @cos; r = f(pi);");
    EXPECT_NEAR(getVar("r"), -1.0, 1e-12);
}

TEST_F(EngineChainCallTest, AnonFuncCall)
{
    eval("f = @(x) x^2; r = f(5);");
    EXPECT_DOUBLE_EQ(getVar("r"), 25.0);
}

TEST_F(EngineChainCallTest, AnonFuncWithClosure)
{
    eval("a = 10; f = @(x) x + a; r = f(5);");
    EXPECT_DOUBLE_EQ(getVar("r"), 15.0);
}

// ============================================================
// Colon expressions
// ============================================================

class EngineColonTest : public EngineTest
{};

TEST_F(EngineColonTest, SimpleRange)
{
    eval("v = 1:5;");
    auto *v = getVarPtr("v");
    EXPECT_EQ(v->numel(), 5u);
    EXPECT_DOUBLE_EQ(v->doubleData()[0], 1.0);
    EXPECT_DOUBLE_EQ(v->doubleData()[4], 5.0);
}

TEST_F(EngineColonTest, SteppedRange)
{
    eval("v = 0:2:10;");
    auto *v = getVarPtr("v");
    EXPECT_EQ(v->numel(), 6u);
    EXPECT_DOUBLE_EQ(v->doubleData()[0], 0.0);
    EXPECT_DOUBLE_EQ(v->doubleData()[5], 10.0);
}

TEST_F(EngineColonTest, NegativeStep)
{
    eval("v = 5:-1:1;");
    auto *v = getVarPtr("v");
    EXPECT_EQ(v->numel(), 5u);
    EXPECT_DOUBLE_EQ(v->doubleData()[0], 5.0);
    EXPECT_DOUBLE_EQ(v->doubleData()[4], 1.0);
}

TEST_F(EngineColonTest, EmptyRange)
{
    eval("v = 5:1;");
    auto *v = getVarPtr("v");
    EXPECT_EQ(v->numel(), 0u);
}

TEST_F(EngineColonTest, ColonIndexing)
{
    eval("A = [1 2 3; 4 5 6]; r = A(:, 2);");
    auto *r = getVarPtr("r");
    EXPECT_EQ(r->dims().rows(), 2u);
    EXPECT_EQ(r->dims().cols(), 1u);
    EXPECT_DOUBLE_EQ((*r)(0, 0), 2.0);
    EXPECT_DOUBLE_EQ((*r)(1, 0), 5.0);
}

// ============================================================
// Control flow
// ============================================================

class EngineControlFlowTest : public EngineTest
{};

TEST_F(EngineControlFlowTest, IfTrue)
{
    eval("x = 0; if true, x = 1; end");
    EXPECT_DOUBLE_EQ(getVar("x"), 1.0);
}

TEST_F(EngineControlFlowTest, IfFalse)
{
    eval("x = 0; if false, x = 1; end");
    EXPECT_DOUBLE_EQ(getVar("x"), 0.0);
}

TEST_F(EngineControlFlowTest, IfElse)
{
    eval("x = 5; if x > 10, y = 1; else, y = 2; end");
    EXPECT_DOUBLE_EQ(getVar("y"), 2.0);
}

TEST_F(EngineControlFlowTest, IfElseif)
{
    eval("x = 5; if x > 10, y = 1; elseif x > 3, y = 2; else, y = 3; end");
    EXPECT_DOUBLE_EQ(getVar("y"), 2.0);
}

TEST_F(EngineControlFlowTest, ForLoop)
{
    eval("s = 0; for i = 1:5, s = s + i; end");
    EXPECT_DOUBLE_EQ(getVar("s"), 15.0);
}

TEST_F(EngineControlFlowTest, ForBreak)
{
    eval("s = 0; for i = 1:10\n  if i > 3, break; end\n  s = s + i;\nend");
    EXPECT_DOUBLE_EQ(getVar("s"), 6.0); // 1+2+3
}

TEST_F(EngineControlFlowTest, ForContinue)
{
    eval("s = 0; for i = 1:5\n  if mod(i, 2) == 0, continue; end\n  s = s + i;\nend");
    EXPECT_DOUBLE_EQ(getVar("s"), 9.0); // 1+3+5
}

TEST_F(EngineControlFlowTest, WhileLoop)
{
    eval("x = 1; while x < 100, x = x * 2; end");
    EXPECT_DOUBLE_EQ(getVar("x"), 128.0);
}

TEST_F(EngineControlFlowTest, SwitchCase)
{
    eval("x = 2; switch x\n  case 1, y = 10;\n  case 2, y = 20;\n  otherwise, y = 0;\nend");
    EXPECT_DOUBLE_EQ(getVar("y"), 20.0);
}

TEST_F(EngineControlFlowTest, SwitchOtherwise)
{
    eval("x = 99; switch x\n  case 1, y = 10;\n  otherwise, y = -1;\nend");
    EXPECT_DOUBLE_EQ(getVar("y"), -1.0);
}

TEST_F(EngineControlFlowTest, SwitchCellCase)
{
    eval("x = 2; switch x\n  case {1, 2, 3}, y = 1;\n  otherwise, y = 0;\nend");
    EXPECT_DOUBLE_EQ(getVar("y"), 1.0);
}

TEST_F(EngineControlFlowTest, TryCatch)
{
    eval("try\n  error('oops');\ncatch e\n  msg = e.message;\nend");
    auto *msg = getVarPtr("msg");
    EXPECT_EQ(msg->toString(), "oops");
}

TEST_F(EngineControlFlowTest, TryNoCatch)
{
    // try без catch — ошибка игнорируется
    eval("try\n  error('oops');\nend");
    // Не должно бросить исключение
}

// ============================================================
// Functions
// ============================================================

class EngineFunctionTest : public EngineTest
{};

TEST_F(EngineFunctionTest, SimpleFunction)
{
    eval("function y = square(x)\n  y = x^2;\nend");
    eval("r = square(7);");
    EXPECT_DOUBLE_EQ(getVar("r"), 49.0);
}

TEST_F(EngineFunctionTest, FunctionMultiReturn)
{
    eval("function [mn, mx] = minmax(v)\n  mn = min(v);\n  mx = max(v);\nend");
    eval("[a, b] = minmax([3 1 4 1 5]);");
    EXPECT_DOUBLE_EQ(getVar("a"), 1.0);
    EXPECT_DOUBLE_EQ(getVar("b"), 5.0);
}

TEST_F(EngineFunctionTest, RecursiveFunction)
{
    eval("function n = fact(x)\n  if x <= 1, n = 1; else, n = x * fact(x-1); end\nend");
    eval("r = fact(6);");
    EXPECT_DOUBLE_EQ(getVar("r"), 720.0);
}

TEST_F(EngineFunctionTest, AnonFunction)
{
    eval("f = @(x, y) x + y;");
    eval("r = f(3, 4);");
    EXPECT_DOUBLE_EQ(getVar("r"), 7.0);
}

TEST_F(EngineFunctionTest, FunctionHandle)
{
    eval("f = @sin; r = f(pi/2);");
    EXPECT_NEAR(getVar("r"), 1.0, 1e-12);
}

// ============================================================
// Built-in functions
// ============================================================

class EngineBuiltinTest : public EngineTest
{};

TEST_F(EngineBuiltinTest, Zeros)
{
    eval("A = zeros(2, 3);");
    auto *A = getVarPtr("A");
    EXPECT_EQ(A->dims().rows(), 2u);
    EXPECT_EQ(A->dims().cols(), 3u);
    for (size_t i = 0; i < A->numel(); ++i)
        EXPECT_DOUBLE_EQ(A->doubleData()[i], 0.0);
}

TEST_F(EngineBuiltinTest, Ones)
{
    eval("A = ones(2, 2);");
    auto *A = getVarPtr("A");
    for (size_t i = 0; i < A->numel(); ++i)
        EXPECT_DOUBLE_EQ(A->doubleData()[i], 1.0);
}

TEST_F(EngineBuiltinTest, Eye)
{
    eval("I = eye(3);");
    auto *I = getVarPtr("I");
    EXPECT_DOUBLE_EQ((*I)(0, 0), 1.0);
    EXPECT_DOUBLE_EQ((*I)(1, 1), 1.0);
    EXPECT_DOUBLE_EQ((*I)(0, 1), 0.0);
}

TEST_F(EngineBuiltinTest, Size)
{
    eval("A = ones(3, 5);");
    eval("r = size(A, 1);");
    EXPECT_DOUBLE_EQ(getVar("r"), 3.0);
    eval("c = size(A, 2);");
    EXPECT_DOUBLE_EQ(getVar("c"), 5.0);
}

TEST_F(EngineBuiltinTest, Length)
{
    eval("v = [1 2 3 4 5]; l = length(v);");
    EXPECT_DOUBLE_EQ(getVar("l"), 5.0);
}

TEST_F(EngineBuiltinTest, Numel)
{
    eval("A = ones(3, 4); n = numel(A);");
    EXPECT_DOUBLE_EQ(getVar("n"), 12.0);
}

TEST_F(EngineBuiltinTest, Sum)
{
    eval("r = sum([1 2 3 4 5]);");
    EXPECT_DOUBLE_EQ(getVar("r"), 15.0);
}

TEST_F(EngineBuiltinTest, MinMax)
{
    eval("function [a, b] = mymin(v)\n  a = min(v);\n  b = 0;\n  for i = 1:length(v)\n    if v(i) "
         "== a, b = i; break; end\n  end\nend");
    eval("[mn, mi] = mymin([3 1 4 1 5]);");
    EXPECT_DOUBLE_EQ(getVar("mn"), 1.0);
    EXPECT_DOUBLE_EQ(getVar("mi"), 2.0);
}

TEST_F(EngineBuiltinTest, MathFunctions)
{
    EXPECT_NEAR(evalScalar("sin(pi/2);"), 1.0, 1e-12);
    EXPECT_NEAR(evalScalar("cos(0);"), 1.0, 1e-12);
    EXPECT_NEAR(evalScalar("sqrt(16);"), 4.0, 1e-12);
    EXPECT_NEAR(evalScalar("abs(-5);"), 5.0, 1e-12);
    EXPECT_NEAR(evalScalar("exp(0);"), 1.0, 1e-12);
    EXPECT_NEAR(evalScalar("log(1);"), 0.0, 1e-12);
}

TEST_F(EngineBuiltinTest, Floor)
{
    EXPECT_DOUBLE_EQ(evalScalar("floor(3.7);"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("floor(-3.2);"), -4.0);
}

TEST_F(EngineBuiltinTest, Mod)
{
    EXPECT_DOUBLE_EQ(evalScalar("mod(7, 3);"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("mod(10, 5);"), 0.0);
}

TEST_F(EngineBuiltinTest, Reshape)
{
    eval("A = [1 2 3 4 5 6]; B = reshape(A, 2, 3);");
    auto *B = getVarPtr("B");
    EXPECT_EQ(B->dims().rows(), 2u);
    EXPECT_EQ(B->dims().cols(), 3u);
}

TEST_F(EngineBuiltinTest, ErrorFunction)
{
    EXPECT_THROW(eval("error('test error');"), std::runtime_error);
}

TEST_F(EngineBuiltinTest, Strcmp)
{
    EXPECT_TRUE(evalBool("strcmp('hello', 'hello');"));
    EXPECT_FALSE(evalBool("strcmp('hello', 'world');"));
}

// ============================================================
// Cell arrays
// ============================================================

class EngineCellTest : public EngineTest
{};

TEST_F(EngineCellTest, CellCreate)
{
    eval("c = {1, 'hello', [1 2 3]};");
    auto *c = getVarPtr("c");
    EXPECT_TRUE(c->isCell());
    EXPECT_DOUBLE_EQ(c->cellAt(0).toScalar(), 1.0);
    EXPECT_EQ(c->cellAt(1).toString(), "hello");
    EXPECT_EQ(c->cellAt(2).numel(), 3u);
}

TEST_F(EngineCellTest, CellIndex)
{
    eval("c = {10, 20, 30}; r = c{2};");
    EXPECT_DOUBLE_EQ(getVar("r"), 20.0);
}

// ============================================================
// Struct
// ============================================================

class EngineStructTest : public EngineTest
{};

TEST_F(EngineStructTest, CreateAndAccess)
{
    eval("s.x = 1; s.y = 2;");
    auto *s = getVarPtr("s");
    EXPECT_DOUBLE_EQ(s->field("x").toScalar(), 1.0);
    EXPECT_DOUBLE_EQ(s->field("y").toScalar(), 2.0);
}

TEST_F(EngineStructTest, NestedStruct)
{
    eval("s.inner.val = 42;");
    auto *s = getVarPtr("s");
    EXPECT_DOUBLE_EQ(s->field("inner").field("val").toScalar(), 42.0);
}

// ============================================================
// Delete assign — FIX #12
// ============================================================

class EngineDeleteTest : public EngineTest
{};

TEST_F(EngineDeleteTest, DeleteElements)
{
    eval("v = [1 2 3 4 5]; v(3) = [];");
    auto *v = getVarPtr("v");
    EXPECT_EQ(v->numel(), 4u);
    EXPECT_DOUBLE_EQ(v->doubleData()[0], 1.0);
    EXPECT_DOUBLE_EQ(v->doubleData()[1], 2.0);
    EXPECT_DOUBLE_EQ(v->doubleData()[2], 4.0);
    EXPECT_DOUBLE_EQ(v->doubleData()[3], 5.0);
}

TEST_F(EngineDeleteTest, DeleteMultiple)
{
    eval("v = [1 2 3 4 5]; v([1 3 5]) = [];");
    auto *v = getVarPtr("v");
    EXPECT_EQ(v->numel(), 2u);
    EXPECT_DOUBLE_EQ(v->doubleData()[0], 2.0);
    EXPECT_DOUBLE_EQ(v->doubleData()[1], 4.0);
}

// ============================================================
// End keyword in indexing
// ============================================================

class EngineEndTest : public EngineTest
{};

TEST_F(EngineEndTest, EndInIndex)
{
    eval("v = [10 20 30]; r = v(end);");
    EXPECT_DOUBLE_EQ(getVar("r"), 30.0);
}

TEST_F(EngineEndTest, EndMinusOne)
{
    eval("v = [10 20 30]; r = v(end-1);");
    EXPECT_DOUBLE_EQ(getVar("r"), 20.0);
}

TEST_F(EngineEndTest, EndInRange)
{
    eval("v = [10 20 30 40 50]; r = v(2:end);");
    auto *r = getVarPtr("r");
    EXPECT_EQ(r->numel(), 4u);
    EXPECT_DOUBLE_EQ(r->doubleData()[0], 20.0);
}

// ============================================================
// Output / display
// ============================================================

class EngineDisplayTest : public EngineTest
{};

TEST_F(EngineDisplayTest, SuppressOutput)
{
    capturedOutput.clear();
    eval("42;");
    EXPECT_TRUE(capturedOutput.empty()); // semicolon suppresses output
}

TEST_F(EngineDisplayTest, ShowOutput)
{
    capturedOutput.clear();
    eval("42");
    EXPECT_FALSE(capturedOutput.empty()); // no semicolon → show "ans = 42"
    EXPECT_NE(capturedOutput.find("42"), std::string::npos);
}

// ============================================================
// Complex programs
// ============================================================

class EngineComplexProgramTest : public EngineTest
{};

TEST_F(EngineComplexProgramTest, Fibonacci)
{
    eval(R"(
        function y = fib(n)
            if n <= 1
                y = n;
            else
                y = fib(n-1) + fib(n-2);
            end
        end
    )");
    eval("r = fib(10);");
    EXPECT_DOUBLE_EQ(getVar("r"), 55.0);
}

TEST_F(EngineComplexProgramTest, BubbleSort)
{
    eval(R"(
        function v = bsort(v)
            n = length(v);
            for i = 1:n-1
                for j = 1:n-i
                    if v(j) > v(j+1)
                        tmp = v(j);
                        v(j) = v(j+1);
                        v(j+1) = tmp;
                    end
                end
            end
        end
    )");
    eval("r = bsort([5 3 1 4 2]);");
    auto *r = getVarPtr("r");
    ASSERT_EQ(r->numel(), 5u);
    for (size_t i = 0; i < 5; ++i)
        EXPECT_DOUBLE_EQ(r->doubleData()[i], static_cast<double>(i + 1));
}

TEST_F(EngineComplexProgramTest, MatrixOps)
{
    eval(R"(
        A = [1 2; 3 4];
        B = A';
        C = A * B;
        d = sum(sum(C));
    )");
    // A*A' = [1 2;3 4]*[1 3;2 4] = [5 11;11 25], sum = 52
    EXPECT_DOUBLE_EQ(getVar("d"), 52.0);
}

TEST_F(EngineComplexProgramTest, LogicalIndexing)
{
    eval("v = [1 2 3 4 5 6]; r = v(v > 3);");
    auto *r = getVarPtr("r");
    ASSERT_EQ(r->numel(), 3u);
    EXPECT_DOUBLE_EQ(r->doubleData()[0], 4.0);
    EXPECT_DOUBLE_EQ(r->doubleData()[1], 5.0);
    EXPECT_DOUBLE_EQ(r->doubleData()[2], 6.0);
}

TEST_F(EngineComplexProgramTest, GCD)
{
    eval(R"(
        function g = mygcd(a, b)
            while b ~= 0
                t = b;
                b = mod(a, b);
                a = t;
            end
            g = a;
        end
    )");
    eval("r = mygcd(48, 18);");
    EXPECT_DOUBLE_EQ(getVar("r"), 6.0);
}

// ============================================================
// Global / persistent
// ============================================================

class EngineGlobalTest : public EngineTest
{};

TEST_F(EngineGlobalTest, GlobalVariable)
{
    eval(R"(
        function setg()
            global g;
            g = 42;
        end
    )");
    eval("global g; setg();");
    EXPECT_DOUBLE_EQ(getVar("g"), 42.0);
}
