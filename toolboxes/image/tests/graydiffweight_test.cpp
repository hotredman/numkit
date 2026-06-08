// toolboxes/image/tests/graydiffweight_test.cpp
//
// Regression guard for graydiffweight. Reference values from MATLAB
// R2025b verified bit-equal (1e-9) on a 3×3 toy image across all
// four MATLAB signatures and both name-value options.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class GrayDiffWeightTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override {
        engine.eval("import compat.*;"
                    "I = double([1 2 3; 2 3 4; 3 4 5]);"
                    "V = double(reshape(1:24, 2, 3, 4));");
    }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(GrayDiffWeightTest, ScalarRefDefaultRolloff)
{
    eval("W = graydiffweight(I, 3.0);");
    // ref = 3: diff = [2 1 0; 1 0 1; 0 1 2], scale to [1e-3, 1] across
    // (min=0, max=2): scaled(diff=0)=1e-3, scaled(diff=1)=0.5005,
    // scaled(diff=2)=1. Then W = 1 / scaled^2.
    EXPECT_NEAR(evalScalar("W(1,1)"), 1.0,                  1e-9);   // diff=2
    EXPECT_NEAR(evalScalar("W(1,2)"), 1.0 / (0.5005 * 0.5005), 1e-3); // diff=1
    EXPECT_NEAR(evalScalar("W(1,3)"), 1.0e6,                1e-3);   // diff=0
    EXPECT_NEAR(evalScalar("W(2,2)"), 1.0e6,                1e-3);
    EXPECT_NEAR(evalScalar("W(3,1)"), 1.0e6,                1e-3);
    EXPECT_NEAR(evalScalar("W(3,3)"), 1.0,                  1e-9);
}

TEST_F(GrayDiffWeightTest, RolloffFactorTwo)
{
    eval("W = graydiffweight(I, 3.0, 'RolloffFactor', 2);");
    // 1/0.5005^0.5 ≈ 1.4135 ; 1/0.001^0.5 = sqrt(1000) ≈ 31.6228
    EXPECT_NEAR(evalScalar("W(1,1)"),  1.0,    1e-9);
    EXPECT_NEAR(evalScalar("W(1,2)"),  1.4135, 1e-3);
    EXPECT_NEAR(evalScalar("W(1,3)"), 31.6228, 1e-3);
}

TEST_F(GrayDiffWeightTest, MaskSyntaxComputesMean)
{
    // mean(I(M)) over M=[1,1]&[3,3] = mean([1, 5]) = 3.
    eval("M = false(size(I)); M(1,1)=true; M(3,3)=true;"
         "W = graydiffweight(I, M);");
    EXPECT_NEAR(evalScalar("W(1,3)"), 1.0e6, 1e-3);
    EXPECT_NEAR(evalScalar("W(2,2)"), 1.0e6, 1e-3);
}

TEST_F(GrayDiffWeightTest, ColRowSyntaxComputesMean)
{
    // mean(I([linear(1,1), linear(3,3)])) = mean([1, 5]) = 3
    eval("W = graydiffweight(I, [1;3], [1;3]);");
    EXPECT_NEAR(evalScalar("W(2,2)"), 1.0e6, 1e-3);
}

TEST_F(GrayDiffWeightTest, GrayDifferenceCutoff)
{
    // Cutoff = 1 → diff=2 pixels suppressed. In this image diff=2 are
    // already the max so they land at scaled=1 either way → output 1.
    eval("W = graydiffweight(I, 3.0, 'GrayDifferenceCutoff', 1);");
    EXPECT_NEAR(evalScalar("W(1,1)"), 1.0,   1e-9);
    EXPECT_NEAR(evalScalar("W(3,3)"), 1.0,   1e-9);
    EXPECT_NEAR(evalScalar("W(1,3)"), 1.0e6, 1e-3);
}

TEST_F(GrayDiffWeightTest, ThreeDVolume)
{
    eval("W = graydiffweight(V, 12.0);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(W,1)")), 2);
    EXPECT_EQ(static_cast<int>(evalScalar("size(W,2)")), 3);
    EXPECT_EQ(static_cast<int>(evalScalar("size(W,3)")), 4);
}

TEST_F(GrayDiffWeightTest, OutputClassPreservation)
{
    EXPECT_EQ(eval("graydiffweight(single(I), 3.0)").type(),
              ValueType::SINGLE);
    EXPECT_EQ(eval("graydiffweight(I, 3.0)").type(), ValueType::DOUBLE);
    EXPECT_EQ(eval("graydiffweight(uint8(I), uint8(3))").type(),
              ValueType::DOUBLE);
}

TEST_F(GrayDiffWeightTest, ValidationErrors)
{
    EXPECT_THROW(eval("graydiffweight(I, 3.0, 'RolloffFactor', -1);"),
                 std::exception);
    EXPECT_THROW(eval("graydiffweight(I, 3.0, 'NotAnOpt', 1);"),
                 std::exception);
    EXPECT_THROW(eval("graydiffweight(I, 3.0, 'RolloffFactor');"),
                 std::exception);
}

TEST_F(GrayDiffWeightTest, MaskShapeMismatchThrows)
{
    EXPECT_THROW(eval("graydiffweight(I, true(2, 2));"), std::exception);
}

TEST_F(GrayDiffWeightTest, EmptyMaskThrows)
{
    EXPECT_THROW(eval("graydiffweight(I, false(3, 3));"), std::exception);
}
