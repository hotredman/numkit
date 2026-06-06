// libs/comm/tests/randerr_test.cpp
//
// Regression guard for randerr() — random binary error matrix.
// Bit-equal with MATLAB R2025b when seeded.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class RanderrTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(RanderrTest, ScalarOneErrorPerRowSeeded)
{
    // MATLAB: randerr(5, 8, 1, 42) — column placements per row:
    //   row 1: col 8;  row 2: col 1;  row 3: col 3;
    //   row 4: col 4;  row 5: col 2.
    eval("y = randerr(5, 8, 1, 42);");
    EXPECT_DOUBLE_EQ(evalScalar("y(1, 8)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(2, 1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(3, 3)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(4, 4)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(5, 2)"), 1.0);
    // Each row sum = 1
    EXPECT_DOUBLE_EQ(evalScalar("sum(y(1,:))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("sum(y(5,:))"), 1.0);
}

TEST_F(RanderrTest, ScalarThreeErrorsSeeded)
{
    // Each row should have exactly 3 ones.
    eval("y = randerr(5, 10, 3, 42);");
    for (int r = 1; r <= 5; ++r) {
        const std::string q = "sum(y(" + std::to_string(r) + ",:))";
        EXPECT_DOUBLE_EQ(evalScalar(q), 3.0);
    }
}

TEST_F(RanderrTest, VectorErrorsUniform)
{
    // errors=[1 2 3], seed=42 -> row sums per MATLAB: 2 3 1 3 1
    eval("y = randerr(5, 10, [1 2 3], 42);");
    EXPECT_DOUBLE_EQ(evalScalar("sum(y(1,:))"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("sum(y(2,:))"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("sum(y(3,:))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("sum(y(4,:))"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("sum(y(5,:))"), 1.0);
}

TEST_F(RanderrTest, WeightedErrors)
{
    // [0 1 2; 0.0 0.7 0.3], seed=42 -> row sums: 1 2 1 2 1
    eval("y = randerr(5, 10, [0 1 2; 0.0 0.7 0.3], 42);");
    EXPECT_DOUBLE_EQ(evalScalar("sum(y(1,:))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("sum(y(2,:))"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("sum(y(3,:))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("sum(y(4,:))"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("sum(y(5,:))"), 1.0);
}

TEST_F(RanderrTest, OutputIsBinary)
{
    // Every entry is 0 or 1.
    eval("y = randerr(20, 20, [1 2 3 4], 99);"
         "all_binary = all(all(y == 0 | y == 1));");
    EXPECT_DOUBLE_EQ(evalScalar("all_binary"), 1.0);
}

TEST_F(RanderrTest, ShapePreserved)
{
    eval("y = randerr(7, 12, 1, 42);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(y, 1)")), 7);
    EXPECT_EQ(static_cast<int>(evalScalar("size(y, 2)")), 12);
}

TEST_F(RanderrTest, DeterministicOnSameSeed)
{
    eval("a = randerr(20, 20, 3, 7);"
         "b = randerr(20, 20, 3, 7);"
         "match = isequal(a, b);");
    EXPECT_DOUBLE_EQ(evalScalar("match"), 1.0);
}

TEST_F(RanderrTest, RejectsErrorsExceedingN)
{
    bool threw = false;
    try { eval("randerr(2, 3, 5);"); }
    catch (...) { threw = true; }
    EXPECT_TRUE(threw);
}

TEST_F(RanderrTest, RejectsBadProbabilitySum)
{
    bool threw = false;
    try { eval("randerr(5, 10, [1 2; 0.5 0.6]);"); }   // sums to 1.1
    catch (...) { threw = true; }
    EXPECT_TRUE(threw);
}
