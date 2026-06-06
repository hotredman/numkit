// libs/builtin/tests/zeros_ones_typed_test.cpp
//
// Regression guard for the type-arg form of zeros / ones:
//   zeros(M, N, P, ..., 'uint8')   typed N-D zero array
//   ones(M, N, ..., 'single')      typed N-D ones array
//   zeros(N, 'logical')            square typed array
//   zeros(..., 'like', X)          type pulled from X
// All MATLAB type names supported: double, single, logical,
// {int,uint}{8,16,32,64}.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class ZerosOnesTypedTest : public ::testing::Test
{
public:
    StandardEngine engine;
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
    std::string evalString(const std::string &c) { return eval(c).toString(); }
};

// ── 3-D RGB-image pattern: zeros(M, N, 3, 'uint8') ───────────────────
TEST_F(ZerosOnesTypedTest, RgbImage3DUint8)
{
    eval("z = zeros(4, 5, 3, 'uint8');");
    EXPECT_EQ(static_cast<int>(evalScalar("size(z, 1)")), 4);
    EXPECT_EQ(static_cast<int>(evalScalar("size(z, 2)")), 5);
    EXPECT_EQ(static_cast<int>(evalScalar("size(z, 3)")), 3);
    EXPECT_EQ(evalString("class(z)"), "uint8");
    EXPECT_DOUBLE_EQ(evalScalar("double(z(1, 1, 1))"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(z(4, 5, 3))"), 0.0);
}

// ── Each MATLAB type accepted by zeros ────────────────────────────────
TEST_F(ZerosOnesTypedTest, ZerosWithEachType)
{
    EXPECT_EQ(evalString("class(zeros(2, 3, 'double'))"),  "double");
    EXPECT_EQ(evalString("class(zeros(2, 3, 'single'))"),  "single");
    EXPECT_EQ(evalString("class(zeros(2, 3, 'int8'))"),    "int8");
    EXPECT_EQ(evalString("class(zeros(2, 3, 'int16'))"),   "int16");
    EXPECT_EQ(evalString("class(zeros(2, 3, 'int32'))"),   "int32");
    EXPECT_EQ(evalString("class(zeros(2, 3, 'int64'))"),   "int64");
    EXPECT_EQ(evalString("class(zeros(2, 3, 'uint8'))"),   "uint8");
    EXPECT_EQ(evalString("class(zeros(2, 3, 'uint16'))"),  "uint16");
    EXPECT_EQ(evalString("class(zeros(2, 3, 'uint32'))"),  "uint32");
    EXPECT_EQ(evalString("class(zeros(2, 3, 'uint64'))"),  "uint64");
    EXPECT_EQ(evalString("class(zeros(2, 3, 'logical'))"), "logical");
}

// ── ones with type fills with 1 of the right type ─────────────────────
TEST_F(ZerosOnesTypedTest, OnesUint8FillsWithOne)
{
    eval("o = ones(2, 3, 'uint8');");
    EXPECT_EQ(evalString("class(o)"), "uint8");
    EXPECT_DOUBLE_EQ(evalScalar("double(o(1, 1))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(o(2, 3))"), 1.0);
}

TEST_F(ZerosOnesTypedTest, OnesSingleFillsWithOne)
{
    eval("o = ones(3, 'single');");
    EXPECT_EQ(evalString("class(o)"), "single");
    EXPECT_DOUBLE_EQ(evalScalar("double(o(1, 1))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(o(3, 3))"), 1.0);
}

TEST_F(ZerosOnesTypedTest, OnesLogicalFillsWithTrue)
{
    eval("o = ones(2, 'logical');");
    EXPECT_EQ(evalString("class(o)"), "logical");
    EXPECT_DOUBLE_EQ(evalScalar("double(o(1, 1))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(o(2, 2))"), 1.0);
}

// ── 'like' form pulls type from another value ─────────────────────────
TEST_F(ZerosOnesTypedTest, ZerosLikeForm)
{
    eval("X = uint8([1 2 3]);"
         "z = zeros(2, 3, 'like', X);");
    EXPECT_EQ(evalString("class(z)"), "uint8");
    EXPECT_DOUBLE_EQ(evalScalar("double(z(1, 1))"), 0.0);
}

TEST_F(ZerosOnesTypedTest, OnesLikeForm)
{
    eval("X = int16(0);"
         "o = ones(2, 'like', X);");
    EXPECT_EQ(evalString("class(o)"), "int16");
    EXPECT_DOUBLE_EQ(evalScalar("double(o(1, 1))"), 1.0);
}

// ── Backward compat: no type arg → double ─────────────────────────────
TEST_F(ZerosOnesTypedTest, BackwardCompatDoubleDefault)
{
    EXPECT_EQ(evalString("class(zeros(3, 4))"), "double");
    EXPECT_EQ(evalString("class(ones(2, 3))"),  "double");
    EXPECT_EQ(evalString("class(zeros(5))"),    "double");
}

// ── 3-D and N-D forms with type ───────────────────────────────────────
TEST_F(ZerosOnesTypedTest, ThreeDimUint8)
{
    eval("z = zeros(2, 3, 4, 'uint8');");
    EXPECT_EQ(static_cast<int>(evalScalar("size(z, 3)")), 4);
    EXPECT_EQ(evalString("class(z)"), "uint8");
}

TEST_F(ZerosOnesTypedTest, ThreeDimViaVectorArg)
{
    eval("z = zeros([2 3 4], 'int32');");
    EXPECT_EQ(static_cast<int>(evalScalar("size(z, 1)")), 2);
    EXPECT_EQ(static_cast<int>(evalScalar("size(z, 2)")), 3);
    EXPECT_EQ(static_cast<int>(evalScalar("size(z, 3)")), 4);
    EXPECT_EQ(evalString("class(z)"), "int32");
}

// ── nan / NaN / inf / Inf as functions ────────────────────────────────
TEST_F(ZerosOnesTypedTest, NanBareReturnsScalarNaN)
{
    eval("x = nan;");
    EXPECT_TRUE(std::isnan(evalScalar("x")));
    eval("y = NaN;");
    EXPECT_TRUE(std::isnan(evalScalar("y")));
}

TEST_F(ZerosOnesTypedTest, InfBareReturnsScalarInf)
{
    eval("x = inf;");
    EXPECT_TRUE(std::isinf(evalScalar("x")));
    eval("y = Inf;");
    EXPECT_TRUE(std::isinf(evalScalar("y")));
}

TEST_F(ZerosOnesTypedTest, NanWithDimsFillsArray)
{
    eval("x = nan(2, 3);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(x, 1)")), 2);
    EXPECT_EQ(static_cast<int>(evalScalar("size(x, 2)")), 3);
    EXPECT_TRUE(std::isnan(evalScalar("x(1, 1)")));
    EXPECT_TRUE(std::isnan(evalScalar("x(2, 3)")));
    EXPECT_EQ(evalString("class(x)"), "double");
}

