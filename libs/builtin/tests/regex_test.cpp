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

// ── regexp: 'names' option (named tokens → struct) ─────────────

TEST_P(RegexTest, RegexpNamesSingleMatch)
{
    eval("n = regexp('John Smith age 42', "
         "'(?<first>\\w+)\\s+(?<last>\\w+)\\s+age\\s+(?<age>\\d+)', 'names');");
    EXPECT_EQ(static_cast<int>(evalScalar("numel(n)")), 1);
    EXPECT_EQ(eval("n.first").toString(), "John");
    EXPECT_EQ(eval("n.last").toString(),  "Smith");
    EXPECT_EQ(eval("n.age").toString(),   "42");
}

TEST_P(RegexTest, RegexpNamesStructArray)
{
    eval("t = regexp('a1 b2 c3', '(?<L>[a-z])(?<D>\\d)', 'names');");
    EXPECT_EQ(static_cast<int>(evalScalar("numel(t)")), 3);
    EXPECT_EQ(static_cast<int>(evalScalar("size(t,1)")), 1);
    EXPECT_EQ(eval("t(1).L").toString(), "a");
    EXPECT_EQ(eval("t(2).L").toString(), "b");
    EXPECT_EQ(eval("t(3).D").toString(), "3");
}

TEST_P(RegexTest, RegexpNamesNonParticipatingGroupEmpty)
{
    // Alternation: only one named group participates → other field is ''.
    eval("np = regexp('x', '(?<a>a)|(?<b>x)', 'names');");
    EXPECT_EQ(eval("np.a").toString(), "");
    EXPECT_EQ(eval("np.b").toString(), "x");
}

TEST_P(RegexTest, RegexpNamesNoMatchIsEmpty)
{
    eval("z = regexp('zzz', '(?<L>[a-z])(?<D>\\d)', 'names');");
    EXPECT_EQ(static_cast<int>(evalScalar("numel(z)")), 0);
}

