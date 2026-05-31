// libs/stats/tests/ncfcdf_ncfinv_test.cpp
//
// Regression guard for ncfcdf + ncfinv.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

#include <cmath>

using namespace numkit;

class NcfCdfInvTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// ── ncfcdf ──────────────────────────────────────────────────────────

TEST_F(NcfCdfInvTest, NcfcdfMatchesMatlab)
{
    EXPECT_NEAR(evalScalar("ncfcdf(1.5, 5, 10, 3)"), 0.491323141971, 1e-9);
}

TEST_F(NcfCdfInvTest, NcfcdfVectorMatchesMatlab)
{
    eval("p = ncfcdf([0.5 1.0 2.0 3.0], 5, 10, 3);");
    EXPECT_NEAR(evalScalar("p(1)"), 0.0922673, 1e-5);
    EXPECT_NEAR(evalScalar("p(2)"), 0.2973240, 1e-5);
    EXPECT_NEAR(evalScalar("p(3)"), 0.6391475, 1e-5);
    EXPECT_NEAR(evalScalar("p(4)"), 0.8167019, 1e-5);
}

TEST_F(NcfCdfInvTest, NcfcdfDeltaZeroEqualsCentral)
{
    EXPECT_NEAR(evalScalar("ncfcdf(1.5, 5, 10, 0)"),
                evalScalar("fcdf(1.5, 5, 10)"), 1e-12);
}

TEST_F(NcfCdfInvTest, NcfcdfUpper)
{
    EXPECT_NEAR(evalScalar("ncfcdf(1.5, 5, 10, 3, 'upper')"),
                1.0 - 0.491323141971, 1e-9);
}

TEST_F(NcfCdfInvTest, NcfcdfZeroXIsZero)
{
    EXPECT_EQ(evalScalar("ncfcdf(0, 5, 10, 3)"), 0.0);
}

// ── ncfinv ──────────────────────────────────────────────────────────

TEST_F(NcfCdfInvTest, NcfinvMatchesMatlab)
{
    EXPECT_NEAR(evalScalar("ncfinv(0.3, 5, 10, 3)"), 1.0063340603, 1e-6);
}

TEST_F(NcfCdfInvTest, NcfinvVectorMatchesMatlab)
{
    eval("x = ncfinv([0.1 0.5 0.9], 5, 10, 3);");
    EXPECT_NEAR(evalScalar("x(1)"), 0.5216, 1e-3);
    EXPECT_NEAR(evalScalar("x(2)"), 1.5254, 1e-3);
    EXPECT_NEAR(evalScalar("x(3)"), 3.9619, 1e-3);
}

TEST_F(NcfCdfInvTest, NcfinvRoundTrip)
{
    eval("p = 0.42; x = ncfinv(p, 6, 12, 2.5); rt = ncfcdf(x, 6, 12, 2.5);");
    EXPECT_NEAR(evalScalar("rt"), 0.42, 1e-8);
}

TEST_F(NcfCdfInvTest, NcfinvBoundaryP0)
{
    EXPECT_EQ(evalScalar("ncfinv(0, 5, 10, 3)"), 0.0);
}

TEST_F(NcfCdfInvTest, NcfinvBoundaryP1)
{
    EXPECT_TRUE(std::isinf(evalScalar("ncfinv(1, 5, 10, 3)")));
}

TEST_F(NcfCdfInvTest, NcfinvDeltaZeroEqualsCentral)
{
    EXPECT_NEAR(evalScalar("ncfinv(0.7, 5, 10, 0)"),
                evalScalar("finv(0.7, 5, 10)"), 1e-10);
}
