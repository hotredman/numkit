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

#include <cmath>

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

// lower/upper/strtrim/deblank/strip applied to a CELL array: element-wise ->
// cell of char vectors, same shape. Were all throwing "Not a char array".
// vs MATLAB R2025b. DEEP-PROBE 2026-05-31.
TEST_F(StringsBatchTest, CaseTrimCellArrays)
{
    eval("lo = lower({'AbC','XyZ'}); up = upper({'aBc','xYz'});");
    EXPECT_DOUBLE_EQ(evalScalar("double(iscell(lo))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("numel(lo)"), 2.0);
    EXPECT_EQ(evalString("lo{1}"), "abc");
    EXPECT_EQ(evalString("lo{2}"), "xyz");
    EXPECT_EQ(evalString("up{1}"), "ABC");
    EXPECT_EQ(evalString("up{2}"), "XYZ");
    // strtrim keeps interior whitespace; deblank trims trailing only.
    eval("st = strtrim({'  a b ','  x  '});");
    EXPECT_EQ(evalString("st{1}"), "a b");
    EXPECT_EQ(evalString("st{2}"), "x");
    eval("db = deblank({'a  ','  b '});");
    EXPECT_EQ(evalString("db{1}"), "a");
    EXPECT_EQ(evalString("db{2}"), "  b");
    // strip (default both) + strip with an explicit char.
    eval("sp = strip({'  a  ','  b'});");
    EXPECT_EQ(evalString("sp{1}"), "a");
    EXPECT_EQ(evalString("sp{2}"), "b");
    eval("sx = strip({'xxaxx','xb'}, 'both', 'x');");
    EXPECT_EQ(evalString("sx{1}"), "a");
    EXPECT_EQ(evalString("sx{2}"), "b");
    // shape preserved (column cell stays a column).
    eval("col = lower({'AB';'CD'});");
    EXPECT_DOUBLE_EQ(evalScalar("size(col,1)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(col,2)"), 1.0);
    // scalar (non-cell) path still returns a char row.
    EXPECT_EQ(evalString("lower('HELLO')"), "hello");
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

// str2double on a CELL array / non-scalar string array -> DOUBLE matrix of
// the same shape (NaN where an element doesn't parse). Was throwing "Not a
// char array". vs MATLAB R2025b. DEEP-PROBE 2026-05-31.
TEST_F(StringsBatchTest, Str2DoubleCellArray)
{
    eval("a = str2double({'1.5','2.5','x'});");
    EXPECT_DOUBLE_EQ(evalScalar("a(1)"), 1.5);
    EXPECT_DOUBLE_EQ(evalScalar("a(2)"), 2.5);
    EXPECT_TRUE(std::isnan(evalScalar("a(3)")));
    EXPECT_DOUBLE_EQ(evalScalar("numel(a)"), 3.0);
    // Shape is preserved: 3x1 column cell -> 3x1 double.
    eval("b = str2double({'1';'2';'3'});");
    EXPECT_DOUBLE_EQ(evalScalar("size(b,1)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(b,2)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("b(2)"), 2.0);
    // 2x2 cell -> 2x2 double (column-major element check).
    eval("c = str2double({'1','2';'3','4'});");
    EXPECT_DOUBLE_EQ(evalScalar("c(2,1)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("c(1,2)"), 2.0);
    // Comma-stripping, Inf/-Inf/NaN, whitespace, empty -> NaN, per element.
    eval("d = str2double({' 42 ','1,234','Inf','-Inf','NaN',''});");
    EXPECT_DOUBLE_EQ(evalScalar("d(1)"), 42.0);
    EXPECT_DOUBLE_EQ(evalScalar("d(2)"), 1234.0);
    EXPECT_TRUE(std::isinf(evalScalar("d(3)")) && evalScalar("d(3)") > 0);
    EXPECT_TRUE(std::isinf(evalScalar("d(4)")) && evalScalar("d(4)") < 0);
    EXPECT_TRUE(std::isnan(evalScalar("d(5)")));
    EXPECT_TRUE(std::isnan(evalScalar("d(6)")));
    // Non-scalar string array also maps element-wise.
    eval("e = str2double([\"10\" \"20\" \"abc\"]);");
    EXPECT_DOUBLE_EQ(evalScalar("e(1)"), 10.0);
    EXPECT_TRUE(std::isnan(evalScalar("e(3)")));
    // Scalar char/string form is unchanged.
    EXPECT_DOUBLE_EQ(evalScalar("str2double('3.14')"), 3.14);
}

// contains/startsWith/endsWith on a CELL / non-scalar string-array SOURCE ->
// LOGICAL array of the same shape (was throwing "Not a char array" on a cell,
// scalar on a string array). vs MATLAB R2025b. DEEP-PROBE 2026-05-31.
TEST_F(StringsBatchTest, ContainsStartsEndsCellSource)
{
    eval("ca = contains({'hello','world','help'},'el');");
    EXPECT_TRUE(evalScalar("islogical(ca)") != 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("numel(ca)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(ca(1))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(ca(2))"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(ca(3))"), 1.0);
    eval("sa = startsWith({'hello','world','help'},'he');");
    EXPECT_DOUBLE_EQ(evalScalar("double(sa(1))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(sa(2))"), 0.0);
    eval("ea = endsWith([\"cat.txt\",\"dog.csv\",\"fish.txt\"],'.txt');");
    EXPECT_DOUBLE_EQ(evalScalar("double(ea(1))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(ea(3))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(ea(2))"), 0.0);
    // Shape preserved: 2x2 cell -> 2x2 logical.
    eval("m = contains({'ab','cd';'be','xy'},'b');");
    EXPECT_DOUBLE_EQ(evalScalar("size(m,1)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(m,2)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(m(1,1))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(m(2,1))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(m(1,2))"), 0.0);
    // 'IgnoreCase' + multi-pattern any-match over a cell source.
    eval("ic = startsWith({'Hello','world'},'he','IgnoreCase',true);");
    EXPECT_DOUBLE_EQ(evalScalar("double(ic(1))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(ic(2))"), 0.0);
    eval("mp = contains({'foo','bar','baz'},{'oo','az'});");
    EXPECT_DOUBLE_EQ(evalScalar("double(mp(1))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(mp(2))"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(mp(3))"), 1.0);
    // Scalar source still returns a logical scalar.
    EXPECT_DOUBLE_EQ(evalScalar("double(contains('hello','ell'))"), 1.0);
    EXPECT_TRUE(evalScalar("islogical(contains('hello','ell'))") != 0.0);
}

// str2num evaluates the (bracket-wrapped) string as an expression: matrices,
// ranges, arithmetic, with [] on failure and a logical success flag for the
// 2-output form. DEEP-PROBE 2026-05-31: was a scalar-only std::stod that
// returned [] for matrices/ranges. vs MATLAB R2025b.
TEST_F(StringsBatchTest, Str2NumEvaluatesExpressions)
{
    eval("m = str2num('[1 2; 3 4]');");
    EXPECT_DOUBLE_EQ(evalScalar("size(m,1)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(m,2)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("m(2,1)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("m(1,2)"), 2.0);
    eval("v = str2num('1:5');");
    EXPECT_DOUBLE_EQ(evalScalar("numel(v)"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("v(5)"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("str2num('2+3')"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("str2num('42.5')"), 42.5);
    EXPECT_DOUBLE_EQ(evalScalar("double(isempty(str2num('not a number')))"), 1.0);
    // 2-output form: [X, tf] = str2num(s).
    eval("[x, tf] = str2num('[10 20 30]');");
    EXPECT_DOUBLE_EQ(evalScalar("double(tf)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("x(2)"), 20.0);
    eval("[y, tf2] = str2num('garbage');");
    EXPECT_DOUBLE_EQ(evalScalar("double(tf2)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(isempty(y))"), 1.0);
}

// split output-class preservation vs MATLAB R2025b. 2026-05-31:
// previously split always returned a CELL. MATLAB returns a STRING array
// for a string input, and a cell array of char vectors only for char /
// cellstr input. Values and the N x 1 shape were already correct.
TEST_F(StringsBatchTest, SplitPreservesInputClass)
{
    // string input -> string array
    eval("p1 = split(\"a,b,c\", \",\");");
    EXPECT_DOUBLE_EQ(evalScalar("double(isstring(p1))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("numel(p1)"), 3.0);
    EXPECT_EQ(evalString("p1(2)"), "b");
    EXPECT_EQ(evalString("p1(3)"), "c");
    // char input -> cell array of char vectors
    eval("p2 = split('a,b,c', ',');");
    EXPECT_DOUBLE_EQ(evalScalar("double(iscell(p2))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("numel(p2)"), 3.0);
    EXPECT_EQ(evalString("p2{2}"), "b");
    // default whitespace delimiter on a string -> string array
    eval("p3 = split(\"a b c\");");
    EXPECT_DOUBLE_EQ(evalScalar("double(isstring(p3))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("numel(p3)"), 3.0);
}

// compose applies the format repeatedly across each ROW of x, consuming
// M = #conversion-specs values per output (R x ceil(C/M)); a short trailing
// chunk leaves unfilled specs literal; output class mirrors the fmt class.
// vs MATLAB R2025b. 2026-05-31: previously compose formatted per-ELEMENT
// with a single value, so a multi-spec format only filled the first spec.
TEST_F(StringsBatchTest, ComposeMultiSpecAndClass)
{
    // multi-spec, divisible -> 2x1 cell {"1-2";"3-4"} (was 4 wrong outputs)
    eval("A = compose('%d-%d', [1 2; 3 4]);");
    EXPECT_DOUBLE_EQ(evalScalar("double(iscell(A))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("numel(A)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(A,1)"), 2.0);
    EXPECT_EQ(evalString("A{1}"), "1-2");
    EXPECT_EQ(evalString("A{2}"), "3-4");
    // M=1 over a 2x2 -> 2x2 cell, column-major (B{3} == B(1,2))
    eval("B = compose('%d', [1 2; 3 4]);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(B)"), 4.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(B,2)"), 2.0);
    EXPECT_EQ(evalString("B{3}"), "2");
    // row vector, 2 specs -> 1x2
    eval("C = compose('%d-%d', [1 2 3 4]);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(C)"), 2.0);
    EXPECT_EQ(evalString("C{2}"), "3-4");
    // non-divisible: short trailing chunk keeps the unfilled spec literal
    eval("H = compose('%d-%d', [1 2 3; 4 5 6]);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(H)"), 4.0);
    EXPECT_EQ(evalString("H{3}"), "3-%d");   // H(1,2)
    EXPECT_EQ(evalString("H{4}"), "6-%d");   // H(2,2)
    // string format -> string array (class mirrors fmt)
    eval("S = compose(\"%d-%d\", [1 2; 3 4]);");
    EXPECT_DOUBLE_EQ(evalScalar("double(isstring(S))"), 1.0);
    EXPECT_EQ(evalString("S(1)"), "1-2");
}

// hex2num / num2hex: IEEE-754 hex <-> float bit reinterpret vs MATLAB
// R2025b. 2026-05-31: both were previously missing.
TEST_F(StringsBatchTest, HexNumRoundTrip)
{
    // hex2num: hex bit-pattern -> double (NOT decimal digits)
    EXPECT_DOUBLE_EQ(evalScalar("hex2num('3ff0000000000000')"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("hex2num('bff0000000000000')"), -1.0);
    EXPECT_DOUBLE_EQ(evalScalar("hex2num('4')"), 2.0);          // right-padded
    EXPECT_DOUBLE_EQ(evalScalar("hex2num('41')"), 131072.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(isinf(hex2num('7ff0000000000000')))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(isnan(hex2num('fff8000000000000')))"), 1.0);
    // char MATRIX -> N×1 double column
    eval("D = hex2num(['3ff0000000000000'; '4000000000000000']);");
    EXPECT_DOUBLE_EQ(evalScalar("size(D,1)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(D,2)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("D(2)"), 2.0);
    // num2hex: double -> 16 hex chars, single -> 8
    EXPECT_EQ(evalString("num2hex(1)"),  "3ff0000000000000");
    EXPECT_EQ(evalString("num2hex(-2)"), "c000000000000000");
    EXPECT_EQ(evalString("num2hex(pi)"), "400921fb54442d18");
    EXPECT_EQ(evalString("num2hex(single(1))"),  "3f800000");
    EXPECT_EQ(evalString("num2hex(single(-2))"), "c0000000");
    EXPECT_DOUBLE_EQ(evalScalar("numel(num2hex(single(1)))"), 8.0);
    // vector -> numel × 16 char matrix, row per element
    eval("V = num2hex([1 2 3]);");
    EXPECT_DOUBLE_EQ(evalScalar("size(V,1)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(V,2)"), 16.0);
    EXPECT_EQ(evalString("V(2,:)"), "4000000000000000");
    // round-trip and non-float error
    EXPECT_DOUBLE_EQ(evalScalar("double(hex2num(num2hex(pi)) == pi)"), 1.0);
    EXPECT_THROW(eval("num2hex(int8(5));"), std::exception);
}

// deblank / strtrim / strip preserve the input container class on a STRING
// input vs MATLAB R2025b. 2026-05-31: previously a string input returned a
// CHAR (deblank/strtrim) or collapsed a string array to 1x1 (strip).
TEST_F(StringsBatchTest, TrimPreservesStringClass)
{
    // string array -> string array, same shape + values
    eval("d = deblank([\"ab  \" \"cd \"]);");
    EXPECT_DOUBLE_EQ(evalScalar("double(isstring(d))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("numel(d)"), 2.0);
    EXPECT_EQ(evalString("d(1)"), "ab");
    EXPECT_EQ(evalString("d(2)"), "cd");
    eval("t = strtrim([\"  ab \" \"  cd\"]);");
    EXPECT_DOUBLE_EQ(evalScalar("double(isstring(t))"), 1.0);
    EXPECT_EQ(evalString("t(2)"), "cd");
    // strip on a string array keeps every element (was collapsing to 1x1)
    eval("p = strip([\"xxab\" \"xcd\"], 'left', 'x');");
    EXPECT_DOUBLE_EQ(evalScalar("double(isstring(p))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("numel(p)"), 2.0);
    EXPECT_EQ(evalString("p(2)"), "cd");
    // column string array -> shape preserved
    eval("cs = strtrim([\"  a\"; \"b  \"]);");
    EXPECT_DOUBLE_EQ(evalScalar("size(cs,1)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(cs,2)"), 1.0);
    // string scalar -> string scalar (NOT char)
    eval("d1 = deblank(\"ab  \");");
    EXPECT_DOUBLE_EQ(evalScalar("double(isstring(d1))"), 1.0);
    EXPECT_EQ(evalString("d1"), "ab");
    // char input -> char (unchanged); cell input -> cell (unchanged)
    eval("dc = deblank('ab  ');");
    EXPECT_DOUBLE_EQ(evalScalar("double(ischar(dc))"), 1.0);
    eval("ce = strtrim({'  a ', ' b  '});");
    EXPECT_DOUBLE_EQ(evalScalar("double(iscell(ce))"), 1.0);
    EXPECT_EQ(evalString("ce{2}"), "b");
}

// lower / upper / reverse preserve the input container class on a STRING
// input vs MATLAB R2025b. 2026-05-31: previously a string input returned a
// CHAR (lower/upper) or collapsed a string array to 1x1 (reverse).
TEST_F(StringsBatchTest, CaseReversePreserveStringClass)
{
    // lower/upper on a string array -> string array, same shape + values
    eval("lo = lower([\"AB\" \"CD\"]);");
    EXPECT_DOUBLE_EQ(evalScalar("double(isstring(lo))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("numel(lo)"), 2.0);
    EXPECT_EQ(evalString("lo(1)"), "ab");
    EXPECT_EQ(evalString("lo(2)"), "cd");
    eval("up = upper([\"ab\" \"cd\"]);");
    EXPECT_DOUBLE_EQ(evalScalar("double(isstring(up))"), 1.0);
    EXPECT_EQ(evalString("up(2)"), "CD");
    // reverse on a string array keeps every element (was collapsing to 1x1)
    eval("rv = reverse([\"abc\" \"de\"]);");
    EXPECT_DOUBLE_EQ(evalScalar("double(isstring(rv))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("numel(rv)"), 2.0);
    EXPECT_EQ(evalString("rv(1)"), "cba");
    EXPECT_EQ(evalString("rv(2)"), "ed");
    // column string array -> shape preserved
    eval("cs = upper([\"a\"; \"b\"]);");
    EXPECT_DOUBLE_EQ(evalScalar("size(cs,1)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(cs,2)"), 1.0);
    // string scalar -> string scalar (NOT char)
    eval("ls = lower(\"HeLLo\");");
    EXPECT_DOUBLE_EQ(evalScalar("double(isstring(ls))"), 1.0);
    EXPECT_EQ(evalString("ls"), "hello");
    // char input -> char; cell input -> cell (unchanged)
    eval("lc = lower('HeLLo');");
    EXPECT_DOUBLE_EQ(evalScalar("double(ischar(lc))"), 1.0);
    eval("ce2 = upper({'ab','cd'});");
    EXPECT_DOUBLE_EQ(evalScalar("double(iscell(ce2))"), 1.0);
    EXPECT_EQ(evalString("ce2{1}"), "AB");
}

// erase / pad / extractAfter / extractBefore / insertAfter / insertBefore
// preserve the input container class + shape on a STRING array vs MATLAB
// R2025b. 2026-05-31: previously each collapsed a string array to 1x1.
TEST_F(StringsBatchTest, ExtractInsertErasePadPreserveStringClass)
{
    eval("e = erase([\"a1b\" \"c2d\"], \"1\");");
    EXPECT_DOUBLE_EQ(evalScalar("double(isstring(e))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("numel(e)"), 2.0);
    EXPECT_EQ(evalString("e(1)"), "ab");
    EXPECT_EQ(evalString("e(2)"), "c2d");
    // pad explicit width + default width (longest element)
    eval("p = pad([\"a\" \"bb\"], 4);");
    EXPECT_DOUBLE_EQ(evalScalar("double(isstring(p))"), 1.0);
    EXPECT_EQ(evalString("p(1)"), "a   ");
    eval("pn = pad([\"a\" \"bbb\"]);");
    EXPECT_EQ(evalString("pn(1)"), "a  ");
    EXPECT_EQ(evalString("pn(2)"), "bbb");
    // extractAfter / extractBefore
    eval("ea = extractAfter([\"a-b\" \"c-d\"], \"-\");");
    EXPECT_DOUBLE_EQ(evalScalar("double(isstring(ea))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("numel(ea)"), 2.0);
    EXPECT_EQ(evalString("ea(2)"), "d");
    eval("eb = extractBefore([\"a-b\" \"c-d\"], \"-\");");
    EXPECT_EQ(evalString("eb(1)"), "a");
    // insertAfter / insertBefore
    eval("ia = insertAfter([\"ab\" \"cd\"], \"a\", \"X\");");
    EXPECT_DOUBLE_EQ(evalScalar("double(isstring(ia))"), 1.0);
    EXPECT_EQ(evalString("ia(1)"), "aXb");
    EXPECT_EQ(evalString("ia(2)"), "cd");
    eval("ib = insertBefore([\"ab\" \"cd\"], \"b\", \"X\");");
    EXPECT_EQ(evalString("ib(1)"), "aXb");
    // column string array shape preserved
    eval("col = erase([\"x1\"; \"y1\"], \"1\");");
    EXPECT_DOUBLE_EQ(evalScalar("size(col,1)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(col,2)"), 1.0);
    // char + cell paths unchanged
    eval("ec = erase('a1b1c', '1');");
    EXPECT_DOUBLE_EQ(evalScalar("double(ischar(ec))"), 1.0);
    EXPECT_EQ(evalString("ec"), "abc");
    eval("pc = pad({'a','bb'}, 3);");
    EXPECT_DOUBLE_EQ(evalScalar("double(iscell(pc))"), 1.0);
    EXPECT_EQ(evalString("pc{1}"), "a  ");
}

// matches / count return a SAME-SHAPE logical/double array for a string
// array source vs MATLAB R2025b. 2026-05-31: both collapsed a string array
// to a scalar; the cell branches were already element-wise.
TEST_F(StringsBatchTest, MatchesCountStringArray)
{
    // matches: string array -> logical array
    eval("m = matches([\"cat\" \"dog\" \"cat\"], \"cat\");");
    EXPECT_DOUBLE_EQ(evalScalar("double(islogical(m))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("numel(m)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(m(1))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(m(2))"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(m(3))"), 1.0);
    // multi-pattern: match if equal to ANY alternative
    eval("mc = matches([\"cat\" \"dog\" \"fish\"], [\"cat\" \"fish\"]);");
    EXPECT_DOUBLE_EQ(evalScalar("double(mc(1))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(mc(3))"), 1.0);
    // scalar + cell unchanged
    EXPECT_DOUBLE_EQ(evalScalar("double(matches(\"cat\", \"cat\"))"), 1.0);
    eval("mcl = matches({'a','b'}, 'a');");
    EXPECT_DOUBLE_EQ(evalScalar("double(mcl(2))"), 0.0);
    // count: string array -> double array
    eval("c = count([\"aa\" \"aba\"], \"a\");");
    EXPECT_DOUBLE_EQ(evalScalar("numel(c)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("c(1)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("c(2)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("count(\"banana\", \"a\")"), 3.0);
    // column string array -> shape preserved
    eval("mcol = matches([\"a\"; \"b\"], \"a\");");
    EXPECT_DOUBLE_EQ(evalScalar("size(mcol,1)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(mcol,2)"), 1.0);
}

// strcmp / strcmpi / strncmp / strncmpi compare element-wise with broadcast
// on a string-array operand vs MATLAB R2025b. 2026-05-31: a non-scalar
// string array previously collapsed to a logical scalar.
TEST_F(StringsBatchTest, StrcmpStringArray)
{
    // string array vs scalar -> element-wise logical array
    eval("a = strcmp([\"a\" \"b\" \"a\"], \"a\");");
    EXPECT_DOUBLE_EQ(evalScalar("double(islogical(a))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("numel(a)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(a(1))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(a(2))"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(a(3))"), 1.0);
    // two equal-size string arrays
    eval("b = strcmp([\"ab\" \"cd\"], [\"ab\" \"xy\"]);");
    EXPECT_DOUBLE_EQ(evalScalar("double(b(1))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(b(2))"), 0.0);
    // strcmpi / strncmp / strncmpi on string arrays
    eval("ei = strcmpi([\"A\" \"b\"], \"a\");");
    EXPECT_DOUBLE_EQ(evalScalar("double(ei(1))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(ei(2))"), 0.0);
    eval("f = strncmp([\"abc\" \"abd\" \"xyz\"], \"ab\", 2);");
    EXPECT_DOUBLE_EQ(evalScalar("double(f(1))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(f(3))"), 0.0);
    eval("fi = strncmpi([\"ABc\" \"xyz\"], \"ab\", 2);");
    EXPECT_DOUBLE_EQ(evalScalar("double(fi(1))"), 1.0);
    // string array vs cell mixes
    eval("g = strcmp([\"a\" \"b\"], {'a' 'x'});");
    EXPECT_DOUBLE_EQ(evalScalar("double(g(1))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(g(2))"), 0.0);
    // column array shape preserved
    eval("col = strcmp([\"a\"; \"b\"], \"a\");");
    EXPECT_DOUBLE_EQ(evalScalar("size(col,1)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(col,2)"), 1.0);
    // scalar + cell paths unchanged
    EXPECT_DOUBLE_EQ(evalScalar("double(strcmp(\"ab\", \"ab\"))"), 1.0);
    eval("c = strcmp({'a','b'}, {'a','x'});");
    EXPECT_DOUBLE_EQ(evalScalar("double(c(2))"), 0.0);
}
