// libs/builtin/tests/math_reductions_batch_test.cpp
// math primitives + reductions — 11 functions:
//   cospi / sinpi
//   deg2rad / rad2deg
//   eps
//   cumsum / cumprod / diff
//   diag
//   prod / sum
// All  — bit-identical MATLAB R2025b
// on probed inputs (parity tol=1e-12).
// eps has 3 known sub-gaps
// (no-arg form, fractional input, vector input) — only the working
// scalar-positive path is pinned here.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>
#include <cmath>

using namespace numkit;

class MathReductionsBatchTest : public ::testing::Test
{
public:
    StdEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(MathReductionsBatchTest, CospiSinpi)
{
    EXPECT_DOUBLE_EQ(evalScalar("cospi(0)"),    1.0);
    EXPECT_DOUBLE_EQ(evalScalar("cospi(0.5)"),  0.0);
    EXPECT_DOUBLE_EQ(evalScalar("cospi(1)"),   -1.0);
    EXPECT_DOUBLE_EQ(evalScalar("sinpi(0)"),    0.0);
    EXPECT_DOUBLE_EQ(evalScalar("sinpi(0.5)"),  1.0);
    EXPECT_DOUBLE_EQ(evalScalar("sinpi(1)"),    0.0);
}

TEST_F(MathReductionsBatchTest, Deg2RadRad2Deg)
{
    EXPECT_NEAR(evalScalar("deg2rad(180)"),  3.141592653589793, 1e-12);
    EXPECT_NEAR(evalScalar("deg2rad(90)"),   1.570796326794897, 1e-12);
    EXPECT_NEAR(evalScalar("rad2deg(pi)"),   180.0,             1e-12);
    EXPECT_NEAR(evalScalar("rad2deg(pi/2)"), 90.0,              1e-12);
    // Round-trip
    EXPECT_NEAR(evalScalar("rad2deg(deg2rad(73))"), 73.0, 1e-12);
}

TEST_F(MathReductionsBatchTest, WrapToPi)
{
    constexpr double pi = 3.14159265358979323846;
    // strictly outside [-pi,pi] is wrapped
    EXPECT_NEAR(evalScalar("wrapToPi(4)"),  4.0 - 2.0 * pi, 1e-12);
    EXPECT_NEAR(evalScalar("wrapToPi(-4)"), -4.0 + 2.0 * pi, 1e-12);
    EXPECT_NEAR(evalScalar("wrapToPi(7)"),  7.0 - 2.0 * pi, 1e-12);
    // closed endpoints are preserved (not wrapped)
    EXPECT_NEAR(evalScalar("wrapToPi(pi)"),  pi,  1e-12);
    EXPECT_NEAR(evalScalar("wrapToPi(-pi)"), -pi, 1e-12);
    // in-range value untouched
    EXPECT_NEAR(evalScalar("wrapToPi(1)"), 1.0, 1e-12);
}

TEST_F(MathReductionsBatchTest, WrapTo2Pi)
{
    constexpr double pi = 3.14159265358979323846;
    EXPECT_NEAR(evalScalar("wrapTo2Pi(-1)"), -1.0 + 2.0 * pi, 1e-12);
    EXPECT_NEAR(evalScalar("wrapTo2Pi(7)"),  7.0 - 2.0 * pi, 1e-12);
    EXPECT_NEAR(evalScalar("wrapTo2Pi(-7)"), -7.0 + 4.0 * pi, 1e-12);
    // positive input landing on the open boundary maps to 2*pi
    EXPECT_NEAR(evalScalar("wrapTo2Pi(2*pi)"), 2.0 * pi, 1e-12);
    // but zero and a negative full turn map to 0
    EXPECT_NEAR(evalScalar("wrapTo2Pi(0)"),     0.0, 1e-12);
    EXPECT_NEAR(evalScalar("wrapTo2Pi(-2*pi)"), 0.0, 1e-12);
}

TEST_F(MathReductionsBatchTest, WrapTo180)
{
    EXPECT_DOUBLE_EQ(evalScalar("wrapTo180(190)"),  -170.0);
    EXPECT_DOUBLE_EQ(evalScalar("wrapTo180(-190)"),  170.0);
    EXPECT_DOUBLE_EQ(evalScalar("wrapTo180(540)"),   180.0); // 540 -> 180 (kept)
    // closed endpoints preserved
    EXPECT_DOUBLE_EQ(evalScalar("wrapTo180(180)"),   180.0);
    EXPECT_DOUBLE_EQ(evalScalar("wrapTo180(-180)"), -180.0);
    EXPECT_DOUBLE_EQ(evalScalar("wrapTo180(45)"),     45.0);
}

TEST_F(MathReductionsBatchTest, WrapTo360)
{
    EXPECT_DOUBLE_EQ(evalScalar("wrapTo360(-10)"),  350.0);
    EXPECT_DOUBLE_EQ(evalScalar("wrapTo360(370)"),   10.0);
    // positive input landing on 0 maps to 360
    EXPECT_DOUBLE_EQ(evalScalar("wrapTo360(720)"),  360.0);
    EXPECT_DOUBLE_EQ(evalScalar("wrapTo360(360)"),  360.0);
    // zero stays 0
    EXPECT_DOUBLE_EQ(evalScalar("wrapTo360(0)"),      0.0);
}

TEST_F(MathReductionsBatchTest, Eps)
{
    EXPECT_DOUBLE_EQ(evalScalar("eps(1)"), 2.220446049250313e-16);
}

TEST_F(MathReductionsBatchTest, Cumsum)
{
    eval("y = cumsum([1, 2, 3, 4, 5]);");
    EXPECT_DOUBLE_EQ(evalScalar("y(1)"),  1.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(3)"),  6.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(5)"), 15.0);
}

