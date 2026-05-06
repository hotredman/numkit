// libs/wavelet/tests/wmaxlev_test.cpp
//
// Backfill gtest for libs/wavelet/src/dwt/dyad.cpp::wmaxlev.
// Reference values from MATLAB R2025b probe.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class WmaxlevTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// L = floor(log2(N / (Lf - 1)))

TEST_F(WmaxlevTest, Db2On64)
{
    EXPECT_DOUBLE_EQ(evalScalar("wmaxlev(64, 'db2')"), 4);
}

TEST_F(WmaxlevTest, HaarOn64)
{
    EXPECT_DOUBLE_EQ(evalScalar("wmaxlev(64, 'db1')"), 6);
}

TEST_F(WmaxlevTest, Db4On1024)
{
    EXPECT_DOUBLE_EQ(evalScalar("wmaxlev(1024, 'db4')"), 7);
}

TEST_F(WmaxlevTest, VectorNUsesMin)
{
    EXPECT_DOUBLE_EQ(evalScalar("wmaxlev([8 8], 'db1')"), 3);
}

TEST_F(WmaxlevTest, MinimalSignal)
{
    EXPECT_DOUBLE_EQ(evalScalar("wmaxlev(2, 'db1')"), 1);
}

TEST_F(WmaxlevTest, ScalarSignal)
{
    // N = 1, Lf = 2 → N / (Lf-1) = 1; floor(log2(1)) = 0
    EXPECT_DOUBLE_EQ(evalScalar("wmaxlev(1, 'db1')"), 0);
}
