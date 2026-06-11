// toolboxes/wavelet/tests/orthfilt_test.cpp
//
// Backfill gtest for toolboxes/wavelet/src/filter/families.cpp::orthfilt.

#include <numkit/core/engine.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class OrthfiltTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(OrthfiltTest, Db2QuadrupleMatchesMATLAB)
{
    eval("[Lo_D, Hi_D, Lo_R, Hi_R] = orthfilt(dbwavf('db2'));");
    EXPECT_NEAR(evalScalar("Lo_D(1)"), -0.129410, 1e-5);
    EXPECT_NEAR(evalScalar("Lo_D(4)"),  0.482963, 1e-5);
    EXPECT_NEAR(evalScalar("Lo_R(1)"),  0.482963, 1e-5);
    EXPECT_NEAR(evalScalar("Lo_R(4)"), -0.129410, 1e-5);
    EXPECT_NEAR(evalScalar("Hi_D(1)"), -0.482963, 1e-5);
    EXPECT_NEAR(evalScalar("Hi_R(1)"), -0.129410, 1e-5);
}

TEST_F(OrthfiltTest, LoRReversesIntoLoD)
{
    eval("[Lo_D, ~, Lo_R, ~] = orthfilt(dbwavf('db2'));");
    eval("rev = fliplr(Lo_R);");
    EXPECT_DOUBLE_EQ(evalScalar("max(abs(Lo_D - rev))"), 0.0);
}

TEST_F(OrthfiltTest, HiRReversesIntoHiD)
{
    eval("[~, Hi_D, ~, Hi_R] = orthfilt(dbwavf('db2'));");
    eval("rev = fliplr(Hi_R);");
    EXPECT_DOUBLE_EQ(evalScalar("max(abs(Hi_D - rev))"), 0.0);
}

TEST_F(OrthfiltTest, Db2HighPrecision)
{
    // High-precision regression — captures the exact filter values used
    // throughout the wavelet library; any drift would cascade into dwt/idwt.
    eval("[Lo_D, Hi_D, Lo_R, Hi_R] = orthfilt(dbwavf('db2'));");
    EXPECT_NEAR(evalScalar("Lo_D(1)"), -0.1294095225512603, 1e-12);
    EXPECT_NEAR(evalScalar("Lo_D(2)"),  0.2241438680420134, 1e-12);
    EXPECT_NEAR(evalScalar("Lo_D(3)"),  0.8365163037378079, 1e-12);
    EXPECT_NEAR(evalScalar("Lo_D(4)"),  0.4829629131445341, 1e-12);
}

TEST_F(OrthfiltTest, Db4LongerFilter)
{
    // 8-tap db4 filter — verifies the algorithm scales beyond 4 taps.
    eval("[Lo_D, Hi_D, Lo_R, Hi_R] = orthfilt(dbwavf('db4'));");
    EXPECT_NEAR(evalScalar("Lo_D(1)"), -0.0105974017849973, 1e-12);
    EXPECT_NEAR(evalScalar("Hi_D(8)"), -0.0105974017849973, 1e-12);
}

TEST_F(OrthfiltTest, CustomShortFilter)
{
    // 2-tap custom scaling filter [0.4 0.6] (sums to 1):
    // Lo_R = √2 · [0.4 0.6] = [0.5657 0.8485].
    // MATLAB returns Lo_R as the un-reversed version; Lo_D = reverse(Lo_R).
    eval("[Lo_D, Hi_D, Lo_R, Hi_R] = orthfilt([0.4 0.6]);");
    EXPECT_NEAR(evalScalar("Lo_D(1)"),  0.8485281374238570, 1e-12);
    EXPECT_NEAR(evalScalar("Lo_D(2)"),  0.5656854249492381, 1e-12);
    EXPECT_NEAR(evalScalar("Hi_D(1)"), -0.5656854249492381, 1e-12);
    EXPECT_NEAR(evalScalar("Hi_D(2)"),  0.8485281374238570, 1e-12);
}
