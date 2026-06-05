// libs/builtin/tests/maxmin_char_double_test.cpp
//
// Regression guard for bugs/builtin/maxmin-char-double.md: max/min of a char
// array used to return CHAR; MATLAB R2025b returns DOUBLE (the code point).
// (mode KEEPS char — that is correct and asserted separately.) Bit-exact
// MATLAB R2025b.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class MaxMinCharDoubleTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// max/min of a char vector -> double code point.
TEST_F(MaxMinCharDoubleTest, VectorReduction)
{
    eval("m = max('abc');");
    EXPECT_DOUBLE_EQ(evalScalar("ischar(m)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("isa(m,'double')"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("m"), 99.0);
    eval("n = min('abc');");
    EXPECT_DOUBLE_EQ(evalScalar("ischar(n)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("n"), 97.0);
}

// 2nd-output index is double; value is double.
TEST_F(MaxMinCharDoubleTest, ValueIndex)
{
    eval("[v, i] = max('abc');");
    EXPECT_DOUBLE_EQ(evalScalar("v"), 99.0);
    EXPECT_DOUBLE_EQ(evalScalar("i"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("ischar(v)"), 0.0);
}

// Column-wise on a 2-D char matrix.
TEST_F(MaxMinCharDoubleTest, MatrixColumnwise)
{
    eval("c = max(['abc'; 'xyz']);");   // per column: x y z = 120 121 122
    EXPECT_DOUBLE_EQ(evalScalar("ischar(c)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("c(1)"), 120.0);
    EXPECT_DOUBLE_EQ(evalScalar("c(2)"), 121.0);
    EXPECT_DOUBLE_EQ(evalScalar("c(3)"), 122.0);
    // 'all' + dim 2.
    EXPECT_DOUBLE_EQ(evalScalar("max('abc',[],'all')"), 99.0);
    eval("r = max(['abc';'xyz'], [], 2);");
    EXPECT_DOUBLE_EQ(evalScalar("r(1)"), 99.0);   // 'c'
    EXPECT_DOUBLE_EQ(evalScalar("r(2)"), 122.0);  // 'z'
}

// NOTE: the binary max('a','c') form is intentionally NOT asserted — MATLAB
// ERRORS on it ("Invalid second argument"); numkit is lenient (returns double)
// but that is not a MATLAB-matched form (see bugs/builtin/maxmin-char-double.md).

// mode KEEPS the char class (NOT changed) — guards we didn't over-reach.
TEST_F(MaxMinCharDoubleTest, ModeStillChar)
{
    eval("mo = mode('abc');");
    EXPECT_DOUBLE_EQ(evalScalar("ischar(mo)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(mo)"), 97.0);   // 'a'
}
