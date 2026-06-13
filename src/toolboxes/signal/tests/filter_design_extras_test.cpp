// toolboxes/signal/tests/filter_design_extras_test.cpp
//
// Coverage for parity-only filter design / order / transform helpers:
//   cheb1ord cheb2ord ellipap            (order estimation + analog prototype)
//   lp2hp lp2bs                          (frequency transforms)
//   filternorm filtord firtype           (filter properties)
//   sosfiltfilt                          (zero-phase SOS filtering)
// Values verified against the engine; orders/types are exact by construction.

#include "dual_engine_fixture.hpp"

using namespace m_test;

class FilterDesignExtrasTest : public DualEngineTest
{};

TEST_P(FilterDesignExtrasTest, OrderEstimation)
{
    eval("[n1, Wn1] = cheb1ord(0.2, 0.3, 1, 40);");
    EXPECT_DOUBLE_EQ(evalScalar("n1"), 6.0);
    EXPECT_NEAR(evalScalar("Wn1"), 0.2, 1e-9);     // Chebyshev-I edge = passband edge
    eval("[n2, Wn2] = cheb2ord(0.2, 0.3, 1, 40);");
    EXPECT_DOUBLE_EQ(evalScalar("n2"), 6.0);
    EXPECT_NEAR(evalScalar("Wn2"), 0.3, 1e-9);     // Chebyshev-II edge = stopband edge
}

TEST_P(FilterDesignExtrasTest, Ellipap)
{
    eval("[z, p, k] = ellipap(3, 1, 40);");        // 3rd-order elliptic prototype
    EXPECT_EQ(eval("z").numel(), 2u);              // floor(n/2)*2 finite zeros
    EXPECT_EQ(eval("p").numel(), 3u);
    EXPECT_NEAR(evalScalar("k"), 0.069201, 1e-5);
}

TEST_P(FilterDesignExtrasTest, FreqTransforms)
{
    // lp2hp of a 2nd-order lowpass with Wo=2: denom -> [1, 1.4142*2, 2^2].
    eval("[bh, ah] = lp2hp(1, [1 1.4142 1], 2);");
    EXPECT_NEAR(evalScalar("ah(2)"), 2.8284, 1e-3);
    EXPECT_NEAR(evalScalar("ah(3)"), 4.0, 1e-3);
    // lp2bs doubles the order (2 -> 4, i.e. length 5).
    eval("[bb, ab] = lp2bs(1, [1 1.4142 1], 10, 2);");
    EXPECT_EQ(eval("ab").numel(), 5u);
}

TEST_P(FilterDesignExtrasTest, NormOrderType)
{
    EXPECT_NEAR(evalScalar("filternorm([1 1], [1 0.5])"), 1.154771, 1e-5);   // L2 norm
    EXPECT_DOUBLE_EQ(evalScalar("filtord([1 2 1], [1 0.5 0.2])"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("firtype([1 2 3 2 1])"), 1.0);   // symmetric, odd length
    EXPECT_DOUBLE_EQ(evalScalar("firtype([1 0 -1])"), 3.0);      // antisymmetric, odd length
}

TEST_P(FilterDesignExtrasTest, Sosfiltfilt)
{
    eval("Fs = 1000; t = (0:999)'/Fs; x = sin(2*pi*50*t);");
    eval("sos = [1 2 1 1 -0.5 0.06; 1 0 -1 1 0.2 0.1];");
    eval("y = sosfiltfilt(sos, x);");
    EXPECT_EQ(eval("y").numel(), 1000u);            // zero-phase: length preserved
    EXPECT_DOUBLE_EQ(evalScalar("all(isfinite(y))"), 1.0);
}

INSTANTIATE_DUAL(FilterDesignExtrasTest);
