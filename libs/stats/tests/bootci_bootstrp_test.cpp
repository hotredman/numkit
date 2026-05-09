// libs/stats/tests/bootci_bootstrp_test.cpp
//
// Regression guard for bootci / bootstrp (function-handle based).

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>
#include <cmath>

using namespace numkit;

class BootTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// ── bootstrp ───────────────────────────────────────────────

TEST_F(BootTest, BootstrpShape)
{
    eval("rng(42); x = randn(100, 1); B = bootstrp(500, @mean, x);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(B,1)")), 500);
    EXPECT_EQ(static_cast<int>(evalScalar("size(B,2)")), 1);
}

TEST_F(BootTest, BootstrpMeanConvergence)
{
    // Mean of bootstrap means should be close to true mean (CLT).
    eval("rng(42); x = randn(200, 1); B = bootstrp(2000, @mean, x);");
    EXPECT_NEAR(evalScalar("mean(B) - mean(x)"), 0.0, 0.05);
}

TEST_F(BootTest, BootstrpStdScaling)
{
    // Std of bootstrap means ≈ std(x) / sqrt(N).
    eval("rng(42); x = randn(100, 1); B = bootstrp(1000, @mean, x); "
         "expected_se = std(x) / sqrt(100);");
    const double observed_se = evalScalar("std(B)");
    const double expected_se = evalScalar("expected_se");
    EXPECT_LT(std::fabs(observed_se - expected_se), 0.05);
}

TEST_F(BootTest, BootstrpRequiresFuncHandle)
{
    EXPECT_THROW(eval("bootstrp(100, 5, [1 2 3]);"), std::exception);
}

TEST_F(BootTest, BootstrpRequiresPositiveN)
{
    EXPECT_THROW(eval("bootstrp(0, @mean, [1 2 3]);"), std::exception);
}

// ── bootci ──────────────────────────────────────────────────

TEST_F(BootTest, BootciContainsTrueMean)
{
    eval("rng(42); x = randn(200, 1); ci = bootci(1000, @mean, x); m = mean(x);");
    EXPECT_LE(evalScalar("ci(1)"), evalScalar("m"));
    EXPECT_GE(evalScalar("ci(2)"), evalScalar("m"));
}

TEST_F(BootTest, BootciAlphaWidens)
{
    // 99% CI should be wider than 95% CI.
    eval("rng(42); x = randn(100, 1); "
         "ci95 = bootci(1000, @mean, x, 0.05); "
         "ci99 = bootci(1000, @mean, x, 0.01);");
    const double width95 = evalScalar("ci95(2) - ci95(1)");
    const double width99 = evalScalar("ci99(2) - ci99(1)");
    EXPECT_GT(width99, width95);
}

TEST_F(BootTest, BootciRequiresMinimumN)
{
    EXPECT_THROW(eval("bootci(5, @mean, [1 2 3]);"), std::exception);
}

TEST_F(BootTest, BootciAlphaRangeChecked)
{
    EXPECT_THROW(eval("bootci(100, @mean, [1 2 3 4 5], 0);"), std::exception);
    EXPECT_THROW(eval("bootci(100, @mean, [1 2 3 4 5], 1);"), std::exception);
}
