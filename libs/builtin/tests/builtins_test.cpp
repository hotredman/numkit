// tests/test_builtins.cpp — Built-in functions (math, array creation, strings)
// Parameterized: runs on both TreeWalker and VM backends

#include "dual_engine_fixture.hpp"

using namespace m_test;

class BuiltinTest : public DualEngineTest {};

// ── Array creation ──────────────────────────────────────────

TEST_P(BuiltinTest, Zeros)
{
    eval("A = zeros(2, 3);");
    auto *A = getVarPtr("A");
    EXPECT_EQ(rows(*A), 2u);
    EXPECT_EQ(cols(*A), 3u);
    for (size_t i = 0; i < A->numel(); ++i)
        EXPECT_DOUBLE_EQ(A->doubleData()[i], 0.0);
}

TEST_P(BuiltinTest, Ones)
{
    eval("A = ones(2, 2);");
    auto *A = getVarPtr("A");
    for (size_t i = 0; i < A->numel(); ++i)
        EXPECT_DOUBLE_EQ(A->doubleData()[i], 1.0);
}

TEST_P(BuiltinTest, Eye)
{
    eval("I = eye(3);");
    auto *I = getVarPtr("I");
    EXPECT_DOUBLE_EQ((*I)(0, 0), 1.0);
    EXPECT_DOUBLE_EQ((*I)(1, 1), 1.0);
    EXPECT_DOUBLE_EQ((*I)(0, 1), 0.0);
}

// ── Vector syntax for array creation ────────────────────────

TEST_P(BuiltinTest, ZerosVectorArg)
{
    eval("A = zeros([2 3]);");
    auto *A = getVarPtr("A");
    EXPECT_EQ(rows(*A), 2u);
    EXPECT_EQ(cols(*A), 3u);
}

TEST_P(BuiltinTest, OnesVectorArg)
{
    eval("A = ones([3 4]);");
    auto *A = getVarPtr("A");
    EXPECT_EQ(rows(*A), 3u);
    EXPECT_EQ(cols(*A), 4u);
    EXPECT_DOUBLE_EQ(A->doubleData()[0], 1.0);
}

TEST_P(BuiltinTest, RandVectorArg)
{
    eval("A = rand([2 5]);");
    auto *A = getVarPtr("A");
    EXPECT_EQ(rows(*A), 2u);
    EXPECT_EQ(cols(*A), 5u);
}

TEST_P(BuiltinTest, RandnVectorArg)
{
    eval("A = randn([3 4]);");
    auto *A = getVarPtr("A");
    EXPECT_EQ(rows(*A), 3u);
    EXPECT_EQ(cols(*A), 4u);
}

TEST_P(BuiltinTest, ZerosSizeArg)
{
    // zeros(size(x)) — copy dimensions from another array
    eval("x = ones(3, 5); A = zeros(size(x));");
    auto *A = getVarPtr("A");
    EXPECT_EQ(rows(*A), 3u);
    EXPECT_EQ(cols(*A), 5u);
}

TEST_P(BuiltinTest, RandnSizeArg)
{
    eval("x = ones(4, 6); A = randn(size(x));");
    auto *A = getVarPtr("A");
    EXPECT_EQ(rows(*A), 4u);
    EXPECT_EQ(cols(*A), 6u);
}

TEST_P(BuiltinTest, Rand3D)
{
    eval("A = rand(2, 3, 4);");
    auto *A = getVarPtr("A");
    EXPECT_EQ(rows(*A), 2u);
    EXPECT_EQ(cols(*A), 3u);
    EXPECT_EQ(A->dims().pages(), 4u);
}

TEST_P(BuiltinTest, Randn3D)
{
    eval("A = randn(2, 3, 4);");
    auto *A = getVarPtr("A");
    EXPECT_EQ(rows(*A), 2u);
    EXPECT_EQ(cols(*A), 3u);
    EXPECT_EQ(A->dims().pages(), 4u);
}

