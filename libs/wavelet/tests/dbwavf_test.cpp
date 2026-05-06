// libs/wavelet/tests/dbwavf_test.cpp
//
// Backfill gtest for libs/wavelet/src/filter/families.cpp::dbwavf.
// Reference values from MATLAB R2025b probe.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class DbwavfTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// dbwavf(N) returns the Lo_R / sqrt(2) (sum = 1) Daubechies scaling filter.

TEST_F(DbwavfTest, Db1Haar)
{
    eval("h = dbwavf('db1');");
    EXPECT_EQ(static_cast<size_t>(evalScalar("numel(h)")), 2u);
    EXPECT_DOUBLE_EQ(evalScalar("h(1)"), 0.5);
    EXPECT_DOUBLE_EQ(evalScalar("h(2)"), 0.5);
}

TEST_F(DbwavfTest, Db2Length4)
{
    eval("h = dbwavf('db2');");
    EXPECT_EQ(static_cast<size_t>(evalScalar("numel(h)")), 4u);
    EXPECT_NEAR(evalScalar("h(1)"),  0.341506, 1e-5);
    EXPECT_NEAR(evalScalar("h(2)"),  0.591506, 1e-5);
    EXPECT_NEAR(evalScalar("h(3)"),  0.158494, 1e-5);
    EXPECT_NEAR(evalScalar("h(4)"), -0.091506, 1e-5);
}

TEST_F(DbwavfTest, Db4Length8)
{
    eval("h = dbwavf('db4');");
    EXPECT_EQ(static_cast<size_t>(evalScalar("numel(h)")), 8u);
    EXPECT_NEAR(evalScalar("h(1)"),  0.162902, 1e-5);
    EXPECT_NEAR(evalScalar("h(8)"), -0.007493, 1e-5);
}

TEST_F(DbwavfTest, NormalisedToSumOne)
{
    EXPECT_NEAR(evalScalar("sum(dbwavf('db1'))"), 1.0, 1e-12);
    EXPECT_NEAR(evalScalar("sum(dbwavf('db2'))"), 1.0, 1e-12);
    EXPECT_NEAR(evalScalar("sum(dbwavf('db4'))"), 1.0, 1e-12);
}

TEST_F(DbwavfTest, HaarAlias)
{
    // 'haar' should yield the same filter as 'db1'.
    eval("a = dbwavf('haar'); b = dbwavf('db1');");
    EXPECT_DOUBLE_EQ(evalScalar("max(abs(a - b))"), 0.0);
}
