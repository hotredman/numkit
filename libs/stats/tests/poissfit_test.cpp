// libs/stats/tests/poissfit_test.cpp
// poissfit.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class PoissfitTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(PoissfitTest, BasicMLE)
{
    eval("x = [3 4 5 4 5 6 7 5 4 3 5 6 4 3 5]';");
    eval("[lh, lci] = poissfit(x);");
    EXPECT_NEAR(evalScalar("lh"),     4.6,                1e-12);
    EXPECT_NEAR(evalScalar("lci(1)"), 3.5790745705,       1e-9);
    EXPECT_NEAR(evalScalar("lci(2)"), 5.8215944063,       1e-9);
}

TEST_F(PoissfitTest, AllZeroData)
{
    // S == 0 branch: lo = 0; hi from chi2inv(1-α/2, 2(S+1)).
    eval("[lh, lci] = poissfit([0 0 0 0]');");
    EXPECT_DOUBLE_EQ(evalScalar("lh"),     0.0);
    EXPECT_DOUBLE_EQ(evalScalar("lci(1)"), 0.0);
    EXPECT_NEAR(evalScalar("lci(2)"), 0.9222198635, 1e-9);
}

TEST_F(PoissfitTest, NonDefaultAlpha)
{
    eval("x = [3 4 5 4 5 6 7 5 4 3 5 6 4 3 5]';");
    eval("[lh, lci] = poissfit(x, 0.01);");
    EXPECT_NEAR(evalScalar("lh"),     4.6,           1e-12);
    EXPECT_NEAR(evalScalar("lci(1)"), 3.2987924205,  1e-9);
    EXPECT_NEAR(evalScalar("lci(2)"), 6.2282280721,  1e-9);
}

TEST_F(PoissfitTest, EmptyInput)
{
    eval("[lh, lci] = poissfit([]);");
    EXPECT_TRUE(std::isnan(evalScalar("lh")));
    EXPECT_TRUE(std::isnan(evalScalar("lci(1)")));
    EXPECT_TRUE(std::isnan(evalScalar("lci(2)")));
}
