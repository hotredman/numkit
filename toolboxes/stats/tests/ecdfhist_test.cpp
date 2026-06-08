// toolboxes/stats/tests/ecdfhist_test.cpp
// ecdfhist.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class EcdfhistTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(EcdfhistTest, DefaultM10)
{
    eval("[f, x] = ecdf(1:10); [n, c] = ecdfhist(f, x);");
    EXPECT_EQ(static_cast<size_t>(evalScalar("numel(n)")), 10u);
    // Uniform 1..10 -> uniform density 0.1111 in each of 10 bins.
    EXPECT_NEAR(evalScalar("n(1)"),  0.1111111, 1e-7);
    EXPECT_NEAR(evalScalar("n(10)"), 0.1111111, 1e-7);
    EXPECT_NEAR(evalScalar("c(1)"),  1.45, 1e-9);
    EXPECT_NEAR(evalScalar("c(10)"), 9.55, 1e-9);
}

TEST_F(EcdfhistTest, M5)
{
    eval("[f, x] = ecdf(1:10); [n, c] = ecdfhist(f, x, 5);");
    EXPECT_EQ(static_cast<size_t>(evalScalar("numel(n)")), 5u);
    EXPECT_NEAR(evalScalar("c(1)"), 1.9, 1e-9);
    EXPECT_NEAR(evalScalar("c(5)"), 9.1, 1e-9);
}

TEST_F(EcdfhistTest, M3BinAssignmentBoundary)
{
    // Bug fix 2026-05-08: boundary values were going to the wrong bin.
    // For [1..10] with m=3 (edges 1, 4, 7, 10), value 4 belongs to bin 1
    // (edge[k-1], edge[k]], not bin 2 as `floor` produced.
    eval("[f, x] = ecdf(1:10); [n, c] = ecdfhist(f, x, 3);");
    // bin 1 = vals 1..4 (4 jumps × 0.1) → density 0.4/3 ≈ 0.1333.
    // bin 2 = vals 5..7 (3 jumps × 0.1) → 0.3/3 = 0.1.
    // bin 3 = vals 8..10 (3 jumps × 0.1) → 0.1.
    EXPECT_NEAR(evalScalar("n(1)"), 0.1333333, 1e-7);
    EXPECT_NEAR(evalScalar("n(2)"), 0.1, 1e-9);
    EXPECT_NEAR(evalScalar("n(3)"), 0.1, 1e-9);
    EXPECT_NEAR(evalScalar("c(1)"), 2.5, 1e-9);
    EXPECT_NEAR(evalScalar("c(3)"), 8.5, 1e-9);
}

TEST_F(EcdfhistTest, NonUniformInput)
{
    // [1 2 2 3 3 3 5 8] — repeated values produce non-uniform jumps.
    eval("[f, x] = ecdf([1 2 2 3 3 3 5 8]); [n, c] = ecdfhist(f, x);");
    EXPECT_EQ(static_cast<size_t>(evalScalar("numel(n)")), 10u);
    EXPECT_NEAR(evalScalar("n(1)"),  0.1785714, 1e-7);
    EXPECT_NEAR(evalScalar("n(10)"), 0.1785714, 1e-7);
}

TEST_F(EcdfhistTest, AllEqualDataDegenerate)
{
    // All-equal input: full mass goes into the centre bin.
    eval("[f, x] = ecdf([5 5 5]); [n, c] = ecdfhist(f, x, 5);");
    // n(3) (centre of 5 bins) should be non-zero; others zero.
    EXPECT_NEAR(evalScalar("n(3)"), 1.0, 1e-9);  // 1/width with width=1
    EXPECT_DOUBLE_EQ(evalScalar("n(1)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("n(5)"), 0.0);
}
