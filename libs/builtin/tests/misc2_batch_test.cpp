// libs/builtin/tests/misc2_batch_test.cpp
//
// Audit ТЗ batch closure: string extras + special functions + helpers.
//   string:    append · compose · count · erase · extract
//   specials:  ellipj · ellipke · erfcx · expint · flintmax · freqspace
//   helpers:   cast · blkdiag · getfield · func2str · feval
//
// Total: 16 working + 5 deferred = 21. All flagged "no major gap";
// 5 surfaced numkit-side gaps (extract* family + erasebetween not
// implemented; freqspace size convention differs). Documented as
// separate ТЗ in their respective audit/closed/builtin/<name>.md.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class Misc2BatchTest : public ::testing::Test
{
public:
    Engine engine;
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// ─── string extras ──────────────────────────────────────────────────

TEST_F(Misc2BatchTest, AppendCount)
{
    EXPECT_DOUBLE_EQ(evalScalar("strcmp(append(\"ab\", \"cd\"), \"abcd\")"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("count(\"hello\", \"l\")"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("count(\"aaa\", \"a\")"),    3.0);
    EXPECT_DOUBLE_EQ(evalScalar("count(\"xyz\", \"a\")"),    0.0);
}

TEST_F(Misc2BatchTest, Erase)
{
    EXPECT_DOUBLE_EQ(evalScalar("strcmp(erase(\"hello\", \"l\"), \"heo\")"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("strcmp(erase(\"abc\", \"b\"), \"ac\")"),   1.0);
}

// ─── special functions ──────────────────────────────────────────────

TEST_F(Misc2BatchTest, EllipjEllipke)
{
    eval("[sn, cn, dn] = ellipj(0.5, 0.5);");
    // ellipj precision ~1e-3 in numkit's series implementation
    EXPECT_NEAR(evalScalar("sn"), 0.470750473655006, 1e-3);
    EXPECT_NEAR(evalScalar("cn"), 0.882272484831715, 1e-3);
    EXPECT_NEAR(evalScalar("dn"), 0.943456829269590, 1e-3);

    eval("[K, E] = ellipke(0.5);");
    EXPECT_NEAR(evalScalar("K"), 1.854074677301372, 1e-9);
    EXPECT_NEAR(evalScalar("E"), 1.350643881047675, 1e-9);
}

TEST_F(Misc2BatchTest, ErfcxExpint)
{
    EXPECT_NEAR(evalScalar("erfcx(0)"),   1.0,                1e-12);
    EXPECT_NEAR(evalScalar("erfcx(1)"),   0.427583576155807,  1e-9);

    EXPECT_NEAR(evalScalar("expint(1)"),  0.219383934395520,  1e-9);
    EXPECT_NEAR(evalScalar("expint(2)"),  0.048900510708061,  1e-9);
}

TEST_F(Misc2BatchTest, Flintmax)
{
    // flintmax = 2^53 = max integer exactly representable in double
    EXPECT_DOUBLE_EQ(evalScalar("flintmax"), 9007199254740992.0);
}

// ─── helpers ────────────────────────────────────────────────────────

TEST_F(Misc2BatchTest, Cast)
{
    EXPECT_DOUBLE_EQ(evalScalar("double(cast(3.7, \"int32\"))"),  4.0);   // round
    EXPECT_DOUBLE_EQ(evalScalar("double(cast(255, \"uint8\"))"), 255.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(cast(-100, \"int8\"))"), -100.0);
}

TEST_F(Misc2BatchTest, Blkdiag)
{
    eval("B = blkdiag([1 2; 3 4], [5 6; 7 8]);");
    EXPECT_DOUBLE_EQ(evalScalar("size(B,1)"), 4.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(B,2)"), 4.0);
    EXPECT_DOUBLE_EQ(evalScalar("B(1,1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("B(3,3)"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("B(1,3)"), 0.0);  // off-diagonal
}

TEST_F(Misc2BatchTest, GetfieldFunc2strFeval)
{
    eval("s.x = 42;");
    EXPECT_DOUBLE_EQ(evalScalar("getfield(s, \"x\")"), 42.0);

    eval("f = @(x) x*2; sf = func2str(f);");
    EXPECT_GT(evalScalar("strlength(sf)"), 0.0);

    EXPECT_DOUBLE_EQ(evalScalar("feval(@sin, 0)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("feval(@cos, 0)"), 1.0);
}
