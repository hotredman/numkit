// libs/builtin/tests/misc4_batch_test.cpp
// (20 functions):
//   convert:     convertCharsToStrings · convertContainedStringsToChars
//                · convertStringsToChars
//   interp ND:   interp3 · interpn (both deferred — adapter arg-shape gaps)
//   int extr:    intmax · intmin
//   collection:  iscellstr · ismembertol · issorted · isstrprop · mat2cell
//   shape:       meshgrid · ndgrid
//   misc:        newline · nnz · nonzeros · nthroot · num2cell · pad
// All . Bit-identical MATLAB R2025b.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class Misc4BatchTest : public ::testing::Test
{
public:
    StandardEngine engine;
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(Misc4BatchTest, ConvertFamily)
{
    EXPECT_DOUBLE_EQ(evalScalar("isstring(convertCharsToStrings('hello'))"),     1.0);
    eval("c = convertContainedStringsToChars({\"hi\", \"world\"});");
    EXPECT_DOUBLE_EQ(evalScalar("ischar(c{1})"),                                 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("ischar(convertStringsToChars(\"hi\"))"),        1.0);
}

TEST_F(Misc4BatchTest, IntmaxIntmin)
{
    EXPECT_DOUBLE_EQ(evalScalar("double(intmax('int8'))"),   127.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(intmax('int16'))"),  32767.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(intmin('int8'))"), -128.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(intmin('int16'))"), -32768.0);
}

TEST_F(Misc4BatchTest, IsCellStrIsSortedIsStrProp)
{
    // iscellstr: cell of CHAR (single-quoted), not string (double-quoted).
    EXPECT_DOUBLE_EQ(evalScalar("iscellstr({'a', 'b'})"),     1.0);
    EXPECT_DOUBLE_EQ(evalScalar("iscellstr({\"a\", \"b\"})"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("iscellstr({1, 2})"),         0.0);
    EXPECT_DOUBLE_EQ(evalScalar("issorted([1 2 3])"),         1.0);
    EXPECT_DOUBLE_EQ(evalScalar("issorted([3 1 2])"),         0.0);
    eval("v = isstrprop('a1b', 'alpha');");
    EXPECT_DOUBLE_EQ(evalScalar("double(v(1))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(v(2))"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(v(3))"), 1.0);
}

TEST_F(Misc4BatchTest, IsMemberTol)
{
    eval("tf = ismembertol([1.0001, 2, 3], [1, 2, 4], 1e-3);");
    EXPECT_DOUBLE_EQ(evalScalar("double(tf(1))"), 1.0);  // within tol
    EXPECT_DOUBLE_EQ(evalScalar("double(tf(2))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(tf(3))"), 0.0);
}

TEST_F(Misc4BatchTest, Mat2CellNum2Cell)
{
    eval("C = mat2cell([1 2 3 4], 1, [2 2]);");
    EXPECT_DOUBLE_EQ(evalScalar("C{1}(1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("C{1}(2)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("C{2}(1)"), 3.0);

    eval("c = num2cell([1 2 3]);");
    EXPECT_DOUBLE_EQ(evalScalar("c{1}"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("c{3}"), 3.0);
}

// num2cell(A, dims): the listed dimensions are collapsed into each cell.
// Was element-wise only (num2cell(A,dim) threw). vs MATLAB R2025b on
// A = [1 2 3; 4 5 6]. DEEP-PROBE 2026-05-31.
TEST_F(Misc4BatchTest, Num2CellDim)
{
    eval("A = [1 2 3; 4 5 6];");
    // dim 2: collapse columns -> 2x1 cell of rows.
    eval("c2 = num2cell(A, 2);");
    EXPECT_DOUBLE_EQ(evalScalar("size(c2,1)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(c2,2)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("c2{1}(2)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("c2{2}(3)"), 6.0);
    // dim 1: collapse rows -> 1x3 cell of columns.
    eval("c1 = num2cell(A, 1);");
    EXPECT_DOUBLE_EQ(evalScalar("size(c1,1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(c1,2)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("c1{1}(2)"), 4.0);
    EXPECT_DOUBLE_EQ(evalScalar("c1{3}(2)"), 6.0);
    // dims [1 2]: whole matrix in a 1x1 cell.
    eval("cb = num2cell(A, [1 2]);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(cb)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("cb{1}(2,3)"), 6.0);
    // a dim past ndim (3 on a 2-D array) is a trivial singleton -> element-wise.
    eval("c3 = num2cell(A, 3);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(c3)"), 6.0);
    EXPECT_DOUBLE_EQ(evalScalar("c3{2,2}"), 5.0);
    // complex preserved.
    eval("cz = num2cell([1+2i 3; 4 5-1i], 1);");
    EXPECT_DOUBLE_EQ(evalScalar("real(cz{1}(1))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("imag(cz{1}(1))"), 2.0);
}

TEST_F(Misc4BatchTest, MeshgridNdgrid)
{
    eval("[X, Y] = meshgrid(1:3, 1:2);");
    EXPECT_DOUBLE_EQ(evalScalar("X(1,1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("X(2,3)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("Y(2,3)"), 2.0);

    eval("[X, Y] = ndgrid(1:3, 1:2);");
    EXPECT_DOUBLE_EQ(evalScalar("X(1,1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("X(3,2)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("Y(3,2)"), 2.0);
}

TEST_F(Misc4BatchTest, NewlineNnzNonzeros)
{
    EXPECT_DOUBLE_EQ(evalScalar("strlength(newline)"), 1.0);

    EXPECT_DOUBLE_EQ(evalScalar("nnz([1 0 2 0 3])"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("nnz([0 0 0])"),     0.0);
    EXPECT_DOUBLE_EQ(evalScalar("nnz(eye(4))"),      4.0);

    eval("v = nonzeros([1 0 2 0 3]);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(v)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("v(1)"),     1.0);
    EXPECT_DOUBLE_EQ(evalScalar("v(3)"),     3.0);
}

TEST_F(Misc4BatchTest, Nthroot)
{
    EXPECT_NEAR(evalScalar("nthroot(8, 3)"),    2.0, 1e-12);
    EXPECT_NEAR(evalScalar("nthroot(16, 4)"),   2.0, 1e-12);
    EXPECT_NEAR(evalScalar("nthroot(-27, 3)"), -3.0, 1e-12);  // odd root of negative
}

TEST_F(Misc4BatchTest, Pad)
{
    EXPECT_DOUBLE_EQ(evalScalar("strlength(pad(\"hi\", 5))"), 5.0);
}

// pad on a CELL str (was throwing "Not a char array") + the default-width
// form (n omitted -> the longest element). vs MATLAB R2025b. DEEP-PROBE
// 2026-05-31.
TEST_F(Misc4BatchTest, PadCellAndDefaultWidth)
{
    // default width = max strlength across the cell (3 here).
    eval("d = pad({'a','bbb'});");
    EXPECT_DOUBLE_EQ(evalScalar("double(iscell(d))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("strlength(d{1})"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("strlength(d{2})"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("strcmp(d{1}, 'a  ')"), 1.0);   // right-pad spaces
    // explicit width.
    eval("cn = pad({'a','bb'}, 4);");
    EXPECT_DOUBLE_EQ(evalScalar("strcmp(cn{1}, 'a   ')"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("strcmp(cn{2}, 'bb  ')"), 1.0);
    // left + both with a custom pad char.
    eval("lf = pad({'a','bb'}, 4, 'left');");
    EXPECT_DOUBLE_EQ(evalScalar("strcmp(lf{1}, '   a')"), 1.0);
    eval("bt = pad({'a','bb'}, 4, 'both', '*');");
    EXPECT_DOUBLE_EQ(evalScalar("strcmp(bt{1}, '*a**')"), 1.0);  // floor-left, ceil-right
    EXPECT_DOUBLE_EQ(evalScalar("strcmp(bt{2}, '*bb*')"), 1.0);
    // shape preserved (column cell stays a column).
    eval("cc = pad({'a';'bbb'});");
    EXPECT_DOUBLE_EQ(evalScalar("size(cc,1)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(cc,2)"), 1.0);
    // scalar (non-cell) paths unchanged.
    EXPECT_DOUBLE_EQ(evalScalar("strlength(pad('a'))"), 1.0);   // default n = own length -> no-op
    EXPECT_DOUBLE_EQ(evalScalar("strcmp(pad('hi', 5, 'right', '-'), 'hi---')"), 1.0);
}
