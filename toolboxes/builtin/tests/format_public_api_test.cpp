// toolboxes/builtin/tests/format_public_api_test.cpp
//
// Direct-call tests for numkit::builtin formatting primitives.

#include <numkit/builtin/language/strings/format.hpp>

#include <memory_resource>
#include <numkit/value/span.hpp>
#include <numkit/value/value.hpp>

#include <cmath>
#include <limits>

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
    EXPECT_EQ(numkit::lang::countFormatSpecs("%d %s"), 2u);
    EXPECT_EQ(numkit::lang::countFormatSpecs("no specs here"), 0u);
    EXPECT_EQ(numkit::lang::countFormatSpecs(""), 0u);
}

TEST(BuiltinFormatPublicApi, CountFormatSpecsIgnoresEscapedPercent)
{
    EXPECT_EQ(numkit::lang::countFormatSpecs("100%% done: %d"), 1u);
}

TEST(BuiltinFormatPublicApi, CountFormatSpecsWithFlagsWidthPrecision)
{
    EXPECT_EQ(numkit::lang::countFormatSpecs("%-5.2f %+03d"), 2u);
}

// ── formatOnce ──────────────────────────────────────────────────────────
TEST(BuiltinFormatPublicApi, FormatOnceIntegerAndString)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    Value args[] = {Value::scalar(42.0, mr), mkStr(mr, "hello")};
    std::string out = numkit::lang::formatOnce("%d - %s!", Span<const Value>(args, 2));
    EXPECT_EQ(out, "42 - hello!");
}

TEST(BuiltinFormatPublicApi, FormatOnceFloatPrecision)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    Value args[] = {Value::scalar(3.14159, mr)};
    std::string out = numkit::lang::formatOnce("%.2f", Span<const Value>(args, 1));
    EXPECT_EQ(out, "3.14");
}

TEST(BuiltinFormatPublicApi, FormatOnceEscapes)
{
    std::string out = numkit::lang::formatOnce(
        "line1\\nline2\\t\\\\", Span<const Value>{});
    EXPECT_EQ(out, "line1\nline2\t\\");
}

TEST(BuiltinFormatPublicApi, FormatOncePercentPercent)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    Value args[] = {Value::scalar(50.0, mr)};
    std::string out = numkit::lang::formatOnce("%d%%", Span<const Value>(args, 1));
    EXPECT_EQ(out, "50%");
}

TEST(BuiltinFormatPublicApi, FormatOnceHexAndOctal)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    Value args[] = {Value::scalar(255.0, mr), Value::scalar(8.0, mr)};
    std::string out =
        numkit::lang::formatOnce("%x %o", Span<const Value>(args, 2));
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
    std::string out = numkit::lang::formatCyclic("%d ", Span<const Value>(args, 1), 0, mr);
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
    std::string out = numkit::lang::formatCyclic("%d %d\\n", Span<const Value>(args, 1), 0, mr);
    EXPECT_EQ(out, "1 2\n3 4\n");
}

TEST(BuiltinFormatPublicApi, FormatCyclicEmptyArgsJustPrintsFmt)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    std::string out = numkit::lang::formatCyclic("just text\\n", Span<const Value>{}, 0, mr);
    EXPECT_EQ(out, "just text\n");
}

// ── sprintf (Value wrapper) ────────────────────────────────────────────
TEST(BuiltinFormatPublicApi, SprintfReturnsCharArray)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    Value args[] = {Value::scalar(7.0, mr), mkStr(mr, "foo")};
    Value r = numkit::lang::sprintf(mkStr(mr, "%d/%s"), Span<const Value>(args, 2), mr);
    ASSERT_TRUE(r.isChar());
    EXPECT_EQ(r.toString(), "7/foo");
}

TEST(BuiltinFormatPublicApi, SprintfNonCharFmtReturnsEmpty)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    Value r = numkit::lang::sprintf(Value::scalar(1.0, mr), Span<const Value>{}, mr);
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
    std::string out = numkit::lang::formatOnce("[%s]", Span<const Value>(args, 1));
    EXPECT_EQ(out, "[hello]");
}

