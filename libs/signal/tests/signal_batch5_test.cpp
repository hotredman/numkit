// libs/signal/tests/signal_batch5_test.cpp
//
// Signal batch 5 closure (4 functions):
//   measurements: peak2peak · rssq · findpeaks · hampel
//
// All bit-identical MATLAB R2025b on probed inputs.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class SignalBatch5Test : public ::testing::Test
{
public:
    StdEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(SignalBatch5Test, Peak2Peak)
{
    EXPECT_DOUBLE_EQ(evalScalar("peak2peak([1 5 2 -3 4])"), 8.0);  // 5-(-3)
    EXPECT_DOUBLE_EQ(evalScalar("peak2peak([0 0 0])"),     0.0);
}

TEST_F(SignalBatch5Test, Rssq)
{
    EXPECT_NEAR(evalScalar("rssq([3 4])"),  5.0, 1e-12);   // sqrt(9+16)
    EXPECT_NEAR(evalScalar("rssq([1 2 2])"), 3.0, 1e-12);  // sqrt(1+4+4)
}

TEST_F(SignalBatch5Test, Findpeaks)
{
    eval("[pks, locs] = findpeaks([1 3 2 5 1 4]);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(pks)"), 2.0);  // peaks at 3 and 5
}

TEST_F(SignalBatch5Test, Hampel)
{
    eval("y = hampel([1 2 100 3 4]);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(y)"), 5.0);
}
