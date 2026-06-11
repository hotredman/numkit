// toolboxes/builtin/tests/strings_public_api_test.cpp
//
// Direct-call tests for numkit::builtin string functions.
// Exercises algorithm without Engine/Parser/VM.

#include <numkit/lang/strings/strings.hpp>
#include <numkit/lang/strings/regex.hpp>

#include <limits>
#include <memory_resource>
#include <numkit/core/types.hpp>
#include <numkit/value/value.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <complex>

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
    Value r = numkit::lang::num2str(Value::scalar(3.14, mr), mr);
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
        return numkit::lang::num2str(Value::scalar(v, mr), mr).toString();
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
        return numkit::lang::num2str(Value::scalar(v, mr), std::string(f), mr)
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

// num2str(X, FMT) with VECTOR/MATRIX input. Previously threw "Cannot convert
// double to scalar" (only scalar was handled). MATLAB applies the format
// cyclically across each ROW, strips the leading/trailing blank COLUMNS common
// to all rows, and returns an N-row char matrix. vs MATLAB R2025b.
// DEEP-PROBE 2026-05-31.
TEST(BuiltinStringsPublicApi, Num2StrFormatVectorMatrix)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();

    // Row vector -> single char row, common leading 3 spaces trimmed.
    Value row = Value::matrix(1, 3, ValueType::DOUBLE, mr);
    double *rd = row.doubleDataMut();
    rd[0] = 1.5; rd[1] = 2.25; rd[2] = 3.125;
    Value sr = numkit::lang::num2str(row, std::string("%8.3f"), mr);
    ASSERT_TRUE(sr.isChar());
    EXPECT_EQ(sr.toString(), "1.500   2.250   3.125");
    EXPECT_EQ(sr.numel(), 21u);

    // 2x2 matrix -> 2x13 char matrix (col-major).
    Value mtx = Value::matrix(2, 2, ValueType::DOUBLE, mr);
    double *md = mtx.doubleDataMut();
    md[0] = 1.5; md[1] = 3.1; md[2] = 2.25; md[3] = 4.0;  // col-major
    Value sm = numkit::lang::num2str(mtx, std::string("%8.3f"), mr);
    ASSERT_TRUE(sm.isChar());
    EXPECT_EQ(sm.dims().rows(), 2u);
    EXPECT_EQ(sm.dims().cols(), 13u);
    const char *cm = static_cast<const char *>(sm.rawData());
    EXPECT_EQ(cm[0], '1');   // (row0,col0)
    EXPECT_EQ(cm[1], '3');   // (row1,col0)

    // Column vector -> 2x5 char matrix, common leading space trimmed.
    Value col = Value::matrix(2, 1, ValueType::DOUBLE, mr);
    double *cd = col.doubleDataMut();
    cd[0] = 1.5; cd[1] = 22.25;
    Value sc = numkit::lang::num2str(col, std::string("%6.2f"), mr);
    ASSERT_TRUE(sc.isChar());
    EXPECT_EQ(sc.dims().rows(), 2u);
    EXPECT_EQ(sc.dims().cols(), 5u);
    const char *cc = static_cast<const char *>(sc.rawData());
    EXPECT_EQ(cc[0], ' ');   // (row0,col0) -> " 1.50"
    EXPECT_EQ(cc[1], '2');   // (row1,col0) -> "22.25"

    // Empty input -> empty char.
    Value empt = numkit::lang::num2str(Value::matrix(0, 0, ValueType::DOUBLE, mr),
                                          std::string("%8.3f"), mr);
    EXPECT_EQ(empt.numel(), 0u);
}

