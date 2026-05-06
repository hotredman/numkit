// libs/stats/tests/bounds_mode_test.cpp
//
// Closes audit/closed/stats/{bounds,mode}.md — 'all' / vecdim dispatch.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class BoundsModeTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override
    {
        engine.eval("import compat.*;");
        engine.eval("A = [1 4 7; 2 5 8; 3 6 9; 4 7 10; 5 8 11];");
    }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(BoundsModeTest, BoundsBasic)
{
    eval("[lo, hi] = bounds(A);");
    EXPECT_DOUBLE_EQ(evalScalar("lo(1)"), 1);
    EXPECT_DOUBLE_EQ(evalScalar("hi(3)"), 11);
}

TEST_F(BoundsModeTest, BoundsAllString)
{
    eval("[lo, hi] = bounds(A, 'all');");
    EXPECT_DOUBLE_EQ(evalScalar("lo"), 1);
    EXPECT_DOUBLE_EQ(evalScalar("hi"), 11);
}

TEST_F(BoundsModeTest, BoundsVecdim)
{
    eval("[lo, hi] = bounds(A, [1 2]);");
    EXPECT_DOUBLE_EQ(evalScalar("lo"), 1);
    EXPECT_DOUBLE_EQ(evalScalar("hi"), 11);
}

TEST_F(BoundsModeTest, ModeBasic)
{
    EXPECT_DOUBLE_EQ(evalScalar("mode([1 2 2 3 3 3])"), 3);
}

TEST_F(BoundsModeTest, ModeAllMatrix)
{
    EXPECT_DOUBLE_EQ(evalScalar("mode(A, 'all')"), 4);
}

TEST_F(BoundsModeTest, ModeVecdim)
{
    EXPECT_DOUBLE_EQ(evalScalar("mode(A, [1 2])"), 4);
}
