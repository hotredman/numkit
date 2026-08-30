// toolboxes/image/tests/deconvreg_test.cpp
//
// Regression guard for deconvreg — Tikhonov-regularized FFT-based
// deconvolution. Reference values from MATLAB R2025b, bit-equal at
// the deterministic (no-noise) test case.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class DeconvregTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override {
        engine.eval(

            "I = checkerboard(8);"
            "PSF = fspecial('gaussian', 7, 10);"
            "B = imfilter(I, PSF, 'circular');");
    }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// ── 2-arg form (default NP=0, default LRANGE [1e-9, 1e9]) ───────────

TEST_F(DeconvregTest, DefaultArgs)
{
    eval("J1 = deconvreg(B, PSF);");
    EXPECT_NEAR(evalScalar("J1(1,1)"),   0.2370041031, 1e-8);
    EXPECT_NEAR(evalScalar("J1(32,32)"), 0.2370041031, 1e-8);
}

// ── 3-arg form (NP only, default LRANGE search) ─────────────────────

TEST_F(DeconvregTest, ThreeArgNP)
{
    eval("[J4, L4] = deconvreg(B, PSF, 0.01);");
    EXPECT_NEAR(evalScalar("J4(1,1)"), 0.2370041031, 1e-8);
    EXPECT_NEAR(evalScalar("L4"),      3.486676892e-5, 1e-9);
}

// ── 4-arg form with scalar LRANGE (fixed LAGRA, no search) ──────────

TEST_F(DeconvregTest, ScalarLrangeFixedLagra)
{
    eval("[J9, L9] = deconvreg(B, PSF, 0.01, 0.5);");
    EXPECT_NEAR(evalScalar("J9(1,1)"), 0.397423146, 1e-9);
    EXPECT_NEAR(evalScalar("L9"),      0.5, 1e-12);
}

// ── 4-arg form with [lo,hi] LRANGE (Brent fminbnd search) ───────────

TEST_F(DeconvregTest, VectorLrangeSearch)
{
    eval("[Jb, Lb] = deconvreg(B, PSF, 0.01, [1e-9, 1e9]);");
    EXPECT_NEAR(evalScalar("Jb(1,1)"), 0.2370041031, 1e-8);
    EXPECT_NEAR(evalScalar("Lb"),      3.486676892e-5, 1e-9);
}

// ── 5-arg form with custom 2-D REGOP ────────────────────────────────

TEST_F(DeconvregTest, CustomRegop2D)
{
    eval("regop2 = [1 -1 0; -1 0 1; 0 1 -1];"
         "[Jr, Lr] = deconvreg(B, PSF, 0.01, [1e-9, 1e9], regop2);");
    EXPECT_NEAR(evalScalar("Jr(1,1)"), 0.1891534481, 1e-8);
    EXPECT_NEAR(evalScalar("Lr"),      0.0002888970374, 1e-10);
}

// ── Errors ─────────────────────────────────────────────────────────

TEST_F(DeconvregTest, PsfTooSmallThrows)
{
    EXPECT_THROW(eval("deconvreg(B, [1]);"), std::exception);
}

TEST_F(DeconvregTest, NPMustBeScalarThrows)
{
    EXPECT_THROW(eval("deconvreg(B, PSF, [1 2 3]);"), std::exception);
}

TEST_F(DeconvregTest, LrangeReversedThrows)
{
    EXPECT_THROW(eval("deconvreg(B, PSF, 0.01, [1e9, 1e-9]);"),
                 std::exception);
}
