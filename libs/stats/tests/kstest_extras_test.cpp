// libs/stats/tests/kstest_extras_test.cpp
// — Tail aliases
// (unequal/larger/smaller) and Name-Value parsing (Alpha=, Tail=).

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class KstestExtrasTest : public ::testing::Test
{
public:
    StdEngine engine;
    void SetUp() override
    {
        engine.eval("import compat.*;");
        engine.eval("x = [1.2 2.4 3.1 4.5 5.0 6.2 7.1]';");
        engine.eval("y = [0.8 1.9 2.7 4.0 4.5 5.7 6.4]';");
    }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// ── kstest2 ──────────────────────────────────────────────────────────

TEST_F(KstestExtrasTest, Kstest2DefaultBoth)
{
    eval("[h, p, D] = kstest2(x, y);");
    EXPECT_EQ(static_cast<int>(evalScalar("h")), 0);
    EXPECT_NEAR(evalScalar("D"), 0.142857, 1e-5);
}

TEST_F(KstestExtrasTest, Kstest2TailLarger)
{
    // 'larger' → Right one-sided D⁺
    eval("[h, p, D] = kstest2(x, y, 'Tail', 'larger');");
    EXPECT_EQ(static_cast<int>(evalScalar("h")), 0);
    EXPECT_DOUBLE_EQ(evalScalar("D"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("p"), 1.0);
}

TEST_F(KstestExtrasTest, Kstest2TailSmaller)
{
    eval("[h, p, D] = kstest2(x, y, 'Tail', 'smaller');");
    EXPECT_NEAR(evalScalar("D"), 0.142857, 1e-5);
}

TEST_F(KstestExtrasTest, Kstest2TailUnequalEqualsBoth)
{
    eval("[~,~,D1] = kstest2(x, y, 'Tail', 'unequal'); [~,~,D2] = kstest2(x, y);");
    EXPECT_DOUBLE_EQ(evalScalar("D1"), evalScalar("D2"));
}

TEST_F(KstestExtrasTest, Kstest2NameValueAlpha)
{
    // 'Alpha', value should not throw
    EXPECT_NO_THROW(eval("kstest2(x, y, 'Alpha', 0.01);"));
}

TEST_F(KstestExtrasTest, Kstest2CombinedAlphaTail)
{
    eval("[h, p, D] = kstest2(x, y, 'Alpha', 0.01, 'Tail', 'larger');");
    EXPECT_DOUBLE_EQ(evalScalar("D"), 0.0);
}

// ── kstest ───────────────────────────────────────────────────────────

TEST_F(KstestExtrasTest, KstestDefault)
{
    eval("[h, p, D] = kstest(x);");
    // Against N(0,1), our x is far in tail so h=1.
    EXPECT_EQ(static_cast<int>(evalScalar("h")), 1);
}

TEST_F(KstestExtrasTest, KstestTailNV)
{
    EXPECT_NO_THROW(eval("kstest(x, 'Tail', 'larger');"));
    EXPECT_NO_THROW(eval("kstest(x, 'Tail', 'smaller');"));
    EXPECT_NO_THROW(eval("kstest(x, 'Tail', 'unequal');"));
}

TEST_F(KstestExtrasTest, KstestAlphaNV)
{
    EXPECT_NO_THROW(eval("kstest(x, 'Alpha', 0.01);"));
}
