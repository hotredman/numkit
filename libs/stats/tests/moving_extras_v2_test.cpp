// libs/stats/tests/moving_extras_v2_test.cpp
//
// Regression tests for the mov* family's nanflag + Endpoints support
// (closes audit/findings/stats/{movmean,movmedian,movsum,movmin,movmax,
// movprod,movmad,movstd,movvar}.md). Hardcoded expected values
// captured from MATLAB R2025b probe runs.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class MovExtrasTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override
    {
        engine.eval("import compat.*;");
        // Common test inputs.
        engine.eval("A  = [1 3 2 5 4 6 NaN 8 7 10]';");
        engine.eval("A2 = (1:9)';");
    }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// ── default (includenan) — NaN poisons the window ─────────────────────

TEST_F(MovExtrasTest, MovmeanDefaultPoisonsNaN)
{
    eval("y = movmean(A, 3);");
    EXPECT_NEAR(evalScalar("y(5)"), 5.0, 1e-12);
    EXPECT_TRUE(std::isnan(evalScalar("y(6)")));
    EXPECT_TRUE(std::isnan(evalScalar("y(7)")));
    EXPECT_TRUE(std::isnan(evalScalar("y(8)")));
    EXPECT_NEAR(evalScalar("y(9)"), 25.0/3.0, 1e-12);
}

// ── omitnan / omitmissing aliases ─────────────────────────────────────

TEST_F(MovExtrasTest, MovmeanOmitnanDropsNaN)
{
    eval("y = movmean(A, 3, 'omitnan');");
    EXPECT_NEAR(evalScalar("y(6)"), 5.0, 1e-12);   // (4+6)/2
    EXPECT_NEAR(evalScalar("y(7)"), 7.0, 1e-12);   // (6+8)/2
    EXPECT_NEAR(evalScalar("y(8)"), 7.5, 1e-12);   // (8+7)/2
}

TEST_F(MovExtrasTest, MovmeanOmitmissingAlias)
{
    eval("y1 = movmean(A, 3, 'omitnan'); y2 = movmean(A, 3, 'omitmissing');");
    EXPECT_NEAR(evalScalar("max(abs(y1 - y2))"), 0.0, 1e-15);
}

TEST_F(MovExtrasTest, MovmeanIncludenanExplicitMatchesDefault)
{
    eval("y_def = movmean(A, 3); y_inc = movmean(A, 3, 'includenan');");
    eval("idx_def = isnan(y_def); idx_inc = isnan(y_inc);");
    EXPECT_TRUE(eval("isequal(idx_def, idx_inc)").toBool());
}

// ── Endpoints discard / fill / scalar ─────────────────────────────────

TEST_F(MovExtrasTest, EndpointsDiscardShortensOutput)
{
    eval("y = movmean(A2, 3, 'Endpoints', 'discard');");
    EXPECT_EQ(static_cast<size_t>(evalScalar("numel(y)")), 7u);
    EXPECT_NEAR(evalScalar("y(1)"), 2.0, 1e-12);
    EXPECT_NEAR(evalScalar("y(7)"), 8.0, 1e-12);
}

TEST_F(MovExtrasTest, EndpointsFillPadsWithNaN)
{
    eval("y = movmean(A2, 3, 'Endpoints', 'fill');");
    EXPECT_TRUE(std::isnan(evalScalar("y(1)")));
    EXPECT_NEAR(evalScalar("y(2)"), 2.0, 1e-12);
    EXPECT_TRUE(std::isnan(evalScalar("y(9)")));
}

TEST_F(MovExtrasTest, EndpointsScalarPadsWithValue)
{
    eval("y = movmean(A2, 3, 'Endpoints', 0);");
    // window at i=1: [0, 1, 2] → mean = 1
    EXPECT_NEAR(evalScalar("y(1)"), 1.0, 1e-12);
    EXPECT_NEAR(evalScalar("y(2)"), 2.0, 1e-12);
    // window at i=9: [8, 9, 0] → mean = 17/3
    EXPECT_NEAR(evalScalar("y(9)"), 17.0/3.0, 1e-12);
}

// ── combined matrix + dim + nanflag + endpoints ───────────────────────

TEST_F(MovExtrasTest, MatrixDimOmitnanDiscard)
{
    eval("M = [A2 A2*2]; y = movmean(M, 3, 1, 'omitnan', 'Endpoints', 'discard');");
    EXPECT_EQ(static_cast<size_t>(evalScalar("size(y, 1)")), 7u);
    EXPECT_EQ(static_cast<size_t>(evalScalar("size(y, 2)")), 2u);
    EXPECT_NEAR(evalScalar("y(1, 1)"), 2.0, 1e-12);
    EXPECT_NEAR(evalScalar("y(7, 2)"), 16.0, 1e-12);
}

// ── error paths ───────────────────────────────────────────────────────

