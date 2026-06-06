// libs/builtin/tests/magic_test.cpp
//
// Regression guard for builtin::magic. Hardcoded expected values
// captured from MATLAB R2025b parity probe.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class MagicTest : public ::testing::Test
{
public:
    StdEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// N == 1: degenerate -- single cell [1].
TEST_F(MagicTest, N1)
{
    eval("M = magic(1);");
    EXPECT_EQ(static_cast<int>(evalScalar("numel(M)")), 1);
    EXPECT_DOUBLE_EQ(evalScalar("M(1,1)"), 1.0);
}

// N == 2: MATLAB convention [1 3; 4 2] -- not strictly magic.
TEST_F(MagicTest, N2_MatlabConvention)
{
    eval("M = magic(2);");
    EXPECT_DOUBLE_EQ(evalScalar("M(1,1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("M(1,2)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("M(2,1)"), 4.0);
    EXPECT_DOUBLE_EQ(evalScalar("M(2,2)"), 2.0);
}

// N == 3 (odd / Siamese branch). Magic constant = 3*(9+1)/2 = 15.
TEST_F(MagicTest, N3_Odd)
{
    eval("M = magic(3);");
    // Hardcoded MATLAB R2025b values.
    EXPECT_DOUBLE_EQ(evalScalar("M(1,1)"), 8.0);
    EXPECT_DOUBLE_EQ(evalScalar("M(2,2)"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("M(3,3)"), 2.0);
    // Magic property: every row, every col, both diagonals sum to 15.
    EXPECT_DOUBLE_EQ(evalScalar("sum(M(1,:))"),       15.0);
    EXPECT_DOUBLE_EQ(evalScalar("sum(M(2,:))"),       15.0);
    EXPECT_DOUBLE_EQ(evalScalar("sum(M(3,:))"),       15.0);
    EXPECT_DOUBLE_EQ(evalScalar("sum(M(:,1))"),       15.0);
    EXPECT_DOUBLE_EQ(evalScalar("sum(diag(M))"),      15.0);
    EXPECT_DOUBLE_EQ(evalScalar("sum(diag(fliplr(M)))"), 15.0);
}

// N == 4 (doubly-even branch). Magic constant = 4*17/2 = 34.
TEST_F(MagicTest, N4_DoublyEven)
{
    eval("M = magic(4);");
    EXPECT_DOUBLE_EQ(evalScalar("M(1,1)"), 16.0);
    EXPECT_DOUBLE_EQ(evalScalar("M(1,4)"), 13.0);
    EXPECT_DOUBLE_EQ(evalScalar("M(4,1)"),  4.0);
    EXPECT_DOUBLE_EQ(evalScalar("M(4,4)"),  1.0);
    EXPECT_DOUBLE_EQ(evalScalar("sum(M(1,:))"),         34.0);
    EXPECT_DOUBLE_EQ(evalScalar("sum(M(:,2))"),         34.0);
    EXPECT_DOUBLE_EQ(evalScalar("sum(diag(M))"),        34.0);
    EXPECT_DOUBLE_EQ(evalScalar("sum(diag(fliplr(M)))"),34.0);
}

// N == 6 (singly-even / Strachey branch). Magic constant = 6*37/2 = 111.
TEST_F(MagicTest, N6_SinglyEven)
{
    eval("M = magic(6);");
    // Magic property is the strong invariant for the Strachey branch.
    for (int r = 1; r <= 6; ++r) {
        EXPECT_DOUBLE_EQ(evalScalar("sum(M(" + std::to_string(r) + ",:))"), 111.0);
        EXPECT_DOUBLE_EQ(evalScalar("sum(M(:," + std::to_string(r) + "))"), 111.0);
    }
    EXPECT_DOUBLE_EQ(evalScalar("sum(diag(M))"),        111.0);
    EXPECT_DOUBLE_EQ(evalScalar("sum(diag(fliplr(M)))"),111.0);
}

// N == 8 (doubly-even). Magic constant = 8*65/2 = 260.
TEST_F(MagicTest, N8_DoublyEvenLarger)
{
    eval("M = magic(8);");
    for (int r = 1; r <= 8; ++r) {
        EXPECT_DOUBLE_EQ(evalScalar("sum(M(" + std::to_string(r) + ",:))"), 260.0);
        EXPECT_DOUBLE_EQ(evalScalar("sum(M(:," + std::to_string(r) + "))"), 260.0);
    }
    EXPECT_DOUBLE_EQ(evalScalar("sum(diag(M))"),        260.0);
    EXPECT_DOUBLE_EQ(evalScalar("sum(diag(fliplr(M)))"),260.0);
}

// N == 10 (singly-even, K=2). Magic constant = 10*101/2 = 505.
TEST_F(MagicTest, N10_SinglyEvenLarger)
{
    eval("M = magic(10);");
    for (int r = 1; r <= 10; ++r)
        EXPECT_DOUBLE_EQ(evalScalar("sum(M(" + std::to_string(r) + ",:))"), 505.0);
    EXPECT_DOUBLE_EQ(evalScalar("sum(diag(M))"),        505.0);
    EXPECT_DOUBLE_EQ(evalScalar("sum(diag(fliplr(M)))"),505.0);
}

// Permutation: magic(N) must contain each integer 1..N² exactly once.
TEST_F(MagicTest, N5_Permutation)
{
    eval("M = magic(5);");
    EXPECT_DOUBLE_EQ(evalScalar("min(M(:))"),   1.0);
    EXPECT_DOUBLE_EQ(evalScalar("max(M(:))"),  25.0);
    EXPECT_DOUBLE_EQ(evalScalar("sum(M(:))"),  325.0);  // 25*26/2
    EXPECT_DOUBLE_EQ(evalScalar("numel(unique(M(:)))"), 25.0);
}

// Negative / non-integer N rejected.
TEST_F(MagicTest, BadInputsRejected)
{
    EXPECT_THROW(eval("magic(-1);"), std::exception);
    EXPECT_THROW(eval("magic(3.5);"), std::exception);
}

// N == 0 is the empty matrix.
TEST_F(MagicTest, N0_Empty)
{
    eval("M = magic(0);");
    EXPECT_EQ(static_cast<int>(evalScalar("numel(M)")), 0);
}
