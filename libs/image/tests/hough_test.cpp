// libs/image/tests/hough_test.cpp
//
// Regression guard for hough + houghpeaks — Standard Hough Transform
// for line detection. Bit-exact MATLAB R2025b (tol = 0).

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class HoughTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override {
        engine.eval(
            "import compat.*;"
            "BW = false(11, 11);"
            "BW(6, 1:11) = true;"   // horizontal line
            "BW(1:11, 6) = true;");  // vertical line
    }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// ── Default Hough output shape ─────────────────────────────────────

TEST_F(HoughTest, DefaultShape)
{
    eval("[H, T, R] = hough(BW);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(H, 1)")), 31);
    EXPECT_EQ(static_cast<int>(evalScalar("size(H, 2)")), 180);
    EXPECT_EQ(static_cast<int>(evalScalar("length(T)")), 180);
    EXPECT_EQ(static_cast<int>(evalScalar("length(R)")), 31);
    EXPECT_NEAR(evalScalar("T(1)"),     -90.0, 1e-12);
    EXPECT_NEAR(evalScalar("T(end)"),    89.0, 1e-12);
    EXPECT_NEAR(evalScalar("R(1)"),     -15.0, 1e-12);
    EXPECT_NEAR(evalScalar("R(end)"),    15.0, 1e-12);
}

// ── Accumulator peak from horizontal+vertical lines ───────────────

TEST_F(HoughTest, AccumulatorPeak)
{
    eval("[H, T, R] = hough(BW);");
    EXPECT_EQ(static_cast<int>(evalScalar("max(H(:))")), 11);
}

// ── Custom RhoResolution ───────────────────────────────────────────

TEST_F(HoughTest, CustomRhoResolution)
{
    eval("[H, T, R] = hough(BW, 'RhoResolution', 2);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(H, 1)")), 17);
    EXPECT_EQ(static_cast<int>(evalScalar("max(H(:))")), 12);
}

// ── Custom Theta range ─────────────────────────────────────────────

TEST_F(HoughTest, CustomTheta)
{
    eval("[H, T, R] = hough(BW, 'Theta', -45:45);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(H, 2)")), 91);
    EXPECT_EQ(static_cast<int>(evalScalar("max(H(:))")), 11);
}

// ── houghpeaks default ─────────────────────────────────────────────

TEST_F(HoughTest, HoughpeaksDefault)
{
    eval("[H, T, R] = hough(BW); P = houghpeaks(H, 2);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(P, 1)")), 2);
    EXPECT_EQ(static_cast<int>(evalScalar("size(P, 2)")), 2);
    EXPECT_EQ(static_cast<int>(evalScalar("P(1,1)")), 11);
    EXPECT_EQ(static_cast<int>(evalScalar("P(1,2)")), 1);
    EXPECT_EQ(static_cast<int>(evalScalar("P(2,1)")), 21);
    EXPECT_EQ(static_cast<int>(evalScalar("P(2,2)")), 89);
}

// ── houghpeaks with Threshold ──────────────────────────────────────

TEST_F(HoughTest, HoughpeaksThreshold)
{
    eval("[H, T, R] = hough(BW); P = houghpeaks(H, 5, 'Threshold', 8);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(P, 1)")), 5);
}

// ── houghpeaks with custom NHoodSize ──────────────────────────────

TEST_F(HoughTest, HoughpeaksNHoodSize)
{
    eval("[H, T, R] = hough(BW); P = houghpeaks(H, 5, 'NHoodSize', [3 3]);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(P, 1)")), 5);
}

// ── houghpeaks with explicit Theta (antisymmetric wrap) ───────────

TEST_F(HoughTest, HoughpeaksWithTheta)
{
    eval("[H, T, R] = hough(BW); P = houghpeaks(H, 2, 'Theta', T);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(P, 1)")), 2);
}

// ── Errors ─────────────────────────────────────────────────────────

TEST_F(HoughTest, BadRhoResThrows)
{
    EXPECT_THROW(eval("hough(BW, 'RhoResolution', -1);"), std::exception);
}

TEST_F(HoughTest, ThetaOutOfRangeThrows)
{
    EXPECT_THROW(eval("hough(BW, 'Theta', [-90 90]);"), std::exception);
}