TEST_P(RegexTest, RegexpNamedGroupInTokensActsAsCapture)
{
    // A named group still functions as an ordinary capture group for
    // 'tokens' (std::regex can't parse the (?<name>) syntax raw).
    eval("tk = regexp('a1', '(?<L>[a-z])(\\d)', 'tokens');");
    EXPECT_EQ(eval("tk{1}{1}").toString(), "a");
    EXPECT_EQ(eval("tk{1}{2}").toString(), "1");
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

// regexprep with cell-array arguments. Was throwing "s, pat, rep must be
// strings". A cell STR processes element-wise (cell of char vectors, same
// shape); a cell PATTERN is a LIST applied sequentially to each string; a
// single replacement is recycled across patterns. vs MATLAB R2025b.
// DEEP-PROBE 2026-05-31.
TEST_P(RegexTest, RegexprepCell)
{
    // cell str, scalar pattern -> per-element replace.
    eval("r = regexprep({'foo123','bar45'}, '\\d+', '#');");
    EXPECT_DOUBLE_EQ(getVarPtr("r")->numel(), 2u);
    eval("a = r{1}; b = r{2};");
    EXPECT_EQ(getVarPtr("a")->toString(), "foo#");
    EXPECT_EQ(getVarPtr("b")->toString(), "bar#");
    // scalar str + cell pattern -> sequential chaining into ONE string.
    eval("q = regexprep('a1b2', {'\\d','[ab]'}, {'#','@'});");
    EXPECT_EQ(getVarPtr("q")->toString(), "@#@#");
    // single replacement recycled across all patterns.
    eval("z = regexprep('a1b2', {'\\d','[ab]'}, 'Z');");
    EXPECT_EQ(getVarPtr("z")->toString(), "ZZZZ");
    // cell str + cell pattern: pattern list applied to EACH string.
    eval("c = regexprep({'a1','b2'}, {'\\d','[ab]'}, {'#','@'}); c1 = c{1}; c2 = c{2};");
    EXPECT_EQ(getVarPtr("c1")->toString(), "@#");
    EXPECT_EQ(getVarPtr("c2")->toString(), "@#");
    // 'ignorecase' option still applies across a cell.
    eval("ic = regexprep({'AbC','xyZ'}, '[a-z]', '_', 'ignorecase'); i1 = ic{1};");
    EXPECT_EQ(getVarPtr("i1")->toString(), "___");
    // column-cell shape preserved.
    eval("cc = regexprep({'a1';'b2'}, '\\d', '#');");
    EXPECT_DOUBLE_EQ(getVarPtr("cc")->dims().rows(), 2);
    EXPECT_DOUBLE_EQ(getVarPtr("cc")->dims().cols(), 1);
    // mismatched pattern/replacement counts throw.
    EXPECT_THROW(eval("regexprep('abc', {'a','b','c'}, {'1','2'});"), std::exception);
}

// ── Pack 36: regexptranslate ────────────────────────────────────────
TEST_P(RegexTest, RegexptranslateEscapesMetachars)
{
    eval("s = regexptranslate('escape', 'a.b*c');");
    EXPECT_EQ(getVarPtr("s")->toString(), "a\\.b\\*c");
}

TEST_P(RegexTest, RegexptranslateEscapeNoMetaPassthrough)
{
    eval("s = regexptranslate('escape', 'hello');");
    EXPECT_EQ(getVarPtr("s")->toString(), "hello");
}

TEST_P(RegexTest, RegexptranslateWildcardStarBecomesDotStar)
{
    eval("s = regexptranslate('wildcard', '*.txt');");
    EXPECT_EQ(getVarPtr("s")->toString(), ".*\\.txt");
}

TEST_P(RegexTest, RegexptranslateWildcardQuestionBecomesDot)
{
    eval("s = regexptranslate('wildcard', 'data?.csv');");
    EXPECT_EQ(getVarPtr("s")->toString(), "data.\\.csv");
}

TEST_P(RegexTest, RegexptranslateBadOpThrows)
{
    EXPECT_THROW(eval("s = regexptranslate('bogus', 'x');"),
                 std::exception);
}

// ── regexp default positional multi-output [start, end, te, m, t, nm, sp] ──
TEST_P(RegexTest, RegexpStartEnd)
{
    eval("function [a,b] = wRE(s,p)\n  [a,b] = regexp(s,p);\nend");
    eval("[s, e] = wRE('a1b2', '\\d');");
    auto *s = getVarPtr("s");
    auto *e = getVarPtr("e");
    ASSERT_NE(s, nullptr);
    ASSERT_NE(e, nullptr);
    EXPECT_EQ(s->numel(), 2u);
    EXPECT_DOUBLE_EQ(s->doubleData()[0], 2.0);
    EXPECT_DOUBLE_EQ(s->doubleData()[1], 4.0);
    EXPECT_DOUBLE_EQ(e->doubleData()[0], 2.0);
    EXPECT_DOUBLE_EQ(e->doubleData()[1], 4.0);
}

TEST_P(RegexTest, RegexpFullPositionalOrder)
{
    eval("function [a,b,c,d,f,g,h] = wRF(s,p)\n"
         "  [a,b,c,d,f,g,h] = regexp(s,p);\nend");
    eval("[s, e, te, m, t, nm, sp] = wRF('a1b2', '(\\d)');");
    EXPECT_DOUBLE_EQ(evalScalar("s(1)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("e(2)"), 4.0);
    // tokenExtents: 1x2 [start end] of the capture group in match 1
    EXPECT_DOUBLE_EQ(evalScalar("te{1}(1)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("te{1}(2)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("strcmp(m{1}, '1')"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("strcmp(t{1}{1}, '1')"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("strcmp(sp{1}, 'a')"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("strcmp(sp{2}, 'b')"), 1.0);
}

TEST_P(RegexTest, RegexpSingleOutputUnchanged)
{
    eval("ix = regexp('a1b2', '\\d');");
    auto *ix = getVarPtr("ix");
    EXPECT_EQ(ix->numel(), 2u);
    EXPECT_DOUBLE_EQ(ix->doubleData()[0], 2.0);
    EXPECT_DOUBLE_EQ(ix->doubleData()[1], 4.0);
}

// regexp(...,'once'): match the first occurrence, return the scalarised
// form. DEEP-PROBE 2026-05-31 — previously threw "unknown option 'once'".
// vs MATLAB R2025b on s='a1b2c3'.
TEST_P(RegexTest, RegexpOnceOption)
{
    // 'start' (default selector) -> scalar index.
    EXPECT_DOUBLE_EQ(evalScalar("regexp('a1b2c3','\\d','once')"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("regexp('a1b2c3','\\d','end','once')"), 2.0);
    // 'match','once' -> char row (not a cell).
    eval("m = regexp('a1b2c3','\\d','match','once');");
    EXPECT_TRUE(evalBool("ischar(m)"));
    EXPECT_TRUE(evalBool("strcmp(m,'1')"));
    // 'tokens','once' -> 1xk cell of capture chars (unwrapped one level).
    eval("t = regexp('a1b2c3','(\\w)(\\d)','tokens','once');");
    EXPECT_TRUE(evalBool("iscell(t)"));
    EXPECT_DOUBLE_EQ(evalScalar("numel(t)"), 2.0);
    EXPECT_TRUE(evalBool("strcmp(t{1},'a') && strcmp(t{2},'1')"));
    // 'split','once' -> split at the FIRST match only.
    eval("sp = regexp('a,b,c',',','split','once');");
    EXPECT_DOUBLE_EQ(evalScalar("numel(sp)"), 2.0);
    EXPECT_TRUE(evalBool("strcmp(sp{1},'a') && strcmp(sp{2},'b,c')"));
    // No match: start/end -> [], match -> ''.
    EXPECT_TRUE(evalBool("isempty(regexp('xyz','\\d','once'))"));
    EXPECT_TRUE(evalBool("isempty(regexp('xyz','\\d','match','once'))"));
    EXPECT_TRUE(evalBool("ischar(regexp('xyz','\\d','match','once'))"));
}

INSTANTIATE_DUAL(RegexTest);
