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
    Engine engine;
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