TEST_F(ZerosOnesTypedTest, NanWithSingleType)
{
    eval("x = NaN(2, 'single');");
    EXPECT_EQ(evalString("class(x)"), "single");
    EXPECT_TRUE(std::isnan(evalScalar("double(x(1, 1))")));
}

TEST_F(ZerosOnesTypedTest, InfWithDimsFillsArray)
{
    eval("x = Inf(2, 3, 'single');");
    EXPECT_EQ(static_cast<int>(evalScalar("size(x, 1)")), 2);
    EXPECT_EQ(evalString("class(x)"), "single");
    EXPECT_TRUE(std::isinf(evalScalar("double(x(1, 1))")));
}

TEST_F(ZerosOnesTypedTest, NanRejectsIntegerType)
{
    bool threw = false;
    try { eval("x = nan(2, 'uint8');"); } catch (...) { threw = true; }
    EXPECT_TRUE(threw);
}

// ── eye with type arg ─────────────────────────────────────────────────
TEST_F(ZerosOnesTypedTest, EyeTypedSingle)
{
    eval("e = eye(3, 'single');");
    EXPECT_EQ(evalString("class(e)"), "single");
    EXPECT_DOUBLE_EQ(evalScalar("double(e(1, 1))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(e(2, 2))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(e(1, 2))"), 0.0);
}

