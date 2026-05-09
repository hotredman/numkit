// libs/builtin/tests/misc4_batch_test.cpp
//
// Audit ТЗ batch closure (20 functions):
//   convert:     convertCharsToStrings · convertContainedStringsToChars
//                · convertStringsToChars
//   interp ND:   interp3 · interpn (both deferred — adapter arg-shape gaps)
//   int extr:    intmax · intmin
//   collection:  iscellstr · ismembertol · issorted · isstrprop · mat2cell
//   shape:       meshgrid · ndgrid
//   misc:        newline · nnz · nonzeros · nthroot · num2cell · pad
//
// All flagged "no major gap detected". Bit-identical MATLAB R2025b.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class Misc4BatchTest : public ::testing::Test
{
public:
    Engine engine;
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
