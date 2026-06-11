// toolboxes/builtin/tests/predicates2_batch_test.cpp
// predicates batch 2 + set ops + format/matrix:
//   predicates: isempty/isscalar/isvector/ismatrix/isrow/iscolumn/
//               isnumeric/isreal/isfinite/isinf/isnan/islogical/
//               ischar/isstring/isstruct/iscell
//   set ops:    union/intersect/setdiff/setxor/ismember
//   format:     sprintf/num2str/str2double
//   matrix:     transpose/ctranspose
// Total: 26 functions. All  — bit-
// identical MATLAB R2025b on probed inputs.

#include <numkit/core/engine.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class Predicates2BatchTest : public ::testing::Test
{
public:
    StandardEngine engine;
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// ─── shape predicates ───────────────────────────────────────────────

TEST_F(Predicates2BatchTest, ShapePredicates)
{
    EXPECT_DOUBLE_EQ(evalScalar("isempty([])"),                1.0);
    EXPECT_DOUBLE_EQ(evalScalar("isempty([1])"),               0.0);
    EXPECT_DOUBLE_EQ(evalScalar("isscalar(5)"),                1.0);
    EXPECT_DOUBLE_EQ(evalScalar("isscalar([1 2])"),            0.0);
    EXPECT_DOUBLE_EQ(evalScalar("isvector([1 2 3])"),          1.0);
    EXPECT_DOUBLE_EQ(evalScalar("isvector([1 2; 3 4])"),       0.0);
    EXPECT_DOUBLE_EQ(evalScalar("ismatrix([1 2; 3 4])"),       1.0);
    EXPECT_DOUBLE_EQ(evalScalar("isrow([1 2 3])"),             1.0);
    EXPECT_DOUBLE_EQ(evalScalar("isrow([1; 2])"),              0.0);
    EXPECT_DOUBLE_EQ(evalScalar("iscolumn([1; 2])"),           1.0);
    EXPECT_DOUBLE_EQ(evalScalar("iscolumn([1 2 3])"),          0.0);
}

// ─── type predicates ────────────────────────────────────────────────

TEST_F(Predicates2BatchTest, TypePredicates)
{
    EXPECT_DOUBLE_EQ(evalScalar("isnumeric(5)"),     1.0);
    EXPECT_DOUBLE_EQ(evalScalar("isnumeric(\"a\")"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("isreal(3.14)"),     1.0);
    EXPECT_DOUBLE_EQ(evalScalar("isreal(1+1i)"),     0.0);
    EXPECT_DOUBLE_EQ(evalScalar("islogical(true)"),  1.0);
    EXPECT_DOUBLE_EQ(evalScalar("islogical(1)"),     0.0);
    EXPECT_DOUBLE_EQ(evalScalar("ischar('a')"),      1.0);
    EXPECT_DOUBLE_EQ(evalScalar("ischar(5)"),        0.0);
    EXPECT_DOUBLE_EQ(evalScalar("isstring(\"hi\")"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("isstring('a')"),    0.0);
    eval("s.x = 1;");
    EXPECT_DOUBLE_EQ(evalScalar("isstruct(s)"),      1.0);
    EXPECT_DOUBLE_EQ(evalScalar("isstruct(5)"),      0.0);
    EXPECT_DOUBLE_EQ(evalScalar("iscell({1, 2})"),   1.0);
    EXPECT_DOUBLE_EQ(evalScalar("iscell(5)"),        0.0);
}

// ─── value predicates ───────────────────────────────────────────────

TEST_F(Predicates2BatchTest, ValuePredicates)
{
    eval("v = isfinite([1 NaN Inf 2]);");
    EXPECT_DOUBLE_EQ(evalScalar("double(v(1))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(v(2))"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(v(3))"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(v(4))"), 1.0);

    eval("v = isnan([1 NaN 2]);");
    EXPECT_DOUBLE_EQ(evalScalar("double(v(1))"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(v(2))"), 1.0);

    eval("v = isinf([1 Inf -Inf 2]);");
    EXPECT_DOUBLE_EQ(evalScalar("double(v(2))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(v(3))"), 1.0);
}

// ─── set operations ─────────────────────────────────────────────────

TEST_F(Predicates2BatchTest, SetOps)
{
    eval("u = union([1 2 3], [3 4 5]);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(u)"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("u(1)"),     1.0);
    EXPECT_DOUBLE_EQ(evalScalar("u(5)"),     5.0);

    eval("u = intersect([1 2 3 4], [2 4 6]);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(u)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("u(1)"),     2.0);
    EXPECT_DOUBLE_EQ(evalScalar("u(2)"),     4.0);

    eval("u = setdiff([1 2 3 4], [2 4]);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(u)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("u(1)"),     1.0);
    EXPECT_DOUBLE_EQ(evalScalar("u(2)"),     3.0);

    eval("u = setxor([1 2 3 4], [2 4 5 6]);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(u)"), 4.0);

    eval("tf = ismember([1 2 3 4], [2 4]);");
    EXPECT_DOUBLE_EQ(evalScalar("double(tf(1))"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(tf(2))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(tf(3))"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(tf(4))"), 1.0);
}

// ─── format / printf ────────────────────────────────────────────────

TEST_F(Predicates2BatchTest, FormatPrint)
{
    EXPECT_DOUBLE_EQ(evalScalar("strlength(sprintf('%d', 42))"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("strcmp(sprintf('%d', 42), '42')"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("strcmp(sprintf('%.2f', 3.14159), '3.14')"), 1.0);

    EXPECT_DOUBLE_EQ(evalScalar("strcmp(num2str(42), '42')"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("str2double('3.14')"), 3.14);
    EXPECT_DOUBLE_EQ(evalScalar("str2double('-1.5')"), -1.5);
}

// ─── transpose / ctranspose ─────────────────────────────────────────

TEST_F(Predicates2BatchTest, Transpose)
{
    eval("T = transpose([1 2 3]);");
    EXPECT_DOUBLE_EQ(evalScalar("size(T,1)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(T,2)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("T(1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("T(3)"), 3.0);

    eval("C = ctranspose([1 2 3]);");
    EXPECT_DOUBLE_EQ(evalScalar("size(C,1)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("C(1)"), 1.0);
}
