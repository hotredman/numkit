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
//   - gamma       (gampdf/gamcdf/gaminv)     — cycle 30 pdf+cdf, 31 inv
//   - beta        (betapdf/betacdf/betainv)  — cycle 30 pdf+cdf, 31 inv
//   - chi2        (chi2pdf/chi2cdf/chi2inv)  — cycle 30 pdf+cdf, 31 inv
//   - rayleigh    (raylpdf/raylcdf/raylinv)  — cycle 32
//   - weibull     (wblpdf/wblcdf/wblinv)     — cycle 32
//   - lognormal   (lognpdf/logncdf/logninv)  — cycle 32
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

// ── Gamma / Beta / Chi2: pdf + cdf parameter broadcast (cycle 30) ─────
TEST_F(DistBroadcastTest, GampdfBroadcast)
{
    eval("y = gampdf(1, [1 2 3], 1);");        // vector shape a
    EXPECT_NEAR(evalScalar("y(1)"), 0.3678794411714423, 1e-12);
    EXPECT_NEAR(evalScalar("y(3)"), 0.1839397205857212, 1e-12);
    eval("z = gampdf([1 2 3], 2, 2);");        // vector x, scalar a/b
    EXPECT_NEAR(evalScalar("z(1)"), 0.1516326649281583, 1e-12);
    EXPECT_NEAR(evalScalar("z(3)"), 0.1673476201113224, 1e-12);
    // per-element domain: a==0 → 0, a<0 → NaN
    eval("d = gampdf(1, [0 -1 2], 1);");
    EXPECT_DOUBLE_EQ(evalScalar("d(1)"), 0.0);
    EXPECT_TRUE(eval("isnan(d(2))").toBool());
}

TEST_F(DistBroadcastTest, GamcdfBroadcast)
{
    eval("y = gamcdf(1, [1 2 3], 1);");
    EXPECT_NEAR(evalScalar("y(1)"), 0.6321205588285577, 1e-10);
    EXPECT_NEAR(evalScalar("y(3)"), 0.0803013970713942, 1e-10);
    eval("z = gamcdf([1 2 3], 2, 2);");
    EXPECT_NEAR(evalScalar("z(3)"), 0.4421745996289252, 1e-10);
    // b<=0 → NaN per element ('upper' too)
    eval("u = gamcdf(1, 2, [1 0 -1], 'upper');");
    EXPECT_NEAR(evalScalar("u(1)"), 0.7357588823428847, 1e-10);
    EXPECT_TRUE(eval("isnan(u(2))").toBool());
}

TEST_F(DistBroadcastTest, BetapdfBroadcast)
{
    eval("y = betapdf(0.5, [2 3], [2 2]);");   // both params vectors
    EXPECT_NEAR(evalScalar("y(1)"), 1.5, 1e-12);
    EXPECT_NEAR(evalScalar("y(2)"), 1.5, 1e-12);
    eval("z = betapdf([.2 .5 .8], 2, 3);");
    EXPECT_NEAR(evalScalar("z(1)"), 1.536, 1e-12);
    EXPECT_NEAR(evalScalar("z(3)"), 0.384, 1e-12);
    EXPECT_TRUE(eval("isnan(betapdf(0.5, -1, 2))").toBool());   // a<=0 → NaN
}

TEST_F(DistBroadcastTest, BetacdfBroadcast)
{
    eval("y = betacdf(0.5, [2 3], 2);");
    EXPECT_NEAR(evalScalar("y(1)"), 0.5, 1e-10);
    EXPECT_NEAR(evalScalar("y(2)"), 0.3125, 1e-10);
}

TEST_F(DistBroadcastTest, Chi2pdfCdfBroadcast)
{
    eval("y = chi2pdf(2, [1 2 3]);");          // vector dof k
    EXPECT_NEAR(evalScalar("y(1)"), 0.1037768743551486, 1e-12);
    EXPECT_NEAR(evalScalar("y(3)"), 0.2075537487102976, 1e-12);
    eval("c = chi2cdf(2, [1 2 3]);");
    EXPECT_NEAR(evalScalar("c(1)"), 0.8427007929497149, 1e-10);
    EXPECT_NEAR(evalScalar("c(3)"), 0.4275932955291202, 1e-10);
    // k==0 → 0, k<0 → NaN
    eval("d = chi2pdf(2, [0 -1 4]);");
    EXPECT_DOUBLE_EQ(evalScalar("d(1)"), 0.0);
    EXPECT_TRUE(eval("isnan(d(2))").toBool());
}

