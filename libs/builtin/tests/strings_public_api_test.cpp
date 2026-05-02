// libs/builtin/tests/strings_public_api_test.cpp
//
// Direct-call tests for numkit::builtin string functions.
// Exercises algorithm without Engine/Parser/VM.

#include <numkit/builtin/language/strings/strings.hpp>
#include <numkit/builtin/language/strings/regex.hpp>

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
    Value r = numkit::builtin::num2str(mr, Value::scalar(3.14, mr));
    ASSERT_TRUE(r.isChar());
    EXPECT_EQ(r.toString(), "3.14");
}

TEST(BuiltinStringsPublicApi, Str2NumSuccess)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    Value r = numkit::builtin::str2num(mr, mkStr(mr, "42.5"));
    EXPECT_DOUBLE_EQ(r.toScalar(), 42.5);
}

TEST(BuiltinStringsPublicApi, Str2NumFailureReturnsEmpty)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    Value r = numkit::builtin::str2num(mr, mkStr(mr, "not a number"));
    EXPECT_TRUE(r.isEmpty());
}

TEST(BuiltinStringsPublicApi, Str2DoubleSuccess)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    Value r = numkit::builtin::str2double(mr, mkStr(mr, "3.14e2"));
    EXPECT_DOUBLE_EQ(r.toScalar(), 314.0);
}

TEST(BuiltinStringsPublicApi, Str2DoubleFailureReturnsNaN)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    Value r = numkit::builtin::str2double(mr, mkStr(mr, "xyz"));
    EXPECT_TRUE(std::isnan(r.toScalar()));
}

// ── toString / toChar ────────────────────────────────────────────────────
TEST(BuiltinStringsPublicApi, ToStringFromScalar)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    Value r = numkit::builtin::toString(mr, Value::scalar(7.5, mr));
    ASSERT_TRUE(r.isString());
    EXPECT_EQ(r.toString(), "7.5");
}

TEST(BuiltinStringsPublicApi, ToCharFromAsciiCodes)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    auto v = Value::matrix(1, 3, ValueType::DOUBLE, mr);
    double *d = v.doubleDataMut();
    d[0] = 72; d[1] = 105; d[2] = 33; // "Hi!"
    Value r = numkit::builtin::toChar(mr, v);
    ASSERT_TRUE(r.isChar());
    EXPECT_EQ(r.toString(), "Hi!");
}

// ── strcmp / strcmpi ─────────────────────────────────────────────────────
TEST(BuiltinStringsPublicApi, StrcmpExact)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    EXPECT_TRUE(numkit::builtin::strcmp(mr, mkStr(mr, "abc"),
                                           mkStr(mr, "abc"))
                    .toBool());
    EXPECT_FALSE(numkit::builtin::strcmp(mr, mkStr(mr, "abc"),
                                            mkStr(mr, "ABC"))
                     .toBool());
}

TEST(BuiltinStringsPublicApi, StrcmpiCaseInsensitive)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    EXPECT_TRUE(numkit::builtin::strcmpi(mr, mkStr(mr, "Hello"),
                                            mkStr(mr, "hELLO"))
                    .toBool());
}

// ── upper / lower ────────────────────────────────────────────────────────
TEST(BuiltinStringsPublicApi, UpperAscii)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    Value r = numkit::builtin::upper(mr, mkStr(mr, "Mixed Case"));
    EXPECT_EQ(r.toString(), "MIXED CASE");
}

TEST(BuiltinStringsPublicApi, LowerAscii)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    Value r = numkit::builtin::lower(mr, mkStr(mr, "Mixed Case"));
    EXPECT_EQ(r.toString(), "mixed case");
}

// ── strtrim ──────────────────────────────────────────────────────────────
TEST(BuiltinStringsPublicApi, StrtrimStripsWhitespace)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    Value r = numkit::builtin::strtrim(mr, mkStr(mr, "  \t hello\n "));
    EXPECT_EQ(r.toString(), "hello");
}

TEST(BuiltinStringsPublicApi, StrtrimAllWhitespaceReturnsEmpty)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    Value r = numkit::builtin::strtrim(mr, mkStr(mr, "   \n\t"));
    EXPECT_EQ(r.toString(), "");
}

// ── strsplit ─────────────────────────────────────────────────────────────
TEST(BuiltinStringsPublicApi, StrsplitDefaultDelimIsSpace)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    Value r = numkit::builtin::strsplit(mr, mkStr(mr, "one two three"));
    ASSERT_EQ(r.numel(), 3u);
    EXPECT_EQ(r.cellAt(0).toString(), "one");
    EXPECT_EQ(r.cellAt(1).toString(), "two");
    EXPECT_EQ(r.cellAt(2).toString(), "three");
}

TEST(BuiltinStringsPublicApi, StrsplitCustomDelim)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    Value r = numkit::builtin::strsplit(mr, mkStr(mr, "a,b,c"),
                                            mkStr(mr, ","));
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
    Value r = numkit::builtin::strcat(mr, span);
    EXPECT_EQ(r.toString(), "foobarbaz");
}

// ── strlength ────────────────────────────────────────────────────────────
TEST(BuiltinStringsPublicApi, StrlengthOfCharArray)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    Value r = numkit::builtin::strlength(mr, mkStr(mr, "hello"));
    EXPECT_DOUBLE_EQ(r.toScalar(), 5.0);
}

