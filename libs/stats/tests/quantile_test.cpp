// libs/stats/tests/quantile_test.cpp
// Joint regression tests for quantile / prctile / iqr. Hardcoded
// expected values captured from MATLAB R2025b probe.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class QuantileTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override
    {
        engine.eval("import compat.*;");
        engine.eval("v10 = [1:10]';");
        engine.eval("A = [1 4 7; 2 5 8; 3 6 9; 4 7 10; 5 8 11];");
    }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// ── R2007a ("midpoint") default verifies MATLAB quartile values ───────

TEST_F(QuantileTest, MidpointDefaultMatchesMATLABQuartiles)
{
    EXPECT_DOUBLE_EQ(evalScalar("quantile(v10, 0.25)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("quantile(v10, 0.5)"),  5.5);
    EXPECT_DOUBLE_EQ(evalScalar("quantile(v10, 0.75)"), 8.0);
}

TEST_F(QuantileTest, MidpointSmallNExactPositions)
{
    EXPECT_DOUBLE_EQ(evalScalar("quantile([1:5]', 0.25)"), 1.75);
    EXPECT_DOUBLE_EQ(evalScalar("quantile([1:5]', 0.5)"),  3.0);
    EXPECT_DOUBLE_EQ(evalScalar("quantile([1:5]', 0.75)"), 4.25);
}

TEST_F(QuantileTest, ClampingAtEdges)
{
    // p < 0.5/N maps to q < 1, clamps to first element
    EXPECT_DOUBLE_EQ(evalScalar("quantile([1:5]', 0.05)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("quantile([1:5]', 0.99)"), 5.0);
}

// ── Method = inclusive | exclusive | approximate ──────────────────────

TEST_F(QuantileTest, MethodInclusive)
{
    // Type-7: q = p*(N-1)+1 → 0.25*9+1 = 3.25
    EXPECT_DOUBLE_EQ(evalScalar("quantile(v10, 0.25, 'Method', 'inclusive')"),
                     3.25);
}

TEST_F(QuantileTest, MethodExclusive)
{
    // Type-6: q = p*(N+1) → 0.25*11 = 2.75
    EXPECT_DOUBLE_EQ(evalScalar("quantile(v10, 0.25, 'Method', 'exclusive')"),
                     2.75);
}

TEST_F(QuantileTest, MethodApproximateFallsBackToMidpoint)
{
    // Approximate currently routes to midpoint.
    EXPECT_DOUBLE_EQ(evalScalar("quantile(v10, 0.25, 'Method', 'approximate')"),
                     3.0);
}

TEST_F(QuantileTest, BadMethodErrors)
{
    EXPECT_THROW(eval("quantile(v10, 0.25, 'Method', 'unknown');"),
                 numkit::Error);
}

// ── 'all' / vecdim dispatch ───────────────────────────────────────────

TEST_F(QuantileTest, AllStringFlattens)
{
    EXPECT_DOUBLE_EQ(evalScalar("quantile(A, 0.5, 'all')"), 6.0);
}

TEST_F(QuantileTest, VecdimFullCoverageEqualsAll)
{
    EXPECT_DOUBLE_EQ(evalScalar("quantile(A, 0.5, [1 2])"), 6.0);
}

TEST_F(QuantileTest, VectorPGivesRow)
{
    eval("qv = quantile(v10, [0.25 0.5 0.75]);");
    EXPECT_DOUBLE_EQ(evalScalar("qv(1)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("qv(2)"), 5.5);
    EXPECT_DOUBLE_EQ(evalScalar("qv(3)"), 8.0);
}

// ── matrix dim default and explicit ───────────────────────────────────

TEST_F(QuantileTest, MatrixDefaultDim)
{
    eval("qA = quantile(A, 0.5);");
    EXPECT_DOUBLE_EQ(evalScalar("qA(1)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("qA(2)"), 6.0);
    EXPECT_DOUBLE_EQ(evalScalar("qA(3)"), 9.0);
}

TEST_F(QuantileTest, MatrixDim2)
{
    eval("qD2 = quantile(A, 0.5, 2);");
    EXPECT_DOUBLE_EQ(evalScalar("qD2(1)"), 4.0);
    EXPECT_DOUBLE_EQ(evalScalar("qD2(5)"), 8.0);
}

// ── prctile ────────────────────────────────────────────────────────────

TEST_F(QuantileTest, PrctileMatchesQuantileAtScaledP)
{
    EXPECT_DOUBLE_EQ(evalScalar("prctile(v10, 25)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("prctile(v10, 75)"), 8.0);
}

TEST_F(QuantileTest, PrctileVectorP)
{
    eval("p2 = prctile(v10, [25 75]);");
    EXPECT_DOUBLE_EQ(evalScalar("p2(1)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("p2(2)"), 8.0);
}

TEST_F(QuantileTest, PrctileAll)
{
    EXPECT_DOUBLE_EQ(evalScalar("prctile(A, 50, 'all')"), 6.0);
}

// ── iqr ────────────────────────────────────────────────────────────────

TEST_F(QuantileTest, IqrVectorMatchesMATLAB)
{
    EXPECT_DOUBLE_EQ(evalScalar("iqr(v10)"), 5.0);
}

TEST_F(QuantileTest, IqrMatrixDefaultDim)
{
    eval("rA = iqr(A);");
    EXPECT_DOUBLE_EQ(evalScalar("rA(1)"), 2.5);
    EXPECT_DOUBLE_EQ(evalScalar("rA(2)"), 2.5);
    EXPECT_DOUBLE_EQ(evalScalar("rA(3)"), 2.5);
}

TEST_F(QuantileTest, IqrAll)
{
    EXPECT_DOUBLE_EQ(evalScalar("iqr(A, 'all')"), 4.0);
}

TEST_F(QuantileTest, IqrVecdim)
{
    EXPECT_DOUBLE_EQ(evalScalar("iqr(A, [1 2])"), 4.0);
}

TEST_F(QuantileTest, IqrPartialVecdimRejected)
{
    // Partial vecdim is not yet supported (parity gap, documented).
    EXPECT_THROW(eval("A3 = repmat(A, [1 1 2]); iqr(A3, [1 2]);"),
                 numkit::Error);
}

// MATLAB's 'Method' is 'exact' (default) / 'approximate'. 'exact' is the
// linear-interpolation order-statistic method (numkit's default). Was
// rejected before. vs MATLAB R2025b.
TEST_F(QuantileTest, MethodExactAlias)
{
    EXPECT_DOUBLE_EQ(evalScalar("quantile([1 2 3 4], 0.25, 'Method', 'exact')"), 1.5);
    EXPECT_DOUBLE_EQ(evalScalar("quantile([1 2 3 4], 0.25, 'Method', 'approximate')"), 1.5);
    // 'exact' == the default.
    EXPECT_DOUBLE_EQ(evalScalar("quantile(v10, 0.5, 'Method', 'exact')"),
                     evalScalar("quantile(v10, 0.5)"));
    EXPECT_THROW(eval("quantile([1 2 3 4], 0.25, 'Method', 'bogus');"), numkit::Error);
}
