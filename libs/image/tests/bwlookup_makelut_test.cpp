// libs/image/tests/bwlookup_makelut_test.cpp
//
// Regression guard for image morphology LUT pair: makelut + bwlookup.
// Fingerprints from MATLAB R2025b.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class BwlookupMakelutTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override
    {
        engine.eval("import compat.*;");
        engine.eval("BW = [0 1 1 0; 1 1 0 1; 0 0 1 1; 1 0 1 0];");
    }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// ── makelut ───────────────────────────────────────────────────────────
TEST_F(BwlookupMakelutTest, Makelut2x2SumThreshold)
{
    eval("l = makelut(@(x) sum(x(:)) >= 3, 2);");
    EXPECT_EQ(static_cast<int>(evalScalar("numel(l)")), 16);
    // Only patterns with >=3 of 4 bits set -> entries with index 7,11,13,14,15
    // (0-based) i.e. 8,12,14,15,16 (1-based). First 7 entries are 0.
    EXPECT_DOUBLE_EQ(evalScalar("l(8)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("l(7)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("sum(l)"), 5.0);  // C(4,3)+C(4,4)=4+1
}

TEST_F(BwlookupMakelutTest, Makelut3x3CenterIsIdentity)
{
    eval("lc = makelut(@(x) x(5), 3);");        // center pixel passthrough
    EXPECT_EQ(static_cast<int>(evalScalar("numel(lc)")), 512);
    eval("A = bwlookup(BW, lc);");
    EXPECT_DOUBLE_EQ(evalScalar("isequal(logical(A), logical(BW))"), 1.0);
}

TEST_F(BwlookupMakelutTest, Makelut3x3MajorityCount)
{
    eval("lm = makelut(@(x) sum(x(:)) >= 5, 3);");
    // sum_{k=5}^{9} C(9,k) = 126+84+36+9+1 = 256.
    EXPECT_DOUBLE_EQ(evalScalar("sum(lm)"), 256.0);
}

TEST_F(BwlookupMakelutTest, MakelutOutputAlwaysDouble)
{
    eval("l = makelut(@(x) true, 2);");
    EXPECT_TRUE(eval("l").type() == ValueType::DOUBLE);
    EXPECT_DOUBLE_EQ(evalScalar("l(1)"), 1.0);
}

TEST_F(BwlookupMakelutTest, MakelutBadNThrows)
{
    bool t4 = false, t1 = false;
    try { eval("makelut(@(x) 1, 4);"); } catch (...) { t4 = true; }
    try { eval("makelut(@(x) 1, 1);"); } catch (...) { t1 = true; }
    EXPECT_TRUE(t4);
    EXPECT_TRUE(t1);
}

// ── bwlookup ──────────────────────────────────────────────────────────
TEST_F(BwlookupMakelutTest, BwlookupEqualsApplylut)
{
    eval("lp = double(mod(0:511, 2));");
    eval("A = bwlookup(BW, lp); B = applylut(BW, lp);");
    EXPECT_DOUBLE_EQ(evalScalar("isequal(A, B)"), 1.0);
}

TEST_F(BwlookupMakelutTest, BwlookupOutputClassFollowsLut)
{
    eval("lu = uint8(mod(0:511, 2));");
    eval("A = bwlookup(BW, lu);");
    EXPECT_TRUE(eval("A").type() == ValueType::UINT8);
}

TEST_F(BwlookupMakelutTest, Bwlookup2x2Lut)
{
    // 16-element lut also accepted (2x2 neighbourhood).
    eval("l2 = makelut(@(x) sum(x(:)) >= 1, 2);");
    eval("A = bwlookup(BW, l2);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(A,1)")), 4);
    EXPECT_EQ(static_cast<int>(evalScalar("size(A,2)")), 4);
}

TEST_F(BwlookupMakelutTest, BwlookupBadLutLengthThrows)
{
    bool t3 = false, t32 = false;
    try { eval("bwlookup(BW, [1 2 3]);"); } catch (...) { t3 = true; }
    try { eval("bwlookup(BW, ones(1,32));"); } catch (...) { t32 = true; }
    EXPECT_TRUE(t3);
    EXPECT_TRUE(t32);
}
