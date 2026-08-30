// toolboxes/stats/tests/isoutlier_gesd_test.cpp
//
// Regression guard for bugs/stats/isoutlier-gesd.md (FIXED): isoutlier(x,'gesd')
// implements Rosner's generalized extreme Studentized deviate test — peel the
// most-extreme point for MaxNumOutliers iterations (default max(1,round(n/10))),
// flag up to the largest iteration whose studentized deviate exceeds the
// Grubbs-form critical value (handles masking). ThresholdFactor is the
// significance level (default 0.05). MATLAB R2025b reference values.

#include <numkit/core/engine.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

class IsoutlierGesdTest : public ::testing::Test
{
public:
    numkit::StandardEngine engine;
    void SetUp() override {}
    numkit::Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// The repro: a single extreme value is flagged.
TEST_F(IsoutlierGesdTest, SingleOutlier)
{
    eval("m = isoutlier([1 2 3 4 5 6 7 8 9 50], 'gesd');");
    EXPECT_DOUBLE_EQ(evalScalar("double(m(10))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("sum(double(m))"), 1.0);
    // mid-vector outlier
    eval("g = isoutlier([1 2 3 100 4 5], 'gesd');");
    EXPECT_DOUBLE_EQ(evalScalar("double(g(4))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("sum(double(g))"), 1.0);
}

// Clean data → nothing flagged.
TEST_F(IsoutlierGesdTest, NoOutliers)
{
    eval("m = isoutlier([1 2 3 4 5 6 7 8 9 10], 'gesd');");
    EXPECT_DOUBLE_EQ(evalScalar("sum(double(m))"), 0.0);
}

// Several spread outliers detected by peeling (n=25, default cap round(2.5)=3).
TEST_F(IsoutlierGesdTest, SpreadOutliersPeeled)
{
    eval("m = isoutlier([zeros(1,22) 20 30 40], 'gesd');");
    EXPECT_DOUBLE_EQ(evalScalar("sum(double(m))"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(m(23))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(m(25))"), 1.0);
}

// Masking: 5 mutually-inflating extremes with default cap=round(15/10)=2 → none.
TEST_F(IsoutlierGesdTest, MaskingDefaultCap)
{
    eval("m = isoutlier([zeros(1,10) 100 101 102 103 104], 'gesd');");
    EXPECT_DOUBLE_EQ(evalScalar("sum(double(m))"), 0.0);
    // raising MaxNumOutliers to 5 unmasks all five
    eval("m5 = isoutlier([zeros(1,10) 100 101 102 103 104], 'gesd', 'MaxNumOutliers', 5);");
    EXPECT_DOUBLE_EQ(evalScalar("sum(double(m5))"), 5.0);
}

// ThresholdFactor (significance level) still flags a very extreme value.
TEST_F(IsoutlierGesdTest, ThresholdFactor)
{
    eval("m = isoutlier([1 2 3 4 5 6 7 8 9 50], 'gesd', 'ThresholdFactor', 0.01);");
    EXPECT_DOUBLE_EQ(evalScalar("sum(double(m))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(m(10))"), 1.0);
}

// Small sample (n=5) with one extreme.
TEST_F(IsoutlierGesdTest, SmallSample)
{
    eval("m = isoutlier([0 0 0 0 50], 'gesd');");
    EXPECT_DOUBLE_EQ(evalScalar("sum(double(m))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(m(5))"), 1.0);
}

// Bad MaxNumOutliers / wrong method for the option throw.
TEST_F(IsoutlierGesdTest, OptionValidation)
{
    EXPECT_ANY_THROW(eval("isoutlier([1 2 3 4 5], 'gesd', 'MaxNumOutliers', 0);"));
    EXPECT_ANY_THROW(eval("isoutlier([1 2 3 4 5], 'gesd', 'MaxNumOutliers', 1.5);"));
    EXPECT_ANY_THROW(eval("isoutlier([1 2 3 4 5], 'median', 'MaxNumOutliers', 2);"));
}
