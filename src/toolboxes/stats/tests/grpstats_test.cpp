// toolboxes/stats/tests/grpstats_test.cpp
//
// Regression guard for grpstats() — per-group statistics.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class GrpstatsTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override {}
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(GrpstatsTest, DefaultMean)
{
    eval("X = [1 10; 2 20; 3 30; 4 40; 5 50; 6 60];"
         "g = [1 1 2 2 1 2];"
         "m = grpstats(X, g);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(m, 1)")), 2);
    EXPECT_EQ(static_cast<int>(evalScalar("size(m, 2)")), 2);
    EXPECT_NEAR(evalScalar("m(1, 1)"),  8.0/3.0, 1e-12);
    EXPECT_NEAR(evalScalar("m(1, 2)"), 80.0/3.0, 1e-12);
    EXPECT_NEAR(evalScalar("m(2, 1)"), 13.0/3.0, 1e-12);
    EXPECT_NEAR(evalScalar("m(2, 2)"), 130.0/3.0, 1e-12);
}

TEST_F(GrpstatsTest, MultiFnReturnsMultiOutputs)
{
    eval("X = [1 10; 2 20; 3 30; 4 40; 5 50; 6 60];"
         "g = [1 1 2 2 1 2];"
         "[m, s] = grpstats(X, g, {'mean', 'std'});");
    EXPECT_NEAR(evalScalar("m(1, 1)"), 8.0/3.0, 1e-12);
    EXPECT_NEAR(evalScalar("s(1, 1)"), 2.0816659994661326, 1e-9);
    EXPECT_NEAR(evalScalar("s(2, 2)"), 15.275252316519465, 1e-9);
}

TEST_F(GrpstatsTest, SumAggregator)
{
    eval("ms = grpstats([1 10; 2 20; 3 30; 4 40; 5 50; 6 60], "
         "                [1 1 2 2 1 2], 'sum');");
    EXPECT_DOUBLE_EQ(evalScalar("ms(1, 1)"),  8.0);
    EXPECT_DOUBLE_EQ(evalScalar("ms(1, 2)"), 80.0);
    EXPECT_DOUBLE_EQ(evalScalar("ms(2, 1)"), 13.0);
    EXPECT_DOUBLE_EQ(evalScalar("ms(2, 2)"), 130.0);
}

TEST_F(GrpstatsTest, NumelAggregator)
{
    eval("mn = grpstats([1 10; 2 20; 3 30; 4 40; 5 50; 6 60], "
         "                [1 1 2 2 1 2], 'numel');");
    EXPECT_DOUBLE_EQ(evalScalar("mn(1, 1)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("mn(2, 1)"), 3.0);
}

TEST_F(GrpstatsTest, MinMaxAggregators)
{
    eval("X = [1 10; 5 50; 3 30; 4 40];"
         "g = [1 1 2 2];"
         "mn = grpstats(X, g, 'min');"
         "mx = grpstats(X, g, 'max');");
    EXPECT_DOUBLE_EQ(evalScalar("mn(1, 1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("mn(1, 2)"), 10.0);
    EXPECT_DOUBLE_EQ(evalScalar("mx(1, 1)"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("mx(2, 2)"), 40.0);
}

TEST_F(GrpstatsTest, VectorInputColumnOutput)
{
    eval("v = [1 2 3 4 5 6]; g = [1 1 2 2 1 2]; m = grpstats(v, g);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(m, 1)")), 2);
    EXPECT_EQ(static_cast<int>(evalScalar("size(m, 2)")), 1);
    EXPECT_NEAR(evalScalar("m(1)"), 8.0/3.0, 1e-12);
    EXPECT_NEAR(evalScalar("m(2)"), 13.0/3.0, 1e-12);
}

TEST_F(GrpstatsTest, NaNExcludedFromGroup)
{
    eval("X = [1 NaN; 2 20; 3 30];"
         "g = [1 1 1];"
         "m = grpstats(X, g);");
    EXPECT_DOUBLE_EQ(evalScalar("m(1, 1)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("m(1, 2)"), 25.0);  // mean(20, 30)
}

TEST_F(GrpstatsTest, RejectsLengthMismatch)
{
    bool threw = false;
    try { eval("grpstats([1 2; 3 4], [1 1 1]);"); } catch (...) { threw = true; }
    EXPECT_TRUE(threw);
}

TEST_F(GrpstatsTest, RejectsBadFnName)
{
    bool threw = false;
    try { eval("grpstats([1 2 3]', [1 1 1], 'unknown');"); }
    catch (...) { threw = true; }
    EXPECT_TRUE(threw);
}

TEST_F(GrpstatsTest, GroupOrderIsAscending)
{
    // Output rows should be in unique-group sorted ascending order.
    eval("X = [1 2 3 4 5 6]';"
         "g = [3 1 3 2 1 2];"
         "m = grpstats(X, g);");
    // Group 1: rows 2, 5 -> mean(2, 5) = 3.5
    // Group 2: rows 4, 6 -> mean(4, 6) = 5
    // Group 3: rows 1, 3 -> mean(1, 3) = 2
    EXPECT_DOUBLE_EQ(evalScalar("m(1)"), 3.5);
    EXPECT_DOUBLE_EQ(evalScalar("m(2)"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("m(3)"), 2.0);
}

// DEEP-PROBE c178: with no whichstats arg, the default outputs are
// [means, sem, counts] (MATLAB nargout default). numkit only emitted means,
// so [m, sem, counts] = grpstats(X, g) threw "Index 1 exceeds array size 0".
TEST_F(GrpstatsTest, DefaultSemAndCountsOutputs)
{
    eval("x = [1 2 3 4 5 6]'; g = [1 1 2 2 3 3]';");
    eval("[m, s, c] = grpstats(x, g);");
    EXPECT_DOUBLE_EQ(evalScalar("m(1)"), 1.5);
    EXPECT_DOUBLE_EQ(evalScalar("m(2)"), 3.5);
    EXPECT_DOUBLE_EQ(evalScalar("m(3)"), 5.5);
    // sem = std/sqrt(n); each group has [k, k+1] -> std 0.7071, n 2 -> 0.5.
    EXPECT_NEAR(evalScalar("s(1)"), 0.5, 1e-12);
    EXPECT_NEAR(evalScalar("s(2)"), 0.5, 1e-12);
    EXPECT_NEAR(evalScalar("s(3)"), 0.5, 1e-12);
    EXPECT_DOUBLE_EQ(evalScalar("c(1)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("c(2)"), 2.0);
    // Multi-column: sem of the 2nd column scales with the data.
    eval("X = [1 10; 2 20; 3 30; 4 40; 5 50; 6 60]; [mm, ss] = grpstats(X, g);");
    EXPECT_NEAR(evalScalar("mm(1,2)"), 15.0, 1e-12);
    EXPECT_NEAR(evalScalar("ss(1,2)"),  5.0, 1e-12);
}