TEST_F(ZerosOnesTypedTest, EyeTypedUint8Rectangular)
{
    eval("e = eye(2, 4, 'uint8');");
    EXPECT_EQ(evalString("class(e)"), "uint8");
    EXPECT_EQ(static_cast<int>(evalScalar("size(e, 1)")), 2);
    EXPECT_EQ(static_cast<int>(evalScalar("size(e, 2)")), 4);
    EXPECT_DOUBLE_EQ(evalScalar("double(e(1, 1))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(e(2, 4))"), 0.0);
}

// ── rand / randn / randi with type ────────────────────────────────────
TEST_F(ZerosOnesTypedTest, RandTypedSingle)
{
    eval("x = rand(2, 3, 'single');");
    EXPECT_EQ(evalString("class(x)"), "single");
    EXPECT_GE(evalScalar("double(x(1, 1))"), 0.0);
    EXPECT_LT(evalScalar("double(x(1, 1))"), 1.0);
}

TEST_F(ZerosOnesTypedTest, RandnTypedSingle)
{
    eval("x = randn(3, 'single');");
    EXPECT_EQ(evalString("class(x)"), "single");
}

TEST_F(ZerosOnesTypedTest, RandiTypedUint8)
{
    eval("x = randi(10, 3, 'uint8');");
    EXPECT_EQ(evalString("class(x)"), "uint8");
    EXPECT_GE(evalScalar("double(x(1, 1))"), 1.0);
    EXPECT_LE(evalScalar("double(x(1, 1))"), 10.0);
}

TEST_F(ZerosOnesTypedTest, RandiTypedInt16Multidim)
{
    eval("x = randi(100, 2, 3, 'int16');");
    EXPECT_EQ(evalString("class(x)"), "int16");
    EXPECT_EQ(static_cast<int>(evalScalar("size(x, 1)")), 2);
    EXPECT_EQ(static_cast<int>(evalScalar("size(x, 2)")), 3);
}

// ── cast(x, 'like', y) form ───────────────────────────────────────────
TEST_F(ZerosOnesTypedTest, CastLikePullsTypeFromSource)
{
    eval("y = cast(3.14, 'like', uint8(0));");
    EXPECT_EQ(evalString("class(y)"), "uint8");
    EXPECT_DOUBLE_EQ(evalScalar("double(y)"), 3.0);  // truncating cast
}

TEST_F(ZerosOnesTypedTest, CastLikeSingleFromArray)
{
    eval("y = cast([1.5 2.5], 'like', single(0));");
    EXPECT_EQ(evalString("class(y)"), "single");
}

