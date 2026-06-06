// libs/signal/tests/signal_waveforms_batch_test.cpp
//
// Signal waveform-generation batch (8 functions):
//   square · sawtooth · rectpuls · sinc · diric ·
//   pulstran · tripuls · gauspuls
//
// All flagged "no major gap detected". Bit-identical MATLAB R2025b.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class SignalWaveformsBatchTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(SignalWaveformsBatchTest, SquareSawtooth)
{
    eval("y = square(2*pi*0.1*(0:9));");
    EXPECT_DOUBLE_EQ(evalScalar("numel(y)"), 10.0);

    eval("y = sawtooth(2*pi*0.1*(0:9));");
    EXPECT_DOUBLE_EQ(evalScalar("numel(y)"), 10.0);
}

TEST_F(SignalWaveformsBatchTest, RectPulsTriPuls)
{
    eval("y = rectpuls(-1:0.5:1, 1);");
    // rectpuls(0) = 1 (centre); rectpuls(±1) = 0 outside support
    EXPECT_DOUBLE_EQ(evalScalar("y(3)"), 1.0);

    eval("y = tripuls(-1:0.5:1, 1);");
    EXPECT_DOUBLE_EQ(evalScalar("y(3)"), 1.0);  // peak at 0
}

TEST_F(SignalWaveformsBatchTest, Sinc)
{
    EXPECT_DOUBLE_EQ(evalScalar("sinc(0)"), 1.0);  // sinc(0) = 1
    EXPECT_NEAR(evalScalar("sinc(1)"),     0.0, 1e-12);  // sinc(integer) = 0
    EXPECT_NEAR(evalScalar("sinc(-1)"),    0.0, 1e-12);
}

TEST_F(SignalWaveformsBatchTest, Diric)
{
    EXPECT_DOUBLE_EQ(evalScalar("diric(0, 5)"), 1.0);  // Dirichlet at 0 = 1
}

TEST_F(SignalWaveformsBatchTest, PulstranGauspuls)
{
    eval("y = pulstran(0:0.01:0.1, 0.05, 'rectpuls', 0.02);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(y)"), 11.0);

    eval("y = gauspuls(0:0.001:0.01, 1000);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(y)"), 11.0);
}
