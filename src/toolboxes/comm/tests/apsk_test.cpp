// toolboxes/comm/tests/apsk_test.cpp
//
// Regression guard for apskmod / apskdemod (multi-ring constellation).
// All tests use explicit identity SymbolMapping = 0..N-1 (numkit's
// default); MATLAB's default 'gray' mapping is deferred and tests
// passing it through MATLAB use the same identity mapping for parity.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class ApskTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override {}
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(ApskTest, SixteenAPSKConstellationKnown)
{
    eval("M = [4 12]; r = [1 2.7]; po = pi./M; map = (0:15);"
         "y = apskmod((0:15)', M, r, po, map);");
    // Inner ring (M=4, r=1, phase=pi/4):
    EXPECT_NEAR(evalScalar("real(y(1))"),  0.7071067811865476, 1e-12);
    EXPECT_NEAR(evalScalar("imag(y(1))"),  0.7071067811865475, 1e-12);
    EXPECT_NEAR(evalScalar("real(y(3))"), -0.7071067811865477, 1e-12);
    // Outer ring (M=12, r=2.7, phase=pi/12), idx 4 = first outer (15°):
    EXPECT_NEAR(evalScalar("abs(y(5))"), 2.7, 1e-12);
}

TEST_F(ApskTest, RoundTripIdentityMapping)
{
    eval("M = [4 12]; r = [1 2.7]; po = pi./M; map = (0:15);"
         "x = (0:15)';"
         "y = apskmod(x, M, r, po, map);"
         "z = apskdemod(y, M, r, po, map);"
         "match = isequal(x, z);");
    EXPECT_DOUBLE_EQ(evalScalar("match"), 1.0);
}

TEST_F(ApskTest, DefaultPhaseOffsetIsPiOverM)
{
    eval("M = [4 12]; r = [1 2.7]; map = (0:15);"
         "y1 = apskmod((0:15)', M, r, [],     map);"
         "y2 = apskmod((0:15)', M, r, pi./M,  map);"
         "diff_max = max(abs(y1 - y2));");
    EXPECT_LT(evalScalar("diff_max"), 1e-15);
}

TEST_F(ApskTest, SimpleQpskRingOnly)
{
    // M = [4], r = [1] -> standard QPSK at pi/4 + k*pi/2.
    eval("y = apskmod((0:3)', 4, 1, pi/4, (0:3));");
    EXPECT_NEAR(evalScalar("real(y(1))"),  0.7071067811865476, 1e-12);
    EXPECT_NEAR(evalScalar("real(y(2))"), -0.7071067811865475, 1e-12);
}

TEST_F(ApskTest, NearestNeighborDemod)
{
    // Add small noise; demod should still pick correct constellation point.
    eval("M = [4 12]; r = [1 2.7]; po = pi./M; map = (0:15);"
         "x = (0:15)';"
         "y = apskmod(x, M, r, po, map);"
         "noisy = y + 0.05*(1+1i);"
         "z = apskdemod(noisy, M, r, po, map);"
         "match = isequal(x, z);");
    EXPECT_DOUBLE_EQ(evalScalar("match"), 1.0);
}

TEST_F(ApskTest, ShapePreserved)
{
    eval("y = apskmod([0 1; 2 3], 4, 1, pi/4, (0:3));");
    EXPECT_EQ(static_cast<int>(evalScalar("size(y, 1)")), 2);
    EXPECT_EQ(static_cast<int>(evalScalar("size(y, 2)")), 2);
}

TEST_F(ApskTest, RejectsOutOfRangeIndex)
{
    bool threw = false;
    try {
        eval("apskmod(99, [4 12], [1 2.7], pi./[4 12], (0:15));");
    } catch (...) { threw = true; }
    EXPECT_TRUE(threw);
}

TEST_F(ApskTest, RejectsMismatchedMRadii)
{
    bool threw = false;
    try {
        eval("apskmod((0:7)', [4 8], [1], pi/4, (0:11));");  // |M|=2, |radii|=1
    } catch (...) { threw = true; }
    EXPECT_TRUE(threw);
}

TEST_F(ApskTest, ScalarPhaseOffsetBroadcasts)
{
    // Single phase offset broadcasts to all rings.
    eval("y = apskmod((0:7)', [4 4], [1 2], 0, (0:7));");
    EXPECT_EQ(static_cast<int>(evalScalar("numel(y)")), 8);
}
