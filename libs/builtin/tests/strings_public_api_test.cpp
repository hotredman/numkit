// libs/builtin/tests/strings_public_api_test.cpp
//
// Direct-call tests for numkit::builtin string functions.
// Exercises algorithm without Engine/Parser/VM.

#include <numkit/builtin/datatypes/strings/strings.hpp>
#include <numkit/builtin/datatypes/strings/regex.hpp>

#include <numkit/core/allocator.hpp>
#include <numkit/core/types.hpp>
#include <numkit/core/value.hpp>

#include <gtest/gtest.h>

#include <cmath>

using numkit::Allocator;
using numkit::Error;
using numkit::ValueType;
using numkit::Value;

namespace {

Value mkStr(Allocator &alloc, const char *s) { return Value::fromString(s, &alloc); }

} // namespace

// ── num2str / str2num / str2double ───────────────────────────────────────
TEST(BuiltinStringsPublicApi, Num2StrScalar)
{
    Allocator alloc = Allocator::defaultAllocator();
    Value r = numkit::builtin::num2str(alloc, Value::scalar(3.14, &alloc));
    ASSERT_TRUE(r.isChar());
    EXPECT_EQ(r.toString(), "3.14");
}

TEST(BuiltinStringsPublicApi, Str2NumSuccess)
{
    Allocator alloc = Allocator::defaultAllocator();
    Value r = numkit::builtin::str2num(alloc, mkStr(alloc, "42.5"));
    EXPECT_DOUBLE_EQ(r.toScalar(), 42.5);
}

TEST(BuiltinStringsPublicApi, Str2NumFailureReturnsEmpty)
{
    Allocator alloc = Allocator::defaultAllocator();
    Value r = numkit::builtin::str2num(alloc, mkStr(alloc, "not a number"));
    EXPECT_TRUE(r.isEmpty());
}

TEST(BuiltinStringsPublicApi, Str2DoubleSuccess)
{
    Allocator alloc = Allocator::defaultAllocator();
    Value r = numkit::builtin::str2double(alloc, mkStr(alloc, "3.14e2"));
    EXPECT_DOUBLE_EQ(r.toScalar(), 314.0);
}

TEST(BuiltinStringsPublicApi, Str2DoubleFailureReturnsNaN)
{
    Allocator alloc = Allocator::defaultAllocator();
    Value r = numkit::builtin::str2double(alloc, mkStr(alloc, "xyz"));
    EXPECT_TRUE(std::isnan(r.toScalar()));
}

// ── toString / toChar ────────────────────────────────────────────────────
TEST(BuiltinStringsPublicApi, ToStringFromScalar)
{
    Allocator alloc = Allocator::defaultAllocator();
    Value r = numkit::builtin::toString(alloc, Value::scalar(7.5, &alloc));
    ASSERT_TRUE(r.isString());
    EXPECT_EQ(r.toString(), "7.5");
}

TEST(BuiltinStringsPublicApi, ToCharFromAsciiCodes)
{
    Allocator alloc = Allocator::defaultAllocator();
    auto v = Value::matrix(1, 3, ValueType::DOUBLE, &alloc);
    double *d = v.doubleDataMut();
    d[0] = 72; d[1] = 105; d[2] = 33; // "Hi!"
    Value r = numkit::builtin::toChar(alloc, v);
    ASSERT_TRUE(r.isChar());
    EXPECT_EQ(r.toString(), "Hi!");
}

// ── strcmp / strcmpi ─────────────────────────────────────────────────────
TEST(BuiltinStringsPublicApi, StrcmpExact)
{
    Allocator alloc = Allocator::defaultAllocator();
    EXPECT_TRUE(numkit::builtin::strcmp(alloc, mkStr(alloc, "abc"),
                                           mkStr(alloc, "abc"))
                    .toBool());
    EXPECT_FALSE(numkit::builtin::strcmp(alloc, mkStr(alloc, "abc"),
                                            mkStr(alloc, "ABC"))
                     .toBool());
}

TEST(BuiltinStringsPublicApi, StrcmpiCaseInsensitive)
{
    Allocator alloc = Allocator::defaultAllocator();
    EXPECT_TRUE(numkit::builtin::strcmpi(alloc, mkStr(alloc, "Hello"),
                                            mkStr(alloc, "hELLO"))
                    .toBool());
}

// ── upper / lower ────────────────────────────────────────────────────────
TEST(BuiltinStringsPublicApi, UpperAscii)
{
    Allocator alloc = Allocator::defaultAllocator();
    Value r = numkit::builtin::upper(alloc, mkStr(alloc, "Mixed Case"));
    EXPECT_EQ(r.toString(), "MIXED CASE");
}

TEST(BuiltinStringsPublicApi, LowerAscii)
{
    Allocator alloc = Allocator::defaultAllocator();
    Value r = numkit::builtin::lower(alloc, mkStr(alloc, "Mixed Case"));
    EXPECT_EQ(r.toString(), "mixed case");
}

