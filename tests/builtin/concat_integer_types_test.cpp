// toolboxes/builtin/tests/concat_integer_types_test.cpp
//
// Regression guard for bugs/builtin/concat-integer-types.md: concatenation of
// integer arrays used to throw "Concatenation not supported for type '<int>'"
// (core promoteNumericType). MATLAB R2025b concatenates integers, preserving
// the class — the FIRST integer operand's class wins; double/logical/a
// different integer class / the real part of complex are cast with
// round-half-away + saturate. Bit-exact MATLAB R2025b.

#include <numkit/core/engine.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class ConcatIntegerTypesTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// [;] and [,] operators + cat(1)/cat(2) on same int class.
TEST_F(ConcatIntegerTypesTest, OperatorsSameClass)
{
    eval("a = [int8([1 2]); int8([3 4])];");      // int8 [1 2; 3 4]
    EXPECT_DOUBLE_EQ(evalScalar("isa(a,'int8')"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("a(2,1)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("a(1,2)"), 2.0);
    eval("b = [int8([1 2]), int8([3 4])];");      // int8 1x4
    EXPECT_DOUBLE_EQ(evalScalar("isa(b,'int8')"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("numel(b)"), 4.0);
    EXPECT_DOUBLE_EQ(evalScalar("b(3)"), 3.0);
}

// cat(1)/cat(2)/cat(3) + vertcat/horzcat builtins preserve uint class.
TEST_F(ConcatIntegerTypesTest, CatBuiltins)
{
    eval("c = cat(1, uint16([10 20]), uint16([30 40]));");
    EXPECT_DOUBLE_EQ(evalScalar("isa(c,'uint16')"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("c(2,2)"), 40.0);
    eval("v = vertcat(int8(1), int8(2));");
    EXPECT_DOUBLE_EQ(evalScalar("isa(v,'int8')"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("numel(v)"), 2.0);
    eval("h = horzcat(int8(5), int8(6));");
    EXPECT_DOUBLE_EQ(evalScalar("h(2)"), 6.0);
    eval("c3 = cat(3, int8([1 2]), int8([3 4]));");   // 1x2x2 int8
    EXPECT_DOUBLE_EQ(evalScalar("isa(c3,'int8')"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(c3,3)"), 2.0);
}

// Mixed int + double: result is the int class, double cast with round+saturate.
TEST_F(ConcatIntegerTypesTest, MixedDoubleRoundSaturate)
{
    eval("d = [int8(5); 50.6];");       // 50.6 -> 51 (round half away)
    EXPECT_DOUBLE_EQ(evalScalar("isa(d,'int8')"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("d(2)"), 51.0);
    EXPECT_DOUBLE_EQ(evalScalar("[int8(5); 300](2)"), 127.0);    // saturate hi
    EXPECT_DOUBLE_EQ(evalScalar("[int8(5); -300](2)"), -128.0);  // saturate lo
    eval("u = [uint8(250); 10.9];");    // 10.9 -> 11
    EXPECT_DOUBLE_EQ(evalScalar("u(2)"), 11.0);
    EXPECT_DOUBLE_EQ(evalScalar("isa(u,'uint8')"), 1.0);
}

// Mixed int + logical / + a different int class / + complex (R2025b: first int
// wins, others cast; NOT an error).
TEST_F(ConcatIntegerTypesTest, MixedOtherClasses)
{
    eval("g = [int8(5); true];");
    EXPECT_DOUBLE_EQ(evalScalar("isa(g,'int8')"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("g(2)"), 1.0);
    eval("m = [int8(5), int16(6)];");     // first int (int8) wins
    EXPECT_DOUBLE_EQ(evalScalar("isa(m,'int8')"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("m(2)"), 6.0);
    eval("n = [int16(5), int8(6)];");     // first int (int16) wins
    EXPECT_DOUBLE_EQ(evalScalar("isa(n,'int16')"), 1.0);
    eval("z = [int8(5); 2+3i];");         // real part of complex
    EXPECT_DOUBLE_EQ(evalScalar("isa(z,'int8')"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("z(2)"), 2.0);
}

// Double / logical / complex concatenation is UNCHANGED (zero regression).
TEST_F(ConcatIntegerTypesTest, NonIntegerUnaffected)
{
    EXPECT_DOUBLE_EQ(evalScalar("isa([1 2; 3 4],'double')"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("islogical([true false; false true])"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("isreal([1+2i 3])"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("ischar(['ab'; 'cd'])"), 1.0);
}
