// tests/stdlib/integer_types_test.cpp — Integer/single type constructors, isequal
// Parameterized: runs on both TreeWalker and VM backends

#include "dual_engine_fixture.hpp"

using namespace mlab_test;

class IntegerTypesTest : public DualEngineTest {};

// ── Constructor basics ──────────────────────────────────────

TEST_P(IntegerTypesTest, Int32Scalar)
{
    eval("x = int32(42);");
    auto *x = getVarPtr("x");
    EXPECT_EQ(x->type(), numkit::MType::INT32);
    EXPECT_DOUBLE_EQ(x->toScalar(), 42.0);
}

TEST_P(IntegerTypesTest, Uint8Scalar)
{
    eval("x = uint8(200);");
    auto *x = getVarPtr("x");
    EXPECT_EQ(x->type(), numkit::MType::UINT8);
    EXPECT_DOUBLE_EQ(x->toScalar(), 200.0);
}

TEST_P(IntegerTypesTest, Int8Saturation)
{
    // int8 range: -128..127
    eval("x = int8(200);");
    EXPECT_DOUBLE_EQ(getVar("x"), 127.0);
    eval("y = int8(-200);");
    EXPECT_DOUBLE_EQ(getVar("y"), -128.0);
}

TEST_P(IntegerTypesTest, Uint8Saturation)
{
    eval("x = uint8(300);");
    EXPECT_DOUBLE_EQ(getVar("x"), 255.0);
    eval("y = uint8(-5);");
    EXPECT_DOUBLE_EQ(getVar("y"), 0.0);
}

TEST_P(IntegerTypesTest, Int32Rounding)
{
    // MATLAB rounds to nearest integer
    eval("x = int32(3.7);");
    EXPECT_DOUBLE_EQ(getVar("x"), 4.0);
    eval("y = int32(3.2);");
    EXPECT_DOUBLE_EQ(getVar("y"), 3.0);
}

TEST_P(IntegerTypesTest, Int32NaN)
{
    eval("x = int32(nan);");
    EXPECT_DOUBLE_EQ(getVar("x"), 0.0);
}

TEST_P(IntegerTypesTest, SingleScalar)
{
    eval("x = single(3.14);");
    auto *x = getVarPtr("x");
    EXPECT_EQ(x->type(), numkit::MType::SINGLE);
    EXPECT_NEAR(x->toScalar(), 3.14, 1e-5);
}

TEST_P(IntegerTypesTest, Int16Array)
{
    eval("x = int16([1 2 3 40000]);");
    auto *x = getVarPtr("x");
    EXPECT_EQ(x->type(), numkit::MType::INT16);
    EXPECT_EQ(x->numel(), 4u);
    const int16_t *d = x->int16Data();
    EXPECT_EQ(d[0], 1);
    EXPECT_EQ(d[1], 2);
    EXPECT_EQ(d[2], 3);
    EXPECT_EQ(d[3], 32767); // 40000 saturates to int16 max
}

TEST_P(IntegerTypesTest, Uint64Scalar)
{
    eval("x = uint64(12345678);");
    auto *x = getVarPtr("x");
    EXPECT_EQ(x->type(), numkit::MType::UINT64);
}

TEST_P(IntegerTypesTest, NoArgConstructor)
{
    eval("x = int32();");
    EXPECT_DOUBLE_EQ(getVar("x"), 0.0);
    eval("y = single();");
    auto *y = getVarPtr("y");
    EXPECT_EQ(y->type(), numkit::MType::SINGLE);
}

TEST_P(IntegerTypesTest, DoubleFromInt)
{
    eval("x = double(int32(42));");
    auto *x = getVarPtr("x");
    EXPECT_EQ(x->type(), numkit::MType::DOUBLE);
    EXPECT_DOUBLE_EQ(x->toScalar(), 42.0);
}

// ── class() ─────────────────────────────────────────────────

