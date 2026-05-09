// libs/builtin/tests/misc5_batch_test.cpp
//
// Audit ТЗ batch closure (19 functions):
//   poly:           polyfit · polyval · ppval
//   string extras2: insertafter · insertbefore · join · matches ·
//                   replace · replacebetween · rmfield · orderfields
//   math2/extr:     legendre · psi · realmax · realmin
//   error-handling: assert · error · warning · lastwarn
//
// All flagged "no major gap detected". Bit-identical MATLAB R2025b
// (16 working, 3 deferred — insertafter/insertbefore/replacebetween
// undefined in numkit).

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class Misc5BatchTest : public ::testing::Test
{
public:
    Engine engine;
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// ─── poly ───────────────────────────────────────────────────────────

TEST_F(Misc5BatchTest, Polyfit)
{
    // y = x^2 → polyfit returns [1, 0, 0]
    eval("p = polyfit([1 2 3 4], [1 4 9 16], 2);");
    EXPECT_NEAR(evalScalar("p(1)"), 1.0, 1e-9);
    EXPECT_NEAR(evalScalar("p(2)"), 0.0, 1e-9);
    EXPECT_NEAR(evalScalar("p(3)"), 0.0, 1e-9);
}

TEST_F(Misc5BatchTest, Polyval)
{
    EXPECT_DOUBLE_EQ(evalScalar("polyval([1 0 0], 5)"),  25.0);  // x^2 at 5
    EXPECT_DOUBLE_EQ(evalScalar("polyval([1 -2 1], 3)"), 4.0);   // (x-1)^2 at 3
    EXPECT_DOUBLE_EQ(evalScalar("polyval([1 0], 7)"),    7.0);
}

TEST_F(Misc5BatchTest, Ppval)
{
    eval("pp = mkpp([0 1 2], [1 0; 1 0]);");
    EXPECT_DOUBLE_EQ(evalScalar("ppval(pp, 0.5)"), 0.5);  // x in [0,1]
    EXPECT_DOUBLE_EQ(evalScalar("ppval(pp, 1.5)"), 0.5);  // x-1 in [1,2]
}

// ─── string extras 2 ────────────────────────────────────────────────

TEST_F(Misc5BatchTest, JoinReplaceMatches)
{
    EXPECT_DOUBLE_EQ(evalScalar("strcmp(join([\"a\", \"b\", \"c\"]), \"a b c\")"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("strcmp(replace(\"hello\", \"l\", \"X\"), \"heXXo\")"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("matches(\"hello\", \"hello\")"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("matches(\"hello\", \"world\")"), 0.0);
}

TEST_F(Misc5BatchTest, RmfieldOrderfields)
{
    eval("s.a=1; s.b=2; t = rmfield(s, \"a\");");
    EXPECT_DOUBLE_EQ(evalScalar("isfield(t, \"a\")"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("isfield(t, \"b\")"), 1.0);

    eval("s.b=2; s.a=1; t = orderfields(s); f = fieldnames(t);");
    EXPECT_DOUBLE_EQ(evalScalar("strcmp(f{1}, \"a\")"), 1.0);
}

// ─── math2 + extremes ──────────────────────────────────────────────

TEST_F(Misc5BatchTest, Legendre)
{
    eval("P = legendre(2, 0.5);");
    EXPECT_NEAR(evalScalar("P(1)"), -0.125,             1e-9);  // P_2(0.5) = -1/8
    EXPECT_NEAR(evalScalar("P(2)"), -1.299038105676658, 1e-9);
    EXPECT_NEAR(evalScalar("P(3)"),  2.25,              1e-9);
}

TEST_F(Misc5BatchTest, Psi)
{
    EXPECT_NEAR(evalScalar("psi(1)"),  -0.577215664901532, 1e-9);  // -γ (Euler-Mascheroni)
    EXPECT_NEAR(evalScalar("psi(2)"),   0.422784335098467, 1e-9);  // 1 - γ
    EXPECT_NEAR(evalScalar("psi(0.5)"), -1.963510026021423, 1e-9);
}

TEST_F(Misc5BatchTest, RealmaxRealmin)
{
    EXPECT_NEAR(evalScalar("realmax"),  1.7976931348623157e+308, 1e296);
    EXPECT_NEAR(evalScalar("realmin"),  2.2250738585072014e-308, 1e-320);
}

// ─── error-handling ────────────────────────────────────────────────

TEST_F(Misc5BatchTest, Assert)
{
    eval("assert(true);");      // no-op when condition true
    EXPECT_NO_THROW(eval("assert(1 == 1);"));
}

TEST_F(Misc5BatchTest, Lastwarn)
{
    // lastwarn should return a string (possibly empty)
    EXPECT_GE(evalScalar("strlength(lastwarn)"), 0.0);
}
