// libs/builtin/tests/strings_public_api_test.cpp
//
// Direct-call tests for numkit::builtin string functions.
// Exercises algorithm without Engine/Parser/VM.

#include <numkit/builtin/language/strings/strings.hpp>
#include <numkit/builtin/language/strings/regex.hpp>

#include <limits>
#include <memory_resource>
#include <numkit/core/types.hpp>
#include <numkit/core/value.hpp>

#include <gtest/gtest.h>

#include <cmath>

using numkit::Error;
using numkit::ValueType;
using numkit::Value;

namespace {

Value mkStr(std::pmr::memory_resource *mr, const char *s) { return Value::fromString(s, mr); }

} // namespace

// ── num2str / str2num / str2double ───────────────────────────────────────
TEST(BuiltinStringsPublicApi, Num2StrScalar)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    Value r = numkit::builtin::num2str(Value::scalar(3.14, mr), mr);
    ASSERT_TRUE(r.isChar());
    EXPECT_EQ(r.toString(), "3.14");
}

// MATLAB num2str default precision is magnitude-aware (~4 digits after the
// integer part), NOT a fixed 5 sig figs. num2str(1000000)="1000000" (numkit
// previously gave "1e+06" from a fixed "%.5g"). 2026-05-29.
TEST(BuiltinStringsPublicApi, Num2StrMagnitudeAware)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    auto n2s = [&](double v) {
        return numkit::builtin::num2str(Value::scalar(v, mr), mr).toString();
    };
    EXPECT_EQ(n2s(1000000.0),   "1000000");
    EXPECT_EQ(n2s(1000000.5),   "1000000.5");
    EXPECT_EQ(n2s(123456789.0), "123456789");
    EXPECT_EQ(n2s(12345.678),   "12345.678");
    EXPECT_EQ(n2s(3.14159265),  "3.1416");      // small magnitude -> 5 sig figs
    EXPECT_EQ(n2s(-42.0),       "-42");
    EXPECT_EQ(n2s(0.0),         "0");
    EXPECT_EQ(n2s(0.00012345),  "0.00012345");
    EXPECT_EQ(n2s(std::numeric_limits<double>::infinity()), "Inf");
    EXPECT_EQ(n2s(-std::numeric_limits<double>::infinity()), "-Inf");
    EXPECT_EQ(n2s(std::nan("")), "NaN");
}

// num2str(X, FMT): routes through the sprintf engine so integer conversions
// work, and strips leading/trailing blanks (keeping leading zeros + internal
// spacing). vs MATLAB R2025b. Bug fixed 2026-05-30: a raw snprintf(fmt,double)
// printed "00000"/"0" for %d specs and never trimmed the width padding.
TEST(BuiltinStringsPublicApi, Num2StrFormatString)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    auto fmt = [&](double v, const char *f) {
        return numkit::builtin::num2str(Value::scalar(v, mr), std::string(f), mr)
            .toString();
    };
    EXPECT_EQ(fmt(3.14159265, "%8.4f"),        "3.1416");   // leading pad trimmed
    EXPECT_EQ(fmt(-3.14159265, "%10.4f"),      "-3.1416");
    EXPECT_EQ(fmt(5.0, "%05d"),                "00005");    // leading zeros kept
    EXPECT_EQ(fmt(42.0, "%8d"),                "42");       // %d not garbage/0
    EXPECT_EQ(fmt(3.14159265, "%.4f"),         "3.1416");
    EXPECT_EQ(fmt(5.0, "%-8d"),                "5");        // trailing pad trimmed
    EXPECT_EQ(fmt(3.14159265, "   value=%6.2f"), "value=  3.14"); // internal kept
    EXPECT_EQ(fmt(1000.0, "%e"),               "1.000000e+03");
}

