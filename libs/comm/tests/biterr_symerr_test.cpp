// libs/comm/tests/biterr_symerr_test.cpp
//
// Regression guard for biterr / symerr.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class BitErrTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(BitErrTest, BiterrIdentical)
{
    eval("[n, r] = biterr([1 2 3], [1 2 3]);");
    EXPECT_DOUBLE_EQ(evalScalar("n"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("r"), 0.0);
}

TEST_F(BitErrTest, BiterrSingleDiff)
{
    // 7 = 111, 5 = 101 -> 1 bit difference. k=3 by default (max=7).
    eval("[n, r] = biterr(7, 5);");
    EXPECT_DOUBLE_EQ(evalScalar("n"), 1.0);
    EXPECT_NEAR(evalScalar("r"), 1.0/3.0, 1e-12);
}

TEST_F(BitErrTest, BiterrBinaryArrays)
{
    eval("[n, r] = biterr([0 1 0 1 1 0 1], [0 0 0 1 1 1 1]);");
    EXPECT_DOUBLE_EQ(evalScalar("n"), 2.0);
    EXPECT_NEAR(evalScalar("r"), 2.0/7.0, 1e-12);
}

TEST_F(BitErrTest, BiterrAllDifferent)
{
    // [15, 7, 3] vs [0, 0, 0]: 4+3+2 = 9 bits, k=4 (since max=15).
    eval("[n, r] = biterr([15 7 3], [0 0 0]);");
    EXPECT_DOUBLE_EQ(evalScalar("n"), 9.0);
    EXPECT_NEAR(evalScalar("r"), 9.0/12.0, 1e-12);
}

TEST_F(BitErrTest, BiterrSizeMismatchThrows)
{
    EXPECT_THROW(eval("biterr([1 2 3], [1 2]);"), std::exception);
}

TEST_F(BitErrTest, SymerrIdentical)
{
    eval("[n, r] = symerr([1 2 3 4], [1 2 3 4]);");
    EXPECT_DOUBLE_EQ(evalScalar("n"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("r"), 0.0);
}

TEST_F(BitErrTest, SymerrCount)
{
    eval("[n, r] = symerr([0 1 2 3 4 5 6 7], [0 1 2 3 4 5 6 5]);");
    EXPECT_DOUBLE_EQ(evalScalar("n"), 1.0);
    EXPECT_NEAR(evalScalar("r"), 1.0/8.0, 1e-12);
}

TEST_F(BitErrTest, SymerrAllDifferent)
{
    eval("[n, r] = symerr([1 2 3], [4 5 6]);");
    EXPECT_DOUBLE_EQ(evalScalar("n"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("r"), 1.0);
}

TEST_F(BitErrTest, SymerrSizeMismatchThrows)
{
    EXPECT_THROW(eval("symerr([1 2 3], [1 2]);"), std::exception);
}
