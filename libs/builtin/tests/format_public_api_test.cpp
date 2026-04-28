// libs/builtin/tests/format_public_api_test.cpp
//
// Direct-call tests for numkit::builtin formatting primitives.

#include <numkit/builtin/datatypes/strings/format.hpp>

#include <numkit/core/allocator.hpp>
#include <numkit/core/span.hpp>
#include <numkit/core/value.hpp>

#include <gtest/gtest.h>

using numkit::Allocator;
using numkit::ValueType;
using numkit::Value;
using numkit::Span;

namespace {

Value mkStr(Allocator &alloc, const char *s) { return Value::fromString(s, &alloc); }

} // namespace

// ── countFormatSpecs ────────────────────────────────────────────────────
TEST(BuiltinFormatPublicApi, CountFormatSpecsBasic)
{
    EXPECT_EQ(numkit::builtin::countFormatSpecs("%d %s"), 2u);
    EXPECT_EQ(numkit::builtin::countFormatSpecs("no specs here"), 0u);
    EXPECT_EQ(numkit::builtin::countFormatSpecs(""), 0u);
}

TEST(BuiltinFormatPublicApi, CountFormatSpecsIgnoresEscapedPercent)
{
    EXPECT_EQ(numkit::builtin::countFormatSpecs("100%% done: %d"), 1u);
}

TEST(BuiltinFormatPublicApi, CountFormatSpecsWithFlagsWidthPrecision)
{
    EXPECT_EQ(numkit::builtin::countFormatSpecs("%-5.2f %+03d"), 2u);
}

// ── formatOnce ──────────────────────────────────────────────────────────
TEST(BuiltinFormatPublicApi, FormatOnceIntegerAndString)
{
    Allocator alloc = Allocator::defaultAllocator();
    Value args[] = {Value::scalar(42.0, &alloc), mkStr(alloc, "hello")};
    std::string out = numkit::builtin::formatOnce("%d - %s!", Span<const Value>(args, 2));
    EXPECT_EQ(out, "42 - hello!");
}

TEST(BuiltinFormatPublicApi, FormatOnceFloatPrecision)
{
    Allocator alloc = Allocator::defaultAllocator();
    Value args[] = {Value::scalar(3.14159, &alloc)};
    std::string out = numkit::builtin::formatOnce("%.2f", Span<const Value>(args, 1));
    EXPECT_EQ(out, "3.14");
}

TEST(BuiltinFormatPublicApi, FormatOnceEscapes)
{
    std::string out = numkit::builtin::formatOnce(
        "line1\\nline2\\t\\\\", Span<const Value>{});
    EXPECT_EQ(out, "line1\nline2\t\\");
}

TEST(BuiltinFormatPublicApi, FormatOncePercentPercent)
{
    Allocator alloc = Allocator::defaultAllocator();
    Value args[] = {Value::scalar(50.0, &alloc)};
    std::string out = numkit::builtin::formatOnce("%d%%", Span<const Value>(args, 1));
    EXPECT_EQ(out, "50%");
}

TEST(BuiltinFormatPublicApi, FormatOnceHexAndOctal)
{
    Allocator alloc = Allocator::defaultAllocator();
    Value args[] = {Value::scalar(255.0, &alloc), Value::scalar(8.0, &alloc)};
    std::string out =
        numkit::builtin::formatOnce("%x %o", Span<const Value>(args, 2));
    EXPECT_EQ(out, "ff 10");
}

// ── formatCyclic ────────────────────────────────────────────────────────
TEST(BuiltinFormatPublicApi, FormatCyclicRepeatsOverArray)
{
    Allocator alloc = Allocator::defaultAllocator();
    auto arr = Value::matrix(1, 4, ValueType::DOUBLE, &alloc);
    double *d = arr.doubleDataMut();
    d[0] = 1; d[1] = 2; d[2] = 3; d[3] = 4;
    Value args[] = {arr};
    std::string out = numkit::builtin::formatCyclic(
        alloc, "%d ", Span<const Value>(args, 1), 0);
    EXPECT_EQ(out, "1 2 3 4 ");
}

TEST(BuiltinFormatPublicApi, FormatCyclicMultipleSpecsPerCycle)
{
    Allocator alloc = Allocator::defaultAllocator();
    auto arr = Value::matrix(1, 4, ValueType::DOUBLE, &alloc);
    double *d = arr.doubleDataMut();
    d[0] = 1; d[1] = 2; d[2] = 3; d[3] = 4;
    Value args[] = {arr};
    // Two specs → two pairs: "1 2\n3 4\n"
    std::string out = numkit::builtin::formatCyclic(
        alloc, "%d %d\\n", Span<const Value>(args, 1), 0);
    EXPECT_EQ(out, "1 2\n3 4\n");
}

TEST(BuiltinFormatPublicApi, FormatCyclicEmptyArgsJustPrintsFmt)
{
    Allocator alloc = Allocator::defaultAllocator();
    std::string out = numkit::builtin::formatCyclic(
        alloc, "just text\\n", Span<const Value>{}, 0);
    EXPECT_EQ(out, "just text\n");
}

// ── sprintf (Value wrapper) ────────────────────────────────────────────
TEST(BuiltinFormatPublicApi, SprintfReturnsCharArray)
{
    Allocator alloc = Allocator::defaultAllocator();
    Value args[] = {Value::scalar(7.0, &alloc), mkStr(alloc, "foo")};
    Value r = numkit::builtin::sprintf(alloc, mkStr(alloc, "%d/%s"),
                                           Span<const Value>(args, 2));
    ASSERT_TRUE(r.isChar());
    EXPECT_EQ(r.toString(), "7/foo");
}

TEST(BuiltinFormatPublicApi, SprintfNonCharFmtReturnsEmpty)
{
    Allocator alloc = Allocator::defaultAllocator();
    Value r = numkit::builtin::sprintf(alloc, Value::scalar(1.0, &alloc),
                                           Span<const Value>{});
    ASSERT_TRUE(r.isChar());
    EXPECT_EQ(r.toString(), "");
}
