// toolboxes/builtin/tests/cellfun_inputforms_test.cpp
//
// Regression guard for bugs/builtin/cellfun-inputforms.md (FIXED): cellfun
// supports (1) multiple cell arrays — cellfun(fn, C1, C2, ...) applies
// fn(C1{i}, C2{i}, ...) — and (2) the legacy string-function-name forms
// cellfun('isempty'|'length'|'ndims'|'prodofsize'|'isreal'|'islogical', C),
// plus cellfun('size', C, k) and cellfun('isclass', C, 'cls'). MATLAB R2025b.

#include <numkit/core/engine.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

class CellfunInputFormsTest : public ::testing::Test
{
public:
    numkit::StandardEngine engine;
    void SetUp() override {}
    numkit::Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// cellfun(fn, C1, C2): fn(C1{i}, C2{i}).
TEST_F(CellfunInputFormsTest, TwoCells)
{
    eval("r = cellfun(@(a,b) a+b, {1,2,3}, {10,20,30});");
    EXPECT_DOUBLE_EQ(evalScalar("r(1)"), 11.0);
    EXPECT_DOUBLE_EQ(evalScalar("r(2)"), 22.0);
    EXPECT_DOUBLE_EQ(evalScalar("r(3)"), 33.0);
    EXPECT_EQ(static_cast<int>(evalScalar("numel(r)")), 3);
}

// Three cell arrays.
TEST_F(CellfunInputFormsTest, ThreeCells)
{
    eval("r = cellfun(@(a,b,c) a+b+c, {1,2}, {10,20}, {100,200});");
    EXPECT_DOUBLE_EQ(evalScalar("r(1)"), 111.0);
    EXPECT_DOUBLE_EQ(evalScalar("r(2)"), 222.0);
}

// Multi-cell with UniformOutput=false → cell output.
TEST_F(CellfunInputFormsTest, MultiCellNonUniform)
{
    eval("c = cellfun(@(a,b) a*b, {2,3}, {5,7}, 'UniformOutput', false);");
    EXPECT_TRUE(eval("iscell(c)").toBool());
    EXPECT_DOUBLE_EQ(evalScalar("c{1}"), 10.0);
    EXPECT_DOUBLE_EQ(evalScalar("c{2}"), 21.0);
}

// Mismatched cell sizes throw.
TEST_F(CellfunInputFormsTest, SizeMismatchThrows)
{
    EXPECT_ANY_THROW(eval("cellfun(@(a,b) a+b, {1,2}, {10,20,30});"));
}

// Legacy string names returning logical / double per cell.
TEST_F(CellfunInputFormsTest, StringNamesSimple)
{
    eval("ie = cellfun('isempty', {[],[1],[]});");
    EXPECT_DOUBLE_EQ(evalScalar("ie(1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("ie(2)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("ie(3)"), 1.0);
    EXPECT_TRUE(eval("islogical(ie)").toBool());

    eval("ln = cellfun('length', {[1 2],[1 2 3]});");
    EXPECT_DOUBLE_EQ(evalScalar("ln(1)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("ln(2)"), 3.0);

    eval("nd = cellfun('ndims', {1, ones(2,2,2)});");
    EXPECT_DOUBLE_EQ(evalScalar("nd(1)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("nd(2)"), 3.0);

    eval("ps = cellfun('prodofsize', {[1 2 3], ones(2,3)});");
    EXPECT_DOUBLE_EQ(evalScalar("ps(1)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("ps(2)"), 6.0);

    eval("ir = cellfun('isreal', {1, 1+2i});");
    EXPECT_DOUBLE_EQ(evalScalar("ir(1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("ir(2)"), 0.0);

    eval("il = cellfun('islogical', {true, 1});");
    EXPECT_DOUBLE_EQ(evalScalar("il(1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("il(2)"), 0.0);
}

// 'size' with a dim arg and 'isclass' with a class name.
TEST_F(CellfunInputFormsTest, StringNamesWithExtraArg)
{
    eval("sz = cellfun('size', {[1 2 3], ones(2,4)}, 2);");
    EXPECT_DOUBLE_EQ(evalScalar("sz(1)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("sz(2)"), 4.0);

    eval("ic = cellfun('isclass', {1, int8(2), 'str'}, 'double');");
    EXPECT_DOUBLE_EQ(evalScalar("ic(1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("ic(2)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("ic(3)"), 0.0);
}

// An unrecognised string name is rejected (use a function handle instead).
TEST_F(CellfunInputFormsTest, UnknownStringNameThrows)
{
    EXPECT_ANY_THROW(eval("cellfun('sin', {1,2});"));
}

// Single-cell forms are unchanged.
TEST_F(CellfunInputFormsTest, SingleCellUnchanged)
{
    eval("r = cellfun(@(x) x*2, {1,2,3});");
    EXPECT_DOUBLE_EQ(evalScalar("r(2)"), 4.0);
    eval("n = cellfun(@numel, {[1 2],[1 2 3 4]});");   // builtin handle fast-path
    EXPECT_DOUBLE_EQ(evalScalar("n(1)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("n(2)"), 4.0);
}
