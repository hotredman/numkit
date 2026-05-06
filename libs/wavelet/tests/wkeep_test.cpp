// libs/wavelet/tests/wkeep_test.cpp
//
// Backfill gtest for libs/wavelet/src/dwt/wkeep_wextend.cpp::wkeep.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class WkeepTest : public ::testing::Test
{
public:
    Engine engine;
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
