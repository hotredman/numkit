// toolboxes/stats/tests/vartest_extras_test.cpp
// — Name-Value parsing
// for Alpha/Tail.

#include <numkit/core/engine.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class VartestExtrasTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override
    {
        engine.eval("import compat.*;");
        engine.eval("x = [1.2 2.4 3.1 4.5 5.0]';");
        engine.eval("y = [0.8 1.9 2.7 4.0 4.5]';");
    }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(VartestExtrasTest, AlphaNVChangesCI)
{
    eval("[~,~,c95] = vartest(x, 1, 'Alpha', 0.05); [~,~,c99] = vartest(x, 1, 'Alpha', 0.01);");
    EXPECT_GT(evalScalar("(c99(2) - c99(1))"),
              evalScalar("(c95(2) - c95(1))"));
}

TEST_F(VartestExtrasTest, TailNVRecognised)
{
    EXPECT_NO_THROW(eval("vartest(x, 1, 'Tail', 'right');"));
    EXPECT_NO_THROW(eval("vartest(x, 1, 'Tail', 'left');"));
}

TEST_F(VartestExtrasTest, DimRejected)
{
    EXPECT_THROW(eval("vartest(x, 1, 'Dim', 1);"), numkit::Error);
}

TEST_F(VartestExtrasTest, Vartest2AlphaNVRecognised)
{
    EXPECT_NO_THROW(eval("vartest2(x, y, 'Alpha', 0.01);"));
}

TEST_F(VartestExtrasTest, Vartest2TailNV)
{
    EXPECT_NO_THROW(eval("vartest2(x, y, 'Tail', 'right');"));
}

TEST_F(VartestExtrasTest, Vartest2DimRejected)
{
    EXPECT_THROW(eval("vartest2(x, y, 'Dim', 1);"), numkit::Error);
}

TEST_F(VartestExtrasTest, BasicValuesUnchanged)
{
    // Sanity: existing positional behaviour must not regress.
    eval("[h, p, ~, T] = vartest(x, 1);");
    EXPECT_NEAR(evalScalar("p"), 0.0966, 1e-3);
}
