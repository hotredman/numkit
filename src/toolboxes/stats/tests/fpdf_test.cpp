// toolboxes/stats/tests/fpdf_test.cpp
// fpdf. Reference values from MATLAB R2025b.

#include <numkit/core/engine.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class FpdfTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(FpdfTest, ScalarMidRange)
{
    EXPECT_NEAR(evalScalar("fpdf(2, 5, 10)"), 0.1620057421801153, 1e-12);
}

TEST_F(FpdfTest, VectorInputs)
{
    eval("y = fpdf([0.5 1 2 5]', 5, 10);");
    EXPECT_NEAR(evalScalar("y(1)"), 0.6876070027706258, 1e-12);
    EXPECT_NEAR(evalScalar("y(2)"), 0.4954797834866400, 1e-12);
    EXPECT_NEAR(evalScalar("y(3)"), 0.1620057421801153, 1e-12);
    EXPECT_NEAR(evalScalar("y(4)"), 0.0096306379378101, 1e-12);
}

TEST_F(FpdfTest, NegativeXReturnsZero)
{
    EXPECT_DOUBLE_EQ(evalScalar("fpdf(-1.0, 5, 10)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("fpdf(-0.1, 5, 10)"), 0.0);
}

TEST_F(FpdfTest, DensityAtZero_V1eq2_IsFinite)
{
    // F(2, ν2) at x=0 → finite density (= ν2 / (ν2 + 0) limit term).
    // For F(2, 10): exact density at 0 is 1.0.
    EXPECT_NEAR(evalScalar("fpdf(0, 2, 10)"), 1.0, 1e-12);
}

TEST_F(FpdfTest, DensityAtZero_V1lt2_IsInf)
{
    // F(1, ν2) at x=0 → +∞ (heavy at origin).
    EXPECT_TRUE(std::isinf(evalScalar("fpdf(0, 1, 10)")));
}

TEST_F(FpdfTest, DensityAtZero_V1gt2_IsZero)
{
    // F(v1>2, ν2) at x=0 → 0.
    EXPECT_DOUBLE_EQ(evalScalar("fpdf(0, 5, 10)"), 0.0);
}

TEST_F(FpdfTest, InvalidDofReturnsNaN)
{
    EXPECT_TRUE(std::isnan(evalScalar("fpdf(2,  0, 10)")));
    EXPECT_TRUE(std::isnan(evalScalar("fpdf(2,  5,  0)")));
    EXPECT_TRUE(std::isnan(evalScalar("fpdf(2, -1, 10)")));
}
