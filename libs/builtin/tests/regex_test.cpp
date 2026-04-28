// libs/builtin/tests/regex_test.cpp
//
// regexp / regexpi / regexprep — ECMAScript via std::regex.

#include "dual_engine_fixture.hpp"

#include <cmath>

using namespace m_test;

class RegexTest : public DualEngineTest
{};

// ── regexp: default form (start indices) ───────────────────────

TEST_P(RegexTest, RegexpFindsLiteral)
{
    eval("ix = regexp('hello world hello', 'hello');");
    auto *ix = getVarPtr("ix");
    EXPECT_EQ(ix->numel(), 2u);
    EXPECT_DOUBLE_EQ(ix->doubleData()[0], 1.0);
    EXPECT_DOUBLE_EQ(ix->doubleData()[1], 13.0);
}

TEST_P(RegexTest, RegexpDigitClass)
{
    eval("ix = regexp('a1 b22 c333', '\\d+');");
    auto *ix = getVarPtr("ix");
    EXPECT_EQ(ix->numel(), 3u);
    EXPECT_DOUBLE_EQ(ix->doubleData()[0], 2.0);
    EXPECT_DOUBLE_EQ(ix->doubleData()[1], 5.0);
    EXPECT_DOUBLE_EQ(ix->doubleData()[2], 9.0);
}

TEST_P(RegexTest, RegexpNoMatchReturnsEmpty)
{
    eval("ix = regexp('hello', 'xyz');");
    auto *ix = getVarPtr("ix");
    EXPECT_EQ(ix->numel(), 0u);
}

// ── regexp: 'match' option ─────────────────────────────────────

TEST_P(RegexTest, RegexpMatchOption)
{
    eval("m = regexp('a1 b22 c333', '\\d+', 'match');");
    auto *m = getVarPtr("m");
    EXPECT_TRUE(m->isCell());
    EXPECT_EQ(m->numel(), 3u);
    EXPECT_EQ(m->cellAt(0).toString(), "1");
    EXPECT_EQ(m->cellAt(1).toString(), "22");
    EXPECT_EQ(m->cellAt(2).toString(), "333");
}

// ── regexp: 'tokens' option ────────────────────────────────────

TEST_P(RegexTest, RegexpTokensCaptureGroups)
{
    eval("t = regexp('age=42 height=175', '(\\w+)=(\\d+)', 'tokens');");
    auto *t = getVarPtr("t");
    EXPECT_TRUE(t->isCell());
    EXPECT_EQ(t->numel(), 2u);
    // First match: tokens = {'age', '42'}.
    auto &m1 = t->cellAt(0);
    EXPECT_TRUE(m1.isCell());
    EXPECT_EQ(m1.numel(), 2u);
    EXPECT_EQ(m1.cellAt(0).toString(), "age");
    EXPECT_EQ(m1.cellAt(1).toString(), "42");
    // Second match: tokens = {'height', '175'}.
    auto &m2 = t->cellAt(1);
    EXPECT_EQ(m2.cellAt(0).toString(), "height");
    EXPECT_EQ(m2.cellAt(1).toString(), "175");
}

// ── regexp: 'split' option ─────────────────────────────────────

TEST_P(RegexTest, RegexpSplitOnDelimiter)
{
    eval("s = regexp('a,b,,c', ',', 'split');");
    auto *s = getVarPtr("s");
    EXPECT_TRUE(s->isCell());
    EXPECT_EQ(s->numel(), 4u);
    EXPECT_EQ(s->cellAt(0).toString(), "a");
    EXPECT_EQ(s->cellAt(1).toString(), "b");
    EXPECT_EQ(s->cellAt(2).toString(), "");
    EXPECT_EQ(s->cellAt(3).toString(), "c");
}

TEST_P(RegexTest, RegexpUnknownOptionThrows)
{
    EXPECT_THROW(eval("ix = regexp('abc', 'b', 'noSuchOption');"), std::exception);
}

TEST_P(RegexTest, RegexpBadPatternThrows)
{
    EXPECT_THROW(eval("ix = regexp('abc', '(unclosed');"), std::exception);
}

// ── regexpi: case-insensitive ──────────────────────────────────

TEST_P(RegexTest, RegexpiIgnoresCase)
{
    eval("ix = regexpi('Hello WORLD hello', 'hello');");
    auto *ix = getVarPtr("ix");
    EXPECT_EQ(ix->numel(), 2u);
    EXPECT_DOUBLE_EQ(ix->doubleData()[0], 1.0);
    EXPECT_DOUBLE_EQ(ix->doubleData()[1], 13.0);
}

TEST_P(RegexTest, RegexpiMatchOption)
{
    eval("m = regexpi('Apple, BANANA, cherry', '[a-z]+', 'match');");
    auto *m = getVarPtr("m");
    EXPECT_EQ(m->numel(), 3u);
    EXPECT_EQ(m->cellAt(0).toString(), "Apple");
    EXPECT_EQ(m->cellAt(1).toString(), "BANANA");
    EXPECT_EQ(m->cellAt(2).toString(), "cherry");
}

// ── regexprep ──────────────────────────────────────────────────

TEST_P(RegexTest, RegexprepLiteral)
{
    eval("s = regexprep('hello world', 'world', 'there');");
    EXPECT_EQ(getVarPtr("s")->toString(), "hello there");
}

TEST_P(RegexTest, RegexprepBackReference)
{
    eval("s = regexprep('John Doe', '(\\w+) (\\w+)', '$2, $1');");
    EXPECT_EQ(getVarPtr("s")->toString(), "Doe, John");
}

TEST_P(RegexTest, RegexprepReplaceAllOccurrences)
{
    eval("s = regexprep('a-b-c-d', '-', '/');");
    EXPECT_EQ(getVarPtr("s")->toString(), "a/b/c/d");
}

TEST_P(RegexTest, RegexprepNoMatchReturnsOriginal)
{
    eval("s = regexprep('abc', 'xyz', '!');");
    EXPECT_EQ(getVarPtr("s")->toString(), "abc");
}

INSTANTIATE_DUAL(RegexTest);
