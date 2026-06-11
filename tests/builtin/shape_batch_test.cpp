// toolboxes/builtin/tests/shape_batch_test.cpp
// shape/size/manipulation ops — 16 functions:
//   size / numel / length / ndims
//   reshape / squeeze
//   cat / horzcat / vertcat
//   permute / ipermute
//   circshift / fliplr / flipud / rot90 / flip
// All  — bit-identical MATLAB R2025b
// on probed inputs.

#include <numkit/core/engine.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class ShapeBatchTest : public ::testing::Test
{
public:
    StandardEngine engine;
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(ShapeBatchTest, SizeNumelLength)
{
    eval("A = [1 2 3; 4 5 6];");
    EXPECT_DOUBLE_EQ(evalScalar("size(A,1)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(A,2)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("numel(A)"),  6.0);
    EXPECT_DOUBLE_EQ(evalScalar("length(A)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("numel([])"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("length([])"), 0.0);
}

TEST_F(ShapeBatchTest, Ndims)
{
    EXPECT_DOUBLE_EQ(evalScalar("ndims([1 2; 3 4])"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("ndims([1 2 3])"),    2.0);  // MATLAB: row is 2D
    EXPECT_DOUBLE_EQ(evalScalar("ndims(0)"),          2.0);  // scalars are 1×1 = 2D
}

TEST_F(ShapeBatchTest, Reshape)
{
    eval("B = reshape(1:12, 3, 4);");
    EXPECT_DOUBLE_EQ(evalScalar("B(1,1)"),  1.0);
    EXPECT_DOUBLE_EQ(evalScalar("B(2,3)"),  8.0);  // column-major
    EXPECT_DOUBLE_EQ(evalScalar("B(3,4)"), 12.0);
}

TEST_F(ShapeBatchTest, CatHorzcatVertcat)
{
    eval("B = cat(2, [1;2], [3;4]);");
    EXPECT_DOUBLE_EQ(evalScalar("size(B,1)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(B,2)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("B(2,2)"),    4.0);

    eval("H = horzcat([1;2], [3;4]);");
    EXPECT_DOUBLE_EQ(evalScalar("size(H,2)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("H(1,2)"),    3.0);

    eval("V = vertcat([1 2], [3 4]);");
    EXPECT_DOUBLE_EQ(evalScalar("size(V,1)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("V(2,1)"),    3.0);
}

TEST_F(ShapeBatchTest, PermuteIpermute)
{
    eval("A = [1 2 3; 4 5 6]; B = permute(A, [2 1]);");
    EXPECT_DOUBLE_EQ(evalScalar("size(B,1)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(B,2)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("B(3,2)"),    6.0);

    eval("C = ipermute(B, [2 1]);");
    // ipermute undoes permute → C should equal A
    EXPECT_DOUBLE_EQ(evalScalar("C(2,3)"), 6.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(C,1)"), 2.0);
}

TEST_F(ShapeBatchTest, Circshift)
{
    eval("B = circshift(1:5, 2);");
    EXPECT_DOUBLE_EQ(evalScalar("B(1)"), 4.0);
    EXPECT_DOUBLE_EQ(evalScalar("B(2)"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("B(3)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("B(5)"), 3.0);
}

TEST_F(ShapeBatchTest, FliplrFlipudFlip)
{
    eval("R = fliplr([1 2 3]);");
    EXPECT_DOUBLE_EQ(evalScalar("R(1)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("R(3)"), 1.0);

    eval("U = flipud([1; 2; 3]);");
    EXPECT_DOUBLE_EQ(evalScalar("U(1)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("U(3)"), 1.0);

    eval("F = flip([1 2 3 4]);");
    EXPECT_DOUBLE_EQ(evalScalar("F(1)"), 4.0);
    EXPECT_DOUBLE_EQ(evalScalar("F(4)"), 1.0);
}

TEST_F(ShapeBatchTest, Rot90)
{
    // rot90([1 2; 3 4]) → [2 4; 1 3]
    eval("R = rot90([1 2; 3 4]);");
    EXPECT_DOUBLE_EQ(evalScalar("R(1,1)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("R(1,2)"), 4.0);
    EXPECT_DOUBLE_EQ(evalScalar("R(2,1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("R(2,2)"), 3.0);
}

TEST_F(ShapeBatchTest, Squeeze)
{
    // squeeze removes singleton dims (numkit 2D case: a 1×N row stays 1×N)
    eval("S = squeeze(ones(1, 5));");
    EXPECT_DOUBLE_EQ(evalScalar("numel(S)"), 5.0);
}