TEST_P(BuiltinTest, Zeros3DVectorArg)
{
    eval("A = zeros([2 3 4]);");
    auto *A = getVarPtr("A");
    EXPECT_EQ(rows(*A), 2u);
    EXPECT_EQ(cols(*A), 3u);
    EXPECT_EQ(A->dims().pages(), 4u);
}

TEST_P(BuiltinTest, EyeVectorArg)
{
    eval("I = eye([3 4]);");
    auto *I = getVarPtr("I");
    EXPECT_EQ(rows(*I), 3u);
    EXPECT_EQ(cols(*I), 4u);
    EXPECT_DOUBLE_EQ((*I)(0, 0), 1.0);
    EXPECT_DOUBLE_EQ((*I)(2, 2), 1.0);
    EXPECT_DOUBLE_EQ((*I)(0, 3), 0.0);
}

// ── Size / length / numel ───────────────────────────────────

TEST_P(BuiltinTest, SizeWithDim)
{
    eval("A = ones(3, 5);");
    eval("r = size(A, 1);");
    EXPECT_DOUBLE_EQ(getVar("r"), 3.0);
    eval("c = size(A, 2);");
    EXPECT_DOUBLE_EQ(getVar("c"), 5.0);
}

TEST_P(BuiltinTest, SizeNoArgs)
{
    // size without arguments → error (MATLAB: "Not enough input arguments")
    EXPECT_THROW(eval("size;"), std::exception);
    EXPECT_THROW(eval("size()"), std::exception);
}

TEST_P(BuiltinTest, SizeReturnsRowVector)
{
    // size(A) returns [rows, cols] as 1x2 row vector
    eval("A = ones(3, 5); s = size(A);");
    auto *s = getVarPtr("s");
    EXPECT_EQ(rows(*s), 1u);
    EXPECT_EQ(cols(*s), 2u);
    EXPECT_DOUBLE_EQ(s->doubleData()[0], 3.0);
    EXPECT_DOUBLE_EQ(s->doubleData()[1], 5.0);
}

TEST_P(BuiltinTest, SizeScalar)
{
    // size(scalar) → [1, 1]
    eval("s = size(42);");
    auto *s = getVarPtr("s");
    EXPECT_EQ(rows(*s), 1u);
    EXPECT_EQ(cols(*s), 2u);
    EXPECT_DOUBLE_EQ(s->doubleData()[0], 1.0);
    EXPECT_DOUBLE_EQ(s->doubleData()[1], 1.0);
}

TEST_P(BuiltinTest, SizeRowVector)
{
    eval("s = size([1 2 3 4]);");
    auto *s = getVarPtr("s");
    EXPECT_DOUBLE_EQ(s->doubleData()[0], 1.0);
    EXPECT_DOUBLE_EQ(s->doubleData()[1], 4.0);
}

TEST_P(BuiltinTest, SizeColumnVector)
{
    eval("s = size([1; 2; 3]);");
    auto *s = getVarPtr("s");
    EXPECT_DOUBLE_EQ(s->doubleData()[0], 3.0);
    EXPECT_DOUBLE_EQ(s->doubleData()[1], 1.0);
}

TEST_P(BuiltinTest, SizeEmptyMatrix)
{
    eval("s = size([]);");
    auto *s = getVarPtr("s");
    EXPECT_DOUBLE_EQ(s->doubleData()[0], 0.0);
    EXPECT_DOUBLE_EQ(s->doubleData()[1], 0.0);
}

TEST_P(BuiltinTest, SizeDimBeyondNdims)
{
    // size(A, dim) where dim > ndims → 1 (MATLAB behavior)
    eval("A = [1 2 3]; r = size(A, 3);");
    EXPECT_DOUBLE_EQ(getVar("r"), 1.0);
}

TEST_P(BuiltinTest, SizeMultiReturn)
{
    // [m, n] = size(A)
    eval("A = ones(4, 7); [m, n] = size(A);");
    EXPECT_DOUBLE_EQ(getVar("m"), 4.0);
    EXPECT_DOUBLE_EQ(getVar("n"), 7.0);
}

