// toolboxes/signal/tests/rceps_test.cpp
//
// DEEP-PROBE 2026-06: rceps was (1) transforming on a nextPow2-padded
// buffer, so log|X| blew up at the padded near-zero bins and non-power-of-
// two lengths returned garbage; and (2) missing its 2nd output (the
// minimum-phase reconstruction). Both fixed in
// toolboxes/signal/src/transforms/extras.cpp. Reference values: MATLAB R2025b.
// The fix also runs cceps/icceps at the exact length (no padding garbage),
// preserving the existing power-of-two parity.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <numkit/signal/transforms/extras.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class RcepsTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// Non-power-of-two length (n=7) — was garbage (~[-258, 87, ...]) before the
// exact-length fix; now bit-identical to MATLAB.
TEST_F(RcepsTest, RealCepstrumOddLength)
{
    eval("y = rceps([1 2 3 4 3 2 1]);");
    EXPECT_EQ(static_cast<int>(evalScalar("numel(y)")), 7);
    EXPECT_NEAR(evalScalar("y(1)"),  0.396084, 1e-6);
    EXPECT_NEAR(evalScalar("y(2)"),  0.873038, 1e-6);
    EXPECT_NEAR(evalScalar("y(4)"), -0.202457, 1e-6);
    EXPECT_NEAR(evalScalar("sum(y)"), 2.772588, 1e-4);
}

// 2nd output: minimum-phase reconstruction (was "Too many output args").
TEST_F(RcepsTest, MinimumPhaseSecondOutput)
{
    eval("[y, ym] = rceps([1 2 3 4 3 2 1]);");
    EXPECT_EQ(static_cast<int>(evalScalar("numel(ym)")), 7);
    EXPECT_NEAR(evalScalar("ym(1)"), 1.603952, 1e-6);
    EXPECT_NEAR(evalScalar("ym(3)"), 3.739440, 1e-6);
    EXPECT_NEAR(evalScalar("ym(7)"), 0.643953, 1e-6);
}

// Power-of-two length unchanged (regression guard for the exact-length path).
TEST_F(RcepsTest, PowerOfTwoUnchanged)
{
    eval("r = rceps((1:8)');");
    EXPECT_NEAR(evalScalar("r(1)"), 2.007521, 1e-5);
    EXPECT_EQ(static_cast<int>(evalScalar("numel(r)")), 8);
}

// Output orientation matches the input (row in → row out).
TEST_F(RcepsTest, RowOrientation)
{
    eval("yr = rceps([1 2 3 4 3 2 1]);");      // row input
    EXPECT_EQ(static_cast<int>(evalScalar("size(yr,1)")), 1);
    EXPECT_EQ(static_cast<int>(evalScalar("size(yr,2)")), 7);
}

// cceps/icceps: the exact-length change leaves the power-of-two result
// finite and unchanged (no padding blow-up), and non-power-of-two no longer
// returns a corrupted (huge-magnitude) cepstrum.
TEST_F(RcepsTest, CcepsExactLengthSane)
{
    eval("c8 = cceps((1:8)');");
    EXPECT_NEAR(evalScalar("c8(1)"), 2.007521, 1e-5);
    eval("c7 = cceps([1 2 3 4 3 2 1]);");
    EXPECT_LT(evalScalar("max(abs(c7))"), 10.0);   // was ~258 with padding
    EXPECT_NEAR(evalScalar("c7(1)"), 0.396084, 1e-6);
}

// Direct C++ API — 2-output pair.
TEST_F(RcepsTest, PublicApiPair)
{
    eval("x = [1 2 3 4 3 2 1];");
    auto [y, ym] = signal::rcepsMinPhase(*engine.getVariable("x"), engine.resource());
    ASSERT_EQ(y.numel(), 7u);
    ASSERT_EQ(ym.numel(), 7u);
    EXPECT_NEAR(y.elemAsDouble(0),  0.396084, 1e-6);
    EXPECT_NEAR(ym.elemAsDouble(0), 1.603952, 1e-6);
}