TEST(BuiltinFormatPublicApi, SprintfStringScalarArg)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    Value args[] = {Value::scalar(7.0, mr), mkStrScalar(mr, "foo")};
    Value r = numkit::lang::sprintf(mkStr(mr, "%d/%s"), Span<const Value>(args, 2), mr);
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
        numkit::lang::formatCyclic("%s-", Span<const Value>(args, 1), 0, mr);
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
    EXPECT_EQ(numkit::lang::formatOnce("[%5s]", Span<const Value>(args, 1)),
              "[   hi]");
    EXPECT_EQ(numkit::lang::formatOnce("[%-5s]", Span<const Value>(args, 1)),
              "[hi   ]");
}

TEST(BuiltinFormatPublicApi, FormatOnceStringPrecision)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    Value a[] = {mkStr(mr, "hi")};
    EXPECT_EQ(numkit::lang::formatOnce("[%.1s]", Span<const Value>(a, 1)),
              "[h]");
    Value b[] = {mkStr(mr, "hello")};
    EXPECT_EQ(numkit::lang::formatOnce("[%5.1s]", Span<const Value>(b, 1)),
              "[    h]");
}

TEST(BuiltinFormatPublicApi, FormatOnceStringWidthAppliesToStringType)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    Value args[] = {mkStrScalar(mr, "hi")};
    EXPECT_EQ(numkit::lang::formatOnce("[%5s]", Span<const Value>(args, 1)),
              "[   hi]");
}

// ── %d on a non-integer falls back to %e (MATLAB semantics) ──────────────
TEST(BuiltinFormatPublicApi, IntegerConvNonIntegerFallsBackToE)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    Value a[] = {Value::scalar(3.7, mr)};
    EXPECT_EQ(numkit::lang::formatOnce("%d", Span<const Value>(a, 1)),
              "3.700000e+00");
    EXPECT_EQ(numkit::lang::formatOnce("%i", Span<const Value>(a, 1)),
              "3.700000e+00");
    EXPECT_EQ(numkit::lang::formatOnce("%x", Span<const Value>(a, 1)),
              "3.700000e+00");
    // precision carries to the e format
    EXPECT_EQ(numkit::lang::formatOnce("%.2d", Span<const Value>(a, 1)),
              "3.70e+00");
    Value neg[] = {Value::scalar(-2.5, mr)};
    EXPECT_EQ(numkit::lang::formatOnce("%d", Span<const Value>(neg, 1)),
              "-2.500000e+00");
}

TEST(BuiltinFormatPublicApi, IntegerConvWholeNumberStaysInteger)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    Value a[] = {Value::scalar(5.0, mr)};
    EXPECT_EQ(numkit::lang::formatOnce("%d", Span<const Value>(a, 1)), "5");
    Value big[] = {Value::scalar(1e10, mr)};
    EXPECT_EQ(numkit::lang::formatOnce("%d", Span<const Value>(big, 1)),
              "10000000000");
    Value hex[] = {Value::scalar(255.0, mr)};
    EXPECT_EQ(numkit::lang::formatOnce("%x", Span<const Value>(hex, 1)), "ff");
}

TEST(BuiltinFormatPublicApi, IntegerConvInfNan)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    Value inf[] = {Value::scalar(std::numeric_limits<double>::infinity(), mr)};
    EXPECT_EQ(numkit::lang::formatOnce("%d", Span<const Value>(inf, 1)), "Inf");
    Value ninf[] = {Value::scalar(-std::numeric_limits<double>::infinity(), mr)};
    EXPECT_EQ(numkit::lang::formatOnce("%d", Span<const Value>(ninf, 1)), "-Inf");
    Value nan[] = {Value::scalar(std::numeric_limits<double>::quiet_NaN(), mr)};
    EXPECT_EQ(numkit::lang::formatOnce("%d", Span<const Value>(nan, 1)), "NaN");
}