TEST_P(IntegerTypesTest, ClassFunction)
{
    eval("c = class(int32(5));");
    auto *c = getVarPtr("c");
    EXPECT_EQ(c->toString(), "int32");

    eval("c2 = class(single(1.0));");
    auto *c2 = getVarPtr("c2");
    EXPECT_EQ(c2->toString(), "single");

    eval("c3 = class(3.14);");
    auto *c3 = getVarPtr("c3");
    EXPECT_EQ(c3->toString(), "double");
}

// ── Type query functions ────────────────────────────────────

TEST_P(IntegerTypesTest, IsintegerTrue)
{
    EXPECT_DOUBLE_EQ(evalScalar("isinteger(int32(5))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("isinteger(uint8(5))"), 1.0);
}

TEST_P(IntegerTypesTest, IsintegerFalse)
{
    EXPECT_DOUBLE_EQ(evalScalar("isinteger(3.14)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("isinteger(single(3.14))"), 0.0);
}

TEST_P(IntegerTypesTest, IsfloatTrue)
{
    EXPECT_DOUBLE_EQ(evalScalar("isfloat(3.14)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("isfloat(single(3.14))"), 1.0);
}

TEST_P(IntegerTypesTest, IsfloatFalse)
{
    EXPECT_DOUBLE_EQ(evalScalar("isfloat(int32(5))"), 0.0);
}

TEST_P(IntegerTypesTest, IssingleTrue)
{
    EXPECT_DOUBLE_EQ(evalScalar("issingle(single(1))"), 1.0);
}

TEST_P(IntegerTypesTest, IssingleFalse)
{
    EXPECT_DOUBLE_EQ(evalScalar("issingle(1.0)"), 0.0);
}

// ── isequal / isequaln ──────────────────────────────────────

TEST_P(IntegerTypesTest, IsequalSameType)
{
    EXPECT_DOUBLE_EQ(evalScalar("isequal(int32(5), int32(5))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("isequal([1 2 3], [1 2 3])"), 1.0);
}

TEST_P(IntegerTypesTest, IsequalDifferentType)
{
    // int32(5) ~= double(5) — different types
    EXPECT_DOUBLE_EQ(evalScalar("isequal(int32(5), 5)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("isequal(int32(5), single(5))"), 0.0);
}

TEST_P(IntegerTypesTest, IsequalDifferentValue)
{
    EXPECT_DOUBLE_EQ(evalScalar("isequal(int32(5), int32(6))"), 0.0);
}

TEST_P(IntegerTypesTest, IsequalArrays)
{
    EXPECT_DOUBLE_EQ(evalScalar("isequal([1 2; 3 4], [1 2; 3 4])"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("isequal([1 2; 3 4], [1 2; 3 5])"), 0.0);
}

TEST_P(IntegerTypesTest, IsequalStrings)
{
    EXPECT_DOUBLE_EQ(evalScalar("isequal('abc', 'abc')"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("isequal('abc', 'abd')"), 0.0);
}

TEST_P(IntegerTypesTest, IsequalNaN)
{
    // isequal: NaN ~= NaN
    EXPECT_DOUBLE_EQ(evalScalar("isequal(nan, nan)"), 0.0);
    // isequaln: NaN == NaN
    EXPECT_DOUBLE_EQ(evalScalar("isequaln(nan, nan)"), 1.0);
}

TEST_P(IntegerTypesTest, IsequalMultipleArgs)
{
    EXPECT_DOUBLE_EQ(evalScalar("isequal(1, 1, 1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("isequal(1, 1, 2)"), 0.0);
}

TEST_P(IntegerTypesTest, IsequalCells)
{
    EXPECT_DOUBLE_EQ(evalScalar("isequal({1,'a'}, {1,'a'})"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("isequal({1,'a'}, {1,'b'})"), 0.0);
}

TEST_P(IntegerTypesTest, IsequalStructs)
{
    eval("s1.x = 1; s1.y = 2;");
    eval("s2.x = 1; s2.y = 2;");
    EXPECT_DOUBLE_EQ(evalScalar("isequal(s1, s2)"), 1.0);
    eval("s3.x = 1; s3.y = 3;");
    EXPECT_DOUBLE_EQ(evalScalar("isequal(s1, s3)"), 0.0);
}

// ── Arithmetic ──────────────────────────────────────────────

TEST_P(IntegerTypesTest, Int32Add)
{
    eval("x = int32(10) + int32(20);");
    EXPECT_DOUBLE_EQ(getVar("x"), 30.0);
    auto *x = getVarPtr("x");
    EXPECT_EQ(x->type(), numkit::MType::INT32);
}

TEST_P(IntegerTypesTest, Int8AddSaturation)
{
    eval("x = int8(100) + int8(100);");
    EXPECT_DOUBLE_EQ(getVar("x"), 127.0); // saturates
}

TEST_P(IntegerTypesTest, Uint8SubSaturation)
{
    eval("x = uint8(10) - uint8(20);");
    EXPECT_DOUBLE_EQ(getVar("x"), 0.0); // unsigned saturates to 0
}

TEST_P(IntegerTypesTest, Int32MulSaturation)
{
    eval("x = int32(100000) * int32(100000);");
    EXPECT_DOUBLE_EQ(getVar("x"), 2147483647.0); // INT32_MAX
}

TEST_P(IntegerTypesTest, Int32Div)
{
    eval("x = int32(7) / int32(2);");
    EXPECT_DOUBLE_EQ(getVar("x"), 4.0); // rounded
}

TEST_P(IntegerTypesTest, Int32DivByZero)
{
    eval("x = int32(5) / int32(0);");
    EXPECT_DOUBLE_EQ(getVar("x"), 2147483647.0); // INT32_MAX
}

TEST_P(IntegerTypesTest, IntPlusDouble)
{
    // int32 + double → int32 (MATLAB behavior)
    eval("x = int32(10) + 2.7;");
    auto *x = getVarPtr("x");
    EXPECT_EQ(x->type(), numkit::MType::INT32);
    EXPECT_DOUBLE_EQ(x->toScalar(), 13.0); // 10 + round(2.7) = 13
}

TEST_P(IntegerTypesTest, IntMixedTypeError)
{
    // int32 + int16 → error
    EXPECT_THROW(eval("int32(1) + int16(1);"), std::exception);
}

TEST_P(IntegerTypesTest, SingleAdd)
{
    eval("x = single(1.5) + single(2.5);");
    auto *x = getVarPtr("x");
    EXPECT_EQ(x->type(), numkit::MType::SINGLE);
    EXPECT_NEAR(x->toScalar(), 4.0, 1e-6);
}

TEST_P(IntegerTypesTest, SinglePlusDouble)
{
    // single + double → single (MATLAB behavior)
    eval("x = single(1.5) + 2.5;");
    auto *x = getVarPtr("x");
    EXPECT_EQ(x->type(), numkit::MType::SINGLE);
    EXPECT_NEAR(x->toScalar(), 4.0, 1e-6);
}

TEST_P(IntegerTypesTest, UnaryNegInt)
{
    eval("x = -int32(5);");
    EXPECT_DOUBLE_EQ(getVar("x"), -5.0);
    auto *x = getVarPtr("x");
    EXPECT_EQ(x->type(), numkit::MType::INT32);
}

TEST_P(IntegerTypesTest, UnaryNegUint)
{
    eval("x = -uint8(5);");
    EXPECT_DOUBLE_EQ(getVar("x"), 0.0); // unsigned neg → 0
}

TEST_P(IntegerTypesTest, Int32ArrayArith)
{
    eval("a = int32([1 2 3]); b = int32([4 5 6]); c = a + b;");
    auto *c = getVarPtr("c");
    EXPECT_EQ(c->type(), numkit::MType::INT32);
    const int32_t *d = c->int32Data();
    EXPECT_EQ(d[0], 5);
    EXPECT_EQ(d[1], 7);
    EXPECT_EQ(d[2], 9);
}

TEST_P(IntegerTypesTest, Int32ScalarBroadcast)
{
    eval("x = int32([1 2 3]) + int32(10);");
    auto *x = getVarPtr("x");
    const int32_t *d = x->int32Data();
    EXPECT_EQ(d[0], 11);
    EXPECT_EQ(d[1], 12);
    EXPECT_EQ(d[2], 13);
}

INSTANTIATE_DUAL(IntegerTypesTest);