// ── colon as a function ───────────────────────────────────────────────
TEST_F(ZerosOnesTypedTest, ColonTwoArg)
{
    eval("v = colon(1, 5);");
    EXPECT_EQ(static_cast<int>(evalScalar("numel(v)")), 5);
    EXPECT_DOUBLE_EQ(evalScalar("v(1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("v(5)"), 5.0);
}

TEST_F(ZerosOnesTypedTest, ColonThreeArgWithStep)
{
    eval("v = colon(0, 0.5, 2);");
    EXPECT_EQ(static_cast<int>(evalScalar("numel(v)")), 5);
    EXPECT_DOUBLE_EQ(evalScalar("v(1)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("v(end)"), 2.0);
}

// ── sparse stub ───────────────────────────────────────────────────────
TEST_F(ZerosOnesTypedTest, SparseStubReturnsDenseZeros)
{
    eval("s = sparse(3, 4);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(s, 1)")), 3);
    EXPECT_EQ(static_cast<int>(evalScalar("size(s, 2)")), 4);
    EXPECT_DOUBLE_EQ(evalScalar("s(1, 1)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(issparse(s))"), 0.0);  // we have no sparse
}

TEST_F(ZerosOnesTypedTest, SparseStubPassthrough)
{
    eval("A = [1 2; 3 4]; sA = sparse(A);");
    EXPECT_DOUBLE_EQ(evalScalar("sA(1, 1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("sA(2, 2)"), 4.0);
}

// ── Typed colon operator (j:k and j:i:k preserve int/single type) ──
TEST_F(ZerosOnesTypedTest, ColonOpInt32Preserved)
{
    eval("v = int32(1):int32(5);");
    EXPECT_EQ(evalString("class(v)"), "int32");
    EXPECT_EQ(static_cast<int>(evalScalar("numel(v)")), 5);
    EXPECT_DOUBLE_EQ(evalScalar("double(v(1))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(v(5))"), 5.0);
}

TEST_F(ZerosOnesTypedTest, ColonOpUint8Preserved)
{
    eval("v = uint8(0):uint8(2):uint8(10);");
    EXPECT_EQ(evalString("class(v)"), "uint8");
    EXPECT_EQ(static_cast<int>(evalScalar("numel(v)")), 6);
    EXPECT_DOUBLE_EQ(evalScalar("double(v(end))"), 10.0);
}

TEST_F(ZerosOnesTypedTest, ColonOpSinglePreserved)
{
    eval("v = single(1):single(5);");
    EXPECT_EQ(evalString("class(v)"), "single");
}

TEST_F(ZerosOnesTypedTest, ColonOpMixedDoubleInt32IsInt32)
{
    eval("v = 1:int32(5);");
    EXPECT_EQ(evalString("class(v)"), "int32");
    eval("u = int32(1):5;");
    EXPECT_EQ(evalString("class(u)"), "int32");
}

TEST_F(ZerosOnesTypedTest, ColonOpAllDoubleStaysDouble)
{
    eval("v = 1:5;");
    EXPECT_EQ(evalString("class(v)"), "double");
}

TEST_F(ZerosOnesTypedTest, ColonOpIntNegStep)
{
    eval("v = int32(5):int32(-1):int32(1);");
    EXPECT_EQ(evalString("class(v)"), "int32");
    EXPECT_EQ(static_cast<int>(evalScalar("numel(v)")), 5);
    EXPECT_DOUBLE_EQ(evalScalar("double(v(1))"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(v(end))"), 1.0);
}

TEST_F(ZerosOnesTypedTest, ColonOpMixedIntKindsThrows)
{
    bool threw = false;
    try { eval("v = int8(1):int16(5);"); } catch (...) { threw = true; }
    EXPECT_TRUE(threw);
}

TEST_F(ZerosOnesTypedTest, ColonFunctionTypePropagation)
{
    eval("v = colon(int32(1), int32(5));");
    EXPECT_EQ(evalString("class(v)"), "int32");
    eval("u = colon(uint8(0), uint8(2), uint8(10));");
    EXPECT_EQ(evalString("class(u)"), "uint8");
}

// ── Pre-existing colon count off-by-one fix ──────────────────────────
// 1:2:10 should give 5 elements [1 3 5 7 9], not 6 [1 3 5 7 9 10].
// Same for typed variants.
TEST_F(ZerosOnesTypedTest, ColonCountNoOvershootCleanInt)
{
    EXPECT_EQ(static_cast<int>(evalScalar("numel(1:2:10)")), 5);
    EXPECT_DOUBLE_EQ(evalScalar("v=1:2:10; v(end)"), 9.0);
}

TEST_F(ZerosOnesTypedTest, ColonCountKeepsLastFractional)
{
    // 0:0.1:1.0 should give 11 elements (preserves last via FP-tol).
    EXPECT_EQ(static_cast<int>(evalScalar("numel(0:0.1:1.0)")), 11);
}
