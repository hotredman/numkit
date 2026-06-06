// libs/stats/tests/nctrnd_ncfpdf_test.cpp
//
// Regression guard for nctrnd + ncfpdf.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

#include <cmath>

using namespace numkit;

class NctRndNcfPdfTest : public ::testing::Test
{
public:
    StdEngine engine;
    void SetUp() override { engine.eval("import compat.*; rng(0);"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// ── nctrnd ──────────────────────────────────────────────────────────

TEST_F(NctRndNcfPdfTest, NctrndShape)
{
    eval("R = nctrnd(10, 2, 7, 3);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(R, 1)")), 7);
    EXPECT_EQ(static_cast<int>(evalScalar("size(R, 2)")), 3);
}

TEST_F(NctRndNcfPdfTest, NctrndMeanMatches)
{
    eval("S = nctrnd(10, 2, 5000, 1); m = mean(S);");
    EXPECT_NEAR(evalScalar("m"), 2.1674, 0.15);
}

TEST_F(NctRndNcfPdfTest, NctrndVarMatches)
{
    eval("S = nctrnd(10, 2, 5000, 1); v = var(S);");
    EXPECT_NEAR(evalScalar("v"), 1.5522, 0.3);
}

TEST_F(NctRndNcfPdfTest, NctrndDeltaZeroIsCentralT)
{
    // δ = 0 should give central-t distribution; mean ≈ 0.
    eval("S = nctrnd(10, 0, 5000, 1); m = mean(S);");
    EXPECT_LT(std::abs(evalScalar("m")), 0.2);
}

// ── ncfpdf ──────────────────────────────────────────────────────────

TEST_F(NctRndNcfPdfTest, NcfpdfMatchesMatlab)
{
    EXPECT_NEAR(evalScalar("ncfpdf(1.5, 5, 10, 3)"), 0.343970191513, 1e-10);
}

TEST_F(NctRndNcfPdfTest, NcfpdfVectorMatchesMatlab)
{
    eval("y = ncfpdf([0.5 1.0 2.0 3.0], 5, 10, 3);");
    EXPECT_NEAR(evalScalar("y(1)"), 0.3541260, 1e-5);
    EXPECT_NEAR(evalScalar("y(2)"), 0.4226859, 1e-5);
    EXPECT_NEAR(evalScalar("y(3)"), 0.2492816, 1e-5);
    EXPECT_NEAR(evalScalar("y(4)"), 0.1202986, 1e-5);
}

TEST_F(NctRndNcfPdfTest, NcfpdfDeltaZeroEqualsCentral)
{
    EXPECT_NEAR(evalScalar("ncfpdf(1.5, 5, 10, 0)"),
                evalScalar("fpdf(1.5, 5, 10)"), 1e-12);
}

TEST_F(NctRndNcfPdfTest, NcfpdfNegativeXIsZero)
{
    EXPECT_EQ(evalScalar("ncfpdf(-1.0, 5, 10, 3)"), 0.0);
    EXPECT_EQ(evalScalar("ncfpdf(0.0, 5, 10, 3)"), 0.0);
}

TEST_F(NctRndNcfPdfTest, NcfpdfShapePreserved)
{
    eval("y = ncfpdf([1 2; 3 4], 6, 8, 2);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(y, 1)")), 2);
    EXPECT_EQ(static_cast<int>(evalScalar("size(y, 2)")), 2);
}
