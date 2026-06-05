// libs/stats/tests/dist_broadcast_test.cpp
//
// Regression guard for bugs/stats/distribution-array-params.md: the *pdf /
// *cdf / *inv distribution functions broadcast ALL arguments (data AND the
// distribution parameters) to a common size, like MATLAB. A scalar expands;
// equal non-scalar sizes element-align; mismatched non-scalar sizes error.
// Per-element domain (sigma<=0 / mu<=0 → NaN) is honoured under broadcast.
//
// Coverage in this file (filled in per /loop cycle as families land):
//   - normal      (normpdf/normcdf/norminv)  — cycle 29
//   - exponential (exppdf/expcdf/expinv)      — cycle 29
// MATLAB R2025b reference values.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

class DistBroadcastTest : public ::testing::Test
{
public:
    numkit::Engine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    numkit::Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// ── Normal: parameter broadcast ──────────────────────────────────────
TEST_F(DistBroadcastTest, NormpdfVectorSigma)
{
    eval("y = normpdf(0, 0, [1 2 4]);");
    EXPECT_NEAR(evalScalar("y(1)"), 0.3989422804014326, 1e-13);
    EXPECT_NEAR(evalScalar("y(2)"), 0.1994711402007163, 1e-13);
    EXPECT_NEAR(evalScalar("y(3)"), 0.0997355701003582, 1e-13);
}

TEST_F(DistBroadcastTest, NormpdfAllVector)
{
    eval("y = normpdf([0 1], [0 0], [1 2]);");
    EXPECT_NEAR(evalScalar("y(1)"), 0.3989422804014326, 1e-13);
    EXPECT_NEAR(evalScalar("y(2)"), 0.1760326633821498, 1e-13);
}

TEST_F(DistBroadcastTest, NormcdfVectorMu)
{
    eval("y = normcdf(1, [0 1], 1);");
    EXPECT_NEAR(evalScalar("y(1)"), 0.8413447460685429, 1e-13);
    EXPECT_NEAR(evalScalar("y(2)"), 0.5, 1e-15);
}

TEST_F(DistBroadcastTest, NorminvVectorMu)
{
    eval("y = norminv(0.5, [0 5], 1);");
    EXPECT_DOUBLE_EQ(evalScalar("y(1)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(2)"), 5.0);
    eval("z = norminv([.1 .5 .9], 0, [1 2 3]);");
    EXPECT_NEAR(evalScalar("z(1)"), -1.2815515655446001, 1e-9);   // norminv(.1)
    EXPECT_NEAR(evalScalar("z(3)"),  3.8446546966338003, 1e-9);   // 3·norminv(.9)
}

// Per-element domain: sigma<=0 → NaN only at those elements.
TEST_F(DistBroadcastTest, NormpdfPerElementBadSigma)
{
    eval("y = normpdf(0, 0, [1 -1 2]);");
    EXPECT_NEAR(evalScalar("y(1)"), 0.3989422804014326, 1e-13);
    EXPECT_TRUE(eval("isnan(y(2))").toBool());
    EXPECT_NEAR(evalScalar("y(3)"), 0.1994711402007163, 1e-13);
    EXPECT_TRUE(eval("isnan(normpdf(0,0,0))").toBool());
}

// Shape follows the non-scalar operand (2×2 here).
TEST_F(DistBroadcastTest, NormpdfShapePreserved)
{
    eval("Y = normpdf(0, 0, [1 2; 3 4]);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(Y,1)")), 2);
    EXPECT_EQ(static_cast<int>(evalScalar("size(Y,2)")), 2);
    EXPECT_NEAR(evalScalar("Y(2,1)"), 0.1329807601338109, 1e-13);   // sigma=3
}

// ── Exponential: parameter broadcast ─────────────────────────────────
TEST_F(DistBroadcastTest, ExppdfVectorMu)
{
    eval("y = exppdf(1, [1 2 4]);");
    EXPECT_NEAR(evalScalar("y(1)"), 0.3678794411714423, 1e-13);
    EXPECT_NEAR(evalScalar("y(2)"), 0.3032653298563167, 1e-13);
    EXPECT_NEAR(evalScalar("y(3)"), 0.1947001957678512, 1e-13);
}

TEST_F(DistBroadcastTest, ExpcdfVectorMu)
{
    eval("y = expcdf(1, [1 2 4]);");
    EXPECT_NEAR(evalScalar("y(1)"), 0.6321205588285577, 1e-13);
    EXPECT_NEAR(evalScalar("y(3)"), 0.2211992169285951, 1e-13);
    // 'upper' tail under broadcast
    eval("u = expcdf(1, [1 2 4], 'upper');");
    EXPECT_NEAR(evalScalar("u(1)"), 0.3678794411714423, 1e-13);
    EXPECT_NEAR(evalScalar("u(3)"), 0.7788007830714049, 1e-13);
}

TEST_F(DistBroadcastTest, ExpinvVectorMu)
{
    eval("y = expinv(0.5, [1 2 4]);");
    EXPECT_NEAR(evalScalar("y(1)"), 0.6931471805599453, 1e-13);
    EXPECT_NEAR(evalScalar("y(2)"), 1.3862943611198906, 1e-13);
    EXPECT_NEAR(evalScalar("y(3)"), 2.7725887222397811, 1e-13);
}

TEST_F(DistBroadcastTest, ExppdfPerElementBadMu)
{
    eval("y = exppdf(1, [1 0 2]);");
    EXPECT_NEAR(evalScalar("y(1)"), 0.3678794411714423, 1e-13);
    EXPECT_TRUE(eval("isnan(y(2))").toBool());          // mu=0 → NaN
    EXPECT_NEAR(evalScalar("y(3)"), 0.3032653298563167, 1e-13);
}

// ── Regressions: scalar-parameter path unchanged; edges ──────────────
TEST_F(DistBroadcastTest, ScalarPathUnchanged)
{
    EXPECT_NEAR(evalScalar("normpdf(0)"),     0.3989422804014326, 1e-15);
    EXPECT_NEAR(evalScalar("normpdf(1,0,2)"), 0.1760326633821498, 1e-15);
    EXPECT_NEAR(evalScalar("exppdf(2)"),      0.1353352832366127, 1e-15);
    EXPECT_NEAR(evalScalar("expcdf(2)"),      0.8646647167633873, 1e-15);   // mu=1 default
    EXPECT_DOUBLE_EQ(evalScalar("exppdf(-1, 2)"), 0.0);
}

TEST_F(DistBroadcastTest, EmptyAndMismatch)
{
    EXPECT_EQ(static_cast<int>(evalScalar("numel(normpdf([], 0, 1))")), 0);
    EXPECT_EQ(static_cast<int>(evalScalar("numel(exppdf([], 2))")), 0);
    EXPECT_THROW(eval("normpdf([1 2 3], [1 2], 1);"), std::exception);
    EXPECT_THROW(eval("exppdf([1 2 3], [1 2]);"), std::exception);
}
