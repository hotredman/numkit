// toolboxes/image/tests/cornermetric_test.cpp
//
// Regression guard for cornermetric — Harris & Shi-Tomasi corner
// detectors. Reference values from MATLAB R2025b at 1e-7 tolerance
// (slight accumulated FP error from the imfilter pad-then-same
// workaround for MATLAB's `full + replicate` behaviour).

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class CornermetricTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override {
        engine.eval(

            "I = double([0 0 0 0 0; 0 1 1 0 0; 0 1 1 0 0; 0 0 0 0 0; 0 0 0 0 0]);");
    }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// ── default Harris ─────────────────────────────────────────────────

TEST_F(CornermetricTest, HarrisDefault)
{
    eval("C = cornermetric(I);");
    EXPECT_NEAR(evalScalar("C(1,1)"), 0.5066534253, 1e-6);
    EXPECT_NEAR(evalScalar("C(2,2)"), 0.6215222835, 1e-6);
    EXPECT_NEAR(evalScalar("C(3,3)"), 0.3505751252, 1e-6);
}

// ── MinimumEigenvalue (Shi-Tomasi) ────────────────────────────────

TEST_F(CornermetricTest, MinimumEigenvalue)
{
    eval("C = cornermetric(I, 'MinimumEigenvalue');");
    EXPECT_NEAR(evalScalar("C(2,2)"), 0.7100456656, 1e-6);
    EXPECT_NEAR(evalScalar("C(3,3)"), 0.6422120438, 1e-6);
}

// ── custom SensitivityFactor ──────────────────────────────────────

TEST_F(CornermetricTest, CustomSensitivity)
{
    eval("C = cornermetric(I, 'Harris', 'SensitivityFactor', 0.1);");
    EXPECT_NEAR(evalScalar("C(2,2)"), 0.4356993915, 1e-6);
}

// ── custom FilterCoefficients (uniform [1 1 1]/3) ─────────────────

TEST_F(CornermetricTest, CustomFilter)
{
    eval("C = cornermetric(I, 'Harris', 'FilterCoefficients', [1 1 1]/3);");
    EXPECT_NEAR(evalScalar("C(2,2)"), 0.827654321, 1e-9);
}

// ── uint8 input → double output ───────────────────────────────────

TEST_F(CornermetricTest, Uint8Class)
{
    eval("Iu = uint8(I * 255); C = cornermetric(Iu);");
    EXPECT_NEAR(evalScalar("C(2,2)"), 0.6215222835, 1e-6);
    EXPECT_EQ(eval("class(C)").toString(), "double");
}

// ── peaks(8) — larger smooth image ────────────────────────────────

TEST_F(CornermetricTest, PeaksImage)
{
    eval("Ip = peaks(8); C = cornermetric(Ip);");
    EXPECT_NEAR(evalScalar("C(4,4)"), 80.70753415, 1e-5);
    EXPECT_NEAR(evalScalar("C(5,5)"), 92.02543873, 1e-5);
}

// ── output is same size as input ──────────────────────────────────

TEST_F(CornermetricTest, OutputSizeMatchesInput)
{
    eval("C = cornermetric(I);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(C, 1)")), 5);
    EXPECT_EQ(static_cast<int>(evalScalar("size(C, 2)")), 5);
}

// ── Errors ─────────────────────────────────────────────────────────

TEST_F(CornermetricTest, UnknownMethodThrows)
{
    EXPECT_THROW(eval("cornermetric(I, 'Foobar');"), std::exception);
}

TEST_F(CornermetricTest, OutOfRangeSensitivityThrows)
{
    EXPECT_THROW(eval("cornermetric(I, 'Harris', 'SensitivityFactor', 0.5);"),
                 std::exception);
}

TEST_F(CornermetricTest, EvenFilterLengthThrows)
{
    EXPECT_THROW(eval("cornermetric(I, 'Harris', 'FilterCoefficients', [1 1 1 1]);"),
                 std::exception);
}
