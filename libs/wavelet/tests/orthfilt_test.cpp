// libs/wavelet/tests/orthfilt_test.cpp
//
// Backfill gtest for libs/wavelet/src/filter/families.cpp::orthfilt.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class OrthfiltTest : public ::testing::Test
{
public:
    Engine engine;
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