// ── strrep ───────────────────────────────────────────────────────────────
TEST(BuiltinStringsPublicApi, StrrepReplacesAllOccurrences)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    Value r = numkit::builtin::strrep(mr,
                                          mkStr(mr, "foo bar foo baz foo"),
                                          mkStr(mr, "foo"),
                                          mkStr(mr, "XYZ"));
    EXPECT_EQ(r.toString(), "XYZ bar XYZ baz XYZ");
}

TEST(BuiltinStringsPublicApi, StrrepEmptyOldPatIsPassThrough)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    Value r = numkit::builtin::strrep(mr, mkStr(mr, "abc"),
                                          mkStr(mr, ""),
                                          mkStr(mr, "X"));
    EXPECT_EQ(r.toString(), "abc");
}

// ── contains / startsWith / endsWith ─────────────────────────────────────
TEST(BuiltinStringsPublicApi, ContainsPositive)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    EXPECT_TRUE(numkit::builtin::contains(mr, mkStr(mr, "hello world"),
                                             mkStr(mr, "lo wo"))
                    .toBool());
}

TEST(BuiltinStringsPublicApi, ContainsNegative)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    EXPECT_FALSE(numkit::builtin::contains(mr, mkStr(mr, "hello"),
                                              mkStr(mr, "xyz"))
                     .toBool());
}

TEST(BuiltinStringsPublicApi, StartsWithTrueFalse)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    EXPECT_TRUE(numkit::builtin::startsWith(mr, mkStr(mr, "hello"),
                                               mkStr(mr, "hel"))
                    .toBool());
    EXPECT_FALSE(numkit::builtin::startsWith(mr, mkStr(mr, "hello"),
                                                mkStr(mr, "world"))
                     .toBool());
}

TEST(BuiltinStringsPublicApi, EndsWithTrueFalse)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    EXPECT_TRUE(numkit::builtin::endsWith(mr, mkStr(mr, "hello.txt"),
                                             mkStr(mr, ".txt"))
                    .toBool());
    EXPECT_FALSE(numkit::builtin::endsWith(mr, mkStr(mr, "hello"),
                                              mkStr(mr, ".txt"))
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
    Value r = numkit::builtin::stringsND(mr, nullptr, 0);
    EXPECT_EQ(r.dims().rows(), 1u);
    EXPECT_EQ(r.dims().cols(), 1u);
    EXPECT_EQ(r.numel(), 1u);
    EXPECT_EQ(r.stringElem(0), "");
}

TEST(BuiltinStringsPublicApi, StringsNxNFromSingleArg)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    size_t d[] = {4};
    Value r = numkit::builtin::stringsND(mr, d, 1);
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
    Value r = numkit::builtin::stringsND(mr, d, 2);
    EXPECT_EQ(r.dims().rows(), 2u);
    EXPECT_EQ(r.dims().cols(), 3u);
    EXPECT_EQ(r.numel(), 6u);
}

TEST(BuiltinStringsPublicApi, Strings3D)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    size_t d[] = {2, 3, 4};
    Value r = numkit::builtin::stringsND(mr, d, 3);
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
    Value c = numkit::builtin::compose(mr, fmt, a);
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
    Value c = numkit::builtin::compose(mr, mkStr(mr, "x=%g"), Value::scalar(2.5, mr));
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
    Value r = numkit::builtin::strjust(mr, m, "right");
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
    Value c = numkit::builtin::extract(mr, mkStr(mr, "hello hello world"), mkStr(mr, "hello"));
    ASSERT_TRUE(c.isCell());
    EXPECT_EQ(c.dims().rows(), 2u);
    EXPECT_EQ(c.dims().cols(), 1u);
    EXPECT_EQ(c.cellAt(0).toString(), "hello");
    EXPECT_EQ(c.cellAt(1).toString(), "hello");
}

TEST(BuiltinStringsPublicApi, ExtractNoMatch)
{
    auto *mr = std::pmr::get_default_resource();
    Value c = numkit::builtin::extract(mr, mkStr(mr, "hi"), mkStr(mr, "world"));
    ASSERT_TRUE(c.isCell());
    EXPECT_EQ(c.numel(), 0u);
}

TEST(BuiltinStringsPublicApi, SplitKeepsEmptyTokens)
{
    auto *mr = std::pmr::get_default_resource();
    Value c = numkit::builtin::split(mr, mkStr(mr, "a,b,,c"), mkStr(mr, ","));
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
    Value j = numkit::builtin::join(mr, arr, &d);
    EXPECT_TRUE(j.isString());
    EXPECT_EQ(j.numel(), 1u);
    EXPECT_EQ(j.stringElem(0), "a-b-c");
}

TEST(BuiltinStringsPublicApi, JoinDefaultDelimIsSpace)
{
    auto *mr = std::pmr::get_default_resource();
    Value arr = Value::stringArray(1, 3, mr);
    arr.stringElemSet(0, "x"); arr.stringElemSet(1, "y"); arr.stringElemSet(2, "z");
    Value j = numkit::builtin::join(mr, arr, nullptr);
    EXPECT_EQ(j.stringElem(0), "x y z");
}