// ── %f / %e / %g of Inf / NaN print capitalised (MATLAB), not lowercase ──
TEST(BuiltinFormatPublicApi, FloatConvInfNanCapitalised)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    const double inf = std::numeric_limits<double>::infinity();
    const double nan = std::numeric_limits<double>::quiet_NaN();
    Value vi[] = {Value::scalar(inf, mr)};
    Value vn[] = {Value::scalar(nan, mr)};
    Value vni[] = {Value::scalar(-inf, mr)};
    EXPECT_EQ(numkit::lang::formatOnce("%f", Span<const Value>(vi, 1)), "Inf");
    EXPECT_EQ(numkit::lang::formatOnce("%g", Span<const Value>(vi, 1)), "Inf");
    EXPECT_EQ(numkit::lang::formatOnce("%e", Span<const Value>(vni, 1)), "-Inf");
    EXPECT_EQ(numkit::lang::formatOnce("%f", Span<const Value>(vn, 1)), "NaN");
    // width honoured, precision ignored
    EXPECT_EQ(numkit::lang::formatOnce("%8.2f", Span<const Value>(vi, 1)), "     Inf");
    EXPECT_EQ(numkit::lang::formatOnce("%-8f", Span<const Value>(vi, 1)), "Inf     ");
    // '+' flag applies to +Inf but not NaN
    EXPECT_EQ(numkit::lang::formatOnce("%+f", Span<const Value>(vi, 1)), "+Inf");
    EXPECT_EQ(numkit::lang::formatOnce("%+f", Span<const Value>(vn, 1)), "NaN");
}

TEST(BuiltinFormatPublicApi, FloatConvFiniteUnchanged)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    Value v[] = {Value::scalar(3.14159, mr)};
    EXPECT_EQ(numkit::lang::formatOnce("%.2f", Span<const Value>(v, 1)), "3.14");
}

// ── '*' field width / precision from the argument list (MATLAB) ──────────
// DEEP-PROBE 2026-05-31: numkit parsed past '*' but never consumed an arg
// for it, so snprintf("%*d", v) read the value as the width and printed a
// garbage pointer. Now each '*' consumes a numeric arg (width then
// precision) before the conversion's value arg. vs MATLAB R2025b.
TEST(BuiltinFormatPublicApi, FormatOnceStarWidthAndPrecision)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    Value wd[] = {Value::scalar(5.0, mr), Value::scalar(42.0, mr)};
    EXPECT_EQ(numkit::lang::formatOnce("%*d", Span<const Value>(wd, 2)), "   42");
    Value lj[] = {Value::scalar(5.0, mr), Value::scalar(42.0, mr)};
    EXPECT_EQ(numkit::lang::formatOnce("%-*d", Span<const Value>(lj, 2)), "42   ");
    Value pf[] = {Value::scalar(3.0, mr), Value::scalar(3.14159, mr)};
    EXPECT_EQ(numkit::lang::formatOnce("%.*f", Span<const Value>(pf, 2)), "3.142");
    Value wp[] = {Value::scalar(8.0, mr), Value::scalar(2.0, mr), Value::scalar(3.14159, mr)};
    EXPECT_EQ(numkit::lang::formatOnce("%*.*f", Span<const Value>(wp, 3)), "    3.14");
}

// '*' must count as a consumed arg so cyclic chunking lines up.
TEST(BuiltinFormatPublicApi, CountFormatSpecsStarCountsAsArg)
{
    EXPECT_EQ(numkit::lang::countFormatSpecs("%*d"), 2u);
    EXPECT_EQ(numkit::lang::countFormatSpecs("%.*f"), 2u);
    EXPECT_EQ(numkit::lang::countFormatSpecs("%*.*f"), 3u);
    EXPECT_EQ(numkit::lang::countFormatSpecs("%5.2f"), 1u);   // no star, unchanged
}

// Cyclic recycling consumes width+value pairs per cycle (MATLAB).
TEST(BuiltinFormatPublicApi, FormatCyclicStarWidthRecycles)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    auto arr = Value::matrix(1, 6, ValueType::DOUBLE, mr);
    double *d = arr.doubleDataMut();
    d[0] = 4; d[1] = 1; d[2] = 4; d[3] = 22; d[4] = 4; d[5] = 333;
    Value args[] = {arr};
    std::string out =
        numkit::lang::formatCyclic("[%*d]", Span<const Value>(args, 1), 0, mr);
    EXPECT_EQ(out, "[   1][  22][ 333]");
}
