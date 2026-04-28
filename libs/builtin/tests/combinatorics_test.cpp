// libs/builtin/tests/combinatorics_test.cpp
//
// Combinatorics: perms (factorial / nchoosek planned for tier-2).

#include "dual_engine_fixture.hpp"

#include <algorithm>
#include <cmath>
#include <set>

using namespace m_test;

class CombinatoricsTest : public DualEngineTest
{};

// ── perms ──────────────────────────────────────────────────────

TEST_P(CombinatoricsTest, PermsScalarReturnsOneRow)
{
    eval("P = perms([7]);");
    auto *P = getVarPtr("P");
    EXPECT_EQ(rows(*P), 1u);
    EXPECT_EQ(cols(*P), 1u);
    EXPECT_DOUBLE_EQ((*P)(0, 0), 7.0);
}

TEST_P(CombinatoricsTest, PermsTwoElements)
{
    // perms([1 2]) → [2 1; 1 2] (reverse-lex order).
    eval("P = perms([1 2]);");
    auto *P = getVarPtr("P");
    EXPECT_EQ(rows(*P), 2u);
    EXPECT_EQ(cols(*P), 2u);
    EXPECT_DOUBLE_EQ((*P)(0, 0), 2.0);
    EXPECT_DOUBLE_EQ((*P)(0, 1), 1.0);
    EXPECT_DOUBLE_EQ((*P)(1, 0), 1.0);
    EXPECT_DOUBLE_EQ((*P)(1, 1), 2.0);
}

TEST_P(CombinatoricsTest, PermsThreeElementsMatlabOrder)
{
    // MATLAB perms([1 2 3]) =
    //   3 2 1
    //   3 1 2
    //   2 3 1
    //   2 1 3
    //   1 3 2
    //   1 2 3
    eval("P = perms([1 2 3]);");
    auto *P = getVarPtr("P");
    EXPECT_EQ(rows(*P), 6u);
    EXPECT_EQ(cols(*P), 3u);
    const double expected[6][3] = {
        {3, 2, 1}, {3, 1, 2}, {2, 3, 1},
        {2, 1, 3}, {1, 3, 2}, {1, 2, 3},
    };
    for (size_t r = 0; r < 6; ++r)
        for (size_t c = 0; c < 3; ++c)
            EXPECT_DOUBLE_EQ((*P)(r, c), expected[r][c])
                << "at (" << r << "," << c << ")";
}

TEST_P(CombinatoricsTest, PermsRowVsColumnInputBothWork)
{
    // Row input.
    eval("Pr = perms([1 2 3 4]);");
    // Column input — should produce the same matrix.
    eval("Pc = perms([1; 2; 3; 4]);");
    eval("delta = max(abs(Pr(:) - Pc(:)));");
    EXPECT_DOUBLE_EQ(evalScalar("delta;"), 0.0);
}

TEST_P(CombinatoricsTest, PermsCount)
{
    // 5! = 120 rows.
    eval("P = perms(1:5);");
    auto *P = getVarPtr("P");
    EXPECT_EQ(rows(*P), 120u);
    EXPECT_EQ(cols(*P), 5u);
}

TEST_P(CombinatoricsTest, PermsEachRowIsDistinctPermutation)
{
    // Verify uniqueness + sortedness sanity for n=4.
    eval("P = perms([1 2 3 4]);");
    auto *P = getVarPtr("P");
    EXPECT_EQ(rows(*P), 24u);
    std::set<std::vector<double>> seen;
    for (size_t r = 0; r < 24; ++r) {
        std::vector<double> row(4);
        for (size_t c = 0; c < 4; ++c) row[c] = (*P)(r, c);
        // Must be a permutation of {1,2,3,4}.
        std::vector<double> sorted = row;
        std::sort(sorted.begin(), sorted.end());
        EXPECT_DOUBLE_EQ(sorted[0], 1.0);
        EXPECT_DOUBLE_EQ(sorted[1], 2.0);
        EXPECT_DOUBLE_EQ(sorted[2], 3.0);
        EXPECT_DOUBLE_EQ(sorted[3], 4.0);
        // Row must be unique.
        EXPECT_TRUE(seen.insert(row).second);
    }
}

TEST_P(CombinatoricsTest, PermsEmptyReturnsOneEmptyRow)
{
    eval("P = perms([]);");
    auto *P = getVarPtr("P");
    EXPECT_EQ(rows(*P), 1u);
    EXPECT_EQ(cols(*P), 0u);
}

TEST_P(CombinatoricsTest, PermsTooLargeThrows)
{
    // 12! = 479M rows would exhaust memory — n > 11 throws.
    EXPECT_THROW(eval("P = perms(1:12);"), std::exception);
}

TEST_P(CombinatoricsTest, PermsMatrixInputThrows)
{
    EXPECT_THROW(eval("P = perms([1 2; 3 4]);"), std::exception);
}

TEST_P(CombinatoricsTest, PermsComplexThrows)
{
    EXPECT_THROW(eval("P = perms([1+2i, 3]);"), std::exception);
}

TEST_P(CombinatoricsTest, PermsPromotesIntegerToDouble)
{
    eval("P = perms(int32([10 20]));");
    auto *P = getVarPtr("P");
    EXPECT_EQ(P->type(), MType::DOUBLE);
    EXPECT_DOUBLE_EQ((*P)(0, 0), 20.0);
    EXPECT_DOUBLE_EQ((*P)(0, 1), 10.0);
}

INSTANTIATE_DUAL(CombinatoricsTest);
