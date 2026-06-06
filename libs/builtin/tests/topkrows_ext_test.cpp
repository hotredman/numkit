// libs/builtin/tests/topkrows_ext_test.cpp
//
// Regression guard for topkrows extensions: col selector, direction
// ('ascend'/'descend'), 2-output index, ComparisonMethod (accept-only).
// The classic (A, k) form is covered in quickwins5_test.cpp.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class TopkRowsExtTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override {
        engine.eval("A = [3 1; 1 2; 4 5; 4 3; 2 0];");
    }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(TopkRowsExtTest, ColSingle)
{
    eval("B = topkrows(A, 2, 1);");
    // col 1 desc → rows with col1=4 (idx 3, 4). Stable: row 3 first
    // (1-indexed), which has [4 5]. Then row 4 = [4 3].
    EXPECT_NEAR(evalScalar("B(1,1)"), 4.0, 1e-12);
    EXPECT_NEAR(evalScalar("B(1,2)"), 5.0, 1e-12);
    EXPECT_NEAR(evalScalar("B(2,1)"), 4.0, 1e-12);
    EXPECT_NEAR(evalScalar("B(2,2)"), 3.0, 1e-12);
}

TEST_F(TopkRowsExtTest, ColVector)
{
    // Sort by col 2 (primary) then col 1.
    eval("B = topkrows(A, 3, [2 1]);");
    EXPECT_NEAR(evalScalar("B(1,1)"), 4.0, 1e-12);
    EXPECT_NEAR(evalScalar("B(1,2)"), 5.0, 1e-12);
    EXPECT_NEAR(evalScalar("B(2,1)"), 4.0, 1e-12);
    EXPECT_NEAR(evalScalar("B(2,2)"), 3.0, 1e-12);
    EXPECT_NEAR(evalScalar("B(3,1)"), 1.0, 1e-12);
    EXPECT_NEAR(evalScalar("B(3,2)"), 2.0, 1e-12);
}

TEST_F(TopkRowsExtTest, DirectionAscend)
{
    eval("B = topkrows(A, 2, 1, 'ascend');");
    EXPECT_NEAR(evalScalar("B(1,1)"), 1.0, 1e-12);
    EXPECT_NEAR(evalScalar("B(1,2)"), 2.0, 1e-12);
    EXPECT_NEAR(evalScalar("B(2,1)"), 2.0, 1e-12);
    EXPECT_NEAR(evalScalar("B(2,2)"), 0.0, 1e-12);
}

TEST_F(TopkRowsExtTest, DirectionDescend)
{
    eval("B = topkrows(A, 2, 1, 'descend');");
    EXPECT_NEAR(evalScalar("B(1,1)"), 4.0, 1e-12);
    EXPECT_NEAR(evalScalar("B(2,1)"), 4.0, 1e-12);
}

TEST_F(TopkRowsExtTest, IndexOutput)
{
    eval("[B, I] = topkrows(A, 2);");
    // Default desc lex → first hit [4 5] @ row 3, then [4 3] @ row 4.
    EXPECT_EQ(static_cast<int>(evalScalar("I(1)")), 3);
    EXPECT_EQ(static_cast<int>(evalScalar("I(2)")), 4);
    EXPECT_EQ(static_cast<int>(evalScalar("size(I,2)")), 1);
}

TEST_F(TopkRowsExtTest, KGreaterThanRows)
{
    eval("B = topkrows(A, 100);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(B,1)")), 5);
    EXPECT_EQ(static_cast<int>(evalScalar("size(B,2)")), 2);
}

TEST_F(TopkRowsExtTest, KZero)
{
    eval("B = topkrows(A, 0);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(B,1)")), 0);
    EXPECT_EQ(static_cast<int>(evalScalar("size(B,2)")), 2);
}

TEST_F(TopkRowsExtTest, ComparisonMethodAcceptIgnore)
{
    // ComparisonMethod NV is accepted-and-ignored (numkit only does real).
    eval("B = topkrows(A, 2, 1, 'ComparisonMethod', 'auto');");
    EXPECT_NEAR(evalScalar("B(1,1)"), 4.0, 1e-12);
}

TEST_F(TopkRowsExtTest, BadColThrows)
{
    EXPECT_THROW(eval("topkrows(A, 2, 99);"), std::exception);
}

TEST_F(TopkRowsExtTest, BadDirectionThrows)
{
    EXPECT_THROW(eval("topkrows(A, 2, 1, 'gibberish');"), std::exception);
}
