// codegen/tests/transfer_test.cpp
//
// Unit tests for the transfer functions in isolation (no engine): the
// rules' shape-from-value logic and the registry's unknown-builtin
// fallback. The differential test (transfer_diff_test.cpp) separately
// checks these predictions against the real runtime.

#include <numkit/codegen/transfer.hpp>

#include <gtest/gtest.h>

using numkit::ValueType;
using namespace numkit::codegen;

namespace {
TransferRegistry makeRegistry()
{
    TransferRegistry reg;
    registerStandardTransfers(reg);
    return reg;
}
}  // namespace

// An unknown builtin has no transfer -> Dynamic (sound boxed fallback).
TEST(Transfer, UnknownBuiltinIsDynamic)
{
    const auto reg = makeRegistry();
    EXPECT_FALSE(reg.has("totally_unknown_fn"));
    EXPECT_TRUE(reg.apply("totally_unknown_fn", {}).isDynamic());
}

// linspace with a known constant length -> concrete 1 x N double row.
TEST(Transfer, LinspaceKnownLength)
{
    const auto reg = makeRegistry();
    const auto t = reg.apply("linspace", {
        ArgInfo::scalarConst(ValueType::DOUBLE, 0.0),
        ArgInfo::scalarConst(ValueType::DOUBLE, 1.0),
        ArgInfo::scalarConst(ValueType::DOUBLE, 64.0),
    });
    ASSERT_TRUE(t.isConcrete());
    EXPECT_EQ(t.dtype, ValueType::DOUBLE);
    EXPECT_EQ(t.shape.kind, ShapeKind::KnownDims);
    EXPECT_EQ(t.shape.rows, 1u);
    EXPECT_EQ(t.shape.cols, 64u);
}

// The 2-argument form uses MATLAB's default length of 100.
TEST(Transfer, LinspaceDefaultLength)
{
    const auto reg = makeRegistry();
    const auto t = reg.apply("linspace", {
        ArgInfo::scalarConst(ValueType::DOUBLE, 0.0),
        ArgInfo::scalarConst(ValueType::DOUBLE, 1.0),
    });
    ASSERT_TRUE(t.isConcrete());
    EXPECT_EQ(t.shape.cols, 100u);
}

// A runtime (non-constant) length keeps the dtype but loses the shape.
TEST(Transfer, LinspaceRuntimeLength)
{
    const auto reg = makeRegistry();
    const auto t = reg.apply("linspace", {
        ArgInfo::scalarConst(ValueType::DOUBLE, 0.0),
        ArgInfo::scalarConst(ValueType::DOUBLE, 1.0),
        ArgInfo::of(InferredType::scalar(ValueType::DOUBLE)),  // n: unknown const
    });
    ASSERT_TRUE(t.isConcrete());
    EXPECT_EQ(t.dtype, ValueType::DOUBLE);
    EXPECT_EQ(t.shape.kind, ShapeKind::Unknown);
}

// zeros(m, n) with constant dims -> KnownDims(m, n); zeros(n) -> n x n;
// zeros() -> scalar.
TEST(Transfer, ZerosShapes)
{
    const auto reg = makeRegistry();

    const auto sq = reg.apply("zeros", {ArgInfo::scalarConst(ValueType::DOUBLE, 5.0)});
    EXPECT_EQ(sq.shape.kind, ShapeKind::KnownDims);
    EXPECT_EQ(sq.shape.rows, 5u);
    EXPECT_EQ(sq.shape.cols, 5u);

    const auto rc = reg.apply("zeros", {
        ArgInfo::scalarConst(ValueType::DOUBLE, 3.0),
        ArgInfo::scalarConst(ValueType::DOUBLE, 4.0),
    });
    EXPECT_EQ(rc.shape.rows, 3u);
    EXPECT_EQ(rc.shape.cols, 4u);

    EXPECT_TRUE(reg.apply("zeros", {}).shape.isScalar());

    // A runtime dimension drops the shape to Unknown (dtype kept).
    const auto rt = reg.apply("zeros", {ArgInfo::of(InferredType::scalar(ValueType::DOUBLE))});
    EXPECT_EQ(rt.dtype, ValueType::DOUBLE);
    EXPECT_EQ(rt.shape.kind, ShapeKind::Unknown);
}

// ones shares the size-constructor rule.
TEST(Transfer, OnesShape)
{
    const auto reg = makeRegistry();
    const auto t = reg.apply("ones", {
        ArgInfo::scalarConst(ValueType::DOUBLE, 2.0),
        ArgInfo::scalarConst(ValueType::DOUBLE, 7.0),
    });
    EXPECT_EQ(t.dtype, ValueType::DOUBLE);
    EXPECT_EQ(t.shape.rows, 2u);
    EXPECT_EQ(t.shape.cols, 7u);
}

