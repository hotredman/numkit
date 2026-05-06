// libs/stats/tests/maxk_mink_test.cpp
//
// Closes audit/closed/stats/{maxk,mink}.md (partial — ComparisonMethod
// real/auto accepted; 'abs' deferred).

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class MaxkMinkTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(MaxkMinkTest, MaxkBasic)
{
    eval("y = maxk([3 1 4 1 5 9 2 6], 3);");
    EXPECT_DOUBLE_EQ(evalScalar("y(1)"), 9);
    EXPECT_DOUBLE_EQ(evalScalar("y(2)"), 6);
    EXPECT_DOUBLE_EQ(evalScalar("y(3)"), 5);
}

TEST_F(MaxkMinkTest, MinkBasic)
{
    eval("y = mink([3 1 4 1 5 9 2 6], 3);");
    EXPECT_DOUBLE_EQ(evalScalar("y(1)"), 1);
    EXPECT_DOUBLE_EQ(evalScalar("y(3)"), 2);
}

TEST_F(MaxkMinkTest, ComparisonMethodReal)
{
    EXPECT_NO_THROW(eval("maxk([3 1 4], 2, 'ComparisonMethod', 'real');"));
    EXPECT_NO_THROW(eval("maxk([3 1 4], 2, 'ComparisonMethod', 'auto');"));
}

TEST_F(MaxkMinkTest, ComparisonMethodAbsRejected)
{
    EXPECT_THROW(eval("maxk([3 1 4], 2, 'ComparisonMethod', 'abs');"),
                 numkit::Error);
}

TEST_F(MaxkMinkTest, BadComparisonMethodErrors)
{
    EXPECT_THROW(eval("maxk([3 1 4], 2, 'ComparisonMethod', 'unknown');"),
                 numkit::Error);
}

TEST_F(MaxkMinkTest, BadNVErrors)
{
    EXPECT_THROW(eval("maxk([3 1 4], 2, 'Foo', 'bar');"), numkit::Error);
}
