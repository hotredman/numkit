// toolboxes/builtin/tests/fwd_trig_batch_test.cpp
// forward-trig family — 18 functions:
//   sin / sind / sinh    cos / cosd / cosh    tan / tand / tanh
//   sec / secd / sech    csc / cscd / csch    cot / cotd / coth
// All  — libm-backed, bit-identical
// MATLAB R2025b on probed inputs (parity tol=1e-12).

#include <numkit/core/engine.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class FwdTrigBatchTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(FwdTrigBatchTest, SinSinhSind)
{
    EXPECT_NEAR(evalScalar("sin(0)"),    0.0,               1e-12);
    EXPECT_NEAR(evalScalar("sin(pi/2)"), 1.0,               1e-12);
    EXPECT_NEAR(evalScalar("sind(30)"),  0.5,               1e-12);
    EXPECT_NEAR(evalScalar("sind(90)"),  1.0,               1e-12);
    EXPECT_NEAR(evalScalar("sinh(0)"),   0.0,               1e-12);
    EXPECT_NEAR(evalScalar("sinh(1)"),   1.175201193643801, 1e-12);
}

TEST_F(FwdTrigBatchTest, CosCoshCosd)
{
    EXPECT_NEAR(evalScalar("cos(0)"),    1.0, 1e-12);
    EXPECT_NEAR(evalScalar("cosd(60)"),  0.5, 1e-12);
    EXPECT_NEAR(evalScalar("cosd(90)"),  0.0, 1e-12);
    EXPECT_NEAR(evalScalar("cosh(0)"),   1.0, 1e-12);
    EXPECT_NEAR(evalScalar("cosh(1)"),   1.543080634815244, 1e-12);
}

TEST_F(FwdTrigBatchTest, TanTanhTand)
{
    EXPECT_NEAR(evalScalar("tan(0)"),    0.0,               1e-12);
    EXPECT_NEAR(evalScalar("tand(45)"),  1.0,               1e-12);
    EXPECT_NEAR(evalScalar("tanh(1)"),   0.761594155955765, 1e-12);
    EXPECT_NEAR(evalScalar("tanh(-1)"), -0.761594155955765, 1e-12);
}

// tan on an ARRAY runs through the SIMD xtan kernel (Cody-Waite reduction
// + half-angle polynomial). Cover the 2-step (<15), extended (<1e6) and
// scalar-fallback (>=1e6) ranges. Values pinned from MATLAB R2025b.
TEST_F(FwdTrigBatchTest, TanArraySimd)
{
    eval("v = tan([0 0.5 1 -1 1.5 2 3 5]);"); // SIMD body, 2-step (|x|<15)
    EXPECT_NEAR(evalScalar("v(2)"),  0.5463024898437905, 1e-12);
    EXPECT_NEAR(evalScalar("v(3)"),  1.5574077246549023, 1e-12);
    EXPECT_NEAR(evalScalar("v(4)"), -1.5574077246549023, 1e-12);
    EXPECT_NEAR(evalScalar("v(5)"), 14.101419947171719,  1e-10); // near pi/2
    eval("w = tan([10 100 1000 50000]);"); // extended reduction (15..1e6)
    EXPECT_NEAR(evalScalar("w(1)"),  0.6483608274590866, 1e-12);
    EXPECT_NEAR(evalScalar("w(2)"), -0.5872139151569290, 1e-11);
    EXPECT_NEAR(evalScalar("w(3)"),  1.4703241557027185, 1e-11);
    EXPECT_NEAR(evalScalar("w(4)"), 55.9280569098652,    1e-9);
    // |x| >= 1e6 falls back to scalar std::tan; result is finite.
    EXPECT_DOUBLE_EQ(evalScalar("double(all(isfinite(tan([2e6 -3e6 5e6]))))"), 1.0);
}

TEST_F(FwdTrigBatchTest, SecSechSecd)
{
    EXPECT_NEAR(evalScalar("sec(0)"),    1.0,               1e-12);
    EXPECT_NEAR(evalScalar("secd(60)"),  2.0,               1e-12);
    EXPECT_NEAR(evalScalar("sech(0)"),   1.0,               1e-12);
    EXPECT_NEAR(evalScalar("sech(1)"),   0.648054273663885, 1e-12);
}

TEST_F(FwdTrigBatchTest, CscCschCscd)
{
    EXPECT_NEAR(evalScalar("csc(pi/2)"),  1.0,              1e-12);
    EXPECT_NEAR(evalScalar("cscd(30)"),   2.0,              1e-12);
    EXPECT_NEAR(evalScalar("csch(1)"),    0.850918128239322, 1e-12);
    EXPECT_NEAR(evalScalar("csch(-1)"),  -0.850918128239322, 1e-12);
}

TEST_F(FwdTrigBatchTest, CotCothCotd)
{
    EXPECT_NEAR(evalScalar("cot(pi/4)"),  1.0,               1e-12);
    EXPECT_NEAR(evalScalar("cotd(45)"),   1.0,               1e-12);
    EXPECT_NEAR(evalScalar("coth(1)"),    1.313035285499331, 1e-12);
    EXPECT_NEAR(evalScalar("coth(2)"),    1.037314720727548, 1e-12);
}

TEST_F(FwdTrigBatchTest, RoundTripIdentities)
{
    EXPECT_NEAR(evalScalar("sin(asin(0.42))"), 0.42, 1e-12);
    EXPECT_NEAR(evalScalar("cos(acos(0.7))"),  0.7,  1e-12);
    EXPECT_NEAR(evalScalar("tan(atan(2.7))"),  2.7,  1e-12);
}

TEST_F(FwdTrigBatchTest, PythagoreanIdentity)
{
    EXPECT_NEAR(evalScalar("sin(0.7)^2 + cos(0.7)^2"), 1.0, 1e-12);
}
