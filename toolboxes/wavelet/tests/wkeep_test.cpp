// toolboxes/wavelet/tests/wkeep_test.cpp
//
// Backfill gtest for toolboxes/wavelet/src/dwt/wkeep_wextend.cpp::wkeep.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class WkeepTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override
    {
        engine.eval("import compat.*;");
        engine.eval("x = 1:10;");
    }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(WkeepTest, CentralDefault)
{
    eval("y = wkeep(x, 4);");
    EXPECT_DOUBLE_EQ(evalScalar("y(1)"), 4);
    EXPECT_DOUBLE_EQ(evalScalar("y(4)"), 7);
}

TEST_F(WkeepTest, Left)
{
    eval("y = wkeep(x, 4, 'l');");
    EXPECT_DOUBLE_EQ(evalScalar("y(1)"), 1);
    EXPECT_DOUBLE_EQ(evalScalar("y(4)"), 4);
}

TEST_F(WkeepTest, Right)
{
    eval("y = wkeep(x, 4, 'r');");
    EXPECT_DOUBLE_EQ(evalScalar("y(1)"), 7);
    EXPECT_DOUBLE_EQ(evalScalar("y(4)"), 10);
}

TEST_F(WkeepTest, NumericFirstStart)
{
    eval("y = wkeep(x, 4, 3);");
    EXPECT_DOUBLE_EQ(evalScalar("y(1)"), 3);
    EXPECT_DOUBLE_EQ(evalScalar("y(4)"), 6);
}

TEST_F(WkeepTest, OddNCentral)
{
    eval("y = wkeep([1:9], 4);");
    EXPECT_DOUBLE_EQ(evalScalar("y(1)"), 3);
    EXPECT_DOUBLE_EQ(evalScalar("y(4)"), 6);
}

TEST_F(WkeepTest, EvenNOddKeep)
{
    eval("y = wkeep(x, 3);");
    EXPECT_DOUBLE_EQ(evalScalar("y(1)"), 4);
    EXPECT_DOUBLE_EQ(evalScalar("y(3)"), 6);
}

TEST_F(WkeepTest, ColumnPreservesShape)
{
    eval("y = wkeep([1:10]', 4);");
    EXPECT_EQ(static_cast<size_t>(evalScalar("size(y, 1)")), 4u);
    EXPECT_EQ(static_cast<size_t>(evalScalar("size(y, 2)")), 1u);
}

// Bug fix 2026-05-08 — 2-D matrix form was throwing "Cannot convert
// double to scalar" because adapter did args[1].toScalar() on a 2-vector.

TEST_F(WkeepTest, MatrixCentral)
{
    // magic(5) hardcoded:
    eval("M = [17 24 1 8 15; 23 5 7 14 16; 4 6 13 20 22; 10 12 19 21 3; 11 18 25 2 9];");
    eval("y = wkeep(M, [3 3]);");
    EXPECT_EQ(static_cast<size_t>(evalScalar("size(y, 1)")), 3u);
    EXPECT_EQ(static_cast<size_t>(evalScalar("size(y, 2)")), 3u);
    EXPECT_DOUBLE_EQ(evalScalar("y(1,1)"),  5.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(1,2)"),  7.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(1,3)"), 14.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(2,2)"), 13.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(3,3)"), 21.0);
}

TEST_F(WkeepTest, MatrixExplicitTopLeft)
{
    eval("M = [17 24 1 8 15; 23 5 7 14 16; 4 6 13 20 22; 10 12 19 21 3; 11 18 25 2 9];");
    eval("y = wkeep(M, [3 3], [1 1]);");
    EXPECT_DOUBLE_EQ(evalScalar("y(1,1)"), 17.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(1,3)"),  1.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(3,1)"),  4.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(3,3)"), 13.0);
}

TEST_F(WkeepTest, MatrixOutOfRangeThrows)
{
    eval("M = [1 2; 3 4];");
    bool threw = false;
    try { eval("wkeep(M, [3 3]);"); } catch (...) { threw = true; }
    EXPECT_TRUE(threw);
}
