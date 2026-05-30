// libs/builtin/tests/format_public_api_test.cpp
//
// Direct-call tests for numkit::builtin formatting primitives.

#include <numkit/builtin/language/strings/format.hpp>

#include <memory_resource>
#include <numkit/core/span.hpp>
#include <numkit/core/value.hpp>

#include <gtest/gtest.h>

using numkit::ValueType;
using numkit::Value;
using numkit::Span;

namespace {

Value mkStr(std::pmr::memory_resource *mr, const char *s) { return Value::fromString(s, mr); }
Value mkStrScalar(std::pmr::memory_resource *mr, const char *s) { return Value::stringScalar(s, mr); }

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
    std::string out = numkit::builtin::formatCyclic("%d ", Span<const Value>(args, 1), 0, mr);
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
    std::string out = numkit::builtin::formatCyclic("%d %d\\n", Span<const Value>(args, 1), 0, mr);
    EXPECT_EQ(out, "1 2\n3 4\n");
}

TEST(BuiltinFormatPublicApi, FormatCyclicEmptyArgsJustPrintsFmt)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    std::string out = numkit::builtin::formatCyclic("just text\\n", Span<const Value>{}, 0, mr);
    EXPECT_EQ(out, "just text\n");
}

// ── sprintf (Value wrapper) ────────────────────────────────────────────
TEST(BuiltinFormatPublicApi, SprintfReturnsCharArray)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    Value args[] = {Value::scalar(7.0, mr), mkStr(mr, "foo")};
    Value r = numkit::builtin::sprintf(mkStr(mr, "%d/%s"), Span<const Value>(args, 2), mr);
    ASSERT_TRUE(r.isChar());
    EXPECT_EQ(r.toString(), "7/foo");
}

TEST(BuiltinFormatPublicApi, SprintfNonCharFmtReturnsEmpty)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    Value r = numkit::builtin::sprintf(Value::scalar(1.0, mr), Span<const Value>{}, mr);
    ASSERT_TRUE(r.isChar());
    EXPECT_EQ(r.toString(), "");
}

// ── %s with the string type (regression) ───────────────────────────────
// Bug: %s only accepted char arrays, so a string scalar printed nothing
// (e.g. fprintf('%s', "hi") -> empty). MATLAB %s accepts string scalars
// and cycles the format over the elements of a string array.
TEST(BuiltinFormatPublicApi, FormatOnceStringScalar)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    Value args[] = {mkStrScalar(mr, "hello")};
    std::string out = numkit::builtin::formatOnce("[%s]", Span<const Value>(args, 1));
    EXPECT_EQ(out, "[hello]");
}

TEST(BuiltinFormatPublicApi, SprintfStringScalarArg)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    Value args[] = {Value::scalar(7.0, mr), mkStrScalar(mr, "foo")};
    Value r = numkit::builtin::sprintf(mkStr(mr, "%d/%s"), Span<const Value>(args, 2), mr);
    EXPECT_EQ(r.toString(), "7/foo");
}

TEST(BuiltinFormatPublicApi, FormatCyclicStringArrayCycles)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    auto arr = Value::stringArray(1, 2, mr);
    arr.stringElemSet(0, "ab");
    arr.stringElemSet(1, "cd");
    Value args[] = {arr};
    std::string out =
        numkit::builtin::formatCyclic("%s-", Span<const Value>(args, 1), 0, mr);
    EXPECT_EQ(out, "ab-cd-");
}

// ── %s width / precision (regression) ───────────────────────────────────
// Bug: %s ignored its width and precision spec (printed the raw string).
// MATLAB honours them: %5s right-justifies, %-5s left-justifies, %.Ns caps
// the character count.
TEST(BuiltinFormatPublicApi, FormatOnceStringWidth)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    Value args[] = {mkStr(mr, "hi")};
    EXPECT_EQ(numkit::builtin::formatOnce("[%5s]", Span<const Value>(args, 1)),
              "[   hi]");
    EXPECT_EQ(numkit::builtin::formatOnce("[%-5s]", Span<const Value>(args, 1)),
              "[hi   ]");
}

TEST(BuiltinFormatPublicApi, FormatOnceStringPrecision)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    Value a[] = {mkStr(mr, "hi")};
    EXPECT_EQ(numkit::builtin::formatOnce("[%.1s]", Span<const Value>(a, 1)),
              "[h]");
    Value b[] = {mkStr(mr, "hello")};
    EXPECT_EQ(numkit::builtin::formatOnce("[%5.1s]", Span<const Value>(b, 1)),
              "[    h]");
}

TEST(BuiltinFormatPublicApi, FormatOnceStringWidthAppliesToStringType)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    Value args[] = {mkStrScalar(mr, "hi")};
    EXPECT_EQ(numkit::builtin::formatOnce("[%5s]", Span<const Value>(args, 1)),
              "[   hi]");
}