// ── strtrim ──────────────────────────────────────────────────────────────
TEST(BuiltinStringsPublicApi, StrtrimStripsWhitespace)
{
    Allocator alloc = Allocator::defaultAllocator();
    Value r = numkit::builtin::strtrim(alloc, mkStr(alloc, "  \t hello\n "));
    EXPECT_EQ(r.toString(), "hello");
}

TEST(BuiltinStringsPublicApi, StrtrimAllWhitespaceReturnsEmpty)
{
    Allocator alloc = Allocator::defaultAllocator();
    Value r = numkit::builtin::strtrim(alloc, mkStr(alloc, "   \n\t"));
    EXPECT_EQ(r.toString(), "");
}

// ── strsplit ─────────────────────────────────────────────────────────────
TEST(BuiltinStringsPublicApi, StrsplitDefaultDelimIsSpace)
{
    Allocator alloc = Allocator::defaultAllocator();
    Value r = numkit::builtin::strsplit(alloc, mkStr(alloc, "one two three"));
    ASSERT_EQ(r.numel(), 3u);
    EXPECT_EQ(r.cellAt(0).toString(), "one");
    EXPECT_EQ(r.cellAt(1).toString(), "two");
    EXPECT_EQ(r.cellAt(2).toString(), "three");
}

TEST(BuiltinStringsPublicApi, StrsplitCustomDelim)
{
    Allocator alloc = Allocator::defaultAllocator();
    Value r = numkit::builtin::strsplit(alloc, mkStr(alloc, "a,b,c"),
                                            mkStr(alloc, ","));
    ASSERT_EQ(r.numel(), 3u);
    EXPECT_EQ(r.cellAt(0).toString(), "a");
    EXPECT_EQ(r.cellAt(2).toString(), "c");
}

// ── strcat ───────────────────────────────────────────────────────────────
TEST(BuiltinStringsPublicApi, StrcatConcatenatesAll)
{
    Allocator alloc = Allocator::defaultAllocator();
    Value a = mkStr(alloc, "foo");
    Value b = mkStr(alloc, "bar");
    Value c = mkStr(alloc, "baz");
    Value parts[] = {a, b, c};
    numkit::Span<const Value> span(parts, 3);
    Value r = numkit::builtin::strcat(alloc, span);
    EXPECT_EQ(r.toString(), "foobarbaz");
}

// ── strlength ────────────────────────────────────────────────────────────
TEST(BuiltinStringsPublicApi, StrlengthOfCharArray)
{
    Allocator alloc = Allocator::defaultAllocator();
    Value r = numkit::builtin::strlength(alloc, mkStr(alloc, "hello"));
    EXPECT_DOUBLE_EQ(r.toScalar(), 5.0);
}

// ── strrep ───────────────────────────────────────────────────────────────
TEST(BuiltinStringsPublicApi, StrrepReplacesAllOccurrences)
{
    Allocator alloc = Allocator::defaultAllocator();
    Value r = numkit::builtin::strrep(alloc,
                                          mkStr(alloc, "foo bar foo baz foo"),
                                          mkStr(alloc, "foo"),
                                          mkStr(alloc, "XYZ"));
    EXPECT_EQ(r.toString(), "XYZ bar XYZ baz XYZ");
}

TEST(BuiltinStringsPublicApi, StrrepEmptyOldPatIsPassThrough)
{
    Allocator alloc = Allocator::defaultAllocator();
    Value r = numkit::builtin::strrep(alloc, mkStr(alloc, "abc"),
                                          mkStr(alloc, ""),
                                          mkStr(alloc, "X"));
    EXPECT_EQ(r.toString(), "abc");
}

// ── contains / startsWith / endsWith ─────────────────────────────────────
TEST(BuiltinStringsPublicApi, ContainsPositive)
{
    Allocator alloc = Allocator::defaultAllocator();
    EXPECT_TRUE(numkit::builtin::contains(alloc, mkStr(alloc, "hello world"),
                                             mkStr(alloc, "lo wo"))
                    .toBool());
}

TEST(BuiltinStringsPublicApi, ContainsNegative)
{
    Allocator alloc = Allocator::defaultAllocator();
    EXPECT_FALSE(numkit::builtin::contains(alloc, mkStr(alloc, "hello"),
                                              mkStr(alloc, "xyz"))
                     .toBool());
}

TEST(BuiltinStringsPublicApi, StartsWithTrueFalse)
{
    Allocator alloc = Allocator::defaultAllocator();
    EXPECT_TRUE(numkit::builtin::startsWith(alloc, mkStr(alloc, "hello"),
                                               mkStr(alloc, "hel"))
                    .toBool());
    EXPECT_FALSE(numkit::builtin::startsWith(alloc, mkStr(alloc, "hello"),
                                                mkStr(alloc, "world"))
                     .toBool());
}

TEST(BuiltinStringsPublicApi, EndsWithTrueFalse)
{
    Allocator alloc = Allocator::defaultAllocator();
    EXPECT_TRUE(numkit::builtin::endsWith(alloc, mkStr(alloc, "hello.txt"),
                                             mkStr(alloc, ".txt"))
                    .toBool());
    EXPECT_FALSE(numkit::builtin::endsWith(alloc, mkStr(alloc, "hello"),
                                              mkStr(alloc, ".txt"))
                     .toBool());
}
