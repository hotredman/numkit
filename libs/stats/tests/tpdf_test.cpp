// libs/stats/tests/tpdf_test.cpp
// tpdf.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class TpdfTest : public ::testing::Test
{
public:
    StdEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(TpdfTest, Mode)
{
    EXPECT_NEAR(evalScalar("tpdf(0, 5)"), 0.3796066898224945, 1e-12);
}

TEST_F(TpdfTest, Tail)
{
    EXPECT_NEAR(evalScalar("tpdf(1, 5)"), 0.2196797973509806, 1e-12);
}

TEST_F(TpdfTest, VectorSym)
{
    eval("v = tpdf([-2 -1 0 1 2], 10);");
    EXPECT_NEAR(evalScalar("v(1)"), 0.0611457663212182, 1e-12);
    EXPECT_NEAR(evalScalar("v(3)"), 0.3891083839660311, 1e-12);
    EXPECT_NEAR(evalScalar("v(5)"), 0.0611457663212182, 1e-12);
}

TEST_F(TpdfTest, GaussianLimit)
{
    // nu = Inf -> normpdf(x)
    EXPECT_NEAR(evalScalar("tpdf(0, Inf)"),   0.3989422804014327, 1e-12);
    EXPECT_NEAR(evalScalar("tpdf(1, Inf)"),   0.2419707245191434, 1e-12);
    EXPECT_NEAR(evalScalar("tpdf(2.5, Inf)"), 0.0175283004935685, 1e-12);
}

TEST_F(TpdfTest, LargeNuApproachesGaussian)
{
    EXPECT_NEAR(evalScalar("tpdf(0, 1e10)"), 0.3989422804014327, 1e-6);
}

TEST_F(TpdfTest, EdgeCases)
{
    EXPECT_TRUE(std::isnan(evalScalar("tpdf(0,    0)")));
    EXPECT_TRUE(std::isnan(evalScalar("tpdf(0,   -1)")));
    EXPECT_TRUE(std::isnan(evalScalar("tpdf(NaN,  5)")));
}
