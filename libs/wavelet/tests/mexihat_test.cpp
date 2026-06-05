// libs/wavelet/tests/mexihat_test.cpp
// mexihat.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class MexihatTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// ψ(t) = (2/√3)·π^(-1/4)·(1-t²)·exp(-t²/2)
// Even function, peaks at t=0 with value 2/(√3·π^(1/4)) ≈ 0.8673,
// zero crossings at t=±1.

TEST_F(MexihatTest, BasicReferenceValues)
{
    eval("[psi, x] = mexihat(-5, 5, 8);");
    EXPECT_NEAR(evalScalar("psi(1)"), -0.0000775732734124, 1e-12);
    EXPECT_NEAR(evalScalar("psi(4)"),  0.3291604543823751, 1e-12);
    EXPECT_NEAR(evalScalar("psi(8)"), -0.0000775732734124, 1e-12);  // symmetric
    EXPECT_DOUBLE_EQ(evalScalar("x(1)"), -5.0);
    EXPECT_DOUBLE_EQ(evalScalar("x(8)"),  5.0);
}

TEST_F(MexihatTest, IsEvenFunction)
{
    // Even N (16): the central pair should match.
    eval("[psi, x] = mexihat(-5, 5, 16);");
    EXPECT_NEAR(evalScalar("psi(8)"), evalScalar("psi(9)"), 1e-12);
    // Endpoint pairs.
    EXPECT_NEAR(evalScalar("psi(1)"), evalScalar("psi(16)"), 1e-12);
}

TEST_F(MexihatTest, AsymmetricRange)
{
    // [0, 5] starts at peak (≈ 0.8673) and falls.
    eval("[psi, x] = mexihat(0, 5, 16);");
    EXPECT_NEAR(evalScalar("psi(1)"),  0.8673250705840777, 1e-12);
    EXPECT_NEAR(evalScalar("psi(16)"), -0.0000775732734124, 1e-12);
    EXPECT_DOUBLE_EQ(evalScalar("x(1)"),  0.0);
    EXPECT_DOUBLE_EQ(evalScalar("x(16)"), 5.0);
}

TEST_F(MexihatTest, FineGridSampledAtCentre)
{
    eval("[psi, x] = mexihat(-5, 5, 64);");
    // x grid contains 64 evenly spaced points from -5 to 5; midpoint pair.
    EXPECT_NEAR(evalScalar("psi(32)"), 0.8591518646849210, 1e-12);
}