// int2str: round half away from zero, render as a plain integer (no decimals
// or scientific notation); Inf/-Inf/NaN pass through. vs MATLAB R2025b.
// Implemented 2026-05-30 (was an undefined function).
TEST(BuiltinStringsPublicApi, Int2StrScalar)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    auto i2s = [&](double v) {
        return numkit::builtin::int2str(Value::scalar(v, mr), mr).toString();
    };
    EXPECT_EQ(i2s(3.4),     "3");
    EXPECT_EQ(i2s(2.5),     "3");          // round half away from zero
    EXPECT_EQ(i2s(-2.5),    "-3");
    EXPECT_EQ(i2s(0.5),     "1");
    EXPECT_EQ(i2s(-0.5),    "-1");
    EXPECT_EQ(i2s(100000.0), "100000");
    EXPECT_EQ(i2s(1e10),    "10000000000"); // full integer, not scientific
    EXPECT_EQ(i2s(-0.0),    "0");           // -0 normalised
    EXPECT_EQ(i2s(std::numeric_limits<double>::infinity()),  "Inf");
    EXPECT_EQ(i2s(-std::numeric_limits<double>::infinity()), "-Inf");
    EXPECT_EQ(i2s(std::nan("")), "NaN");
}

TEST(BuiltinStringsPublicApi, Str2NumSuccess)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    Value r = numkit::builtin::str2num(mkStr(mr, "42.5"), mr);
    EXPECT_DOUBLE_EQ(r.toScalar(), 42.5);
}

TEST(BuiltinStringsPublicApi, Str2NumFailureReturnsEmpty)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    Value r = numkit::builtin::str2num(mkStr(mr, "not a number"), mr);
    EXPECT_TRUE(r.isEmpty());
}

TEST(BuiltinStringsPublicApi, Str2DoubleSuccess)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    Value r = numkit::builtin::str2double(mkStr(mr, "3.14e2"), mr);
    EXPECT_DOUBLE_EQ(r.toScalar(), 314.0);
}

TEST(BuiltinStringsPublicApi, Str2DoubleFailureReturnsNaN)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    Value r = numkit::builtin::str2double(mkStr(mr, "xyz"), mr);
    EXPECT_TRUE(std::isnan(r.toScalar()));
}

// MATLAB str2double strips ALL commas (thousands separators) and requires the
// ENTIRE trimmed token to parse: '1,234'->1234, '1,2,3'->123, but '42abc' /
// '42 7' / ',' -> NaN (std::stod used to parse just a numeric prefix). 2026-05-29.
TEST(BuiltinStringsPublicApi, Str2DoubleCommasAndConsumeAll)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    auto s2d = [&](const char *s) {
        return numkit::builtin::str2double(mkStr(mr, s), mr).toScalar();
    };
    EXPECT_DOUBLE_EQ(s2d("1,234"),     1234.0);
    EXPECT_DOUBLE_EQ(s2d("1,2,3"),     123.0);
    EXPECT_DOUBLE_EQ(s2d("1,000,000"), 1000000.0);
    EXPECT_DOUBLE_EQ(s2d("1,234.5"),   1234.5);
    EXPECT_DOUBLE_EQ(s2d("  42  "),    42.0);
    EXPECT_TRUE(std::isnan(s2d("42abc")));
    EXPECT_TRUE(std::isnan(s2d("42 7")));
    EXPECT_TRUE(std::isnan(s2d(",")));
}

// ── toString / toChar ────────────────────────────────────────────────────
TEST(BuiltinStringsPublicApi, ToStringFromScalar)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    Value r = numkit::builtin::toString(Value::scalar(7.5, mr), mr);
    ASSERT_TRUE(r.isString());
    EXPECT_EQ(r.toString(), "7.5");
}

TEST(BuiltinStringsPublicApi, ToCharFromAsciiCodes)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    auto v = Value::matrix(1, 3, ValueType::DOUBLE, mr);
    double *d = v.doubleDataMut();
    d[0] = 72; d[1] = 105; d[2] = 33; // "Hi!"
    Value r = numkit::builtin::toChar(v, mr);
    ASSERT_TRUE(r.isChar());
    EXPECT_EQ(r.toString(), "Hi!");
}

// ── strcmp / strcmpi ─────────────────────────────────────────────────────
TEST(BuiltinStringsPublicApi, StrcmpExact)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    EXPECT_TRUE(numkit::builtin::strcmp(mkStr(mr, "abc"), mkStr(mr, "abc"), mr)
                    .toBool());
    EXPECT_FALSE(numkit::builtin::strcmp(mkStr(mr, "abc"), mkStr(mr, "ABC"), mr)
                     .toBool());
}

TEST(BuiltinStringsPublicApi, StrcmpiCaseInsensitive)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    EXPECT_TRUE(numkit::builtin::strcmpi(mkStr(mr, "Hello"), mkStr(mr, "hELLO"), mr)
                    .toBool());
}

