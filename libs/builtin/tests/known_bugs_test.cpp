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

// bugs/builtin/find-count-direction.md — FIXED (find(x,k[,'first'/'last'])).
// Live regression guard moved to libs/builtin/tests/find_count_direction_test.cpp.

// bugs/builtin/cumsum-complex.md — FIXED (cumsum/cumprod accumulate complex).
// Live regression guard moved to
// libs/builtin/tests/cumsum_cumprod_complex_test.cpp.

// bugs/builtin/diff-complex.md — FIXED (diff differences both parts).
// Live regression guard moved to libs/builtin/tests/diff_complex_test.cpp.

// bugs/builtin/diff-zero-order.md — diff(X,0) should error (MATLAB: "N must
// be a positive integer scalar"); numkit currently returns identity.
TEST_F(BuiltinKnownBug, DISABLED_DiffZeroOrderErrors)
{
    EXPECT_ANY_THROW(eval("diff([1 2 3], 0);"));
}

// bugs/builtin/complex-input-unsupported.md — FULLY FIXED 2026-06-05. All nine
// members (trapz/cumtrapz/median/interp1/gradient/movmean/detrend/conv/filter)
// accept complex now, each with its own live guard:
//   libs/builtin/tests/{trapz,cumtrapz,interp1,gradient}_complex_test.cpp,
//   libs/stats/tests/{median,movmean,detrend}_complex_test.cpp,
//   libs/signal/tests/{conv,filter}_complex_test.cpp.

// bugs/builtin/gradient-3d.md — gradient of an N-D (3-D) array. FIXED 2026-06-05
// (deep coverage in libs/builtin/tests/gradient_nd_test.cpp).
TEST_F(BuiltinKnownBug, Gradient3D)
{
    eval("A = reshape(1:8,2,2,2); [gx,gy,gz] = gradient(A);");  // MATLAB gz(1,1,1)=4
    EXPECT_NEAR(evalScalar("gz(1,1,1)"), 4.0, 1e-12);
    EXPECT_NEAR(evalScalar("gx(1,1,1)"), 2.0, 1e-12);
}

// bugs/builtin/acos-asin-complex.md — FIXED (acos/asin go complex for |x|>1).
// Live regression guard moved to libs/builtin/tests/acos_asin_complex_test.cpp.

// bugs/builtin/complex-promotion-arrays.md — FIXED (sqrt/acosh/atanh promote
// whole real arrays + atanh x<-1 branch sign). Live regression guard moved to
// libs/builtin/tests/complex_promotion_arrays_test.cpp.

// bugs/builtin/numerical-integration-nd.md — quadgk/integral2/integral3/quad2d.
TEST_F(BuiltinKnownBug, DISABLED_NumericalIntegrationND)
{
    EXPECT_NEAR(evalScalar("quadgk(@(x)exp(-x.^2),0,1)"), 0.746824132812427, 1e-9);
    EXPECT_NEAR(evalScalar("integral2(@(x,y)x.*y,0,1,0,1)"), 0.25, 1e-9);
    EXPECT_NEAR(evalScalar("integral3(@(x,y,z)x+y+z,0,1,0,1,0,1)"), 1.5, 1e-9);
    EXPECT_NEAR(evalScalar("quad2d(@(x,y)x.*y,0,1,0,1)"), 0.25, 1e-9);
}

// bugs/builtin/ode-stiff.md — ode15s (stiff/multistep solver family).
TEST_F(BuiltinKnownBug, DISABLED_Ode15s)
{
    eval("[t, y] = ode15s(@(t,y) -y, [0 1], 1);");   // y = e^{-t}
    EXPECT_NEAR(evalScalar("y(1)"),   1.0,          1e-9);
    EXPECT_NEAR(evalScalar("y(end)"), 0.367879441,  1e-2);   // e^-1, solver tol
}
