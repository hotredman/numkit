// libs/stats/tests/rng_size_test.cpp
// the 14 stats.dist RNG functions.
// Closes: betarnd, binornd, chi2rnd, exprnd, frnd, gamrnd, lognrnd,
// normrnd, poissrnd, raylrnd, trnd, unidrnd, unifrnd, wblrnd.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class RngSizeTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// 2026-05-08 — gap closure: vector-size form `<rnd>(..., [m n])`
// previously threw "Cannot convert double to scalar". Now uses shared
// `parse_rng_size` helper.

TEST_F(RngSizeTest, NormrndVectorSize)
{
    eval("a = normrnd(0, 1, [2 5]);");
    EXPECT_DOUBLE_EQ(evalScalar("size(a, 1)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(a, 2)"), 5.0);
}

TEST_F(RngSizeTest, NormrndScalarShortcut)
{
    eval("a = normrnd(0, 1, 4);");  // n×n
    EXPECT_DOUBLE_EQ(evalScalar("size(a, 1)"), 4.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(a, 2)"), 4.0);
}

TEST_F(RngSizeTest, NormrndScalarMxN)
{
    eval("a = normrnd(0, 1, 3, 7);");
    EXPECT_DOUBLE_EQ(evalScalar("size(a, 1)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(a, 2)"), 7.0);
}

#define VECTOR_SZ_TEST(NAME, EXPR)                                           \
    TEST_F(RngSizeTest, NAME)                                                \
    {                                                                        \
        eval("a = " EXPR ";");                                               \
        EXPECT_DOUBLE_EQ(evalScalar("size(a, 1)"), 2.0);                    \
        EXPECT_DOUBLE_EQ(evalScalar("size(a, 2)"), 3.0);                    \
    }

VECTOR_SZ_TEST(BetarndVecSz,   "betarnd(2, 5, [2 3])")
VECTOR_SZ_TEST(BinorndVecSz,   "binornd(10, 0.5, [2 3])")
VECTOR_SZ_TEST(Chi2rndVecSz,   "chi2rnd(5, [2 3])")
VECTOR_SZ_TEST(ExprndVecSz,    "exprnd(1, [2 3])")
VECTOR_SZ_TEST(FrndVecSz,      "frnd(5, 10, [2 3])")
VECTOR_SZ_TEST(GamrndVecSz,    "gamrnd(2, 1, [2 3])")
VECTOR_SZ_TEST(LognrndVecSz,   "lognrnd(0, 1, [2 3])")
VECTOR_SZ_TEST(PoissrndVecSz,  "poissrnd(2, [2 3])")
VECTOR_SZ_TEST(RaylrndVecSz,   "raylrnd(1, [2 3])")
VECTOR_SZ_TEST(TrndVecSz,      "trnd(5, [2 3])")
VECTOR_SZ_TEST(UnidrndVecSz,   "unidrnd(10, [2 3])")
VECTOR_SZ_TEST(UnifrndVecSz,   "unifrnd(0, 1, [2 3])")
VECTOR_SZ_TEST(WblrndVecSz,    "wblrnd(1, 1, [2 3])")
