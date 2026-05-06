// libs/wavelet/tests/dyadup_test.cpp
//
// Backfill gtest for libs/wavelet/src/dwt/dyad.cpp::dyadup.
// Reference values from MATLAB R2025b probe.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class DyadupTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// MATLAB convention:
//   default ODD=1: [0 x(1) 0 x(2) 0 ... x(N) 0]   length 2N+1
//   ODD=0:        [x(1) 0 x(2) 0 ... x(N)]        length 2N-1

TEST_F(DyadupTest, DefaultOddOne)
{
    eval("y = dyadup([1 2 3]);");
    EXPECT_EQ(static_cast<size_t>(evalScalar("numel(y)")), 7u);
    EXPECT_DOUBLE_EQ(evalScalar("y(1)"), 0);
    EXPECT_DOUBLE_EQ(evalScalar("y(2)"), 1);
    EXPECT_DOUBLE_EQ(evalScalar("y(3)"), 0);
    EXPECT_DOUBLE_EQ(evalScalar("y(4)"), 2);
    EXPECT_DOUBLE_EQ(evalScalar("y(7)"), 0);
}

TEST_F(DyadupTest, ExplicitOddZero)
{
    eval("y = dyadup([1 2 3], 0);");
    EXPECT_EQ(static_cast<size_t>(evalScalar("numel(y)")), 5u);
    EXPECT_DOUBLE_EQ(evalScalar("y(1)"), 1);
    EXPECT_DOUBLE_EQ(evalScalar("y(2)"), 0);
    EXPECT_DOUBLE_EQ(evalScalar("y(5)"), 3);
}

TEST_F(DyadupTest, ExplicitOddOneMatchesDefault)
{
    eval("y0 = dyadup([1 2 3]); y1 = dyadup([1 2 3], 1);");
    EXPECT_DOUBLE_EQ(evalScalar("max(abs(y0 - y1))"), 0.0);
}

TEST_F(DyadupTest, ColumnPreservesShape)
{
    eval("y = dyadup([1; 2; 3]);");
    EXPECT_EQ(static_cast<size_t>(evalScalar("size(y, 1)")), 7u);
    EXPECT_EQ(static_cast<size_t>(evalScalar("size(y, 2)")), 1u);
    EXPECT_DOUBLE_EQ(evalScalar("y(2)"), 1);
}

TEST_F(DyadupTest, Scalar)
{
    eval("y = dyadup(5);");
    EXPECT_EQ(static_cast<size_t>(evalScalar("numel(y)")), 3u);
    EXPECT_DOUBLE_EQ(evalScalar("y(1)"), 0);
    EXPECT_DOUBLE_EQ(evalScalar("y(2)"), 5);
    EXPECT_DOUBLE_EQ(evalScalar("y(3)"), 0);
}

TEST_F(DyadupTest, Empty)
{
    eval("y = dyadup([]);");
    EXPECT_EQ(static_cast<size_t>(evalScalar("numel(y)")), 0u);
}