// ── elementwise rules in isolation ───────────────────────────────────

// Arithmetic promotion: complex dominates, then integer, then single.
TEST(Transfer, ArithmeticPromotion)
{
    const auto reg = makeRegistry();
    auto plus = [&](ValueType a, ValueType b) {
        return reg.apply("plus", {ArgInfo::of(InferredType::scalar(a)),
                                  ArgInfo::of(InferredType::scalar(b))});
    };
    EXPECT_EQ(plus(ValueType::DOUBLE, ValueType::DOUBLE).dtype, ValueType::DOUBLE);
    EXPECT_EQ(plus(ValueType::DOUBLE, ValueType::COMPLEX).dtype, ValueType::COMPLEX);
    EXPECT_EQ(plus(ValueType::DOUBLE, ValueType::INT8).dtype, ValueType::INT8);
    EXPECT_EQ(plus(ValueType::DOUBLE, ValueType::SINGLE).dtype, ValueType::SINGLE);
    EXPECT_EQ(plus(ValueType::LOGICAL, ValueType::LOGICAL).dtype, ValueType::DOUBLE);
    // mixed distinct integers -> can't type (MATLAB errors)
    EXPECT_TRUE(plus(ValueType::INT8, ValueType::INT16).isDynamic());
}

// Broadcast: scalar + array -> array shape; array + same-array -> same.
TEST(Transfer, ArithmeticBroadcast)
{
    const auto reg = makeRegistry();
    const auto arr = InferredType::concrete(ValueType::DOUBLE, Shape::dims(1, 3));
    const auto sc  = InferredType::scalar(ValueType::DOUBLE);

    const auto t1 = reg.apply("plus", {ArgInfo::of(sc), ArgInfo::of(arr)});
    EXPECT_EQ(t1.shape.kind, ShapeKind::KnownDims);
    EXPECT_EQ(t1.shape.cols, 3u);

    const auto t2 = reg.apply("plus", {ArgInfo::of(arr), ArgInfo::of(arr)});
    EXPECT_EQ(t2.shape.cols, 3u);

    // differing known dims -> shape not provable -> Unknown (dtype kept)
    const auto other = InferredType::concrete(ValueType::DOUBLE, Shape::dims(2, 2));
    const auto t3 = reg.apply("plus", {ArgInfo::of(arr), ArgInfo::of(other)});
    EXPECT_EQ(t3.dtype, ValueType::DOUBLE);
    EXPECT_EQ(t3.shape.kind, ShapeKind::Unknown);
}

// Comparison is logical regardless of numeric input dtype.
TEST(Transfer, ComparisonIsLogical)
{
    const auto reg = makeRegistry();
    const auto t = reg.apply("lt", {ArgInfo::of(InferredType::scalar(ValueType::SINGLE)),
                                    ArgInfo::of(InferredType::scalar(ValueType::DOUBLE))});
    EXPECT_EQ(t.dtype, ValueType::LOGICAL);
    EXPECT_TRUE(t.shape.isScalar());
}

// abs narrows complex to real double; sin keeps real real.
TEST(Transfer, UnaryMath)
{
    const auto reg = makeRegistry();
    EXPECT_EQ(reg.apply("abs", {ArgInfo::of(InferredType::scalar(ValueType::COMPLEX))}).dtype,
              ValueType::DOUBLE);
    EXPECT_EQ(reg.apply("sin", {ArgInfo::of(InferredType::scalar(ValueType::DOUBLE))}).dtype,
              ValueType::DOUBLE);
    // sin on an integer errors in MATLAB -> not safely typeable
    EXPECT_TRUE(reg.apply("sin", {ArgInfo::of(InferredType::scalar(ValueType::INT8))}).isDynamic());
}

// atan2/hypot: scalar real^2 -> scalar real; complex or array arg -> Dynamic
// (no std complex overload; codegen lowers only the scalar real case).
TEST(Transfer, BinaryMath)
{
    const auto reg = makeRegistry();
    EXPECT_EQ(reg.apply("atan2", {ArgInfo::of(InferredType::scalar(ValueType::DOUBLE)),
                                  ArgInfo::of(InferredType::scalar(ValueType::DOUBLE))})
                  .dtype,
              ValueType::DOUBLE);
    EXPECT_TRUE(reg.apply("hypot", {ArgInfo::of(InferredType::scalar(ValueType::COMPLEX)),
                                    ArgInfo::of(InferredType::scalar(ValueType::DOUBLE))})
                    .isDynamic());
    EXPECT_TRUE(reg.apply("atan2",
                          {ArgInfo::of(InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())),
                           ArgInfo::of(InferredType::scalar(ValueType::DOUBLE))})
                    .isDynamic());
}
