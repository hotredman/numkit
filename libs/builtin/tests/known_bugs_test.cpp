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

// bugs/builtin/unique-last.md — 'last' returns last-occurrence indices.
TEST_F(BuiltinKnownBug, DISABLED_UniqueLast)
{
    eval("[c, ia] = unique([3 1 2 1 3], 'last');");
    EXPECT_DOUBLE_EQ(evalScalar("ia(1)"), 4.0);   // value 1, last at idx 4
    EXPECT_DOUBLE_EQ(evalScalar("ia(2)"), 3.0);   // value 2, only at idx 3
    EXPECT_DOUBLE_EQ(evalScalar("ia(3)"), 5.0);   // value 3, last at idx 5
}

// bugs/builtin/max-all-linear.md — max over all + linear index.
TEST_F(BuiltinKnownBug, DISABLED_MaxAllLinear)
{
    eval("[m, i] = max([3 1; 4 1; 2 9], [], 'all', 'linear');");
    EXPECT_DOUBLE_EQ(evalScalar("m"), 9.0);
    EXPECT_DOUBLE_EQ(evalScalar("i"), 6.0);
}