TEST_P(BuiltinTest, SizeString)
{
    // size of a string → [1, length]
    eval("s = size('hello');");
    auto *s = getVarPtr("s");
    EXPECT_DOUBLE_EQ(s->doubleData()[0], 1.0);
    EXPECT_DOUBLE_EQ(s->doubleData()[1], 5.0);
}

TEST_P(BuiltinTest, Length)
{
    eval("v = [1 2 3 4 5]; l = length(v);");
    EXPECT_DOUBLE_EQ(getVar("l"), 5.0);
}

TEST_P(BuiltinTest, Numel)
{
    eval("A = ones(3, 4); n = numel(A);");
    EXPECT_DOUBLE_EQ(getVar("n"), 12.0);
}

// ── Aggregation ─────────────────────────────────────────────

TEST_P(BuiltinTest, Sum)
{
    eval("r = sum([1 2 3 4 5]);");
    EXPECT_DOUBLE_EQ(getVar("r"), 15.0);
}

TEST_P(BuiltinTest, MinMax)
{
    eval("function [a, b] = mymin(v)\n  a = min(v);\n  b = 0;\n  for i = 1:length(v)\n    if v(i) "
         "== a, b = i; break; end\n  end\nend");
    eval("[mn, mi] = mymin([3 1 4 1 5]);");
    EXPECT_DOUBLE_EQ(getVar("mn"), 1.0);
    EXPECT_DOUBLE_EQ(getVar("mi"), 2.0);
}

// ── Math functions ──────────────────────────────────────────

TEST_P(BuiltinTest, MathFunctions)
{
    EXPECT_NEAR(evalScalar("sin(pi/2);"), 1.0, 1e-12);
    EXPECT_NEAR(evalScalar("cos(0);"), 1.0, 1e-12);
    EXPECT_NEAR(evalScalar("sqrt(16);"), 4.0, 1e-12);
    EXPECT_NEAR(evalScalar("abs(-5);"), 5.0, 1e-12);
    EXPECT_NEAR(evalScalar("exp(0);"), 1.0, 1e-12);
    EXPECT_NEAR(evalScalar("log(1);"), 0.0, 1e-12);
}

// ── Hyperbolic trig ─────────────────────────────────────────
TEST_P(BuiltinTest, Hyperbolic)
{
    EXPECT_NEAR(evalScalar("sinh(0);"), 0.0, 1e-15);
    EXPECT_NEAR(evalScalar("cosh(0);"), 1.0, 1e-15);
    EXPECT_NEAR(evalScalar("tanh(0);"), 0.0, 1e-15);
    EXPECT_NEAR(evalScalar("sinh(1);"),  1.1752011936438014, 1e-12);
    EXPECT_NEAR(evalScalar("cosh(1);"),  1.5430806348152437, 1e-12);
    EXPECT_NEAR(evalScalar("tanh(1);"),  0.7615941559557649, 1e-12);
    // Inverses are exact identities.
    EXPECT_NEAR(evalScalar("asinh(sinh(0.5));"), 0.5, 1e-12);
    EXPECT_NEAR(evalScalar("acosh(cosh(0.5));"), 0.5, 1e-12);
    EXPECT_NEAR(evalScalar("atanh(tanh(0.5));"), 0.5, 1e-12);
    // acosh of a value < 1 returns a complex result (matches MATLAB).
    eval("z = acosh(0);");
    auto *z = getVarPtr("z");
    ASSERT_NE(z, nullptr);
    EXPECT_TRUE(z->isComplex());
}

TEST_P(BuiltinTest, HyperbolicVector)
{
    eval("v = sinh([0 1 -1]);");
    auto *v = getVarPtr("v");
    ASSERT_NE(v, nullptr);
    EXPECT_EQ(v->numel(), 3u);
    EXPECT_NEAR(v->doubleData()[0], 0.0, 1e-12);
    EXPECT_NEAR(v->doubleData()[1],  1.1752011936438014, 1e-12);
    EXPECT_NEAR(v->doubleData()[2], -1.1752011936438014, 1e-12);
}

