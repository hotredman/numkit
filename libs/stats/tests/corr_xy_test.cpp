// libs/stats/tests/corr_xy_test.cpp
//
// Regression guard for corr's two-argument matrix-vs-matrix form.
// Closes the ⚠️ gap.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class CorrXYTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(CorrXYTest, MatrixMatrix)
{
    eval("X = [1 2; 2 1; 3 3; 4 4; 5 5];"
         "Y = [10 100; 20 200; 30 300; 40 400; 50 500];"
         "C = corr(X, Y);");
    // Y columns are linear in row index (10*idx, 100*idx);
    // X col 1 = 1..5 → perfect corr = 1.
    // X col 2 = [2 1 3 4 5] vs linear → corr = 0.9.
    EXPECT_NEAR(evalScalar("C(1,1)"), 1.0, 1e-12);
    EXPECT_NEAR(evalScalar("C(1,2)"), 1.0, 1e-12);
    EXPECT_NEAR(evalScalar("C(2,1)"), 0.9, 1e-12);
    EXPECT_NEAR(evalScalar("C(2,2)"), 0.9, 1e-12);
    EXPECT_EQ(static_cast<int>(evalScalar("size(C,1)")), 2);
    EXPECT_EQ(static_cast<int>(evalScalar("size(C,2)")), 2);
}

TEST_F(CorrXYTest, ColumnVectors)
{
    eval("x = [1; 2; 3; 4; 5]; y = [2; 4; 6; 8; 10];"
         "c = corr(x, y);");
    EXPECT_NEAR(evalScalar("c"), 1.0, 1e-12);
}

TEST_F(CorrXYTest, AntiCorrelated)
{
    eval("x = [1; 2; 3; 4; 5]; y = [5; 4; 3; 2; 1];"
         "c = corr(x, y);");
    EXPECT_NEAR(evalScalar("c"), -1.0, 1e-12);
}

TEST_F(CorrXYTest, ConstantColumnReturnsNaN)
{
    eval("X = [1; 1; 1; 1; 1]; Y = [1; 2; 3; 4; 5];"
         "c = corr(X, Y);");
    EXPECT_TRUE(std::isnan(evalScalar("c")));
}

TEST_F(CorrXYTest, AsymmetricSizesMatrix)
{
    // X 4x3, Y 4x2 → output 3x2.
    eval("X = [1 2 3; 4 5 6; 7 8 9; 10 11 12];"
         "Y = [1 4; 2 3; 3 2; 4 1];"
         "C = corr(X, Y);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(C,1)")), 3);
    EXPECT_EQ(static_cast<int>(evalScalar("size(C,2)")), 2);
    EXPECT_NEAR(evalScalar("C(1,1)"),  1.0, 1e-12);
    EXPECT_NEAR(evalScalar("C(1,2)"), -1.0, 1e-12);
}

TEST_F(CorrXYTest, RowMismatchThrows)
{
    EXPECT_THROW(eval("corr([1; 2; 3], [1; 2]);"), std::exception);
}

// Single-arg path still works (regression).
TEST_F(CorrXYTest, SingleArgUnchanged)
{
    eval("X = [1 2; 2 1; 3 3; 4 4; 5 5];"
         "C = corr(X);");
    EXPECT_NEAR(evalScalar("C(1,1)"), 1.0, 1e-12);
    EXPECT_NEAR(evalScalar("C(1,2)"), 0.9, 1e-12);
    EXPECT_NEAR(evalScalar("C(2,1)"), 0.9, 1e-12);
    EXPECT_NEAR(evalScalar("C(2,2)"), 1.0, 1e-12);
}