// ── upper / lower ────────────────────────────────────────────────────────
TEST(BuiltinStringsPublicApi, UpperAscii)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    Value r = numkit::builtin::upper(mkStr(mr, "Mixed Case"), mr);
    EXPECT_EQ(r.toString(), "MIXED CASE");
}

TEST(BuiltinStringsPublicApi, LowerAscii)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    Value r = numkit::builtin::lower(mkStr(mr, "Mixed Case"), mr);
    EXPECT_EQ(r.toString(), "mixed case");
}

// ── strtrim ──────────────────────────────────────────────────────────────
TEST(BuiltinStringsPublicApi, StrtrimStripsWhitespace)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    Value r = numkit::builtin::strtrim(mkStr(mr, "  \t hello\n "), mr);
    EXPECT_EQ(r.toString(), "hello");
}

TEST(BuiltinStringsPublicApi, StrtrimAllWhitespaceReturnsEmpty)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    Value r = numkit::builtin::strtrim(mkStr(mr, "   \n\t"), mr);
    EXPECT_EQ(r.toString(), "");
}

// ── strsplit ─────────────────────────────────────────────────────────────
TEST(BuiltinStringsPublicApi, StrsplitDefaultDelimIsSpace)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    Value r = numkit::builtin::strsplit(mkStr(mr, "one two three"), mr);
    ASSERT_EQ(r.numel(), 3u);
    EXPECT_EQ(r.cellAt(0).toString(), "one");
    EXPECT_EQ(r.cellAt(1).toString(), "two");
    EXPECT_EQ(r.cellAt(2).toString(), "three");
}

TEST(BuiltinStringsPublicApi, StrsplitCustomDelim)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    Value r = numkit::builtin::strsplit(mkStr(mr, "a,b,c"), mkStr(mr, ","), mr);
    ASSERT_EQ(r.numel(), 3u);
    EXPECT_EQ(r.cellAt(0).toString(), "a");
    EXPECT_EQ(r.cellAt(2).toString(), "c");
}

// ── strcat ───────────────────────────────────────────────────────────────
TEST(BuiltinStringsPublicApi, StrcatConcatenatesAll)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    Value a = mkStr(mr, "foo");
    Value b = mkStr(mr, "bar");
    Value c = mkStr(mr, "baz");
    Value parts[] = {a, b, c};
    numkit::Span<const Value> span(parts, 3);
    Value r = numkit::builtin::strcat(span, mr);
    EXPECT_EQ(r.toString(), "foobarbaz");
}

// ── strlength ────────────────────────────────────────────────────────────
TEST(BuiltinStringsPublicApi, StrlengthOfCharArray)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    Value r = numkit::builtin::strlength(mkStr(mr, "hello"), mr);
    EXPECT_DOUBLE_EQ(r.toScalar(), 5.0);
}

// ── strrep ───────────────────────────────────────────────────────────────
TEST(BuiltinStringsPublicApi, StrrepReplacesAllOccurrences)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    Value r = numkit::builtin::strrep(mkStr(mr, "foo bar foo baz foo"), mkStr(mr, "foo"), mkStr(mr, "XYZ"), mr);
    EXPECT_EQ(r.toString(), "XYZ bar XYZ baz XYZ");
}

TEST(BuiltinStringsPublicApi, StrrepEmptyOldPatIsPassThrough)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    Value r = numkit::builtin::strrep(mkStr(mr, "abc"), mkStr(mr, ""), mkStr(mr, "X"), mr);
    EXPECT_EQ(r.toString(), "abc");
}

// ── contains / startsWith / endsWith ─────────────────────────────────────
TEST(BuiltinStringsPublicApi, ContainsPositive)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    EXPECT_TRUE(numkit::builtin::contains(mkStr(mr, "hello world"), mkStr(mr, "lo wo"), mr)
                    .toBool());
}

TEST(BuiltinStringsPublicApi, ContainsNegative)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    EXPECT_FALSE(numkit::builtin::contains(mkStr(mr, "hello"), mkStr(mr, "xyz"), mr)
                     .toBool());
}

TEST(BuiltinStringsPublicApi, StartsWithTrueFalse)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    EXPECT_TRUE(numkit::builtin::startsWith(mkStr(mr, "hello"), mkStr(mr, "hel"), mr)
                    .toBool());
    EXPECT_FALSE(numkit::builtin::startsWith(mkStr(mr, "hello"), mkStr(mr, "world"), mr)
                     .toBool());
}

