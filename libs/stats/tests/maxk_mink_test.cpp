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

// ── second output: indices [M, I] = mink/maxk(...) ──────────────────────
TEST_F(MaxkMinkTest, MinkIndexVector)
{
    eval("[m, ix] = mink([5 2 8 1 9], 2);");
    EXPECT_DOUBLE_EQ(evalScalar("m(1)"), 1);
    EXPECT_DOUBLE_EQ(evalScalar("m(2)"), 2);
    EXPECT_DOUBLE_EQ(evalScalar("ix(1)"), 4); // value 1 is at position 4
    EXPECT_DOUBLE_EQ(evalScalar("ix(2)"), 2); // value 2 is at position 2
}

TEST_F(MaxkMinkTest, MaxkIndexVector)
{
    eval("[m, ix] = maxk([5 2 8 1 9], 2);");
    EXPECT_DOUBLE_EQ(evalScalar("m(1)"), 9);
    EXPECT_DOUBLE_EQ(evalScalar("m(2)"), 8);
    EXPECT_DOUBLE_EQ(evalScalar("ix(1)"), 5); // value 9 is at position 5
    EXPECT_DOUBLE_EQ(evalScalar("ix(2)"), 3); // value 8 is at position 3
}

TEST_F(MaxkMinkTest, MinkIndexTiesKeepLowerPosition)
{
    // Equal values keep the lower original index (stable).
    eval("[m, ix] = mink([3 1 3], 3);");
    EXPECT_DOUBLE_EQ(evalScalar("ix(1)"), 2); // the 1
    EXPECT_DOUBLE_EQ(evalScalar("ix(2)"), 1); // first 3
    EXPECT_DOUBLE_EQ(evalScalar("ix(3)"), 3); // second 3
}

TEST_F(MaxkMinkTest, MinkIndexMatrixDefaultDim)
{
    // mink along dim 1: indices are row positions within each column.
    eval("[m, ix] = mink([3 6; 1 4; 2 5], 2);");
    EXPECT_DOUBLE_EQ(evalScalar("m(1,1)"), 1);
    EXPECT_DOUBLE_EQ(evalScalar("ix(1,1)"), 2);
    EXPECT_DOUBLE_EQ(evalScalar("ix(2,1)"), 3);
    EXPECT_DOUBLE_EQ(evalScalar("ix(1,2)"), 2);
}

TEST_F(MaxkMinkTest, MaxkIndexDim2)
{
    eval("[m, ix] = maxk([1 5 2; 8 3 9], 2, 2);");
    EXPECT_DOUBLE_EQ(evalScalar("ix(1,1)"), 2); // row1: max 5 at col 2
    EXPECT_DOUBLE_EQ(evalScalar("ix(1,2)"), 3); // row1: next 2 at col 3
    EXPECT_DOUBLE_EQ(evalScalar("ix(2,1)"), 3); // row2: max 9 at col 3
    EXPECT_DOUBLE_EQ(evalScalar("ix(2,2)"), 1); // row2: next 8 at col 1
}