TEST_F(MovExtrasTest, KZeroErrors)
{
    EXPECT_THROW(eval("movmean(A2, 0);"), numkit::Error);
}

TEST_F(MovExtrasTest, SamplePointsRejected)
{
    EXPECT_THROW(eval("movmean(A2, 2, 'SamplePoints', [0 1 2 3 4 5 7 8 9]');"),
                 numkit::Error);
}

// ── per-function omitnan smoke ─────────────────────────────────────────

TEST_F(MovExtrasTest, MovmedianOmitnan)
{
    eval("y = movmedian(A, 3, 'omitnan');");
    EXPECT_NEAR(evalScalar("y(7)"), 7.0, 1e-12);   // median([6, 8])
    EXPECT_NEAR(evalScalar("y(8)"), 7.5, 1e-12);   // median([7, 8])
}

TEST_F(MovExtrasTest, MovsumOmitnan)
{
    eval("y = movsum(A, 3, 'omitnan');");
    // sum([4, 6]) at pos 6, sum([6, 8]) at pos 7, sum([8, 7]) at pos 8
    EXPECT_NEAR(evalScalar("y(6)"), 10.0, 1e-12);
    EXPECT_NEAR(evalScalar("y(7)"), 14.0, 1e-12);
    EXPECT_NEAR(evalScalar("y(8)"), 15.0, 1e-12);
}

TEST_F(MovExtrasTest, MovstdOmitnan)
{
    eval("y = movstd(A, 3, 'omitnan');");
    // std([4, 6])=sqrt(2), std([6, 8])=sqrt(2), std([7, 8])=sqrt(0.5)
    EXPECT_NEAR(evalScalar("y(6)"), std::sqrt(2.0), 1e-12);
    EXPECT_NEAR(evalScalar("y(7)"), std::sqrt(2.0), 1e-12);
    EXPECT_NEAR(evalScalar("y(8)"), std::sqrt(0.5), 1e-12);
}

TEST_F(MovExtrasTest, MovvarNormFlag1Omitnan)
{
    eval("y = movvar(A, 3, 1, 'omitnan');");
    // var([4, 6], 1) = 1, var([6, 8], 1) = 1, var([7, 8], 1) = 0.25
    EXPECT_NEAR(evalScalar("y(6)"), 1.0, 1e-12);
    EXPECT_NEAR(evalScalar("y(7)"), 1.0, 1e-12);
    EXPECT_NEAR(evalScalar("y(8)"), 0.25, 1e-12);
}

TEST_F(MovExtrasTest, MovmaxOmitnan)
{
    eval("y = movmax(A, 3, 'omitnan');");
    EXPECT_NEAR(evalScalar("y(7)"), 8.0, 1e-12);     // max([6, 8])
    EXPECT_NEAR(evalScalar("y(8)"), 8.0, 1e-12);     // max([8, 7])
}

TEST_F(MovExtrasTest, MovminOmitnan)
{
    eval("y = movmin(A, 3, 'omitnan');");
    EXPECT_NEAR(evalScalar("y(7)"), 6.0, 1e-12);     // min([6, 8])
    EXPECT_NEAR(evalScalar("y(8)"), 7.0, 1e-12);     // min([8, 7])
}

TEST_F(MovExtrasTest, MovmadOmitnan)
{
    eval("y = movmad(A, 3, 'omitnan');");
    // mad([4, 6]) = 1, mad([6, 8]) = 1, mad([7, 8]) = 0.5
    EXPECT_NEAR(evalScalar("y(6)"), 1.0, 1e-12);
    EXPECT_NEAR(evalScalar("y(7)"), 1.0, 1e-12);
    EXPECT_NEAR(evalScalar("y(8)"), 0.5, 1e-12);
}

TEST_F(MovExtrasTest, MovprodOmitnan)
{
    eval("y = movprod(A, 3, 'omitnan');");
    // prod([4, 6]) = 24, prod([6, 8]) = 48, prod([8, 7]) = 56
    EXPECT_NEAR(evalScalar("y(6)"), 24.0, 1e-12);
    EXPECT_NEAR(evalScalar("y(7)"), 48.0, 1e-12);
    EXPECT_NEAR(evalScalar("y(8)"), 56.0, 1e-12);
}

// ── all-NaN window collapses to NaN even with omitnan ─────────────────

TEST_F(MovExtrasTest, AllNanWindowReturnsNaN)
{
    eval("A3 = [NaN NaN NaN 4 5 6]'; y = movmean(A3, 3, 'omitnan');");
    EXPECT_TRUE(std::isnan(evalScalar("y(1)")));
    EXPECT_TRUE(std::isnan(evalScalar("y(2)")));
    EXPECT_NEAR(evalScalar("y(3)"), 4.0, 1e-12);    // omit gives [4]
}
