// toolboxes/stats/tests/kruskalwallis_extras_test.cpp

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class KruskalwallisExtrasTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override
    {
        engine.eval("import compat.*;");
        engine.eval("xg = [3 5 4 7 8 6 9 10 11]'; g = [1 1 1 2 2 2 3 3 3]';");
    }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(KruskalwallisExtrasTest, BasicPValue)
{
    eval("[p, tbl, st] = kruskalwallis(xg, g, 'off');");
    EXPECT_NEAR(evalScalar("p"), 0.027324, 1e-5);
    EXPECT_DOUBLE_EQ(evalScalar("st.chi2stat"), 7.2);
    EXPECT_DOUBLE_EQ(evalScalar("st.df"), 2);
}

TEST_F(KruskalwallisExtrasTest, StatsHasN)
{
    eval("[~,~,st] = kruskalwallis(xg, g, 'off');");
    EXPECT_DOUBLE_EQ(evalScalar("st.n(1)"), 3);
    EXPECT_DOUBLE_EQ(evalScalar("st.n(2)"), 3);
    EXPECT_DOUBLE_EQ(evalScalar("st.n(3)"), 3);
}

TEST_F(KruskalwallisExtrasTest, StatsHasMeanranks)
{
    eval("[~,~,st] = kruskalwallis(xg, g, 'off');");
    EXPECT_DOUBLE_EQ(evalScalar("st.meanranks(1)"), 2);
    EXPECT_DOUBLE_EQ(evalScalar("st.meanranks(2)"), 5);
    EXPECT_DOUBLE_EQ(evalScalar("st.meanranks(3)"), 8);
}

TEST_F(KruskalwallisExtrasTest, MatrixOnlyForm)
{
    eval("M = [3 7 9; 5 8 10; 4 6 11]; [p,~,st] = kruskalwallis(M, [], 'off');");
    EXPECT_NEAR(evalScalar("p"), 0.027324, 1e-5);
    EXPECT_DOUBLE_EQ(evalScalar("st.n(1)"), 3);
}

TEST_F(KruskalwallisExtrasTest, SumtZeroNoTies)
{
    eval("[~,~,st] = kruskalwallis(xg, g, 'off');");
    EXPECT_DOUBLE_EQ(evalScalar("st.sumt"), 0);
}
