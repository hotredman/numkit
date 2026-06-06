// libs/image/tests/graydist_test.cpp
//
// Regression guard for graydist — gray-weighted geodesic distance
// transform. Bit-equal MATLAB R2025b at 1e-10 tolerance.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class GraydistTest : public ::testing::Test
{
public:
    StdEngine engine;
    void SetUp() override {
        engine.eval(
            "import compat.*;"
            "A = [1 2 3 4; 2 11 12 2; 3 13 14 3; 4 15 16 4];"
            "seed = false(4,4); seed(1,1) = true;");
    }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// ── cityblock distance ────────────────────────────────────────────

TEST_F(GraydistTest, Cityblock)
{
    eval("D = graydist(A, seed, 'cityblock');");
    EXPECT_NEAR(evalScalar("D(2,2)"), 8.0,  1e-10);
    EXPECT_NEAR(evalScalar("D(3,3)"), 21.5, 1e-10);
    EXPECT_NEAR(evalScalar("D(4,4)"), 16.5, 1e-10);
}

// ── chessboard (default) ─────────────────────────────────────────

TEST_F(GraydistTest, Chessboard)
{
    eval("D = graydist(A, seed, 'chessboard');");
    EXPECT_NEAR(evalScalar("D(2,2)"), 6.0,  1e-10);
    EXPECT_NEAR(evalScalar("D(3,3)"), 14.5, 1e-10);
}

// ── quasi-euclidean ───────────────────────────────────────────────

TEST_F(GraydistTest, QuasiEuclidean)
{
    eval("D = graydist(A, seed, 'quasi-euclidean');");
    EXPECT_NEAR(evalScalar("D(2,2)"), 8.0, 1e-10);
    EXPECT_NEAR(evalScalar("D(3,3)"), 18.535533905, 1e-8);
}

// ── default method == chessboard ─────────────────────────────────

TEST_F(GraydistTest, DefaultIsChessboard)
{
    eval("D1 = graydist(A, seed); D2 = graydist(A, seed, 'chessboard');"
         "delta = max(abs(D1(:) - D2(:)));");
    EXPECT_NEAR(evalScalar("delta"), 0.0, 1e-12);
}

// ── (C, R) form gives same result as mask ────────────────────────

TEST_F(GraydistTest, ColRowForm)
{
    eval("Dm = graydist(A, seed, 'cityblock');"
         "Dcr = graydist(A, 1, 1, 'cityblock');"
         "delta = max(abs(Dm(:) - Dcr(:)));");
    EXPECT_NEAR(evalScalar("delta"), 0.0, 1e-12);
}

// ── Linear-index form gives same result ──────────────────────────

TEST_F(GraydistTest, IndForm)
{
    eval("Dm = graydist(A, seed, 'cityblock');"
         "Did = graydist(A, 1, 'cityblock');"
         "delta = max(abs(Dm(:) - Did(:)));");
    EXPECT_NEAR(evalScalar("delta"), 0.0, 1e-12);
}

// ── Multi-seed reaches every pixel ───────────────────────────────

TEST_F(GraydistTest, MultiSeed)
{
    eval("s2 = false(4,4); s2(1,1)=true; s2(4,4)=true;"
         "D = graydist(A, s2, 'cityblock');");
    EXPECT_NEAR(evalScalar("D(3,3)"), 12.0, 1e-10);
    EXPECT_NEAR(evalScalar("D(4,3)"), 10.0, 1e-10);
    EXPECT_NEAR(evalScalar("D(4,4)"), 0.0,  1e-12);
}

// ── uint8 input → single output ───────────────────────────────────

TEST_F(GraydistTest, Uint8Class)
{
    eval("Au = uint8(A); D = graydist(Au, seed, 'cityblock');");
    EXPECT_NEAR(evalScalar("D(2,2)"), 8.0, 1e-6);
    EXPECT_EQ(eval("class(D)").toString(), "single");
}

// ── Seed is itself zero ──────────────────────────────────────────

TEST_F(GraydistTest, SeedIsZero)
{
    eval("D = graydist(A, seed, 'cityblock');");
    EXPECT_NEAR(evalScalar("D(1,1)"), 0.0, 1e-12);
}

// ── Errors ─────────────────────────────────────────────────────────

TEST_F(GraydistTest, BadMethodThrows)
{
    EXPECT_THROW(eval("graydist(A, seed, 'wrong');"), std::exception);
}

TEST_F(GraydistTest, MaskWrongSizeThrows)
{
    EXPECT_THROW(eval("graydist(A, false(2,2));"), std::exception);
}