TEST(BuiltinStringsPublicApi, EndsWithTrueFalse)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    EXPECT_TRUE(numkit::builtin::endsWith(mkStr(mr, "hello.txt"), mkStr(mr, ".txt"), mr)
                    .toBool());
    EXPECT_FALSE(numkit::builtin::endsWith(mkStr(mr, "hello"), mkStr(mr, ".txt"), mr)
                     .toBool());
}

// ── Pack 36: newline / strings ────────────────────────────────────────
TEST(BuiltinStringsPublicApi, NewlineIsLfChar)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    Value r = numkit::builtin::newlineFn(mr);
    ASSERT_TRUE(r.isChar());
    EXPECT_EQ(r.numel(), 1u);
    EXPECT_EQ(r.toString(), std::string("\n"));
}

TEST(BuiltinStringsPublicApi, StringsZeroArgIsEmptyScalar)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    Value r = numkit::builtin::stringsND({}, mr);
    EXPECT_EQ(r.dims().rows(), 1u);
    EXPECT_EQ(r.dims().cols(), 1u);
    EXPECT_EQ(r.numel(), 1u);
    EXPECT_EQ(r.stringElem(0), "");
}

TEST(BuiltinStringsPublicApi, StringsNxNFromSingleArg)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    size_t d[] = {4};
    Value r = numkit::builtin::stringsND(numkit::Span<const size_t>(d, 1), mr);
    EXPECT_EQ(r.dims().rows(), 4u);
    EXPECT_EQ(r.dims().cols(), 4u);
    EXPECT_EQ(r.numel(), 16u);
    for (size_t i = 0; i < r.numel(); ++i)
        EXPECT_EQ(r.stringElem(i), "");
}

TEST(BuiltinStringsPublicApi, StringsMxN)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    size_t d[] = {2, 3};
    Value r = numkit::builtin::stringsND(numkit::Span<const size_t>(d, 2), mr);
    EXPECT_EQ(r.dims().rows(), 2u);
    EXPECT_EQ(r.dims().cols(), 3u);
    EXPECT_EQ(r.numel(), 6u);
}

TEST(BuiltinStringsPublicApi, Strings3D)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    size_t d[] = {2, 3, 4};
    Value r = numkit::builtin::stringsND(numkit::Span<const size_t>(d, 3), mr);
    EXPECT_EQ(r.dims().rows(), 2u);
    EXPECT_EQ(r.dims().cols(), 3u);
    EXPECT_EQ(r.numel(), 24u);
}

// ── Pack 36: compose / strjust / extract / split / join ──────────────
TEST(BuiltinStringsPublicApi, ComposeBroadcastsOverArray)
{
    auto *mr = std::pmr::get_default_resource();
    auto a = Value::matrix(1, 3, numkit::ValueType::DOUBLE, mr);
    a.doubleDataMut()[0] = 1; a.doubleDataMut()[1] = 2; a.doubleDataMut()[2] = 3;
    auto fmt = mkStr(mr, "v%d");
    Value c = numkit::builtin::compose(fmt, a, mr);
    ASSERT_TRUE(c.isCell());
    EXPECT_EQ(c.dims().rows(), 1u);
    EXPECT_EQ(c.dims().cols(), 3u);
    EXPECT_EQ(c.cellAt(0).toString(), "v1");
    EXPECT_EQ(c.cellAt(1).toString(), "v2");
    EXPECT_EQ(c.cellAt(2).toString(), "v3");
}

TEST(BuiltinStringsPublicApi, ComposeScalar)
{
    auto *mr = std::pmr::get_default_resource();
    Value c = numkit::builtin::compose(mkStr(mr, "x=%g"), Value::scalar(2.5, mr), mr);
    ASSERT_TRUE(c.isCell());
    EXPECT_EQ(c.numel(), 1u);
    EXPECT_EQ(c.cellAt(0).toString(), "x=2.5");
}

