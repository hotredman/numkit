// toolboxes/builtin/tests/misc3_batch_test.cpp
// : interp + ind2sub + predicates3 + helpers (18).
//   interp:        interp1 · interp2 · discretize
//   ind/sub:       ind2sub · sub2ind
//   predicates3:   isfloat · isinteger · iskeyword · isletter · isspace
//   helpers:       disp · celldisp · ans · inf · nan · idivide
//   deferred:      functions · formattedDisplayText (numkit gaps)
// All . Bit-identical MATLAB R2025b
// on probed inputs.

#include <numkit/core/engine.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class Misc3BatchTest : public ::testing::Test
{
public:
    StandardEngine engine;
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(Misc3BatchTest, Interp1)
{
    EXPECT_DOUBLE_EQ(evalScalar("interp1([1 2 3], [10 20 30], 1.5)"), 15.0);
    EXPECT_DOUBLE_EQ(evalScalar("interp1([1 2 3], [10 20 30], 2.5)"), 25.0);
    EXPECT_DOUBLE_EQ(evalScalar("interp1([1 2 3], [10 20 30], 1)"),   10.0);
    EXPECT_DOUBLE_EQ(evalScalar("interp1([1 2 3], [10 20 30], 3)"),   30.0);
}

TEST_F(Misc3BatchTest, Interp2)
{
    eval("X = [1 2; 3 4];");
    EXPECT_NEAR(evalScalar("interp2([1 2], [1; 2], X, 1.5, 1.5)"), 2.5, 1e-12);
}

TEST_F(Misc3BatchTest, Discretize)
{
    eval("idx = discretize([0.5, 1.5, 2.5], [0 1 2 3]);");
    EXPECT_DOUBLE_EQ(evalScalar("idx(1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("idx(2)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("idx(3)"), 3.0);
}

TEST_F(Misc3BatchTest, Ind2SubSub2Ind)
{
    // ind2sub for 3×4 matrix, linear index 5 → row 2, col 2 (column-major)
    eval("[i, j] = ind2sub([3 4], 5);");
    EXPECT_DOUBLE_EQ(evalScalar("i"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("j"), 2.0);

    // Round-trip
    EXPECT_DOUBLE_EQ(evalScalar("sub2ind([3 4], 2, 2)"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("sub2ind([3 4], 1, 1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("sub2ind([3 4], 3, 4)"), 12.0);
}

TEST_F(Misc3BatchTest, IsFloatIsInteger)
{
    EXPECT_DOUBLE_EQ(evalScalar("isfloat(3.14)"),       1.0);
    EXPECT_DOUBLE_EQ(evalScalar("isfloat(int8(5))"),    0.0);
    EXPECT_DOUBLE_EQ(evalScalar("isinteger(int8(5))"),  1.0);
    EXPECT_DOUBLE_EQ(evalScalar("isinteger(3.14)"),     0.0);
    EXPECT_DOUBLE_EQ(evalScalar("isinteger(uint32(0))"), 1.0);
}

TEST_F(Misc3BatchTest, IsKeywordLetterSpace)
{
    EXPECT_DOUBLE_EQ(evalScalar("iskeyword('if')"),  1.0);
    EXPECT_DOUBLE_EQ(evalScalar("iskeyword('for')"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("iskeyword('foo')"), 0.0);

    eval("v = isletter('abc 123');");
    EXPECT_DOUBLE_EQ(evalScalar("double(v(1))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(v(4))"), 0.0);  // space
    EXPECT_DOUBLE_EQ(evalScalar("double(v(5))"), 0.0);  // digit

    eval("v = isspace('a b c');");
    EXPECT_DOUBLE_EQ(evalScalar("double(v(1))"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(v(2))"), 1.0);
}

// isvarname: valid MATLAB identifier? starts with a letter, only
// letters/digits/underscore, not a keyword; non-text yields false (no error).
// vs MATLAB R2025b. Implemented 2026-05-30 (was an undefined function).
TEST_F(Misc3BatchTest, IsVarname)
{
    EXPECT_DOUBLE_EQ(evalScalar("isvarname('abc')"),  1.0);
    EXPECT_DOUBLE_EQ(evalScalar("isvarname('a_1')"),  1.0);
    EXPECT_DOUBLE_EQ(evalScalar("isvarname('1abc')"), 0.0);  // leading digit
    EXPECT_DOUBLE_EQ(evalScalar("isvarname('_x')"),   0.0);  // leading underscore
    EXPECT_DOUBLE_EQ(evalScalar("isvarname('a b')"),  0.0);  // space
    EXPECT_DOUBLE_EQ(evalScalar("isvarname('')"),     0.0);  // empty
    EXPECT_DOUBLE_EQ(evalScalar("isvarname('if')"),   0.0);  // keyword
    EXPECT_DOUBLE_EQ(evalScalar("isvarname('end')"),  0.0);  // keyword
    EXPECT_DOUBLE_EQ(evalScalar("isvarname(\"abc\")"), 1.0); // string scalar
    EXPECT_DOUBLE_EQ(evalScalar("isvarname(5)"),      0.0);  // numeric -> false
    EXPECT_DOUBLE_EQ(evalScalar("isvarname({'abc'})"), 0.0); // cell -> false
}

TEST_F(Misc3BatchTest, InfNan)
{
    EXPECT_DOUBLE_EQ(evalScalar("isinf(inf)"),  1.0);
    EXPECT_DOUBLE_EQ(evalScalar("inf > 0"),     1.0);
    EXPECT_DOUBLE_EQ(evalScalar("isnan(nan)"),  1.0);
    EXPECT_DOUBLE_EQ(evalScalar("nan == nan"),  0.0);  // NaN never equal
}

TEST_F(Misc3BatchTest, Idivide)
{
    EXPECT_DOUBLE_EQ(evalScalar("double(idivide(int32(7),  int32(2)))"),  3.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(idivide(int32(-7), int32(2)))"), -3.0);
}

TEST_F(Misc3BatchTest, Ans)
{
    eval("5;");
    EXPECT_DOUBLE_EQ(evalScalar("ans"), 5.0);
}
