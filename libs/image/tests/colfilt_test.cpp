// libs/image/tests/colfilt_test.cpp
//
// Regression guard for colfilt — column-wise neighbourhood operation.
// Reference values verified bit-equal MATLAB R2025b on magic(5) for
// sliding mode and on magic(6) for distinct mode.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class ColfiltTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override {
        engine.eval("import compat.*; A = magic(5); A6 = magic(6);");
    }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(ColfiltTest, SlidingMean3x3)
{
    eval("B = colfilt(A, [3 3], 'sliding', @mean);");
    EXPECT_NEAR(evalScalar("B(1,1)"), 69.0 / 9.0, 1e-12);
    EXPECT_NEAR(evalScalar("B(3,3)"), 13.0,        1e-12);
    EXPECT_NEAR(evalScalar("B(5,5)"), 35.0 / 9.0,  1e-12);
}

TEST_F(ColfiltTest, SlidingSum3x3)
{
    eval("B = colfilt(A, [3 3], 'sliding', @sum);");
    EXPECT_DOUBLE_EQ(evalScalar("B(1,1)"),  69.0);
    EXPECT_DOUBLE_EQ(evalScalar("B(3,3)"), 117.0);
    EXPECT_DOUBLE_EQ(evalScalar("B(5,5)"),  35.0);
}

TEST_F(ColfiltTest, SlidingEvenNeighbourhood2x3)
{
    eval("B = colfilt(A, [2 3], 'sliding', @sum);");
    EXPECT_DOUBLE_EQ(evalScalar("B(1,1)"), 69.0);
    EXPECT_DOUBLE_EQ(evalScalar("B(2,3)"), 65.0);
    EXPECT_DOUBLE_EQ(evalScalar("B(5,5)"), 11.0);
}

TEST_F(ColfiltTest, IndexedSlidingMin)
{
    eval("B = colfilt(A, 'indexed', [3 3], 'sliding', @min);");
    EXPECT_DOUBLE_EQ(evalScalar("B(1,1)"), 1.0);   // padval 1 dominates
    EXPECT_DOUBLE_EQ(evalScalar("B(3,3)"), 5.0);   // interior
    EXPECT_DOUBLE_EQ(evalScalar("B(5,5)"), 1.0);   // padval 1 dominates
}

TEST_F(ColfiltTest, DistinctShapePreserving)
{
    eval("B = colfilt(A6, [2 2], 'distinct', @(x) x.^2);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(B,1)")), 6);
    EXPECT_EQ(static_cast<int>(evalScalar("size(B,2)")), 6);
    // magic(6)(1,1) = 35 → 35^2 = 1225 (1225 because magic(6) starts
    // with [35 1 6 ...]).
    EXPECT_DOUBLE_EQ(evalScalar("B(1,1)"), 1225.0);
}

TEST_F(ColfiltTest, NlfilterEquivalenceForSum)
{
    eval("Bn = nlfilter(A, [3 3], @(x) sum(x(:)));"
         "Bc = colfilt(A, [3 3], 'sliding', @sum);"
         "d  = max(max(abs(Bn - Bc)));");
    EXPECT_DOUBLE_EQ(evalScalar("d"), 0.0);
}

TEST_F(ColfiltTest, MblockArgumentIsIgnoredAsMemoryOptimisation)
{
    // [mblock nblock] is purely a memory-optimisation per MATLAB docs.
    // Output must be identical to the no-block form.
    eval("B1 = colfilt(A, [3 3], [2 2], 'sliding', @sum);"
         "B2 = colfilt(A, [3 3], 'sliding', @sum);"
         "d  = max(max(abs(B1 - B2)));");
    EXPECT_DOUBLE_EQ(evalScalar("d"), 0.0);
}

TEST_F(ColfiltTest, BadBlockTypeThrows)
{
    EXPECT_THROW(eval("colfilt(A, [3 3], 'not-a-type', @sum);"),
                 std::exception);
}

TEST_F(ColfiltTest, BadNargin)
{
    EXPECT_THROW(eval("colfilt(A, [3 3]);"),  std::exception);
    EXPECT_THROW(eval("colfilt(A, [3 3], 'sliding');"), std::exception);
}

TEST_F(ColfiltTest, SlidingFunShapeMismatchThrows)
{
    EXPECT_THROW(eval("colfilt(A, [3 3], 'sliding', @(x) x);"),
                 std::exception);
}
