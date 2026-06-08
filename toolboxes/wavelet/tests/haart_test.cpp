// toolboxes/wavelet/tests/haart_test.cpp
//
// Backfill gtest for toolboxes/wavelet/src/dwt/haart.cpp::haart.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class HaartTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(HaartTest, Level1Vector)
{
    eval("[a, d] = haart([1 2 3 4 5 6 7 8], 1);");
    EXPECT_NEAR(evalScalar("a(1)"), 2.121320343559643, 1e-12);
    EXPECT_NEAR(evalScalar("a(4)"), 10.606601717798213, 1e-12);
    EXPECT_NEAR(evalScalar("d(1)"), 0.7071067811865475, 1e-12);
}

TEST_F(HaartTest, DefaultLevelMaxK)
{
    eval("[a, d] = haart([1 2 3 4 5 6 7 8]);");
    // N=8 → max-level k = 3
    EXPECT_NEAR(evalScalar("a"), 12.727922061357855, 1e-12);
    EXPECT_TRUE(eval("iscell(d)").toBool());
    EXPECT_EQ(static_cast<size_t>(evalScalar("length(d)")), 3u);
}

TEST_F(HaartTest, IntegerLifting)
{
    eval("[a, d] = haart([10 20 30 40 50 60 70 80], 1, 'integer');");
    EXPECT_DOUBLE_EQ(evalScalar("a(1)"), 15);
    EXPECT_DOUBLE_EQ(evalScalar("a(4)"), 75);
    EXPECT_DOUBLE_EQ(evalScalar("d(1)"), 10);
}

TEST_F(HaartTest, IntegerOnDoubles)
{
    eval("[a, d] = haart([1.5 2.5 3.5 4.5], 1, 'integer');");
    EXPECT_DOUBLE_EQ(evalScalar("a(1)"), 1.5);
    EXPECT_DOUBLE_EQ(evalScalar("a(2)"), 3.5);
    EXPECT_DOUBLE_EQ(evalScalar("d(1)"), 1);
}

TEST_F(HaartTest, MatrixColumnwise)
{
    eval("M = [1 2 3 4; 5 6 7 8; 9 10 11 12; 13 14 15 16]; [a, d] = haart(M, 1);");
    EXPECT_EQ(static_cast<size_t>(evalScalar("size(a, 1)")), 2u);
    EXPECT_EQ(static_cast<size_t>(evalScalar("size(a, 2)")), 4u);
}

TEST_F(HaartTest, RowInputColumnOutput)
{
    eval("[a, ~] = haart([1 2 3 4]);");
    EXPECT_EQ(static_cast<size_t>(evalScalar("size(a, 1)")), 1u);
    EXPECT_EQ(static_cast<size_t>(evalScalar("size(a, 2)")), 1u);
}

TEST_F(HaartTest, OddLengthErrors)
{
    EXPECT_THROW(eval("haart([1 2 3 4 5]);"), numkit::Error);
}
