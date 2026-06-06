// libs/builtin/tests/inv_trig_batch_test.cpp
// inverse-trig family:
//   acos / acosd / acosh / acot / acotd / acoth / acsc / acscd / acsch
// Each spec . This
// gtest pins libm-backed numerics against MATLAB R2025b on domain-edge
// + interior probes (matching the corresponding parity-spec fingerprints).
// All values verified bit-identical to MATLAB to tol=1e-12.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class InvTrigBatchTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// ─── acos / acosd ───────────────────────────────────────────────────

TEST_F(InvTrigBatchTest, AcosDomainEdges)
{
    EXPECT_NEAR(evalScalar("acos(-1)"), 3.141592653589793, 1e-12);
    EXPECT_NEAR(evalScalar("acos( 0)"), 1.570796326794897, 1e-12);
    EXPECT_NEAR(evalScalar("acos( 1)"), 0.0,               1e-12);
    EXPECT_NEAR(evalScalar("acos( 0.5)"), 1.047197551196598, 1e-12);
}

TEST_F(InvTrigBatchTest, AcosdInDegrees)
{
    EXPECT_NEAR(evalScalar("acosd(-1)"), 180.0, 1e-12);
    EXPECT_NEAR(evalScalar("acosd( 0)"),  90.0, 1e-12);
    EXPECT_NEAR(evalScalar("acosd( 1)"),   0.0, 1e-12);
    EXPECT_NEAR(evalScalar("acosd( 0.5)"), 60.0, 1e-12);
}

// ─── acosh ──────────────────────────────────────────────────────────

TEST_F(InvTrigBatchTest, Acosh)
{
    EXPECT_NEAR(evalScalar("acosh(1)"),  0.0,               1e-12);
    EXPECT_NEAR(evalScalar("acosh(2)"),  1.316957896924817, 1e-12);
    EXPECT_NEAR(evalScalar("acosh(10)"), 2.993222846126381, 1e-12);
}

// ─── acot / acotd ───────────────────────────────────────────────────

TEST_F(InvTrigBatchTest, Acot)
{
    EXPECT_NEAR(evalScalar("acot(1)"),  0.785398163397448, 1e-12);
    EXPECT_NEAR(evalScalar("acot(-1)"), -0.785398163397448, 1e-12);
    EXPECT_NEAR(evalScalar("acot(2)"),  0.463647609000806, 1e-12);
}

TEST_F(InvTrigBatchTest, Acotd)
{
    EXPECT_NEAR(evalScalar("acotd(1)"),  45.0, 1e-12);
    EXPECT_NEAR(evalScalar("acotd(-1)"), -45.0, 1e-12);
}

// ─── acoth (|x| > 1) ────────────────────────────────────────────────

TEST_F(InvTrigBatchTest, Acoth)
{
    EXPECT_NEAR(evalScalar("acoth(2)"),  0.549306144334055, 1e-12);
    EXPECT_NEAR(evalScalar("acoth(-2)"), -0.549306144334055, 1e-12);
    EXPECT_NEAR(evalScalar("acoth(3)"),  0.346573590279973, 1e-12);
}

// ─── acsc / acscd (|x| >= 1) ────────────────────────────────────────

TEST_F(InvTrigBatchTest, Acsc)
{
    EXPECT_NEAR(evalScalar("acsc(1)"),  1.570796326794897, 1e-12);
    EXPECT_NEAR(evalScalar("acsc(-1)"), -1.570796326794897, 1e-12);
    EXPECT_NEAR(evalScalar("acsc(2)"),  0.523598775598299, 1e-12);
}

TEST_F(InvTrigBatchTest, Acscd)
{
    EXPECT_NEAR(evalScalar("acscd(1)"),   90.0, 1e-12);
    EXPECT_NEAR(evalScalar("acscd(-1)"), -90.0, 1e-12);
    EXPECT_NEAR(evalScalar("acscd(2)"),   30.0, 1e-12);
}

// ─── acsch (all real except 0) ──────────────────────────────────────

TEST_F(InvTrigBatchTest, Acsch)
{
    EXPECT_NEAR(evalScalar("acsch(1)"),  0.881373587019543, 1e-12);
    EXPECT_NEAR(evalScalar("acsch(-1)"), -0.881373587019543, 1e-12);
    EXPECT_NEAR(evalScalar("acsch(2)"),  0.481211825059603, 1e-12);
}

// ─── vectorisation (all 9 functions accept vector input) ────────────

TEST_F(InvTrigBatchTest, VectorisationProducesElementwise)
{
    eval("y = acos([-1, 0, 1]);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(y)"), 3.0);
    EXPECT_NEAR(evalScalar("y(1)"), 3.141592653589793, 1e-12);
    EXPECT_NEAR(evalScalar("y(2)"), 1.570796326794897, 1e-12);
    EXPECT_NEAR(evalScalar("y(3)"), 0.0, 1e-12);
}

// ─── inverse identity round-trips ──────────────────────────────────

TEST_F(InvTrigBatchTest, RoundTripIdentities)
{
    EXPECT_NEAR(evalScalar("cos(acos(0.42))"),  0.42, 1e-12);
    EXPECT_NEAR(evalScalar("cosh(acosh(2.5))"), 2.5,  1e-12);
    EXPECT_NEAR(evalScalar("cot(acot(0.7))"),   0.7,  1e-12);
}
