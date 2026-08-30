// toolboxes/stats/tests/rmse_mape_test.cpp
// (partial — 'all'/vecdim).

#include <numkit/core/engine.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class RmseMapeTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override
    {
                engine.eval("F = [1 2; 3 4]; A = [1.1 1.9; 2.8 4.2];");
    }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(RmseMapeTest, RmseDefault)
{
    eval("y = rmse(F, A);");
    EXPECT_EQ(static_cast<size_t>(evalScalar("numel(y)")), 2u);
}

TEST_F(RmseMapeTest, RmseAll)
{
    EXPECT_NEAR(evalScalar("rmse(F, A, 'all')"), 0.1581, 1e-3);
}

TEST_F(RmseMapeTest, RmseVecdim)
{
    EXPECT_NEAR(evalScalar("rmse(F, A, [1 2])"), 0.1581, 1e-3);
}

TEST_F(RmseMapeTest, MapeAll)
{
    EXPECT_NEAR(evalScalar("mape(F, A, 'all')"), 6.5647, 1e-3);
}

TEST_F(RmseMapeTest, BadFlagThrows)
{
    EXPECT_THROW(eval("rmse(F, A, 'unknown');"), numkit::Error);
}
