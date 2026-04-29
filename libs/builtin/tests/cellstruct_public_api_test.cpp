// libs/builtin/tests/cellstruct_public_api_test.cpp
//
// Direct-call tests for numkit::builtin cell/struct functions.

#include <numkit/builtin/datatypes/cell/cell.hpp>
#include <numkit/builtin/datatypes/struct/struct.hpp>

#include <memory_resource>
#include <numkit/core/types.hpp>
#include <numkit/core/value.hpp>

#include <gtest/gtest.h>

using numkit::Error;
using numkit::ValueType;
using numkit::Value;
using numkit::Span;

namespace {

Value mkStr(std::pmr::memory_resource *mr, const char *s) { return Value::fromString(s, mr); }

} // namespace

// ── structure() ─────────────────────────────────────────────────────────
TEST(BuiltinCellStructPublicApi, EmptyStructure)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    Value s = numkit::builtin::structure(mr);
    EXPECT_TRUE(s.isStruct());
    EXPECT_EQ(s.structFields().size(), 0u);
}

TEST(BuiltinCellStructPublicApi, StructureFromNameValuePairs)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    Value pairs[] = {
        mkStr(mr, "x"), Value::scalar(3.14, mr),
        mkStr(mr, "y"), Value::scalar(42.0, mr),
    };
    Span<const Value> span(pairs, 4);
    Value s = numkit::builtin::structure(mr, span);
    ASSERT_TRUE(s.isStruct());
    EXPECT_TRUE(s.hasField("x"));
    EXPECT_TRUE(s.hasField("y"));
    EXPECT_DOUBLE_EQ(s.field("x").toScalar(), 3.14);
    EXPECT_DOUBLE_EQ(s.field("y").toScalar(), 42.0);
}

TEST(BuiltinCellStructPublicApi, StructureRejectsNonCharName)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    Value pairs[] = {Value::scalar(1.0, mr), Value::scalar(2.0, mr)};
    Span<const Value> span(pairs, 2);
    EXPECT_THROW(numkit::builtin::structure(mr, span), Error);
}

TEST(BuiltinCellStructPublicApi, StructureOddArgDropsTrailingName)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    Value pairs[] = {
        mkStr(mr, "a"), Value::scalar(1.0, mr),
        mkStr(mr, "b"),  // no matching value
    };
    Span<const Value> span(pairs, 3);
    Value s = numkit::builtin::structure(mr, span);
    EXPECT_TRUE(s.hasField("a"));
    EXPECT_FALSE(s.hasField("b"));
}

// ── fieldnames / isfield / rmfield ──────────────────────────────────────
TEST(BuiltinCellStructPublicApi, FieldnamesReturnsAllNames)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    Value pairs[] = {
        mkStr(mr, "alpha"), Value::scalar(1.0, mr),
        mkStr(mr, "beta"),  Value::scalar(2.0, mr),
    };
    Value s = numkit::builtin::structure(mr, Span<const Value>(pairs, 4));
    Value names = numkit::builtin::fieldnames(mr, s);
    ASSERT_EQ(names.numel(), 2u);
    // Insertion order preserved.
    EXPECT_EQ(names.cellAt(0).toString(), "alpha");
    EXPECT_EQ(names.cellAt(1).toString(), "beta");
}

TEST(BuiltinCellStructPublicApi, FieldnamesThrowsOnNonStruct)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    EXPECT_THROW(
        numkit::builtin::fieldnames(mr, Value::scalar(1.0, mr)), Error);
}

TEST(BuiltinCellStructPublicApi, IsfieldPresent)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    Value pairs[] = {mkStr(mr, "x"), Value::scalar(1.0, mr)};
    Value s = numkit::builtin::structure(mr, Span<const Value>(pairs, 2));
    EXPECT_TRUE(numkit::builtin::isfield(mr, s, mkStr(mr, "x")).toBool());
    EXPECT_FALSE(numkit::builtin::isfield(mr, s, mkStr(mr, "y")).toBool());
}

TEST(BuiltinCellStructPublicApi, IsfieldOnNonStructReturnsFalse)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    Value r = numkit::builtin::isfield(mr, Value::scalar(1.0, mr),
                                           mkStr(mr, "x"));
    EXPECT_FALSE(r.toBool());
}

TEST(BuiltinCellStructPublicApi, RmfieldRemovesExistingField)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    Value pairs[] = {
        mkStr(mr, "a"), Value::scalar(1.0, mr),
        mkStr(mr, "b"), Value::scalar(2.0, mr),
    };
    Value s = numkit::builtin::structure(mr, Span<const Value>(pairs, 4));
    Value r = numkit::builtin::rmfield(mr, s, mkStr(mr, "a"));
    EXPECT_FALSE(r.hasField("a"));
    EXPECT_TRUE(r.hasField("b"));
}

TEST(BuiltinCellStructPublicApi, RmfieldIgnoresMissingField)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    Value s = numkit::builtin::structure(mr);
    Value r = numkit::builtin::rmfield(mr, s, mkStr(mr, "nope"));
    EXPECT_TRUE(r.isStruct());
    EXPECT_EQ(r.structFields().size(), 0u);
}

// ── cell ────────────────────────────────────────────────────────────────
TEST(BuiltinCellStructPublicApi, CellOneArgIsSquare)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    Value c = numkit::builtin::cell(mr, 3);
    EXPECT_EQ(c.dims().rows(), 3u);
    EXPECT_EQ(c.dims().cols(), 3u);
}

TEST(BuiltinCellStructPublicApi, CellTwoArgs)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    Value c = numkit::builtin::cell(mr, 2, 5);
    EXPECT_EQ(c.dims().rows(), 2u);
    EXPECT_EQ(c.dims().cols(), 5u);
}

TEST(BuiltinCellStructPublicApi, CellThreeArgsIs3D)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    Value c = numkit::builtin::cell(mr, 2, 3, 4);
    EXPECT_EQ(c.dims().rows(), 2u);
    EXPECT_EQ(c.dims().cols(), 3u);
    EXPECT_EQ(c.dims().pages(), 4u);
}

TEST(BuiltinCellStructPublicApi, CellThreeArgsPagesZeroIs2D)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    Value c = numkit::builtin::cell(mr, 2, 3, 0);
    EXPECT_EQ(c.dims().rows(), 2u);
    EXPECT_EQ(c.dims().cols(), 3u);
    EXPECT_FALSE(c.dims().is3D());
}
