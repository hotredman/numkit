// libs/wavelet/tests/wrev_test.cpp
//
// Backfill gtest for libs/wavelet/src/filter/qmf.cpp::wrev.
// Reference values captured from MATLAB R2025b probe.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class WrevTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(WrevTest, IntegerRow)
{
    eval("y = wrev([1 2 3 4 5]);");
    EXPECT_EQ(static_cast<size_t>(evalScalar("size(y, 1)")), 1u);
    EXPECT_EQ(static_cast<size_t>(evalScalar("size(y, 2)")), 5u);
    EXPECT_DOUBLE_EQ(evalScalar("y(1)"), 5);
    EXPECT_DOUBLE_EQ(evalScalar("y(2)"), 4);
    EXPECT_DOUBLE_EQ(evalScalar("y(3)"), 3);
    EXPECT_DOUBLE_EQ(evalScalar("y(4)"), 2);
    EXPECT_DOUBLE_EQ(evalScalar("y(5)"), 1);
}

TEST_F(WrevTest, MixedSignDoubles)
{
    eval("y = wrev([1.5 -2 0 7 -1.5]);");
    EXPECT_DOUBLE_EQ(evalScalar("y(1)"), -1.5);
    EXPECT_DOUBLE_EQ(evalScalar("y(2)"),  7.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(3)"),  0.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(4)"), -2.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(5)"),  1.5);
}

TEST_F(WrevTest, ColumnPreservesShape)
{
    eval("y = wrev([1; 2; 3]);");
    EXPECT_EQ(static_cast<size_t>(evalScalar("size(y, 1)")), 3u);
    EXPECT_EQ(static_cast<size_t>(evalScalar("size(y, 2)")), 1u);
    EXPECT_DOUBLE_EQ(evalScalar("y(1)"), 3);
    EXPECT_DOUBLE_EQ(evalScalar("y(3)"), 1);
}

TEST_F(WrevTest, Scalar)
{
    eval("y = wrev(42);");
    EXPECT_DOUBLE_EQ(evalScalar("y"), 42.0);
}

TEST_F(WrevTest, Empty)
{
    eval("y = wrev([]);");
    EXPECT_EQ(static_cast<size_t>(evalScalar("numel(y)")), 0u);
}

TEST_F(WrevTest, RoundtripReverseTwice)
{
    eval("x = [3.14 -2.7 1.41 0 9.9]; y = wrev(wrev(x));");
    EXPECT_DOUBLE_EQ(evalScalar("max(abs(y - x))"), 0.0);
}
