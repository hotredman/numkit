// tests/stdlib/concat_test.cpp — Colon ranges, horzcat, vertcat
// Parameterized: runs on both TreeWalker and VM backends

#include "dual_engine_fixture.hpp"

using namespace mlab_test;

// ============================================================
// Concatenation and colon range operations
// ============================================================

class ConcatTest : public DualEngineTest
{};

TEST_P(ConcatTest, ColonRangeUnitStep)
{
    eval("v = 1:5;");
    auto *v = getVarPtr("v");
    ASSERT_EQ(v->numel(), 5u);
    for (size_t i = 0; i < 5; ++i)
        EXPECT_DOUBLE_EQ(v->doubleData()[i], 1.0 + i);
}

TEST_P(ConcatTest, ColonRangeFractionalStep)
{
    eval("v = 0:0.5:2;");
    auto *v = getVarPtr("v");
    ASSERT_EQ(v->numel(), 5u);
    EXPECT_DOUBLE_EQ(v->doubleData()[0], 0.0);
    EXPECT_DOUBLE_EQ(v->doubleData()[4], 2.0);
}

TEST_P(ConcatTest, ColonRangeNegativeStep)
{
    eval("v = 10:-3:1;");
    auto *v = getVarPtr("v");
    ASSERT_EQ(v->numel(), 4u);
    EXPECT_DOUBLE_EQ(v->doubleData()[0], 10.0);
    EXPECT_DOUBLE_EQ(v->doubleData()[3], 1.0);
}

TEST_P(ConcatTest, ColonRangeEmptyResult)
{
    eval("v = 5:1;");
    auto *v = getVarPtr("v");
    EXPECT_EQ(v->numel(), 0u);
}

TEST_P(ConcatTest, ColonRangeZeroStepError)
{
    auto r = engine.evalSafe("v = 1:0:10;");
    EXPECT_FALSE(r.ok);
}

TEST_P(ConcatTest, ColonRangeLastElementCorrection)
{
    // 0:0.3:1 — last element should be exactly 0.9, not 0.9000000000000001
    eval("v = 0:0.3:1;");
    auto *v = getVarPtr("v");
    ASSERT_EQ(v->numel(), 4u);
    EXPECT_DOUBLE_EQ(v->doubleData()[3], 0.9);
}

TEST_P(ConcatTest, ColonRangeSingleElement)
{
    eval("v = 5:5;");
    auto *v = getVarPtr("v");
    ASSERT_EQ(v->numel(), 1u);
    EXPECT_DOUBLE_EQ(v->doubleData()[0], 5.0);
}

TEST_P(ConcatTest, HorzcatScalars)
{
    eval("v = [1, 2, 3];");
    auto *v = getVarPtr("v");
    EXPECT_EQ(rows(*v), 1u);
    EXPECT_EQ(cols(*v), 3u);
    EXPECT_DOUBLE_EQ(v->doubleData()[0], 1.0);
    EXPECT_DOUBLE_EQ(v->doubleData()[2], 3.0);
}

TEST_P(ConcatTest, HorzcatVectors)
{
    eval("a = [1 2]; b = [3 4 5]; v = [a, b];");
    auto *v = getVarPtr("v");
    EXPECT_EQ(v->numel(), 5u);
    EXPECT_DOUBLE_EQ(v->doubleData()[4], 5.0);
}

TEST_P(ConcatTest, HorzcatMatrices)
{
    eval("A = [1; 2]; B = [3; 4]; C = [A, B];");
    auto *v = getVarPtr("C");
    EXPECT_EQ(rows(*v), 2u);
    EXPECT_EQ(cols(*v), 2u);
    EXPECT_DOUBLE_EQ((*v)(0, 0), 1.0);
    EXPECT_DOUBLE_EQ((*v)(0, 1), 3.0);
    EXPECT_DOUBLE_EQ((*v)(1, 1), 4.0);
}

TEST_P(ConcatTest, HorzcatDimensionMismatchError)
{
    auto r = engine.evalSafe("A = [1 2; 3 4]; B = [5; 6; 7]; C = [A, B];");
    EXPECT_FALSE(r.ok);
}

TEST_P(ConcatTest, HorzcatStrings)
{
    eval("s = ['hello', ' ', 'world'];");
    auto *v = getVarPtr("s");
    EXPECT_EQ(v->toString(), "hello world");
}

TEST_P(ConcatTest, HorzcatWithEmpty)
{
    eval("v = [1 2 [] 3];");
    auto *v = getVarPtr("v");
    EXPECT_EQ(v->numel(), 3u);
    EXPECT_DOUBLE_EQ(v->doubleData()[2], 3.0);
}

