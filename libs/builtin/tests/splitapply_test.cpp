// libs/builtin/tests/splitapply_test.cpp
//
// Regression guard for splitapply. (Function existed but lacked
// gtest/parity coverage; this closes the gap and flips the PROGRESS
// row from ❌ to ✅.)

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class SplitApplyTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override {
        engine.eval("g = [1 2 1 2 1]; x = [10 20 30 40 50];");
    }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(SplitApplyTest, SumByGroup)
{
    eval("B = splitapply(@sum, x, g);");
    EXPECT_EQ(static_cast<int>(evalScalar("B(1)")), 90);
    EXPECT_EQ(static_cast<int>(evalScalar("B(2)")), 60);
}

TEST_F(SplitApplyTest, MeanByGroup)
{
    eval("B = splitapply(@mean, x, g);");
    EXPECT_NEAR(evalScalar("B(1)"), 30.0, 1e-12);
    EXPECT_NEAR(evalScalar("B(2)"), 30.0, 1e-12);
}

TEST_F(SplitApplyTest, MaxMinByGroup)
{
    eval("Bx = splitapply(@max, x, g);"
         "Bn = splitapply(@min, x, g);");
    EXPECT_EQ(static_cast<int>(evalScalar("Bx(1)")), 50);
    EXPECT_EQ(static_cast<int>(evalScalar("Bx(2)")), 40);
    EXPECT_EQ(static_cast<int>(evalScalar("Bn(1)")), 10);
    EXPECT_EQ(static_cast<int>(evalScalar("Bn(2)")), 20);
}

TEST_F(SplitApplyTest, MultiInputHandle)
{
    eval("y = [100 200 300 400 500];"
         "B = splitapply(@(a,b) sum(a) + sum(b), x, y, g);");
    EXPECT_EQ(static_cast<int>(evalScalar("B(1)")), 990);
    EXPECT_EQ(static_cast<int>(evalScalar("B(2)")), 660);
}

TEST_F(SplitApplyTest, OutputIsColumnVector)
{
    eval("B = splitapply(@sum, x, g);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(B,1)")), 2);
    EXPECT_EQ(static_cast<int>(evalScalar("size(B,2)")), 1);
}

TEST_F(SplitApplyTest, NoHandleThrows)
{
    EXPECT_THROW(eval("splitapply(x, g);"), std::exception);
}

TEST_F(SplitApplyTest, MismatchSizeThrows)
{
    EXPECT_THROW(eval("splitapply(@sum, [1 2 3], [1 2]);"), std::exception);
}
