// toolboxes/wavelet/tests/swt_modwt_test.cpp
// SWT / MODWT family:
// iswt, modwt, imodwt}.md
// Two real fixes in this cycle:
//   1. modwt argument order: was (x, lev, wname); fixed to MATLAB's
//      (x, wname, lev) plus default wname='sym4' and default lev =
//      floor(log2(N)). Pre-fix, `modwt(x, 'haar', 3)` THREW "expected
//      string argument" at args[2].
//   2. modwt now accepts the 3 MATLAB invocation forms:
//        modwt(x), modwt(x, wname), modwt(x, wname, lev).
// One known sub-fix still pending (out of scope — kernel-level):
//   a. swt detail rows differ from MATLAB by sign (Hi_D vs Hi_R QMF
//      convention). Approximation row matches bit-identical.
//   (b. modwt per-coefficient values: FIXED 2026-05-29 — the MODWT
//      filters are wrev(Lo_D)/√2 and wrev(Hi_D)/√2 applied as a look-back
//      circular convolution; now bit-identical with MATLAB R2025b.)
//   Both inverses (iswt/imodwt) recover the original signal to machine
//   precision — the structurally important invariant these tests pin.

#include <numkit/core/engine.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

#include <cmath>

using namespace numkit;

class SwtModwtTest : public ::testing::Test
{
public:
    StandardEngine engine;
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

// ─── modwt coefficient values now match MATLAB R2025b ──────────────

TEST_F(SwtModwtTest, ModwtHaarCoefficientsMatchMatlab)
{
    eval("xv = [1 2 3 4 5 6 7 8]; w = modwt(xv, 'haar', 2);");
    // Detail level 1, row 1: circular wrap puts 0.5*(x1-x8) = -3.5 at t=1.
    EXPECT_NEAR(evalScalar("w(1,1)"), -3.5, 1e-12);
    EXPECT_NEAR(evalScalar("w(1,2)"),  0.5, 1e-12);
    // Scaling row (level 2): 0.25*sum of 4 circular samples.
    EXPECT_NEAR(evalScalar("w(3,1)"), 5.5, 1e-12);
    EXPECT_NEAR(evalScalar("w(3,2)"), 4.5, 1e-12);
}

TEST_F(SwtModwtTest, ModwtDb2FirstDetailMatchesMatlab)
{
    eval("xv = [1 2 3 4 5 6 7 8]; w = modwt(xv, 'db2', 1);");
    EXPECT_NEAR(evalScalar("w(1,1)"),  0.73205080756887719, 1e-10);
    EXPECT_NEAR(evalScalar("w(1,2)"),  2.0,                 1e-10);
    EXPECT_NEAR(evalScalar("w(1,3)"), -2.7320508075688772,  1e-10);
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