// num2str of a SCALAR complex value. Previously threw "Cannot convert complex
// with nonzero imaginary part to double scalar". MATLAB formats re±|im|i with
// a common precision from max(|re|,|im|); a zero-imag element prints as a bare
// real. vs MATLAB R2025b. DEEP-PROBE 2026-05-31.
TEST(BuiltinStringsPublicApi, Num2StrComplexScalar)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    auto z = [&](double re, double im) {
        return Value::complexScalar(std::complex<double>(re, im), mr);
    };
    EXPECT_EQ(numkit::lang::num2str(z(3, -1), mr).toString(),  "3-1i");
    EXPECT_EQ(numkit::lang::num2str(z(1, 2), mr).toString(),   "1+2i");
    EXPECT_EQ(numkit::lang::num2str(z(-2, -3), mr).toString(), "-2-3i");
    EXPECT_EQ(numkit::lang::num2str(z(0, 1), mr).toString(),   "0+1i");
    EXPECT_EQ(numkit::lang::num2str(z(5, 0), mr).toString(),   "5");  // zero imag -> bare real
    EXPECT_EQ(numkit::lang::num2str(z(1.23456789, 9.87654321), mr).toString(),
              "1.2346+9.8765i");  // default 5 sig
    // common precision from max magnitude (8 sig), NOT per-part 5.
    EXPECT_EQ(numkit::lang::num2str(z(1234.5, 6.789012), mr).toString(),
              "1234.5+6.789012i");
    EXPECT_EQ(numkit::lang::num2str(z(3.141592653589793, 2.5), 8, mr).toString(),
              "3.1415927+2.5i");
    EXPECT_EQ(numkit::lang::num2str(z(1, 2), 3, mr).toString(), "1+2i");
    EXPECT_EQ(numkit::lang::num2str(z(3.14159, -2.71828), std::string("%.3f"), mr).toString(),
              "3.142-2.718i");
    // complex ARRAY remains a deferred gap -> clear throw, not a crash.
    Value arr = Value::matrix(1, 2, ValueType::COMPLEX, mr);
    arr.complexDataMut()[0] = {1, 2};
    arr.complexDataMut()[1] = {3, -4};
    EXPECT_THROW(numkit::lang::num2str(arr, mr), Error);
}

// num2str(X) / num2str(X,N) with VECTOR/MATRIX input and NO explicit format.
// Previously SCALAR-ONLY (threw "Cannot convert double to scalar"); only the
// FMT form handled arrays. MATLAB synthesises a column format: all-integer
// -> "%<maxChars+2>.0f"; fractional -> "%<P+7>.<P>g", P=max(floor(log10
// maxabs),0)+5; the N form -> "%<N+7>.<N>g". vs MATLAB R2025b. DEEP-PROBE
// 2026-05-31.
TEST(BuiltinStringsPublicApi, Num2StrDefaultVectorMatrix)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    auto row3 = [&](double a, double b, double c) {
        Value v = Value::matrix(1, 3, ValueType::DOUBLE, mr);
        double *d = v.doubleDataMut();
        d[0] = a; d[1] = b; d[2] = c;
        return v;
    };
    auto row2 = [&](double a, double b) {
        Value v = Value::matrix(1, 2, ValueType::DOUBLE, mr);
        double *d = v.doubleDataMut();
        d[0] = a; d[1] = b;
        return v;
    };
    EXPECT_EQ(numkit::lang::num2str(row3(1, 2, 3), mr).toString(), "1  2  3");
    EXPECT_EQ(numkit::lang::num2str(row3(10, 200, 3), mr).toString(),
              "10  200    3");
    EXPECT_EQ(numkit::lang::num2str(row3(1.5, 2.25, 3), mr).toString(),
              "1.5        2.25           3");
    EXPECT_EQ(numkit::lang::num2str(row2(-5, 10), mr).toString(), "-5  10");
    EXPECT_EQ(numkit::lang::num2str(row2(-1.5, 2), mr).toString(),
              "-1.5           2");
    // num2str(X, N): "%<N+7>.<N>g" per element (no integer-detection).
    EXPECT_EQ(numkit::lang::num2str(row3(1, 2, 3), 3, mr).toString(),
              "1         2         3");

    // 2x2 matrix -> 2x4 char matrix; rows "1  2" / "3  4" (col-major).
    Value mtx = Value::matrix(2, 2, ValueType::DOUBLE, mr);
    double *md = mtx.doubleDataMut();
    md[0] = 1; md[1] = 3; md[2] = 2; md[3] = 4;   // col-major
    Value sm = numkit::lang::num2str(mtx, mr);
    ASSERT_TRUE(sm.isChar());
    EXPECT_EQ(sm.dims().rows(), 2u);
    EXPECT_EQ(sm.dims().cols(), 4u);
    const char *cm = static_cast<const char *>(sm.rawData());
    EXPECT_EQ(cm[0], '1');   // (r0,c0)
    EXPECT_EQ(cm[1], '3');   // (r1,c0)
    EXPECT_EQ(cm[6], '2');   // (r0,c3)
    EXPECT_EQ(cm[7], '4');   // (r1,c3)

    // Empty -> empty char.
    EXPECT_EQ(numkit::lang::num2str(Value::matrix(0, 0, ValueType::DOUBLE, mr), mr)
                  .numel(),
              0u);
}

