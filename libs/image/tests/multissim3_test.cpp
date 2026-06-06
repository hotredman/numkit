// libs/image/tests/multissim3_test.cpp
//
// Regression guard for multissim3 — volumetric multi-scale SSIM
// (Wang/Simoncelli/Bovik 2003, 3-D extension). References from
// MATLAB R2025b on a deterministic gradient volume.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class MultiSSIM3Test : public ::testing::Test
{
public:
    StdEngine engine;
    void SetUp() override
    {
        engine.eval(
            "import compat.*;"
            "[X, Y, Z] = ndgrid(1:16, 1:16, 1:16);"
            "A = uint8(min(255, X + Y + Z));"
            "B = uint8(min(255, X + Y + 2*Z));");
    }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(MultiSSIM3Test, DefaultScore)
{
    EXPECT_NEAR(evalScalar("multissim3(A, B)"), 0.9332945347, 1e-5);
}

TEST_F(MultiSSIM3Test, NumScales1)
{
    EXPECT_NEAR(evalScalar("multissim3(A, B, 'NumScales', 1)"),
                0.9325174093, 1e-5);
}

TEST_F(MultiSSIM3Test, NumScales3)
{
    EXPECT_NEAR(evalScalar("multissim3(A, B, 'NumScales', 3)"),
                0.9366688728, 1e-5);
}

TEST_F(MultiSSIM3Test, WangWeights)
{
    EXPECT_NEAR(
        evalScalar("multissim3(A, B, 'ScaleWeights', "
                   "[0.0448 0.2856 0.3001 0.2363 0.1333])"),
        0.9366778731, 1e-5);
}

TEST_F(MultiSSIM3Test, SigmaHalf)
{
    EXPECT_NEAR(evalScalar("multissim3(A, B, 'Sigma', 0.5)"),
                0.9694747925, 1e-5);
}

TEST_F(MultiSSIM3Test, DynamicRange128)
{
    EXPECT_NEAR(evalScalar("multissim3(A, B, 'DynamicRange', 128)"),
                0.9089443684, 1e-5);
}

TEST_F(MultiSSIM3Test, IdenticalIsOne)
{
    EXPECT_NEAR(evalScalar("multissim3(A, A)"), 1.0, 1e-12);
}

TEST_F(MultiSSIM3Test, DoubleInDoubleOut)
{
    eval("Ad = double(A)/255; Bd = double(B)/255;"
         "s = multissim3(Ad, Bd);");
    EXPECT_EQ(eval("class(s)").toString(), "double");
    EXPECT_NEAR(evalScalar("s"), 0.9332950389, 1e-5);
}

TEST_F(MultiSSIM3Test, SingleInSingleOut)
{
    eval("As = single(A)/255; Bs = single(B)/255;"
         "s = multissim3(As, Bs);");
    EXPECT_EQ(eval("class(s)").toString(), "single");
}

TEST_F(MultiSSIM3Test, QualityMapEntries)
{
    eval("[s, qmap] = multissim3(A, B);");
    EXPECT_EQ(static_cast<int>(evalScalar("length(qmap)")), 5);
    EXPECT_EQ(static_cast<int>(evalScalar("size(qmap{1}, 1)")), 16);
    EXPECT_EQ(static_cast<int>(evalScalar("size(qmap{1}, 3)")), 16);
    EXPECT_EQ(static_cast<int>(evalScalar("size(qmap{2}, 1)")), 8);
    EXPECT_EQ(static_cast<int>(evalScalar("size(qmap{3}, 3)")), 4);
}

TEST_F(MultiSSIM3Test, SizeMismatchThrows)
{
    EXPECT_THROW(eval("multissim3(A, uint8(zeros(8, 8, 8)));"),
                 std::exception);
}

TEST_F(MultiSSIM3Test, ClassMismatchThrows)
{
    EXPECT_THROW(eval("multissim3(A, single(B));"), std::exception);
}

TEST_F(MultiSSIM3Test, ScaleWeightsMismatchThrows)
{
    EXPECT_THROW(
        eval("multissim3(A, B, 'NumScales', 3, 'ScaleWeights', [1 1]);"),
        std::exception);
}

TEST_F(MultiSSIM3Test, NegativeSigmaThrows)
{
    EXPECT_THROW(eval("multissim3(A, B, 'Sigma', 0);"), std::exception);
}

TEST_F(MultiSSIM3Test, NumScalesTooLargeThrows)
{
    EXPECT_THROW(eval("multissim3(A, B, 'NumScales', 20);"),
                 std::exception);
}