TEST_F(MathReductionsBatchTest, Cumprod)
{
    eval("y = cumprod([1, 2, 3, 4, 5]);");
    EXPECT_DOUBLE_EQ(evalScalar("y(1)"),   1.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(3)"),   6.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(5)"), 120.0);
}

// cumsum/cumprod 'reverse' direction + 'omitnan' nanflag (were unsupported
// -> threw "Cannot convert char to scalar"). vs MATLAB R2025b.
TEST_F(MathReductionsBatchTest, CumsumProdReverseAndOmitnan)
{
    eval("rr = cumsum([1 2 3 4], 'reverse');");
    EXPECT_DOUBLE_EQ(evalScalar("rr(1)"), 10.0);
    EXPECT_DOUBLE_EQ(evalScalar("rr(2)"),  9.0);
    EXPECT_DOUBLE_EQ(evalScalar("rr(4)"),  4.0);
    eval("pr = cumprod([1 2 3 4], 'reverse');");
    EXPECT_DOUBLE_EQ(evalScalar("pr(1)"), 24.0);
    EXPECT_DOUBLE_EQ(evalScalar("pr(3)"), 12.0);
    // 'omitnan' treats NaN as the identity (0 for sum, 1 for prod);
    // default 'includenan' propagates NaN.
    eval("so = cumsum([1 NaN 3], 'omitnan');");
    EXPECT_DOUBLE_EQ(evalScalar("so(2)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("so(3)"), 4.0);
    eval("si = cumsum([1 NaN 3]);");
    EXPECT_TRUE(std::isnan(evalScalar("si(3)")));
    // dim + direction together.
    eval("md = cumsum([1 2 3; 4 5 6], 2, 'reverse');");
    EXPECT_DOUBLE_EQ(evalScalar("md(1,1)"), 6.0);
    EXPECT_DOUBLE_EQ(evalScalar("md(1,3)"), 3.0);
}

TEST_F(MathReductionsBatchTest, Diff)
{
    eval("y = diff([1, 4, 9, 16, 25]);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(y)"), 4.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(1)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(2)"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(4)"), 9.0);
}

