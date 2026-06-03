// libs/builtin/tests/known_bugs_test.cpp
//
// One DISABLED_ test per OPEN bug in bugs/builtin/*.md. Disabled until
// fixed; remove `DISABLED_` to turn into a live regression guard.
// MATLAB R2025b reference values. (FIXED builtin bugs get real tests in
// their own files, e.g. sort-missingplacement -> matrix_test.cpp.)

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class BuiltinKnownBug : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// bugs/builtin/histcounts-autobinning.md — automatic bin selection.
TEST_F(BuiltinKnownBug, DISABLED_HistcountsAutoBins)
{
    eval("[N, e] = histcounts([1 2 2 3 3 3]);");
    EXPECT_DOUBLE_EQ(evalScalar("N(1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("N(2)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("N(3)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("e(1)"), 0.5);
    EXPECT_DOUBLE_EQ(evalScalar("e(4)"), 3.5);
}

// bugs/builtin/histcounts-autobinning.md — explicit nbins form.
TEST_F(BuiltinKnownBug, DISABLED_HistcountsNbins)
{
    eval("N = histcounts([1 2 3 4 5 6 7 8 9 10], 3);");
    EXPECT_DOUBLE_EQ(evalScalar("N(1)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("N(2)"), 4.0);
    EXPECT_DOUBLE_EQ(evalScalar("N(3)"), 3.0);
}

// bugs/builtin/unique-last.md — sorted-order 'last' FIXED; live tests in
// libs/builtin/tests/unique_last_test.cpp. Remaining sub-gap: 'stable'+'last'
// ORDERING (MATLAB orders unique values by their last occurrence).
TEST_F(BuiltinKnownBug, DISABLED_UniqueStableLast)
{
    // MATLAB: unique([3 1 2 1 3],'stable','last') -> C=[2 1 3], ia=[3 4 5].
    eval("[c, ia] = unique([3 1 2 1 3], 'stable', 'last');");
    EXPECT_DOUBLE_EQ(evalScalar("ia(1)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("ia(2)"), 4.0);
    EXPECT_DOUBLE_EQ(evalScalar("ia(3)"), 5.0);
}

// bugs/builtin/max-all-linear.md — FIXED (max/min over 'all'); the live
// test is MathReductionsBatchTest.MaxMinAll in math_reductions_batch_test.cpp.

// bugs/builtin/cellfun-inputforms.md — multiple cell arrays + string name.
TEST_F(BuiltinKnownBug, DISABLED_CellfunMultiCell)
{
    eval("r = cellfun(@(a,b) a+b, {1,2}, {10,20});");   // MATLAB: [11 22]
    EXPECT_DOUBLE_EQ(evalScalar("r(1)"), 11.0);
    EXPECT_DOUBLE_EQ(evalScalar("r(2)"), 22.0);
}

TEST_F(BuiltinKnownBug, DISABLED_CellfunStringName)
{
    eval("r = cellfun('isempty', {[], [1], []});");     // MATLAB: [1 0 1]
    EXPECT_DOUBLE_EQ(evalScalar("r(1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("r(2)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("r(3)"), 1.0);
}

// bugs/builtin/func2str-anonymous.md — anon handle should return its source.
TEST_F(BuiltinKnownBug, DISABLED_Func2StrAnonymous)
{
    eval("s = func2str(@(x) x + 1);");                  // MATLAB: '@(x)x+1'
    EXPECT_EQ(eval("s").toString(), std::string("@(x)x+1"));
}

// bugs/builtin/find-count-direction.md — find(x,k[,'first'/'last']).
TEST_F(BuiltinKnownBug, DISABLED_FindCountDirection)
{
    eval("a = find([0 1 0 1 1], 2);");           // MATLAB: [2 4]
    EXPECT_EQ(static_cast<int>(evalScalar("numel(a)")), 2);
    EXPECT_DOUBLE_EQ(evalScalar("a(2)"), 4.0);
    eval("b = find([0 1 0 1 1], 1, 'last');");   // MATLAB: 5
    EXPECT_EQ(static_cast<int>(evalScalar("numel(b)")), 1);
    EXPECT_DOUBLE_EQ(evalScalar("b"), 5.0);
    eval("c = find([0 1 0 1 1], 2, 'last');");   // MATLAB: [4 5]
    EXPECT_DOUBLE_EQ(evalScalar("c(1)"), 4.0);
    EXPECT_DOUBLE_EQ(evalScalar("c(2)"), 5.0);
}
