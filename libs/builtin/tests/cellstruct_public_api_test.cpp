// libs/builtin/tests/cellstruct_public_api_test.cpp
//
// Direct-call tests for numkit::builtin cell/struct functions.

#include <numkit/builtin/datatypes/cell/cell.hpp>
#include <numkit/builtin/datatypes/struct/struct.hpp>

#include <numkit/core/allocator.hpp>
#include <numkit/core/types.hpp>
#include <numkit/core/value.hpp>

#include <gtest/gtest.h>

using numkit::Allocator;
using numkit::Error;
using numkit::ValueType;
using numkit::Value;
using numkit::Span;

namespace {

Value mkStr(Allocator &alloc, const char *s) { return Value::fromString(s, &alloc); }

} // namespace

// ── structure() ─────────────────────────────────────────────────────────
TEST(BuiltinCellStructPublicApi, EmptyStructure)
{
    Allocator alloc = Allocator::defaultAllocator();
    Value s = numkit::builtin::structure(alloc);
    EXPECT_TRUE(s.isStruct());
    EXPECT_EQ(s.structFields().size(), 0u);
}

TEST(BuiltinCellStructPublicApi, StructureFromNameValuePairs)
{
    Allocator alloc = Allocator::defaultAllocator();
    Value pairs[] = {
        mkStr(alloc, "x"), Value::scalar(3.14, &alloc),
        mkStr(alloc, "y"), Value::scalar(42.0, &alloc),
    };
    Span<const Value> span(pairs, 4);
    Value s = numkit::builtin::structure(alloc, span);
    ASSERT_TRUE(s.isStruct());
    EXPECT_TRUE(s.hasField("x"));
    EXPECT_TRUE(s.hasField("y"));
    EXPECT_DOUBLE_EQ(s.field("x").toScalar(), 3.14);
    EXPECT_DOUBLE_EQ(s.field("y").toScalar(), 42.0);
}

TEST(BuiltinCellStructPublicApi, StructureRejectsNonCharName)
{
    Allocator alloc = Allocator::defaultAllocator();
    Value pairs[] = {Value::scalar(1.0, &alloc), Value::scalar(2.0, &alloc)};
    Span<const Value> span(pairs, 2);
    EXPECT_THROW(numkit::builtin::structure(alloc, span), Error);
}

TEST(BuiltinCellStructPublicApi, StructureOddArgDropsTrailingName)
{
    Allocator alloc = Allocator::defaultAllocator();
    Value pairs[] = {
        mkStr(alloc, "a"), Value::scalar(1.0, &alloc),
        mkStr(alloc, "b"),  // no matching value
    };
    Span<const Value> span(pairs, 3);
    Value s = numkit::builtin::structure(alloc, span);
    EXPECT_TRUE(s.hasField("a"));
    EXPECT_FALSE(s.hasField("b"));
}

// ── fieldnames / isfield / rmfield ──────────────────────────────────────
TEST(BuiltinCellStructPublicApi, FieldnamesReturnsAllNames)
{
    Allocator alloc = Allocator::defaultAllocator();
    Value pairs[] = {
        mkStr(alloc, "alpha"), Value::scalar(1.0, &alloc),
        mkStr(alloc, "beta"),  Value::scalar(2.0, &alloc),
    };
    Value s = numkit::builtin::structure(alloc, Span<const Value>(pairs, 4));
    Value names = numkit::builtin::fieldnames(alloc, s);
    ASSERT_EQ(names.numel(), 2u);
    // Insertion order preserved.
    EXPECT_EQ(names.cellAt(0).toString(), "alpha");
    EXPECT_EQ(names.cellAt(1).toString(), "beta");
}

TEST(BuiltinCellStructPublicApi, FieldnamesThrowsOnNonStruct)
{
    Allocator alloc = Allocator::defaultAllocator();
    EXPECT_THROW(
        numkit::builtin::fieldnames(alloc, Value::scalar(1.0, &alloc)), Error);
}

TEST(BuiltinCellStructPublicApi, IsfieldPresent)
{
    Allocator alloc = Allocator::defaultAllocator();
    Value pairs[] = {mkStr(alloc, "x"), Value::scalar(1.0, &alloc)};
    Value s = numkit::builtin::structure(alloc, Span<const Value>(pairs, 2));
    EXPECT_TRUE(numkit::builtin::isfield(alloc, s, mkStr(alloc, "x")).toBool());
    EXPECT_FALSE(numkit::builtin::isfield(alloc, s, mkStr(alloc, "y")).toBool());
}

TEST(BuiltinCellStructPublicApi, IsfieldOnNonStructReturnsFalse)
{
    Allocator alloc = Allocator::defaultAllocator();
    Value r = numkit::builtin::isfield(alloc, Value::scalar(1.0, &alloc),
                                           mkStr(alloc, "x"));
    EXPECT_FALSE(r.toBool());
}

TEST(BuiltinCellStructPublicApi, RmfieldRemovesExistingField)
{
    Allocator alloc = Allocator::defaultAllocator();
    Value pairs[] = {
        mkStr(alloc, "a"), Value::scalar(1.0, &alloc),
        mkStr(alloc, "b"), Value::scalar(2.0, &alloc),
    };
    Value s = numkit::builtin::structure(alloc, Span<const Value>(pairs, 4));
    Value r = numkit::builtin::rmfield(alloc, s, mkStr(alloc, "a"));
    EXPECT_FALSE(r.hasField("a"));
    EXPECT_TRUE(r.hasField("b"));
}

TEST(BuiltinCellStructPublicApi, RmfieldIgnoresMissingField)
{
    Allocator alloc = Allocator::defaultAllocator();
    Value s = numkit::builtin::structure(alloc);
    Value r = numkit::builtin::rmfield(alloc, s, mkStr(alloc, "nope"));
    EXPECT_TRUE(r.isStruct());
    EXPECT_EQ(r.structFields().size(), 0u);
}

// ── cell ────────────────────────────────────────────────────────────────
TEST(BuiltinCellStructPublicApi, CellOneArgIsSquare)
{
    Allocator alloc = Allocator::defaultAllocator();
    Value c = numkit::builtin::cell(alloc, 3);
    EXPECT_EQ(c.dims().rows(), 3u);
    EXPECT_EQ(c.dims().cols(), 3u);
}

TEST(BuiltinCellStructPublicApi, CellTwoArgs)
{
    Allocator alloc = Allocator::defaultAllocator();
    Value c = numkit::builtin::cell(alloc, 2, 5);
    EXPECT_EQ(c.dims().rows(), 2u);
    EXPECT_EQ(c.dims().cols(), 5u);
}

TEST(BuiltinCellStructPublicApi, CellThreeArgsIs3D)
{
    Allocator alloc = Allocator::defaultAllocator();
    Value c = numkit::builtin::cell(alloc, 2, 3, 4);
    EXPECT_EQ(c.dims().rows(), 2u);
    EXPECT_EQ(c.dims().cols(), 3u);
    EXPECT_EQ(c.dims().pages(), 4u);
}

TEST(BuiltinCellStructPublicApi, CellThreeArgsPagesZeroIs2D)
{
    Allocator alloc = Allocator::defaultAllocator();
    Value c = numkit::builtin::cell(alloc, 2, 3, 0);
    EXPECT_EQ(c.dims().rows(), 2u);
    EXPECT_EQ(c.dims().cols(), 3u);
    EXPECT_FALSE(c.dims().is3D());
}