// int2str: round half away from zero, render as a plain integer (no decimals
// or scientific notation); Inf/-Inf/NaN pass through. vs MATLAB R2025b.
// Implemented 2026-05-30 (was an undefined function).
TEST(BuiltinStringsPublicApi, Int2StrScalar)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    auto i2s = [&](double v) {
        return numkit::lang::int2str(Value::scalar(v, mr), mr).toString();
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

    // Complex scalar: MATLAB int2str operates on the REAL part (imag
    // discarded). Previously threw "Cannot convert complex ... to scalar".
    // vs MATLAB R2025b. DEEP-PROBE 2026-05-31.
    auto i2sc = [&](double re, double im) {
        return numkit::lang::int2str(Value::complexScalar(std::complex<double>(re, im), mr), mr)
            .toString();
    };
    EXPECT_EQ(i2sc(3.6, 1.2),   "4");    // round(3.6)=4, imag discarded
    EXPECT_EQ(i2sc(-3.2, -4.8), "-3");   // round(-3.2)=-3
    EXPECT_EQ(i2sc(7.0, 0.0),   "7");    // pure-real complex
}

// int2str on VECTOR/MATRIX. Previously SCALAR-ONLY (threw "only scalar inputs
// are supported"). MATLAB rounds each element half-away-from-zero, formats
// with a fixed integer field W = digits(max|rounded|)+2, and strips common
// leading blank columns. vs MATLAB R2025b. DEEP-PROBE 2026-05-31.
TEST(BuiltinStringsPublicApi, Int2StrVectorMatrix)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    auto rowv = [&](std::initializer_list<double> vs) {
        Value v = Value::matrix(1, vs.size(), ValueType::DOUBLE, mr);
        double *d = v.doubleDataMut();
        size_t i = 0;
        for (double x : vs) d[i++] = x;
        return numkit::lang::int2str(v, mr).toString();
    };
    EXPECT_EQ(rowv({1, 2, 3}),        "1  2  3");
    EXPECT_EQ(rowv({10, 200, 3}),     "10  200    3");
    EXPECT_EQ(rowv({1.5, 2.5, -2.5}), "2  3 -3");   // half-away rounding
    EXPECT_EQ(rowv({-5, 10, 3}),      "-5  10   3");
    EXPECT_EQ(rowv({3.4, -2.6}),      "3 -3");

    // 2x2 matrix -> 2x6 char matrix; rows " 1   2" / "30   4" (col-major).
    Value mtx = Value::matrix(2, 2, ValueType::DOUBLE, mr);
    double *md = mtx.doubleDataMut();
    md[0] = 1; md[1] = 30; md[2] = 2; md[3] = 4;   // col-major
    Value sm = numkit::lang::int2str(mtx, mr);
    ASSERT_TRUE(sm.isChar());
    EXPECT_EQ(sm.dims().rows(), 2u);
    EXPECT_EQ(sm.dims().cols(), 6u);
    const char *cm = static_cast<const char *>(sm.rawData());
    EXPECT_EQ(cm[0], ' ');   // (r0,c0) -> " 1   2"
    EXPECT_EQ(cm[1], '3');   // (r1,c0) -> "30   4"

    // Empty -> empty char.
    EXPECT_EQ(numkit::lang::int2str(Value::matrix(0, 0, ValueType::DOUBLE, mr), mr)
                  .numel(),
              0u);
}

