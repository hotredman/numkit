// toolboxes/stats/tests/gevfit_test.cpp
//
// Regression guard for gevfit — 3-parameter GEV MLE.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>
#include <cmath>

using namespace numkit;

class GevfitTest : public ::testing::Test {
public:
    StandardEngine engine;
    void SetUp() override {}
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(GevfitTest, FrechetCaseMatchesMatlab)
{
    // GEV(k=0.2, sigma=1.0, mu=0.5), n=2000. MATLAB:
    //   parm = (0.2000693363, 1.0000000000, 0.5002300535)
    eval(R"(
        n=2000; u=((1:n)' - 0.5)/n; x = gevinv(u, 0.2, 1.0, 0.5);
        [p, pci] = gevfit(x);
    )");
    EXPECT_NEAR(evalScalar("p(1)"), 0.2000693363, 1e-3);
    EXPECT_NEAR(evalScalar("p(2)"), 1.0000000000, 1e-3);
    EXPECT_NEAR(evalScalar("p(3)"), 0.5002300535, 1e-3);
    EXPECT_NEAR(evalScalar("pci(1, 1)"), 0.1647, 5e-3);
    EXPECT_NEAR(evalScalar("pci(2, 1)"), 0.2354, 5e-3);
    EXPECT_NEAR(evalScalar("pci(1, 2)"), 0.9614, 5e-3);
    EXPECT_NEAR(evalScalar("pci(2, 2)"), 1.0402, 5e-3);
    EXPECT_NEAR(evalScalar("pci(1, 3)"), 0.4507, 5e-3);
    EXPECT_NEAR(evalScalar("pci(2, 3)"), 0.5498, 5e-3);
}

TEST_F(GevfitTest, ReverseWeibullCaseMatchesMatlab)
{
    // GEV(k=-0.2, sigma=2.0, mu=1.0), bounded above. MATLAB:
    //   parm = (-0.2008751464, 1.9998464423, 1.0010633338)
    eval(R"(
        n=2000; u=((1:n)' - 0.5)/n; x = gevinv(u, -0.2, 2.0, 1.0);
        p = gevfit(x);
    )");
    EXPECT_NEAR(evalScalar("p(1)"), -0.2008751464, 1e-3);
    EXPECT_NEAR(evalScalar("p(2)"),  1.9998464423, 1e-3);
    EXPECT_NEAR(evalScalar("p(3)"),  1.0010633338, 1e-3);
}

TEST_F(GevfitTest, GumbelLimitMatchesMatlab)
{
    // k = 0 (Gumbel for maxima). MATLAB returns k≈0.
    eval(R"(
        n=2000; u=((1:n)' - 0.5)/n; x = gevinv(u, 0, 1.5, 0.0);
        p = gevfit(x);
    )");
    EXPECT_NEAR(evalScalar("p(1)"), 0.0, 0.005);
    EXPECT_NEAR(evalScalar("p(2)"), 1.5, 0.01);
    EXPECT_NEAR(evalScalar("p(3)"), 0.0, 0.01);
}

TEST_F(GevfitTest, OutputShape)
{
    eval("p = gevfit(gevinv(((1:100)' - 0.5)/100, 0.1, 1.0, 0));");
    EXPECT_EQ(static_cast<int>(evalScalar("size(p, 1)")), 1);
    EXPECT_EQ(static_cast<int>(evalScalar("size(p, 2)")), 3);
}

TEST_F(GevfitTest, AlphaArgument)
{
    eval(R"(
        x = gevinv(((1:500)' - 0.5)/500, 0.1, 1.0, 0);
        [~, c95] = gevfit(x);
        [~, c99] = gevfit(x, 0.01);
    )");
    // Wider CI at α=0.01.
    EXPECT_GT(evalScalar("c99(2, 1) - c99(1, 1)"),
              evalScalar("c95(2, 1) - c95(1, 1)"));
}

TEST_F(GevfitTest, TooFewObsThrows)
{
    EXPECT_THROW(eval("gevfit([1.0, 2.0]);"), std::exception);
}
