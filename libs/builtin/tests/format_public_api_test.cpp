// libs/builtin/tests/format_public_api_test.cpp
//
// Direct-call tests for numkit::builtin formatting primitives.

#include <numkit/builtin/datatypes/strings/format.hpp>

#include <memory_resource>
#include <numkit/core/span.hpp>
#include <numkit/core/value.hpp>

#include <gtest/gtest.h>

using numkit::ValueType;
using numkit::Value;
using numkit::Span;

namespace {

Value mkStr(std::pmr::memory_resource *mr, const char *s) { return Value::fromString(s, mr); }

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
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    Value args[] = {Value::scalar(42.0, mr), mkStr(mr, "hello")};
    std::string out = numkit::builtin::formatOnce("%d - %s!", Span<const Value>(args, 2));
    EXPECT_EQ(out, "42 - hello!");
}

TEST(BuiltinFormatPublicApi, FormatOnceFloatPrecision)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    Value args[] = {Value::scalar(3.14159, mr)};
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
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    Value args[] = {Value::scalar(50.0, mr)};
    std::string out = numkit::builtin::formatOnce("%d%%", Span<const Value>(args, 1));
    EXPECT_EQ(out, "50%");
}

TEST(BuiltinFormatPublicApi, FormatOnceHexAndOctal)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    Value args[] = {Value::scalar(255.0, mr), Value::scalar(8.0, mr)};
    std::string out =
        numkit::builtin::formatOnce("%x %o", Span<const Value>(args, 2));
    EXPECT_EQ(out, "ff 10");
}

// ── formatCyclic ────────────────────────────────────────────────────────
TEST(BuiltinFormatPublicApi, FormatCyclicRepeatsOverArray)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    auto arr = Value::matrix(1, 4, ValueType::DOUBLE, mr);
    double *d = arr.doubleDataMut();
    d[0] = 1; d[1] = 2; d[2] = 3; d[3] = 4;
    Value args[] = {arr};
    std::string out = numkit::builtin::formatCyclic(
        mr, "%d ", Span<const Value>(args, 1), 0);
    EXPECT_EQ(out, "1 2 3 4 ");
}

TEST(BuiltinFormatPublicApi, FormatCyclicMultipleSpecsPerCycle)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    auto arr = Value::matrix(1, 4, ValueType::DOUBLE, mr);
    double *d = arr.doubleDataMut();
    d[0] = 1; d[1] = 2; d[2] = 3; d[3] = 4;
    Value args[] = {arr};
    // Two specs → two pairs: "1 2\n3 4\n"
    std::string out = numkit::builtin::formatCyclic(
        mr, "%d %d\\n", Span<const Value>(args, 1), 0);
    EXPECT_EQ(out, "1 2\n3 4\n");
}

TEST(BuiltinFormatPublicApi, FormatCyclicEmptyArgsJustPrintsFmt)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    std::string out = numkit::builtin::formatCyclic(
        mr, "just text\\n", Span<const Value>{}, 0);
    EXPECT_EQ(out, "just text\n");
}

// ── sprintf (Value wrapper) ────────────────────────────────────────────
TEST(BuiltinFormatPublicApi, SprintfReturnsCharArray)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    Value args[] = {Value::scalar(7.0, mr), mkStr(mr, "foo")};
    Value r = numkit::builtin::sprintf(mr, mkStr(mr, "%d/%s"),
                                           Span<const Value>(args, 2));
    ASSERT_TRUE(r.isChar());
    EXPECT_EQ(r.toString(), "7/foo");
}

TEST(BuiltinFormatPublicApi, SprintfNonCharFmtReturnsEmpty)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    Value r = numkit::builtin::sprintf(mr, Value::scalar(1.0, mr),
                                           Span<const Value>{});
    ASSERT_TRUE(r.isChar());
    EXPECT_EQ(r.toString(), "");
}
