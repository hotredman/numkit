// libs/builtin/tests/grouptransform_test.cpp
//
// Regression guard for grouptransform array form.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

#include <cmath>

using namespace numkit;

class GroupTransformTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override {
        engine.eval("A = [10; 20; 30; 40; 50]; G = [1; 2; 1; 2; 1];");
    }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// ── string methods ───────────────────────────────────────────────

TEST_F(GroupTransformTest, MeanCenter)
{
    eval("B = grouptransform(A, G, 'meancenter');");
    EXPECT_NEAR(evalScalar("B(1)"), -20.0, 1e-12);
    EXPECT_NEAR(evalScalar("B(2)"), -10.0, 1e-12);
    EXPECT_NEAR(evalScalar("B(3)"),   0.0, 1e-12);
    EXPECT_NEAR(evalScalar("B(4)"),  10.0, 1e-12);
    EXPECT_NEAR(evalScalar("B(5)"),  20.0, 1e-12);
}

TEST_F(GroupTransformTest, ZScore)
{
    eval("B = grouptransform(A, G, 'zscore');");
    EXPECT_NEAR(evalScalar("B(1)"), -1.0,                 1e-12);
    EXPECT_NEAR(evalScalar("B(2)"), -0.7071067811865475, 1e-12);
    EXPECT_NEAR(evalScalar("B(3)"),  0.0,                 1e-12);
    EXPECT_NEAR(evalScalar("B(5)"),  1.0,                 1e-12);
}

TEST_F(GroupTransformTest, Rescale)
{
    eval("B = grouptransform(A, G, 'rescale');");
    EXPECT_NEAR(evalScalar("B(1)"), 0.0, 1e-12);
    EXPECT_NEAR(evalScalar("B(3)"), 0.5, 1e-12);
    EXPECT_NEAR(evalScalar("B(5)"), 1.0, 1e-12);
}

TEST_F(GroupTransformTest, Norm2)
{
    eval("B = grouptransform(A, G, 'norm');");
    EXPECT_NEAR(evalScalar("B(1)"), 10.0 / std::sqrt(3500.0), 1e-12);
    EXPECT_NEAR(evalScalar("B(2)"), 20.0 / std::sqrt(2000.0), 1e-12);
}

TEST_F(GroupTransformTest, MeanFill)
{
    eval("A3 = [10; NaN; 30; 40; NaN]; B = grouptransform(A3, G, 'meanfill');");
    EXPECT_NEAR(evalScalar("B(1)"), 10.0, 1e-12);
    EXPECT_NEAR(evalScalar("B(2)"), 40.0, 1e-12);
    EXPECT_NEAR(evalScalar("B(3)"), 30.0, 1e-12);
    EXPECT_NEAR(evalScalar("B(4)"), 40.0, 1e-12);
    EXPECT_NEAR(evalScalar("B(5)"), 20.0, 1e-12);
}

TEST_F(GroupTransformTest, LinearFill)
{
    eval("A3 = [10; NaN; 30; 40; NaN]; B = grouptransform(A3, G, 'linearfill');");
    EXPECT_NEAR(evalScalar("B(1)"), 10.0, 1e-12);
    EXPECT_TRUE(std::isnan(evalScalar("B(2)")));   // group 2 < 2 good values
    EXPECT_NEAR(evalScalar("B(3)"), 30.0, 1e-12);
    EXPECT_NEAR(evalScalar("B(5)"), 50.0, 1e-12); // extrapolated 10→30→50
}

// ── NaN group ────────────────────────────────────────────────────

TEST_F(GroupTransformTest, NaNGroupAsSingleton)
{
    eval("G2 = [1; 2; NaN; 1; 2]; B = grouptransform(A, G2, 'meancenter');");
    EXPECT_NEAR(evalScalar("B(1)"), -15.0, 1e-12);
    EXPECT_NEAR(evalScalar("B(2)"), -15.0, 1e-12);
    EXPECT_NEAR(evalScalar("B(3)"),   0.0, 1e-12);  // singleton NaN group
    EXPECT_NEAR(evalScalar("B(4)"),  15.0, 1e-12);
    EXPECT_NEAR(evalScalar("B(5)"),  15.0, 1e-12);
}

// ── matrix A ─────────────────────────────────────────────────────

TEST_F(GroupTransformTest, MatrixMeanCenter)
{
    eval("A2 = [10 100; 20 200; 30 300; 40 400; 50 500];"
         "B = grouptransform(A2, G, 'meancenter');");
    EXPECT_EQ(static_cast<int>(evalScalar("size(B,1)")), 5);
    EXPECT_EQ(static_cast<int>(evalScalar("size(B,2)")), 2);
    EXPECT_NEAR(evalScalar("B(1,1)"), -20.0, 1e-12);
    EXPECT_NEAR(evalScalar("B(1,2)"), -200.0, 1e-12);
}

// ── function handle ──────────────────────────────────────────────

TEST_F(GroupTransformTest, FunctionHandleMeanCenter)
{
    eval("B = grouptransform(A, G, @(x) x - mean(x));");
    EXPECT_NEAR(evalScalar("B(1)"), -20.0, 1e-12);
    EXPECT_NEAR(evalScalar("B(3)"),   0.0, 1e-12);
    EXPECT_NEAR(evalScalar("B(5)"),  20.0, 1e-12);
}

// ── errors ───────────────────────────────────────────────────────

TEST_F(GroupTransformTest, NoMethodThrows)
{
    EXPECT_THROW(eval("grouptransform(A, G);"), std::exception);
}

TEST_F(GroupTransformTest, BadMethodThrows)
{
    EXPECT_THROW(eval("grouptransform(A, G, 'gibberish');"), std::exception);
}

TEST_F(GroupTransformTest, MismatchedSizeThrows)
{
    EXPECT_THROW(eval("grouptransform(A, [1; 2; 1], 'meancenter');"), std::exception);
}