TEST_P(ConcatTest, VertcatScalars)
{
    eval("v = [1; 2; 3];");
    auto *v = getVarPtr("v");
    EXPECT_EQ(rows(*v), 3u);
    EXPECT_EQ(cols(*v), 1u);
}

TEST_P(ConcatTest, VertcatRowVectors)
{
    eval("A = [1 2 3; 4 5 6];");
    auto *v = getVarPtr("A");
    EXPECT_EQ(rows(*v), 2u);
    EXPECT_EQ(cols(*v), 3u);
    EXPECT_DOUBLE_EQ((*v)(1, 2), 6.0);
}

TEST_P(ConcatTest, VertcatMatrices)
{
    eval("A = [1 2; 3 4]; B = [5 6; 7 8]; C = [A; B];");
    auto *v = getVarPtr("C");
    EXPECT_EQ(rows(*v), 4u);
    EXPECT_EQ(cols(*v), 2u);
    EXPECT_DOUBLE_EQ((*v)(2, 0), 5.0);
    EXPECT_DOUBLE_EQ((*v)(3, 1), 8.0);
}

TEST_P(ConcatTest, VertcatDimensionMismatchError)
{
    auto r = engine.evalSafe("x = [1 2; 3 4 5];");
    EXPECT_FALSE(r.ok);
}

TEST_P(ConcatTest, VertcatScalarAndVector)
{
    eval("v = [1 2 3; 4 5 6; 7 8 9];");
    auto *v = getVarPtr("v");
    EXPECT_EQ(rows(*v), 3u);
    EXPECT_EQ(cols(*v), 3u);
    EXPECT_DOUBLE_EQ((*v)(2, 2), 9.0);
}

TEST_P(ConcatTest, MixedConcatBuildMatrix)
{
    // [1 2; 3 4] uses horzcat per row then vertcat
    eval("M = [1 2; 3 4];");
    auto *v = getVarPtr("M");
    EXPECT_EQ(rows(*v), 2u);
    EXPECT_EQ(cols(*v), 2u);
    EXPECT_DOUBLE_EQ((*v)(0, 0), 1.0);
    EXPECT_DOUBLE_EQ((*v)(1, 1), 4.0);
}

TEST_P(ConcatTest, ConcatWithColonRange)
{
    eval("v = [1:3, 10:12];");
    auto *v = getVarPtr("v");
    ASSERT_EQ(v->numel(), 6u);
    EXPECT_DOUBLE_EQ(v->doubleData()[0], 1.0);
    EXPECT_DOUBLE_EQ(v->doubleData()[3], 10.0);
    EXPECT_DOUBLE_EQ(v->doubleData()[5], 12.0);
}

TEST_P(ConcatTest, ConcatPreservesColumnMajor)
{
    // Column-major layout: [1 3; 2 4] stored as [1,2,3,4]
    eval("M = [1 3; 2 4];");
    auto *v = getVarPtr("M");
    EXPECT_DOUBLE_EQ(v->doubleData()[0], 1.0);
    EXPECT_DOUBLE_EQ(v->doubleData()[1], 2.0);
    EXPECT_DOUBLE_EQ(v->doubleData()[2], 3.0);
    EXPECT_DOUBLE_EQ(v->doubleData()[3], 4.0);
}

TEST_P(ConcatTest, HorzcatComplex)
{
    eval("v = [1+2i, 3+4i];");
    auto *v = getVarPtr("v");
    EXPECT_TRUE(v->isComplex());
    EXPECT_EQ(v->numel(), 2u);
    EXPECT_DOUBLE_EQ(v->complexData()[0].real(), 1.0);
    EXPECT_DOUBLE_EQ(v->complexData()[0].imag(), 2.0);
    EXPECT_DOUBLE_EQ(v->complexData()[1].real(), 3.0);
    EXPECT_DOUBLE_EQ(v->complexData()[1].imag(), 4.0);
}

TEST_P(ConcatTest, VertcatComplex)
{
    eval("v = [1+2i; 3+4i];");
    auto *v = getVarPtr("v");
    EXPECT_TRUE(v->isComplex());
    EXPECT_EQ(rows(*v), 2u);
    EXPECT_EQ(cols(*v), 1u);
}

