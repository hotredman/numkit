// libs/stats/tests/betalike_test.cpp
// Backfill gtest + betalike. Reference values
// from MATLAB R2025b probe.
// Note: betalike's AVAR uses BHHH (outer-product-of-gradients), NOT
// the Hessian — verified by direct MATLAB probe. The two estimators
// only coincide at the MLE.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class BetalikeTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override
    {
        engine.eval("import compat.*;");
        engine.eval("x = [0.1 0.3 0.5 0.7 0.9]';");
    }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(BetalikeTest, BasicNLogL)
{
    EXPECT_NEAR(evalScalar("betalike([2, 2], x)"), 0.3646837288, 1e-9);
}

TEST_F(BetalikeTest, AVarBasicSymmetricBeta)
{
    eval("[nL, av] = betalike([2, 2], x);");
    EXPECT_NEAR(evalScalar("av(1,1)"),  0.9234392430, 1e-9);
    EXPECT_NEAR(evalScalar("av(1,2)"),  0.7431196647, 1e-9);
    EXPECT_NEAR(evalScalar("av(2,1)"),  0.7431196647, 1e-9);  // symmetry
    EXPECT_NEAR(evalScalar("av(2,2)"),  0.9234392430, 1e-9);
}

TEST_F(BetalikeTest, InvalidShapeReturnsNaN)
{
    EXPECT_TRUE(std::isnan(evalScalar("betalike([0, 2], x)")));
    EXPECT_TRUE(std::isnan(evalScalar("betalike([2, -1], x)")));
}

TEST_F(BetalikeTest, DataOutOfUnitInterval)
{
    EXPECT_TRUE(std::isnan(evalScalar("betalike([2, 2], [0.5 1.5]')")));
    EXPECT_TRUE(std::isnan(evalScalar("betalike([2, 2], [0.0 0.5]')")));
}

TEST_F(BetalikeTest, EmptyDataReturnsInf)
{
    EXPECT_TRUE(std::isinf(evalScalar("betalike([2, 2], [])")));
}
