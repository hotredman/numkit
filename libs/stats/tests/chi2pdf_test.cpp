// libs/stats/tests/chi2pdf_test.cpp
// chi2pdf. Reference values from MATLAB R2025b.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class Chi2pdfTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(Chi2pdfTest, ScalarMidRange)
{
    EXPECT_NEAR(evalScalar("chi2pdf(2, 3)"), 0.2075537487, 1e-9);
}

TEST_F(Chi2pdfTest, K2AtZeroIsHalf)
{
    // Chi2(2) is exponential with rate 1/2 → density at 0 = 0.5.
    EXPECT_NEAR(evalScalar("chi2pdf(0, 2)"), 0.5, 1e-12);
}

TEST_F(Chi2pdfTest, VectorInputs)
{
    eval("y = chi2pdf([0.5 1 2 5]', 3);");
    EXPECT_NEAR(evalScalar("y(1)"), 0.2196956447338612, 1e-12);
    EXPECT_NEAR(evalScalar("y(2)"), 0.2419707245191434, 1e-12);
    EXPECT_NEAR(evalScalar("y(3)"), 0.2075537487102974, 1e-12);
    EXPECT_NEAR(evalScalar("y(4)"), 0.0732249128096324, 1e-12);
}

TEST_F(Chi2pdfTest, NegativeXReturnsZero)
{
    EXPECT_DOUBLE_EQ(evalScalar("chi2pdf(-1.0, 3)"),  0.0);
    EXPECT_DOUBLE_EQ(evalScalar("chi2pdf(-0.1, 3)"),  0.0);
    EXPECT_DOUBLE_EQ(evalScalar("chi2pdf( 0.0, 3)"),  0.0);  // Chi2(3): density 0 at 0
}

TEST_F(Chi2pdfTest, KEqualsZeroReturnsZero)
{
    // MATLAB convention: degenerate Chi²(0) has density 0 almost everywhere.
    EXPECT_DOUBLE_EQ(evalScalar("chi2pdf(2, 0)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("chi2pdf(5, 0)"), 0.0);
}

TEST_F(Chi2pdfTest, NegativeKReturnsNaN)
{
    EXPECT_TRUE(std::isnan(evalScalar("chi2pdf(2, -1)")));
}

TEST_F(Chi2pdfTest, LargeDof)
{
    // k=30, x=30 — peak region for Chi²(30) is near k-2 = 28.
    EXPECT_NEAR(evalScalar("chi2pdf(30, 30)"), 0.0512179333322674, 1e-12);
}
