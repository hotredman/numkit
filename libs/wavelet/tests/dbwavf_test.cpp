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

// Bug fix 2026-05-08 — extended Daubechies table from db1..db4 to db1..db10.

TEST_F(DbwavfTest, Db5ToDb10Lengths)
{
    EXPECT_EQ(static_cast<size_t>(evalScalar("numel(dbwavf('db5'))")),  10u);
    EXPECT_EQ(static_cast<size_t>(evalScalar("numel(dbwavf('db6'))")),  12u);
    EXPECT_EQ(static_cast<size_t>(evalScalar("numel(dbwavf('db7'))")),  14u);
    EXPECT_EQ(static_cast<size_t>(evalScalar("numel(dbwavf('db8'))")),  16u);
    EXPECT_EQ(static_cast<size_t>(evalScalar("numel(dbwavf('db9'))")),  18u);
    EXPECT_EQ(static_cast<size_t>(evalScalar("numel(dbwavf('db10'))")), 20u);
}

TEST_F(DbwavfTest, Db5ToDb10SumsToOne)
{
    // All Daubechies scaling filters satisfy sum(h) = 1.
    EXPECT_NEAR(evalScalar("sum(dbwavf('db5'))"),  1.0, 1e-12);
    EXPECT_NEAR(evalScalar("sum(dbwavf('db6'))"),  1.0, 1e-12);
    EXPECT_NEAR(evalScalar("sum(dbwavf('db7'))"),  1.0, 1e-12);
    EXPECT_NEAR(evalScalar("sum(dbwavf('db8'))"),  1.0, 1e-12);
    EXPECT_NEAR(evalScalar("sum(dbwavf('db9'))"),  1.0, 1e-12);
    EXPECT_NEAR(evalScalar("sum(dbwavf('db10'))"), 1.0, 1e-12);
}

TEST_F(DbwavfTest, Db5HighPrecision)
{
    eval("h = dbwavf('db5');");
    // Reference: MATLAB R2025b dbwavf('db5').
    EXPECT_NEAR(evalScalar("h(1)"),  0.11320949, 1e-7);
    EXPECT_NEAR(evalScalar("h(10)"), 0.00235871, 1e-7);
}

TEST_F(DbwavfTest, Db8HighPrecision)
{
    eval("h = dbwavf('db8');");
    EXPECT_NEAR(evalScalar("h(1)"),   0.03847781, 1e-7);
    EXPECT_NEAR(evalScalar("h(16)"), -0.00008307, 1e-7);
}