TEST_P(ConcatTest, MixedDoubleComplexPromotes)
{
    // Mixing double and complex → result should be complex
    eval("v = [1, 2+3i, 4];");
    auto *v = getVarPtr("v");
    EXPECT_TRUE(v->isComplex());
    EXPECT_EQ(v->numel(), 3u);
    EXPECT_DOUBLE_EQ(v->complexData()[0].real(), 1.0);
    EXPECT_DOUBLE_EQ(v->complexData()[0].imag(), 0.0);
    EXPECT_DOUBLE_EQ(v->complexData()[1].real(), 2.0);
    EXPECT_DOUBLE_EQ(v->complexData()[1].imag(), 3.0);
}

TEST_P(ConcatTest, VertcatMixedDoubleComplex)
{
    eval("A = [1 2; 3 4]; B = [5+1i 6+2i]; C = [A; B];");
    auto *v = getVarPtr("C");
    EXPECT_TRUE(v->isComplex());
    EXPECT_EQ(rows(*v), 3u);
    EXPECT_EQ(cols(*v), 2u);
    // A elements promoted to complex
    EXPECT_DOUBLE_EQ(v->complexData()[0].real(), 1.0);
    EXPECT_DOUBLE_EQ(v->complexData()[0].imag(), 0.0);
}

TEST_P(ConcatTest, HorzcatLogicalStaysLogical)
{
    eval("v = [true, false, true];");
    auto *v = getVarPtr("v");
    // MATLAB: [true, false, true] stays logical
    EXPECT_TRUE(v->isLogical());
    EXPECT_EQ(v->numel(), 3u);
    EXPECT_EQ(v->logicalData()[0], 1);
    EXPECT_EQ(v->logicalData()[1], 0);
    EXPECT_EQ(v->logicalData()[2], 1);
}

TEST_P(ConcatTest, MixedLogicalDoubleConcat)
{
    eval("v = [true, 5, false];");
    auto *v = getVarPtr("v");
    EXPECT_EQ(v->type(), mlab::MType::DOUBLE);
    EXPECT_DOUBLE_EQ(v->doubleData()[0], 1.0);
    EXPECT_DOUBLE_EQ(v->doubleData()[1], 5.0);
    EXPECT_DOUBLE_EQ(v->doubleData()[2], 0.0);
}

TEST_P(ConcatTest, Vertcat3DArrays)
{
    // Create two 1×2×2 arrays, vertcat → 2×2×2
    eval(R"(
        A = zeros(1,2,2); A(1,1,1) = 1; A(1,2,1) = 2; A(1,1,2) = 3; A(1,2,2) = 4;
        B = zeros(1,2,2); B(1,1,1) = 5; B(1,2,1) = 6; B(1,1,2) = 7; B(1,2,2) = 8;
        C = vertcat(A, B);
    )");
    auto *v = getVarPtr("C");
    EXPECT_EQ(rows(*v), 2u);
    EXPECT_EQ(cols(*v), 2u);
    EXPECT_TRUE(v->dims().is3D());
    EXPECT_EQ(v->dims().pages(), 2u);
}

TEST_P(ConcatTest, Horzcat3DArrays)
{
    // Create two 2×1×2 arrays, horzcat → 2×2×2
    eval(R"(
        A = zeros(2,1,2); A(1,1,1) = 1; A(2,1,1) = 2; A(1,1,2) = 3; A(2,1,2) = 4;
        B = zeros(2,1,2); B(1,1,1) = 5; B(2,1,1) = 6; B(1,1,2) = 7; B(2,1,2) = 8;
        C = horzcat(A, B);
    )");
    auto *v = getVarPtr("C");
    EXPECT_EQ(rows(*v), 2u);
    EXPECT_EQ(cols(*v), 2u);
    EXPECT_TRUE(v->dims().is3D());
    EXPECT_EQ(v->dims().pages(), 2u);
}

TEST_P(ConcatTest, Vertcat3DPagesMismatchError)
{
    auto r = engine.evalSafe(R"(
        A = zeros(1,2,2);
        B = zeros(1,2,3);
        C = vertcat(A, B);
    )");
    EXPECT_FALSE(r.ok);
}

TEST_P(ConcatTest, VertcatChar)
{
    eval("r = ['abc'; 'def'];");
    auto *r = getVarPtr("r");
    EXPECT_TRUE(r->isChar());
    EXPECT_EQ(rows(*r), 2u);
    EXPECT_EQ(cols(*r), 3u);
}

TEST_P(ConcatTest, HorzcatCharDoubleVector)
{
    eval("r = ['AB', [67 68]];");
    auto *r = getVarPtr("r");
    EXPECT_TRUE(r->isChar());
    EXPECT_EQ(r->toString(), "ABCD");
}

INSTANTIATE_DUAL(ConcatTest);