TEST_F(MathReductionsBatchTest, DiagFromVector)
{
    // diag(vec) → diagonal matrix
    eval("D = diag([1, 2, 3]);");
    EXPECT_DOUBLE_EQ(evalScalar("D(1,1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("D(2,2)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("D(3,3)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("D(1,2)"), 0.0);
}

TEST_F(MathReductionsBatchTest, ProdSum)
{
    EXPECT_DOUBLE_EQ(evalScalar("sum([1, 2, 3, 4, 5])"),  15.0);
    EXPECT_DOUBLE_EQ(evalScalar("prod([1, 2, 3, 4, 5])"), 120.0);
    EXPECT_DOUBLE_EQ(evalScalar("sum([2.5, 3.5])"),       6.0);
    EXPECT_DOUBLE_EQ(evalScalar("prod([0.5, 4])"),        2.0);
}

// MATLAB R2025b: a default reduction of the 0x0 empty [] returns the scalar
// identity, NOT a 1x0 empty: sum([])==0, prod([])==1, mean([])==NaN.
// DEEP-PROBE 2026-05-29: numkit previously returned a 1x0 empty for all three.
TEST_F(MathReductionsBatchTest, EmptyDefaultReductionScalarIdentity)
{
    EXPECT_DOUBLE_EQ(evalScalar("sum([])"),  0.0);
    EXPECT_DOUBLE_EQ(evalScalar("numel(sum([]))"),  1.0);
    EXPECT_DOUBLE_EQ(evalScalar("prod([])"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("numel(prod([]))"), 1.0);
    EXPECT_TRUE(std::isnan(evalScalar("mean([])")));
    EXPECT_DOUBLE_EQ(evalScalar("numel(mean([]))"), 1.0);
}

// Partial empties keep their per-column shape (unchanged by the 0x0 guard):
// sum(zeros(0,3)) -> [0 0 0] (1x3); sum(zeros(3,0)) -> 1x0 empty.
TEST_F(MathReductionsBatchTest, PartialEmptyReductionKeepsShape)
{
    eval("a = sum(zeros(0,3));");
    EXPECT_DOUBLE_EQ(evalScalar("numel(a)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("a(1)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("a(3)"), 0.0);
    eval("b = prod(zeros(0,3));");
    EXPECT_DOUBLE_EQ(evalScalar("b(2)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("numel(sum(zeros(3,0)))"), 0.0);
}

// MATLAB R2025b: max/min of an EMPTY array returns empty and never errors.
// Shape = input size with the operating dim clamped to min(size,1):
// max([])=0x0, max(zeros(0,3))=0x3, max(zeros(3,0))=1x0.
// DEEP-PROBE 2026-05-29: numkit previously threw "min/max of empty array
// is not supported".
TEST_F(MathReductionsBatchTest, MaxMinEmptyReturnsEmpty)
{
    EXPECT_NO_THROW(eval("max([]);"));
    EXPECT_NO_THROW(eval("min([]);"));
    EXPECT_DOUBLE_EQ(evalScalar("numel(max([]))"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(max([]),1)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(max([]),2)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("numel(min([]))"), 0.0);

    // 0x3 stays 0x3; 3x0 collapses the (>0) operating dim -> 1x0.
    EXPECT_DOUBLE_EQ(evalScalar("size(max(zeros(0,3)),1)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(max(zeros(0,3)),2)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(max(zeros(3,0)),1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(max(zeros(3,0)),2)"), 0.0);
}

// [M,I] = max([]) -> both empty; non-empty index regression still holds.
TEST_F(MathReductionsBatchTest, MaxEmptyTwoOutputAndIndexRegression)
{
    eval("function [m,i] = mx2(v)\n  [m,i] = max(v);\nend");
    eval("[me, ie] = mx2([]);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(me)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("numel(ie)"), 0.0);
    eval("[mv, iv] = mx2([3 1 5 2]);");
    EXPECT_DOUBLE_EQ(evalScalar("mv"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("iv"), 3.0);
}

// cumsum / cumprod on integer types: MATLAB keeps the integer class and
// accumulates natively with saturation at each step (the saturated running
// value carries forward). numkit previously threw "Not a double array".
// DEEP-PROBE 2026-05-30.
TEST_F(MathReductionsBatchTest, CumsumCumprodIntegerClass)
{
    // Saturation + class preservation.
    eval("a = cumsum(int8([100 100 100]));");        // 100, 200->127, 227->127
    EXPECT_DOUBLE_EQ(evalScalar("double(a(1))"), 100.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(a(2))"), 127.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(a(3))"), 127.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(strcmp(class(a),'int8'))"), 1.0);
    // Native saturation: the clamped 127 is carried forward, then -100 -> 27.
    eval("b = cumsum(int8([100 100 -100]));");
    EXPECT_DOUBLE_EQ(evalScalar("double(b(2))"), 127.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(b(3))"), 27.0);
    // uint8 upper saturation.
    eval("c = cumsum(uint8([200 100]));");
    EXPECT_DOUBLE_EQ(evalScalar("double(c(2))"), 255.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(strcmp(class(c),'uint8'))"), 1.0);
    // cumprod saturates too.
    eval("d = cumprod(int8([5 10 10]));");           // 5, 50, 500->127
    EXPECT_DOUBLE_EQ(evalScalar("double(d(3))"), 127.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(strcmp(class(d),'int8'))"), 1.0);
    // Per-dim on a matrix keeps the class.
    eval("f = cumsum(int32([1 2; 3 4]));");          // dim1 -> [1 2;4 6]
    EXPECT_DOUBLE_EQ(evalScalar("double(f(2,1))"), 4.0);
    eval("g = cumsum(int32([1 2; 3 4]), 2);");       // dim2 -> [1 3;3 7]
    EXPECT_DOUBLE_EQ(evalScalar("double(g(1,2))"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(strcmp(class(g),'int32'))"), 1.0);
    // int16 no saturation, exact.
    eval("e = cumsum(int16([10 20 30]));");
    EXPECT_DOUBLE_EQ(evalScalar("double(e(3))"), 60.0);
    // double input is unchanged (regress).
    eval("dd = cumsum([1 2 3 4]);");
    EXPECT_DOUBLE_EQ(evalScalar("dd(4)"), 10.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(strcmp(class(dd),'double'))"), 1.0);
}

// diff() on integer types keeps the class and SATURATES each pass (MATLAB
// R2025b). numkit previously promoted to double. DEEP-PROBE 2026-05-30.
TEST_F(MathReductionsBatchTest, DiffIntegerClassSaturates)
{
    eval("a = diff(int8([10 5 20]));");
    EXPECT_DOUBLE_EQ(evalScalar("double(a(1))"), -5.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(a(2))"), 15.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(strcmp(class(a),'int8'))"), 1.0);
    // Overflow saturates: 100 - (-100) = 200 -> 127.
    eval("b = diff(int8([-100 100]));");
    EXPECT_DOUBLE_EQ(evalScalar("double(b(1))"), 127.0);
    // Unsigned underflow saturates at 0: 3 - 5 -> 0.
    eval("c = diff(uint8([5 3]));");
    EXPECT_DOUBLE_EQ(evalScalar("double(c(1))"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(strcmp(class(c),'uint8'))"), 1.0);
    // n=2 order applies saturation per pass.
    eval("e = diff(int8([1 2 4 8]), 2);");
    EXPECT_DOUBLE_EQ(evalScalar("double(e(1))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(e(2))"), 2.0);
    // Per-dim on a matrix keeps the class.
    eval("g = diff(int32([1 2 3; 5 8 13]), 1, 2);");   // [1 1;3 5]
    EXPECT_DOUBLE_EQ(evalScalar("double(g(2,2))"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(strcmp(class(g),'int32'))"), 1.0);
    // double input unchanged (regress).
    eval("dd = diff([1 4 9 16]);");
    EXPECT_DOUBLE_EQ(evalScalar("dd(2)"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(strcmp(class(dd),'double'))"), 1.0);
}

// max/min(A, [], 'all') — reduce over every element. Was broken entirely
// (toScalar on the 'all' string). The 2nd output is the linear index.
// vs MATLAB R2025b.
TEST_F(MathReductionsBatchTest, MaxMinAll)
{
    eval("A = [3 1; 4 1; 2 9];");
    eval("[m, i] = max(A, [], 'all');");
    EXPECT_DOUBLE_EQ(evalScalar("m"), 9.0);
    EXPECT_DOUBLE_EQ(evalScalar("i"), 6.0);            // linear index of the 9
    // 'all' + 'linear' (redundant — 'all' index is already linear).
    eval("[m2, i2] = max(A, [], 'all', 'linear');");
    EXPECT_DOUBLE_EQ(evalScalar("m2"), 9.0);
    EXPECT_DOUBLE_EQ(evalScalar("i2"), 6.0);
    // min over all.
    EXPECT_DOUBLE_EQ(evalScalar("min(A, [], 'all')"), 1.0);
    // 3-D array.
    eval("B = reshape(1:24, 2, 3, 4);");
    EXPECT_DOUBLE_EQ(evalScalar("max(B, [], 'all')"), 24.0);
    EXPECT_DOUBLE_EQ(evalScalar("min(B, [], 'all')"), 1.0);
    // omitnan over all.
    eval("[mo, io] = min([5 NaN 2 8], [], 'all', 'omitnan');");
    EXPECT_DOUBLE_EQ(evalScalar("mo"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("io"), 3.0);
}
