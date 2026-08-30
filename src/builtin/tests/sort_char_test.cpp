// toolboxes/builtin/tests/sort_char_test.cpp
//
// Regression guard for bugs/builtin/sort-char.md: sort used to throw
// "Not a double array" on char input. MATLAB R2025b sorts char by CODE POINT
// PRESERVING the char class on the values; the 2nd-output index stays double.
// Values below are bit-exact MATLAB R2025b.

#include <numkit/core/engine.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class SortCharTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override {}
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// sort('dcba') -> 'abcd' char (CLASS PRESERVED).
TEST_F(SortCharTest, VectorByCodePoint)
{
    eval("y = sort('dcba');");
    EXPECT_DOUBLE_EQ(evalScalar("double(y(1))"), 97.0);   // 'a'
    EXPECT_DOUBLE_EQ(evalScalar("double(y(2))"), 98.0);   // 'b'
    EXPECT_DOUBLE_EQ(evalScalar("double(y(3))"), 99.0);   // 'c'
    EXPECT_DOUBLE_EQ(evalScalar("double(y(4))"), 100.0);  // 'd'
    EXPECT_DOUBLE_EQ(evalScalar("ischar(y)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("isequal(y,'abcd')"), 1.0);
}

// [S,I] = sort('dcba') -> S char, I double [4 3 2 1].
TEST_F(SortCharTest, IndexOutputIsDouble)
{
    eval("[S, I] = sort('dcba');");
    EXPECT_DOUBLE_EQ(evalScalar("ischar(S)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("islogical(I)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("I(1)"), 4.0);
    EXPECT_DOUBLE_EQ(evalScalar("I(2)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("I(3)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("I(4)"), 1.0);
}

// 'descend' on char.
TEST_F(SortCharTest, Descend)
{
    eval("y = sort('dcba', 'descend');");
    EXPECT_DOUBLE_EQ(evalScalar("isequal(y,'dcba')"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("ischar(y)"), 1.0);
}

// Stability with repeated chars: sort('cbacb') -> 'abbcc', I=[3 2 5 1 4].
TEST_F(SortCharTest, RepeatedStable)
{
    eval("[S, I] = sort('cbacb');");
    EXPECT_DOUBLE_EQ(evalScalar("isequal(S,'abbcc')"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("I(1)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("I(2)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("I(3)"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("I(4)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("I(5)"), 4.0);
}

// 2-D char matrix: column-wise (default) and along dim 2.
TEST_F(SortCharTest, MatrixColumnAndDim2)
{
    // M = ['bd';'ca'] -> col-wise ['ba';'cd']
    eval("C = sort(['bd';'ca']);");
    EXPECT_DOUBLE_EQ(evalScalar("double(C(1,1))"), 98.0);  // 'b'
    EXPECT_DOUBLE_EQ(evalScalar("double(C(2,1))"), 99.0);  // 'c'
    EXPECT_DOUBLE_EQ(evalScalar("double(C(1,2))"), 97.0);  // 'a'
    EXPECT_DOUBLE_EQ(evalScalar("double(C(2,2))"), 100.0); // 'd'
    EXPECT_DOUBLE_EQ(evalScalar("ischar(C)"), 1.0);
    // dim 2 -> ['bd';'ac']
    eval("R = sort(['bd';'ca'], 2);");
    EXPECT_DOUBLE_EQ(evalScalar("double(R(2,1))"), 97.0);  // 'a'
    EXPECT_DOUBLE_EQ(evalScalar("double(R(2,2))"), 99.0);  // 'c'
}

// Scalar + empty char.
TEST_F(SortCharTest, ScalarAndEmpty)
{
    eval("y = sort('x');");
    EXPECT_DOUBLE_EQ(evalScalar("double(y)"), 120.0);
    EXPECT_DOUBLE_EQ(evalScalar("ischar(y)"), 1.0);
    eval("e = sort('');");
    EXPECT_DOUBLE_EQ(evalScalar("numel(e)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("ischar(e)"), 1.0);
}
