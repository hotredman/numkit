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
    EXPECT_EQ(P->type(), ValueType::DOUBLE);
    EXPECT_DOUBLE_EQ((*P)(0, 0), 20.0);
    EXPECT_DOUBLE_EQ((*P)(0, 1), 10.0);
}

// ── factorial ──────────────────────────────────────────────────

TEST_P(CombinatoricsTest, FactorialKnownValues)
{
    EXPECT_DOUBLE_EQ(evalScalar("factorial(0);"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("factorial(1);"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("factorial(5);"), 120.0);
    EXPECT_DOUBLE_EQ(evalScalar("factorial(10);"), 3628800.0);
}

TEST_P(CombinatoricsTest, FactorialArrayElementwise)
{
    eval("y = factorial([0 1 2 3 4 5]);");
    auto *y = getVarPtr("y");
    EXPECT_EQ(y->numel(), 6u);
    const double expected[] = {1, 1, 2, 6, 24, 120};
    for (size_t i = 0; i < 6; ++i)
        EXPECT_DOUBLE_EQ(y->doubleData()[i], expected[i]);
}

TEST_P(CombinatoricsTest, FactorialPreservesShape)
{
    eval("y = factorial([1 2; 3 4]);");
    auto *y = getVarPtr("y");
    EXPECT_EQ(rows(*y), 2u);
    EXPECT_EQ(cols(*y), 2u);
    EXPECT_DOUBLE_EQ((*y)(0, 0), 1.0);   // 1!
    EXPECT_DOUBLE_EQ((*y)(0, 1), 2.0);   // 2!
    EXPECT_DOUBLE_EQ((*y)(1, 0), 6.0);   // 3!
    EXPECT_DOUBLE_EQ((*y)(1, 1), 24.0);  // 4!
}

TEST_P(CombinatoricsTest, FactorialOverflowsToInf)
{
    // 171! exceeds double range.
    EXPECT_TRUE(std::isinf(evalScalar("factorial(171);")));
}

TEST_P(CombinatoricsTest, FactorialNegativeThrows)
{
    EXPECT_THROW(eval("factorial(-1);"), std::exception);
}

TEST_P(CombinatoricsTest, FactorialNonIntegerThrows)
{
    EXPECT_THROW(eval("factorial(2.5);"), std::exception);
}

TEST_P(CombinatoricsTest, FactorialComplexThrows)
{
    EXPECT_THROW(eval("factorial(3+4i);"), std::exception);
}

// ── nchoosek ───────────────────────────────────────────────────

TEST_P(CombinatoricsTest, NchoosekKnownValues)
{
    EXPECT_DOUBLE_EQ(evalScalar("nchoosek(0, 0);"),  1.0);
    EXPECT_DOUBLE_EQ(evalScalar("nchoosek(5, 0);"),  1.0);
    EXPECT_DOUBLE_EQ(evalScalar("nchoosek(5, 5);"),  1.0);
    EXPECT_DOUBLE_EQ(evalScalar("nchoosek(5, 2);"), 10.0);
    EXPECT_DOUBLE_EQ(evalScalar("nchoosek(10, 3);"), 120.0);
    EXPECT_DOUBLE_EQ(evalScalar("nchoosek(20, 10);"), 184756.0);
}

TEST_P(CombinatoricsTest, NchoosekSymmetry)
{
    // C(n, k) == C(n, n-k).
    EXPECT_DOUBLE_EQ(evalScalar("nchoosek(15, 4);"),
                     evalScalar("nchoosek(15, 11);"));
    EXPECT_DOUBLE_EQ(evalScalar("nchoosek(30, 7);"),
                     evalScalar("nchoosek(30, 23);"));
}

TEST_P(CombinatoricsTest, NchoosekLargeValuesSurviveOverflow)
{
    // C(100, 50) ≈ 1.0089e29 — fits in double.
    const double v = evalScalar("nchoosek(100, 50);");
    EXPECT_NEAR(v, 1.00891344545564e29, 1e25);
}

TEST_P(CombinatoricsTest, NchoosekKExceedsNThrows)
{
    EXPECT_THROW(eval("nchoosek(5, 10);"), std::exception);
}

TEST_P(CombinatoricsTest, NchoosekNegativeThrows)
{
    EXPECT_THROW(eval("nchoosek(-3, 2);"), std::exception);
    EXPECT_THROW(eval("nchoosek(5, -1);"), std::exception);
}

TEST_P(CombinatoricsTest, NchoosekNonIntegerThrows)
{
    EXPECT_THROW(eval("nchoosek(5.5, 2);"), std::exception);
}

TEST_P(CombinatoricsTest, NchoosekVectorFormThrows)
{
    // Vector form nchoosek(v, k) is not yet supported.
    EXPECT_THROW(eval("nchoosek([1 2 3 4], 2);"), std::exception);
}

INSTANTIATE_DUAL(CombinatoricsTest);
