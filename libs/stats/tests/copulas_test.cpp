// libs/stats/tests/copulas_test.cpp
//
// Regression guard for copulapdf + copulacdf across all 5 families.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>
#include <cmath>

using namespace numkit;

class CopulasTest : public ::testing::Test {
public:
    Engine engine;
    void SetUp() override {
        engine.eval("import compat.*;");
        engine.eval("U = [0.3 0.4; 0.5 0.6; 0.7 0.2; 0.1 0.9];");
        engine.eval("R = [1 0.5; 0.5 1];");
    }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// ── Gaussian ────────────────────────────────────────────────────────

TEST_F(CopulasTest, GaussianPdfMatchesMatlab)
{
    eval("y = copulapdf('Gaussian', U, R);");
    EXPECT_NEAR(evalScalar("y(1)"), 1.1923, 1e-3);
    EXPECT_NEAR(evalScalar("y(2)"), 1.1424, 1e-3);
    EXPECT_NEAR(evalScalar("y(3)"), 0.7303, 1e-3);
    EXPECT_NEAR(evalScalar("y(4)"), 0.2235, 1e-3);
}

TEST_F(CopulasTest, GaussianCdfMatchesMatlab)
{
    eval("p = copulacdf('Gaussian', U, R);");
    EXPECT_NEAR(evalScalar("p(1)"), 0.1919, 1e-3);
    EXPECT_NEAR(evalScalar("p(2)"), 0.3804, 1e-3);
    EXPECT_NEAR(evalScalar("p(3)"), 0.1829, 1e-3);
    EXPECT_NEAR(evalScalar("p(4)"), 0.0993, 1e-3);
}

// ── Student-t ───────────────────────────────────────────────────────

TEST_F(CopulasTest, TPdfMatchesMatlab)
{
    eval("y = copulapdf('t', U, R, 5);");
    EXPECT_NEAR(evalScalar("y(1)"), 1.2908, 1e-3);
    EXPECT_NEAR(evalScalar("y(2)"), 1.2457, 1e-3);
    EXPECT_NEAR(evalScalar("y(3)"), 0.6723, 1e-3);
    EXPECT_NEAR(evalScalar("y(4)"), 0.3268, 1e-3);
}

TEST_F(CopulasTest, TCdfMatchesMatlab)
{
    // t-copula CDF uses Monte Carlo internally (MC tolerance ~0.005).
    eval("p = copulacdf('t', U, R, 5);");
    EXPECT_NEAR(evalScalar("p(1)"), 0.1927, 0.01);
    EXPECT_NEAR(evalScalar("p(2)"), 0.3801, 0.01);
    EXPECT_NEAR(evalScalar("p(3)"), 0.1780, 0.01);
    EXPECT_NEAR(evalScalar("p(4)"), 0.0969, 0.01);
}

// ── Clayton ─────────────────────────────────────────────────────────

TEST_F(CopulasTest, ClaytonPdfMatchesMatlab)
{
    eval("y = copulapdf('Clayton', U, 2);");
    EXPECT_NEAR(evalScalar("y(1)"), 1.6034, 1e-3);
    EXPECT_NEAR(evalScalar("y(2)"), 1.3847, 1e-3);
    EXPECT_NEAR(evalScalar("y(3)"), 0.3159, 1e-3);
    EXPECT_NEAR(evalScalar("y(4)"), 0.0409, 1e-3);
}

TEST_F(CopulasTest, ClaytonCdfMatchesMatlab)
{
    eval("p = copulacdf('Clayton', U, 2);");
    EXPECT_NEAR(evalScalar("p(1)"), 0.2472, 1e-3);
    EXPECT_NEAR(evalScalar("p(2)"), 0.4160, 1e-3);
    EXPECT_NEAR(evalScalar("p(3)"), 0.1960, 1e-3);
    EXPECT_NEAR(evalScalar("p(4)"), 0.0999, 1e-3);
}

// ── Frank ───────────────────────────────────────────────────────────

TEST_F(CopulasTest, FrankPdfMatchesMatlab)
{
    eval("y = copulapdf('Frank', U, 3);");
    EXPECT_NEAR(evalScalar("y(1)"), 1.2172, 1e-3);
    EXPECT_NEAR(evalScalar("y(2)"), 1.1547, 1e-3);
    EXPECT_NEAR(evalScalar("y(3)"), 0.6236, 1e-3);
    EXPECT_NEAR(evalScalar("y(4)"), 0.2828, 1e-3);
}

TEST_F(CopulasTest, FrankCdfMatchesMatlab)
{
    eval("p = copulacdf('Frank', U, 3);");
    EXPECT_NEAR(evalScalar("p(1)"), 0.1911, 1e-3);
    EXPECT_NEAR(evalScalar("p(2)"), 0.3824, 1e-3);
    EXPECT_NEAR(evalScalar("p(3)"), 0.1797, 1e-3);
    EXPECT_NEAR(evalScalar("p(4)"), 0.0979, 1e-3);
}

// ── Gumbel ──────────────────────────────────────────────────────────

TEST_F(CopulasTest, GumbelPdfMatchesMatlab)
{
    eval("y = copulapdf('Gumbel', U, 1.5);");
    EXPECT_NEAR(evalScalar("y(1)"), 1.2371, 1e-3);
    EXPECT_NEAR(evalScalar("y(2)"), 1.2000, 1e-3);
    EXPECT_NEAR(evalScalar("y(3)"), 0.7278, 1e-3);
    EXPECT_NEAR(evalScalar("y(4)"), 0.2828, 1e-3);
}

TEST_F(CopulasTest, GumbelCdfMatchesMatlab)
{
    eval("p = copulacdf('Gumbel', U, 1.5);");
    EXPECT_NEAR(evalScalar("p(1)"), 0.1844, 1e-3);
    EXPECT_NEAR(evalScalar("p(2)"), 0.3825, 1e-3);
    EXPECT_NEAR(evalScalar("p(3)"), 0.1792, 1e-3);
    EXPECT_NEAR(evalScalar("p(4)"), 0.0985, 1e-3);
}

// ── Edge cases / errors ─────────────────────────────────────────────

TEST_F(CopulasTest, UnknownFamilyThrows)
{
    EXPECT_THROW(eval("copulapdf('badfam', U, R);"), std::exception);
}

TEST_F(CopulasTest, OutOfRangeUThrows)
{
    EXPECT_THROW(eval("copulapdf('Gaussian', [1.5 0.4], R);"), std::exception);
}

TEST_F(CopulasTest, ClaytonNegativeAlphaThrows)
{
    EXPECT_THROW(eval("copulapdf('Clayton', U, -1);"), std::exception);
}

TEST_F(CopulasTest, GumbelAlphaBelowOneThrows)
{
    EXPECT_THROW(eval("copulapdf('Gumbel', U, 0.5);"), std::exception);
}

TEST_F(CopulasTest, IndependenceLimits)
{
    // Gaussian with ρ=0 → c=1, C(u,v)=u·v (independence).
    eval("R0 = eye(2); y = copulapdf('Gaussian', [0.5 0.5], R0); "
         "p = copulacdf('Gaussian', [0.5 0.5], R0);");
    EXPECT_NEAR(evalScalar("y"), 1.0, 1e-6);
    EXPECT_NEAR(evalScalar("p"), 0.25, 1e-3);
}

// ── 3-D Gaussian and t (closes the d≥3 gap) ─────────────────────────

TEST_F(CopulasTest, Gaussian3DPdfMatchesMatlab)
{
    eval(R"(
        U3 = [0.3 0.4 0.5; 0.6 0.7 0.2];
        R3 = [1 0.5 0.3; 0.5 1 0.4; 0.3 0.4 1];
        y = copulapdf('Gaussian', U3, R3);
    )");
    EXPECT_NEAR(evalScalar("y(1)"), 1.2926, 1e-3);
    EXPECT_NEAR(evalScalar("y(2)"), 0.9590, 1e-3);
}

TEST_F(CopulasTest, Gaussian3DCdfMatchesMatlab)
{
    // d≥3 CDF uses mvncdf (MC); ~5e-3 tolerance.
    eval(R"(
        U3 = [0.3 0.4 0.5; 0.6 0.7 0.2];
        R3 = [1 0.5 0.3; 0.5 1 0.4; 0.3 0.4 1];
        p = copulacdf('Gaussian', U3, R3);
    )");
    EXPECT_NEAR(evalScalar("p(1)"), 0.1372, 5e-3);
    EXPECT_NEAR(evalScalar("p(2)"), 0.1396, 5e-3);
}

TEST_F(CopulasTest, T3DPdfMatchesMatlab)
{
    eval(R"(
        U3 = [0.3 0.4 0.5; 0.6 0.7 0.2];
        R3 = [1 0.5 0.3; 0.5 1 0.4; 0.3 0.4 1];
        y = copulapdf('t', U3, R3, 5);
    )");
    EXPECT_NEAR(evalScalar("y(1)"), 1.6129, 1e-3);
    EXPECT_NEAR(evalScalar("y(2)"), 0.9340, 1e-3);
}

TEST_F(CopulasTest, T3DCdfMatchesMatlab)
{
    eval(R"(
        U3 = [0.3 0.4 0.5; 0.6 0.7 0.2];
        R3 = [1 0.5 0.3; 0.5 1 0.4; 0.3 0.4 1];
        p = copulacdf('t', U3, R3, 5);
    )");
    EXPECT_NEAR(evalScalar("p(1)"), 0.1373, 5e-3);
    EXPECT_NEAR(evalScalar("p(2)"), 0.1344, 5e-3);
}

TEST_F(CopulasTest, NonPDRThrows)
{
    EXPECT_THROW(eval("copulapdf('Gaussian', [0.5 0.5], [1 1.5; 1.5 1]);"),
                 std::exception);
}
