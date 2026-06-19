// toolboxes/builtin/tests/known_bugs_test.cpp
//
// One DISABLED_ test per OPEN bug in bugs/builtin/*.md. Disabled until
// fixed; remove `DISABLED_` to turn into a live regression guard.
// MATLAB R2025b reference values. (FIXED builtin bugs get real tests in
// their own files, e.g. sort-missingplacement -> matrix_test.cpp.)

#include <numkit/core/engine.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class BuiltinKnownBug : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// bugs/math/histcounts-autobinning.md — automatic bin selection (FIXED; live guard).
TEST_F(BuiltinKnownBug, HistcountsAutoBins)
{
    eval("[N, e] = histcounts([1 2 2 3 3 3]);");
    EXPECT_DOUBLE_EQ(evalScalar("N(1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("N(2)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("N(3)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("e(1)"), 0.5);
    EXPECT_DOUBLE_EQ(evalScalar("e(4)"), 3.5);
}

// bugs/math/histcounts-autobinning.md — explicit nbins form (FIXED; live guard).
TEST_F(BuiltinKnownBug, HistcountsNbins)
{
    eval("N = histcounts([1 2 3 4 5 6 7 8 9 10], 3);");
    EXPECT_DOUBLE_EQ(evalScalar("N(1)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("N(2)"), 4.0);
    EXPECT_DOUBLE_EQ(evalScalar("N(3)"), 3.0);
}

// bugs/lang/multi-output-handle-call.md — [a,b]=h(x) for a handle variable
// (FIXED 2026-06-19 via CALL_INDIRECT_MULTI; live guard). Named + user-fn
// handles now dispatch a multi-output indirect call.
TEST_F(BuiltinKnownBug, MultiOutputHandleCall)
{
    eval("h = @size; [r, c] = h(ones(2, 3));");
    EXPECT_DOUBLE_EQ(evalScalar("r"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("c"), 3.0);
}

// bugs/lang/anonymous-multi-output.md — varargout (dynamic-count returns)
// (FIXED core 2026-06-19 via RET_VARARGOUT; live guard). nargout drives the
// returned count; fixed + varargout mix; single-output.
TEST_F(BuiltinKnownBug, Varargout)
{
    eval("clear; function varargout = gen(n)\n"
         "  for k = 1:nargout, varargout{k} = k*10; end\n"
         "end\n"
         "[a, b, c] = gen(0);");
    EXPECT_DOUBLE_EQ(evalScalar("a"), 10.0);
    EXPECT_DOUBLE_EQ(evalScalar("b"), 20.0);
    EXPECT_DOUBLE_EQ(evalScalar("c"), 30.0);
    eval("function [first, varargout] = mixed(v)\n"
         "  first = v; varargout{1} = v*2; varargout{2} = v*3;\n"
         "end\n"
         "[p, q, r] = mixed(5);");
    EXPECT_DOUBLE_EQ(evalScalar("p"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("q"), 10.0);
    EXPECT_DOUBLE_EQ(evalScalar("r"), 15.0);
}

// bugs/lang/anonymous-multi-output.md — an anonymous call-body function now
// forwards nargout to its body (FIXED 2026-06-19; live guard). MATLAB: a=6,b=4.
TEST_F(BuiltinKnownBug, AnonymousMultiOutput)
{
    eval("h = @(x) deal(x+1, x-1); [a, b] = h(5);");
    EXPECT_DOUBLE_EQ(evalScalar("a"), 6.0);
    EXPECT_DOUBLE_EQ(evalScalar("b"), 4.0);
    // single-output still works, and captures forward too.
    EXPECT_DOUBLE_EQ(evalScalar("h(5)"), 6.0);
    eval("k = 100; m = @(x) deal(x+k, x-k); [p, q] = m(5);");
    EXPECT_DOUBLE_EQ(evalScalar("p"), 105.0);
    EXPECT_DOUBLE_EQ(evalScalar("q"), -95.0);
}

// bugs/builtin/unique-last.md — sorted-order 'last' FIXED; live tests in
// toolboxes/builtin/tests/unique_last_test.cpp. Remaining sub-gap: 'stable'+'last'
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
// FIXED 2026-06-05 (deep coverage in toolboxes/builtin/tests/cellfun_inputforms_test.cpp).
TEST_F(BuiltinKnownBug, CellfunMultiCell)
{
    eval("r = cellfun(@(a,b) a+b, {1,2}, {10,20});");   // MATLAB: [11 22]
    EXPECT_DOUBLE_EQ(evalScalar("r(1)"), 11.0);
    EXPECT_DOUBLE_EQ(evalScalar("r(2)"), 22.0);
}

TEST_F(BuiltinKnownBug, CellfunStringName)
{
    eval("r = cellfun('isempty', {[], [1], []});");     // MATLAB: [1 0 1]
    EXPECT_DOUBLE_EQ(evalScalar("r(1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("r(2)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("r(3)"), 1.0);
}

// bugs/runtime/func2str-anonymous.md — anon handle returns its source (FIXED; live guard).
TEST_F(BuiltinKnownBug, Func2StrAnonymous)
{
    eval("s = func2str(@(x) x + 1);");                  // MATLAB: '@(x)x+1'
    EXPECT_EQ(eval("s").toString(), std::string("@(x)x+1"));
}

// bugs/builtin/find-count-direction.md — FIXED (find(x,k[,'first'/'last'])).
// Live regression guard moved to toolboxes/builtin/tests/find_count_direction_test.cpp.

// bugs/builtin/cumsum-complex.md — FIXED (cumsum/cumprod accumulate complex).
// Live regression guard moved to
// toolboxes/builtin/tests/cumsum_cumprod_complex_test.cpp.

// bugs/builtin/diff-complex.md — FIXED (diff differences both parts).
// Live regression guard moved to toolboxes/builtin/tests/diff_complex_test.cpp.

// bugs/builtin/diff-zero-order.md — diff(X,0) errors (MATLAB: "Difference
// order N must be a positive integer scalar"). FIXED 2026-06-05 (deep coverage
// in toolboxes/builtin/tests/diff_order_test.cpp).
TEST_F(BuiltinKnownBug, DiffZeroOrderErrors)
{
    EXPECT_ANY_THROW(eval("diff([1 2 3], 0);"));
}

// bugs/builtin/complex-input-unsupported.md — FULLY FIXED 2026-06-05. All nine
// members (trapz/cumtrapz/median/interp1/gradient/movmean/detrend/conv/filter)
// accept complex now, each with its own live guard:
//   toolboxes/builtin/tests/{trapz,cumtrapz,interp1,gradient}_complex_test.cpp,
//   toolboxes/stats/tests/{median,movmean,detrend}_complex_test.cpp,
//   toolboxes/signal/tests/{conv,filter}_complex_test.cpp.

// bugs/builtin/gradient-3d.md — gradient of an N-D (3-D) array. FIXED 2026-06-05
// (deep coverage in toolboxes/builtin/tests/gradient_nd_test.cpp).
TEST_F(BuiltinKnownBug, Gradient3D)
{
    eval("A = reshape(1:8,2,2,2); [gx,gy,gz] = gradient(A);");  // MATLAB gz(1,1,1)=4
    EXPECT_NEAR(evalScalar("gz(1,1,1)"), 4.0, 1e-12);
    EXPECT_NEAR(evalScalar("gx(1,1,1)"), 2.0, 1e-12);
}

// bugs/builtin/acos-asin-complex.md — FIXED (acos/asin go complex for |x|>1).
// Live regression guard moved to toolboxes/builtin/tests/acos_asin_complex_test.cpp.

// bugs/builtin/complex-promotion-arrays.md — FIXED (sqrt/acosh/atanh promote
// whole real arrays + atanh x<-1 branch sign). Live regression guard moved to
// toolboxes/builtin/tests/complex_promotion_arrays_test.cpp.

// bugs/builtin/numerical-integration-nd.md — quadgk/integral2/integral3/quad2d.
// FIXED 2026-06-19 (nested adaptive integral) — promoted live.
TEST_F(BuiltinKnownBug, NumericalIntegrationND)
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

// bugs/builtin/cumulative-logical.md — FIXED (cumsum/cumprod/cummax/cummin
// accept logical: cumsum/cumprod -> double, cummax/cummin -> logical).
// Full guard in toolboxes/builtin/tests/cumulative_logical_test.cpp; this is the
// flipped-live known-bug sentinel.
TEST_F(BuiltinKnownBug, CumulativeLogical)
{
    eval("s = cumsum(logical([1 0 1 1]));");        // -> double [1 1 2 3]
    EXPECT_DOUBLE_EQ(evalScalar("s(4)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("islogical(s)"), 0.0);
    eval("m = cummax(logical([0 1 0 1]));");        // -> logical [0 1 1 1]
    EXPECT_DOUBLE_EQ(evalScalar("m(1)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("m(2)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("islogical(m)"), 1.0);
}

// bugs/builtin/trapz-logical.md — FIXED (trapz promotes logical X/Y -> double).
// Full guard in toolboxes/builtin/tests/trapz_logical_test.cpp.
TEST_F(BuiltinKnownBug, TrapzLogical)
{
    EXPECT_DOUBLE_EQ(evalScalar("trapz(logical([1 0 1 1]))"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("trapz([1 3 4 7], logical([1 0 1 1]))"), 4.5);
}

// bugs/builtin/sort-logical.md — FIXED (sort preserves the logical class on
// values; index stays double). Full guard in sort_logical_test.cpp.
TEST_F(BuiltinKnownBug, SortLogical)
{
    eval("[S, I] = sort(logical([0 1 0 1]));");
    EXPECT_DOUBLE_EQ(evalScalar("S(2)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("S(3)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("islogical(S)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("islogical(I)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("I(2)"), 3.0);
}

// bugs/builtin/sort-char.md — FIXED (sort sorts char by code point, preserving
// the char class; index stays double). Full guard in sort_char_test.cpp.
TEST_F(BuiltinKnownBug, SortChar)
{
    eval("[S, I] = sort('dcba');");
    EXPECT_DOUBLE_EQ(evalScalar("isequal(S,'abcd')"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("ischar(S)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("islogical(I)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("I(1)"), 4.0);
}

// bugs/builtin/unique-typeclass.md — FIXED (unique preserves the input class on
// values: char/logical/int; ia/ic stay double). Full guard in
// unique_typeclass_test.cpp.
TEST_F(BuiltinKnownBug, UniqueTypeClass)
{
    eval("uc = unique('cbabc');");
    EXPECT_DOUBLE_EQ(evalScalar("isequal(uc,'abc')"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("ischar(uc)"), 1.0);
    eval("ul = unique(logical([1 0 1 1]));");
    EXPECT_DOUBLE_EQ(evalScalar("islogical(ul)"), 1.0);
    eval("uj = unique(int8([3 1 3 2]));");
    EXPECT_DOUBLE_EQ(evalScalar("isa(uj,'int8')"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("uj(3)"), 3.0);
}

// bugs/builtin/setops-typeclass.md — FIXED (ismember/intersect/setdiff/union
// accept char/logical/int; intersect/setdiff/union preserve class on values,
// ismember -> logical tf + double loc). Full guard in setops_typeclass_test.cpp.
TEST_F(BuiltinKnownBug, SetopsTypeClass)
{
    EXPECT_DOUBLE_EQ(evalScalar("ismember('b','abcd')"), 1.0);
    eval("c = intersect('cabc','bdc');");
    EXPECT_DOUBLE_EQ(evalScalar("isequal(c,'bc')"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("ischar(c)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("isequal(setdiff('abce','bd'),'ace')"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("isequal(union('ab','bc'),'abc')"), 1.0);
    eval("ci = intersect(int8([3 1 2]),int8([2 4 1]));");
    EXPECT_DOUBLE_EQ(evalScalar("isa(ci,'int8')"), 1.0);
}

// bugs/builtin/cummax-cummin-integer.md — FIXED (cummax/cummin preserve the
// integer class). Full guard in cummax_cummin_integer_test.cpp.
TEST_F(BuiltinKnownBug, CummaxCumminInteger)
{
    eval("a = cummax(int8([3 1 2 5 4]));");
    EXPECT_DOUBLE_EQ(evalScalar("isa(a,'int8')"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("a(3)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("a(4)"), 5.0);
    eval("b = cummin(int8([3 1 2 5 4]));");
    EXPECT_DOUBLE_EQ(evalScalar("isa(b,'int8')"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("b(2)"), 1.0);
}

// bugs/builtin/sprintf-complex.md — FIXED (sprintf/fprintf use the real part of
// a complex argument). Full guard in sprintf_complex_test.cpp.
TEST_F(BuiltinKnownBug, SprintfComplex)
{
    EXPECT_EQ(eval("sprintf('%g', 1+2i)").toString(), "1");
    EXPECT_EQ(eval("sprintf('%d ', [1+2i 3+4i])").toString(), "1 3 ");
}

// bugs/builtin/maxmin-char-double.md — FIXED (max/min of char return double
// code points; mode keeps char). Full guard in maxmin_char_double_test.cpp.
TEST_F(BuiltinKnownBug, MaxMinCharDouble)
{
    EXPECT_DOUBLE_EQ(evalScalar("max('abc')"), 99.0);
    EXPECT_DOUBLE_EQ(evalScalar("ischar(max('abc'))"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("min('abc')"), 97.0);
    EXPECT_DOUBLE_EQ(evalScalar("ischar(mode('abc'))"), 1.0);   // mode keeps char
}

// bugs/builtin/gamma-negative-integer-poles.md — FIXED (gamma returns +Inf at
// non-positive integer poles, not NaN). Dedicated guard in special_funcs_test.cpp.
TEST_F(BuiltinKnownBug, GammaNegativeIntegerPoles)
{
    eval("g = gamma([-1 -2 -3 0 -0.5 5]);");
    EXPECT_DOUBLE_EQ(evalScalar("isinf(g(1))"), 1.0);   // -1  -> Inf
    EXPECT_DOUBLE_EQ(evalScalar("isinf(g(3))"), 1.0);   // -3  -> Inf
    EXPECT_DOUBLE_EQ(evalScalar("g(1)>0"), 1.0);        // +Inf
    EXPECT_NEAR(evalScalar("g(5)"), -3.5449077, 1e-6);  // -0.5 unchanged
    EXPECT_DOUBLE_EQ(evalScalar("g(6)"), 24.0);         // 5! = 24
    EXPECT_DOUBLE_EQ(evalScalar("isinf(gamma(-Inf))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("isnan(gamma(NaN))"), 1.0);
}

// bugs/builtin/polyder-product.md — FIXED (polyder(a,b) single output = product
// derivative, not the quotient numerator). Dedicated guard in poly_test.cpp.
TEST_F(BuiltinKnownBug, PolyderProduct)
{
    eval("d = polyder([1 0], [1 1]);");   // d/dx[x*(x+1)] = 2x+1 -> [2 1]
    EXPECT_DOUBLE_EQ(evalScalar("d(1)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("d(2)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("numel(d)"), 2.0);
}

// bugs/builtin/psi-zero-pole.md — FIXED (psi(0) = -Inf, was NaN). Dedicated
// guard in special_funcs_test.cpp.
TEST_F(BuiltinKnownBug, PsiZeroPole)
{
    EXPECT_DOUBLE_EQ(evalScalar("isinf(psi(0))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("psi(0) < 0"), 1.0);   // -Inf
    EXPECT_NEAR(evalScalar("psi(1)"), -0.5772156649, 1e-9);
}

// bugs/builtin/str2double-complex.md — FIXED (str2double parses complex
// strings). Full guard in str2double_complex_test.cpp.
TEST_F(BuiltinKnownBug, Str2doubleComplex)
{
    EXPECT_DOUBLE_EQ(evalScalar("real(str2double('1+2i'))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("imag(str2double('1+2i'))"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("imag(str2double('2i'))"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("imag(str2double('i'))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("imag(str2double('1+2j'))"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("str2double('1.5')"), 1.5);   // real path unchanged
    EXPECT_DOUBLE_EQ(evalScalar("isreal(str2double('5'))"), 1.0);
}

// bugs/builtin/concat-integer-types.md — FIXED (core promoteNumericType now
// concatenates integers, preserving the class). Full guard in
// concat_integer_types_test.cpp.
TEST_F(BuiltinKnownBug, ConcatIntegerTypes)
{
    eval("a = cat(1, int8([1 2]), int8([3 4]));");   // MATLAB: int8 [1 2; 3 4]
    EXPECT_DOUBLE_EQ(evalScalar("isa(a,'int8')"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("a(2,1)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("a(1,2)"), 2.0);
    eval("b = [int8([1 2]), int8([3 4])];");         // MATLAB: int8 [1 2 3 4]
    EXPECT_DOUBLE_EQ(evalScalar("isa(b,'int8')"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("numel(b)"), 4.0);
}

// bugs/builtin/accumarray-integer-vals.md — accumarray accepts integer/logical
// vals (was: throw "vals must be DOUBLE"). Output class follows the reducer:
// sum/prod/mean -> double, max/min preserve the integer class. FIXED
// 2026-06-05; deep coverage in accumarray_integer_vals_test.cpp.
TEST_F(BuiltinKnownBug, AccumarrayIntegerVals)
{
    eval("s = accumarray([1;2;1], int8([10;20;30]));");
    EXPECT_DOUBLE_EQ(evalScalar("isa(s,'double')"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("s(1)"), 40.0);
    eval("mx = accumarray([1;2;1], int8([100;100;30]), [], @max);");
    EXPECT_DOUBLE_EQ(evalScalar("isa(mx,'int8')"), 1.0);   // max preserves int class
    EXPECT_DOUBLE_EQ(evalScalar("double(mx(1))"), 100.0);
    eval("lg = accumarray([1;2;1], logical([1;0;1]));");
    EXPECT_DOUBLE_EQ(evalScalar("isa(lg,'double')"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("lg(1)"), 2.0);
}

// bugs/math/interpn-nan.md — interpn 1-D grid-vector query (was NaN; FIXED:
// 1-D forms delegate to interp1). Live regression guard. 4+-D still a parity gap.
TEST_F(BuiltinKnownBug, InterpnOneDimNaN)
{
    // MATLAB: interpn([1 2 3],[1 4 9],2.5) = 6.5 (linear).
    EXPECT_NEAR(evalScalar("interpn([1 2 3], [1 4 9], 2.5)"), 6.5, 1e-12);
}
