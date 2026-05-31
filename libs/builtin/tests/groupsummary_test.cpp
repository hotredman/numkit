// libs/builtin/tests/groupsummary_test.cpp
//
// Regression guard for groupsummary array form.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

#include <cmath>

using namespace numkit;

class GroupSummaryTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override {
        engine.eval("A = [10; 20; 30; 40; 50]; G = [1; 2; 1; 2; 1];");
    }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// ── reduction methods ────────────────────────────────────────────

TEST_F(GroupSummaryTest, SumFull3Out)
{
    eval("[B, BG, BC] = groupsummary(A, G, 'sum');");
    EXPECT_EQ(static_cast<int>(evalScalar("B(1)")), 90);
    EXPECT_EQ(static_cast<int>(evalScalar("B(2)")), 60);
    EXPECT_EQ(static_cast<int>(evalScalar("BG(1)")), 1);
    EXPECT_EQ(static_cast<int>(evalScalar("BG(2)")), 2);
    EXPECT_EQ(static_cast<int>(evalScalar("BC(1)")), 3);
    EXPECT_EQ(static_cast<int>(evalScalar("BC(2)")), 2);
}

TEST_F(GroupSummaryTest, Mean)
{
    eval("B = groupsummary(A, G, 'mean');");
    EXPECT_NEAR(evalScalar("B(1)"), 30.0, 1e-12);
    EXPECT_NEAR(evalScalar("B(2)"), 30.0, 1e-12);
}

TEST_F(GroupSummaryTest, Median)
{
    eval("B = groupsummary(A, G, 'median');");
    EXPECT_NEAR(evalScalar("B(1)"), 30.0, 1e-12);
    EXPECT_NEAR(evalScalar("B(2)"), 30.0, 1e-12);
}

TEST_F(GroupSummaryTest, MaxMin)
{
    eval("Bx = groupsummary(A, G, 'max'); Bn = groupsummary(A, G, 'min');");
    EXPECT_NEAR(evalScalar("Bx(1)"), 50.0, 1e-12);
    EXPECT_NEAR(evalScalar("Bx(2)"), 40.0, 1e-12);
    EXPECT_NEAR(evalScalar("Bn(1)"), 10.0, 1e-12);
    EXPECT_NEAR(evalScalar("Bn(2)"), 20.0, 1e-12);
}

TEST_F(GroupSummaryTest, Std)
{
    eval("B = groupsummary(A, G, 'std');");
    EXPECT_NEAR(evalScalar("B(1)"), 20.0, 1e-12);                // std([10 30 50])
    EXPECT_NEAR(evalScalar("B(2)"), 14.142135623730951, 1e-10); // std([20 40])
}

TEST_F(GroupSummaryTest, NumUnique)
{
    eval("B = groupsummary(A, G, 'numunique');");
    EXPECT_EQ(static_cast<int>(evalScalar("B(1)")), 3);
    EXPECT_EQ(static_cast<int>(evalScalar("B(2)")), 2);
}

TEST_F(GroupSummaryTest, Nnz)
{
    eval("A2 = [0; 1; 0; 1; 1]; B = groupsummary(A2, G, 'nnz');");
    EXPECT_EQ(static_cast<int>(evalScalar("B(1)")), 1);  // group 1 = [0 0 1] → 1 nonzero
    EXPECT_EQ(static_cast<int>(evalScalar("B(2)")), 2);  // group 2 = [1 1] → 2 nonzero
}

// ── matrix A ─────────────────────────────────────────────────────

TEST_F(GroupSummaryTest, MatrixA)
{
    eval("A2 = [10 100; 20 200; 30 300; 40 400; 50 500];"
         "B = groupsummary(A2, G, 'sum');");
    EXPECT_EQ(static_cast<int>(evalScalar("size(B,1)")), 2);
    EXPECT_EQ(static_cast<int>(evalScalar("size(B,2)")), 2);
    EXPECT_EQ(static_cast<int>(evalScalar("B(1,1)")),  90);
    EXPECT_EQ(static_cast<int>(evalScalar("B(2,1)")),  60);
    EXPECT_EQ(static_cast<int>(evalScalar("B(1,2)")), 900);
    EXPECT_EQ(static_cast<int>(evalScalar("B(2,2)")), 600);
}

// ── NaN group ────────────────────────────────────────────────────

TEST_F(GroupSummaryTest, NaNGroupTrailing)
{
    eval("G2 = [1; 2; NaN; 1; 2]; B = groupsummary(A, G2, 'sum');");
    EXPECT_EQ(static_cast<int>(evalScalar("size(B,1)")), 3);   // 1, 2, NaN
    EXPECT_EQ(static_cast<int>(evalScalar("B(1)")), 50);       // 10 + 40
    EXPECT_EQ(static_cast<int>(evalScalar("B(2)")), 70);       // 20 + 50
    EXPECT_EQ(static_cast<int>(evalScalar("B(3)")), 30);       // 30
}

// ── errors ───────────────────────────────────────────────────────

TEST_F(GroupSummaryTest, NoMethodThrows)
{
    EXPECT_THROW(eval("groupsummary(A, G);"), std::exception);
}

TEST_F(GroupSummaryTest, BadMethodThrows)
{
    EXPECT_THROW(eval("groupsummary(A, G, 'gibberish');"), std::exception);
}

TEST_F(GroupSummaryTest, MismatchedSizeThrows)
{
    EXPECT_THROW(eval("groupsummary(A, [1; 2; 1], 'sum');"), std::exception);
}