TEST(BuiltinStringsPublicApi, StrjustRight)
{
    auto *mr = std::pmr::get_default_resource();
    // "ab  " over "c   " over " de "  →  "  ab" / "   c" / "  de"
    auto m = Value::matrix(3, 4, numkit::ValueType::CHAR, mr);
    const char *src = "abc     ddee   ";  // dummy, fill below in column-major
    (void)src;
    char *p = m.charDataMut();
    // Column-major. Row r, col c → p[c*rows + r].
    // r0 = "ab  ", r1 = "c   ", r2 = " de "
    auto put = [&](size_t r, size_t c, char ch) { p[c * 3 + r] = ch; };
    put(0,0,'a'); put(0,1,'b'); put(0,2,' '); put(0,3,' ');
    put(1,0,'c'); put(1,1,' '); put(1,2,' '); put(1,3,' ');
    put(2,0,' '); put(2,1,'d'); put(2,2,'e'); put(2,3,' ');
    Value r = numkit::builtin::strjust(m, "right", mr);
    EXPECT_EQ(r.dims().rows(), 3u);
    EXPECT_EQ(r.dims().cols(), 4u);
    const char *q = r.charData();
    auto at = [&](size_t row, size_t col) { return q[col * 3 + row]; };
    EXPECT_EQ(at(0,0), ' '); EXPECT_EQ(at(0,1), ' '); EXPECT_EQ(at(0,2), 'a'); EXPECT_EQ(at(0,3), 'b');
    EXPECT_EQ(at(1,0), ' '); EXPECT_EQ(at(1,1), ' '); EXPECT_EQ(at(1,2), ' '); EXPECT_EQ(at(1,3), 'c');
    EXPECT_EQ(at(2,0), ' '); EXPECT_EQ(at(2,1), ' '); EXPECT_EQ(at(2,2), 'd'); EXPECT_EQ(at(2,3), 'e');
}

TEST(BuiltinStringsPublicApi, ExtractMultipleMatches)
{
    auto *mr = std::pmr::get_default_resource();
    Value c = numkit::builtin::extract(mkStr(mr, "hello hello world"), mkStr(mr, "hello"), mr);
    ASSERT_TRUE(c.isCell());
    EXPECT_EQ(c.dims().rows(), 2u);
    EXPECT_EQ(c.dims().cols(), 1u);
    EXPECT_EQ(c.cellAt(0).toString(), "hello");
    EXPECT_EQ(c.cellAt(1).toString(), "hello");
}

TEST(BuiltinStringsPublicApi, ExtractNoMatch)
{
    auto *mr = std::pmr::get_default_resource();
    Value c = numkit::builtin::extract(mkStr(mr, "hi"), mkStr(mr, "world"), mr);
    ASSERT_TRUE(c.isCell());
    EXPECT_EQ(c.numel(), 0u);
}

TEST(BuiltinStringsPublicApi, SplitKeepsEmptyTokens)
{
    auto *mr = std::pmr::get_default_resource();
    Value c = numkit::builtin::split(mkStr(mr, "a,b,,c"), mkStr(mr, ","), mr);
    ASSERT_TRUE(c.isCell());
    EXPECT_EQ(c.dims().rows(), 4u);
    EXPECT_EQ(c.dims().cols(), 1u);
    EXPECT_EQ(c.cellAt(0).toString(), "a");
    EXPECT_EQ(c.cellAt(1).toString(), "b");
    EXPECT_EQ(c.cellAt(2).toString(), "");
    EXPECT_EQ(c.cellAt(3).toString(), "c");
}

TEST(BuiltinStringsPublicApi, JoinStringArray)
{
    auto *mr = std::pmr::get_default_resource();
    Value arr = Value::stringArray(1, 3, mr);
    arr.stringElemSet(0, "a"); arr.stringElemSet(1, "b"); arr.stringElemSet(2, "c");
    Value d = mkStr(mr, "-");
    Value j = numkit::builtin::join(arr, d, mr);
    EXPECT_TRUE(j.isString());
    EXPECT_EQ(j.numel(), 1u);
    EXPECT_EQ(j.stringElem(0), "a-b-c");
}

TEST(BuiltinStringsPublicApi, JoinDefaultDelimIsSpace)
{
    auto *mr = std::pmr::get_default_resource();
    Value arr = Value::stringArray(1, 3, mr);
    arr.stringElemSet(0, "x"); arr.stringElemSet(1, "y"); arr.stringElemSet(2, "z");
    Value j = numkit::builtin::join(arr, Value::Empty, mr);
    EXPECT_EQ(j.stringElem(0), "x y z");
}
