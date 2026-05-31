// libs/builtin/tests/strings_batch_test.cpp
//
// Audit ТЗ batch closure for string ops — 14 functions:
//   lower / upper / strtrim / deblank / blanks
//   strlength / strrep / strfind / strcat / strsplit / strtok
//   contains / startsWith / endsWith
//
// All flagged "no major gap detected" — bit-identical MATLAB R2025b
// on probed inputs.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class StringsBatchTest : public ::testing::Test
{
public:
    Engine engine;
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
    std::string evalString(const std::string &c) { return eval(c).toString(); }
};

TEST_F(StringsBatchTest, LowerUpper)
{
    EXPECT_EQ(evalString("lower(\"Hello WORLD\")"), "hello world");
    EXPECT_EQ(evalString("upper(\"abc XYZ\")"),     "ABC XYZ");
    EXPECT_EQ(evalString("lower(\"\")"),            "");
}

TEST_F(StringsBatchTest, TrimDeblank)
{
    EXPECT_EQ(evalString("strtrim(\"  abc  \")"), "abc");
    EXPECT_EQ(evalString("deblank(\"abc   \")"),  "abc");
}

TEST_F(StringsBatchTest, Blanks)
{
    EXPECT_DOUBLE_EQ(evalScalar("strlength(blanks(5))"), 5.0);
}

