// libs/builtin/tests/groupfilter_test.cpp
//
// Regression guard for groupfilter array form.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class GroupFilterTest : public ::testing::Test
{
public:
    StdEngine engine;
    void SetUp() override {
        engine.eval("A = [10; 20; 30; 40; 50; 60]; G = [1; 2; 1; 2; 1; 2];");
    }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// ── scalar predicates → keep whole group ────────────────────────

TEST_F(GroupFilterTest, ScalarMeanThreshold)
{
    eval("B = groupfilter(A, G, @(x) mean(x) > 30);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(B,1)")), 3);
    EXPECT_NEAR(evalScalar("B(1)"), 20.0, 1e-12);
    EXPECT_NEAR(evalScalar("B(2)"), 40.0, 1e-12);
    EXPECT_NEAR(evalScalar("B(3)"), 60.0, 1e-12);
}

TEST_F(GroupFilterTest, ScalarNumelAllPass)
{
    eval("B = groupfilter(A, G, @(x) numel(x) > 2);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(B,1)")), 6);
}

TEST_F(GroupFilterTest, ScalarPredicateAllFail)
{
    eval("B = groupfilter(A, G, @(x) mean(x) > 1000);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(B,1)")), 0);
    EXPECT_EQ(static_cast<int>(evalScalar("size(B,2)")), 1);
}

// ── vector predicate → per-row mask ─────────────────────────────

TEST_F(GroupFilterTest, ElementwiseGtMean)
{
    eval("B = groupfilter(A, G, @(x) x > mean(x));");
    EXPECT_EQ(static_cast<int>(evalScalar("size(B,1)")), 2);
    EXPECT_NEAR(evalScalar("B(1)"), 50.0, 1e-12);
    EXPECT_NEAR(evalScalar("B(2)"), 60.0, 1e-12);
}

// ── matrix A ─────────────────────────────────────────────────────

TEST_F(GroupFilterTest, MatrixWithReducedRowVecResult)
{
    // mean(matrix) returns row vec [meanCol1, meanCol2]; both must be > 30
    // for the group to be kept. Group 1 means = [30, 300] → first col not
    // > 30 → drop. Group 2 means = [40, 400] → both > 30 → keep.
    eval("A2 = [10 100; 20 200; 30 300; 40 400; 50 500; 60 600];"
         "B = groupfilter(A2, G, @(x) mean(x) > 30);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(B,1)")), 3);
    EXPECT_EQ(static_cast<int>(evalScalar("size(B,2)")), 2);
    EXPECT_NEAR(evalScalar("B(1,1)"),  20.0, 1e-12);
    EXPECT_NEAR(evalScalar("B(1,2)"), 200.0, 1e-12);
    EXPECT_NEAR(evalScalar("B(3,1)"),  60.0, 1e-12);
    EXPECT_NEAR(evalScalar("B(3,2)"), 600.0, 1e-12);
}

// ── NaN group ────────────────────────────────────────────────────

TEST_F(GroupFilterTest, NaNGroupSingleton)
{
    // NaN entries form a singleton group — each NaN element is its own
    // group of size 1. Predicate mean(x)>25 → for singletons (30, 60)
    // both pass.
    eval("G2 = [1; 2; NaN; 1; 2; NaN];"
         "B = groupfilter(A, G2, @(x) mean(x) > 25);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(B,1)")), 4);
    EXPECT_NEAR(evalScalar("B(1)"), 20.0, 1e-12);
    EXPECT_NEAR(evalScalar("B(2)"), 30.0, 1e-12);
    EXPECT_NEAR(evalScalar("B(3)"), 50.0, 1e-12);
    EXPECT_NEAR(evalScalar("B(4)"), 60.0, 1e-12);
}

// ── 2nd output BG ────────────────────────────────────────────────

TEST_F(GroupFilterTest, TwoOutputBG)
{
    eval("[B, BG] = groupfilter(A, G, @(x) mean(x) > 30);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(BG,1)")), 3);
    EXPECT_EQ(static_cast<int>(evalScalar("BG(1)")), 2);
    EXPECT_EQ(static_cast<int>(evalScalar("BG(2)")), 2);
    EXPECT_EQ(static_cast<int>(evalScalar("BG(3)")), 2);
}

// ── errors ───────────────────────────────────────────────────────

TEST_F(GroupFilterTest, NoPredicateThrows)
{
    EXPECT_THROW(eval("groupfilter(A, G);"), std::exception);
}

TEST_F(GroupFilterTest, StringMethodThrows)
{
    EXPECT_THROW(eval("groupfilter(A, G, 'mean');"), std::exception);
}

TEST_F(GroupFilterTest, MismatchedSizeThrows)
{
    EXPECT_THROW(eval("groupfilter(A, [1; 2; 1], @(x) true);"), std::exception);
}
