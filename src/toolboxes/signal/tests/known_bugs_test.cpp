// toolboxes/signal/tests/known_bugs_test.cpp
//
// One DISABLED_ test per OPEN bug catalogued in bugs/signal/*.md. These do
// NOT run in the normal suite (so they never break the green baseline), but
// they are visible ("YOU HAVE N DISABLED TESTS") and become live regression
// guards the moment the bug is fixed — just remove the `DISABLED_` prefix.
// Each test asserts the MATLAB R2025b-correct behaviour.
//
// Run: numkit_gtest.exe --gtest_also_run_disabled_tests --gtest_filter='SignalKnownBug*'

#include <numkit/core/engine.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class SignalKnownBug : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// bugs/signal/conv-integer-input.md — conv accepts integer/logical input
// (promotes to double; result is always double). FIXED 2026-06-05; deep
// coverage in toolboxes/signal/tests/conv_integer_input_test.cpp.
TEST_F(SignalKnownBug, ConvIntegerInput)
{
    eval("c = conv(int8([1 2 3]), int8([1 1]));");
    EXPECT_TRUE(eval("isa(c, 'double')").toBool());
    EXPECT_DOUBLE_EQ(evalScalar("c(2)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("c(3)"), 5.0);
    // mixed int+double and logical also promote to double.
    EXPECT_TRUE(eval("isa(conv(int8([1 2 3]), [1 1]), 'double')").toBool());
    EXPECT_TRUE(eval("isa(conv(logical([1 0 1]), [1 1]), 'double')").toBool());
}

// bugs/signal/deconv-integer-input.md — deconv accepts integer/logical input
// (promotes to double; quotient and remainder always double). FIXED
// 2026-06-05; deep coverage in toolboxes/signal/tests/deconv_integer_input_test.cpp.
TEST_F(SignalKnownBug, DeconvIntegerInput)
{
    eval("q = deconv(int8([1 3 5 3]), int8([1 1]));");
    EXPECT_TRUE(eval("isa(q, 'double')").toBool());
    EXPECT_DOUBLE_EQ(evalScalar("q(2)"), 2.0);
    eval("[q2, r2] = deconv(int8([1 3 5 3]), int8([1 1]));");
    EXPECT_TRUE(eval("isa(r2, 'double')").toBool());
    EXPECT_DOUBLE_EQ(evalScalar("q2(1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("max(abs(r2))"), 0.0);
    EXPECT_TRUE(eval("isa(deconv([1 3 5 3], int8([1 1])), 'double')").toBool());
}

// bugs/signal/instfreq-instbw.md — instfreq must track a 10->40 Hz chirp.
TEST_F(SignalKnownBug, DISABLED_InstfreqTracksChirp)
{
    eval("fs=1000; t=(0:1/fs:1-1/fs)'; x=chirp(t,10,1,40); ifr=instfreq(x,fs);");
    EXPECT_NEAR(evalScalar("ifr(1)"),   13.96, 0.5);
    EXPECT_NEAR(evalScalar("ifr(end)"), 38.46, 0.5);
}

// NOTE: dct/idct Type 1/3/4 FIXED — live tests in
// toolboxes/signal/tests/dct_types_test.cpp.

// bugs/signal/cceps-nd-phase.md — non-2^n phase + 2nd output nd (FIXED; live guard).
TEST_F(SignalKnownBug, CcepsPhaseAndNd)
{
    eval("[xh, nd] = cceps([1 2 3 4 3 2 1]);");
    EXPECT_NEAR(evalScalar("xh(2)"), 0.523560, 1e-5);
    EXPECT_NEAR(evalScalar("xh(7)"), 1.222516, 1e-5);
    EXPECT_DOUBLE_EQ(evalScalar("nd"), -3.0);
}

// bugs/signal/risetime-falltime-outputs.md — [R,LT,UT] multi-output.
TEST_F(SignalKnownBug, RisetimeMultiOutput)
{
    eval("[R, LT, UT] = risetime([0 0 0 1 1 1 1], 4);");
    EXPECT_NEAR(evalScalar("R"),     0.1980, 1e-3);
    EXPECT_NEAR(evalScalar("LT(1)"), 0.5260, 1e-3);
    EXPECT_NEAR(evalScalar("UT(1)"), 0.7240, 1e-3);
}

// bugs/signal/findpeaks-widthreference.md — 'halfheight' on a pedestal.
TEST_F(SignalKnownBug, DISABLED_FindpeaksHalfHeightWidth)
{
    eval("[p,l,w] = findpeaks([5 5 5 6 9 6 5 5 5], 'WidthReference', 'halfheight');");
    EXPECT_NEAR(evalScalar("w(1)"), 6.0, 1e-6);
}

// bugs/signal/fillgaps.md — AR interpolation of a NaN gap.
TEST_F(SignalKnownBug, DISABLED_Fillgaps)
{
    eval("y = fillgaps([1 2 NaN 4 5]);");
    EXPECT_NEAR(evalScalar("y(3)"), 3.0, 1e-6);
}

// bugs/signal/pmusic-peig.md — pseudospectrum estimators exist + return a
// positive-power column. (Verify exact values vs MATLAB when enabling.)
TEST_F(SignalKnownBug, PmusicExists)
{
    eval("[p,f] = pmusic([1 2 1 3 2 4 1 2 1 3], 4);");
    EXPECT_GT(evalScalar("numel(p)"), 0.0);
    EXPECT_GE(evalScalar("min(p)"), 0.0);
}

// bugs/signal/ellipord-bandstop.md — bandstop order estimate. FIXED
// 2026-06-05 (deep coverage in toolboxes/signal/tests/ellipord_test.cpp).
TEST_F(SignalKnownBug, EllipordBandstop)
{
    eval("[n, Wn] = ellipord([0.1 0.6], [0.2 0.5], 3, 40);");
    EXPECT_EQ(static_cast<int>(evalScalar("n")), 4);          // MATLAB n=4
    EXPECT_EQ(static_cast<int>(evalScalar("numel(Wn)")), 2);
    EXPECT_NEAR(evalScalar("Wn(1)"), 0.1, 1e-12);
    EXPECT_NEAR(evalScalar("Wn(2)"), 0.6, 1e-12);
}

// bugs/signal/impinvar-repeated-poles.md — repeated-pole numerator. FIXED
// 2026-06-05 (deep coverage in toolboxes/signal/tests/impinvar_test.cpp).
TEST_F(SignalKnownBug, ImpinvarRepeatedPoles)
{
    eval("[bz, az] = impinvar(1, [1 2 1], 10);");   // double pole at -1
    EXPECT_NEAR(evalScalar("bz(1)"), 0.0,         1e-9);
    EXPECT_NEAR(evalScalar("bz(2)"), 0.00904837,  1e-7);
}

// bugs/signal/stmcb.md — Steiglitz-McBride IIR identification missing.
TEST_F(SignalKnownBug, Stmcb)
{
    eval("[b, a] = stmcb([1 0.5 0.25 0.125 0.0625], 1, 1);");
    EXPECT_NEAR(evalScalar("a(2)"), -0.5, 1e-4);
}

// bugs/signal/freqs-scalar-w.md — scalar w means N points (MATLAB), not a freq.
TEST_F(SignalKnownBug, DISABLED_FreqsScalarIsNPoints)
{
    eval("h = freqs([1 0], [1 1 1], 2);");
    EXPECT_EQ(static_cast<int>(evalScalar("numel(h)")), 2);  // numkit currently 1
}

// bugs/signal/resample-values.md — resample output values (FIXED, promoted live).
TEST_F(SignalKnownBug, ResampleValues)
{
    eval("y = resample([1 2 3 4 5 6], 3, 2);");
    EXPECT_EQ(static_cast<int>(evalScalar("numel(y)")), 9);     // ceil(6*3/2)
    EXPECT_NEAR(evalScalar("y(1)"),   1.0006061736, 1e-9);
    EXPECT_NEAR(evalScalar("y(5)"),   3.9409926893, 1e-9);
    EXPECT_NEAR(evalScalar("y(9)"),   4.2402907078, 1e-9);
    EXPECT_NEAR(evalScalar("sum(y)"), 31.6965, 1e-3);
}

// bugs/signal/obw-value-outputs.md — value + [bw,flo,fhi,power] (FIXED).
TEST_F(SignalKnownBug, ObwValueAndOutputs)
{
    eval("fs=1000; t=(0:fs-1)/fs; x=sin(2*pi*100*t)+0.5*sin(2*pi*200*t);");
    EXPECT_NEAR(evalScalar("obw(x,fs)"), 100.96875, 1e-4);   // was numkit ~108.77
    eval("[bw,flo,fhi,p]=obw(x,fs);");                        // 4 outputs, was a throw
    EXPECT_NEAR(evalScalar("bw"),  100.96875,  1e-4);
    EXPECT_NEAR(evalScalar("flo"),  99.506250, 1e-4);
    EXPECT_NEAR(evalScalar("fhi"), 200.475000, 1e-4);
    EXPECT_NEAR(evalScalar("p"),     0.618750, 1e-6);
}

// bugs/signal/periodogram-pxxc.md — confidence-interval 3rd output (FIXED).
TEST_F(SignalKnownBug, PeriodogramPxxc)
{
    eval("[pxx,f,pxxc]=periodogram([1 2 3 4 5 6 7 8],[],[],1,'ConfidenceLevel',0.95);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(pxxc,2)")), 2);
    EXPECT_EQ(static_cast<int>(evalScalar("size(pxxc,1)")),
              static_cast<int>(evalScalar("numel(pxx)")));
    // DC bin (real, 1 DOF): lower-bound ratio = 1/chi2inv(0.975,1) = 0.19905;
    // interior bins (2 DOF): 2/chi2inv(0.975,2) = 0.27108. Default nfft=256 is
    // even, so the Nyquist bin (pxx(end)) is also 1 DOF.
    EXPECT_NEAR(evalScalar("pxxc(1,1)/pxx(1)"),     0.1990490952, 1e-9);
    EXPECT_NEAR(evalScalar("pxxc(2,1)/pxx(2)"),     0.2710850307, 1e-9);
    EXPECT_NEAR(evalScalar("pxxc(end,1)/pxx(end)"), 0.1990490952, 1e-9);
}

// bugs/signal/periodogram-nonpow2-nfft.md — non-pow2 nfft via Bluestein (FIXED).
TEST_F(SignalKnownBug, PeriodogramNonPow2Nfft)
{
    eval("fs=1000; t=(0:fs-1)/fs; x=sin(2*pi*100*t)+0.5*sin(2*pi*200*t);");
    eval("[P,F]=periodogram(x,[],1000,fs);");   // nfft=1000 (non-power-of-two)
    EXPECT_EQ(eval("P").numel(), 501u);
    eval("[~,ix]=max(P);");
    EXPECT_NEAR(evalScalar("F(ix)"), 100.0,  1e-9);   // peak at the 100 Hz tone (was ~256)
    EXPECT_NEAR(evalScalar("P(101)"), 0.5,   1e-9);   // f=100
    EXPECT_NEAR(evalScalar("P(201)"), 0.125, 1e-9);   // f=200
    // Parseval: sum(P)*df = mean(x^2) = 0.625 (was ~21.5).
    EXPECT_NEAR(evalScalar("sum(P)*(F(2)-F(1))"), 0.625, 1e-9);
}

// bugs/builtin/complex-input-unsupported.md — conv/filter on complex input.
TEST_F(SignalKnownBug, ConvFilterComplex)
{
    eval("y = conv([1 1i],[1 1]);");             // MATLAB: [1, 1+1i, 1i]
    EXPECT_NEAR(evalScalar("imag(y(2))"), 1.0, 1e-12);
    eval("z = filter([1 1],1,[1i 1i]);");        // MATLAB: [1i, 2i]
    EXPECT_NEAR(evalScalar("imag(z(2))"), 2.0, 1e-12);
}

// bugs/signal/spectrogram-fc-tc.md — 5th/6th outputs fc, tc.
TEST_F(SignalKnownBug, DISABLED_SpectrogramFcTc)
{
    eval("x = sin(2*pi*0.1*(0:99));");
    eval("[s,f,t,ps,fc,tc] = spectrogram(x,16,8,16,1);");   // MATLAB tc(1)=8
    EXPECT_EQ(static_cast<int>(evalScalar("numel(fc)")),
              static_cast<int>(evalScalar("numel(t)")));
    EXPECT_NEAR(evalScalar("tc(1)"), 8.0, 1e-6);
}
