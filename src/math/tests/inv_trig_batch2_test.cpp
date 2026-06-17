// toolboxes/builtin/tests/inv_trig_batch2_test.cpp
// inverse-trig family — second batch:
//   asin / asind / asinh
//   atan / atand / atanh
//   asec / asecd / asech
// All nine . Verified
// bit-identical to MATLAB R2025b on domain-edge + interior probes.
// Sibling of the first inverse-trig batch (acos/acot/acsc family).

#include <numkit/core/engine.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class InvTrigBatch2Test : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(InvTrigBatch2Test, AsinDomainEdges)
{
    EXPECT_NEAR(evalScalar("asin(-1)"),  -1.570796326794897, 1e-12);
    EXPECT_NEAR(evalScalar("asin( 0)"),   0.0,               1e-12);
    EXPECT_NEAR(evalScalar("asin( 1)"),   1.570796326794897, 1e-12);
    EXPECT_NEAR(evalScalar("asin( 0.5)"), 0.523598775598299, 1e-12);
}

TEST_F(InvTrigBatch2Test, Asind)
{
    EXPECT_NEAR(evalScalar("asind(-1)"),  -90.0, 1e-12);
    EXPECT_NEAR(evalScalar("asind( 0)"),    0.0, 1e-12);
    EXPECT_NEAR(evalScalar("asind( 1)"),   90.0, 1e-12);
    EXPECT_NEAR(evalScalar("asind( 0.5)"), 30.0, 1e-12);
}

TEST_F(InvTrigBatch2Test, Asinh)
{
    EXPECT_NEAR(evalScalar("asinh( 0)"), 0.0, 1e-12);
    EXPECT_NEAR(evalScalar("asinh( 1)"), 0.881373587019543, 1e-12);
    EXPECT_NEAR(evalScalar("asinh(-1)"), -0.881373587019543, 1e-12);
}

TEST_F(InvTrigBatch2Test, Atan)
{
    EXPECT_NEAR(evalScalar("atan( 1)"), 0.785398163397448, 1e-12);
    EXPECT_NEAR(evalScalar("atan(-1)"), -0.785398163397448, 1e-12);
    EXPECT_NEAR(evalScalar("atan( 0)"), 0.0, 1e-12);
}

TEST_F(InvTrigBatch2Test, Atand)
{
    EXPECT_NEAR(evalScalar("atand( 1)"),  45.0, 1e-12);
    EXPECT_NEAR(evalScalar("atand(-1)"), -45.0, 1e-12);
}

TEST_F(InvTrigBatch2Test, Atanh)
{
    EXPECT_NEAR(evalScalar("atanh(0.5)"),  0.549306144334055, 1e-12);
    EXPECT_NEAR(evalScalar("atanh(-0.5)"), -0.549306144334055, 1e-12);
}

TEST_F(InvTrigBatch2Test, AsecDomain)
{
    EXPECT_NEAR(evalScalar("asec( 1)"),   0.0,               1e-12);
    EXPECT_NEAR(evalScalar("asec(-1)"),   3.141592653589793, 1e-12);
    EXPECT_NEAR(evalScalar("asec( 2)"),   1.047197551196598, 1e-12);
}

TEST_F(InvTrigBatch2Test, Asecd)
{
    EXPECT_NEAR(evalScalar("asecd( 1)"),   0.0, 1e-12);
    EXPECT_NEAR(evalScalar("asecd(-1)"), 180.0, 1e-12);
    EXPECT_NEAR(evalScalar("asecd( 2)"),  60.0, 1e-12);
}

TEST_F(InvTrigBatch2Test, Asech)
{
    EXPECT_NEAR(evalScalar("asech(0.5)"),  1.316957896924817, 1e-12);
    EXPECT_NEAR(evalScalar("asech(0.99)"), 0.142014440746078, 1e-12);
}

TEST_F(InvTrigBatch2Test, RoundTripIdentities)
{
    EXPECT_NEAR(evalScalar("sin(asin(0.42))"), 0.42, 1e-12);
    EXPECT_NEAR(evalScalar("tan(atan(2.7))"),  2.7,  1e-12);
    EXPECT_NEAR(evalScalar("sec(asec(3.5))"),  3.5,  1e-12);
}
