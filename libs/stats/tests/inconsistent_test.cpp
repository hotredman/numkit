// libs/stats/tests/inconsistent_test.cpp
// Audit ТЗ closure for inconsistent. Closes audit/findings/cluster/inconsistent.md.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class InconsistentTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// Each row of the output Y is [mean, std, count, inc_coeff] for the
// depth-d subtree below each non-leaf node. inc_coeff = 0 if std == 0.

TEST_F(InconsistentTest, DefaultDepth2)
{
    eval("X = [1 1; 1.5 1.5; 5 5; 5.5 5.5; 10 10];");
    eval("Y = inconsistent(linkage(pdist(X)));");
    EXPECT_EQ(static_cast<size_t>(evalScalar("size(Y, 1)")), 4u);
    EXPECT_EQ(static_cast<size_t>(evalScalar("size(Y, 2)")), 4u);
    // Leaf-level rows have count=1, std=0, inc=0.
    EXPECT_NEAR(evalScalar("Y(1,1)"), 0.7071067812, 1e-9);
    EXPECT_DOUBLE_EQ(evalScalar("Y(1,2)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("Y(1,3)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("Y(1,4)"), 0.0);
    // Row 3: 3 children, mean=2.121, std=2.449, inc=1.155.
    EXPECT_NEAR(evalScalar("Y(3,1)"), 2.1213203436, 1e-9);
    EXPECT_NEAR(evalScalar("Y(3,2)"), 2.4494897428, 1e-9);
    EXPECT_DOUBLE_EQ(evalScalar("Y(3,3)"), 3.0);
    EXPECT_NEAR(evalScalar("Y(3,4)"), 1.1547005384, 1e-9);
}

TEST_F(InconsistentTest, ExplicitDepth3)
{
    eval("X = [1 1; 1.5 1.5; 5 5; 5.5 5.5; 10 10];");
    eval("Y = inconsistent(linkage(pdist(X)), 3);");
    // Last row sees more children at depth 3.
    EXPECT_NEAR(evalScalar("Y(4,1)"), 3.1819805153, 1e-9);
    EXPECT_NEAR(evalScalar("Y(4,2)"), 2.9154759474, 1e-9);
    EXPECT_DOUBLE_EQ(evalScalar("Y(4,3)"), 4.0);
    EXPECT_NEAR(evalScalar("Y(4,4)"), 1.0914163088, 1e-5);
}

TEST_F(InconsistentTest, RowCountMatchesNonLeafNodes)
{
    // For N data points, the linkage tree has N-1 non-leaf nodes.
    eval("X = [1 1; 1.5 1.5; 5 5; 5.5 5.5; 10 10];");
    eval("Y = inconsistent(linkage(pdist(X)));");
    EXPECT_EQ(static_cast<size_t>(evalScalar("size(Y, 1)")), 4u);  // 5 - 1
}
