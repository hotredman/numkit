// libs/comm/tests/lloyds_test.cpp
//
// Regression guard for lloyds() — Lloyd-Max scalar quantizer designer.
//
// Bit-equal with MATLAB R2025b on deterministic training sets. Tests
// using `randn` are NOT bit-exact because numkit's randn ≠ MATLAB's
// (Ziggurat deferred); we cover only the deterministic-training path
// here. Statistical-quality checks via known monotone training sets
// where the optimal partition/codebook is known by hand.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class LloydsTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(LloydsTest, KnownInitialCodebookOnUniformTraining)
{
    // MATLAB: lloyds(1:10, [2 5 8])
    //   partition -> [3.5 6.75]
    //   codebook  -> [2 5 8.5]
    //   distor    -> 0.9
    eval("[p, c, d] = lloyds(1:10, [2 5 8]);");
    EXPECT_DOUBLE_EQ(evalScalar("p(1)"), 3.5);
    EXPECT_DOUBLE_EQ(evalScalar("p(2)"), 6.75);
    EXPECT_DOUBLE_EQ(evalScalar("c(1)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("c(2)"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("c(3)"), 8.5);
    EXPECT_DOUBLE_EQ(evalScalar("d"),    0.9);
}

TEST_F(LloydsTest, IntegerKDeterministic)
{
    // Uniform 1..10, K=2 -> two halves: codebook [3, 8], partition [5.5].
    eval("[p, c, d] = lloyds(1:10, 2);");
    EXPECT_DOUBLE_EQ(evalScalar("p(1)"), 5.5);
    EXPECT_DOUBLE_EQ(evalScalar("c(1)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("c(2)"), 8.0);
}

TEST_F(LloydsTest, OutputShapesAreRows)
{
    eval("[p, c] = lloyds(1:10, 4);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(p, 1)")), 1);
    EXPECT_EQ(static_cast<int>(evalScalar("size(c, 1)")), 1);
    EXPECT_EQ(static_cast<int>(evalScalar("size(c, 2)")), 4);
    EXPECT_EQ(static_cast<int>(evalScalar("size(p, 2)")), 3);
}

TEST_F(LloydsTest, RelDistortionUnderTolerance)
{
    eval("[~, ~, ~, r] = lloyds(1:10, [2 5 8], 1e-7);");
    EXPECT_LT(evalScalar("r"), 1e-6);  // generous bound (loop exits below tol)
}

TEST_F(LloydsTest, DistortionMatchesQuantiz)
{
    // lloyds-returned distortion should equal quantiz's distortion on
    // the same training set with the same partition/codebook.
    eval("[p, c, d] = lloyds(1:10, 4);"
         "[~, ~, dq] = quantiz(1:10, p, c);");
    EXPECT_NEAR(evalScalar("d"), evalScalar("dq"), 1e-12);
}

TEST_F(LloydsTest, MonotonicTrainingPartitionInRange)
{
    eval("[p, c] = lloyds(0:0.1:1, 4);");
    EXPECT_GE(evalScalar("min(p)"), 0.0);
    EXPECT_LE(evalScalar("max(p)"), 1.0);
}

TEST_F(LloydsTest, RejectsEmptyTraining)
{
    bool threw = false;
    try { eval("lloyds([], 4);"); } catch (...) { threw = true; }
    EXPECT_TRUE(threw);
}

TEST_F(LloydsTest, RejectsConstantTraining)
{
    bool threw = false;
    try { eval("lloyds([3 3 3 3 3], 2);"); } catch (...) { threw = true; }
    EXPECT_TRUE(threw);
}
