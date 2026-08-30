// toolboxes/comm/tests/mskmod_test.cpp
//
// Regression guard for mskmod() — minimum-shift keying modulator,
// differential variant. Bit-equal with MATLAB R2025b.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class MskmodTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override {}
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(MskmodTest, KnownConstellationPath)
{
    // MATLAB: mskmod([1 0 1 1 0 0 1 0]', 4) traces a continuous-phase
    // path on the unit circle. Sample known anchors.
    eval("y = mskmod([1 0 1 1 0 0 1 0]', 4);");
    EXPECT_EQ(static_cast<int>(evalScalar("numel(y)")), 32);
    // y(1) = 1+0i  (phase 0)
    EXPECT_NEAR(evalScalar("real(y(1))"), 1.0, 1e-12);
    EXPECT_NEAR(evalScalar("imag(y(1))"), 0.0, 1e-12);
    // y(2) = exp(i*pi/8)
    EXPECT_NEAR(evalScalar("real(y(2))"),  0.92387953251128674, 1e-12);
    EXPECT_NEAR(evalScalar("imag(y(2))"),  0.38268343236508984, 1e-12);
    // y(5) = exp(i*pi/2) = i  (end of first symbol when bit=1)
    EXPECT_NEAR(evalScalar("real(y(5))"), 0.0, 1e-12);
    EXPECT_NEAR(evalScalar("imag(y(5))"), 1.0, 1e-12);
    // y(17) = exp(i*pi) = -1
    EXPECT_NEAR(evalScalar("real(y(17))"), -1.0, 1e-12);
    EXPECT_NEAR(evalScalar("imag(y(17))"),  0.0, 1e-12);
}

TEST_F(MskmodTest, OutputLengthIsNtimesNSamp)
{
    eval("y = mskmod([0 1 1 0]', 8);");
    EXPECT_EQ(static_cast<int>(evalScalar("numel(y)")), 32);
}

TEST_F(MskmodTest, AllOnUnitCircle)
{
    eval("y = mskmod([1 1 0 1 0 0 1 0 1 1 0 0]', 4);"
         "err = max(abs(abs(y) - 1));");
    EXPECT_LT(evalScalar("err"), 1e-12);
}

TEST_F(MskmodTest, IniPhaseShifts)
{
    // ini_phase = pi/4 -> y(1) = cos(pi/4) + i sin(pi/4)
    eval("y = mskmod([0]', 4, pi/4);");
    EXPECT_NEAR(evalScalar("real(y(1))"), 0.7071067811865476, 1e-12);
    EXPECT_NEAR(evalScalar("imag(y(1))"), 0.7071067811865475, 1e-12);
}

TEST_F(MskmodTest, RowOrientationPreserved)
{
    eval("y = mskmod([1 0 1 0], 4);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(y, 1)")), 1);
    EXPECT_EQ(static_cast<int>(evalScalar("size(y, 2)")), 16);
}

TEST_F(MskmodTest, ColumnOrientationPreserved)
{
    eval("y = mskmod([1; 0; 1; 0], 4);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(y, 1)")), 16);
    EXPECT_EQ(static_cast<int>(evalScalar("size(y, 2)")), 1);
}

TEST_F(MskmodTest, RejectsNonBinaryInput)
{
    bool threw = false;
    try { eval("mskmod([0 1 2]', 4);"); } catch (...) { threw = true; }
    EXPECT_TRUE(threw);
}

TEST_F(MskmodTest, RejectsNonPositiveNSamp)
{
    bool threw = false;
    try { eval("mskmod([0 1]', 0);"); } catch (...) { threw = true; }
    EXPECT_TRUE(threw);
}

TEST_F(MskmodTest, ContinuousPhaseInvariant)
{
    // Phase should change linearly within each symbol -- consecutive
    // samples differ by a constant unwrapped angle.
    eval("y = mskmod([1 0 1]', 8);"
         "ph = unwrap(angle(y));"
         "diffs = diff(ph);"
         "% Differences should be near pi/2 / 8 = pi/16 in magnitude.\n"
         "ok = all(abs(abs(diffs) - pi/16) < 1e-12);");
    EXPECT_DOUBLE_EQ(evalScalar("ok"), 1.0);
}
