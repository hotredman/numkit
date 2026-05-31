// libs/builtin/tests/matchpairs_test.cpp
//
// Regression guard for matchpairs — Hungarian / Jonker-Volgenant
// linear-assignment solver on rectangular Cost with unmatched-cost
// penalty per row / per col.

#include <numkit/core/engine.hpp>
#include <numkit/builtin/library.hpp>
#include <cmath>
#include <gtest/gtest.h>

using namespace numkit;

class MatchpairsTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// Classic 3×3. Optimal: 1+2+2 = 5.
TEST_F(MatchpairsTest, Square3x3FindsOptimalCost)
{
    eval("Cost = [4 1 3; 2 0 5; 3 2 2];"
         "[M, uR, uC] = matchpairs(Cost, 100);"
         "total = 0; for k = 1:size(M, 1); total = total + Cost(M(k,1), M(k,2)); end");
    EXPECT_EQ(static_cast<int>(evalScalar("size(M, 1)")), 3);
    EXPECT_EQ(static_cast<int>(evalScalar("size(M, 2)")), 2);
    EXPECT_DOUBLE_EQ(evalScalar("total"), 5.0);
    EXPECT_EQ(static_cast<int>(evalScalar("numel(uR)")), 0);
    EXPECT_EQ(static_cast<int>(evalScalar("numel(uC)")), 0);
}

// MATLAB doc example: 3 detections, 3 targets, penalty 20.
TEST_F(MatchpairsTest, DocExampleAllMatched)
{
    eval("Cost = [10 15 9; 9 18 5; 6 14 3];"
         "[M, uR, uC] = matchpairs(Cost, 20);"
         "total = 0; for k = 1:size(M, 1); total = total + Cost(M(k,1), M(k,2)); end");
    EXPECT_EQ(static_cast<int>(evalScalar("size(M, 1)")), 3);
    // Optimal assignment is 1→2, 2→3, 3→1 (or row-permuted equivalent),
    // total = 15 + 5 + 6 = 26.
    EXPECT_DOUBLE_EQ(evalScalar("total"), 26.0);
    EXPECT_EQ(static_cast<int>(evalScalar("numel(uR)")), 0);
}

// Rectangular: 4 rows × 2 cols with high penalty — only 2 rows match.
TEST_F(MatchpairsTest, Rectangular4x2KeepsMinMatches)
{
    eval("Cost = [10 20; 30 5; 25 8; 1 50];"
         "[M, uR, uC] = matchpairs(Cost, 100);"
         "total = 0; for k = 1:size(M, 1); total = total + Cost(M(k,1), M(k,2)); end");
    // 2 cols, so at most 2 matches.
    EXPECT_EQ(static_cast<int>(evalScalar("size(M, 1)")), 2);
    EXPECT_EQ(static_cast<int>(evalScalar("numel(uR)")), 2);
    EXPECT_EQ(static_cast<int>(evalScalar("numel(uC)")), 0);
    // Optimal: (2,2)+(4,1) cost = 5+1 = 6.
    EXPECT_DOUBLE_EQ(evalScalar("total"), 6.0);
}

// Penalty so low that no real match is worthwhile.
TEST_F(MatchpairsTest, LowPenaltyLeavesAllUnmatched)
{
    eval("Cost = [10 20; 30 40];"
         "[M, uR, uC] = matchpairs(Cost, 0.5);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(M, 1)")), 0);
    EXPECT_EQ(static_cast<int>(evalScalar("numel(uR)")), 2);
    EXPECT_EQ(static_cast<int>(evalScalar("numel(uC)")), 2);
}

// 'max' mode with a zero (or negative) penalty: matches everything to
// maximise benefit. NOTE: in MATLAB 'max', `costUnmatched` is a REWARD
// for leaving unmatched (not a penalty), so a high positive value
// pushes the solver to leave everything unmatched. Use 0 to force the
// full max assignment here.
TEST_F(MatchpairsTest, MaxModeMaximisesTotal)
{
    eval("Cost = [1 5; 4 2];"
         "[M, ~, ~] = matchpairs(Cost, 0, 'max');"
         "total = 0; for k = 1:size(M, 1); total = total + Cost(M(k,1), M(k,2)); end");
    EXPECT_EQ(static_cast<int>(evalScalar("size(M, 1)")), 2);
    EXPECT_DOUBLE_EQ(evalScalar("total"), 9.0);
}

// 'max' with a HIGH positive unmatched-reward leaves everything
// unmatched (matching MATLAB's documented convention).
TEST_F(MatchpairsTest, MaxModeHighRewardLeavesAllUnmatched)
{
    eval("Cost = [1 5; 4 2];"
         "[M, uR, uC] = matchpairs(Cost, 100, 'max');");
    EXPECT_EQ(static_cast<int>(evalScalar("size(M, 1)")), 0);
    EXPECT_EQ(static_cast<int>(evalScalar("numel(uR)")), 2);
    EXPECT_EQ(static_cast<int>(evalScalar("numel(uC)")), 2);
}

// Empty cost matrix: 0×3 → all 3 cols unmatched, no rows.
TEST_F(MatchpairsTest, EmptyZeroByThreeReturnsAllColsUnmatched)
{
    eval("[M, uR, uC] = matchpairs(zeros(0, 3), 1);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(M, 1)")), 0);
    EXPECT_EQ(static_cast<int>(evalScalar("numel(uR)")), 0);
    EXPECT_EQ(static_cast<int>(evalScalar("numel(uC)")), 3);
}

// Unknown mode string throws.
TEST_F(MatchpairsTest, BadModeStringThrows)
{
    EXPECT_THROW(eval("matchpairs([1 2; 3 4], 10, 'pickyourpoison');"),
                 std::exception);
}
