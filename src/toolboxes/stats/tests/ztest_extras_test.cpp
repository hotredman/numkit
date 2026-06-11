// toolboxes/stats/tests/ztest_extras_test.cpp
// — Name-Value Alpha/Tail parsing.

#include <numkit/core/engine.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class ZtestExtrasTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override
    {
        engine.eval("import compat.*;");
        engine.eval("x = [1.2 2.4 3.1 4.5 5.0]';");
    }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(ZtestExtrasTest, AlphaNVChangesCI)
{
    eval("[~,~,c95] = ztest(x, 3, 1, 'Alpha', 0.05); [~,~,c99] = ztest(x, 3, 1, 'Alpha', 0.01);");
    EXPECT_GT(evalScalar("(c99(2) - c99(1))"),
              evalScalar("(c95(2) - c95(1))"));
}

TEST_F(ZtestExtrasTest, TailNVRecognised)
{
    EXPECT_NO_THROW(eval("ztest(x, 3, 1, 'Tail', 'right');"));
    EXPECT_NO_THROW(eval("ztest(x, 3, 1, 'Tail', 'left');"));
}

TEST_F(ZtestExtrasTest, DimRejected)
{
    EXPECT_THROW(eval("ztest(x, 3, 1, 'Dim', 1);"), numkit::Error);
}

TEST_F(ZtestExtrasTest, BasicValuesUnchanged)
{
    eval("[h, p, ~, z] = ztest(x, 3, 1);");
    // X mean = 3.24; std error = 1/sqrt(5) = 0.4472; z = (3.24-3)/0.4472 ≈ 0.5367
    EXPECT_NEAR(evalScalar("z"), 0.5367, 1e-3);
}
