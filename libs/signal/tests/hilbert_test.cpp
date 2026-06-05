// libs/signal/tests/hilbert_test.cpp
// — sign convention fix.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class HilbertTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// MATLAB R2025b reference: hilbert([1:8])
//   real: [1 2 3 4 5 6 7 8]
//   imag: [3.8284 -1 -1 -1.8284 -1.8284 -1 -1 3.8284]

TEST_F(HilbertTest, RealPartEqualsInput)
{
    eval("h = hilbert([1:8]'); r = real(h);");
    for (int k = 1; k <= 8; ++k) {
        EXPECT_DOUBLE_EQ(evalScalar("r(" + std::to_string(k) + ")"),
                         static_cast<double>(k));
    }
}

TEST_F(HilbertTest, ImagPartMatchesMATLABSign)
{
    eval("h = hilbert([1:8]'); im = imag(h);");
    EXPECT_NEAR(evalScalar("im(1)"),  3.8284271247, 1e-9);
    EXPECT_NEAR(evalScalar("im(2)"), -1.0, 1e-12);
    EXPECT_NEAR(evalScalar("im(3)"), -1.0, 1e-12);
    EXPECT_NEAR(evalScalar("im(4)"), -1.8284271247, 1e-9);
    EXPECT_NEAR(evalScalar("im(5)"), -1.8284271247, 1e-9);
    EXPECT_NEAR(evalScalar("im(6)"), -1.0, 1e-12);
    EXPECT_NEAR(evalScalar("im(7)"), -1.0, 1e-12);
    EXPECT_NEAR(evalScalar("im(8)"),  3.8284271247, 1e-9);
}

TEST_F(HilbertTest, EnvelopeMagnitudePositive)
{
    // envelope(x) = |hilbert(x)| — sign-flip-invariant; should still
    // produce positive values.
    eval("e = envelope([1:8]');");
    for (int k = 1; k <= 8; ++k) {
        EXPECT_GT(evalScalar("e(" + std::to_string(k) + ")"), 0.0);
    }
}
