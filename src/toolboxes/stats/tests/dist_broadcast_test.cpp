// toolboxes/stats/tests/dist_broadcast_test.cpp
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
//   - students_t  (tpdf/tcdf/tinv)           — cycle 33 (betainc-based + nu==Inf)
//   - fisher_f    (fpdf/fcdf/finv)           — cycle 34 (betainc-based, 2-param)
//   - binomial    (binopdf/binocdf/binoinv)  — cycle 35 (discrete)
//   - poisson     (poisspdf/poisscdf/poissinv) — cycle 35 (discrete)
//   - unid        (unidpdf/unidcdf/unidinv)  — cycle 36 (discrete closed-form)
//   - geometric   (geopdf/geocdf/geoinv)     — cycle 36 (discrete closed-form)
//   - negbin      (nbinpdf/nbincdf/nbininv)  — cycle 37 (discrete, betainc)
//   - hypergeom   (hygepdf/hygecdf/hygeinv)  — cycle 37 (discrete, 4-operand)
// MATLAB R2025b reference values.

#include <numkit/core/engine.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

class DistBroadcastTest : public ::testing::Test
{
public:
    numkit::StandardEngine engine;
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

// ── Student's t: betainc-based broadcast over nu (cycle 33) ──────────
TEST_F(DistBroadcastTest, TpdfBroadcast)
{
    eval("y = tpdf(0, [1 2 10]);");
    EXPECT_NEAR(evalScalar("y(1)"), 0.3183098861837907, 1e-12);
    EXPECT_NEAR(evalScalar("y(3)"), 0.3891083839660307, 1e-12);
    eval("z = tpdf([0 1 2], 5);");
    EXPECT_NEAR(evalScalar("z(3)"), 0.0650903103262226, 1e-12);
    eval("d = tpdf(0, [0 -1 2]);");                 // nu<=0 → NaN
    EXPECT_TRUE(eval("isnan(d(1))").toBool());
    EXPECT_TRUE(eval("isnan(d(2))").toBool());
    EXPECT_NEAR(evalScalar("d(3)"), 0.3535533905932738, 1e-12);
}

TEST_F(DistBroadcastTest, TcdfBroadcast)
{
    eval("y = tcdf(1, [1 2 10]);");
    EXPECT_NEAR(evalScalar("y(1)"), 0.75, 1e-10);
    EXPECT_NEAR(evalScalar("y(3)"), 0.8295534338489997, 1e-10);
    eval("s = tcdf([-1 0 1], 5);");                 // x-sign symmetry
    EXPECT_NEAR(evalScalar("s(1)"), 0.1816087338246085, 1e-10);
    EXPECT_NEAR(evalScalar("s(2)"), 0.5, 1e-12);
    eval("u = tcdf(1, [1 2 10], 'upper');");
    EXPECT_NEAR(evalScalar("u(1)"), 0.25, 1e-10);
    // nu==Inf per element → normcdf
    eval("w = tcdf(1, [5 Inf]);");
    EXPECT_NEAR(evalScalar("w(2)"), 0.8413447460685429, 1e-12);
}

TEST_F(DistBroadcastTest, TinvBroadcast)
{
    eval("y = tinv(0.975, [1 2 10]);");
    EXPECT_NEAR(evalScalar("y(1)"), 12.7062047361747, 1e-7);
    EXPECT_NEAR(evalScalar("y(3)"), 2.2281388519649385, 1e-9);
    eval("s = tinv([.025 .5 .975], 5);");
    EXPECT_NEAR(evalScalar("s(1)"), -2.5705818356363, 1e-9);
    EXPECT_DOUBLE_EQ(evalScalar("s(2)"), 0.0);
    // nu==Inf per element → norminv
    eval("w = tinv(0.975, [5 Inf]);");
    EXPECT_NEAR(evalScalar("w(1)"), 2.5705818356363, 1e-9);
    EXPECT_NEAR(evalScalar("w(2)"), 1.9599639845400, 1e-8);   // Winitzki norminv
}

// ── Fisher F: betainc-based broadcast over d1,d2 (cycle 34) ──────────
TEST_F(DistBroadcastTest, FpdfBroadcast)
{
    eval("y = fpdf(1, [1 2], [1 2]);");
    EXPECT_NEAR(evalScalar("y(1)"), 0.1591549430918954, 1e-12);
    EXPECT_NEAR(evalScalar("y(2)"), 0.25, 1e-12);
    eval("z = fpdf([0.5 1 2], 5, 10);");
    EXPECT_NEAR(evalScalar("z(1)"), 0.6876070027706, 1e-11);
    EXPECT_NEAR(evalScalar("z(3)"), 0.1620057421801, 1e-11);
    // x==0 boundary regimes: v1<2 → Inf, v1==2 → finite, v1>2 → 0
    eval("e = fpdf(0, [1 2 3], 5);");
    EXPECT_TRUE(eval("isinf(e(1))").toBool());
    EXPECT_NEAR(evalScalar("e(2)"), 1.0, 1e-12);
    EXPECT_DOUBLE_EQ(evalScalar("e(3)"), 0.0);
    EXPECT_TRUE(eval("isnan(fpdf(1,0,2))").toBool());   // d1<=0 → NaN
}

TEST_F(DistBroadcastTest, FcdfBroadcast)
{
    eval("y = fcdf(2, [1 5], [1 5]);");
    EXPECT_NEAR(evalScalar("y(1)"), 0.6081734479694, 1e-10);
    EXPECT_NEAR(evalScalar("y(2)"), 0.7674886808696, 1e-10);
    eval("z = fcdf([0.5 1 2], 5, 10);");
    EXPECT_NEAR(evalScalar("z(3)"), 0.8358050491003, 1e-10);
    eval("u = fcdf(2, [1 5], [1 5], 'upper');");
    EXPECT_NEAR(evalScalar("u(2)"), 0.2325113191304, 1e-10);
}

TEST_F(DistBroadcastTest, FinvBroadcast)
{
    eval("y = finv(0.95, [1 5], [1 5]);");
    EXPECT_NEAR(evalScalar("y(1)"), 161.4476387976, 1e-6);
    EXPECT_NEAR(evalScalar("y(2)"), 5.050329057633, 1e-8);
    eval("z = finv([.1 .5 .9], 5, 10);");
    EXPECT_NEAR(evalScalar("z(1)"), 0.3032690890211, 1e-9);
    EXPECT_NEAR(evalScalar("z(3)"), 2.52164068621, 1e-8);
    eval("d = finv(0.95, [0 2], 2);");           // d1<=0 → NaN
    EXPECT_TRUE(eval("isnan(d(1))").toBool());
    EXPECT_NEAR(evalScalar("d(2)"), 19.0, 1e-7);
}

// ── Discrete: binomial + poisson parameter broadcast (cycle 35) ──────
TEST_F(DistBroadcastTest, BinomialBroadcast)
{
    eval("y = binopdf(2, [4 5 6], 0.5);");      // vector n
    EXPECT_NEAR(evalScalar("y(1)"), 0.375, 1e-13);
    EXPECT_NEAR(evalScalar("y(3)"), 0.234375, 1e-13);
    eval("z = binopdf(2, 5, [0.1 0.5 0.9]);");  // vector p
    EXPECT_NEAR(evalScalar("z(1)"), 0.0729, 1e-13);
    EXPECT_NEAR(evalScalar("z(3)"), 0.0081, 1e-13);
    EXPECT_DOUBLE_EQ(evalScalar("binopdf(1.5, 5, 0.5)"), 0.0);     // noninteger k → 0
    EXPECT_TRUE(eval("isnan(binopdf(2, 4.5, 0.5))").toBool());     // noninteger n → NaN
    eval("c = binocdf(2, [4 5 6], 0.5);");
    EXPECT_NEAR(evalScalar("c(1)"), 0.6875, 1e-10);
    EXPECT_NEAR(evalScalar("c(3)"), 0.34375, 1e-10);
    eval("q = binoinv([.1 .5 .9], 10, 0.5);");  // discrete quantile
    EXPECT_DOUBLE_EQ(evalScalar("q(1)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("q(3)"), 7.0);
}

TEST_F(DistBroadcastTest, PoissonBroadcast)
{
    eval("y = poisspdf(2, [1 2 3]);");          // vector lambda
    EXPECT_NEAR(evalScalar("y(1)"), 0.1839397205857212, 1e-13);
    EXPECT_NEAR(evalScalar("y(3)"), 0.2240418076553673, 1e-13);
    eval("c = poisscdf(2, [1 2 3]);");
    EXPECT_NEAR(evalScalar("c(1)"), 0.9196986029286058, 1e-10);
    EXPECT_NEAR(evalScalar("c(3)"), 0.4231900811067840, 1e-9);
    eval("q = poissinv(0.5, [1 5 10]);");
    EXPECT_DOUBLE_EQ(evalScalar("q(2)"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("q(3)"), 10.0);
    // per-element domain: lambda==0 → pmf 0 at k>0, lambda<0 → NaN
    eval("d = poisspdf(2, [0 -1 2]);");
    EXPECT_DOUBLE_EQ(evalScalar("d(1)"), 0.0);
    EXPECT_TRUE(eval("isnan(d(2))").toBool());
}

// ── Discrete closed-form: unid + geometric broadcast (cycle 36) ──────
TEST_F(DistBroadcastTest, UnidBroadcast)
{
    eval("y = unidpdf(3, [5 10]);");            // vector N
    EXPECT_NEAR(evalScalar("y(1)"), 0.2, 1e-13);
    EXPECT_NEAR(evalScalar("y(2)"), 0.1, 1e-13);
    eval("z = unidpdf([1 3 7], 5);");           // k>N → 0
    EXPECT_NEAR(evalScalar("z(2)"), 0.2, 1e-13);
    EXPECT_DOUBLE_EQ(evalScalar("z(3)"), 0.0);
    eval("c = unidcdf(3, [5 10]);");
    EXPECT_NEAR(evalScalar("c(1)"), 0.6, 1e-13);
    eval("q = unidinv(0.5, [10 20]);");
    EXPECT_DOUBLE_EQ(evalScalar("q(1)"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("q(2)"), 10.0);
    // N<1 / noninteger N → NaN per element
    eval("d = unidpdf(3, [0 -1 5]);");
    EXPECT_TRUE(eval("isnan(d(1))").toBool());
    EXPECT_NEAR(evalScalar("d(3)"), 0.2, 1e-13);
    EXPECT_TRUE(eval("isnan(unidpdf(3, 4.5))").toBool());
}

TEST_F(DistBroadcastTest, GeometricBroadcast)
{
    eval("y = geopdf(2, [0.2 0.5]);");          // vector p
    EXPECT_NEAR(evalScalar("y(1)"), 0.128, 1e-13);
    EXPECT_NEAR(evalScalar("y(2)"), 0.125, 1e-13);
    eval("c = geocdf(2, [0.2 0.5]);");
    EXPECT_NEAR(evalScalar("c(1)"), 0.488, 1e-13);
    EXPECT_NEAR(evalScalar("c(2)"), 0.875, 1e-13);
    eval("q = geoinv([.1 .5 .9], 0.3);");       // MATLAB geo starts at 0
    EXPECT_DOUBLE_EQ(evalScalar("q(1)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("q(2)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("q(3)"), 6.0);
    EXPECT_TRUE(eval("isnan(geopdf(2, 1.5))").toBool());   // p>1 → NaN
    EXPECT_DOUBLE_EQ(evalScalar("geopdf(1.5, 0.3)"), 0.0); // noninteger k → 0
}

// ── Discrete: negbin (betainc) + hypergeom (4-operand) broadcast (c37) ─
TEST_F(DistBroadcastTest, NegbinBroadcast)
{
    eval("y = nbinpdf(2, [3 5], 0.5);");        // vector r
    EXPECT_NEAR(evalScalar("y(1)"), 0.1875, 1e-12);
    EXPECT_NEAR(evalScalar("y(2)"), 0.1171875, 1e-12);
    eval("z = nbinpdf(2, 3, [0.2 0.8]);");      // vector p
    EXPECT_NEAR(evalScalar("z(1)"), 0.03072, 1e-12);
    EXPECT_NEAR(evalScalar("z(2)"), 0.12288, 1e-12);
    eval("c = nbincdf(2, [3 5], 0.5);");
    EXPECT_NEAR(evalScalar("c(1)"), 0.5, 1e-10);
    EXPECT_NEAR(evalScalar("c(2)"), 0.2265625, 1e-10);
    eval("q = nbininv(0.5, [3 5], 0.5);");
    EXPECT_DOUBLE_EQ(evalScalar("q(1)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("q(2)"), 4.0);
    EXPECT_DOUBLE_EQ(evalScalar("nbinpdf(1.5,3,0.5)"), 0.0);   // noninteger k → 0
    EXPECT_TRUE(eval("isnan(nbinpdf(2,0,0.5))").toBool());     // r<=0 → NaN
}

TEST_F(DistBroadcastTest, HypergeomBroadcast)
{
    eval("y = hygepdf(2, [10 20], 5, 4);");     // vector M (4-operand broadcast)
    EXPECT_NEAR(evalScalar("y(1)"), 0.4761904761905, 1e-11);
    EXPECT_NEAR(evalScalar("y(2)"), 0.2167182662539, 1e-11);
    eval("z = hygepdf([0 1 2], 20, 7, 5);");    // vector k
    EXPECT_NEAR(evalScalar("z(3)"), 0.3873839009288, 1e-11);
    eval("c = hygecdf(2, 20, 7, [3 5]);");      // vector N
    EXPECT_NEAR(evalScalar("c(1)"), 0.969298245614, 1e-10);
    EXPECT_NEAR(evalScalar("c(2)"), 0.7932146542828, 1e-10);
    eval("q = hygeinv(0.5, 20, 7, [3 5]);");
    EXPECT_DOUBLE_EQ(evalScalar("q(1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("q(2)"), 2.0);
    // per-element domain: k>min(N,K) → 0; N>M → NaN
    EXPECT_DOUBLE_EQ(evalScalar("hygepdf(10,20,7,5)"), 0.0);
    eval("d = hygepdf(2, 20, 7, [8 25]);");
    EXPECT_NEAR(evalScalar("d(1)"), 0.2860681114551, 1e-11);
    EXPECT_TRUE(eval("isnan(d(2))").toBool());                // N=25 > M=20 → NaN
    EXPECT_THROW(eval("hygepdf(2, [10 20], [5 6 7], 4);"), std::exception);  // size clash
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
    // students_t scalar-parameter path unchanged
    EXPECT_NEAR(evalScalar("tpdf(0.5,5)"),     0.3279185313226211, 1e-12);
    EXPECT_NEAR(evalScalar("tcdf(2,10)"),      0.9633059826146113, 1e-10);
    EXPECT_NEAR(evalScalar("tinv(0.9,8)"),     1.3968153097438420, 1e-9);
    // fisher_f scalar-parameter path unchanged
    EXPECT_NEAR(evalScalar("fpdf(2,3,4)"),     0.1394274004635, 1e-12);
    EXPECT_NEAR(evalScalar("fcdf(2,5,10)"),    0.8358050491003, 1e-10);
    EXPECT_NEAR(evalScalar("finv(0.95,5,10)"), 3.325834530413, 1e-8);
    // binomial/poisson scalar-parameter path unchanged
    EXPECT_NEAR(evalScalar("binopdf(2,5,0.3)"), 0.3087, 1e-12);
    EXPECT_NEAR(evalScalar("binocdf(2,5,0.3)"), 0.83692, 1e-10);
    EXPECT_NEAR(evalScalar("poisscdf(3,4)"),    0.4334701203667, 1e-9);
    EXPECT_DOUBLE_EQ(evalScalar("poissinv(0.7,5)"), 6.0);
    // unid/geometric scalar-parameter path unchanged
    EXPECT_NEAR(evalScalar("unidpdf(3,10)"),    0.1, 1e-13);
    EXPECT_NEAR(evalScalar("geopdf(2,0.3)"),    0.147, 1e-13);
    EXPECT_DOUBLE_EQ(evalScalar("geoinv(0.7,0.3)"), 3.0);
    // negbin/hypergeom scalar-parameter path unchanged
    EXPECT_NEAR(evalScalar("nbinpdf(2,3,0.5)"), 0.1875, 1e-12);
    EXPECT_DOUBLE_EQ(evalScalar("nbininv(0.7,3,0.5)"), 4.0);
    EXPECT_NEAR(evalScalar("hygepdf(2,20,7,5)"), 0.3873839009288, 1e-11);
    EXPECT_DOUBLE_EQ(evalScalar("hygeinv(0.6,20,7,5)"), 2.0);
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
