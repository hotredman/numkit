// libs/builtin/tests/fwd_trig_batch_test.cpp
//
// Audit ТЗ batch closure for the forward-trig family — 18 functions:
//   sin / sind / sinh    cos / cosd / cosh    tan / tand / tanh
//   sec / secd / sech    csc / cscd / csch    cot / cotd / coth
//
// All flagged "no major gap detected" — libm-backed, bit-identical
// MATLAB R2025b on probed inputs (parity tol=1e-12).

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class FwdTrigBatchTest : public ::testing::Test
{
public:
    Engine engine;
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
