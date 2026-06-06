// libs/image/tests/nlfilter_test.cpp
//
// Regression guard for nlfilter — general sliding-neighbourhood
// operation. Reference values verified bit-equal against MATLAB
// R2025b on magic(5) across mean/max/median/sum kernels, [3 3] and
// [2 3] neighbourhoods, double/uint8 classes, and 'indexed' mode.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class NlfilterTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*; A = magic(5);"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(NlfilterTest, Mean3x3)
{
    eval("B = nlfilter(A, [3 3], @(x) mean(x(:)));");
    // From MATLAB reference: B(1,1) = (0+0+0 + 0+17+24 + 0+23+5) / 9 = 69/9
    EXPECT_NEAR(evalScalar("B(1,1)"), 69.0 / 9.0, 1e-12);
    EXPECT_NEAR(evalScalar("B(3,3)"), 13.0,        1e-12);
    EXPECT_NEAR(evalScalar("B(5,5)"), 35.0 / 9.0,  1e-12);
}

TEST_F(NlfilterTest, Max3x3)
{
    eval("B = nlfilter(A, [3 3], @(x) max(x(:)));");
    EXPECT_DOUBLE_EQ(evalScalar("B(1,1)"), 24.0);
    EXPECT_DOUBLE_EQ(evalScalar("B(3,3)"), 21.0);
    EXPECT_DOUBLE_EQ(evalScalar("B(5,5)"), 21.0);
}

TEST_F(NlfilterTest, MedianEqualsMedfilt2Reference)
{
    eval("B = nlfilter(A, [3 3], @(x) median(x(:)));");
    EXPECT_DOUBLE_EQ(evalScalar("B(1,1)"),  0.0);
    EXPECT_DOUBLE_EQ(evalScalar("B(3,3)"), 13.0);
    EXPECT_DOUBLE_EQ(evalScalar("B(5,5)"),  0.0);
}

TEST_F(NlfilterTest, EvenNeighbourhoodSum2x3)
{
    eval("B = nlfilter(A, [2 3], @(x) sum(x(:)));");
    EXPECT_DOUBLE_EQ(evalScalar("B(1,1)"), 69.0);
    EXPECT_DOUBLE_EQ(evalScalar("B(2,3)"), 65.0);
    EXPECT_DOUBLE_EQ(evalScalar("B(5,5)"), 11.0);
}

TEST_F(NlfilterTest, Uint8ClassPreservation)
{
    eval("B = nlfilter(uint8(A), [3 3], @(x) uint8(mean(x(:))));");
    EXPECT_EQ(eval("B").type(), ValueType::UINT8);
    EXPECT_EQ(static_cast<int>(evalScalar("B(1,1)")),  8);
    EXPECT_EQ(static_cast<int>(evalScalar("B(3,3)")), 13);
    EXPECT_EQ(static_cast<int>(evalScalar("B(5,5)")),  4);
}

TEST_F(NlfilterTest, IndexedModeDoublePadOne)
{
    // With 'indexed' + double input, pad value is 1.  min over 3x3
    // at corners therefore comes out as 1.
    eval("B = nlfilter(A, 'indexed', [3 3], @(x) min(x(:)));");
    EXPECT_DOUBLE_EQ(evalScalar("B(1,1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("B(5,5)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("B(3,3)"), 5.0);   // interior unaffected
}

TEST_F(NlfilterTest, IndexedModeUint8PadZero)
{
    // For uint8 the indexed padval is 0 — so result equals default mode.
    eval("Bi = double(nlfilter(uint8(A), 'indexed', [3 3], @(x) max(x(:))));"
         "Bd = double(nlfilter(uint8(A), [3 3], @(x) max(x(:))));"
         "d = max(max(abs(Bi - Bd)));");
    EXPECT_DOUBLE_EQ(evalScalar("d"), 0.0);
}

TEST_F(NlfilterTest, OutputShapeMatchesInput)
{
    eval("B = nlfilter(A, [3 3], @(x) mean(x(:)));");
    EXPECT_EQ(static_cast<int>(evalScalar("size(B,1)")), 5);
    EXPECT_EQ(static_cast<int>(evalScalar("size(B,2)")), 5);
}

TEST_F(NlfilterTest, BadArgumentsThrow)
{
    EXPECT_THROW(eval("nlfilter(A, [3 3]);"),     std::exception);
    EXPECT_THROW(eval("nlfilter(A, [3], @sum);"), std::exception);
    EXPECT_THROW(eval("nlfilter(A, [0 3], @sum);"),std::exception);
    EXPECT_THROW(eval("nlfilter(A, 'unknown', [3 3], @sum);"),
                 std::exception);
}

TEST_F(NlfilterTest, FunReturningNonScalarThrows)
{
    EXPECT_THROW(eval("nlfilter(A, [3 3], @(x) x(:));"), std::exception);
}