// ── Gamma / Beta / Chi2: inv parameter broadcast (cycle 31) ──────────
TEST_F(DistBroadcastTest, GaminvBroadcast)
{
    eval("y = gaminv(0.5, [1 2 3], 1);");
    EXPECT_NEAR(evalScalar("y(1)"), 0.6931471805599453, 1e-9);
    EXPECT_NEAR(evalScalar("y(3)"), 2.6740603137235100, 1e-9);
    eval("z = gaminv([.1 .5 .9], 2, 2);");
    EXPECT_NEAR(evalScalar("z(3)"), 7.7794403397348026, 1e-8);
    // degenerate a==0 → 0, b<=0 → NaN
    eval("d = gaminv(0.5, [0 2], 1);");
    EXPECT_DOUBLE_EQ(evalScalar("d(1)"), 0.0);
    eval("e = gaminv(0.5, 2, [1 0 -1]);");
    EXPECT_NEAR(evalScalar("e(1)"), 1.6783469900166607, 1e-9);
    EXPECT_TRUE(eval("isnan(e(2))").toBool());
    EXPECT_TRUE(eval("isnan(e(3))").toBool());
}

TEST_F(DistBroadcastTest, BetainvBroadcast)
{
    eval("y = betainv(0.5, [2 3], 2);");
    EXPECT_NEAR(evalScalar("y(1)"), 0.5, 1e-9);
    EXPECT_NEAR(evalScalar("y(2)"), 0.6142724318676758, 1e-8);
    eval("z = betainv([.1 .5 .9], 2, 3);");
    EXPECT_NEAR(evalScalar("z(1)"), 0.1425593167080900, 1e-8);
    EXPECT_NEAR(evalScalar("z(3)"), 0.6795394162777841, 1e-8);
    EXPECT_TRUE(eval("isnan(betainv(0.5, -1, 2))").toBool());   // a<=0 → NaN
}

TEST_F(DistBroadcastTest, Chi2invBroadcast)
{
    eval("y = chi2inv(0.5, [1 2 3]);");
    EXPECT_NEAR(evalScalar("y(1)"), 0.4549364231195724, 1e-9);
    EXPECT_NEAR(evalScalar("y(3)"), 2.3659738843753377, 1e-9);
    // degenerate k==0 → 0, k<0 → NaN
    eval("d = chi2inv(0.5, [0 4]);");
    EXPECT_DOUBLE_EQ(evalScalar("d(1)"), 0.0);
    EXPECT_NEAR(evalScalar("d(2)"), 3.3566939800333233, 1e-8);
    EXPECT_TRUE(eval("isnan(chi2inv(0.5, -1))").toBool());
}

TEST_F(DistBroadcastTest, InvMismatchThrows)
{
    EXPECT_THROW(eval("gaminv([.1 .5 .9], [1 2], 1);"), std::exception);
    EXPECT_THROW(eval("betainv(0.5, [2 3 4], [1 2]);"), std::exception);
    EXPECT_THROW(eval("chi2inv([.1 .5], [1 2 3]);"), std::exception);
    EXPECT_EQ(static_cast<int>(evalScalar("numel(gaminv([], [2 2], 1))")), 0);
}

// ── Rayleigh / Weibull / Lognormal: closed-form broadcast (cycle 32) ─
TEST_F(DistBroadcastTest, RayleighBroadcast)
{
    eval("y = raylpdf(1, [1 2 3]);");
    EXPECT_NEAR(evalScalar("y(1)"), 0.6065306597126334, 1e-13);
    EXPECT_NEAR(evalScalar("y(3)"), 0.1051066076562796, 1e-13);
    eval("c = raylinv(0.5, [1 2 3]);");
    EXPECT_NEAR(evalScalar("c(1)"), 1.1774100225154747, 1e-12);
    EXPECT_NEAR(evalScalar("c(3)"), 3.5322300675464239, 1e-12);
    eval("d = raylpdf(1, [0 -1 2]);");          // b<=0 → NaN
    EXPECT_TRUE(eval("isnan(d(1))").toBool());
    EXPECT_TRUE(eval("isnan(d(2))").toBool());
    EXPECT_NEAR(evalScalar("d(3)"), 0.2206242256460882, 1e-13);
}

