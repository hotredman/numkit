// libs/signal/tests/windows_extras_test.cpp
//
// Tests for the 11 window functions added in package A1:
//   triang, tukeywin, flattopwin, gausswin, chebwin, parzenwin,
//   nuttallwin, taylorwin, blackmanharris, bohmanwin, barthannwin.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <cmath>
#include <gtest/gtest.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using namespace numkit;

class WindowsExtrasTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &code) { return engine.eval(code); }
    double evalScalar(const std::string &code) { return eval(code).toScalar(); }
};

// Symmetric-window check: w(i) ≈ w(N+1-i) for i = 1 .. floor(N/2).
static void expectSymmetric(WindowsExtrasTest &t, const char *winExpr, int N,
                            double tol = 1e-12)
{
    t.eval(std::string("w = ") + winExpr + ";");
    for (int i = 1; i <= N / 2; ++i) {
        const std::string l = "w(" + std::to_string(i) + ")";
        const std::string r = "w(" + std::to_string(N + 1 - i) + ")";
        EXPECT_NEAR(t.evalScalar(l), t.evalScalar(r), tol)
            << winExpr << " i=" << i;
    }
}

// ── triang ────────────────────────────────────────────────────────────
TEST_F(WindowsExtrasTest, TriangLength) { EXPECT_EQ(eval("triang(7)").numel(), 7u); }

TEST_F(WindowsExtrasTest, TriangSymmetric) { expectSymmetric(*this, "triang(8)", 8); }
TEST_F(WindowsExtrasTest, TriangSymmetricOdd) { expectSymmetric(*this, "triang(9)", 9); }

TEST_F(WindowsExtrasTest, TriangCentrePeak)
{
    eval("w = triang(9);");
    EXPECT_NEAR(evalScalar("w(5)"), 1.0, 1e-12);
}

TEST_F(WindowsExtrasTest, TriangEndpointsNonZero)
{
    eval("w = triang(8);");          // even N
    EXPECT_GT(evalScalar("w(1)"), 0.0);
    EXPECT_GT(evalScalar("w(8)"), 0.0);
}

// ── tukeywin ──────────────────────────────────────────────────────────
TEST_F(WindowsExtrasTest, TukeyR0IsRectangular)
{
    eval("w = tukeywin(8, 0);");
    for (int i = 1; i <= 8; ++i)
        EXPECT_NEAR(evalScalar("w(" + std::to_string(i) + ")"), 1.0, 1e-12);
}

TEST_F(WindowsExtrasTest, TukeyR1MatchesHann)
{
    eval("a = tukeywin(16, 1); b = hann(16);");
    for (int i = 1; i <= 16; ++i) {
        const std::string ai = "a(" + std::to_string(i) + ")";
        const std::string bi = "b(" + std::to_string(i) + ")";
        EXPECT_NEAR(evalScalar(ai), evalScalar(bi), 1e-12);
    }
}

TEST_F(WindowsExtrasTest, TukeyDefaultSymmetric) { expectSymmetric(*this, "tukeywin(16)", 16); }

// ── flattopwin ────────────────────────────────────────────────────────
TEST_F(WindowsExtrasTest, FlattopSymmetric) { expectSymmetric(*this, "flattopwin(16)", 16); }

TEST_F(WindowsExtrasTest, FlattopCentreNearOne)
{
    // The 5-term sum at the centre is ≈ 1.0 (peak normalisation).
    eval("w = flattopwin(33);");
    EXPECT_NEAR(evalScalar("w(17)"), 1.0, 1e-6);
}

// ── gausswin ──────────────────────────────────────────────────────────
TEST_F(WindowsExtrasTest, GausswinPeakAtCentre)
{
    eval("w = gausswin(9);");
    EXPECT_NEAR(evalScalar("w(5)"), 1.0, 1e-12);
}

TEST_F(WindowsExtrasTest, GausswinSymmetric) { expectSymmetric(*this, "gausswin(16)", 16); }

TEST_F(WindowsExtrasTest, GausswinAlphaIncreasesNarrowness)
{
    eval("w1 = gausswin(33, 1.0); w2 = gausswin(33, 5.0);");
    // Endpoint of larger-α window should be much smaller.
    EXPECT_LT(evalScalar("w2(1)"), evalScalar("w1(1)"));
}

// ── chebwin ───────────────────────────────────────────────────────────
TEST_F(WindowsExtrasTest, ChebwinPeakIsOne)
{
    eval("w = chebwin(15, 60);");
    EXPECT_NEAR(evalScalar("max(abs(w))"), 1.0, 1e-9);
}

TEST_F(WindowsExtrasTest, ChebwinSymmetricOdd) { expectSymmetric(*this, "chebwin(15, 80)", 15, 1e-9); }
TEST_F(WindowsExtrasTest, ChebwinSymmetricEven) { expectSymmetric(*this, "chebwin(16, 80)", 16, 1e-9); }

