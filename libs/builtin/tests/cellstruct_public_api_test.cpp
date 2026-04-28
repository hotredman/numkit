// libs/builtin/tests/cellstruct_public_api_test.cpp
//
// Direct-call tests for numkit::m::builtin cell/struct functions.

#include <numkit/m/builtin/datatypes/cell/cell.hpp>
#include <numkit/m/builtin/datatypes/struct/struct.hpp>

#include <numkit/m/core/MAllocator.hpp>
#include <numkit/m/core/MTypes.hpp>
#include <numkit/m/core/MValue.hpp>

#include <gtest/gtest.h>

using numkit::m::Allocator;
using numkit::m::MError;
using numkit::m::MType;
using numkit::m::MValue;
using numkit::m::Span;

namespace {

MValue mkStr(Allocator &alloc, const char *s) { return MValue::fromString(s, &alloc); }

} // namespace

// ── structure() ─────────────────────────────────────────────────────────
TEST(BuiltinCellStructPublicApi, EmptyStructure)
{
    Allocator alloc = Allocator::defaultAllocator();
    MValue s = numkit::m::builtin::structure(alloc);
    EXPECT_TRUE(s.isStruct());
    EXPECT_EQ(s.structFields().size(), 0u);
}

TEST(BuiltinCellStructPublicApi, StructureFromNameValuePairs)
{
    Allocator alloc = Allocator::defaultAllocator();
    MValue pairs[] = {
        mkStr(alloc, "x"), MValue::scalar(3.14, &alloc),
        mkStr(alloc, "y"), MValue::scalar(42.0, &alloc),
    };
    Span<const MValue> span(pairs, 4);
    MValue s = numkit::m::builtin::structure(alloc, span);
    ASSERT_TRUE(s.isStruct());
    EXPECT_TRUE(s.hasField("x"));
    EXPECT_TRUE(s.hasField("y"));
    EXPECT_DOUBLE_EQ(s.field("x").toScalar(), 3.14);
    EXPECT_DOUBLE_EQ(s.field("y").toScalar(), 42.0);
}

TEST(BuiltinCellStructPublicApi, StructureRejectsNonCharName)
{
    Allocator alloc = Allocator::defaultAllocator();
    MValue pairs[] = {MValue::scalar(1.0, &alloc), MValue::scalar(2.0, &alloc)};
    Span<const MValue> span(pairs, 2);
    EXPECT_THROW(numkit::m::builtin::structure(alloc, span), MError);
}

TEST(BuiltinCellStructPublicApi, StructureOddArgDropsTrailingName)
{
    Allocator alloc = Allocator::defaultAllocator();
    MValue pairs[] = {
        mkStr(alloc, "a"), MValue::scalar(1.0, &alloc),
        mkStr(alloc, "b"),  // no matching value
    };
    Span<const MValue> span(pairs, 3);
    MValue s = numkit::m::builtin::structure(alloc, span);
    EXPECT_TRUE(s.hasField("a"));
    EXPECT_FALSE(s.hasField("b"));
}

// ── fieldnames / isfield / rmfield ──────────────────────────────────────
TEST(BuiltinCellStructPublicApi, FieldnamesReturnsAllNames)
{
    Allocator alloc = Allocator::defaultAllocator();
    MValue pairs[] = {
        mkStr(alloc, "alpha"), MValue::scalar(1.0, &alloc),
        mkStr(alloc, "beta"),  MValue::scalar(2.0, &alloc),
    };
    MValue s = numkit::m::builtin::structure(alloc, Span<const MValue>(pairs, 4));
    MValue names = numkit::m::builtin::fieldnames(alloc, s);
    ASSERT_EQ(names.numel(), 2u);
    // Insertion order preserved.
    EXPECT_EQ(names.cellAt(0).toString(), "alpha");
    EXPECT_EQ(names.cellAt(1).toString(), "beta");
}

