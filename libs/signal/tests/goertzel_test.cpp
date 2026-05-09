// libs/signal/tests/goertzel_test.cpp
//
// Audit ТЗ closure for signal/goertzel.
// Closes audit/findings/signal/goertzel.md.
//
// Pre-fix: numkit's adapter required (x, ind) and threw on the 1-arg
// form `goertzel(x)`. Per MATLAB R2025b, 1-arg form defaults
// `ind = 1:N`, computing the full DFT via Goertzel. Adapter now
// builds the default ind vector on the per-call ScratchArena and
// dispatches into the same kernel.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class GoertzelTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override {
        engine.eval("import compat.*;");
        engine.eval("sig = sin(2*pi*0.1*(0:31)') .* exp(-0.05*(0:31)');");
    }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// ─── partial-bin form (existing behaviour) ──────────────────────────

TEST_F(GoertzelTest, PartialBinsMatchMatlab)
{
    eval("y = goertzel(sig, [5 15]);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(y)"), 2.0);
    EXPECT_NEAR(evalScalar("real(y(1))"), -2.70565,    1e-5);
    EXPECT_NEAR(evalScalar("imag(y(1))"), -0.240838,   1e-6);
    EXPECT_NEAR(evalScalar("real(y(2))"), -0.25769,    1e-5);
    EXPECT_NEAR(evalScalar("imag(y(2))"),  0.0194293,  1e-7);
}

// ─── 1-arg form: defaults ind = 1:N, computes full DFT ──────────────

TEST_F(GoertzelTest, OneArgFormDoesFullDFT)
{
    eval("yfull = goertzel(sig);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(yfull)"), 32.0);
    // y(1) is the DC bin: sum(sig). Imag part should be ~0.
    EXPECT_NEAR(evalScalar("real(yfull(1))"),  1.31246, 1e-5);
    EXPECT_NEAR(evalScalar("imag(yfull(1))"),  0.0,     1e-12);
}

TEST_F(GoertzelTest, OneArgFormMatchesPartialBins)
{
    // The full-DFT result should agree element-wise with goertzel
    // called explicitly on the same indices.
    eval("yfull = goertzel(sig);");
    eval("y5    = goertzel(sig, 5);");
    EXPECT_NEAR(evalScalar("real(yfull(5)) - real(y5)"), 0.0, 1e-12);
    EXPECT_NEAR(evalScalar("imag(yfull(5)) - imag(y5)"), 0.0, 1e-12);
}

// NOTE: a tempting cross-check `goertzel(x)` ≡ `fft(x)` was tried here
// and removed: numkit's compat-aliased `fft` differs from MATLAB by a
// per-bin imag sign on the same input where `goertzel` is bit-correct.
// That's a separate gap in the FFT path — out of scope for this ТЗ.

// ─── empty 2nd arg falls through to default ─────────────────────────

TEST_F(GoertzelTest, EmptySecondArgIsTreatedAsDefault)
{
    eval("yE = goertzel(sig, []);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(yE)"), 32.0);
    eval("yfull = goertzel(sig);");
    EXPECT_LT(evalScalar("max(abs(yE - yfull))"), 1e-12);
}

// ─── error path: no args ────────────────────────────────────────────

TEST_F(GoertzelTest, NoArgsThrows)
{
    bool threw = false;
    try { eval("goertzel();"); }
    catch (const std::exception &) { threw = true; }
    EXPECT_TRUE(threw);
}