TEST_F(DistBroadcastTest, WeibullBroadcast)
{
    eval("y = wblpdf(1, [1 2], [1 2]);");       // both params vectors
    EXPECT_NEAR(evalScalar("y(1)"), 0.3678794411714423, 1e-13);
    EXPECT_NEAR(evalScalar("y(2)"), 0.3894003915357024, 1e-13);
    eval("c = wblcdf(1, [1 2], 2);");
    EXPECT_NEAR(evalScalar("c(2)"), 0.2211992169285951, 1e-12);
    eval("q = wblinv(0.5, [1 2], 2);");
    EXPECT_NEAR(evalScalar("q(2)"), 1.6651092223153962, 1e-12);
    EXPECT_TRUE(eval("isnan(wblpdf(1,0,1))").toBool());   // a<=0 → NaN
}

TEST_F(DistBroadcastTest, LognormalBroadcast)
{
    eval("y = lognpdf(1, [0 1], 1);");
    EXPECT_NEAR(evalScalar("y(1)"), 0.3989422804014327, 1e-13);
    EXPECT_NEAR(evalScalar("y(2)"), 0.2419707245191434, 1e-13);
    eval("c = logncdf([1 2 4], 0, 1);");
    EXPECT_NEAR(evalScalar("c(2)"), 0.7558914042144173, 1e-10);
    EXPECT_NEAR(evalScalar("c(3)"), 0.9171714809983015, 1e-10);
    // logninv via Acklam phiInv (~1e-9) → looser tol
    eval("q = logninv([.1 .5 .9], 0, 1);");
    EXPECT_NEAR(evalScalar("q(1)"), 0.2776062418520098, 1e-8);
    EXPECT_NEAR(evalScalar("q(3)"), 3.6022244792791974, 1e-8);
    eval("d = lognpdf(1, 0, [0 -1 2]);");        // sigma<=0 → NaN
    EXPECT_TRUE(eval("isnan(d(1))").toBool());
    EXPECT_NEAR(evalScalar("d(3)"), 0.1994711402007163, 1e-13);
}

// ── Regressions: scalar-parameter path unchanged; edges ──────────────
TEST_F(DistBroadcastTest, ScalarPathUnchanged)
{
    EXPECT_NEAR(evalScalar("normpdf(0)"),     0.3989422804014326, 1e-15);
    EXPECT_NEAR(evalScalar("normpdf(1,0,2)"), 0.1760326633821498, 1e-15);
    EXPECT_NEAR(evalScalar("exppdf(2)"),      0.1353352832366127, 1e-15);
    EXPECT_NEAR(evalScalar("expcdf(2)"),      0.8646647167633873, 1e-15);   // mu=1 default
    EXPECT_DOUBLE_EQ(evalScalar("exppdf(-1, 2)"), 0.0);
    // gamma/beta/chi2 scalar-parameter path unchanged (pdf/cdf/inv)
    EXPECT_NEAR(evalScalar("gampdf(1,2,2)"),  0.1516326649281583, 1e-12);
    EXPECT_NEAR(evalScalar("betapdf(0.3,2,3)"), 1.764, 1e-12);
    EXPECT_NEAR(evalScalar("chi2cdf(3,4)"),   0.4421745996289252, 1e-10);
    EXPECT_NEAR(evalScalar("gaminv(0.5,2,2)"), 3.3566939800333233, 1e-8);
    EXPECT_NEAR(evalScalar("betainv(0.5,2,3)"), 0.3857275681324743, 1e-8);
    EXPECT_NEAR(evalScalar("chi2inv(0.5,4)"),  3.3566939800333233, 1e-8);
    // rayleigh/weibull/lognormal scalar-parameter path unchanged
    EXPECT_NEAR(evalScalar("raylpdf(1,2)"),    0.2206242256460882, 1e-13);
    EXPECT_NEAR(evalScalar("wblpdf(2,3,4)"),   0.3242488131549144, 1e-12);
    EXPECT_NEAR(evalScalar("lognpdf(2,0,1)"),  0.1568740192789855, 1e-13);
}

TEST_F(DistBroadcastTest, EmptyAndMismatch)
{
    EXPECT_EQ(static_cast<int>(evalScalar("numel(normpdf([], 0, 1))")), 0);
    EXPECT_EQ(static_cast<int>(evalScalar("numel(exppdf([], 2))")), 0);
    EXPECT_THROW(eval("normpdf([1 2 3], [1 2], 1);"), std::exception);
    EXPECT_THROW(eval("exppdf([1 2 3], [1 2]);"), std::exception);
    EXPECT_THROW(eval("gampdf(1, [1 2], [1 2 3]);"), std::exception);
    EXPECT_THROW(eval("chi2pdf([1 2 3], [1 2]);"), std::exception);
}
