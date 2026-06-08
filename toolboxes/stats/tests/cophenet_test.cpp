// toolboxes/stats/tests/cophenet_test.cpp
// cophenet.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class CophenetTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(CophenetTest, ScalarOutputCorrelation)
{
    eval("X = [1 1; 1.5 1.5; 5 5; 5.5 5.5; 10 10];");
    eval("Y = pdist(X); Z = linkage(Y);");
    eval("c = cophenet(Z, Y);");
    EXPECT_NEAR(evalScalar("c"), 0.8441281440, 1e-9);
}

TEST_F(CophenetTest, TwoOutputProducesCopheneticDistances)
{
    // Bug fix 2026-05-08: 2-output form was throwing because the
    // adapter only emitted outs[0]. cophenet now returns both the
    // scalar correlation and the (1×Yn) cophenetic distance vector.
    eval("X = [1 1; 1.5 1.5; 5 5; 5.5 5.5; 10 10];");
    eval("Y = pdist(X); Z = linkage(Y);");
    eval("[c, d] = cophenet(Z, Y);");
    EXPECT_NEAR(evalScalar("c"), 0.8441281440, 1e-9);
    EXPECT_EQ(static_cast<size_t>(evalScalar("numel(d)")), 10u);
    EXPECT_NEAR(evalScalar("d(1)"), 0.7071067812, 1e-9);
    EXPECT_NEAR(evalScalar("d(2)"), 4.9497474683, 1e-9);
    EXPECT_NEAR(evalScalar("d(8)"), 0.7071067812, 1e-9);
    EXPECT_NEAR(evalScalar("d(10)"), 6.3639610307, 1e-9);
}

TEST_F(CophenetTest, ScalarMatchesTwoOutputFirst)
{
    eval("X = rand(10, 3);");
    eval("Y = pdist(X); Z = linkage(Y);");
    eval("c1 = cophenet(Z, Y);");
    eval("[c2, ~] = cophenet(Z, Y);");
    EXPECT_DOUBLE_EQ(evalScalar("c1"), evalScalar("c2"));
}
