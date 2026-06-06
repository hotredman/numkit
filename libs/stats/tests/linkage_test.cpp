// libs/stats/tests/linkage_test.cpp
// linkage.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class LinkageTest : public ::testing::Test
{
public:
    StdEngine engine;
    void SetUp() override {
        engine.eval("import compat.*;");
        engine.eval("X = [1 1; 1.5 1.5; 5 5; 5.5 5.5; 10 10; 1 2; 6 6; 11 11];");
    }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// 2026-05-08 — gap closure: tie-breaking now matches MATLAB R2025b
// (prefers largest pair lex when distances tie). Z(1,:) was [1 2 ...]
// in numkit; MATLAB picks [4 7 ...] — both at 0.7071 distance.

TEST_F(LinkageTest, TieBreakMatchesMATLAB)
{
    eval("Z = linkage(pdist(X), 'single');");
    EXPECT_DOUBLE_EQ(evalScalar("Z(1,1)"), 4.0);
    EXPECT_DOUBLE_EQ(evalScalar("Z(1,2)"), 7.0);
    EXPECT_NEAR(evalScalar("Z(1,3)"), 0.7071067812, 1e-9);
}

TEST_F(LinkageTest, AllSevenMethodsExist)
{
    for (auto m : {"single", "complete", "average", "weighted",
                   "centroid", "median", "ward"}) {
        eval(std::string("Z = linkage(pdist(X), '") + m + "');");
        EXPECT_DOUBLE_EQ(evalScalar("size(Z, 1)"), 7.0);
        EXPECT_DOUBLE_EQ(evalScalar("size(Z, 2)"), 3.0);
    }
}

// gap closure: linkage(X) accepts raw N×D data matrix directly.
TEST_F(LinkageTest, DirectFromDataMatrix)
{
    eval("Z = linkage(X);");
    EXPECT_DOUBLE_EQ(evalScalar("Z(1,1)"), 4.0);
    EXPECT_DOUBLE_EQ(evalScalar("Z(1,2)"), 7.0);
    EXPECT_NEAR(evalScalar("Z(end,3)"), 5.6568542495, 1e-9);
}

// gap closure: 3-arg form linkage(X, method, metric) was previously
// silently ignoring the metric (hardcoded euclidean).
TEST_F(LinkageTest, ThreeArgFormMetric)
{
    eval("Z = linkage(X, 'single', 'cityblock');");
    // Cityblock distances are integer in this dataset.
    EXPECT_DOUBLE_EQ(evalScalar("Z(1,3)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("Z(end,3)"), 8.0);
}

// Ward linkage gives a different distance pattern (no ties at the
// tested heights → bit-identical to MATLAB without tie-break heuristics).
TEST_F(LinkageTest, WardMethod)
{
    eval("Z = linkage(pdist(X), 'ward');");
    EXPECT_NEAR(evalScalar("Z(end,3)"), 17.351755, 1e-5);
}
