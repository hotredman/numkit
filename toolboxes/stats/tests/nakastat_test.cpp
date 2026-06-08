// toolboxes/stats/tests/nakastat_test.cpp
// Coverage for nakastat (MATLAB R2025b does not ship it).
// Reference values from Octave statistics package.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class NakastatTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(NakastatTest, ScalarMomentsNaka11)
{
    // Naka(1, 1) ≡ Rayleigh(σ=√(1/2)): m = sqrt(π/4) ≈ 0.886.
    eval("[m, v] = nakastat(1, 1);");
    EXPECT_NEAR(evalScalar("m"), 0.8862269254527580, 1e-9);
    EXPECT_NEAR(evalScalar("v"), 0.2146018366025515, 1e-9);
}

TEST_F(NakastatTest, VectorBroadcasting)
{
    eval("[m, v] = nakastat([0.5 1 2], 1);");
    EXPECT_NEAR(evalScalar("m(1)"), 0.7978845608028654, 1e-9);
    EXPECT_NEAR(evalScalar("m(2)"), 0.8862269254527580, 1e-9);
    EXPECT_NEAR(evalScalar("m(3)"), 0.9399856029866254, 1e-9);
}

TEST_F(NakastatTest, InvalidParamsReturnNaN)
{
    EXPECT_TRUE(std::isnan(evalScalar("nakastat( 0, 1)")));
    EXPECT_TRUE(std::isnan(evalScalar("nakastat( 1, 0)")));
    EXPECT_TRUE(std::isnan(evalScalar("nakastat(-1, 1)")));
}
