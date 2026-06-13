// toolboxes/linalg/tests/page_lsq_mrdiv_test.cpp
//
// Coverage for the page-wise linear-algebra ops that shipped parity-only:
// pagemrdivide (B/A per page) and pagelsqminnorm (minimum-norm least squares
// per page). Each reduces to its 2-D counterpart on a single page; the 3-D
// cases exercise the page loop with independently-verifiable pages.

#include "dual_engine_fixture.hpp"

using namespace m_test;

class PageLsqMrdivTest : public DualEngineTest
{};

// ── pagemrdivide: B/A. Diagonal A/B = diag ratios. ──────────────────────
TEST_P(PageLsqMrdivTest, PagemrdivideSinglePage)
{
    eval("X = pagemrdivide([1 0; 0 2], [2 0; 0 2]);");   // [1 0;0 2]*inv([2 0;0 2])
    EXPECT_NEAR(evalScalar("X(1,1)"), 0.5, 1e-12);
    EXPECT_NEAR(evalScalar("X(2,2)"), 1.0, 1e-12);
    EXPECT_NEAR(evalScalar("X(1,2)"), 0.0, 1e-12);
}

TEST_P(PageLsqMrdivTest, PagemrdivideStacked)
{
    eval("A = cat(3, [1 0; 0 2], [4 0; 0 4]); B = cat(3, [2 0; 0 2], [2 0; 0 2]);");
    eval("X = pagemrdivide(A, B);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(X,3)")), 2);
    EXPECT_NEAR(evalScalar("X(1,1,1)"), 0.5, 1e-12);   // page 1
    EXPECT_NEAR(evalScalar("X(2,2,1)"), 1.0, 1e-12);
    EXPECT_NEAR(evalScalar("X(1,1,2)"), 2.0, 1e-12);   // page 2
    EXPECT_NEAR(evalScalar("X(2,2,2)"), 2.0, 1e-12);
}

// ── pagelsqminnorm: minimum-norm least squares per page ─────────────────
TEST_P(PageLsqMrdivTest, PagelsqminnormSinglePage)
{
    // [1;1] x ~= [2;4] -> least squares x = mean = 3.
    EXPECT_NEAR(evalScalar("pagelsqminnorm([1;1], [2;4])"), 3.0, 1e-12);
    // Square solve [1 2;3 4] x = [5;6] -> [-4; 4.5].
    eval("x = pagelsqminnorm([1 2; 3 4], [5; 6]);");
    EXPECT_NEAR(evalScalar("x(1)"), -4.0, 1e-12);
    EXPECT_NEAR(evalScalar("x(2)"), 4.5, 1e-12);
}

TEST_P(PageLsqMrdivTest, PagelsqminnormStacked)
{
    eval("A = cat(3, [1;1], [1;1]); B = cat(3, [2;4], [10;20]);");
    eval("X = pagelsqminnorm(A, B);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(X,3)")), 2);
    EXPECT_NEAR(evalScalar("X(1,1,1)"), 3.0, 1e-12);    // mean(2,4)
    EXPECT_NEAR(evalScalar("X(1,1,2)"), 15.0, 1e-12);   // mean(10,20)
}

INSTANTIATE_DUAL(PageLsqMrdivTest);
