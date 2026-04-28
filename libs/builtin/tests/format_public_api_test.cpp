// libs/builtin/tests/format_public_api_test.cpp
//
// Direct-call tests for numkit::m::builtin formatting primitives.

#include <numkit/m/builtin/datatypes/strings/format.hpp>

#include <numkit/m/core/MAllocator.hpp>
#include <numkit/m/core/MSpan.hpp>
#include <numkit/m/core/MValue.hpp>

#include <gtest/gtest.h>

using numkit::m::Allocator;
using numkit::m::MType;
using numkit::m::MValue;
using numkit::m::Span;

namespace {

MValue mkStr(Allocator &alloc, const char *s) { return MValue::fromString(s, &alloc); }

} // namespace

// ── countFormatSpecs ────────────────────────────────────────────────────
TEST(BuiltinFormatPublicApi, CountFormatSpecsBasic)
{
    EXPECT_EQ(numkit::m::builtin::countFormatSpecs("%d %s"), 2u);
    EXPECT_EQ(numkit::m::builtin::countFormatSpecs("no specs here"), 0u);
    EXPECT_EQ(numkit::m::builtin::countFormatSpecs(""), 0u);
}

TEST(BuiltinFormatPublicApi, CountFormatSpecsIgnoresEscapedPercent)
{
    EXPECT_EQ(numkit::m::builtin::countFormatSpecs("100%% done: %d"), 1u);
}

TEST(BuiltinFormatPublicApi, CountFormatSpecsWithFlagsWidthPrecision)
{
    EXPECT_EQ(numkit::m::builtin::countFormatSpecs("%-5.2f %+03d"), 2u);
}

// ── formatOnce ──────────────────────────────────────────────────────────
TEST(BuiltinFormatPublicApi, FormatOnceIntegerAndString)
{
    Allocator alloc = Allocator::defaultAllocator();
    MValue args[] = {MValue::scalar(42.0, &alloc), mkStr(alloc, "hello")};
    std::string out = numkit::m::builtin::formatOnce("%d - %s!", Span<const MValue>(args, 2));
    EXPECT_EQ(out, "42 - hello!");
}

TEST(BuiltinFormatPublicApi, FormatOnceFloatPrecision)
{
    Allocator alloc = Allocator::defaultAllocator();
    MValue args[] = {MValue::scalar(3.14159, &alloc)};
    std::string out = numkit::m::builtin::formatOnce("%.2f", Span<const MValue>(args, 1));
    EXPECT_EQ(out, "3.14");
}

TEST(BuiltinFormatPublicApi, FormatOnceEscapes)
{
    std::string out = numkit::m::builtin::formatOnce(
        "line1\\nline2\\t\\\\", Span<const MValue>{});
    EXPECT_EQ(out, "line1\nline2\t\\");
}

TEST(BuiltinFormatPublicApi, FormatOncePercentPercent)
{
    Allocator alloc = Allocator::defaultAllocator();
    MValue args[] = {MValue::scalar(50.0, &alloc)};
    std::string out = numkit::m::builtin::formatOnce("%d%%", Span<const MValue>(args, 1));
    EXPECT_EQ(out, "50%");
}

TEST(BuiltinFormatPublicApi, FormatOnceHexAndOctal)
{
    Allocator alloc = Allocator::defaultAllocator();
    MValue args[] = {MValue::scalar(255.0, &alloc), MValue::scalar(8.0, &alloc)};
    std::string out =
        numkit::m::builtin::formatOnce("%x %o", Span<const MValue>(args, 2));
    EXPECT_EQ(out, "ff 10");
}

// ── formatCyclic ────────────────────────────────────────────────────────
TEST(BuiltinFormatPublicApi, FormatCyclicRepeatsOverArray)
{
    Allocator alloc = Allocator::defaultAllocator();
    auto arr = MValue::matrix(1, 4, MType::DOUBLE, &alloc);
    double *d = arr.doubleDataMut();
    d[0] = 1; d[1] = 2; d[2] = 3; d[3] = 4;
    MValue args[] = {arr};
    std::string out = numkit::m::builtin::formatCyclic(
        alloc, "%d ", Span<const MValue>(args, 1), 0);
    EXPECT_EQ(out, "1 2 3 4 ");
}

TEST(BuiltinFormatPublicApi, FormatCyclicMultipleSpecsPerCycle)
{
    Allocator alloc = Allocator::defaultAllocator();
    auto arr = MValue::matrix(1, 4, MType::DOUBLE, &alloc);
    double *d = arr.doubleDataMut();
    d[0] = 1; d[1] = 2; d[2] = 3; d[3] = 4;
    MValue args[] = {arr};
    // Two specs → two pairs: "1 2\n3 4\n"
    std::string out = numkit::m::builtin::formatCyclic(
        alloc, "%d %d\\n", Span<const MValue>(args, 1), 0);
    EXPECT_EQ(out, "1 2\n3 4\n");
}

TEST(BuiltinFormatPublicApi, FormatCyclicEmptyArgsJustPrintsFmt)
{
    Allocator alloc = Allocator::defaultAllocator();
    std::string out = numkit::m::builtin::formatCyclic(
        alloc, "just text\\n", Span<const MValue>{}, 0);
    EXPECT_EQ(out, "just text\n");
}

// ── sprintf (MValue wrapper) ────────────────────────────────────────────
TEST(BuiltinFormatPublicApi, SprintfReturnsCharArray)
{
    Allocator alloc = Allocator::defaultAllocator();
    MValue args[] = {MValue::scalar(7.0, &alloc), mkStr(alloc, "foo")};
    MValue r = numkit::m::builtin::sprintf(alloc, mkStr(alloc, "%d/%s"),
                                           Span<const MValue>(args, 2));
    ASSERT_TRUE(r.isChar());
    EXPECT_EQ(r.toString(), "7/foo");
}

TEST(BuiltinFormatPublicApi, SprintfNonCharFmtReturnsEmpty)
{
    Allocator alloc = Allocator::defaultAllocator();
    MValue r = numkit::m::builtin::sprintf(alloc, MValue::scalar(1.0, &alloc),
                                           Span<const MValue>{});
    ASSERT_TRUE(r.isChar());
    EXPECT_EQ(r.toString(), "");
}