// Bug fix 2026-05-08 — previous FFT-based chebwin returned all-ones for
// even N and a wrongly-shifted shape for odd N. Cover both branches.
TEST_F(WindowsExtrasTest, ChebwinEvenN8At100dB)
{
    eval("w = chebwin(8, 100);");
    EXPECT_NEAR(evalScalar("w(1)"), 0.0363836809033449, 1e-12);
    EXPECT_NEAR(evalScalar("w(2)"), 0.2253550760453451, 1e-12);
    EXPECT_NEAR(evalScalar("w(3)"), 0.6241595403271379, 1e-12);
    EXPECT_NEAR(evalScalar("w(4)"), 1.0,                1e-12);
    // Symmetric -> w(5..8) mirror w(4..1).
    EXPECT_NEAR(evalScalar("w(5)"), 1.0,                1e-12);
    EXPECT_NEAR(evalScalar("w(8)"), 0.0363836809033449, 1e-12);
}

TEST_F(WindowsExtrasTest, ChebwinOddN7At100dB)
{
    eval("w = chebwin(7, 100);");
    EXPECT_NEAR(evalScalar("w(1)"), 0.0565040506285030, 1e-12);
    EXPECT_NEAR(evalScalar("w(2)"), 0.3166085306484744, 1e-12);
    EXPECT_NEAR(evalScalar("w(3)"), 0.7601208123539079, 1e-12);
    EXPECT_NEAR(evalScalar("w(4)"), 1.0,                1e-12);
    EXPECT_NEAR(evalScalar("w(7)"), 0.0565040506285030, 1e-12);
}

TEST_F(WindowsExtrasTest, ChebwinLowerR)
{
    // R=30 -> wider mainlobe, larger endpoints.
    eval("w = chebwin(8, 30);");
    EXPECT_NEAR(evalScalar("w(1)"), 0.2622164911915367, 1e-12);
    EXPECT_NEAR(evalScalar("w(3)"), 0.8119600672627040, 1e-12);
}

TEST_F(WindowsExtrasTest, ChebwinSinglePoint)
{
    eval("w = chebwin(1, 100);");
    EXPECT_DOUBLE_EQ(evalScalar("w(1)"), 1.0);
}

// ── parzenwin ─────────────────────────────────────────────────────────
TEST_F(WindowsExtrasTest, ParzenSymmetric) { expectSymmetric(*this, "parzenwin(17)", 17); }

TEST_F(WindowsExtrasTest, ParzenPeakAtCentre)
{
    eval("w = parzenwin(17);");
    EXPECT_NEAR(evalScalar("w(9)"), 1.0, 1e-12);
}

// ── nuttallwin ────────────────────────────────────────────────────────
TEST_F(WindowsExtrasTest, NuttallSymmetric) { expectSymmetric(*this, "nuttallwin(16)", 16); }

TEST_F(WindowsExtrasTest, NuttallEndpointSmall)
{
    eval("w = nuttallwin(64);");
    EXPECT_LT(evalScalar("w(1)"), 1e-3);
}

// ── taylorwin ─────────────────────────────────────────────────────────
TEST_F(WindowsExtrasTest, TaylorSymmetric) { expectSymmetric(*this, "taylorwin(32, 4, -30)", 32, 1e-9); }

TEST_F(WindowsExtrasTest, TaylorPeakIsOne)
{
    eval("w = taylorwin(31, 4, -30);");
    EXPECT_NEAR(evalScalar("max(w)"), 1.0, 1e-9);
}

// ── blackmanharris ────────────────────────────────────────────────────
TEST_F(WindowsExtrasTest, BlackmanharrisSymmetric) { expectSymmetric(*this, "blackmanharris(16)", 16); }

TEST_F(WindowsExtrasTest, BlackmanharrisEndpointTiny)
{
    eval("w = blackmanharris(64);");
    // 4-term Blackman-Harris endpoints are ~6e-5.
    EXPECT_LT(evalScalar("w(1)"), 1e-3);
}

// ── bohmanwin ─────────────────────────────────────────────────────────
TEST_F(WindowsExtrasTest, BohmanSymmetric) { expectSymmetric(*this, "bohmanwin(15)", 15); }

TEST_F(WindowsExtrasTest, BohmanEndpointsZero)
{
    eval("w = bohmanwin(15);");
    EXPECT_NEAR(evalScalar("w(1)"), 0.0, 1e-12);
    EXPECT_NEAR(evalScalar("w(15)"), 0.0, 1e-12);
}

// ── barthannwin ───────────────────────────────────────────────────────
TEST_F(WindowsExtrasTest, BarthannSymmetric) { expectSymmetric(*this, "barthannwin(16)", 16); }

TEST_F(WindowsExtrasTest, BarthannPeakAtCentre)
{
    eval("w = barthannwin(33);");
    // Peak at the centre is 1.0 by construction.
    EXPECT_NEAR(evalScalar("w(17)"), 1.0, 1e-12);
}
