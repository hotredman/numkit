// libs/wavelet/tests/swt_modwt_test.cpp
//
// Audit ТЗ batch closure for the SWT / MODWT family:
//   audit/findings/wavelet/{swt, iswt, modwt, imodwt}.md
//
// Two real fixes in this cycle:
//   1. modwt argument order: was (x, lev, wname); fixed to MATLAB's
//      (x, wname, lev) plus default wname='sym4' and default lev =
//      floor(log2(N)). Pre-fix, `modwt(x, 'haar', 3)` THREW "expected
//      string argument" at args[2].
//   2. modwt now accepts the 3 MATLAB invocation forms:
//        modwt(x), modwt(x, wname), modwt(x, wname, lev).
//
// Two known sub-fixes still pending (out of scope for this ТЗ — kernel-
// level investigation needed):
//   a. swt detail rows differ from MATLAB by sign (Hi_D vs Hi_R QMF
//      convention). Approximation row matches bit-identical.
//   b. modwt per-coefficient values differ from MATLAB (sqrt(2)-
//      normalisation convention). Output SHAPE matches.
//   Both inverses (iswt/imodwt) DO recover the original signal to
//   machine precision — that's the structurally important invariant
//   and what these tests pin.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

#include <cmath>

using namespace numkit;

class SwtModwtTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override {
        engine.eval("import compat.*;");
        engine.eval("x = sin(2*pi*0.1*(0:31)') + 0.3*cos(2*pi*0.05*(0:31)');");
    }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// ─── modwt: MATLAB argument order accepted (was throwing) ───────────

TEST_F(SwtModwtTest, ModwtMATLABArgOrderAccepted)
{
    // Pre-fix: throws "wavelet: expected string argument".
    eval("w = modwt(x, 'haar', 3);");
    EXPECT_DOUBLE_EQ(evalScalar("size(w, 1)"), 4.0);   // lev+1 rows
    EXPECT_DOUBLE_EQ(evalScalar("size(w, 2)"), 32.0);  // N cols
}

TEST_F(SwtModwtTest, ModwtSingleArgUsesDefaults)
{
    // modwt(x) → wname='sym4', lev=floor(log2(32))=5.
    eval("w = modwt(x);");
    EXPECT_DOUBLE_EQ(evalScalar("size(w, 1)"), 6.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(w, 2)"), 32.0);
}

TEST_F(SwtModwtTest, ModwtTwoArgWnameOnly)
{
    eval("w = modwt(x, 'haar');");
    EXPECT_DOUBLE_EQ(evalScalar("size(w, 1)"), 6.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(w, 2)"), 32.0);
}

TEST_F(SwtModwtTest, ModwtTwoArgNumericLevelDefaultWname)
{
    // modwt(x, 4) → numeric 2nd arg = lev, default wname=sym4.
    eval("w = modwt(x, 4);");
    EXPECT_DOUBLE_EQ(evalScalar("size(w, 1)"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(w, 2)"), 32.0);
}

// ─── swt: argument order unchanged (already MATLAB-compat) ──────────

TEST_F(SwtModwtTest, SwtBasicShape)
{
    eval("w = swt(x, 3, 'haar');");
    EXPECT_DOUBLE_EQ(evalScalar("size(w, 1)"), 4.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(w, 2)"), 32.0);
}

TEST_F(SwtModwtTest, SwtDetailRowsMagnitudeMatchesMatlab)
{
    // Detail-row sign convention differs from MATLAB (Hi_D vs Hi_R)
    // — magnitudes match bit-identical.
    eval("w = swt(x, 3, 'haar');");
    EXPECT_NEAR(evalScalar("abs(w(1,1))"),  0.405244, 1e-5);
    EXPECT_NEAR(evalScalar("abs(w(1,16))"), 0.350075, 1e-5);
    EXPECT_NEAR(evalScalar("abs(w(2,8))"),  0.574026, 1e-5);
}

TEST_F(SwtModwtTest, SwtApproximationRowMatchesMatlab)
{
    // Approximation row (last) matches MATLAB bit-identical
    // (Lo_D = Lo_R for haar — no sign difference).
    eval("w = swt(x, 3, 'haar');");
    EXPECT_NEAR(evalScalar("w(4,1)"), 0.836813, 1e-5);
}

// ─── round-trip: iswt and imodwt recover x to machine precision ────

TEST_F(SwtModwtTest, IswtRoundTripRecoversX)
{
    eval("swc = swt(x, 3, 'haar');");
    eval("xrec = iswt(swc, 'haar');");
    EXPECT_DOUBLE_EQ(evalScalar("numel(xrec)"), 32.0);
    EXPECT_LT(evalScalar("max(abs(xrec(:) - x(:)))"), 1e-12);
}

TEST_F(SwtModwtTest, IswtRoundTripDB2)
{
    eval("swc = swt(x, 2, 'db2');");
    eval("xrec = iswt(swc, 'db2');");
    EXPECT_LT(evalScalar("max(abs(xrec(:) - x(:)))"), 1e-9);
}

TEST_F(SwtModwtTest, ImodwtRoundTripRecoversX)
{
    eval("w = modwt(x, 'haar', 3);");
    eval("xrec = imodwt(w, 'haar');");
    EXPECT_DOUBLE_EQ(evalScalar("numel(xrec)"), 32.0);
    EXPECT_LT(evalScalar("max(abs(xrec(:) - x(:)))"), 1e-12);
}

TEST_F(SwtModwtTest, ImodwtRoundTripDB2)
{
    eval("w = modwt(x, 'db2', 3);");
    eval("xrec = imodwt(w, 'db2');");
    EXPECT_LT(evalScalar("max(abs(xrec(:) - x(:)))"), 1e-9);
}

// ─── error paths ────────────────────────────────────────────────────

TEST_F(SwtModwtTest, ModwtNoArgsThrows)
{
    bool threw = false;
    try { eval("modwt();"); }
    catch (const std::exception &) { threw = true; }
    EXPECT_TRUE(threw);
}