// ── Degree-input trig ───────────────────────────────────────
TEST_P(BuiltinTest, DegreeTrig)
{
    // Exact zeros at integer multiples of 180°.
    EXPECT_DOUBLE_EQ(evalScalar("sind(0);"),    0.0);
    EXPECT_DOUBLE_EQ(evalScalar("sind(180);"),  0.0);
    EXPECT_DOUBLE_EQ(evalScalar("sind(-180);"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("sind(360);"),  0.0);
    // ±1 at ±90°.
    EXPECT_DOUBLE_EQ(evalScalar("sind(90);"),   1.0);
    EXPECT_DOUBLE_EQ(evalScalar("sind(-90);"), -1.0);
    EXPECT_DOUBLE_EQ(evalScalar("cosd(0);"),    1.0);
    EXPECT_DOUBLE_EQ(evalScalar("cosd(180);"), -1.0);
    EXPECT_DOUBLE_EQ(evalScalar("cosd(90);"),   0.0);
    EXPECT_DOUBLE_EQ(evalScalar("cosd(-90);"),  0.0);
    EXPECT_DOUBLE_EQ(evalScalar("tand(0);"),    0.0);
    EXPECT_DOUBLE_EQ(evalScalar("tand(180);"),  0.0);
    EXPECT_NEAR(evalScalar("tand(45);"), 1.0, 1e-12);
    // tan(±90°) is ±Inf.
    EXPECT_TRUE(std::isinf(evalScalar("tand(90);")));
    EXPECT_TRUE(std::isinf(evalScalar("tand(-90);")));
    // Inverses return degrees.
    EXPECT_NEAR(evalScalar("asind(1);"),  90.0, 1e-12);
    EXPECT_NEAR(evalScalar("asind(0);"),   0.0, 1e-12);
    EXPECT_NEAR(evalScalar("asind(-1);"), -90.0, 1e-12);
    EXPECT_NEAR(evalScalar("acosd(1);"),    0.0, 1e-12);
    EXPECT_NEAR(evalScalar("acosd(0);"),   90.0, 1e-12);
    EXPECT_NEAR(evalScalar("acosd(-1);"), 180.0, 1e-12);
    EXPECT_NEAR(evalScalar("atand(1);"),   45.0, 1e-12);
    EXPECT_NEAR(evalScalar("atand(0);"),    0.0, 1e-12);
    EXPECT_NEAR(evalScalar("atan2d(1, 1);"),  45.0, 1e-12);
    EXPECT_NEAR(evalScalar("atan2d(1, 0);"),  90.0, 1e-12);
    EXPECT_NEAR(evalScalar("atan2d(0, -1);"),180.0, 1e-12);
    EXPECT_NEAR(evalScalar("atan2d(-1, 1);"),-45.0, 1e-12);
}

// ── Pi-scaled trig ──────────────────────────────────────────
TEST_P(BuiltinTest, PiScaledTrig)
{
    // Exact zeros at integer args.
    EXPECT_DOUBLE_EQ(evalScalar("sinpi(0);"),  0.0);
    EXPECT_DOUBLE_EQ(evalScalar("sinpi(1);"),  0.0);
    EXPECT_DOUBLE_EQ(evalScalar("sinpi(-1);"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("sinpi(100);"),0.0);
    EXPECT_DOUBLE_EQ(evalScalar("sinpi(0.5);"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("sinpi(-0.5);"),-1.0);
    EXPECT_DOUBLE_EQ(evalScalar("cospi(0);"),  1.0);
    EXPECT_DOUBLE_EQ(evalScalar("cospi(1);"), -1.0);
    EXPECT_DOUBLE_EQ(evalScalar("cospi(0.5);"),0.0);
    EXPECT_DOUBLE_EQ(evalScalar("cospi(-0.5);"),0.0);
    // 1/3 is irrational in radians-times-pi, no clean equality.
    EXPECT_NEAR(evalScalar("sinpi(1/6);"), 0.5, 1e-12);
    EXPECT_NEAR(evalScalar("cospi(1/3);"), 0.5, 1e-12);
}

// ── Reciprocal trig — sec/csc/cot families ────────────────────
TEST_P(BuiltinTest, ReciprocalTrigPrimary)
{
    EXPECT_NEAR(evalScalar("sec(0);"), 1.0, 1e-12);
    EXPECT_NEAR(evalScalar("sec(pi);"), -1.0, 1e-12);
    EXPECT_NEAR(evalScalar("csc(pi/2);"), 1.0, 1e-12);
    EXPECT_NEAR(evalScalar("csc(-pi/2);"), -1.0, 1e-12);
    EXPECT_NEAR(evalScalar("cot(pi/4);"), 1.0, 1e-12);
    EXPECT_NEAR(evalScalar("cot(pi/2);"), 0.0, 1e-12);
}

TEST_P(BuiltinTest, ReciprocalTrigHyperbolic)
{
    EXPECT_NEAR(evalScalar("sech(0);"), 1.0, 1e-12);
    EXPECT_NEAR(evalScalar("sech(1);"), 0.6480542736638853, 1e-12);
    EXPECT_TRUE(std::isinf(evalScalar("csch(0);")));
    EXPECT_NEAR(evalScalar("csch(1);"), 0.8509181282393216, 1e-12);
    EXPECT_TRUE(std::isinf(evalScalar("coth(0);")));
    EXPECT_NEAR(evalScalar("coth(1);"), 1.3130352854993313, 1e-12);
}

TEST_P(BuiltinTest, ReciprocalTrigDegree)
{
    EXPECT_NEAR(evalScalar("secd(0);"),    1.0, 1e-12);
    EXPECT_NEAR(evalScalar("secd(180);"), -1.0, 1e-12);
    EXPECT_TRUE(std::isinf(evalScalar("secd(90);")));
    EXPECT_NEAR(evalScalar("cscd(90);"),  1.0, 1e-12);
    EXPECT_NEAR(evalScalar("cscd(-90);"),-1.0, 1e-12);
    EXPECT_TRUE(std::isinf(evalScalar("cscd(0);")));
    EXPECT_NEAR(evalScalar("cotd(45);"),  1.0, 1e-12);
    EXPECT_NEAR(evalScalar("cotd(135);"),-1.0, 1e-12);
    EXPECT_TRUE(std::isinf(evalScalar("cotd(0);")));
}

TEST_P(BuiltinTest, InverseReciprocalTrig)
{
    EXPECT_NEAR(evalScalar("asec(1);"), 0.0, 1e-12);
    EXPECT_NEAR(evalScalar("asec(2);"), std::acos(0.5), 1e-12);
    EXPECT_NEAR(evalScalar("acsc(1);"), M_PI / 2, 1e-12);
    EXPECT_NEAR(evalScalar("acsc(2);"), std::asin(0.5), 1e-12);
    EXPECT_NEAR(evalScalar("acot(1);"), M_PI / 4, 1e-12);
    EXPECT_NEAR(evalScalar("acot(0);"), M_PI / 2, 1e-12);
    EXPECT_NEAR(evalScalar("asech(1);"), 0.0, 1e-12);
    EXPECT_NEAR(evalScalar("acsch(1);"), std::asinh(1.0), 1e-12);
    EXPECT_NEAR(evalScalar("acoth(2);"), std::atanh(0.5), 1e-12);
    EXPECT_NEAR(evalScalar("asecd(1);"),  0.0, 1e-12);
    EXPECT_NEAR(evalScalar("asecd(2);"), 60.0, 1e-12);
    EXPECT_NEAR(evalScalar("acscd(1);"), 90.0, 1e-12);
    EXPECT_NEAR(evalScalar("acscd(2);"), 30.0, 1e-12);
    EXPECT_NEAR(evalScalar("acotd(1);"), 45.0, 1e-12);
}

TEST_P(BuiltinTest, ReciprocalTrigOutOfDomainComplex)
{
    eval("z = asec(0.5);");
    auto *z = getVarPtr("z");
    ASSERT_NE(z, nullptr);
    EXPECT_TRUE(z->isComplex());
    eval("w = acsc(0.5);");
    auto *w = getVarPtr("w");
    ASSERT_NE(w, nullptr);
    EXPECT_TRUE(w->isComplex());
    eval("u = acoth(0.5);");
    auto *u = getVarPtr("u");
    ASSERT_NE(u, nullptr);
    EXPECT_TRUE(u->isComplex());
}

// ── Coordinate transforms ─────────────────────────────────────
TEST_P(BuiltinTest, CartPolarRoundtrip)
{
    eval("[t, r] = cart2pol(3, 4);");
    EXPECT_NEAR(getVar("t"), std::atan2(4.0, 3.0), 1e-12);
    EXPECT_NEAR(getVar("r"), 5.0, 1e-12);
    // pol2cart inverse.
    eval("[xv, yv] = pol2cart(t, r);");
    EXPECT_NEAR(getVar("xv"), 3.0, 1e-12);
    EXPECT_NEAR(getVar("yv"), 4.0, 1e-12);
}

TEST_P(BuiltinTest, CartPolar3DPassthrough)
{
    // 3-arg cart2pol/pol2cart treat z as a passthrough (cylindrical).
    eval("[t, r, z] = cart2pol(3, 4, 7);");
    EXPECT_NEAR(getVar("t"), std::atan2(4.0, 3.0), 1e-12);
    EXPECT_NEAR(getVar("r"), 5.0, 1e-12);
    EXPECT_NEAR(getVar("z"), 7.0, 1e-12);
    eval("[a, b, c] = pol2cart(t, r, z);");
    EXPECT_NEAR(getVar("a"), 3.0, 1e-12);
    EXPECT_NEAR(getVar("b"), 4.0, 1e-12);
    EXPECT_NEAR(getVar("c"), 7.0, 1e-12);
}

TEST_P(BuiltinTest, CartSphRoundtrip)
{
    // (1, 1, 1) → az = pi/4, el = atan2(1, sqrt(2)), r = sqrt(3).
    eval("[a, e, r] = cart2sph(1, 1, 1);");
    EXPECT_NEAR(getVar("a"), M_PI / 4, 1e-12);
    EXPECT_NEAR(getVar("e"), std::atan2(1.0, std::sqrt(2.0)), 1e-12);
    EXPECT_NEAR(getVar("r"), std::sqrt(3.0), 1e-12);
    // sph2cart inverse.
    eval("[xv, yv, zv] = sph2cart(a, e, r);");
    EXPECT_NEAR(getVar("xv"), 1.0, 1e-12);
    EXPECT_NEAR(getVar("yv"), 1.0, 1e-12);
    EXPECT_NEAR(getVar("zv"), 1.0, 1e-12);
}

// ── Shape predicates ──────────────────────────────────────────
TEST_P(BuiltinTest, ShapePredicates)
{
    EXPECT_TRUE(evalBool("isvector([1 2 3]);"));
    EXPECT_TRUE(evalBool("isvector([1; 2; 3]);"));
    EXPECT_TRUE(evalBool("isvector(5);"));
    EXPECT_FALSE(evalBool("isvector([1 2; 3 4]);"));
    EXPECT_FALSE(evalBool("isvector([]);"));
    EXPECT_TRUE(evalBool("isrow([1 2 3]);"));
    EXPECT_FALSE(evalBool("isrow([1; 2; 3]);"));
    EXPECT_TRUE(evalBool("iscolumn([1; 2; 3]);"));
    EXPECT_FALSE(evalBool("iscolumn([1 2 3]);"));
    EXPECT_TRUE(evalBool("ismatrix([1 2; 3 4]);"));
    EXPECT_TRUE(evalBool("ismatrix(5);"));
    // 3-D arrays are not matrices.
    EXPECT_FALSE(evalBool("ismatrix(ones(2,2,2));"));
}

// ── Order predicates ──────────────────────────────────────────
TEST_P(BuiltinTest, IsSorted)
{
    EXPECT_TRUE(evalBool("issorted([1 2 3]);"));
    EXPECT_TRUE(evalBool("issorted([1 1 2 3]);"));
    EXPECT_FALSE(evalBool("issorted([1 3 2]);"));
    EXPECT_TRUE(evalBool("issorted([3 2 1], 'descend');"));
    EXPECT_FALSE(evalBool("issorted([1 1 2], 'strictascend');"));
    EXPECT_TRUE(evalBool("issorted([1 2 3], 'strictascend');"));
    EXPECT_TRUE(evalBool("issorted([3 2 1], 'monotonic');"));
    EXPECT_TRUE(evalBool("issorted([1 2 3], 'monotonic');"));
    EXPECT_FALSE(evalBool("issorted([1 3 2], 'monotonic');"));
    // Matrix: each column must be sorted.
    EXPECT_TRUE(evalBool("issorted([1 2; 3 4]);"));
    EXPECT_FALSE(evalBool("issorted([3 1; 1 2]);"));
}

TEST_P(BuiltinTest, IsSortedRows)
{
    EXPECT_TRUE(evalBool("issortedrows([1 2; 3 4]);"));
    EXPECT_FALSE(evalBool("issortedrows([3 4; 1 2]);"));
    // Equal first column → secondary key.
    EXPECT_TRUE(evalBool("issortedrows([1 2; 1 3; 2 1]);"));
    EXPECT_FALSE(evalBool("issortedrows([1 3; 1 2]);"));
}

TEST_P(BuiltinTest, IsUniform)
{
    EXPECT_TRUE(evalBool("isuniform([1 2 3 4 5]);"));
    EXPECT_TRUE(evalBool("isuniform(linspace(0, 1, 11));"));
    EXPECT_FALSE(evalBool("isuniform([1 2 4 8]);"));
    EXPECT_TRUE(evalBool("isuniform(5);"));
    EXPECT_TRUE(evalBool("isuniform([]);"));
}

// ── Numeric limits ────────────────────────────────────────────
TEST_P(BuiltinTest, NumericLimits)
{
    EXPECT_DOUBLE_EQ(evalScalar("flintmax();"), 9007199254740992.0);   // 2^53
    EXPECT_DOUBLE_EQ(evalScalar("flintmax('double');"), 9007199254740992.0);
    EXPECT_DOUBLE_EQ(evalScalar("flintmax('single');"), 16777216.0);   // 2^24

    EXPECT_DOUBLE_EQ(evalScalar("double(intmax());"), 2147483647.0);   // int32 max
    EXPECT_DOUBLE_EQ(evalScalar("double(intmax('int8'));"),  127.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(intmax('uint8'));"), 255.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(intmin('int8'));"), -128.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(intmin('uint8'));"), 0.0);

    EXPECT_NEAR(evalScalar("realmax();"), 1.7976931348623157e+308, 1e295);
    EXPECT_NEAR(evalScalar("realmin();"), 2.2250738585072014e-308, 1e-320);
}

// ── allfinite / anynan ────────────────────────────────────────
TEST_P(BuiltinTest, AllFiniteAnyNan)
{
    EXPECT_TRUE(evalBool("allfinite([1 2 3]);"));
    EXPECT_FALSE(evalBool("allfinite([1 inf 3]);"));
    EXPECT_FALSE(evalBool("allfinite([1 nan 3]);"));
    EXPECT_TRUE(evalBool("allfinite([]);"));
    EXPECT_TRUE(evalBool("allfinite(5);"));

    EXPECT_FALSE(evalBool("anynan([1 2 3]);"));
    EXPECT_TRUE(evalBool("anynan([1 nan 3]);"));
    EXPECT_FALSE(evalBool("anynan([1 inf 3]);"));   // inf is not nan
    EXPECT_FALSE(evalBool("anynan([]);"));
    EXPECT_FALSE(evalBool("anynan(5);"));

    // 2-D arrays.
    EXPECT_TRUE(evalBool("allfinite([1 2; 3 4]);"));
    EXPECT_FALSE(evalBool("allfinite([1 nan; 3 4]);"));
    EXPECT_TRUE(evalBool("anynan([1 nan; 3 4]);"));
}

TEST_P(BuiltinTest, CartPolVectorized)
{
    eval("[t, r] = cart2pol([3 0 -1], [4 1  0]);");
    auto *t = getVarPtr("t");
    auto *r = getVarPtr("r");
    ASSERT_EQ(t->numel(), 3u);
    ASSERT_EQ(r->numel(), 3u);
    EXPECT_NEAR(t->doubleData()[0], std::atan2(4.0, 3.0), 1e-12);
    EXPECT_NEAR(t->doubleData()[1], M_PI / 2,             1e-12);
    EXPECT_NEAR(t->doubleData()[2], M_PI,                 1e-12);
    EXPECT_NEAR(r->doubleData()[0], 5.0, 1e-12);
    EXPECT_NEAR(r->doubleData()[1], 1.0, 1e-12);
    EXPECT_NEAR(r->doubleData()[2], 1.0, 1e-12);
}

TEST_P(BuiltinTest, Floor)
{
    EXPECT_DOUBLE_EQ(evalScalar("floor(3.7);"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("floor(-3.2);"), -4.0);
}

TEST_P(BuiltinTest, Mod)
{
    EXPECT_DOUBLE_EQ(evalScalar("mod(7, 3);"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("mod(10, 5);"), 0.0);
}

// ── Reshape ─────────────────────────────────────────────────

TEST_P(BuiltinTest, Reshape)
{
    eval("A = [1 2 3 4 5 6]; B = reshape(A, 2, 3);");
    auto *B = getVarPtr("B");
    EXPECT_EQ(rows(*B), 2u);
    EXPECT_EQ(cols(*B), 3u);
}

// ── Error ───────────────────────────────────────────────────

TEST_P(BuiltinTest, ErrorFunction)
{
    EXPECT_THROW(eval("error('test error');"), std::runtime_error);
}

// ── String functions ────────────────────────────────────────

TEST_P(BuiltinTest, Strcmp)
{
    EXPECT_TRUE(evalBool("strcmp('hello', 'hello');"));
    EXPECT_FALSE(evalBool("strcmp('hello', 'world');"));
}

// ── Logspace ────────────────────────────────────────────────

TEST_P(BuiltinTest, BasicLogspace)
{
    eval("x = logspace(0, 3, 4);");
    auto *x = getVarPtr("x");
    ASSERT_NE(x, nullptr);
    EXPECT_EQ(x->numel(), 4u);
    EXPECT_NEAR(x->doubleData()[0], 1.0, 1e-10);
    EXPECT_NEAR(x->doubleData()[1], 10.0, 1e-10);
    EXPECT_NEAR(x->doubleData()[2], 100.0, 1e-10);
    EXPECT_NEAR(x->doubleData()[3], 1000.0, 1e-10);
}

TEST_P(BuiltinTest, LogspaceTwoPoints)
{
    eval("x = logspace(1, 2, 2);");
    auto *x = getVarPtr("x");
    EXPECT_NEAR(x->doubleData()[0], 10.0, 1e-10);
    EXPECT_NEAR(x->doubleData()[1], 100.0, 1e-10);
}

TEST_P(BuiltinTest, LogspaceDefaultN)
{
    eval("x = logspace(0, 1);");
    auto *x = getVarPtr("x");
    EXPECT_EQ(x->numel(), 50u);
    EXPECT_NEAR(x->doubleData()[0], 1.0, 1e-10);
    EXPECT_NEAR(x->doubleData()[49], 10.0, 1e-10);
}

TEST_P(BuiltinTest, LogspaceSinglePoint)
{
    eval("x = logspace(2, 2, 1);");
    auto *x = getVarPtr("x");
    EXPECT_EQ(x->numel(), 1u);
    EXPECT_NEAR(x->doubleData()[0], 100.0, 1e-10);
}

INSTANTIATE_DUAL(BuiltinTest);

// ============================================================
// Display / output
// ============================================================

class DisplayTest : public DualEngineTest {};

TEST_P(DisplayTest, SuppressOutput)
{
    capturedOutput.clear();
    eval("42;");
    EXPECT_TRUE(capturedOutput.empty());
}

TEST_P(DisplayTest, ShowOutput)
{
    capturedOutput.clear();
    eval("42");
    EXPECT_FALSE(capturedOutput.empty());
    EXPECT_NE(capturedOutput.find("42"), std::string::npos);
}

INSTANTIATE_DUAL(DisplayTest);
