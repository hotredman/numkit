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