// validatestring: case-insensitive; exact match wins, else a unique prefix,
// else the shortest-prefix-of-all; ambiguous or no-match throws. vs MATLAB
// R2025b. Implemented 2026-05-30 (was an undefined function).
TEST(BuiltinStringsPublicApi, ValidateString)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    auto cell2 = [&](const char *a, const char *b) {
        Value c = Value::cell(1, 2, mr);
        c.cellAt(0) = mkStr(mr, a);
        c.cellAt(1) = mkStr(mr, b);
        return c;
    };
    auto vs = [&](const char *s, const Value &list) {
        return numkit::lang::validatestring(mkStr(mr, s), list, mr).toString();
    };
    EXPECT_EQ(vs("orange", cell2("apple", "orange")), "orange");   // exact
    EXPECT_EQ(vs("app",    cell2("apple", "orange")), "apple");    // prefix
    EXPECT_EQ(vs("APP",    cell2("apple", "orange")), "apple");    // case-insens
    EXPECT_EQ(vs("in",     cell2("in", "input")),     "in");       // exact wins
    EXPECT_EQ(vs("appl",   cell2("apple", "applesauce")), "apple"); // shortest-of-all

    // Ambiguous and no-match both throw.
    EXPECT_THROW(vs("a",   cell2("apple", "apricot")),     numkit::Error);
    EXPECT_THROW(vs("app", cell2("apple", "application")), numkit::Error);
    EXPECT_THROW(vs("xyz", cell2("apple", "orange")),      numkit::Error);
}

TEST(BuiltinStringsPublicApi, Str2NumSuccess)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    Value r = numkit::lang::str2num(mkStr(mr, "42.5"), mr);
    EXPECT_DOUBLE_EQ(r.toScalar(), 42.5);
}

TEST(BuiltinStringsPublicApi, Str2NumFailureReturnsEmpty)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    Value r = numkit::lang::str2num(mkStr(mr, "not a number"), mr);
    EXPECT_TRUE(r.isEmpty());
}

TEST(BuiltinStringsPublicApi, Str2DoubleSuccess)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    Value r = numkit::lang::str2double(mkStr(mr, "3.14e2"), mr);
    EXPECT_DOUBLE_EQ(r.toScalar(), 314.0);
}

TEST(BuiltinStringsPublicApi, Str2DoubleFailureReturnsNaN)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    Value r = numkit::lang::str2double(mkStr(mr, "xyz"), mr);
    EXPECT_TRUE(std::isnan(r.toScalar()));
}

// MATLAB str2double strips ALL commas (thousands separators) and requires the
// ENTIRE trimmed token to parse: '1,234'->1234, '1,2,3'->123, but '42abc' /
// '42 7' / ',' -> NaN (std::stod used to parse just a numeric prefix). 2026-05-29.
TEST(BuiltinStringsPublicApi, Str2DoubleCommasAndConsumeAll)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    auto s2d = [&](const char *s) {
        return numkit::lang::str2double(mkStr(mr, s), mr).toScalar();
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
    Value r = numkit::lang::toString(Value::scalar(7.5, mr), mr);
    ASSERT_TRUE(r.isString());
    EXPECT_EQ(r.toString(), "7.5");
}

TEST(BuiltinStringsPublicApi, ToCharFromAsciiCodes)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    auto v = Value::matrix(1, 3, ValueType::DOUBLE, mr);
    double *d = v.doubleDataMut();
    d[0] = 72; d[1] = 105; d[2] = 33; // "Hi!"
    Value r = numkit::lang::toChar(v, mr);
    ASSERT_TRUE(r.isChar());
    EXPECT_EQ(r.toString(), "Hi!");
}

// ── strcmp / strcmpi ─────────────────────────────────────────────────────
TEST(BuiltinStringsPublicApi, StrcmpExact)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    EXPECT_TRUE(numkit::lang::strcmp(mkStr(mr, "abc"), mkStr(mr, "abc"), mr)
                    .toBool());
    EXPECT_FALSE(numkit::lang::strcmp(mkStr(mr, "abc"), mkStr(mr, "ABC"), mr)
                     .toBool());
}

