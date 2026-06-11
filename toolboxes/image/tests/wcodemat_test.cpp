// toolboxes/image/tests/wcodemat_test.cpp
// wcodemat.

#include <numkit/core/engine.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class WcodematTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// MATLAB R2025b formula: y = floor((v - mn) / span * nb) + 1, with the
// upper edge (v == mx) clamped from nb+1 down to nb.

TEST_F(WcodematTest, DefaultMatNb16Absol1)
{
    eval("M = [1 -2 3; 4 -5 6]; y = wcodemat(M);");
    EXPECT_DOUBLE_EQ(evalScalar("y(1,1)"),  1.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(1,2)"),  4.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(2,3)"), 16.0);
}

TEST_F(WcodematTest, Nb4QuantizesCorrectly)
{
    // Bug fix 2026-05-08: previous impl used `round` and `(nb-1)`,
    // producing wcodemat(M, 4) = [1 2 2; 3 3 4]. MATLAB returns
    // [1 1 2; 3 4 4] using floor and `nb`.
    eval("M = [1 -2 3; 4 -5 6]; y = wcodemat(M, 4);");
    EXPECT_DOUBLE_EQ(evalScalar("y(1,1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(1,2)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(1,3)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(2,1)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(2,2)"), 4.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(2,3)"), 4.0);
}

TEST_F(WcodematTest, AbsolZeroSignedRescale)
{
    // absol=0: no abs(); use signed values around mn..mx.
    eval("M = [1 -2 3; 4 -5 6]; y = wcodemat(M, 16, 'mat', 0);");
    EXPECT_DOUBLE_EQ(evalScalar("y(1,1)"),  9.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(2,1)"), 14.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(2,3)"), 16.0);
}

TEST_F(WcodematTest, RowOption)
{
    eval("M = [1 -2 3; 4 -5 6]; y = wcodemat(M, 16, 'row');");
    // Each row min->1, max->16.
    EXPECT_DOUBLE_EQ(evalScalar("y(1,1)"),  1.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(1,3)"), 16.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(2,1)"),  1.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(2,3)"), 16.0);
}

TEST_F(WcodematTest, ColOption)
{
    eval("M = [1 -2 3; 4 -5 6]; y = wcodemat(M, 16, 'col');");
    // Each column min->1, max->16.
    EXPECT_DOUBLE_EQ(evalScalar("y(1,1)"),  1.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(2,1)"), 16.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(2,3)"), 16.0);
}

TEST_F(WcodematTest, VectorEdges)
{
    eval("y = wcodemat([1 2 3 4 5]);");
    EXPECT_DOUBLE_EQ(evalScalar("y(1)"),  1.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(5)"), 16.0);
}
