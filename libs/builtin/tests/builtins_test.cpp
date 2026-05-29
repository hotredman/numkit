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

// ── Workspace / display — Pack 25 ─────────────────────────────
TEST_P(BuiltinTest, Clearvars)
{
    // Function-call form (avoids MATLAB command syntax for `-except`).
    eval("a = 1; b = 2; c = 3;");
    EXPECT_DOUBLE_EQ(getVar("a"), 1.0);
    eval("clearvars('b');");
    EXPECT_DOUBLE_EQ(getVar("a"), 1.0);
    EXPECT_DOUBLE_EQ(getVar("c"), 3.0);
    EXPECT_EQ(getVarPtr("b"), nullptr);

    // -except keeps only the listed names.
    eval("x = 1; y = 2; z = 3;");
    eval("clearvars('-except', 'y');");
    EXPECT_DOUBLE_EQ(getVar("y"), 2.0);
    EXPECT_EQ(getVarPtr("x"), nullptr);
    EXPECT_EQ(getVarPtr("z"), nullptr);
}

TEST_P(BuiltinTest, FormattedDisplayText)
{
    eval("s = formatteddisplaytext(42);");
    auto *s = getVarPtr("s");
    ASSERT_TRUE(s->isChar());
    // The exact formatting may include leading whitespace and trailing
    // newlines; just check that "42" appears in there.
    EXPECT_NE(s->toString().find("42"), std::string::npos);
}

TEST_P(BuiltinTest, FormatNoOp)
{
    // format accepts known specs without error.
    EXPECT_NO_THROW(eval("format short;"));
    EXPECT_NO_THROW(eval("format long;"));
    EXPECT_NO_THROW(eval("format compact;"));
    EXPECT_NO_THROW(eval("format();"));   // 0-arg also OK
    // Unknown spec throws.
    EXPECT_THROW(eval("format weird;"), std::exception);
}

// ── Cell / struct misc — Pack 24 ──────────────────────────────
TEST_P(BuiltinTest, Deal)
{
    eval("[a, b, c] = deal(1, 2, 3);");
    EXPECT_DOUBLE_EQ(getVar("a"), 1.0);
    EXPECT_DOUBLE_EQ(getVar("b"), 2.0);
    EXPECT_DOUBLE_EQ(getVar("c"), 3.0);
    // Single input broadcasts.
    eval("[x, y, z] = deal(5);");
    EXPECT_DOUBLE_EQ(getVar("x"), 5.0);
    EXPECT_DOUBLE_EQ(getVar("y"), 5.0);
    EXPECT_DOUBLE_EQ(getVar("z"), 5.0);
}

TEST_P(BuiltinTest, Mat2cell)
{
    // 2-D split: 4×6 matrix → 2 row-blocks of 2 rows, 3 col-blocks of 2 cols.
    eval("A = reshape(1:24, 4, 6); C = mat2cell(A, [2 2], [2 2 2]);");
    auto *C = getVarPtr("C");
    ASSERT_TRUE(C->isCell());
    EXPECT_EQ(rows(*C), 2u);
    EXPECT_EQ(cols(*C), 3u);
    EXPECT_EQ(rows(C->cellAt(0)), 2u);   // C{1,1}
    EXPECT_EQ(cols(C->cellAt(0)), 2u);
    // Row-vector form.
    eval("v = 1:6; W = mat2cell(v, [3 3]);");
    auto *W = getVarPtr("W");
    ASSERT_TRUE(W->isCell());
    EXPECT_EQ(W->numel(), 2u);
    EXPECT_EQ(W->cellAt(0).numel(), 3u);
}

// ── Numeric base conversion + rat — Pack 23 ───────────────────
TEST_P(BuiltinTest, Dec2BinHex)
{
    eval("a = dec2bin(5);");
    EXPECT_EQ(getVarPtr("a")->toString(), "101");
    eval("b = dec2bin(5, 8);");
    EXPECT_EQ(getVarPtr("b")->toString(), "00000101");
    eval("c = dec2hex(255);");
    EXPECT_EQ(getVarPtr("c")->toString(), "FF");
    eval("d = dec2hex(16, 4);");
    EXPECT_EQ(getVarPtr("d")->toString(), "0010");
}