TEST(BuiltinStringsPublicApi, StrcmpiCaseInsensitive)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    EXPECT_TRUE(numkit::lang::strcmpi(mkStr(mr, "Hello"), mkStr(mr, "hELLO"), mr)
                    .toBool());
}

// ── upper / lower ────────────────────────────────────────────────────────
TEST(BuiltinStringsPublicApi, UpperAscii)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    Value r = numkit::lang::upper(mkStr(mr, "Mixed Case"), mr);
    EXPECT_EQ(r.toString(), "MIXED CASE");
}

TEST(BuiltinStringsPublicApi, LowerAscii)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    Value r = numkit::lang::lower(mkStr(mr, "Mixed Case"), mr);
    EXPECT_EQ(r.toString(), "mixed case");
}

// ── strtrim ──────────────────────────────────────────────────────────────
TEST(BuiltinStringsPublicApi, StrtrimStripsWhitespace)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    Value r = numkit::lang::strtrim(mkStr(mr, "  \t hello\n "), mr);
    EXPECT_EQ(r.toString(), "hello");
}

TEST(BuiltinStringsPublicApi, StrtrimAllWhitespaceReturnsEmpty)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    Value r = numkit::lang::strtrim(mkStr(mr, "   \n\t"), mr);
    EXPECT_EQ(r.toString(), "");
}

// ── strsplit ─────────────────────────────────────────────────────────────
TEST(BuiltinStringsPublicApi, StrsplitDefaultDelimIsSpace)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    Value r = numkit::lang::strsplit(mkStr(mr, "one two three"), mr);
    ASSERT_EQ(r.numel(), 3u);
    EXPECT_EQ(r.cellAt(0).toString(), "one");
    EXPECT_EQ(r.cellAt(1).toString(), "two");
    EXPECT_EQ(r.cellAt(2).toString(), "three");
}

TEST(BuiltinStringsPublicApi, StrsplitCustomDelim)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    Value r = numkit::lang::strsplit(mkStr(mr, "a,b,c"), mkStr(mr, ","), mr);
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
    Value r = numkit::lang::strcat(span, mr);
    EXPECT_EQ(r.toString(), "foobarbaz");
}

// ── strlength ────────────────────────────────────────────────────────────
TEST(BuiltinStringsPublicApi, StrlengthOfCharArray)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    Value r = numkit::lang::strlength(mkStr(mr, "hello"), mr);
    EXPECT_DOUBLE_EQ(r.toScalar(), 5.0);
}

// ── strrep ───────────────────────────────────────────────────────────────
TEST(BuiltinStringsPublicApi, StrrepReplacesAllOccurrences)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    Value r = numkit::lang::strrep(mkStr(mr, "foo bar foo baz foo"), mkStr(mr, "foo"), mkStr(mr, "XYZ"), mr);
    EXPECT_EQ(r.toString(), "XYZ bar XYZ baz XYZ");
}

TEST(BuiltinStringsPublicApi, StrrepEmptyOldPatIsPassThrough)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    Value r = numkit::lang::strrep(mkStr(mr, "abc"), mkStr(mr, ""), mkStr(mr, "X"), mr);
    EXPECT_EQ(r.toString(), "abc");
}

// ── contains / startsWith / endsWith ─────────────────────────────────────
// Signature is (s, pat, bool ignoreCase, mr) — pass ignoreCase=false here.
TEST(BuiltinStringsPublicApi, ContainsPositive)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    EXPECT_TRUE(numkit::lang::contains(mkStr(mr, "hello world"), mkStr(mr, "lo wo"), false, mr)
                    .toBool());
}

TEST(BuiltinStringsPublicApi, ContainsNegative)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    EXPECT_FALSE(numkit::lang::contains(mkStr(mr, "hello"), mkStr(mr, "xyz"), false, mr)
                     .toBool());
}

