// toolboxes/stats/tests/runstest_extras_test.cpp
// (partial — 'ud' up-down form
// implemented; degenerate-case p-value for all-up sequences still
// returns NaN, deferred).

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class RunstestExtrasTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override
    {
        engine.eval("import compat.*;");
        engine.eval("xs = [-1 1 -1 1 -1 1 -1 1]';");
    }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(RunstestExtrasTest, DefaultMedianBased)
{
    eval("[h, p, st] = runstest(xs);");
    EXPECT_DOUBLE_EQ(evalScalar("st.nruns"), 8);
    EXPECT_DOUBLE_EQ(evalScalar("st.n1"), 4);
    EXPECT_DOUBLE_EQ(evalScalar("st.n0"), 4);
}

TEST_F(RunstestExtrasTest, UdMonotonicAllUp)
{
    eval("[h, p, st] = runstest([1.2 2.4 3.1 4.5 5.0 6.2 7.1]', 'ud');");
    EXPECT_DOUBLE_EQ(evalScalar("st.nruns"), 1);
    EXPECT_DOUBLE_EQ(evalScalar("st.n1"), 6);
    EXPECT_DOUBLE_EQ(evalScalar("st.n0"), 0);
}

TEST_F(RunstestExtrasTest, UdAlternating)
{
    eval("[h, p, st] = runstest([1 3 2 4 3 5 4 6]', 'ud');");
    EXPECT_DOUBLE_EQ(evalScalar("st.nruns"), 7);
}

TEST_F(RunstestExtrasTest, AlphaNV)
{
    EXPECT_NO_THROW(eval("runstest(xs, 'Alpha', 0.01);"));
}

TEST_F(RunstestExtrasTest, TailNV)
{
    EXPECT_NO_THROW(eval("runstest(xs, 'Tail', 'right');"));
}
