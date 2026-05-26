// libs/stats/tests/lhsdesign_lhsnorm_test.cpp
//
// Regression guard for lhsdesign + lhsnorm.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

#include <cmath>

using namespace numkit;

class LhsTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override { engine.eval("import compat.*; rng(0);"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// ── lhsdesign ───────────────────────────────────────────────────────

TEST_F(LhsTest, LhsdesignShape)
{
    eval("X = lhsdesign(7, 4);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(X, 1)")), 7);
    EXPECT_EQ(static_cast<int>(evalScalar("size(X, 2)")), 4);
}

TEST_F(LhsTest, LhsdesignAllInUnitInterval)
{
    eval("X = lhsdesign(20, 5); ok = all(X(:) > 0 & X(:) < 1);");
    EXPECT_EQ(static_cast<int>(evalScalar("ok")), 1);
}

TEST_F(LhsTest, LhsdesignBinPartition)
{
    // Each column must partition [0, 1] into n bins; one sample per bin.
    eval(R"(
        n = 10;
        X = lhsdesign(n, 3);
        bins = ceil(X * n);
        ok = 1;
        for j = 1:3
            s = sort(bins(:, j))';
            if any(s ~= 1:n)
                ok = 0;
            end
        end
    )");
    EXPECT_EQ(static_cast<int>(evalScalar("ok")), 1);
}

TEST_F(LhsTest, LhsdesignMeanCloseToHalf)
{
    eval("X = lhsdesign(200, 2); m = mean(X);");
    EXPECT_NEAR(evalScalar("m(1)"), 0.5, 0.05);
    EXPECT_NEAR(evalScalar("m(2)"), 0.5, 0.05);
}

// ── lhsnorm ─────────────────────────────────────────────────────────

TEST_F(LhsTest, LhsnormShape)
{
    eval("Y = lhsnorm([1 2 3], eye(3), 50);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(Y, 1)")), 50);
    EXPECT_EQ(static_cast<int>(evalScalar("size(Y, 2)")), 3);
}

TEST_F(LhsTest, LhsnormMeanRecovered)
{
    eval("Y = lhsnorm([10, -5], eye(2), 500); m = mean(Y);");
    EXPECT_NEAR(evalScalar("m(1)"), 10.0, 1.0);
    EXPECT_NEAR(evalScalar("m(2)"), -5.0, 1.0);
}

TEST_F(LhsTest, LhsnormCovApprox)
{
    eval(R"(
        S = [1 0.5; 0.5 1];
        Y = lhsnorm([0 0], S, 1000);
        C = cov(Y);
        d11 = abs(C(1,1) - 1);
        d22 = abs(C(2,2) - 1);
        d12 = abs(C(1,2) - 0.5);
    )");
    EXPECT_LT(evalScalar("d11"), 0.2);
    EXPECT_LT(evalScalar("d22"), 0.2);
    EXPECT_LT(evalScalar("d12"), 0.15);
}

TEST_F(LhsTest, LhsnormShapeMismatchThrows)
{
    EXPECT_THROW(eval("lhsnorm([1 2 3], [1 0; 0 1], 5);"), std::exception);
}