// 'IgnoreCase' makes the match case-insensitive (MATLAB). DEEP-PROBE c130.
TEST(BuiltinStringsPublicApi, ContainsStartsEndsIgnoreCase)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    EXPECT_TRUE(numkit::lang::contains(mkStr(mr, "HeLLo"), mkStr(mr, "ell"), true, mr).toBool());
    EXPECT_FALSE(numkit::lang::contains(mkStr(mr, "HeLLo"), mkStr(mr, "ell"), false, mr).toBool());
    EXPECT_TRUE(numkit::lang::startsWith(mkStr(mr, "Hello"), mkStr(mr, "he"), true, mr).toBool());
    EXPECT_FALSE(numkit::lang::startsWith(mkStr(mr, "Hello"), mkStr(mr, "he"), false, mr).toBool());
    EXPECT_TRUE(numkit::lang::endsWith(mkStr(mr, "HELLO"), mkStr(mr, "lo"), true, mr).toBool());
    EXPECT_FALSE(numkit::lang::endsWith(mkStr(mr, "HELLO"), mkStr(mr, "lo"), false, mr).toBool());
}

TEST(BuiltinStringsPublicApi, StartsWithTrueFalse)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    EXPECT_TRUE(numkit::lang::startsWith(mkStr(mr, "hello"), mkStr(mr, "hel"), false, mr)
                    .toBool());
    EXPECT_FALSE(numkit::lang::startsWith(mkStr(mr, "hello"), mkStr(mr, "world"), false, mr)
                     .toBool());
}

TEST(BuiltinStringsPublicApi, EndsWithTrueFalse)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    EXPECT_TRUE(numkit::lang::endsWith(mkStr(mr, "hello.txt"), mkStr(mr, ".txt"), false, mr)
                    .toBool());
    EXPECT_FALSE(numkit::lang::endsWith(mkStr(mr, "hello"), mkStr(mr, ".txt"), false, mr)
                     .toBool());
}

// ── Pack 36: newline / strings ────────────────────────────────────────
TEST(BuiltinStringsPublicApi, NewlineIsLfChar)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    Value r = numkit::lang::newlineFn(mr);
    ASSERT_TRUE(r.isChar());
    EXPECT_EQ(r.numel(), 1u);
    EXPECT_EQ(r.toString(), std::string("\n"));
}

TEST(BuiltinStringsPublicApi, StringsZeroArgIsEmptyScalar)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    Value r = numkit::lang::stringsND({}, mr);
    EXPECT_EQ(r.dims().rows(), 1u);
    EXPECT_EQ(r.dims().cols(), 1u);
    EXPECT_EQ(r.numel(), 1u);
    EXPECT_EQ(r.stringElem(0), "");
}

TEST(BuiltinStringsPublicApi, StringsNxNFromSingleArg)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    size_t d[] = {4};
    Value r = numkit::lang::stringsND(numkit::Span<const size_t>(d, 1), mr);
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
    Value r = numkit::lang::stringsND(numkit::Span<const size_t>(d, 2), mr);
    EXPECT_EQ(r.dims().rows(), 2u);
    EXPECT_EQ(r.dims().cols(), 3u);
    EXPECT_EQ(r.numel(), 6u);
}

TEST(BuiltinStringsPublicApi, Strings3D)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    size_t d[] = {2, 3, 4};
    Value r = numkit::lang::stringsND(numkit::Span<const size_t>(d, 3), mr);
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
    Value c = numkit::lang::compose(fmt, a, mr);
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
    Value c = numkit::lang::compose(mkStr(mr, "x=%g"), Value::scalar(2.5, mr), mr);
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
    Value r = numkit::lang::strjust(m, "right", mr);
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
    Value c = numkit::lang::extract(mkStr(mr, "hello hello world"), mkStr(mr, "hello"), mr);
    ASSERT_TRUE(c.isCell());
    EXPECT_EQ(c.dims().rows(), 2u);
    EXPECT_EQ(c.dims().cols(), 1u);
    EXPECT_EQ(c.cellAt(0).toString(), "hello");
    EXPECT_EQ(c.cellAt(1).toString(), "hello");
}

