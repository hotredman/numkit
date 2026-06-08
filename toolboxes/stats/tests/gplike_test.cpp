// toolboxes/stats/tests/gplike_test.cpp
// Backfill gtest + gplike. Reference values
// from MATLAB R2025b probe.
// Note on edges (matching MATLAB R2025b — asymmetric vs gevlike):
//   sigma == 0 → NaN
//   sigma  < 0 → -Inf (not NaN)
//   per-point support violation → +Inf (not NaN)
//   x < 0 OK as long as 1 + k*x/sigma > 0

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class GplikeTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override
    {
        engine.eval("import compat.*;");
        engine.eval("x = [1 2 3 4 5]';");
    }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(GplikeTest, NLogLKPositive)
{
    EXPECT_NEAR(evalScalar("gplike([0.5, 1], x)"), 13.0988348331, 1e-9);
}

TEST_F(GplikeTest, NLogLKZeroExpLimit)
{
    // GP at k=0 is exponential with rate 1/sigma → nL = N*log(σ) + Σx/σ.
    EXPECT_NEAR(evalScalar("gplike([0, 1], x)"), 15.0, 1e-12);
}

TEST_F(GplikeTest, ACovKPositive)
{
    eval("[nL, ac] = gplike([0.5, 1], x);");
    EXPECT_NEAR(evalScalar("ac(1,1)"),  0.5357555979, 1e-5);
    EXPECT_NEAR(evalScalar("ac(1,2)"), -0.3457416827, 1e-5);
    EXPECT_NEAR(evalScalar("ac(2,1)"), -0.3457416827, 1e-5);
    EXPECT_NEAR(evalScalar("ac(2,2)"),  0.3689249582, 1e-5);
}

TEST_F(GplikeTest, ACovKZero)
{
    eval("[nL, ac] = gplike([0, 1], x);");
    EXPECT_NEAR(evalScalar("ac(1,1)"),  0.0322580645, 1e-5);
    EXPECT_NEAR(evalScalar("ac(2,2)"),  0.1225806452, 1e-5);
}

TEST_F(GplikeTest, NegativeXAllowedIfSupportOK)
{
    // x < 0 is fine as long as the per-point support holds.
    EXPECT_NEAR(evalScalar("gplike([0.5, 1], [-1 1 2]')"), 1.2163953243, 1e-9);
}

TEST_F(GplikeTest, SupportViolationReturnsInf)
{
    // k=-1, x=10 → t = 1 - 10 = -9 → +Inf.
    EXPECT_TRUE(std::isinf(evalScalar("gplike([-1, 1], 10)")) &&
                evalScalar("gplike([-1, 1], 10)") > 0);
}

TEST_F(GplikeTest, NegativeSigmaReturnsNegInf)
{
    const double v = evalScalar("gplike([0.5, -1], x)");
    EXPECT_TRUE(std::isinf(v) && v < 0);
}

TEST_F(GplikeTest, ZeroSigmaReturnsNaN)
{
    EXPECT_TRUE(std::isnan(evalScalar("gplike([0.5, 0], x)")));
}

TEST_F(GplikeTest, EmptyDataReturnsInf)
{
    EXPECT_TRUE(std::isinf(evalScalar("gplike([0.5, 1], [])")));
}
