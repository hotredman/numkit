// libs/stats/tests/mle_test.cpp
//
// Regression guard for mle (closed-form max-likelihood estimator).

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class MleTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(MleTest, NormalDefault)
{
    eval("x = [1.2; 0.8; 1.5; 0.9; 1.1; 1.3; 1.0; 0.7; 1.4; 1.0]; m = mle(x);");
    EXPECT_NEAR(evalScalar("m(1)"), 1.09, 1e-10);
    EXPECT_NEAR(evalScalar("m(2)"), 0.246779, 1e-5);
}

TEST_F(MleTest, NormalExplicit)
{
    eval("x = [1 2 3 4 5]; m = mle(x, 'distribution', 'normal');");
    EXPECT_NEAR(evalScalar("m(1)"), 3.0, 1e-12);
    // sigma = sqrt(mean((x - 3)^2)) = sqrt(2)
    EXPECT_NEAR(evalScalar("m(2)"), std::sqrt(2.0), 1e-12);
}

TEST_F(MleTest, Exponential)
{
    eval("xe = [0.5; 1.0; 1.5; 2.0; 2.5; 0.3; 0.8; 1.2; 1.7; 0.6]; "
         "m = mle(xe, 'distribution', 'exponential');");
    EXPECT_NEAR(evalScalar("m(1)"), 1.21, 1e-10);
}

TEST_F(MleTest, Poisson)
{
    eval("xp = [0; 1; 2; 3; 4; 5; 1; 2; 3; 2]; "
         "m = mle(xp, 'distribution', 'poisson');");
    EXPECT_NEAR(evalScalar("m(1)"), 2.3, 1e-12);
}

TEST_F(MleTest, Lognormal)
{
    eval("xl = exp([0.1; 0.2; 0.3; 0.4; 0.5]); "
         "m = mle(xl, 'distribution', 'lognormal');");
    EXPECT_NEAR(evalScalar("m(1)"), 0.3, 1e-12);
}

TEST_F(MleTest, ExponentialNegativeRejected)
{
    EXPECT_THROW(eval("mle([-1; 2; 3], 'distribution', 'exponential');"), std::exception);
}

TEST_F(MleTest, LognormalZeroRejected)
{
    EXPECT_THROW(eval("mle([0; 1; 2], 'distribution', 'lognormal');"), std::exception);
}

TEST_F(MleTest, PoissonNonIntegerRejected)
{
    EXPECT_THROW(eval("mle([0.5; 1.5; 2.5], 'distribution', 'poisson');"), std::exception);
}

TEST_F(MleTest, UnknownDistRejected)
{
    EXPECT_THROW(eval("mle([1 2 3], 'distribution', 'gamma');"), std::exception);
}

TEST_F(MleTest, CustomFnDeferred)
{
    eval("pdf_fn = @(x, mu) exp(-(x - mu).^2 / 2);");
    EXPECT_THROW(eval("mle([1 2 3], 'pdf', pdf_fn, 'start', 1);"), std::exception);
}