TEST(BuiltinCellStructPublicApi, FieldnamesThrowsOnNonStruct)
{
    Allocator alloc = Allocator::defaultAllocator();
    EXPECT_THROW(
        numkit::m::builtin::fieldnames(alloc, MValue::scalar(1.0, &alloc)), MError);
}

TEST(BuiltinCellStructPublicApi, IsfieldPresent)
{
    Allocator alloc = Allocator::defaultAllocator();
    MValue pairs[] = {mkStr(alloc, "x"), MValue::scalar(1.0, &alloc)};
    MValue s = numkit::m::builtin::structure(alloc, Span<const MValue>(pairs, 2));
    EXPECT_TRUE(numkit::m::builtin::isfield(alloc, s, mkStr(alloc, "x")).toBool());
    EXPECT_FALSE(numkit::m::builtin::isfield(alloc, s, mkStr(alloc, "y")).toBool());
}

TEST(BuiltinCellStructPublicApi, IsfieldOnNonStructReturnsFalse)
{
    Allocator alloc = Allocator::defaultAllocator();
    MValue r = numkit::m::builtin::isfield(alloc, MValue::scalar(1.0, &alloc),
                                           mkStr(alloc, "x"));
    EXPECT_FALSE(r.toBool());
}

TEST(BuiltinCellStructPublicApi, RmfieldRemovesExistingField)
{
    Allocator alloc = Allocator::defaultAllocator();
    MValue pairs[] = {
        mkStr(alloc, "a"), MValue::scalar(1.0, &alloc),
        mkStr(alloc, "b"), MValue::scalar(2.0, &alloc),
    };
    MValue s = numkit::m::builtin::structure(alloc, Span<const MValue>(pairs, 4));
    MValue r = numkit::m::builtin::rmfield(alloc, s, mkStr(alloc, "a"));
    EXPECT_FALSE(r.hasField("a"));
    EXPECT_TRUE(r.hasField("b"));
}

TEST(BuiltinCellStructPublicApi, RmfieldIgnoresMissingField)
{
    Allocator alloc = Allocator::defaultAllocator();
    MValue s = numkit::m::builtin::structure(alloc);
    MValue r = numkit::m::builtin::rmfield(alloc, s, mkStr(alloc, "nope"));
    EXPECT_TRUE(r.isStruct());
    EXPECT_EQ(r.structFields().size(), 0u);
}

// ── cell ────────────────────────────────────────────────────────────────
TEST(BuiltinCellStructPublicApi, CellOneArgIsSquare)
{
    Allocator alloc = Allocator::defaultAllocator();
    MValue c = numkit::m::builtin::cell(alloc, 3);
    EXPECT_EQ(c.dims().rows(), 3u);
    EXPECT_EQ(c.dims().cols(), 3u);
}

TEST(BuiltinCellStructPublicApi, CellTwoArgs)
{
    Allocator alloc = Allocator::defaultAllocator();
    MValue c = numkit::m::builtin::cell(alloc, 2, 5);
    EXPECT_EQ(c.dims().rows(), 2u);
    EXPECT_EQ(c.dims().cols(), 5u);
}

TEST(BuiltinCellStructPublicApi, CellThreeArgsIs3D)
{
    Allocator alloc = Allocator::defaultAllocator();
    MValue c = numkit::m::builtin::cell(alloc, 2, 3, 4);
    EXPECT_EQ(c.dims().rows(), 2u);
    EXPECT_EQ(c.dims().cols(), 3u);
    EXPECT_EQ(c.dims().pages(), 4u);
}

TEST(BuiltinCellStructPublicApi, CellThreeArgsPagesZeroIs2D)
{
    Allocator alloc = Allocator::defaultAllocator();
    MValue c = numkit::m::builtin::cell(alloc, 2, 3, 0);
    EXPECT_EQ(c.dims().rows(), 2u);
    EXPECT_EQ(c.dims().cols(), 3u);
    EXPECT_FALSE(c.dims().is3D());
}
