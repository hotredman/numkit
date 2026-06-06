// libs/signal/tests/enbw_test.cpp
// enbw.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class EnbwTest : public ::testing::Test
{
public:
    StdEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(EnbwTest, RectwinIsUnity)
{
    // Rectangular window: ENBW = N · N / N² = 1.
    EXPECT_NEAR(evalScalar("enbw(rectwin(64))"), 1.0, 1e-12);
    EXPECT_NEAR(evalScalar("enbw(rectwin(8))"),  1.0, 1e-12);
}

TEST_F(EnbwTest, Hamming)
{
    EXPECT_NEAR(evalScalar("enbw(hamming(8))"),  1.4970603237670810, 1e-12);
    // Hamming converges as N grows; explicit small-N value above.
}

TEST_F(EnbwTest, Hann)
{
    EXPECT_NEAR(evalScalar("enbw(hann(64))"), 1.5238095238095226, 1e-12);
}

TEST_F(EnbwTest, Blackman)
{
    EXPECT_NEAR(evalScalar("enbw(blackman(64))"), 1.7541662167512511, 1e-12);
}

TEST_F(EnbwTest, FsScaled)
{
    // With fs, output scales by fs/N relative to bin-width form.
    EXPECT_NEAR(evalScalar("enbw(hamming(64), 100)"), 2.1536278497776933, 1e-12);
    // hamming(8) at fs=2 -> 1.49706 · 2/8 = 0.37427.
    EXPECT_NEAR(evalScalar("enbw(hamming(8), 2)"), 0.3742650809417702, 1e-12);
}
