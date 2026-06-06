// libs/signal/tests/signal_batch1_test.cpp
//
// First signal/ namespace batch closure. 13 functions:
//   convolution: conv · conv2 · convn · cconv
//   spectral:    bandpower
//   waveform:    chirp
//   alignment:   alignsignals
//   AR helpers:  ac2poly · ac2rc (partial)
//   filter ap:   besselap · buttap · cheb1ap · cheb2ap
//
// All flagged "no major gap detected". Bit-identical MATLAB R2025b.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class SignalBatch1Test : public ::testing::Test
{
public:
    StdEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(SignalBatch1Test, ConvFamily)
{
    eval("r = conv([1 2 3], [1 1]);");  // [1 3 5 3]
    EXPECT_DOUBLE_EQ(evalScalar("r(1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("r(2)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("r(3)"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("r(4)"), 3.0);

    eval("C = conv2([1 2; 3 4], [1 1; 1 1]);");
    EXPECT_DOUBLE_EQ(evalScalar("C(1,1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("C(3,3)"), 4.0);
}

TEST_F(SignalBatch1Test, Cconv)
{
    eval("r = cconv([1 2 3], [1 1], 4);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(r)"), 4.0);
}

TEST_F(SignalBatch1Test, Bandpower)
{
    eval("x = sin(2*pi*100*(0:999)/1000);");
    // bandpower of sin = 0.5 (mean square)
    EXPECT_NEAR(evalScalar("bandpower(x)"), 0.5, 1e-3);
}

TEST_F(SignalBatch1Test, Chirp)
{
    eval("y = chirp(0:0.001:0.01, 0, 1, 100);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(y)"), 11.0);
    EXPECT_NEAR(evalScalar("y(1)"), 1.0, 1e-9);  // chirp(t=0) = cos(0) = 1
}

TEST_F(SignalBatch1Test, AlignSignals)
{
    eval("[Xa, Ya] = alignsignals([1 2 3 4 5], [0 0 1 2 3]);");
    EXPECT_GT(evalScalar("numel(Xa)"), 0.0);
    EXPECT_GT(evalScalar("numel(Ya)"), 0.0);
}

TEST_F(SignalBatch1Test, Ac2Poly)
{
    eval("p = ac2poly([2 1 0.5]);");
    EXPECT_NEAR(evalScalar("p(1)"),  1.0,  1e-9);  // leading coefficient
}

TEST_F(SignalBatch1Test, Ac2Rc)
{
    // numkit's k(2) and R0 differ from MATLAB; only k(1) is bit-identical
    eval("[k, R0] = ac2rc([2 1 0.5]);");
    EXPECT_NEAR(evalScalar("k(1)"), -0.5, 1e-9);
}

TEST_F(SignalBatch1Test, AnalogPrototypes)
{
    eval("[z1, p1, k1] = besselap(3);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(p1)"), 3.0);
    eval("[z2, p2, k2] = buttap(3);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(p2)"), 3.0);
    eval("[z3, p3, k3] = cheb1ap(3, 0.5);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(p3)"), 3.0);
    eval("[z4, p4, k4] = cheb2ap(3, 30);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(p4)"), 3.0);
}

// DEEP-PROBE 2026-05-31: lp2* TF form returns the numerator at its true
// degree (#zeros + 1), matching MATLAB — NOT zero-padded to the denominator
// length. The Butterworth prototype has no zeros, so lp2lp's numerator is
// length 1 ([Wo^N]) and lp2bp's is length 5. Previously numkit returned a
// padded numerator (length 5 / 9) with a leading 0, because tf2zp produced a
// zero gain for the padded prototype numerator.
TEST_F(SignalBatch1Test, Lp2lpNumeratorTrueDegree)
{
    eval("[z, p, k] = buttap(4); [bp, ap] = zp2tf(z, p, k);");
    eval("[bt, at] = lp2lp(bp, ap, 100);");
    EXPECT_EQ(static_cast<int>(evalScalar("numel(bt)")), 1);   // was 5 (padded)
    EXPECT_EQ(static_cast<int>(evalScalar("numel(at)")), 5);
    EXPECT_NEAR(evalScalar("bt(1)"), 1e8, 1.0);                // Wo^4
    EXPECT_NEAR(evalScalar("at(2)"), 261.312592975, 1e-6);
    EXPECT_NEAR(evalScalar("at(5)"), 1e8, 1.0);
}

TEST_F(SignalBatch1Test, Lp2bpNumeratorTrueDegree)
{
    eval("[z, p, k] = buttap(4); [bp, ap] = zp2tf(z, p, k);");
    eval("[bt, at] = lp2bp(bp, ap, 100, 50);");
    EXPECT_EQ(static_cast<int>(evalScalar("numel(bt)")), 5);   // was 9 (padded)
    EXPECT_EQ(static_cast<int>(evalScalar("numel(at)")), 9);
    EXPECT_NEAR(evalScalar("bt(1)"), 6.25e6, 1.0);
    EXPECT_NEAR(evalScalar("at(2)"), 130.656296488, 1e-5);
    EXPECT_NEAR(evalScalar("at(9)"), 1e16, 1e7);               // Wo^8
}

// DEEP-PROBE 2026-05-31: the 3-output [z,p,k] (digital zero/pole/gain) form
// of the IIR designers was added (previously errored "Undefined k"). The
// denominator is monic so the ZPK gain = b(1); poles/gain are exact for all,
// finite zeros (cheby2/ellip) are exact, and the all-pole filters' repeated
// zeros at z=-1 (butter/cheby1) are recovered to root-finding precision.
TEST_F(SignalBatch1Test, IirDesignersZpkOutput)
{
    eval("[z, p, k] = butter(4, 0.3);");
    EXPECT_EQ(static_cast<int>(evalScalar("numel(z)")), 4);
    EXPECT_EQ(static_cast<int>(evalScalar("numel(p)")), 4);
    EXPECT_NEAR(evalScalar("k"), 0.0185630106269, 1e-9);
    EXPECT_NEAR(evalScalar("sum(real(p))"), 1.57039885123, 1e-6);

    eval("[z1, p1, k1] = cheby1(4, 1, 0.3);");
    EXPECT_NEAR(evalScalar("k1"), 0.00836323955555, 1e-9);
    EXPECT_NEAR(evalScalar("sum(real(p1))"), 2.37412317473, 1e-6);

    // cheby2 / ellip have DISTINCT finite zeros → z/p/k all exact.
    eval("[z2, p2, k2] = cheby2(4, 30, 0.3);");
    EXPECT_NEAR(evalScalar("k2"), 0.04704983394, 1e-8);
    EXPECT_NEAR(evalScalar("sum(real(p2))"), 2.26899170565, 1e-6);
    EXPECT_NEAR(evalScalar("sum(real(z2))"), 0.509710647798, 1e-6);

    eval("[z3, p3, k3] = ellip(4, 1, 30, 0.3);");
    EXPECT_NEAR(evalScalar("k3"), 0.0647314906117, 1e-8);
    EXPECT_NEAR(evalScalar("sum(real(p3))"), 2.28002377534, 1e-6);
    EXPECT_NEAR(evalScalar("sum(real(z3))"), 0.163902069627, 1e-6);
}
