// toolboxes/control/tests/lti_introspection_test.cpp
//
// Coverage for the parity-only LTI introspection / data-extraction helpers:
//   isct isdt issiso isproper isstatic order   (boolean / scalar queries)
//   ssdata zpkdata frdata                        (model-data extraction)
//   pzmap tzero margin rlocus                    (poles/zeros/margins/locus)
//   d2c                                          (discrete -> continuous)
// Plants are simple enough that poles/zeros/margins are known in closed form
// (margin of 1/(s(s+1)^2): Gm=2 at Wcg=1); other values verified against the
// engine. DualEngineTest (TW + VM).

#include "dual_engine_fixture.hpp"

using namespace m_test;

class LtiIntrospectionTest : public DualEngineTest
{};

// ── scalar / boolean queries ────────────────────────────────────────────
TEST_P(LtiIntrospectionTest, Queries)
{
    eval("sysc = tf(1, [1 2 1]);");                 // continuous 2nd order
    EXPECT_DOUBLE_EQ(evalScalar("isct(sysc)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("isdt(sysc)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("issiso(sysc)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("isproper(sysc)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("order(sysc)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("isstatic(sysc)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("isstatic(tf(5, 1))"), 1.0);   // pure gain
}

// ── d2c: discrete -> continuous round-trip ──────────────────────────────
TEST_P(LtiIntrospectionTest, D2c)
{
    eval("sysd = c2d(tf(1, [1 2 1]), 0.1);");
    EXPECT_DOUBLE_EQ(evalScalar("isdt(sysd)"), 1.0);
    eval("sysc2 = d2c(sysd);");
    EXPECT_DOUBLE_EQ(evalScalar("isct(sysc2)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("order(sysc2)"), 2.0);
}

// ── ssdata: extract [A,B,C,D] ───────────────────────────────────────────
TEST_P(LtiIntrospectionTest, Ssdata)
{
    eval("[A, B, C, D] = ssdata(ss([-2 0; 0 -3], [1; 1], [1 1], 0));");
    EXPECT_DOUBLE_EQ(evalScalar("A(1,1)"), -2.0);
    EXPECT_DOUBLE_EQ(evalScalar("A(2,2)"), -3.0);
    EXPECT_DOUBLE_EQ(evalScalar("B(1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("C(1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("D"), 0.0);
}

// ── zpkdata: cell outputs (SISO) + 'v' vector form ──────────────────────
TEST_P(LtiIntrospectionTest, Zpkdata)
{
    eval("[z, p, k] = zpkdata(zpk(-1, [-2 -3], 4));");
    EXPECT_DOUBLE_EQ(evalScalar("iscell(z)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("k"), 4.0);
    EXPECT_DOUBLE_EQ(evalScalar("z{1}(1)"), -1.0);
    EXPECT_DOUBLE_EQ(evalScalar("p{1}(1)"), -2.0);
    EXPECT_DOUBLE_EQ(evalScalar("p{1}(2)"), -3.0);
    // 'v' flag returns plain vectors.
    eval("[zv, pv, kv] = zpkdata(zpk(-1, [-2 -3], 4), 'v');");
    EXPECT_DOUBLE_EQ(evalScalar("zv(1)"), -1.0);
    EXPECT_DOUBLE_EQ(evalScalar("pv(2)"), -3.0);
    EXPECT_DOUBLE_EQ(evalScalar("kv"), 4.0);
}

// ── pzmap / tzero: poles & zeros of (s+1)/((s+1)(s+2)) ───────────────────
TEST_P(LtiIntrospectionTest, PzmapTzero)
{
    eval("[p, z] = pzmap(tf([1 1], [1 3 2]));");    // poles -1,-2; zero -1
    EXPECT_NEAR(evalScalar("min(real(p))"), -2.0, 1e-9);
    EXPECT_NEAR(evalScalar("max(real(p))"), -1.0, 1e-9);
    EXPECT_NEAR(evalScalar("z(1)"), -1.0, 1e-9);
    EXPECT_NEAR(evalScalar("tzero(tf([1 1], [1 3 2]))"), -1.0, 1e-9);
}

// ── margin: 1/(s(s+1)^2) has Gm=2 at Wcg=1 (numkit grid interp ~0.1%) ────
TEST_P(LtiIntrospectionTest, Margin)
{
    eval("[Gm, Pm, Wcg, Wcp] = margin(tf(1, [1 2 1 0]));");
    EXPECT_NEAR(evalScalar("Gm"), 2.0, 0.01);
    EXPECT_NEAR(evalScalar("Wcg"), 1.0, 0.01);
    EXPECT_NEAR(evalScalar("Pm"), 21.39, 0.1);
}

// ── frdata: response/frequency round-trip from a frd model ──────────────
TEST_P(LtiIntrospectionTest, Frdata)
{
    eval("sysf = frd([1+0i, 0.5-0.5i, 0.1], [0.1 1 10]);");
    eval("[resp, freq] = frdata(sysf);");
    EXPECT_EQ(eval("resp").numel(), 3u);
    EXPECT_DOUBLE_EQ(evalScalar("freq(1)"), 0.1);
    EXPECT_DOUBLE_EQ(evalScalar("freq(3)"), 10.0);
}

// ── rlocus: locus matrix (one column per closed-loop branch) ────────────
TEST_P(LtiIntrospectionTest, Rlocus)
{
    eval("r = rlocus(tf(1, [1 2 1]));");
    EXPECT_EQ(static_cast<int>(evalScalar("size(r,2)")), 2);   // 2 branches
    EXPECT_GT(evalScalar("size(r,1)"), 1.0);                   // multiple gains
}

INSTANTIATE_DUAL(LtiIntrospectionTest);
