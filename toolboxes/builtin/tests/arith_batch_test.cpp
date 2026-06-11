// toolboxes/builtin/tests/arith_batch_test.cpp
// arithmetic ops — 10 functions:
//   plus / minus / times / rdivide / ldivide
//   mtimes / uminus / uplus / power / mpower
// All  — bit-identical MATLAB R2025b
// on probed inputs.
// Known sub-gap: numkit's mpower(matrix, n) (i.e. M^n) not
// implemented — only scalar^scalar tested.
// mpower.md.

#include <numkit/core/engine.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class ArithBatchTest : public ::testing::Test
{
public:
    StandardEngine engine;
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(ArithBatchTest, ScalarOps)
{
    EXPECT_DOUBLE_EQ(evalScalar("plus(2, 3)"),    5.0);
    EXPECT_DOUBLE_EQ(evalScalar("minus(5, 2)"),   3.0);
    EXPECT_DOUBLE_EQ(evalScalar("times(2, 3)"),   6.0);
    EXPECT_DOUBLE_EQ(evalScalar("rdivide(6, 2)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("ldivide(2, 6)"), 3.0);   // 6/2
    EXPECT_DOUBLE_EQ(evalScalar("power(2, 10)"),  1024.0);
}

TEST_F(ArithBatchTest, ElementwiseOps)
{
    eval("y = plus([1 2 3], [10 20 30]);");
    EXPECT_DOUBLE_EQ(evalScalar("y(1)"), 11.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(3)"), 33.0);

    eval("z = times([2 3], [4 5]);");
    EXPECT_DOUBLE_EQ(evalScalar("z(1)"),  8.0);
    EXPECT_DOUBLE_EQ(evalScalar("z(2)"), 15.0);
}

TEST_F(ArithBatchTest, UnaryOps)
{
    EXPECT_DOUBLE_EQ(evalScalar("uminus(5)"),  -5.0);
    EXPECT_DOUBLE_EQ(evalScalar("uminus(-3)"),  3.0);
    EXPECT_DOUBLE_EQ(evalScalar("uplus(5)"),    5.0);
    EXPECT_DOUBLE_EQ(evalScalar("uplus(-3)"),  -3.0);
}

TEST_F(ArithBatchTest, MatrixMultiply)
{
    // [1 2; 3 4] * [5 6; 7 8] = [19 22; 43 50]
    eval("C = mtimes([1 2; 3 4], [5 6; 7 8]);");
    EXPECT_DOUBLE_EQ(evalScalar("C(1,1)"), 19.0);
    EXPECT_DOUBLE_EQ(evalScalar("C(1,2)"), 22.0);
    EXPECT_DOUBLE_EQ(evalScalar("C(2,1)"), 43.0);
    EXPECT_DOUBLE_EQ(evalScalar("C(2,2)"), 50.0);
}

TEST_F(ArithBatchTest, ScalarPower)
{
    // mpower scalar path
    EXPECT_DOUBLE_EQ(evalScalar("mpower(2, 3)"),  8.0);
    EXPECT_DOUBLE_EQ(evalScalar("mpower(5, 2)"), 25.0);
    EXPECT_DOUBLE_EQ(evalScalar("mpower(3, 0)"),  1.0);
}

TEST_F(ArithBatchTest, MatrixPower)
{
    // Matrix raised to an integer scalar power is repeated matrix multiply:
    // [1 2; 0 1]^3 = [1 6; 0 1] (matches MATLAB R2025b).
    eval("P = [1 2; 0 1]^3;");
    EXPECT_DOUBLE_EQ(evalScalar("P(1,1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("P(1,2)"), 6.0);
    EXPECT_DOUBLE_EQ(evalScalar("P(2,1)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("P(2,2)"), 1.0);

    // matrix ^ matrix is undefined — MATLAB errors, so we must too.
    bool threw = false;
    try { eval("[1 2; 0 1]^[1 2; 3 4];"); }
    catch (const std::exception &) { threw = true; }
    EXPECT_TRUE(threw);
}
