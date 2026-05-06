// libs/stats/tests/lognlike_test.cpp
//
// Audit ТЗ closure for lognlike. Reference values from MATLAB R2025b.
// Closes audit/findings/stats/lognlike.md.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class LognlikeTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override
    {
        engine.eval("import compat.*;");
        engine.eval("x = [1 2 3 4 5 6 7 8 9 10]';");
        engine.eval("cens = [0 0 0 0 0 0 0 1 1 1]';");
        engine.eval("freq = [2 2 2 1 1 1 1 1 1 1]';");
    }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(LognlikeTest, BasicNLogL)
{
    EXPECT_NEAR(evalScalar("lognlike([0, 1], x)"), 38.1189198087, 1e-9);
}

TEST_F(LognlikeTest, AVarBasicNonPDOK)
{
    // aVar can have negative diagonal at non-MLE params (observed Fisher).
    eval("[nL, av] = lognlike([0, 1], x);");
    EXPECT_NEAR(evalScalar("av(1,1)"), -0.3984945873, 1e-9);
    EXPECT_NEAR(evalScalar("av(1,2)"),  0.1650162113, 1e-9);
    EXPECT_NEAR(evalScalar("av(2,1)"),  0.1650162113, 1e-9);  // symmetry
    EXPECT_NEAR(evalScalar("av(2,2)"), -0.0546251668, 1e-9);
}

TEST_F(LognlikeTest, WithCensoring)
{
    EXPECT_NEAR(evalScalar("lognlike([0, 1], x, cens)"), 34.3411160493, 1e-9);
}

TEST_F(LognlikeTest, WithFreq)
{
    EXPECT_NEAR(evalScalar("lognlike([0, 1], x, [], freq)"), 43.5111958649, 1e-9);
}

TEST_F(LognlikeTest, CensoringPlusFreq)
{
    EXPECT_NEAR(evalScalar("lognlike([0, 1], x, cens, freq)"), 39.7333921055, 1e-9);
}

TEST_F(LognlikeTest, ZeroSigmaReturnsNaN)
{
    EXPECT_TRUE(std::isnan(evalScalar("lognlike([0, 0], [1 2 3 4 5]')")));
}

TEST_F(LognlikeTest, NegativeXReturnsNaN)
{
    EXPECT_TRUE(std::isnan(evalScalar("lognlike([0, 1], [-1 2 3]')")));
}

TEST_F(LognlikeTest, EmptyDataReturnsZero)
{
    EXPECT_DOUBLE_EQ(evalScalar("lognlike([0, 1], [])"), 0.0);
}

TEST_F(LognlikeTest, ZeroFreqDropsElement)
{
    eval("y1 = lognlike([0, 1], x, [], [1 1 1 1 1 1 1 1 1 0]');");
    eval("y2 = lognlike([0, 1], x(1:9));");
    EXPECT_DOUBLE_EQ(evalScalar("y1"), evalScalar("y2"));
}