TEST(BuiltinStringsPublicApi, ExtractNoMatch)
{
    auto *mr = std::pmr::get_default_resource();
    Value c = numkit::lang::extract(mkStr(mr, "hi"), mkStr(mr, "world"), mr);
    ASSERT_TRUE(c.isCell());
    EXPECT_EQ(c.numel(), 0u);
}

TEST(BuiltinStringsPublicApi, SplitKeepsEmptyTokens)
{
    auto *mr = std::pmr::get_default_resource();
    Value c = numkit::lang::split(mkStr(mr, "a,b,,c"), mkStr(mr, ","), mr);
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
    Value j = numkit::lang::join(arr, d, mr);
    EXPECT_TRUE(j.isString());
    EXPECT_EQ(j.numel(), 1u);
    EXPECT_EQ(j.stringElem(0), "a-b-c");
}

TEST(BuiltinStringsPublicApi, JoinDefaultDelimIsSpace)
{
    auto *mr = std::pmr::get_default_resource();
    Value arr = Value::stringArray(1, 3, mr);
    arr.stringElemSet(0, "x"); arr.stringElemSet(1, "y"); arr.stringElemSet(2, "z");
    Value j = numkit::lang::join(arr, Value::Empty, mr);
    EXPECT_EQ(j.stringElem(0), "x y z");
}

// num2str column width when a NEGATIVE element is the widest: MATLAB's
// integer field is max-abs-DIGITS + 2 (the '-' is absorbed, not extra).
// DEEP-PROBE 2026-05-31: numkit measured "%.0f" of v (incl. sign) so a
// negative-dominant column was one space too wide. vs MATLAB R2025b.
TEST(BuiltinStringsPublicApi, Num2StrNegativeColumnWidth)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    auto row = [&](std::initializer_list<double> xs) {
        Value v = Value::matrix(1, xs.size(), ValueType::DOUBLE, mr);
        double *d = v.doubleDataMut();
        size_t i = 0; for (double x : xs) d[i++] = x;
        return v;
    };
    EXPECT_EQ(numkit::lang::num2str(row({-1, 10, -100}), mr).toString(), "-1   10 -100");
    EXPECT_EQ(numkit::lang::num2str(row({-100, -200}), mr).toString(),   "-100 -200");
    EXPECT_EQ(numkit::lang::num2str(row({5, -5}), mr).toString(),        "5 -5");
    EXPECT_EQ(numkit::lang::num2str(row({-5, -50, -500}), mr).toString(),"-5  -50 -500");
    EXPECT_EQ(numkit::lang::num2str(row({100, -1}), mr).toString(),      "100   -1");
    // Positives unchanged (regression guard).
    EXPECT_EQ(numkit::lang::num2str(row({1, 22, 333}), mr).toString(),   "1   22  333");
}

// strtok: first token + remainder. Lifted to public C++ API 2026-06.
TEST(BuiltinStringsPublicApi, Strtok)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    // explicit whitespace delim
    auto [tok, rem] =
        numkit::lang::strtok(mkStr(mr, "hello world"), " \t\r\n\f\v", mr);
    EXPECT_EQ(tok.toString(), "hello");
    EXPECT_EQ(rem.toString(), " world");
    // leading whitespace skipped
    auto [t2, r2] = numkit::lang::strtok(mkStr(mr, "  lead x"), " \t\r\n\f\v", mr);
    EXPECT_EQ(t2.toString(), "lead");
    EXPECT_EQ(r2.toString(), " x");
    // custom delimiter set
    auto [t3, r3] = numkit::lang::strtok(mkStr(mr, "a,b,c"), ",", mr);
    EXPECT_EQ(t3.toString(), "a");
    EXPECT_EQ(r3.toString(), ",b,c");
    // no delimiter present -> whole string, empty remainder
    auto [t4, r4] = numkit::lang::strtok(mkStr(mr, "nodlim"), " ", mr);
    EXPECT_EQ(t4.toString(), "nodlim");
    EXPECT_EQ(r4.toString(), "");
    // default delim + default mr (omit both trailing args)
    auto [t5, r5] = numkit::lang::strtok(mkStr(mr, "x y"));
    EXPECT_EQ(t5.toString(), "x");
    EXPECT_EQ(r5.toString(), " y");
}
