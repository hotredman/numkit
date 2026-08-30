// toolboxes/stats/tests/nctpdf_nctcdf_test.cpp
//
// Regression guard for nctpdf + nctcdf — noncentral Student's t.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

#include <cmath>

using namespace numkit;

class NctPdfCdfTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override {}
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// ── nctpdf ──────────────────────────────────────────────────────────

TEST_F(NctPdfCdfTest, NctpdfMatchesMatlab)
{
    EXPECT_NEAR(evalScalar("nctpdf(1.5, 10, 2)"), 0.3349931548, 1e-8);
}

TEST_F(NctPdfCdfTest, NctpdfVectorMatchesMatlab)
{
    eval("y = nctpdf([0 1 2 3], 10, 2);");
    EXPECT_NEAR(evalScalar("y(1)"), 0.0526601, 1e-5);
    EXPECT_NEAR(evalScalar("y(2)"), 0.2413720, 1e-5);
    EXPECT_NEAR(evalScalar("y(3)"), 0.3556442, 1e-5);
    EXPECT_NEAR(evalScalar("y(4)"), 0.2197309, 1e-5);
}

TEST_F(NctPdfCdfTest, NctpdfNcpZeroEqualsCentral)
{
    // ncp = 0 should match tpdf exactly.
    EXPECT_NEAR(evalScalar("nctpdf(0.5, 7, 0)"),
                evalScalar("tpdf(0.5, 7)"), 1e-12);
}

TEST_F(NctPdfCdfTest, NctpdfShapePreserved)
{
    eval("y = nctpdf([1 2; 3 4], 8, 1.5);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(y, 1)")), 2);
    EXPECT_EQ(static_cast<int>(evalScalar("size(y, 2)")), 2);
}

// ── nctcdf ──────────────────────────────────────────────────────────

TEST_F(NctPdfCdfTest, NctcdfMatchesMatlab)
{
    EXPECT_NEAR(evalScalar("nctcdf(1.5, 10, 2)"), 0.3047854474, 1e-6);
}

TEST_F(NctPdfCdfTest, NctcdfVectorMatchesMatlab)
{
    eval("p = nctcdf([0 1 2 3], 10, 2);");
    EXPECT_NEAR(evalScalar("p(1)"), 0.0227501, 1e-6);
    EXPECT_NEAR(evalScalar("p(2)"), 0.1585146, 1e-5);
    EXPECT_NEAR(evalScalar("p(3)"), 0.4809732, 1e-5);
    EXPECT_NEAR(evalScalar("p(4)"), 0.7791719, 1e-5);
}

TEST_F(NctPdfCdfTest, NctcdfUpperTail)
{
    EXPECT_NEAR(evalScalar("nctcdf(1.5, 10, 2, 'upper')"),
                1.0 - 0.3047854474, 1e-6);
}

TEST_F(NctPdfCdfTest, NctcdfNcpZeroEqualsCentral)
{
    EXPECT_NEAR(evalScalar("nctcdf(0.5, 7, 0)"),
                evalScalar("tcdf(0.5, 7)"), 1e-12);
}

TEST_F(NctPdfCdfTest, NctcdfNegativeArgument)
{
    // Symmetry: F(x; ν, δ) = 1 - F(-x; ν, -δ).
    // Check internal consistency.
    eval("a = nctcdf(-1.5, 10, 2); b = 1 - nctcdf(1.5, 10, -2);");
    EXPECT_NEAR(evalScalar("a"), evalScalar("b"), 1e-9);
}

TEST_F(NctPdfCdfTest, NctcdfMonotone)
{
    // F should be monotonically increasing in x.
    eval("p = nctcdf([-3 -1 0 1 3], 10, 1);");
    EXPECT_LT(evalScalar("p(1)"), evalScalar("p(2)"));
    EXPECT_LT(evalScalar("p(2)"), evalScalar("p(3)"));
    EXPECT_LT(evalScalar("p(3)"), evalScalar("p(4)"));
    EXPECT_LT(evalScalar("p(4)"), evalScalar("p(5)"));
}
