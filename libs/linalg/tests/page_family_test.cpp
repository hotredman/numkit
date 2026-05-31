// libs/linalg/tests/page_family_test.cpp
//
// Regression guard for the page-family: pageeig, pagesvd, pagepinv,
// pagenorm, pagemldivide, pagemrdivide, pagelsqminnorm.
// Spec-validated against MATLAB R2025b (tools/parity/specs/page_family.json).

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class PageFamilyTest : public ::testing::Test
{
public:
    Engine engine;
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// pagenorm: Frobenius per page, shape (1 × 1 × pages).
TEST_F(PageFamilyTest, PagenormFrobenius3DShape)
{
    eval("A = reshape(1:24, 2, 3, 4) + reshape(0:23, 2, 3, 4);"
         "n = pagenorm(A, 'fro');"
         "s = size(n);");
    EXPECT_EQ(static_cast<int>(evalScalar("s(1)")), 1);
    EXPECT_EQ(static_cast<int>(evalScalar("s(2)")), 1);
    EXPECT_EQ(static_cast<int>(evalScalar("s(3)")), 4);
}

// pagesvd: singular values column per page, shape (min(m,n) × 1 × pages).
TEST_F(PageFamilyTest, PagesvdShape)
{
    eval("A = reshape(1:24, 2, 3, 4) + reshape(0:23, 2, 3, 4);"
         "s = pagesvd(A); sz = size(s);");
    EXPECT_EQ(static_cast<int>(evalScalar("sz(1)")), 2);
    EXPECT_EQ(static_cast<int>(evalScalar("sz(3)")), 4);
}

// pagepinv: shape swap, n × m × pages.
TEST_F(PageFamilyTest, PagepinvShape)
{
    eval("A = reshape(1:24, 2, 3, 4) + reshape(0:23, 2, 3, 4);"
         "P = pagepinv(A); sz = size(P);");
    EXPECT_EQ(static_cast<int>(evalScalar("sz(1)")), 3);
    EXPECT_EQ(static_cast<int>(evalScalar("sz(2)")), 2);
    EXPECT_EQ(static_cast<int>(evalScalar("sz(3)")), 4);
}

// pageeig: symmetric input → real eigenvalues. eye*5 has all 5s.
TEST_F(PageFamilyTest, PageeigSymmetricExactValues)
{
    eval("S = zeros(3, 3, 2);"
         "S(:,:,1) = [4 1 0; 1 3 0; 0 0 2];"
         "S(:,:,2) = eye(3) * 5;"
         "e = pageeig(S);");
    EXPECT_DOUBLE_EQ(evalScalar("e(1,1,2)"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("e(2,1,2)"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("e(3,1,2)"), 5.0);
}

// pageinv: per-page LU solve against identity.
TEST_F(PageFamilyTest, PageinvSatisfiesIdentity)
{
    eval("A = repmat([2 0; 0 3], [1 1 2]);"
         "P = pageinv(A);"
         "v00 = P(1,1,1); v11 = P(2,2,1);"
         "v00b = P(1,1,2); v11b = P(2,2,2);");
    EXPECT_DOUBLE_EQ(evalScalar("v00"), 0.5);
    EXPECT_DOUBLE_EQ(evalScalar("v11"), 1.0 / 3.0);
}

// pagemldivide with diag(2,3) → X(:,:,p) = diag(0.5, 1/3) * B(:,:,p).
TEST_F(PageFamilyTest, PagemldivideDiagonal)
{
    eval("A = repmat([2 0; 0 3], [1 1 2]);"
         "B = reshape(1:8, 2, 2, 2);"
         "X = pagemldivide(A, B);");
    EXPECT_DOUBLE_EQ(evalScalar("X(1,1,1)"), 0.5);
    EXPECT_DOUBLE_EQ(evalScalar("X(2,1,1)"), 2.0 / 3.0);
}

// 2-D input → short-circuit to 2-D linalg op (same value as direct call).
TEST_F(PageFamilyTest, TwoDInputShortCircuits)
{
    eval("A = [1 2; 3 4];"
         "n_page = pagenorm(A, 'fro');"
         "n_dir  = norm(A, 'fro');");
    EXPECT_DOUBLE_EQ(evalScalar("n_page"), evalScalar("n_dir"));
}