TEST_P(BuiltinTest, Bin2DecHex2Dec)
{
    EXPECT_DOUBLE_EQ(evalScalar("bin2dec('101');"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("bin2dec('00000101');"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("hex2dec('FF');"), 255.0);
    EXPECT_DOUBLE_EQ(evalScalar("hex2dec('1A');"), 26.0);
    EXPECT_DOUBLE_EQ(evalScalar("hex2dec('ff');"), 255.0);  // case-insensitive
}

// dec2base / base2dec — arbitrary radix 2..36. vs MATLAB R2025b.
TEST_P(BuiltinTest, Dec2BaseBase2Dec)
{
    eval("a = dec2base(100, 16);");
    EXPECT_EQ(getVarPtr("a")->toString(), "64");
    eval("b = dec2base(10, 2, 8);");
    EXPECT_EQ(getVarPtr("b")->toString(), "00001010");
    eval("z = dec2base(35, 36);");
    EXPECT_EQ(getVarPtr("z")->toString(), "Z");      // base-36 digit
    EXPECT_DOUBLE_EQ(evalScalar("base2dec('64', 16);"), 100.0);
    EXPECT_DOUBLE_EQ(evalScalar("base2dec('1010', 2);"), 10.0);
    EXPECT_DOUBLE_EQ(evalScalar("base2dec('Z', 36);"), 35.0);
    EXPECT_DOUBLE_EQ(evalScalar("base2dec('z', 36);"), 35.0);  // case-insensitive
    // char matrix -> column vector.
    eval("B = base2dec(['64';'1A'], 16);");
    EXPECT_DOUBLE_EQ(evalScalar("B(1)"), 100.0);
    EXPECT_DOUBLE_EQ(evalScalar("B(2)"), 26.0);
}

TEST_P(BuiltinTest, Rat)
{
    // After the audit ТЗ closure (2026-05-09) numkit's `rat()` returns
    // MATLAB R2025b's nested continued-fraction string format using the
    // regularized expansion (round() not floor()), e.g. 0.5 → '1 + 1/(-2)'
    // (NOT the simple-CF '0 + 1/(2)'). See libs/builtin/tests/rat_test.cpp
    // for the full coverage; these three regressions are kept as canaries.
    eval("r = rat(0.5);");
    EXPECT_EQ(getVarPtr("r")->toString(), "1 + 1/(-2)");
    eval("r2 = rat(0.333333);");
    EXPECT_EQ(getVarPtr("r2")->toString(), "0 + 1/(3)");
    eval("r3 = rat(2);");
    EXPECT_EQ(getVarPtr("r3")->toString(), "2");
}

// ── Extract / insert family — Pack 22 ─────────────────────────
TEST_P(BuiltinTest, ExtractAfterBefore)
{
    // String pattern.
    eval("s = extractAfter('hello world', ' ');");
    EXPECT_EQ(getVarPtr("s")->toString(), "world");
    eval("s2 = extractBefore('hello world', ' ');");
    EXPECT_EQ(getVarPtr("s2")->toString(), "hello");
    // Numeric position.
    eval("s3 = extractAfter('abcdef', 3);");
    EXPECT_EQ(getVarPtr("s3")->toString(), "def");
    eval("s4 = extractBefore('abcdef', 3);");
    EXPECT_EQ(getVarPtr("s4")->toString(), "ab");
    // Pattern not found → empty.
    eval("s5 = extractAfter('hello', 'xyz');");
    EXPECT_EQ(getVarPtr("s5")->toString(), "");
}

TEST_P(BuiltinTest, ExtractBetween)
{
    // MATLAB extractBetween always returns an Mx1 cell (even for a
    // single match) - non-overlapping match list. See BUGS.md #27.
    eval("s = extractBetween('[hello] [world]', '[', ']');");
    auto *s = getVarPtr("s");
    ASSERT_TRUE(s->isCell());
    ASSERT_EQ(s->numel(), 2u);
    EXPECT_EQ(s->cellAt(0).toString(), "hello");
    EXPECT_EQ(s->cellAt(1).toString(), "world");

    // Same-delimiter case: 'a-b-c-d' yields one non-overlapping pair.
    eval("s2 = extractBetween('a-b-c-d', '-', '-');");
    auto *s2 = getVarPtr("s2");
    ASSERT_TRUE(s2->isCell());
    ASSERT_EQ(s2->numel(), 1u);
    EXPECT_EQ(s2->cellAt(0).toString(), "b");

    // Single match → 1x1 cell.
    eval("s3 = extractBetween('foo<<a>>bar', '<<', '>>');");
    auto *s3 = getVarPtr("s3");
    ASSERT_TRUE(s3->isCell());
    ASSERT_EQ(s3->numel(), 1u);
    EXPECT_EQ(s3->cellAt(0).toString(), "a");
}

TEST_P(BuiltinTest, InsertAfterBefore)
{
    eval("s = insertAfter('hello', 'lo', ' world');");
    EXPECT_EQ(getVarPtr("s")->toString(), "hello world");
    eval("s2 = insertBefore('world', 'world', 'hello ');");
    EXPECT_EQ(getVarPtr("s2")->toString(), "hello world");
    // Numeric position.
    eval("s3 = insertAfter('abcdef', 3, 'XYZ');");
    EXPECT_EQ(getVarPtr("s3")->toString(), "abcXYZdef");
}

TEST_P(BuiltinTest, EraseReplaceBetween)
{
    eval("s = eraseBetween('a[junk]b', '[', ']');");
    EXPECT_EQ(getVarPtr("s")->toString(), "a[]b");
    eval("s2 = replaceBetween('foo<old>bar', '<', '>', 'NEW');");
    EXPECT_EQ(getVarPtr("s2")->toString(), "foo<NEW>bar");
}

// ── String conversion + char predicates — Pack 21 ─────────────
TEST_P(BuiltinTest, ConvertCharsStrings)
{
    eval("s = convertCharsToStrings('hello');");
    auto *s = getVarPtr("s");
    EXPECT_TRUE(s->isString());
    EXPECT_EQ(s->toString(), "hello");

    eval("c = convertStringsToChars(\"world\");");
    auto *c = getVarPtr("c");
    EXPECT_TRUE(c->isChar());
    EXPECT_EQ(c->toString(), "world");

    EXPECT_TRUE(evalBool("isstringscalar(\"hi\");"));
    EXPECT_FALSE(evalBool("isstringscalar('hi');"));   // char, not string
    EXPECT_FALSE(evalBool("isstringscalar(1);"));
}

TEST_P(BuiltinTest, IsstrpropAndFamily)
{
    // isstrprop on 'aB1!'.
    eval("a = isstrprop('aB1!', 'alpha');");
    auto *a = getVarPtr("a");
    ASSERT_EQ(a->numel(), 4u);
    EXPECT_EQ(a->logicalData()[0], 1);   // a
    EXPECT_EQ(a->logicalData()[1], 1);   // B
    EXPECT_EQ(a->logicalData()[2], 0);   // 1
    EXPECT_EQ(a->logicalData()[3], 0);   // !

    eval("d = isstrprop('a1B2', 'digit');");
    auto *d = getVarPtr("d");
    EXPECT_EQ(d->logicalData()[0], 0);
    EXPECT_EQ(d->logicalData()[1], 1);
    EXPECT_EQ(d->logicalData()[2], 0);
    EXPECT_EQ(d->logicalData()[3], 1);

    // isletter / isspace shortcuts.
    eval("L = isletter('a B 1');");
    auto *L = getVarPtr("L");
    EXPECT_EQ(L->logicalData()[0], 1);
    EXPECT_EQ(L->logicalData()[1], 0);   // space
    EXPECT_EQ(L->logicalData()[2], 1);

    eval("S = isspace('a b\tc');");
    auto *S = getVarPtr("S");
    EXPECT_EQ(S->logicalData()[0], 0);   // a
    EXPECT_EQ(S->logicalData()[1], 1);   // space
    EXPECT_EQ(S->logicalData()[3], 1);   // tab
}

// ── Optimization — Pack 20 ────────────────────────────────────
TEST_P(BuiltinTest, FminbndScalar)
{
    // Min of (x-3)^2 on [0, 10] is x=3, value=0.
    eval("y = fminbnd(@(x) (x-3)^2, 0, 10);");
    EXPECT_NEAR(getVar("y"), 3.0, 1e-4);
    // Min of cos(x) on [0, 2*pi] is at x=pi.
    eval("y2 = fminbnd(@cos, 0, 6);");
    EXPECT_NEAR(getVar("y2"), M_PI, 1e-4);
}

TEST_P(BuiltinTest, FminsearchVector)
{
    // 2-D quadratic: min of (x1-1)^2 + 10*(x2-2)^2 at [1, 2].
    // Nelder-Mead with default 1e-4 tol converges to ~1e-2.
    eval("y = fminsearch(@(v) (v(1)-1)^2 + 10*(v(2)-2)^2, [0 0]);");
    auto *y = getVarPtr("y");
    ASSERT_EQ(y->numel(), 2u);
    EXPECT_NEAR(y->doubleData()[0], 1.0, 1e-2);
    EXPECT_NEAR(y->doubleData()[1], 2.0, 1e-2);
}

// [x, fval, exitflag] multi-output for fzero / fminbnd / fminsearch.
TEST_P(BuiltinTest, FzeroFvalExitflag)
{
    eval("function [a,b,c] = fz(f, x0)\n  [a,b,c] = fzero(f, x0);\nend");
    eval("[x, fval, ef] = fz(@(x) x.^2 - 4, [0 10]);");
    EXPECT_NEAR(getVar("x"),    2.0, 1e-10);
    EXPECT_NEAR(getVar("fval"), 0.0, 1e-10);
    EXPECT_DOUBLE_EQ(getVar("ef"), 1.0);
}

TEST_P(BuiltinTest, FminbndFvalExitflag)
{
    eval("function [a,b,c] = fb(f, lo, hi)\n  [a,b,c] = fminbnd(f, lo, hi);\nend");
    eval("[x, fval, ef] = fb(@(x) (x-3).^2 + 1, 0, 10);");
    EXPECT_NEAR(getVar("x"),    3.0, 1e-6);
    EXPECT_NEAR(getVar("fval"), 1.0, 1e-9);
    EXPECT_DOUBLE_EQ(getVar("ef"), 1.0);
}

TEST_P(BuiltinTest, FminsearchFvalExitflag)
{
    eval("function [a,b,c] = fs(f, x0)\n  [a,b,c] = fminsearch(f, x0);\nend");
    eval("[x, fval, ef] = fs(@(v) (v(1)-1)^2 + (v(2)-2)^2, [0 0]);");
    auto *x = getVarPtr("x");
    ASSERT_EQ(x->numel(), 2u);
    EXPECT_NEAR(x->doubleData()[0], 1.0, 1e-2);
    EXPECT_NEAR(x->doubleData()[1], 2.0, 1e-2);
    EXPECT_NEAR(getVar("fval"), 0.0, 1e-3);
    EXPECT_DOUBLE_EQ(getVar("ef"), 1.0);
}

TEST_P(BuiltinTest, FzeroOutputStructDeferredThrows)
{
    eval("function [a,b,c,d] = fz4(f, x0)\n  [a,b,c,d] = fzero(f, x0);\nend");
    EXPECT_THROW(eval("[x,fv,ef,op] = fz4(@(x) x.^2 - 4, [0 10]);"), std::exception);
}

TEST_P(BuiltinTest, OptimsetGet)
{
    eval("o = optimset('TolX', 1e-9, 'MaxIter', 200);");
    EXPECT_DOUBLE_EQ(evalScalar("optimget(o, 'TolX');"), 1e-9);
    EXPECT_DOUBLE_EQ(evalScalar("optimget(o, 'MaxIter');"), 200.0);
    // Default is preserved when not overridden.
    EXPECT_DOUBLE_EQ(evalScalar("optimget(o, 'TolFun');"), 1e-6);
    // Missing key with user default.
    EXPECT_DOUBLE_EQ(evalScalar("optimget(o, 'NoSuch', 42);"), 42.0);
}

// ── Pack 35 ───────────────────────────────────────────────────
TEST_P(BuiltinTest, ErfcinvErfcx)
{
    // erfcinv(1) = 0 (since erfc(0) = 1).
    EXPECT_NEAR(evalScalar("erfcinv(1);"), 0.0, 1e-12);
    // erfcinv(2) = -Inf, erfcinv(0) = +Inf.
    EXPECT_TRUE(std::isinf(evalScalar("erfcinv(0);")));
    EXPECT_TRUE(std::isinf(evalScalar("erfcinv(2);")));
    // Round-trip: erfc(erfcinv(0.5)) ≈ 0.5.
    EXPECT_NEAR(evalScalar("erfc(erfcinv(0.5));"), 0.5, 1e-10);

    // erfcx(0) = erfc(0) = 1.
    EXPECT_NEAR(evalScalar("erfcx(0);"), 1.0, 1e-12);
    // erfcx(1) ≈ exp(1) * erfc(1).
    EXPECT_NEAR(evalScalar("erfcx(1);"),
                std::exp(1.0) * std::erfc(1.0), 1e-10);
    // Large x: erfcx(50) ≈ 1/(x·sqrt(π)) to leading order. Our series
    // adds the (1 - 0.5/x² + ...) correction, so the leading-order
    // approximation matches only to ~2e-6 absolute / ~2e-4 relative.
    EXPECT_NEAR(evalScalar("erfcx(50);"),
                1.0 / (50.0 * std::sqrt(M_PI)), 5e-6);
}

TEST_P(BuiltinTest, ConvertContainedStringsToChars)
{
    // Cell with mixed entries gets the strings converted to char.
    eval("c = convertContainedStringsToChars({\"hello\", 1, {\"nested\", 2}});");
    auto *c = getVarPtr("c");
    ASSERT_TRUE(c->isCell());
    EXPECT_TRUE(c->cellAt(0).isChar());
    EXPECT_EQ(c->cellAt(0).toString(), "hello");
    EXPECT_DOUBLE_EQ(c->cellAt(1).toScalar(), 1.0);
    EXPECT_TRUE(c->cellAt(2).isCell());
    EXPECT_TRUE(c->cellAt(2).cellAt(0).isChar());
    EXPECT_EQ(c->cellAt(2).cellAt(0).toString(), "nested");
    // Plain numeric / char passes through.
    eval("d = convertContainedStringsToChars(42);");
    EXPECT_DOUBLE_EQ(getVar("d"), 42.0);
}

// ── Function-handle introspection — Pack 34 ───────────────────
TEST_P(BuiltinTest, Functions)
{
    // The .function field name collides with the `function` keyword,
    // so use getfield for read-side access.
    eval("s = functions(@sin);");
    eval("nm = getfield(s, 'function');");
    EXPECT_EQ(getVarPtr("nm")->toString(), "sin");
    eval("ty = s.type;");
    EXPECT_EQ(getVarPtr("ty")->toString(), "simple");
}

TEST_P(BuiltinTest, LocalFunctions)
{
    eval("c = localfunctions();");
    auto *c = getVarPtr("c");
    ASSERT_TRUE(c->isCell());
    EXPECT_EQ(c->numel(), 0u);
}

// ── idivide / bsxfun — Pack 33 ────────────────────────────────
TEST_P(BuiltinTest, Idivide)
{
    // MATLAB rule: at least one operand must be of integer class. The
    // other can be the same-class integer or a scalar double. Result
    // type matches the integer operand. See BUGS.md #29.
    EXPECT_EQ(evalScalar("double(idivide(int32(10), int32(3)));"),  3.0);
    EXPECT_EQ(evalScalar("double(idivide(int32(-10), int32(3)));"), -3.0);
    EXPECT_EQ(evalScalar("double(idivide(int32(-10), int32(3), 'floor'));"), -4.0);
    EXPECT_EQ(evalScalar("double(idivide(int32(7), int32(2), 'ceil'));"),    4.0);
    EXPECT_EQ(evalScalar("double(idivide(int32(7), int32(2), 'round'));"),   4.0);
    // Mixed: integer array + scalar double divisor.
    eval("v = idivide(int32([10 20 30]), 4);");
    auto *v = getVarPtr("v");
    ASSERT_EQ(v->numel(), 3u);
    EXPECT_EQ(v->type(), ValueType::INT32);
    EXPECT_EQ(v->int32Data()[0], 2);   // fix(2.5) = 2
    EXPECT_EQ(v->int32Data()[1], 5);
    EXPECT_EQ(v->int32Data()[2], 7);   // fix(7.5) = 7
}

TEST_P(BuiltinTest, IdivideRejectsDoubleDouble)
{
    // MATLAB R2025b: idivide(double, double) → "At least one argument
    // must belong to an integer class."
    EXPECT_THROW(eval("y = idivide(10, 3);"), std::runtime_error);
}

TEST_P(BuiltinTest, Bsxfun)
{
    // bsxfun(@plus, [1 2 3], [10; 20]) → 2×3 broadcast.
    eval("M = bsxfun(@plus, [1 2 3], [10; 20]);");
    auto *M = getVarPtr("M");
    ASSERT_EQ(rows(*M), 2u);
    ASSERT_EQ(cols(*M), 3u);
    EXPECT_DOUBLE_EQ((*M)(0, 0), 11.0);
    EXPECT_DOUBLE_EQ((*M)(0, 2), 13.0);
    EXPECT_DOUBLE_EQ((*M)(1, 0), 21.0);
    EXPECT_DOUBLE_EQ((*M)(1, 2), 23.0);
    // Custom anon function.
    eval("M2 = bsxfun(@(a, b) a .* b + 1, [1 2 3], [10; 20]);");
    auto *M2 = getVarPtr("M2");
    EXPECT_DOUBLE_EQ((*M2)(0, 0), 11.0);
    EXPECT_DOUBLE_EQ((*M2)(1, 2), 61.0);
}

// ── Array shape pads — Pack 32 ────────────────────────────────
TEST_P(BuiltinTest, PaddataTrimdata)
{
    eval("p = paddata([1 2 3], 6);");
    auto *p = getVarPtr("p");
    ASSERT_EQ(p->numel(), 6u);
    EXPECT_DOUBLE_EQ(p->doubleData()[0], 1.0);
    EXPECT_DOUBLE_EQ(p->doubleData()[2], 3.0);
    EXPECT_DOUBLE_EQ(p->doubleData()[3], 0.0);
    EXPECT_DOUBLE_EQ(p->doubleData()[5], 0.0);
    // Already long enough: pass-through.
    eval("p2 = paddata([1 2 3 4 5], 3);");
    EXPECT_EQ(getVarPtr("p2")->numel(), 5u);

    eval("t = trimdata([1 2 3 4 5], 3);");
    auto *t = getVarPtr("t");
    EXPECT_EQ(t->numel(), 3u);
    EXPECT_DOUBLE_EQ(t->doubleData()[0], 1.0);
    EXPECT_DOUBLE_EQ(t->doubleData()[2], 3.0);

    // resize covers both: pad to 6, trim to 2.
    eval("r = resize([1 2 3], 6);");
    EXPECT_EQ(getVarPtr("r")->numel(), 6u);
    eval("r2 = resize([1 2 3 4 5], 2);");
    EXPECT_EQ(getVarPtr("r2")->numel(), 2u);

    // Column orientation preserved.
    eval("c = paddata([1; 2; 3], 5);");
    auto *c = getVarPtr("c");
    EXPECT_EQ(rows(*c), 5u);
    EXPECT_EQ(cols(*c), 1u);
}

// ── Misc — Pack 31 ────────────────────────────────────────────
TEST_P(BuiltinTest, Freqspace)
{
    // MATLAB-spec: freqspace(n) default form is half-spectrum:
    //   even n → n/2+1 points on [0, 1]
    //   odd n  → (n+1)/2 points on [0, 1 - 1/n]
    // 'whole' form: n points on [0, 2 - 2/n].
    // See BUGS.md #19.
    eval("f = freqspace(4);");
    auto *f = getVarPtr("f");
    ASSERT_EQ(f->numel(), 3u);
    EXPECT_NEAR(f->doubleData()[0], 0.0, 1e-12);
    EXPECT_NEAR(f->doubleData()[1], 0.5, 1e-12);
    EXPECT_NEAR(f->doubleData()[2], 1.0, 1e-12);
    // 'whole' returns n elements on [0, 2-2/n].
    eval("g = freqspace(4, 'whole');");
    auto *g = getVarPtr("g");
    ASSERT_EQ(g->numel(), 4u);
    EXPECT_NEAR(g->doubleData()[0], 0.0, 1e-12);
    EXPECT_NEAR(g->doubleData()[3], 1.5, 1e-12);
}

TEST_P(BuiltinTest, HeadTail)
{
    eval("A = (1:10)';");  // 10x1 column vector
    eval("h = head(A);");  // default n = min(8, 10) = 8
    auto *h = getVarPtr("h");
    EXPECT_EQ(h->numel(), 8u);
    EXPECT_DOUBLE_EQ(h->doubleData()[0], 1.0);
    EXPECT_DOUBLE_EQ(h->doubleData()[7], 8.0);

    eval("t = tail(A, 3);");
    auto *t = getVarPtr("t");
    EXPECT_EQ(t->numel(), 3u);
    EXPECT_DOUBLE_EQ(t->doubleData()[0], 8.0);
    EXPECT_DOUBLE_EQ(t->doubleData()[2], 10.0);
}

TEST_P(BuiltinTest, Iskeyword)
{
    EXPECT_TRUE(evalBool("iskeyword('for');"));
    EXPECT_TRUE(evalBool("iskeyword('while');"));
    EXPECT_TRUE(evalBool("iskeyword('end');"));
    EXPECT_FALSE(evalBool("iskeyword('foo');"));
    EXPECT_FALSE(evalBool("iskeyword('Sin');"));   // case-sensitive
    // 0-arg returns the cell of all keywords.
    eval("kw = iskeyword();");
    auto *kw = getVarPtr("kw");
    ASSERT_TRUE(kw->isCell());
    EXPECT_GT(kw->numel(), 10u);
}

// ── Piecewise polynomial — Pack 30 ────────────────────────────
TEST_P(BuiltinTest, MkppPpvalUnmkpp)
{
    // Two pieces, both linear: piece 1 is x in [0,1], polynomial 2x;
    // piece 2 is x-1 mapped to coefs [1 1] → 1·u+1 with u=x-1.
    eval("pp = mkpp([0 1 2], [2 0; 1 1]);");
    EXPECT_DOUBLE_EQ(evalScalar("ppval(pp, 0);"),    0.0);  // piece 0: 2·0 + 0
    EXPECT_DOUBLE_EQ(evalScalar("ppval(pp, 0.5);"),  1.0);  // piece 0: 2·0.5
    // Closed-left, open-right convention: x=1 is the start of piece 1.
    // u = 1 - 1 = 0 → 1·0 + 1 = 1.
    EXPECT_DOUBLE_EQ(evalScalar("ppval(pp, 1);"),    1.0);
    EXPECT_DOUBLE_EQ(evalScalar("ppval(pp, 1.5);"),  1.5);  // piece 1: 1·0.5 + 1
    EXPECT_DOUBLE_EQ(evalScalar("ppval(pp, 2);"),    2.0);  // piece 1: 1·1 + 1

    // unmkpp returns breaks, coefs, pieces, order, dim.
    eval("[b, c, l, k, d] = unmkpp(pp);");
    auto *b = getVarPtr("b");
    EXPECT_EQ(b->numel(), 3u);
    EXPECT_DOUBLE_EQ(getVar("l"), 2.0);
    EXPECT_DOUBLE_EQ(getVar("k"), 2.0);
}

// ── Polynomial extras — Pack 29 ───────────────────────────────
TEST_P(BuiltinTest, PolyFromRoots)
{
    // poly([1 2]) = [1 -3 2] (coefficients of (x-1)(x-2) = x²-3x+2).
    eval("p = poly([1 2]);");
    auto *p = getVarPtr("p");
    ASSERT_EQ(p->numel(), 3u);
    EXPECT_DOUBLE_EQ(p->doubleData()[0],  1.0);
    EXPECT_DOUBLE_EQ(p->doubleData()[1], -3.0);
    EXPECT_DOUBLE_EQ(p->doubleData()[2],  2.0);
    // poly([0]) = [1 0].
    eval("p2 = poly([0]);");
    auto *p2 = getVarPtr("p2");
    EXPECT_DOUBLE_EQ(p2->doubleData()[0], 1.0);
    EXPECT_DOUBLE_EQ(p2->doubleData()[1], 0.0);
}

TEST_P(BuiltinTest, Polyvalm)
{
    // p(x) = x² - 3x + 2; A = [1 0; 0 2]; p(A) = [1 0; 0 4] - 3·[1 0; 0 2] + 2·I
    //      = [1-3+2  0; 0  4-6+2] = [0 0; 0 0].
    eval("A = [1 0; 0 2]; M = polyvalm([1 -3 2], A);");
    auto *M = getVarPtr("M");
    ASSERT_EQ(rows(*M), 2u);
    EXPECT_NEAR((*M)(0, 0), 0.0, 1e-12);
    EXPECT_NEAR((*M)(1, 1), 0.0, 1e-12);
    // Identity polynomial p(x) = x → p(A) = A.
    eval("M2 = polyvalm([1 0], [1 2; 3 4]);");
    auto *M2 = getVarPtr("M2");
    EXPECT_DOUBLE_EQ((*M2)(0, 0), 1.0);
    EXPECT_DOUBLE_EQ((*M2)(1, 1), 4.0);
}

TEST_P(BuiltinTest, Polydiv)
{
    // (x³ - 1) / (x - 1) = x² + x + 1, remainder 0.
    eval("[q, r] = polydiv([1 0 0 -1], [1 -1]);");
    auto *q = getVarPtr("q");
    auto *r = getVarPtr("r");
    ASSERT_EQ(q->numel(), 3u);
    EXPECT_DOUBLE_EQ(q->doubleData()[0], 1.0);
    EXPECT_DOUBLE_EQ(q->doubleData()[1], 1.0);
    EXPECT_DOUBLE_EQ(q->doubleData()[2], 1.0);
    EXPECT_DOUBLE_EQ(r->doubleData()[0], 0.0);
    // (x² + 3x + 2) / (x + 1) = x + 2, remainder 0.
    eval("[q2, r2] = polydiv([1 3 2], [1 1]);");
    auto *q2 = getVarPtr("q2");
    EXPECT_DOUBLE_EQ(q2->doubleData()[0], 1.0);
    EXPECT_DOUBLE_EQ(q2->doubleData()[1], 2.0);
}

// ── Hankel + elliptic — Pack 28 ───────────────────────────────
TEST_P(BuiltinTest, Besselh)
{
    // H_0^(1)(1) = J_0(1) + i·Y_0(1).
    eval("h = besselh(0, 1, 1);");
    auto *h = getVarPtr("h");
    ASSERT_TRUE(h->isComplex());
    const auto c = h->toComplex();
    EXPECT_NEAR(c.real(), 0.7651976865579666, 1e-10);  // J_0(1)
    EXPECT_NEAR(c.imag(), 0.0882569642156769, 1e-10);  // Y_0(1)

    // H_0^(2)(1) flips Y sign.
    eval("h2 = besselh(0, 2, 1);");
    auto *h2 = getVarPtr("h2");
    const auto c2 = h2->toComplex();
    EXPECT_NEAR(c2.imag(), -0.0882569642156769, 1e-10);
}

TEST_P(BuiltinTest, EllipKE)
{
    // K(0) = π/2, E(0) = π/2.
    eval("[K, E] = ellipke(0);");
    EXPECT_NEAR(getVar("K"), M_PI / 2, 1e-12);
    EXPECT_NEAR(getVar("E"), M_PI / 2, 1e-12);
    // K(1) = Inf, E(1) = 1.
    eval("[K1, E1] = ellipke(1);");
    EXPECT_TRUE(std::isinf(getVar("K1")));
    EXPECT_NEAR(getVar("E1"), 1.0, 1e-12);
    // K(0.5) ≈ 1.854074677301372, E(0.5) ≈ 1.350643881047676.
    eval("[Km, Em] = ellipke(0.5);");
    EXPECT_NEAR(getVar("Km"), 1.854074677301372, 1e-10);
    EXPECT_NEAR(getVar("Em"), 1.350643881047676, 1e-10);
}

// ── Bessel — Pack 27 ──────────────────────────────────────────
TEST_P(BuiltinTest, BesselFamily)
{
    // J_0(0) = 1, J_n(0) = 0 for n ≥ 1.
    EXPECT_NEAR(evalScalar("besselj(0, 0);"), 1.0, 1e-12);
    EXPECT_NEAR(evalScalar("besselj(1, 0);"), 0.0, 1e-12);
    // J_1(1) ≈ 0.4400505857449335.
    EXPECT_NEAR(evalScalar("besselj(1, 1);"), 0.4400505857449335, 1e-10);
    // I_0(0) = 1.
    EXPECT_NEAR(evalScalar("besseli(0, 0);"), 1.0, 1e-12);
    // I_0(1) ≈ 1.2660658777520084.
    EXPECT_NEAR(evalScalar("besseli(0, 1);"), 1.2660658777520084, 1e-10);
    // K_0(1) ≈ 0.4210244382407083 — modified Bessel 2nd kind.
    EXPECT_NEAR(evalScalar("besselk(0, 1);"), 0.4210244382407083, 1e-10);
    // Y_0(1) ≈ 0.0882569642156769.
    EXPECT_NEAR(evalScalar("bessely(0, 1);"), 0.0882569642156769, 1e-10);
}

// ── More special funcs — Pack 26 ──────────────────────────────
TEST_P(BuiltinTest, GammaInc)
{
    // P(1, x) = 1 - exp(-x).
    EXPECT_NEAR(evalScalar("gammainc(1, 1);"),  1.0 - std::exp(-1.0), 1e-12);
    EXPECT_NEAR(evalScalar("gammainc(0.5, 1);"),1.0 - std::exp(-0.5), 1e-12);
    // P(a, 0) = 0.
    EXPECT_DOUBLE_EQ(evalScalar("gammainc(0, 2);"), 0.0);
    // P(2, ∞) approaches 1.
    EXPECT_NEAR(evalScalar("gammainc(50, 2);"), 1.0, 1e-10);
}

TEST_P(BuiltinTest, BetaInc)
{
    // I_0.5(1, 1) = 0.5.
    EXPECT_NEAR(evalScalar("betainc(0.5, 1, 1);"), 0.5, 1e-12);
    // I_0(a, b) = 0; I_1(a, b) = 1.
    EXPECT_DOUBLE_EQ(evalScalar("betainc(0, 2, 3);"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("betainc(1, 2, 3);"), 1.0);
    // I_0.5(2, 2) = 0.5 (symmetric).
    EXPECT_NEAR(evalScalar("betainc(0.5, 2, 2);"), 0.5, 1e-12);
}

TEST_P(BuiltinTest, Legendre)
{
    // P_0(x) = 1.
    eval("p = legendre(0, 0.5);");
    auto *p = getVarPtr("p");
    EXPECT_DOUBLE_EQ(p->doubleData()[0], 1.0);
    // P_1(0.5) = 0.5 (P_1^0); P_1^1(0.5) = -sqrt(1-0.25) ≈ -0.8660254.
    eval("p1 = legendre(1, 0.5);");
    auto *p1 = getVarPtr("p1");
    EXPECT_NEAR(p1->doubleData()[0],  0.5, 1e-12);
    EXPECT_NEAR(p1->doubleData()[1], -std::sqrt(0.75), 1e-12);
    // P_2(1) = 1 for all m, except P_2^m(1) = 0 for m > 0.
    eval("p2 = legendre(2, 1);");
    auto *p2 = getVarPtr("p2");
    EXPECT_NEAR(p2->doubleData()[0], 1.0, 1e-12);
    EXPECT_NEAR(p2->doubleData()[1], 0.0, 1e-12);
    EXPECT_NEAR(p2->doubleData()[2], 0.0, 1e-12);
}

// ── Special funcs — Pack 19 ───────────────────────────────────
TEST_P(BuiltinTest, BetaBetaln)
{
    // B(1, 1) = 1.
    EXPECT_NEAR(evalScalar("beta(1, 1);"), 1.0, 1e-12);
    // B(2, 3) = 1/12.
    EXPECT_NEAR(evalScalar("beta(2, 3);"), 1.0 / 12.0, 1e-12);
    // B(0.5, 0.5) = pi.
    EXPECT_NEAR(evalScalar("beta(0.5, 0.5);"), M_PI, 1e-12);
    // betaln consistency.
    EXPECT_NEAR(evalScalar("betaln(2, 3);"), std::log(1.0 / 12.0), 1e-12);
    EXPECT_NEAR(evalScalar("exp(betaln(0.5, 0.5));"), M_PI, 1e-12);
}

TEST_P(BuiltinTest, ExpintAndPsi)
{
    // E1(1) ≈ 0.21938393439552027
    EXPECT_NEAR(evalScalar("expint(1);"), 0.21938393439552027, 1e-10);
    // E1(0.5) ≈ 0.5597735947761607
    EXPECT_NEAR(evalScalar("expint(0.5);"), 0.5597735947761607, 1e-10);
    // E1(5) ≈ 0.001148295591742
    EXPECT_NEAR(evalScalar("expint(5);"), 0.001148295591742, 1e-12);
    // E1(0) = +Inf.
    EXPECT_TRUE(std::isinf(evalScalar("expint(0);")));

    // psi(1) = -γ ≈ -0.5772156649015329
    EXPECT_NEAR(evalScalar("psi(1);"), -0.5772156649015329, 1e-10);
    // psi(2) = 1 - γ.
    EXPECT_NEAR(evalScalar("psi(2);"), 1.0 - 0.5772156649015329, 1e-10);
    // psi(0.5) = -γ - 2*ln(2).
    EXPECT_NEAR(evalScalar("psi(0.5);"),
                -0.5772156649015329 - 2.0 * std::log(2.0), 1e-10);
}

// ── String utils — Pack 18 ────────────────────────────────────
TEST_P(BuiltinTest, AppendCountErase)
{
    eval("a = append('foo', 'bar', 'baz');");
    EXPECT_EQ(getVarPtr("a")->toString(), "foobarbaz");
    EXPECT_DOUBLE_EQ(evalScalar("count('abracadabra', 'a');"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("count('aaaa', 'aa');"), 2.0);  // non-overlapping
    eval("e = erase('hello world', 'l');");
    EXPECT_EQ(getVarPtr("e")->toString(), "heo word");
    eval("e2 = erase('foofoofoo', 'foo');");
    EXPECT_EQ(getVarPtr("e2")->toString(), "");
}

TEST_P(BuiltinTest, ReverseReplaceMatches)
{
    eval("r = reverse('hello');");
    EXPECT_EQ(getVarPtr("r")->toString(), "olleh");
    eval("rp = replace('foo bar foo', 'foo', 'baz');");
    EXPECT_EQ(getVarPtr("rp")->toString(), "baz bar baz");
    EXPECT_TRUE(evalBool("matches('cat', 'cat');"));
    EXPECT_FALSE(evalBool("matches('cat', 'dog');"));
    EXPECT_TRUE(evalBool("matches('cat', {'dog', 'cat', 'bird'});"));
    EXPECT_FALSE(evalBool("matches('fish', {'dog', 'cat', 'bird'});"));
}

TEST_P(BuiltinTest, Splitlines)
{
    // sprintf interprets escape sequences; single-quoted strings don't.
    eval("c = splitlines(sprintf('a\\nb\\nc'));");
    auto *c = getVarPtr("c");
    ASSERT_EQ(c->numel(), 3u);
    EXPECT_EQ(c->cellAt(0).toString(), "a");
    EXPECT_EQ(c->cellAt(1).toString(), "b");
    EXPECT_EQ(c->cellAt(2).toString(), "c");
    // CRLF: build via char(13)+char(10) since the engine's sprintf
    // doesn't appear to expand \\r the same way it does \\n.
    eval("nl = [char(13) char(10)]; c2 = splitlines(['x' nl 'y' nl 'z']);");
    auto *c2 = getVarPtr("c2");
    ASSERT_EQ(c2->numel(), 3u);
    EXPECT_EQ(c2->cellAt(0).toString(), "x");
    EXPECT_EQ(c2->cellAt(2).toString(), "z");
}

TEST_P(BuiltinTest, PadStrip)
{
    eval("p1 = pad('abc', 6);");           // default right
    EXPECT_EQ(getVarPtr("p1")->toString(), "abc   ");
    eval("p2 = pad('abc', 6, 'left');");
    EXPECT_EQ(getVarPtr("p2")->toString(), "   abc");
    eval("p3 = pad('abc', 7, 'both', '*');");
    EXPECT_EQ(getVarPtr("p3")->toString(), "**abc**");
    // No pad needed.
    eval("p4 = pad('abcdef', 4);");
    EXPECT_EQ(getVarPtr("p4")->toString(), "abcdef");

    eval("s1 = strip('   hello   ');");
    EXPECT_EQ(getVarPtr("s1")->toString(), "hello");
    eval("s2 = strip('   hello   ', 'left');");
    EXPECT_EQ(getVarPtr("s2")->toString(), "hello   ");
    eval("s3 = strip('xxhelloxx', 'both', 'x');");
    EXPECT_EQ(getVarPtr("s3")->toString(), "hello");
}

// ── Bit ops — Pack 17 ─────────────────────────────────────────
TEST_P(BuiltinTest, BitSetBitGet)
{
    // bitset: bit 1 is LSB. bitset(0, 3) = 4 (set bit 3 of 0).
    EXPECT_DOUBLE_EQ(evalScalar("bitset(0, 3);"), 4.0);
    EXPECT_DOUBLE_EQ(evalScalar("bitset(0, 1);"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("bitset(7, 1, 0);"), 6.0);  // clear LSB of 7
    EXPECT_DOUBLE_EQ(evalScalar("bitset(7, 4, 1);"), 15.0); // set bit 4

    // bitget extracts the n-th bit.
    EXPECT_DOUBLE_EQ(evalScalar("bitget(5, 1);"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("bitget(5, 2);"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("bitget(5, 3);"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("bitget(8, 4);"), 1.0);

    // Vectorized.
    eval("v = bitget(15, [1 2 3 4 5]);");
    auto *v = getVarPtr("v");
    ASSERT_EQ(v->numel(), 5u);
    EXPECT_DOUBLE_EQ(v->doubleData()[0], 1.0);
    EXPECT_DOUBLE_EQ(v->doubleData()[3], 1.0);
    EXPECT_DOUBLE_EQ(v->doubleData()[4], 0.0);
}

// ── Set ops — Pack 16 ─────────────────────────────────────────
TEST_P(BuiltinTest, SetXor)
{
    eval("v = setxor([1 2 3 4], [3 4 5 6]);");
    auto *v = getVarPtr("v");
    ASSERT_EQ(v->numel(), 4u);
    EXPECT_DOUBLE_EQ(v->doubleData()[0], 1.0);
    EXPECT_DOUBLE_EQ(v->doubleData()[1], 2.0);
    EXPECT_DOUBLE_EQ(v->doubleData()[2], 5.0);
    EXPECT_DOUBLE_EQ(v->doubleData()[3], 6.0);
    // Disjoint sets → union.
    eval("u = setxor([1 2], [3 4]);");
    EXPECT_EQ(getVarPtr("u")->numel(), 4u);
    // Identical sets → empty.
    eval("e = setxor([1 2 3], [1 2 3]);");
    EXPECT_TRUE(getVarPtr("e")->isEmpty());
}

TEST_P(BuiltinTest, AllUniqueAndNumUnique)
{
    EXPECT_TRUE(evalBool("allunique([1 2 3 4]);"));
    EXPECT_FALSE(evalBool("allunique([1 2 2 3]);"));
    EXPECT_TRUE(evalBool("allunique([]);"));
    EXPECT_TRUE(evalBool("allunique(5);"));

    EXPECT_DOUBLE_EQ(evalScalar("numunique([1 2 3 2 1]);"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("numunique([]);"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("numunique([1 1 1]);"), 1.0);
}

TEST_P(BuiltinTest, IsmemberTolUniquetol)
{
    // Tolerant membership.
    eval("tf = ismembertol([1.0 2.000001 3.0], [1 2 4], 1e-4);");
    auto *tf = getVarPtr("tf");
    ASSERT_EQ(tf->numel(), 3u);
    EXPECT_EQ(tf->logicalData()[0], 1);   // 1.0 ∈ {1,2,4}
    EXPECT_EQ(tf->logicalData()[1], 1);   // 2.000001 ≈ 2
    EXPECT_EQ(tf->logicalData()[2], 0);   // 3.0 not present

    // Tolerant uniqueness.
    eval("u = uniquetol([1.0 1.0000001 2.0 2.0000001 3.0], 1e-4);");
    auto *u = getVarPtr("u");
    ASSERT_EQ(u->numel(), 3u);
    EXPECT_NEAR(u->doubleData()[0], 1.0, 1e-3);
    EXPECT_NEAR(u->doubleData()[1], 2.0, 1e-3);
    EXPECT_NEAR(u->doubleData()[2], 3.0, 1e-3);
}

// ── Cell idioms — Pack 15 ─────────────────────────────────────
TEST_P(BuiltinTest, Num2Cell)
{
    eval("c = num2cell([10 20 30]);");
    auto *c = getVarPtr("c");
    ASSERT_TRUE(c->isCell());
    EXPECT_EQ(c->numel(), 3u);
    EXPECT_DOUBLE_EQ(c->cellAt(0).toScalar(), 10.0);
    EXPECT_DOUBLE_EQ(c->cellAt(2).toScalar(), 30.0);
    // 2-D shape preserved.
    eval("c2 = num2cell([1 2; 3 4]);");
    auto *c2 = getVarPtr("c2");
    EXPECT_EQ(rows(*c2), 2u);
    EXPECT_EQ(cols(*c2), 2u);
}

TEST_P(BuiltinTest, Cell2MatScalars)
{
    eval("c = {1, 2, 3; 4, 5, 6};");
    eval("M = cell2mat(c);");
    auto *M = getVarPtr("M");
    EXPECT_EQ(rows(*M), 2u);
    EXPECT_EQ(cols(*M), 3u);
    EXPECT_DOUBLE_EQ((*M)(0, 0), 1.0);
    EXPECT_DOUBLE_EQ((*M)(1, 2), 6.0);
}

TEST_P(BuiltinTest, Cell2MatBlocks)
{
    // Row of two 2x2 blocks → 2x4.
    eval("A = [1 2; 3 4]; B = [5 6; 7 8];");
    eval("M = cell2mat({A, B});");
    auto *M = getVarPtr("M");
    EXPECT_EQ(rows(*M), 2u);
    EXPECT_EQ(cols(*M), 4u);
    EXPECT_DOUBLE_EQ((*M)(0, 0), 1.0);
    EXPECT_DOUBLE_EQ((*M)(0, 2), 5.0);
    EXPECT_DOUBLE_EQ((*M)(1, 3), 8.0);
}

TEST_P(BuiltinTest, IsCellStr)
{
    EXPECT_TRUE(evalBool("iscellstr({'foo', 'bar', 'baz'});"));
    EXPECT_FALSE(evalBool("iscellstr({'foo', 1, 'bar'});"));
    EXPECT_FALSE(evalBool("iscellstr({1, 2, 3});"));
    // Non-cell input → false.
    EXPECT_FALSE(evalBool("iscellstr('plain string');"));
}

TEST_P(BuiltinTest, Cellstr)
{
    eval("c = cellstr('hello');");
    auto *c = getVarPtr("c");
    ASSERT_TRUE(c->isCell());
    EXPECT_EQ(c->numel(), 1u);
    EXPECT_EQ(c->cellAt(0).toString(), "hello");
    // Cell pass-through.
    eval("c2 = cellstr({'a', 'b'});");
    auto *c2 = getVarPtr("c2");
    EXPECT_EQ(c2->numel(), 2u);
}

// ── Struct utils — Pack 14 ────────────────────────────────────
TEST_P(BuiltinTest, GetSetField)
{
    eval("s = struct('a', 1, 'b', 2);");
    EXPECT_DOUBLE_EQ(evalScalar("getfield(s, 'a');"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("getfield(s, 'b');"), 2.0);
    eval("s2 = setfield(s, 'a', 99);");
    EXPECT_DOUBLE_EQ(evalScalar("getfield(s2, 'a');"), 99.0);
    EXPECT_DOUBLE_EQ(evalScalar("getfield(s2, 'b');"), 2.0);
    // setfield can introduce a new field.
    eval("s3 = setfield(s, 'c', 3);");
    EXPECT_DOUBLE_EQ(evalScalar("getfield(s3, 'c');"), 3.0);
    // Non-existent field on getfield → error.
    EXPECT_THROW(eval("getfield(s, 'nope');"), std::exception);
}

TEST_P(BuiltinTest, OrderFields)
{
    eval("s = struct('zeta', 26, 'alpha', 1, 'mu', 13);");
    eval("s2 = orderfields(s);");
    eval("fn = fieldnames(s2);");
    auto *fn = getVarPtr("fn");
    ASSERT_EQ(fn->numel(), 3u);
    // Map-backed struct already iterates alphabetically.
    EXPECT_EQ(fn->cellAt(0).toString(), "alpha");
    EXPECT_EQ(fn->cellAt(1).toString(), "mu");
    EXPECT_EQ(fn->cellAt(2).toString(), "zeta");
}

TEST_P(BuiltinTest, Struct2CellAndBack)
{
    eval("s = struct('a', 10, 'b', 20, 'c', 30);");
    eval("c = struct2cell(s);");
    auto *c = getVarPtr("c");
    ASSERT_EQ(c->numel(), 3u);
    EXPECT_DOUBLE_EQ(c->cellAt(0).toScalar(), 10.0);
    EXPECT_DOUBLE_EQ(c->cellAt(1).toScalar(), 20.0);
    EXPECT_DOUBLE_EQ(c->cellAt(2).toScalar(), 30.0);

    // cell2struct round-trip.
    eval("fields = {'a'; 'b'; 'c'}; s2 = cell2struct(c, fields);");
    EXPECT_DOUBLE_EQ(evalScalar("s2.a;"), 10.0);
    EXPECT_DOUBLE_EQ(evalScalar("s2.b;"), 20.0);
    EXPECT_DOUBLE_EQ(evalScalar("s2.c;"), 30.0);
}

// ── Function handles — Pack 13 ────────────────────────────────
TEST_P(BuiltinTest, FevalByName)
{
    EXPECT_NEAR(evalScalar("feval('sin', 0);"), 0.0, 1e-12);
    EXPECT_NEAR(evalScalar("feval('cos', 0);"), 1.0, 1e-12);
    EXPECT_NEAR(evalScalar("feval('plus', 2, 3);"), 5.0, 1e-12);
}

TEST_P(BuiltinTest, FevalByHandle)
{
    eval("h = @sin; v = feval(h, pi/2);");
    EXPECT_NEAR(getVar("v"), 1.0, 1e-12);
    eval("h2 = str2func('cos'); w = feval(h2, 0);");
    EXPECT_NEAR(getVar("w"), 1.0, 1e-12);
}

TEST_P(BuiltinTest, Func2Str)
{
    // MATLAB returns the bare name for named handles (no `@` prefix).
    // Anonymous handles return their full source text with `@`.
    // See BUGS.md #16.
    eval("h = @sin; s = func2str(h);");
    EXPECT_EQ(getVarPtr("s")->toString(), "sin");
    eval("h2 = str2func('cos'); s2 = func2str(h2);");
    EXPECT_EQ(getVarPtr("s2")->toString(), "cos");
}

// ── shiftdim — Pack 12 ────────────────────────────────────────
TEST_P(BuiltinTest, ShiftDimPositive)
{
    // 2-D: shiftdim(A, 1) is a transpose-like rotation.
    eval("A = [1 2 3; 4 5 6]; B = shiftdim(A, 1);");
    auto *B = getVarPtr("B");
    ASSERT_NE(B, nullptr);
    EXPECT_EQ(rows(*B), 3u);
    EXPECT_EQ(cols(*B), 2u);
    EXPECT_DOUBLE_EQ((*B)(0, 0), 1.0);
    EXPECT_DOUBLE_EQ((*B)(2, 1), 6.0);

    // 3-D: shiftdim(ones(2,3,4), 1) should yield 3×4×2.
    eval("S = size(shiftdim(ones(2,3,4), 1));");
    auto *S = getVarPtr("S");
    EXPECT_DOUBLE_EQ(S->doubleData()[0], 3.0);
    EXPECT_DOUBLE_EQ(S->doubleData()[1], 4.0);
    EXPECT_DOUBLE_EQ(S->doubleData()[2], 2.0);
}

TEST_P(BuiltinTest, ShiftDimNegativePrepends)
{
    // shiftdim(ones(3,4), -1) prepends a singleton → 1×3×4.
    eval("S = size(shiftdim(ones(3,4), -1));");
    auto *S = getVarPtr("S");
    ASSERT_GE(S->numel(), 3u);
    EXPECT_DOUBLE_EQ(S->doubleData()[0], 1.0);
    EXPECT_DOUBLE_EQ(S->doubleData()[1], 3.0);
    EXPECT_DOUBLE_EQ(S->doubleData()[2], 4.0);
}

TEST_P(BuiltinTest, ShiftDimAuto)
{
    // [B, k] = shiftdim(A) drops leading singletons.
    eval("[B, k] = shiftdim(ones(1,1,3));");
    EXPECT_DOUBLE_EQ(getVar("k"), 2.0);
    auto *B = getVarPtr("B");
    EXPECT_EQ(B->numel(), 3u);
    // Row vector (1×3) has one leading singleton → k=1, B is 3×1 column.
    eval("[B2, k2] = shiftdim([1 2 3]);");
    EXPECT_DOUBLE_EQ(getVar("k2"), 1.0);
    auto *B2 = getVarPtr("B2");
    EXPECT_EQ(rows(*B2), 3u);
    EXPECT_EQ(cols(*B2), 1u);
    // Column vector (3×1): no leading singleton, k=0.
    eval("[B3, k3] = shiftdim([1; 2; 3]);");
    EXPECT_DOUBLE_EQ(getVar("k3"), 0.0);
}

// ── Pack 11: operator-named functions ─────────────────────────
TEST_P(BuiltinTest, OperatorNamedArith)
{
    EXPECT_DOUBLE_EQ(evalScalar("plus(2, 3);"),  5.0);
    EXPECT_DOUBLE_EQ(evalScalar("minus(7, 4);"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("times(3, 4);"), 12.0);
    EXPECT_DOUBLE_EQ(evalScalar("mtimes(3, 4);"), 12.0);
    EXPECT_DOUBLE_EQ(evalScalar("rdivide(10, 4);"), 2.5);
    EXPECT_DOUBLE_EQ(evalScalar("ldivide(4, 10);"), 2.5);
    EXPECT_DOUBLE_EQ(evalScalar("mrdivide(10, 4);"), 2.5);
    EXPECT_DOUBLE_EQ(evalScalar("power(2, 10);"),  1024.0);
    EXPECT_DOUBLE_EQ(evalScalar("mpower(2, 10);"), 1024.0);
    EXPECT_DOUBLE_EQ(evalScalar("uminus(5);"), -5.0);
    EXPECT_DOUBLE_EQ(evalScalar("uplus(-3);"), -3.0);
    eval("v = plus([1 2 3], [10 20 30]);");
    auto *v = getVarPtr("v");
    ASSERT_EQ(v->numel(), 3u);
    EXPECT_DOUBLE_EQ(v->doubleData()[0], 11.0);
    EXPECT_DOUBLE_EQ(v->doubleData()[2], 33.0);
}

TEST_P(BuiltinTest, OperatorNamedCompare)
{
    EXPECT_TRUE(evalBool("eq(3, 3);"));
    EXPECT_FALSE(evalBool("eq(3, 4);"));
    EXPECT_TRUE(evalBool("ne(3, 4);"));
    EXPECT_TRUE(evalBool("lt(2, 3);"));
    EXPECT_TRUE(evalBool("le(3, 3);"));
    EXPECT_TRUE(evalBool("gt(4, 3);"));
    EXPECT_TRUE(evalBool("ge(3, 3);"));
}

TEST_P(BuiltinTest, OperatorNamedLogical)
{
    EXPECT_TRUE(evalBool("and(true, true);"));
    EXPECT_FALSE(evalBool("and(true, false);"));
    EXPECT_TRUE(evalBool("or(false, true);"));
    EXPECT_FALSE(evalBool("or(false, false);"));
    EXPECT_TRUE(evalBool("not(false);"));
    EXPECT_FALSE(evalBool("not(true);"));
}

TEST_P(BuiltinTest, OperatorNamedTranspose)
{
    eval("A = [1 2 3; 4 5 6]; B = ctranspose(A);");
    auto *B = getVarPtr("B");
    ASSERT_NE(B, nullptr);
    EXPECT_EQ(rows(*B), 3u);
    EXPECT_EQ(cols(*B), 2u);
    EXPECT_DOUBLE_EQ((*B)(0, 0), 1.0);
    EXPECT_DOUBLE_EQ((*B)(2, 1), 6.0);
}

// ── String utils — Pack 10 ────────────────────────────────────
TEST_P(BuiltinTest, Strncmp)
{
    EXPECT_TRUE(evalBool("strncmp('hello world', 'hello there', 5);"));
    // First 6 chars match ("hello "), so 6 is still true; first 7 differ.
    EXPECT_TRUE(evalBool("strncmp('hello world', 'hello there', 6);"));
    EXPECT_FALSE(evalBool("strncmp('hello world', 'hello there', 7);"));
    EXPECT_TRUE(evalBool("strncmpi('HELLO', 'hello', 5);"));
    EXPECT_FALSE(evalBool("strncmp('HELLO', 'hello', 5);"));
}

TEST_P(BuiltinTest, Strfind)
{
    eval("p = strfind('abcabc', 'b');");
    auto *p = getVarPtr("p");
    ASSERT_EQ(p->numel(), 2u);
    EXPECT_DOUBLE_EQ(p->doubleData()[0], 2.0);
    EXPECT_DOUBLE_EQ(p->doubleData()[1], 5.0);
    eval("e = strfind('hello', 'xyz');");
    auto *e = getVarPtr("e");
    EXPECT_TRUE(e->isEmpty());
    eval("a = strfind('aaaa', 'aa');");
    auto *a = getVarPtr("a");
    // Overlapping matches: positions 1, 2, 3.
    ASSERT_EQ(a->numel(), 3u);
    EXPECT_DOUBLE_EQ(a->doubleData()[0], 1.0);
    EXPECT_DOUBLE_EQ(a->doubleData()[2], 3.0);
}

TEST_P(BuiltinTest, BlanksAndDeblank)
{
    eval("b = blanks(5);");
    auto *b = getVarPtr("b");
    EXPECT_EQ(b->toString(), "     ");
    eval("d = deblank('hello   ');");
    auto *d = getVarPtr("d");
    EXPECT_EQ(d->toString(), "hello");
    // deblank preserves leading whitespace.
    eval("d2 = deblank('   hello   ');");
    auto *d2 = getVarPtr("d2");
    EXPECT_EQ(d2->toString(), "   hello");
}

TEST_P(BuiltinTest, Mat2str)
{
    eval("s1 = mat2str(5);");
    EXPECT_EQ(getVarPtr("s1")->toString(), "5");
    eval("s2 = mat2str([1 2 3]);");
    EXPECT_EQ(getVarPtr("s2")->toString(), "[1 2 3]");
    eval("s3 = mat2str([1 2; 3 4]);");
    EXPECT_EQ(getVarPtr("s3")->toString(), "[1 2;3 4]");
    eval("s4 = mat2str([]);");
    EXPECT_EQ(getVarPtr("s4")->toString(), "[]");
}

TEST_P(BuiltinTest, Mat2strComplex)
{
    // Complex values: re±|im|i per element (vs MATLAB R2025b).
    eval("c1 = mat2str(1+2i);");
    EXPECT_EQ(getVarPtr("c1")->toString(), "1+2i");
    eval("c2 = mat2str([1+2i 3-4i]);");
    EXPECT_EQ(getVarPtr("c2")->toString(), "[1+2i 3-4i]");
    eval("c3 = mat2str([1+2i; 3-4i]);");
    EXPECT_EQ(getVarPtr("c3")->toString(), "[1+2i;3-4i]");
    // A purely-imaginary element keeps a "0" real part (no "-0").
    eval("c4 = mat2str([1.5+2.25i -3i], 5);");
    EXPECT_EQ(getVarPtr("c4")->toString(), "[1.5+2.25i 0-3i]");
    // All-zero imaginary parts → printed as real.
    eval("c5 = mat2str(complex(1, 0));");
    EXPECT_EQ(getVarPtr("c5")->toString(), "1");
}

// mat2str on integer + logical types and the 'class' option. vs MATLAB
// R2025b: integers print BARE (no class wrapper), logical prints true/false,
// and 'class' wraps the result with the class name. 2026-05-30.
TEST_P(BuiltinTest, Mat2strIntegerLogical)
{
    eval("i1 = mat2str(int8([1 2; 3 4]));");
    EXPECT_EQ(getVarPtr("i1")->toString(), "[1 2;3 4]");
    eval("i2 = mat2str(int8(5));");
    EXPECT_EQ(getVarPtr("i2")->toString(), "5");
    eval("i3 = mat2str(uint8([255 0; 1 2]));");
    EXPECT_EQ(getVarPtr("i3")->toString(), "[255 0;1 2]");
    eval("i4 = mat2str(int32([-5 7]));");
    EXPECT_EQ(getVarPtr("i4")->toString(), "[-5 7]");
    // Logical -> true / false.
    eval("l1 = mat2str(true);");
    EXPECT_EQ(getVarPtr("l1")->toString(), "true");
    eval("l2 = mat2str([true false true]);");
    EXPECT_EQ(getVarPtr("l2")->toString(), "[true false true]");
    eval("l3 = mat2str(logical([1 0; 0 1]));");
    EXPECT_EQ(getVarPtr("l3")->toString(), "[true false;false true]");
    // 'class' option wraps with the class name.
    eval("k1 = mat2str(int8([1 2; 3 4]), 'class');");
    EXPECT_EQ(getVarPtr("k1")->toString(), "int8([1 2;3 4])");
    eval("k2 = mat2str(true, 'class');");
    EXPECT_EQ(getVarPtr("k2")->toString(), "logical(true)");
    eval("k3 = mat2str([1 2], 'class');");
    EXPECT_EQ(getVarPtr("k3")->toString(), "double([1 2])");
}

TEST_P(BuiltinTest, Strjoin)
{
    eval("s = strjoin({'a', 'b', 'c'});");
    EXPECT_EQ(getVarPtr("s")->toString(), "a b c");
    eval("s2 = strjoin({'foo', 'bar', 'baz'}, '-');");
    EXPECT_EQ(getVarPtr("s2")->toString(), "foo-bar-baz");
    // Cell array of N-1 delimiters, interleaved between elements (MATLAB R2025b).
    eval("s3 = strjoin({'a', 'b', 'c'}, {', ', ' and '});");
    EXPECT_EQ(getVarPtr("s3")->toString(), "a, b and c");
    eval("s4 = strjoin({'x', 'y'}, {'->'});");
    EXPECT_EQ(getVarPtr("s4")->toString(), "x->y");
    eval("s5 = strjoin({'solo'}, {});");   // single element, no delimiters
    EXPECT_EQ(getVarPtr("s5")->toString(), "solo");
}

TEST_P(BuiltinTest, Strtok)
{
    eval("[t, r] = strtok('hello world foo');");
    EXPECT_EQ(getVarPtr("t")->toString(), "hello");
    EXPECT_EQ(getVarPtr("r")->toString(), " world foo");
    // With explicit delim.
    eval("[t2, r2] = strtok('a,b,c', ',');");
    EXPECT_EQ(getVarPtr("t2")->toString(), "a");
    EXPECT_EQ(getVarPtr("r2")->toString(), ",b,c");
    // Leading delim is skipped.
    eval("[t3, r3] = strtok('   foo bar');");
    EXPECT_EQ(getVarPtr("t3")->toString(), "foo");
}

// ── pow2 / realpow / reallog / realsqrt ───────────────────────
TEST_P(BuiltinTest, Pow2Family)
{
    // 1-arg form: pow2(y) = 2^y.
    EXPECT_DOUBLE_EQ(evalScalar("pow2(0);"),  1.0);
    EXPECT_DOUBLE_EQ(evalScalar("pow2(10);"), 1024.0);
    EXPECT_DOUBLE_EQ(evalScalar("pow2(-3);"), 0.125);

    // 2-arg form: pow2(F, E) = F * 2^floor(E).
    EXPECT_DOUBLE_EQ(evalScalar("pow2(1.5, 2);"), 6.0);   // 1.5 * 4
    EXPECT_DOUBLE_EQ(evalScalar("pow2(3, 0);"),   3.0);
    EXPECT_DOUBLE_EQ(evalScalar("pow2(1, 10);"),  1024.0);
}

TEST_P(BuiltinTest, RealPowRealLogRealSqrt)
{
    EXPECT_DOUBLE_EQ(evalScalar("realpow(2, 10);"), 1024.0);
    EXPECT_DOUBLE_EQ(evalScalar("realpow(-2, 3);"),   -8.0);   // negative^integer is real.
    // Negative base + non-integer exponent → error.
    EXPECT_THROW(eval("realpow(-2, 0.5);"), std::exception);

    EXPECT_NEAR(evalScalar("reallog(exp(1));"), 1.0, 1e-12);
    EXPECT_DOUBLE_EQ(evalScalar("reallog(1);"), 0.0);
    EXPECT_THROW(eval("reallog(-1);"), std::exception);

    EXPECT_DOUBLE_EQ(evalScalar("realsqrt(4);"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("realsqrt(0);"), 0.0);
    EXPECT_THROW(eval("realsqrt(-1);"), std::exception);
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

// ── flip / repelem / sub2ind / ind2sub ────────────────────────
TEST_P(BuiltinTest, Flip)
{
    eval("v = flip([1 2 3 4]);");
    auto *v = getVarPtr("v");
    ASSERT_NE(v, nullptr);
    EXPECT_EQ(v->numel(), 4u);
    EXPECT_DOUBLE_EQ(v->doubleData()[0], 4.0);
    EXPECT_DOUBLE_EQ(v->doubleData()[3], 1.0);

    // Column vector flips along its single non-singleton dim.
    eval("u = flip([10; 20; 30]);");
    auto *u = getVarPtr("u");
    EXPECT_DOUBLE_EQ(u->doubleData()[0], 30.0);
    EXPECT_DOUBLE_EQ(u->doubleData()[2], 10.0);

    // flip(A, 1) = flipud, flip(A, 2) = fliplr.
    eval("A = [1 2 3; 4 5 6]; B1 = flip(A, 1); B2 = flip(A, 2);");
    auto *B1 = getVarPtr("B1");
    auto *B2 = getVarPtr("B2");
    EXPECT_DOUBLE_EQ((*B1)(0, 0), 4.0);
    EXPECT_DOUBLE_EQ((*B1)(1, 0), 1.0);
    EXPECT_DOUBLE_EQ((*B2)(0, 0), 3.0);
    EXPECT_DOUBLE_EQ((*B2)(0, 2), 1.0);
}

TEST_P(BuiltinTest, RepelemVector)
{
    eval("v = repelem([1 2 3], 2);");
    auto *v = getVarPtr("v");
    ASSERT_NE(v, nullptr);
    EXPECT_EQ(v->numel(), 6u);
    EXPECT_DOUBLE_EQ(v->doubleData()[0], 1.0);
    EXPECT_DOUBLE_EQ(v->doubleData()[1], 1.0);
    EXPECT_DOUBLE_EQ(v->doubleData()[2], 2.0);
    EXPECT_DOUBLE_EQ(v->doubleData()[3], 2.0);
    EXPECT_DOUBLE_EQ(v->doubleData()[4], 3.0);
    EXPECT_DOUBLE_EQ(v->doubleData()[5], 3.0);
}

TEST_P(BuiltinTest, RepelemMatrixBlocks)
{
    eval("A = [1 2; 3 4]; B = repelem(A, 2, 3);");
    auto *B = getVarPtr("B");
    ASSERT_NE(B, nullptr);
    EXPECT_EQ(rows(*B), 4u);
    EXPECT_EQ(cols(*B), 6u);
    // First block is 2x3 of 1's.
    EXPECT_DOUBLE_EQ((*B)(0, 0), 1.0);
    EXPECT_DOUBLE_EQ((*B)(1, 2), 1.0);
    // Block to the right is 2x3 of 2's.
    EXPECT_DOUBLE_EQ((*B)(0, 3), 2.0);
    EXPECT_DOUBLE_EQ((*B)(1, 5), 2.0);
    // Block below is 2x3 of 3's.
    EXPECT_DOUBLE_EQ((*B)(2, 0), 3.0);
    EXPECT_DOUBLE_EQ((*B)(3, 2), 3.0);
    // Bottom-right block is 2x3 of 4's.
    EXPECT_DOUBLE_EQ((*B)(2, 3), 4.0);
    EXPECT_DOUBLE_EQ((*B)(3, 5), 4.0);
}

TEST_P(BuiltinTest, RepelemCountVector)
{
    // Per-element counts: 1->1x, 2->2x, 3->3x → [1 2 2 3 3 3].
    eval("v = repelem([1 2 3], [1 2 3]);");
    auto *v = getVarPtr("v");
    ASSERT_NE(v, nullptr);
    EXPECT_EQ(v->numel(), 6u);
    EXPECT_DOUBLE_EQ(v->doubleData()[0], 1.0);
    EXPECT_DOUBLE_EQ(v->doubleData()[1], 2.0);
    EXPECT_DOUBLE_EQ(v->doubleData()[2], 2.0);
    EXPECT_DOUBLE_EQ(v->doubleData()[3], 3.0);
    EXPECT_DOUBLE_EQ(v->doubleData()[4], 3.0);
    EXPECT_DOUBLE_EQ(v->doubleData()[5], 3.0);
}

TEST_P(BuiltinTest, RepelemCountVectorZeroDropsElement)
{
    // A zero count drops that element; column orientation preserved.
    eval("c = repelem([1; 2; 3], [2 0 1]);");
    auto *c = getVarPtr("c");
    ASSERT_NE(c, nullptr);
    EXPECT_EQ(rows(*c), 3u);
    EXPECT_EQ(cols(*c), 1u);
    EXPECT_DOUBLE_EQ(c->doubleData()[0], 1.0);
    EXPECT_DOUBLE_EQ(c->doubleData()[1], 1.0);
    EXPECT_DOUBLE_EQ(c->doubleData()[2], 3.0);
}

TEST_P(BuiltinTest, RepelemMatrixScalarRowVectorCol)
{
    // r scalar (2), c vector [1 2] → 4x3.
    eval("A = [1 2; 3 4]; B = repelem(A, 2, [1 2]);");
    auto *B = getVarPtr("B");
    ASSERT_NE(B, nullptr);
    EXPECT_EQ(rows(*B), 4u);
    EXPECT_EQ(cols(*B), 3u);
    EXPECT_DOUBLE_EQ((*B)(0, 0), 1.0);  // col 0 once
    EXPECT_DOUBLE_EQ((*B)(0, 1), 2.0);  // col 1 twice
    EXPECT_DOUBLE_EQ((*B)(0, 2), 2.0);
    EXPECT_DOUBLE_EQ((*B)(3, 2), 4.0);
}

TEST_P(BuiltinTest, RepelemMatrixRowVectorScalarCol)
{
    // r vector [2 1], c scalar 3 → 3x6.
    eval("A = [1 2; 3 4]; B = repelem(A, [2 1], 3);");
    auto *B = getVarPtr("B");
    ASSERT_NE(B, nullptr);
    EXPECT_EQ(rows(*B), 3u);
    EXPECT_EQ(cols(*B), 6u);
    EXPECT_DOUBLE_EQ((*B)(0, 0), 1.0);  // row 0 twice
    EXPECT_DOUBLE_EQ((*B)(1, 0), 1.0);
    EXPECT_DOUBLE_EQ((*B)(2, 0), 3.0);  // row 1 once
    EXPECT_DOUBLE_EQ((*B)(2, 5), 4.0);
}

TEST_P(BuiltinTest, RepelemNegativeCountThrows)
{
    EXPECT_THROW(eval("y = repelem([1 2 3], [1 -1 2]);"), std::exception);
}

TEST_P(BuiltinTest, RepelemCountLengthMismatchThrows)
{
    EXPECT_THROW(eval("y = repelem([1 2 3], [1 2]);"), std::exception);
}

TEST_P(BuiltinTest, Sub2IndIndSub)
{
    // 2-D: column-major. siz=[3 4], (2,3) → 8.
    EXPECT_DOUBLE_EQ(evalScalar("sub2ind([3 4], 2, 3);"), 8.0);
    EXPECT_DOUBLE_EQ(evalScalar("sub2ind([3 4], 1, 1);"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("sub2ind([3 4], 3, 4);"), 12.0);

    // ind2sub round-trip.
    eval("[r, c] = ind2sub([3 4], 8);");
    EXPECT_DOUBLE_EQ(getVar("r"), 2.0);
    EXPECT_DOUBLE_EQ(getVar("c"), 3.0);

    // Vector of indices.
    eval("[r2, c2] = ind2sub([3 4], [1 8 12]);");
    auto *r2 = getVarPtr("r2");
    auto *c2 = getVarPtr("c2");
    EXPECT_DOUBLE_EQ(r2->doubleData()[0], 1.0);
    EXPECT_DOUBLE_EQ(c2->doubleData()[0], 1.0);
    EXPECT_DOUBLE_EQ(r2->doubleData()[1], 2.0);
    EXPECT_DOUBLE_EQ(c2->doubleData()[1], 3.0);
    EXPECT_DOUBLE_EQ(r2->doubleData()[2], 3.0);
    EXPECT_DOUBLE_EQ(c2->doubleData()[2], 4.0);

    // 3-D: siz=[2 3 4], (2, 1, 3) → ind = (3-1)*6 + (1-1)*2 + 2 = 14.
    EXPECT_DOUBLE_EQ(evalScalar("sub2ind([2 3 4], 2, 1, 3);"), 14.0);
    eval("[a, b, c] = ind2sub([2 3 4], 14);");
    EXPECT_DOUBLE_EQ(getVar("a"), 2.0);
    EXPECT_DOUBLE_EQ(getVar("b"), 1.0);
    EXPECT_DOUBLE_EQ(getVar("c"), 3.0);
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

// round(x, N) decimals + round(x, N, 'significant'). vs MATLAB R2025b.
TEST_P(BuiltinTest, RoundDigits)
{
    EXPECT_NEAR(evalScalar("round(3.14159, 2);"), 3.14, 1e-12);
    EXPECT_NEAR(evalScalar("round(3.14159, 4);"), 3.1416, 1e-12);
    EXPECT_DOUBLE_EQ(evalScalar("round(12345, -2);"), 12300.0);
    EXPECT_NEAR(evalScalar("round(3.14159, 3, 'significant');"), 3.14, 1e-12);
    EXPECT_DOUBLE_EQ(evalScalar("round(12345, 2, 'significant');"), 12000.0);
    EXPECT_NEAR(evalScalar("round(0.0012345, 2, 'significant');"), 0.0012, 1e-12);
    // round(x) (0-arg) unchanged; half-away-from-zero.
    EXPECT_DOUBLE_EQ(evalScalar("round(2.5);"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("round(-2.5);"), -3.0);
    // vector + bad type.
    eval("v = round([3.14159 2.71828], 2);");
    EXPECT_NEAR(evalScalar("v(2)"), 2.72, 1e-12);
    EXPECT_THROW(eval("round(1.5, 2, 'bogus');"), std::exception);
}

TEST_P(BuiltinTest, Mod)
{
    EXPECT_DOUBLE_EQ(evalScalar("mod(7, 3);"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("mod(10, 5);"), 0.0);
}

// mod/rem on integer types keep the integer class (a double operand is
// promoted to the integer class). numkit previously returned double / threw
// on integer arrays. DEEP-PROBE 2026-05-30.
TEST_P(BuiltinTest, ModRemIntegerClass)
{
    eval("a = mod(int8(7), int8(3));");          // 1 int8
    EXPECT_DOUBLE_EQ(evalScalar("double(a);"), 1.0);
    EXPECT_TRUE(evalBool("isequal(class(a), 'int8');"));
    eval("b = rem(int8(-7), int8(3));");         // -1 int8
    EXPECT_DOUBLE_EQ(evalScalar("double(b);"), -1.0);
    EXPECT_TRUE(evalBool("isequal(class(b), 'int8');"));
    eval("c = mod(int8(-7), int8(3));");         // 2 int8 (floored)
    EXPECT_DOUBLE_EQ(evalScalar("double(c);"), 2.0);
    // Integer VECTOR (previously threw "Not a double array").
    eval("d = mod(int8([7 8 9]), int8(3));");    // [1 2 0] int8
    EXPECT_TRUE(evalBool("isequal(class(d), 'int8');"));
    EXPECT_DOUBLE_EQ(evalScalar("double(d(1));"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(d(3));"), 0.0);
    // Mixed int + double -> integer class.
    eval("e = mod(int8(7), 3);");
    EXPECT_TRUE(evalBool("isequal(class(e), 'int8');"));
    EXPECT_DOUBLE_EQ(evalScalar("double(e);"), 1.0);
    eval("f = rem(uint8(200), uint8(7));");      // 4 uint8
    EXPECT_DOUBLE_EQ(evalScalar("double(f);"), 4.0);
    EXPECT_TRUE(evalBool("isequal(class(f), 'uint8');"));
    // double inputs unchanged (regress).
    EXPECT_DOUBLE_EQ(evalScalar("mod(5.5, 2);"), 1.5);
    EXPECT_DOUBLE_EQ(evalScalar("rem(-7, 3);"), -1.0);
    EXPECT_TRUE(evalBool("isequal(class(mod(7, 3)), 'double');"));
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

// ============================================================
// Prod-grade test pack T1 — ND + non-DOUBLE coverage
// ============================================================
//
// The original parity packs added scalar / 2-D / DOUBLE tests for each
// builtin, which is enough to catch a flat-out wrong impl but misses
// rank-3+ shape preservation and type preservation through byte-copy
// kernels. T1 fills the obvious gaps.

TEST_P(BuiltinTest, FlipPreservesType)
{
    // flip uses byte-copy via flipNDAlongAxis — verify it actually
    // preserves the input type rather than coercing to DOUBLE.
    eval("v = int32([1 2 3 4]); r = flip(v);");
    auto *r = getVarPtr("r");
    EXPECT_EQ(r->type(), ValueType::INT32);
    EXPECT_EQ(r->numel(), 4u);

    eval("L = logical([1 0 1 0]); rL = flip(L);");
    auto *rL = getVarPtr("rL");
    EXPECT_EQ(rL->type(), ValueType::LOGICAL);
    EXPECT_EQ(rL->logicalData()[0], 0);
    EXPECT_EQ(rL->logicalData()[3], 1);
}

TEST_P(BuiltinTest, Flip3DAlongDim)
{
    // 3-D flip along each dim. zeros(2,3,4) is too sparse — use reshape
    // to get unique values per cell.
    eval("A = reshape(1:24, 2, 3, 4);");
    eval("F1 = flip(A, 1); F2 = flip(A, 2); F3 = flip(A, 3);");
    // F1 swaps rows within every 2-row column — A(1,1,1)=1 → F1(2,1,1)=1.
    auto *F1 = getVarPtr("F1");
    EXPECT_DOUBLE_EQ(F1->doubleData()[1], 1.0);   // [r=1][c=0][p=0]
    EXPECT_DOUBLE_EQ(F1->doubleData()[0], 2.0);   // [r=0][c=0][p=0]
    // F3 reverses page order — A(1,1,1)=1 → F3(1,1,4)=1; F3(1,1,1)=A(1,1,4)=19.
    auto *F3 = getVarPtr("F3");
    EXPECT_DOUBLE_EQ(F3->doubleData()[0], 19.0);
}

TEST_P(BuiltinTest, RepelemColumnVector)
{
    // repelem on a column should keep column orientation.
    eval("c = repelem([1; 2; 3], 2);");
    auto *c = getVarPtr("c");
    EXPECT_EQ(rows(*c), 6u);
    EXPECT_EQ(cols(*c), 1u);
    EXPECT_DOUBLE_EQ(c->doubleData()[0], 1.0);
    EXPECT_DOUBLE_EQ(c->doubleData()[5], 3.0);
}

TEST_P(BuiltinTest, Num2Cell3D)
{
    // num2cell of a 3-D array: cell shape mirrors input.
    eval("A = reshape(1:8, 2, 2, 2); C = num2cell(A);");
    auto *C = getVarPtr("C");
    ASSERT_TRUE(C->isCell());
    EXPECT_EQ(C->dims().rows(),  2u);
    EXPECT_EQ(C->dims().cols(),  2u);
    EXPECT_EQ(C->dims().pages(), 2u);
    EXPECT_EQ(C->numel(), 8u);
    // Index 0 is column-major: A(1,1,1) = 1.
    EXPECT_DOUBLE_EQ(C->cellAt(0).toScalar(), 1.0);
    EXPECT_DOUBLE_EQ(C->cellAt(7).toScalar(), 8.0);
}

TEST_P(BuiltinTest, HeadTailPreservesType)
{
    // head/tail operate on rows — use a column vector so n maps to row
    // count and the byte-copy path actually picks up a non-DOUBLE type.
    eval("v = uint8([10 20 30 40 50]'); h = head(v, 3); t = tail(v, 2);");
    auto *h = getVarPtr("h");
    auto *t = getVarPtr("t");
    EXPECT_EQ(h->type(), ValueType::UINT8);
    EXPECT_EQ(t->type(), ValueType::UINT8);
    EXPECT_EQ(rows(*h), 3u);
    EXPECT_EQ(rows(*t), 2u);
}

TEST_P(BuiltinTest, PaddataColumnOrientation)
{
    // paddata on a column should still produce a column.
    eval("c = paddata([1; 2; 3], 6);");
    auto *c = getVarPtr("c");
    EXPECT_EQ(rows(*c), 6u);
    EXPECT_EQ(cols(*c), 1u);
    EXPECT_DOUBLE_EQ(c->doubleData()[5], 0.0);
    // Trim also preserves orientation.
    eval("t = trimdata([1; 2; 3; 4; 5], 2);");
    auto *t = getVarPtr("t");
    EXPECT_EQ(rows(*t), 2u);
    EXPECT_EQ(cols(*t), 1u);
}

// ============================================================
// Prod-grade test pack T2 — roundtrip / property tests
// ============================================================
//
// These check identities the impl must satisfy by construction. They
// catch bugs that pointwise tests miss (e.g. an off-by-one in
// poly/roots that still passes "this case looks right").

TEST_P(BuiltinTest, PolyRootsRoundtrip)
{
    // r = roots(p), then p' = poly(r) should reproduce p up to a
    // leading-coefficient scale (poly always normalises to leading 1).
    eval("p = [1 -6 11 -6];");          // (x-1)(x-2)(x-3)
    eval("r = roots(p); p2 = poly(r);");
    auto *p2 = getVarPtr("p2");
    ASSERT_EQ(p2->numel(), 4u);
    // Reconstructed coefficients should be {1, -6, 11, -6}.
    EXPECT_NEAR(p2->doubleData()[0],  1.0,  1e-10);
    EXPECT_NEAR(p2->doubleData()[1], -6.0,  1e-10);
    EXPECT_NEAR(p2->doubleData()[2], 11.0,  1e-10);
    EXPECT_NEAR(p2->doubleData()[3], -6.0,  1e-10);
}

TEST_P(BuiltinTest, IdivideRemRelation)
{
    // For 'fix' mode: a == idivide(a, b, 'fix') * b + rem(a, b).
    // idivide takes int; rem currently requires double, so do the
    // remainder side in double space and reconstitute. See BUGS.md #29.
    eval("ai = int32([10 -10 7 -7]); bi = int32([3 3 -3 -3]);");
    eval("ad = double(ai); bd = double(bi);");
    eval("q = double(idivide(ai, bi, 'fix')); r = rem(ad, bd); recon = q .* bd + r;");
    auto *recon = getVarPtr("recon");
    ASSERT_EQ(recon->numel(), 4u);
    EXPECT_DOUBLE_EQ(recon->doubleData()[0],  10.0);
    EXPECT_DOUBLE_EQ(recon->doubleData()[1], -10.0);
    EXPECT_DOUBLE_EQ(recon->doubleData()[2],   7.0);
    EXPECT_DOUBLE_EQ(recon->doubleData()[3],  -7.0);
}

TEST_P(BuiltinTest, BetaIncSymmetry)
{
    // I_x(a, b) + I_(1-x)(b, a) == 1 for all a, b > 0, x ∈ (0, 1).
    EXPECT_NEAR(evalScalar("betainc(0.3, 2, 5) + betainc(0.7, 5, 2);"),
                1.0, 1e-10);
    EXPECT_NEAR(evalScalar("betainc(0.7, 1, 1) + betainc(0.3, 1, 1);"),
                1.0, 1e-10);
    EXPECT_NEAR(evalScalar("betainc(0.1, 4, 7) + betainc(0.9, 7, 4);"),
                1.0, 1e-10);
}

TEST_P(BuiltinTest, GammaIncComplementSum)
{
    // P(a, x) + Q(a, x) = 1, where Q is upper incomplete. We only
    // expose the lower form, but the relation P(a, ∞) → 1 is checkable.
    EXPECT_NEAR(evalScalar("gammainc(100, 3);"), 1.0, 1e-10);
    EXPECT_NEAR(evalScalar("gammainc(1e-10, 3);"), 0.0, 1e-15);
}

TEST_P(BuiltinTest, MkppUnmkppRoundtrip)
{
    eval("br = [0 1 2 3]; cf = [1 0; 2 1; 3 0];");
    eval("pp = mkpp(br, cf); [b, c, l, k, d] = unmkpp(pp);");
    auto *b = getVarPtr("b");
    auto *c = getVarPtr("c");
    ASSERT_EQ(b->numel(), 4u);
    EXPECT_DOUBLE_EQ(b->doubleData()[0], 0.0);
    EXPECT_DOUBLE_EQ(b->doubleData()[3], 3.0);
    ASSERT_EQ(rows(*c), 3u);
    ASSERT_EQ(cols(*c), 2u);
    EXPECT_DOUBLE_EQ((*c)(0, 0), 1.0);
    EXPECT_DOUBLE_EQ((*c)(2, 1), 0.0);
    EXPECT_DOUBLE_EQ(getVar("l"), 3.0);
    EXPECT_DOUBLE_EQ(getVar("k"), 2.0);
}

TEST_P(BuiltinTest, CartPolarRoundtripVector)
{
    // 2-D roundtrip on a vector of points.
    eval("xs = [3 -1 0 5]; ys = [4 0 7 -12];");
    eval("[t, r] = cart2pol(xs, ys); [xb, yb] = pol2cart(t, r);");
    auto *xb = getVarPtr("xb");
    auto *yb = getVarPtr("yb");
    ASSERT_EQ(xb->numel(), 4u);
    for (size_t i = 0; i < 4u; ++i) {
        EXPECT_NEAR(xb->doubleData()[i],
                    (i == 0 ? 3.0 : i == 1 ? -1.0 : i == 2 ? 0.0 : 5.0),
                    1e-12);
        EXPECT_NEAR(yb->doubleData()[i],
                    (i == 0 ? 4.0 : i == 1 ? 0.0 : i == 2 ? 7.0 : -12.0),
                    1e-12);
    }
}

TEST_P(BuiltinTest, ErfErfinvRoundtrip)
{
    // erf(erfinv(y)) == y for y ∈ (-1, 1).
    EXPECT_NEAR(evalScalar("erf(erfinv(0.3));"), 0.3, 1e-10);
    EXPECT_NEAR(evalScalar("erf(erfinv(-0.7));"), -0.7, 1e-10);
    EXPECT_NEAR(evalScalar("erf(erfinv(0.99));"), 0.99, 1e-10);
}

TEST_P(BuiltinTest, Sub2IndInd2SubRoundtrip)
{
    // For each linear index, ind2sub then sub2ind returns the original.
    eval("siz = [3 4 2]; ind = [1 7 12 24];");
    eval("[r, c, p] = ind2sub(siz, ind); back = sub2ind(siz, r, c, p);");
    auto *back = getVarPtr("back");
    ASSERT_EQ(back->numel(), 4u);
    EXPECT_DOUBLE_EQ(back->doubleData()[0],  1.0);
    EXPECT_DOUBLE_EQ(back->doubleData()[1],  7.0);
    EXPECT_DOUBLE_EQ(back->doubleData()[2], 12.0);
    EXPECT_DOUBLE_EQ(back->doubleData()[3], 24.0);
}

// ============================================================
// Prod-grade test pack T3 — isstrprop full category coverage
// ============================================================
//
// Sample 'Aa 1!\t' selected so each character lands in a different
// subset:
//   index 0: 'A'  — alpha, upper, alphanum, graphic, print
//   index 1: 'a'  — alpha, lower, alphanum, graphic, print, xdigit
//   index 2: ' '  — space, wspace, print
//   index 3: '1'  — digit, alphanum, xdigit, graphic, print
//   index 4: '!'  — punct, graphic, print
//   index 5: '\t' — space, wspace, cntrl

TEST_P(BuiltinTest, IsStrPropAllCategories)
{
    eval("s = ['A' 'a' ' ' '1' '!' char(9)];");

    eval("a = isstrprop(s, 'alpha');");
    auto *a = getVarPtr("a");
    ASSERT_EQ(a->numel(), 6u);
    EXPECT_EQ(a->logicalData()[0], 1);  // A
    EXPECT_EQ(a->logicalData()[1], 1);  // a
    EXPECT_EQ(a->logicalData()[2], 0);  // space
    EXPECT_EQ(a->logicalData()[3], 0);  // 1
    EXPECT_EQ(a->logicalData()[4], 0);  // !
    EXPECT_EQ(a->logicalData()[5], 0);  // \t

    eval("d = isstrprop(s, 'digit');");
    auto *d = getVarPtr("d");
    EXPECT_EQ(d->logicalData()[3], 1);  // 1
    EXPECT_EQ(d->logicalData()[0], 0);

    eval("an = isstrprop(s, 'alphanum');");
    auto *an = getVarPtr("an");
    EXPECT_EQ(an->logicalData()[0], 1);  // A
    EXPECT_EQ(an->logicalData()[1], 1);  // a
    EXPECT_EQ(an->logicalData()[3], 1);  // 1
    EXPECT_EQ(an->logicalData()[2], 0);  // space
    EXPECT_EQ(an->logicalData()[4], 0);  // !

    eval("lo = isstrprop(s, 'lower');");
    auto *lo = getVarPtr("lo");
    EXPECT_EQ(lo->logicalData()[0], 0);  // A
    EXPECT_EQ(lo->logicalData()[1], 1);  // a

    eval("up = isstrprop(s, 'upper');");
    auto *up = getVarPtr("up");
    EXPECT_EQ(up->logicalData()[0], 1);  // A
    EXPECT_EQ(up->logicalData()[1], 0);  // a

    eval("pn = isstrprop(s, 'punct');");
    auto *pn = getVarPtr("pn");
    EXPECT_EQ(pn->logicalData()[4], 1);  // !
    EXPECT_EQ(pn->logicalData()[0], 0);  // A

    eval("sp = isstrprop(s, 'space');");
    auto *sp = getVarPtr("sp");
    EXPECT_EQ(sp->logicalData()[2], 1);  // ' '
    EXPECT_EQ(sp->logicalData()[5], 1);  // \t
    EXPECT_EQ(sp->logicalData()[0], 0);  // A

    // 'wspace' is an alias for 'space'.
    eval("ws = isstrprop(s, 'wspace');");
    auto *ws = getVarPtr("ws");
    EXPECT_EQ(ws->logicalData()[2], 1);
    EXPECT_EQ(ws->logicalData()[5], 1);

    eval("xd = isstrprop(s, 'xdigit');");
    auto *xd = getVarPtr("xd");
    EXPECT_EQ(xd->logicalData()[1], 1);  // 'a' is hex digit
    EXPECT_EQ(xd->logicalData()[3], 1);  // '1' is hex digit
    EXPECT_EQ(xd->logicalData()[0], 1);  // 'A' is hex digit
    EXPECT_EQ(xd->logicalData()[2], 0);  // space

    eval("ct = isstrprop(s, 'cntrl');");
    auto *ct = getVarPtr("ct");
    EXPECT_EQ(ct->logicalData()[5], 1);  // \t
    EXPECT_EQ(ct->logicalData()[0], 0);  // A

    eval("gr = isstrprop(s, 'graphic');");
    auto *gr = getVarPtr("gr");
    EXPECT_EQ(gr->logicalData()[0], 1);  // A
    EXPECT_EQ(gr->logicalData()[3], 1);  // 1
    EXPECT_EQ(gr->logicalData()[4], 1);  // !
    EXPECT_EQ(gr->logicalData()[2], 0);  // space (printable but not "graphic")
    EXPECT_EQ(gr->logicalData()[5], 0);  // \t

    eval("pr = isstrprop(s, 'print');");
    auto *pr = getVarPtr("pr");
    EXPECT_EQ(pr->logicalData()[0], 1);  // A
    EXPECT_EQ(pr->logicalData()[2], 1);  // space (print includes space)
    EXPECT_EQ(pr->logicalData()[4], 1);  // !
    EXPECT_EQ(pr->logicalData()[5], 0);  // \t (not printable)
}

TEST_P(BuiltinTest, IsStrPropUnknownCategoryThrows)
{
    EXPECT_THROW(eval("isstrprop('abc', 'bogus');"), std::exception);
    // Empty input is allowed; result is 1×0 logical.
    eval("e = isstrprop('', 'alpha');");
    auto *e = getVarPtr("e");
    EXPECT_EQ(e->numel(), 0u);
}

// ============================================================
// Prod-grade test pack T4 — empty-input regression sweep
// ============================================================
//
// For every new builtin that takes an array-shaped argument, [] must
// not crash and must return a sensible empty / scalar result. These
// are the cheap-to-write tests that catch the "didn't think about
// what happens when n == 0" bugs.

TEST_P(BuiltinTest, EmptyInputTrigFamily)
{
    auto isEmpty = [&](const std::string &expr) {
        eval(expr);
        return getVarPtr("z") && getVarPtr("z")->isEmpty();
    };
    EXPECT_TRUE(isEmpty("z = sinh([]);"));
    EXPECT_TRUE(isEmpty("z = cosh([]);"));
    EXPECT_TRUE(isEmpty("z = tanh([]);"));
    EXPECT_TRUE(isEmpty("z = sind([]);"));
    EXPECT_TRUE(isEmpty("z = cosd([]);"));
    EXPECT_TRUE(isEmpty("z = sinpi([]);"));
    EXPECT_TRUE(isEmpty("z = cospi([]);"));
    EXPECT_TRUE(isEmpty("z = sec([]);"));
    EXPECT_TRUE(isEmpty("z = asec([]);"));
}

TEST_P(BuiltinTest, EmptyInputShapePredicates)
{
    // isvector([]) is false (numel == 0), but other predicates handle
    // the empty case in their own way — just check no crash.
    EXPECT_FALSE(evalBool("isvector([]);"));
    EXPECT_TRUE(evalBool("issorted([]);"));
    EXPECT_TRUE(evalBool("isuniform([]);"));
    EXPECT_TRUE(evalBool("allunique([]);"));
    EXPECT_DOUBLE_EQ(evalScalar("numunique([]);"), 0.0);
    EXPECT_TRUE(evalBool("allfinite([]);"));
    EXPECT_FALSE(evalBool("anynan([]);"));
}

TEST_P(BuiltinTest, EmptyInputManipFamily)
{
    auto isEmpty = [&](const std::string &expr) {
        eval(expr);
        return getVarPtr("z") && getVarPtr("z")->isEmpty();
    };
    EXPECT_TRUE(isEmpty("z = flip([]);"));
    EXPECT_TRUE(isEmpty("z = paddata([], 0);"));
    EXPECT_TRUE(isEmpty("z = trimdata([1 2 3], 0);"));
    EXPECT_TRUE(isEmpty("z = resize([], 0);"));
    // poly of empty roots — current impl returns 0-len row.
    eval("e = poly([]);");
    EXPECT_EQ(getVarPtr("e")->numel(), 0u);
    EXPECT_TRUE(isEmpty("z = setxor([], []);"));
    EXPECT_TRUE(isEmpty("z = uniquetol([], 1e-6);"));
}

TEST_P(BuiltinTest, EmptyInputStringFamily)
{
    EXPECT_DOUBLE_EQ(evalScalar("count('', 'x');"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("count('abc', '');"), 0.0);
    eval("a = erase('', 'x');");
    EXPECT_EQ(getVarPtr("a")->toString(), "");
    eval("b = reverse('');");
    EXPECT_EQ(getVarPtr("b")->toString(), "");
    eval("c = pad('', 0);");
    EXPECT_EQ(getVarPtr("c")->toString(), "");
    eval("d = strip('');");
    EXPECT_EQ(getVarPtr("d")->toString(), "");
    EXPECT_FALSE(evalBool("matches('', 'x');"));
    EXPECT_TRUE(evalBool("matches('', '');"));
}

TEST_P(BuiltinTest, EmptyInputSpecialFamily)
{
    auto isEmpty = [&](const std::string &expr) {
        eval(expr);
        return getVarPtr("z") && getVarPtr("z")->isEmpty();
    };
    EXPECT_TRUE(isEmpty("z = besselj(0, []);"));
    EXPECT_TRUE(isEmpty("z = beta([], []);"));
    EXPECT_TRUE(isEmpty("z = gammainc([], 1);"));
    EXPECT_TRUE(isEmpty("z = expint([]);"));
    EXPECT_TRUE(isEmpty("z = psi([]);"));
}

TEST_P(BuiltinTest, EmptyInputCellStruct)
{
    // num2cell([]) returns 0×0 cell.
    eval("c = num2cell([]);");
    EXPECT_TRUE(getVarPtr("c")->isCell());
    EXPECT_EQ(getVarPtr("c")->numel(), 0u);
    // cell2mat({}) returns empty matrix.
    eval("m = cell2mat({});");
    EXPECT_TRUE(getVarPtr("m")->isEmpty());
    // iscellstr({}) is true vacuously.
    EXPECT_TRUE(evalBool("iscellstr({});"));
}

TEST_P(BuiltinTest, EmptyInputBitOps)
{
    auto isEmpty = [&](const std::string &expr) {
        eval(expr);
        return getVarPtr("z") && getVarPtr("z")->isEmpty();
    };
    EXPECT_TRUE(isEmpty("z = bitset([], 1);"));
    EXPECT_TRUE(isEmpty("z = bitget([], 1);"));
}

TEST_P(BuiltinTest, EmptyInputIdivideBsxfun)
{
    auto isEmpty = [&](const std::string &expr) {
        eval(expr);
        return getVarPtr("z") && getVarPtr("z")->isEmpty();
    };
    EXPECT_TRUE(isEmpty("z = idivide(int32([]), int32(3));"));
    EXPECT_TRUE(isEmpty("z = bsxfun(@plus, [], []);"));
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
