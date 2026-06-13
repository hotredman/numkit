// toolboxes/stats/tests/spline_family_test.cpp
//
// gtest coverage for the curve-fit / B-spline subsystem, which shipped
// parity-only: the pp-form splines (csapi / ppmak / fnval / fnder / fnint /
// fnbrk / fncmb), the knot utilities (augknt / aveknt / brk2knt / knt2brk),
// and the data-prep helpers (prepareCurveData / prepareSurfaceData).
//
// A not-a-knot cubic reproduces any cubic (hence any quadratic) exactly, so
// csapi on y = x^2 lets us assert exact interpolated values. Reference values
// verified against the engine.

#include "dual_engine_fixture.hpp"

using namespace m_test;

class SplineFamilyTest : public DualEngineTest
{};

// ── csapi + fnbrk + fnval: not-a-knot cubic reproduces x^2 exactly ──────
TEST_P(SplineFamilyTest, CsapiReproducesQuadratic)
{
    eval("pp = csapi([0 1 2 3 4], [0 1 4 9 16]);");   // y = x^2
    EXPECT_DOUBLE_EQ(evalScalar("strcmp(fnbrk(pp, 'form'), 'pp')"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("fnbrk(pp, 'pieces')"), 4.0);
    EXPECT_DOUBLE_EQ(evalScalar("fnbrk(pp, 'order')"), 4.0);
    EXPECT_DOUBLE_EQ(evalScalar("fnbrk(pp, 'dim')"), 1.0);
    EXPECT_NEAR(evalScalar("fnval(pp, 2.5)"), 6.25, 1e-10);    // 2.5^2
    EXPECT_NEAR(evalScalar("fnval(pp, 2)"), 4.0, 1e-10);
    EXPECT_NEAR(evalScalar("fnval(pp, 3.5)"), 12.25, 1e-10);
}

// ── ppmak + fnval on an explicit linear pp (2x+1 on [0,3]) ──────────────
TEST_P(SplineFamilyTest, PpmakFnvalLinear)
{
    eval("pp = ppmak([0 3], [2 1], 1);");             // one piece, order 2
    EXPECT_NEAR(evalScalar("fnval(pp, 1.5)"), 4.0, 1e-12);   // 2*1.5+1
    EXPECT_NEAR(evalScalar("fnval(pp, 0)"), 1.0, 1e-12);
}

// ── fnder / fnint: d/dx(2x+1)=2, integral(2x+1) from 0 = x^2+x ───────────
TEST_P(SplineFamilyTest, FnderFnint)
{
    eval("pp = ppmak([0 3], [2 1], 1);");
    EXPECT_NEAR(evalScalar("fnval(fnder(pp, 1), 1.0)"), 2.0, 1e-12);   // constant slope
    EXPECT_NEAR(evalScalar("fnval(fnder(pp, 1), 2.7)"), 2.0, 1e-12);
    EXPECT_NEAR(evalScalar("fnval(fnint(pp), 1.0)"), 2.0, 1e-12);      // 1^2+1
    EXPECT_NEAR(evalScalar("fnval(fnint(pp), 0.0)"), 0.0, 1e-12);      // const => 0 at first break
}

// ── fncmb: scalar multiply (4-arg combine path tested elsewhere) ────────
TEST_P(SplineFamilyTest, FncmbScalarMultiply)
{
    eval("pp = ppmak([0 3], [2 1], 1); q = fncmb(pp, 10);");   // 10*(2x+1)
    EXPECT_NEAR(evalScalar("fnval(q, 1.0)"), 30.0, 1e-12);
}

// ── augknt / aveknt: open knot vector + Greville sites ──────────────────
TEST_P(SplineFamilyTest, AugkntAveknt)
{
    eval("a = augknt([0 1 2 3], 3);");                // endpoints to multiplicity 3
    EXPECT_EQ(eval("a").numel(), 8u);
    EXPECT_DOUBLE_EQ(evalScalar("a(1)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("a(3)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("a(4)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("a(8)"), 3.0);
    eval("v = aveknt([0 0 0 1 2 3 3 3], 3);");        // Greville averages
    EXPECT_EQ(eval("v").numel(), 5u);
    EXPECT_DOUBLE_EQ(evalScalar("v(1)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("v(2)"), 0.5);
    EXPECT_DOUBLE_EQ(evalScalar("v(3)"), 1.5);
    EXPECT_DOUBLE_EQ(evalScalar("v(5)"), 3.0);
}

// ── brk2knt / knt2brk: round-trip between breaks+mults and knots ────────
TEST_P(SplineFamilyTest, Brk2kntKnt2brk)
{
    eval("t = brk2knt([0 1 2], [2 1 3]);");           // [0 0 1 2 2 2]
    EXPECT_EQ(eval("t").numel(), 6u);
    EXPECT_DOUBLE_EQ(evalScalar("t(2)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("t(3)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("t(6)"), 2.0);
    eval("[b, m] = knt2brk([0 0 1 2 2 2]);");
    EXPECT_DOUBLE_EQ(evalScalar("b(1)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("b(3)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("m(1)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("m(2)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("m(3)"), 3.0);
}

// ── prepareCurveData: drop NaN/Inf rows, return columns (order kept) ─────
TEST_P(SplineFamilyTest, PrepareCurveData)
{
    eval("[x, y] = prepareCurveData([3 1 2 NaN 4], [9 1 4 99 16]);");
    EXPECT_EQ(eval("x").numel(), 4u);
    EXPECT_EQ(static_cast<int>(evalScalar("size(x, 2)")), 1);   // column
    EXPECT_DOUBLE_EQ(evalScalar("x(1)"), 3.0);                  // order preserved
    EXPECT_DOUBLE_EQ(evalScalar("x(4)"), 4.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(1)"), 9.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(4)"), 16.0);                 // NaN row dropped
}

// ── prepareSurfaceData: meshgrid -> column-major vectors ────────────────
TEST_P(SplineFamilyTest, PrepareSurfaceData)
{
    eval("[X, Y] = meshgrid(1:3, 1:2); Z = X + 10*Y;");
    eval("[x, y, z] = prepareSurfaceData(X, Y, Z);");
    EXPECT_EQ(eval("x").numel(), 6u);
    EXPECT_EQ(static_cast<int>(evalScalar("size(z, 2)")), 1);   // column
    EXPECT_DOUBLE_EQ(evalScalar("x(1)"), 1.0);                  // X(:) col-major
    EXPECT_DOUBLE_EQ(evalScalar("x(3)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("z(1)"), 11.0);                 // Z(:) col-major
    EXPECT_DOUBLE_EQ(evalScalar("z(2)"), 21.0);
}

INSTANTIATE_DUAL(SplineFamilyTest);
