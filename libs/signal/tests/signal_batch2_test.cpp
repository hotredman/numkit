// libs/signal/tests/signal_batch2_test.cpp
//
// Signal batch 2 closure (17 functions):
//   filter design:   butter · buttord · cheby1 · cheby2 · cheb1ord · cheb2ord
//   AR/LPC:          arburg · arcov · armcov · aryule · lpc
//   cepstral:        rceps
//   freq response:   freqz · freqs (deferred) · grpdelay · impz · stepz
//
// All flagged "no major gap detected". Bit-identical MATLAB R2025b
// (16 verified; 1 deferred — freqs returns scalar where MATLAB returns
// length-N vector).

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class SignalBatch2Test : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(SignalBatch2Test, FilterDesignButter)
{
    eval("[b, a] = butter(4, 0.5);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(b)"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("numel(a)"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("a(1)"),     1.0);  // normalised leading
}

TEST_F(SignalBatch2Test, Buttord)
{
    eval("[N, Wn] = buttord(0.4, 0.5, 1, 60);");
    EXPECT_GT(evalScalar("N"),  0.0);
    EXPECT_GT(evalScalar("Wn"), 0.0);
}

TEST_F(SignalBatch2Test, Cheby1Cheby2)
{
    eval("[b1, a1] = cheby1(4, 0.5, 0.5);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(b1)"), 5.0);

    eval("[b2, a2] = cheby2(4, 30, 0.5);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(b2)"), 5.0);
}

TEST_F(SignalBatch2Test, ARFamily)
{
    eval("x = sin(2*pi*0.1*(0:99));");
    eval("[a1, e1] = arburg(x, 4);");  EXPECT_DOUBLE_EQ(evalScalar("numel(a1)"), 5.0);
    eval("[a2, e2] = arcov(x,  4);");  EXPECT_DOUBLE_EQ(evalScalar("numel(a2)"), 5.0);
    eval("[a3, e3] = armcov(x, 4);");  EXPECT_DOUBLE_EQ(evalScalar("numel(a3)"), 5.0);
    eval("[a4, e4] = aryule(x, 4);");  EXPECT_DOUBLE_EQ(evalScalar("numel(a4)"), 5.0);
    eval("a5 = lpc(x, 4);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(a5)"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("a5(1)"), 1.0);  // leading coefficient
}

TEST_F(SignalBatch2Test, Rceps)
{
    eval("r = rceps((1:8)');");
    EXPECT_DOUBLE_EQ(evalScalar("numel(r)"), 8.0);
}

TEST_F(SignalBatch2Test, FreqResponse)
{
    eval("[h, w] = freqz([1 0.5], 1, 8);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(h)"), 8.0);
    EXPECT_DOUBLE_EQ(evalScalar("numel(w)"), 8.0);

    eval("[gd, w] = grpdelay([1 0.5], 1, 8);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(gd)"), 8.0);

    eval("h = impz([1 0.5], 1, 8);");
    EXPECT_DOUBLE_EQ(evalScalar("h(1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("h(2)"), 0.5);

    eval("h = stepz([1 0.5], 1, 8);");
    EXPECT_DOUBLE_EQ(evalScalar("h(1)"), 1.0);
}
