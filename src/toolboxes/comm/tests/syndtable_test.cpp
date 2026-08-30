// toolboxes/comm/tests/syndtable_test.cpp
//
// syndtable(H) — syndrome decoding table (coset-leader lookup) from a
// parity-check matrix. bugs/comm/syndtable.md. Reference values from
// MATLAB R2025b.

#include <numkit/bundle/standard_engine.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class SyndtableTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override {}
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// (7,4) Hamming: perfect code → every nonzero syndrome has a unique weight-1
// coset leader. Row index = bi2de(syndrome,'left-msb')+1.
TEST_F(SyndtableTest, HammingPerfect)
{
    eval("H = hammgen(3); t = syndtable(H);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(t,1)")), 8);   // 2^(n-k)
    EXPECT_EQ(static_cast<int>(evalScalar("size(t,2)")), 7);   // n
    EXPECT_DOUBLE_EQ(evalScalar("sum(t(1,:))"), 0.0);          // syndrome 0 → no error
    // bit-1 error → syndrome = H(:,1) = [1;0;0] → dec 4 → row 5.
    EXPECT_DOUBLE_EQ(evalScalar("t(5,1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("sum(t(5,:))"), 1.0);          // weight-1 leader
    // every nonzero-syndrome leader has weight 1.
    EXPECT_DOUBLE_EQ(evalScalar("sum(sum(t)) "), 7.0);         // 7 single-bit leaders
}

// Code needing weight-2 leaders, with tie-breaking by lowest bit position.
TEST_F(SyndtableTest, WeightTwoLeadersAndTies)
{
    eval("H = [1 0 0 1; 0 1 0 1; 0 0 1 1]; t = syndtable(H);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(t,1)")), 8);
    EXPECT_EQ(static_cast<int>(evalScalar("size(t,2)")), 4);
    EXPECT_DOUBLE_EQ(evalScalar("sum(t(:))"), 10.0);           // total set bits
    // s=3 (binary 011) leader is the weight-2 [1 0 0 1].
    EXPECT_DOUBLE_EQ(evalScalar("t(4,1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("t(4,4)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("sum(t(4,:))"), 2.0);
    // s=7 (column 4 = [1;1;1]) → weight-1 leader at bit 4.
    EXPECT_DOUBLE_EQ(evalScalar("t(8,4)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("sum(t(8,:))"), 1.0);
}

// The bug repro: 3×6 parity-check → 8×6 table.
TEST_F(SyndtableTest, ReproSize)
{
    eval("H = [1 0 1 1 0 0; 0 1 1 0 1 0; 1 1 0 0 0 1]; t = syndtable(H);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(t,1)")), 8);
    EXPECT_EQ(static_cast<int>(evalScalar("size(t,2)")), 6);
    EXPECT_DOUBLE_EQ(evalScalar("sum(t(1,:))"), 0.0);
}