TEST_F(StringsBatchTest, Strlength)
{
    EXPECT_DOUBLE_EQ(evalScalar("strlength(\"hello\")"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("strlength(\"\")"),      0.0);
}

TEST_F(StringsBatchTest, Strrep)
{
    EXPECT_EQ(evalString("strrep(\"hello world\", \"world\", \"matlab\")"), "hello matlab");
}

// strrep with cell-array arguments: any cell input => a cell of char vectors
// (scalars broadcast). Was throwing "Not a char array". vs MATLAB R2025b.
// DEEP-PROBE 2026-05-31.
TEST_F(StringsBatchTest, StrrepCell)
{
    // cell str, scalar old/new -> per-element replace.
    eval("c = strrep({'hello','world','book'}, 'o', 'O');");
    EXPECT_DOUBLE_EQ(evalScalar("double(iscell(c))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("numel(c)"), 3.0);
    EXPECT_EQ(evalString("c{1}"), "hellO");
    EXPECT_EQ(evalString("c{2}"), "wOrld");
    EXPECT_EQ(evalString("c{3}"), "bOOk");
    // scalar str + cell pattern/replacement -> broadcast separately (not chained).
    eval("c2 = strrep('aXbYc', {'X','Y'}, {'-','='});");
    EXPECT_DOUBLE_EQ(evalScalar("numel(c2)"), 2.0);
    EXPECT_EQ(evalString("c2{1}"), "a-bYc");
    EXPECT_EQ(evalString("c2{2}"), "aXb=c");
    // all-cell element-wise.
    eval("c3 = strrep({'aa','bb'}, {'a','b'}, {'X','Y'});");
    EXPECT_EQ(evalString("c3{1}"), "XX");
    EXPECT_EQ(evalString("c3{2}"), "YY");
    // shape preserved: a column cell stays a column.
    eval("cc = strrep({'ax';'bx'}, 'x', 'Z');");
    EXPECT_DOUBLE_EQ(evalScalar("size(cc,1)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(cc,2)"), 1.0);
    EXPECT_EQ(evalString("cc{2}"), "bZ");
    // scalar (no-cell) path still returns a char row, unchanged.
    EXPECT_EQ(evalString("strrep('mississippi', 'iss', 'ISS')"), "mISSISSippi");
    // incompatible non-scalar cell sizes throw.
    EXPECT_THROW(eval("strrep({'a','b','c'}, {'a','b'}, 'X');"), std::exception);
}

TEST_F(StringsBatchTest, Strfind)
{
    eval("ix = strfind(\"hello world\", \"o\");");
    EXPECT_DOUBLE_EQ(evalScalar("ix(1)"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("ix(2)"), 8.0);
}

TEST_F(StringsBatchTest, Contains)
{
    EXPECT_DOUBLE_EQ(evalScalar("contains(\"hello\", \"ell\")"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("contains(\"hello\", \"xyz\")"), 0.0);
}

TEST_F(StringsBatchTest, StartsEndsWith)
{
    EXPECT_DOUBLE_EQ(evalScalar("startsWith(\"hello\", \"he\")"),  1.0);
    EXPECT_DOUBLE_EQ(evalScalar("startsWith(\"hello\", \"ell\")"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("endsWith(\"hello\", \"lo\")"),    1.0);
    EXPECT_DOUBLE_EQ(evalScalar("endsWith(\"hello\", \"ell\")"),   0.0);
}

// contains/startsWith/endsWith accept a cell array (or string array) of
// patterns and match if ANY of them matches. vs MATLAB R2025b. 2026-05-30:
// previously these threw "Not a char array" on a cell pattern argument.
TEST_F(StringsBatchTest, MatchAnyOfCellPatterns)
{
    // startsWith
    EXPECT_DOUBLE_EQ(evalScalar("startsWith('foobar', {'foo','xyz'})"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("startsWith('foobar', {'zzz','xyz'})"), 0.0);
    // endsWith
    EXPECT_DOUBLE_EQ(evalScalar("endsWith('test.m', {'.m','.cpp'})"),   1.0);
    EXPECT_DOUBLE_EQ(evalScalar("endsWith('test.txt', {'.m','.cpp'})"), 0.0);
    // contains
    EXPECT_DOUBLE_EQ(evalScalar("contains('hello', {'ell','xyz'})"),    1.0);
    EXPECT_DOUBLE_EQ(evalScalar("contains('hello', {'zzz','xyz'})"),    0.0);
    // string-array pattern list
    EXPECT_DOUBLE_EQ(evalScalar("startsWith('foobar', [\"foo\" \"xyz\"])"), 1.0);
    // scalar pattern unchanged
    EXPECT_DOUBLE_EQ(evalScalar("contains('hello', 'ell')"),            1.0);
}

TEST_F(StringsBatchTest, Strcat)
{
    EXPECT_EQ(evalString("strcat(\"ab\", \"cd\")"),       "abcd");
    EXPECT_EQ(evalString("strcat(\"a\", \"b\", \"c\")"),  "abc");
}

TEST_F(StringsBatchTest, Strsplit)
{
    eval("p = strsplit(\"a b c\");");
    EXPECT_DOUBLE_EQ(evalScalar("numel(p)"), 3.0);
}

// strsplit: cell-array delimiters, multi-char delimiters, and the
// CollapseDelimiters option. vs MATLAB R2025b. 2026-05-30.
TEST_F(StringsBatchTest, StrsplitCellMultiCollapse)
{
    // Cell array of delimiters, longest-match.
    eval("c1 = strsplit('a, b; c', {', ', '; '});");
    EXPECT_DOUBLE_EQ(evalScalar("numel(c1)"), 3.0);
    EXPECT_EQ(evalString("c1{1}"), "a");
    EXPECT_EQ(evalString("c1{2}"), "b");
    EXPECT_EQ(evalString("c1{3}"), "c");
    // Multi-character delimiter string.
    eval("c2 = strsplit('a==b==c', '==');");
    EXPECT_DOUBLE_EQ(evalScalar("numel(c2)"), 3.0);
    EXPECT_EQ(evalString("c2{2}"), "b");
    // collapse=true (default): leading/trailing empties kept, internal merged.
    eval("c3 = strsplit(',a,b,', ',');");
    EXPECT_DOUBLE_EQ(evalScalar("numel(c3)"), 4.0);
    EXPECT_EQ(evalString("c3{1}"), "");
    EXPECT_EQ(evalString("c3{4}"), "");
    eval("c4 = strsplit('a,,b', ',');");
    EXPECT_DOUBLE_EQ(evalScalar("numel(c4)"), 2.0);  // internal '' collapsed
    // CollapseDelimiters=false: split at every occurrence.
    eval("c5 = strsplit('a,,b', ',', 'CollapseDelimiters', false);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(c5)"), 3.0);
    EXPECT_EQ(evalString("c5{2}"), "");
    // Default whitespace delimiter, collapse.
    eval("c6 = strsplit('  a  b  ');");
    EXPECT_DOUBLE_EQ(evalScalar("numel(c6)"), 4.0);
}

// [tokens, matches] = strsplit(...): the matched delimiters (was missing).
TEST_F(StringsBatchTest, StrsplitMatchesOutput)
{
    eval("function [a,b] = wSS(s,d)\n  [a,b] = strsplit(s,d);\nend");
    eval("[t, m] = wSS('a,b;c', {',', ';'});");
    EXPECT_DOUBLE_EQ(evalScalar("numel(t)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("numel(m)"), 2.0);
    EXPECT_EQ(evalString("m{1}"), ",");
    EXPECT_EQ(evalString("m{2}"), ";");
    // collapsed run -> single match of the whole run
    eval("function [a,b] = wS2(s,d)\n  [a,b] = strsplit(s,d);\nend");
    eval("[t2, m2] = wS2('a,,b', ',');");
    EXPECT_DOUBLE_EQ(evalScalar("numel(m2)"), 1.0);
    EXPECT_EQ(evalString("m2{1}"), ",,");
}

TEST_F(StringsBatchTest, Strtok)
{
    eval("[t, rem] = strtok(\"hello world\");");
    EXPECT_EQ(evalString("t"),   "hello");
    EXPECT_EQ(evalString("rem"), " world");
}
