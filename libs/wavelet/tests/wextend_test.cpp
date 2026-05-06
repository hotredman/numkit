// libs/wavelet/tests/wextend_test.cpp
//
// Backfill gtest for libs/wavelet/src/dwt/wkeep_wextend.cpp::wextend.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class WextendTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override
    {
        engine.eval("import compat.*;");
        engine.eval("x = [1 2 3 4 5];");
    }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// MATLAB R2025b reference (lf=2):
//   sym → [2 1 1 2 3 4 5 5 4]
//   per → [5 5 1 2 3 4 5 5 1 2]   (odd N gets x(end) appended first)
//   zpd → [0 0 1 2 3 4 5 0 0]
//   ppd → [4 5 1 2 3 4 5 1 2]

TEST_F(WextendTest, SymWholePoint)
{
    eval("y = wextend(1, 'sym', x, 2);");
    EXPECT_EQ(static_cast<size_t>(evalScalar("numel(y)")), 9u);
    EXPECT_DOUBLE_EQ(evalScalar("y(1)"), 2);
    EXPECT_DOUBLE_EQ(evalScalar("y(2)"), 1);
    EXPECT_DOUBLE_EQ(evalScalar("y(8)"), 5);
    EXPECT_DOUBLE_EQ(evalScalar("y(9)"), 4);
}

TEST_F(WextendTest, PeriodicOddN)
{
    eval("y = wextend(1, 'per', x, 2);");
    // Odd N → MATLAB pre-pads x with x(end), then wraps.
    EXPECT_EQ(static_cast<size_t>(evalScalar("numel(y)")), 10u);
    EXPECT_DOUBLE_EQ(evalScalar("y(1)"), 5);
    EXPECT_DOUBLE_EQ(evalScalar("y(2)"), 5);
    EXPECT_DOUBLE_EQ(evalScalar("y(9)"), 1);
    EXPECT_DOUBLE_EQ(evalScalar("y(10)"), 2);
}

TEST_F(WextendTest, ZeroPad)
{
    eval("y = wextend(1, 'zpd', x, 2);");
    EXPECT_EQ(static_cast<size_t>(evalScalar("numel(y)")), 9u);
    EXPECT_DOUBLE_EQ(evalScalar("y(1)"), 0);
    EXPECT_DOUBLE_EQ(evalScalar("y(2)"), 0);
    EXPECT_DOUBLE_EQ(evalScalar("y(8)"), 0);
    EXPECT_DOUBLE_EQ(evalScalar("y(9)"), 0);
}

TEST_F(WextendTest, PurePeriodic)
{
    eval("y = wextend(1, 'ppd', x, 2);");
    EXPECT_EQ(static_cast<size_t>(evalScalar("numel(y)")), 9u);
    EXPECT_DOUBLE_EQ(evalScalar("y(1)"), 4);
    EXPECT_DOUBLE_EQ(evalScalar("y(2)"), 5);
    EXPECT_DOUBLE_EQ(evalScalar("y(8)"), 1);
    EXPECT_DOUBLE_EQ(evalScalar("y(9)"), 2);
}

TEST_F(WextendTest, SingleSideLeft)
{
    eval("y = wextend(1, 'sym', x, 2, 'l');");
    EXPECT_EQ(static_cast<size_t>(evalScalar("numel(y)")), 7u);
    EXPECT_DOUBLE_EQ(evalScalar("y(1)"), 2);
    EXPECT_DOUBLE_EQ(evalScalar("y(2)"), 1);
}

TEST_F(WextendTest, SingleSideRight)
{
    eval("y = wextend(1, 'sym', x, 2, 'r');");
    EXPECT_EQ(static_cast<size_t>(evalScalar("numel(y)")), 7u);
    EXPECT_DOUBLE_EQ(evalScalar("y(6)"), 5);
    EXPECT_DOUBLE_EQ(evalScalar("y(7)"), 4);
}
