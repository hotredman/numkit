// libs/stats/tests/evfit_gpfit_test.cpp
//
// Regression guard for evfit + gpfit.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

#include <cmath>

using namespace numkit;

class EvfitGpfitTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override { engine.eval("import compat.*; rng(0);"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// ── evfit ───────────────────────────────────────────────────────────

TEST_F(EvfitGpfitTest, EvfitRecoversParams)
{
    eval("x = evrnd(1.0, 2.0, 3000, 1); f = evfit(x);");
    EXPECT_NEAR(evalScalar("f(1)"), 1.0, 0.25);
    EXPECT_NEAR(evalScalar("f(2)"), 2.0, 0.3);
}

TEST_F(EvfitGpfitTest, EvfitShapeOneByTwo)
{
    eval("f = evfit(evrnd(0.0, 1.0, 100, 1));");
    EXPECT_EQ(static_cast<int>(evalScalar("size(f, 1)")), 1);
    EXPECT_EQ(static_cast<int>(evalScalar("size(f, 2)")), 2);
}

TEST_F(EvfitGpfitTest, EvfitZeroVarianceThrows)
{
    EXPECT_THROW(eval("evfit([3.0, 3.0, 3.0, 3.0, 3.0]);"), std::exception);
}

TEST_F(EvfitGpfitTest, EvfitTooFewObsThrows)
{
    EXPECT_THROW(eval("evfit([1.0]);"), std::exception);
}

// ── gpfit ───────────────────────────────────────────────────────────

TEST_F(EvfitGpfitTest, GpfitRecoversParams)
{
    eval("y = gprnd(0.3, 1.5, 0, 3000, 1); f = gpfit(y);");
    EXPECT_NEAR(evalScalar("f(1)"), 0.3, 0.2);
    EXPECT_NEAR(evalScalar("f(2)"), 1.5, 0.4);
}

TEST_F(EvfitGpfitTest, GpfitShapeOneByTwo)
{
    eval("f = gpfit(gprnd(0.0, 1.0, 0, 100, 1));");
    EXPECT_EQ(static_cast<int>(evalScalar("size(f, 1)")), 1);
    EXPECT_EQ(static_cast<int>(evalScalar("size(f, 2)")), 2);
}

TEST_F(EvfitGpfitTest, GpfitExponentialLimit)
{
    // k = 0 → exponential with mean σ; expect k̂ near 0.
    eval("y = exprnd(1.0, 5000, 1); f = gpfit(y);");
    EXPECT_NEAR(evalScalar("f(1)"), 0.0, 0.2);
    EXPECT_NEAR(evalScalar("f(2)"), 1.0, 0.2);
}

TEST_F(EvfitGpfitTest, GpfitNegativeXThrows)
{
    EXPECT_THROW(eval("gpfit([0.5, 1.0, -0.3, 2.0]);"), std::exception);
}
