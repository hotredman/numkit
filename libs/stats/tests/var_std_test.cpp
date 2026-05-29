// libs/stats/tests/var_std_test.cpp
//
// Closes audit/closed/stats/{var,std}.md — 'all' / vecdim / weight-vec
// support. Reference values from MATLAB R2025b probe.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class VarStdTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override
    {
        engine.eval("import compat.*;");
        engine.eval("A = [1 4 7; 2 5 8; 3 6 9; 4 7 10; 5 8 11];");
        engine.eval("v = [2 5 3 7 4 6 NaN 8 1 9]';");
    }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(VarStdTest, VarMatrixDefault)
{
    eval("y = var(A);");
    EXPECT_DOUBLE_EQ(evalScalar("y(1)"), 2.5);
    EXPECT_DOUBLE_EQ(evalScalar("y(2)"), 2.5);
    EXPECT_DOUBLE_EQ(evalScalar("y(3)"), 2.5);
}

TEST_F(VarStdTest, VarNormFlag1)
{
    eval("y = var(A, 1);");
    EXPECT_DOUBLE_EQ(evalScalar("y(1)"), 2.0);
}

TEST_F(VarStdTest, VarAll)
{
    EXPECT_NEAR(evalScalar("var(A, 0, 'all')"), 8.5714285714, 1e-10);
}

TEST_F(VarStdTest, VarVecdimFullCoverage)
{
    EXPECT_NEAR(evalScalar("var(A, 0, [1 2])"), 8.5714285714, 1e-10);
}

TEST_F(VarStdTest, VarWeightVector)
{
    // var([1 2 3 4 5], [1 2 1 2 1]) ≈ 1.7142857
    EXPECT_NEAR(evalScalar("var([1 2 3 4 5]', [1 2 1 2 1]')"),
                1.7142857142857142, 1e-10);
}

TEST_F(VarStdTest, VarOmitnanExplicit)
{
    EXPECT_DOUBLE_EQ(evalScalar("var(v, 0, 'omitnan')"), 7.5);
}

TEST_F(VarStdTest, VarDefaultPoisonsNaN)
{
    EXPECT_TRUE(std::isnan(evalScalar("var(v)")));
}

TEST_F(VarStdTest, StdMatrix)
{
    eval("y = std(A);");
    EXPECT_NEAR(evalScalar("y(1)"), std::sqrt(2.5), 1e-12);
}

TEST_F(VarStdTest, StdAll)
{
    EXPECT_NEAR(evalScalar("std(A, 0, 'all')"),
                std::sqrt(8.5714285714), 1e-9);
}

TEST_F(VarStdTest, StdWeighted)
{
    EXPECT_NEAR(evalScalar("std([1 2 3 4 5]', [1 2 1 2 1]')"),
                std::sqrt(1.7142857142857142), 1e-10);
}

// --- Single-element variance (n == 1). MATLAB returns 0 for BOTH the
// default N-1 normalization and the N normalization — not NaN from 0/0.
// Regression for the DEEP-PROBE find 2026-05-29.
TEST_F(VarStdTest, VarSingleScalarIsZero)
{
    EXPECT_DOUBLE_EQ(evalScalar("var(5)"), 0.0);
}

TEST_F(VarStdTest, VarSingleElementVectorIsZero)
{
    EXPECT_DOUBLE_EQ(evalScalar("var([7])"), 0.0);
}

TEST_F(VarStdTest, VarSingleNormFlag1IsZero)
{
    EXPECT_DOUBLE_EQ(evalScalar("var(5, 1)"), 0.0);
}

TEST_F(VarStdTest, StdSingleScalarIsZero)
{
    EXPECT_DOUBLE_EQ(evalScalar("std(5)"), 0.0);
}

TEST_F(VarStdTest, StdSingleElementVectorIsZero)
{
    EXPECT_DOUBLE_EQ(evalScalar("std([7])"), 0.0);
}

// NOTE: var([1 2 3], 0, 1) -> [0 0 0] in MATLAB (reduce along singleton
// dim 1 = per-column, each one element). numkit currently returns scalar 1
// because the CORE reduction path collapses a vector along an explicit
// singleton dim instead of treating it as a no-op (sum([1 2 3],1) and
// mean([1 2 3],1) show the same core bug). That is a separate, deeper core
// fix (wide blast radius across all reductions) — deferred to a supervised
// cycle. The single-element n==1 -> 0 fix above is independent and correct.

TEST_F(VarStdTest, BadDimFlagThrows)
{
    EXPECT_THROW(eval("var(A, 0, 'unknown');"), numkit::Error);
}

TEST_F(VarStdTest, PartialVecdimRejected)
{
    // Partial vecdim is documented as not-supported.
    EXPECT_THROW(eval("A3 = repmat(A, [1 1 2]); var(A3, 0, [1 2]);"),
                 numkit::Error);
}
