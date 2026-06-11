// toolboxes/stats/tests/ricestat_test.cpp
// Coverage for ricestat (MATLAB R2025b does not ship it).
// Reference values from Octave statistics package.

#include <numkit/core/engine.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class RicestatTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(RicestatTest, ScalarMomentsRice21)
{
    eval("[m, v] = ricestat(2, 1);");
    EXPECT_NEAR(evalScalar("m"), 2.2723836614886763, 1e-6);  // besseli precision
    EXPECT_NEAR(evalScalar("v"), 0.8362739135082020, 1e-6);
}

TEST_F(RicestatTest, S0ReducesToRayleigh)
{
    // Rice(0, σ) ≡ Rayleigh(σ): m = σ·√(π/2), v = σ²·(2 - π/2).
    eval("[m, v] = ricestat(0, 1);");
    EXPECT_NEAR(evalScalar("m"), 1.2533141373155001, 1e-9);
    EXPECT_NEAR(evalScalar("v"), 0.4292036732051034, 1e-9);
}

TEST_F(RicestatTest, VectorBroadcasting)
{
    eval("[m, v] = ricestat([0 1 2], 1);");
    EXPECT_NEAR(evalScalar("m(1)"), 1.2533141373155001, 1e-9);
    EXPECT_NEAR(evalScalar("m(3)"), 2.2723836614886763, 1e-6);  // besseli precision
}

TEST_F(RicestatTest, InvalidParamsReturnNaN)
{
    EXPECT_TRUE(std::isnan(evalScalar("ricestat( 2, 0)")));
    EXPECT_TRUE(std::isnan(evalScalar("ricestat( 2, -1)")));
    EXPECT_TRUE(std::isnan(evalScalar("ricestat(-1, 1)")));
}
