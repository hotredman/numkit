// toolboxes/comm/tests/known_bugs_test.cpp
//
// One DISABLED_ test per OPEN bug in bugs/comm/*.md. Disabled until fixed;
// remove `DISABLED_` to turn into a live regression guard. MATLAB R2025b.

#include <numkit/core/engine.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class CommKnownBug : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override {}
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// bugs/comm/analog-demodulators.md — every *mod exists but no *demod does.
// Each demod must exist and return a signal of the message length.
// (Verify roundtrip recovery vs MATLAB when enabling.)
TEST_F(CommKnownBug, AmDemodExists)
{
    eval("m=[0.2 0.5 -0.3 0.1]; y=amdemod(ammod(m,30,100),30,100);");
    EXPECT_EQ(static_cast<int>(evalScalar("numel(y)")), 4);
}
TEST_F(CommKnownBug, FmDemodExists)
{
    eval("m=[0.2 0.5 -0.3 0.1]; y=fmdemod(fmmod(m,30,100,5),30,100,5);");
    EXPECT_EQ(static_cast<int>(evalScalar("numel(y)")), 4);
}
TEST_F(CommKnownBug, PmDemodExists)
{
    eval("m=[0.2 0.5 -0.3 0.1]; y=pmdemod(pmmod(m,30,100,1),30,100,1);");
    EXPECT_EQ(static_cast<int>(evalScalar("numel(y)")), 4);
}
TEST_F(CommKnownBug, SsbDemodExists)
{
    eval("m=[0.2 0.5 -0.3 0.1]; y=ssbdemod(ssbmod(m,30,100),30,100);");
    EXPECT_EQ(static_cast<int>(evalScalar("numel(y)")), 4);
}
// bugs/comm/analog-demodulators.md — mskdemod (FIXED 2026-06-19, promoted live).
TEST_F(CommKnownBug, MskDemodExists)
{
    eval("d=[1 0 1 1 0]; y=mskdemod(mskmod(d,8),8);");
    EXPECT_EQ(static_cast<int>(evalScalar("numel(y)")), 5);
    EXPECT_EQ(static_cast<int>(evalScalar("sum(y(:)==d(:))")), 5);   // exact round-trip
}

// bugs/comm/syndtable.md — syndrome decoding table (coset leaders).
// FIXED 2026-06-19 (min-weight coset-leader enumeration) — promoted live.
TEST_F(CommKnownBug, Syndtable)
{
    eval("H = [1 0 1 1 0 0; 0 1 1 0 1 0; 1 1 0 0 0 1];");  // 3x6, n-k=3
    eval("t = syndtable(H);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(t,1)")), 8);   // 2^(n-k)
    EXPECT_EQ(static_cast<int>(evalScalar("size(t,2)")), 6);   // n
    EXPECT_DOUBLE_EQ(evalScalar("sum(t(1,:))"), 0.0);          // syndrome 0 -> no error
}
