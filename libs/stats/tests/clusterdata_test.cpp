// libs/stats/tests/clusterdata_test.cpp
// Audit ТЗ closure for clusterdata. Closes audit/findings/cluster/clusterdata.md.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class ClusterdataTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override {
        engine.eval("import compat.*;");
        engine.eval("X = [1 1; 1.5 1.5; 5 5; 5.5 5.5; 10 10; 1 2; 6 6; 11 11];");
    }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// Auditor said "no major gap" — re-probe surfaced 4 real bugs in
// clusterdata's 2nd-arg shortcut + N-V parsing (case-sensitive,
// missing 'Distance' / 'Depth' wiring).

// Bug 1: scalar c < 2 was interpreted as maxclust (gave 0 → all
// singletons); MATLAB treats it as cutoff (inconsistency).
TEST_F(ClusterdataTest, ScalarShortcutCutoff)
{
    eval("T = clusterdata(X, 0.5);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(unique(T))"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(T(1)==T(2))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(T(3)==T(4))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(T(5)==T(8))"), 1.0);
}

// Bug 1 mirror: scalar c >= 2 IS maxclust.
TEST_F(ClusterdataTest, ScalarShortcutMaxclust)
{
    eval("T = clusterdata(X, 3);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(unique(T))"), 3.0);
}

TEST_F(ClusterdataTest, ScalarShortcutMaxclustFour)
{
    eval("T = clusterdata(X, 4);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(unique(T))"), 4.0);
}

// Bug 1 boundary: c = 1.5 → cutoff (root inc 0.7259 < 1.5 → 1 cluster).
TEST_F(ClusterdataTest, ScalarShortcutBoundary)
{
    eval("T = clusterdata(X, 1.5);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(unique(T))"), 1.0);
}

// Bug 2: case-insensitive N-V parsing (was matching only lowercase).
TEST_F(ClusterdataTest, MixedCaseNV)
{
    eval("T = clusterdata(X, 'MaxClust', 3, 'Linkage', 'ward');");
    EXPECT_DOUBLE_EQ(evalScalar("numel(unique(T))"), 3.0);
}

// Bug 3: 'Cutoff' (capital C) + 'Criterion' = 'distance' wiring.
TEST_F(ClusterdataTest, CutoffCriterionDistance)
{
    eval("T = clusterdata(X, 'Cutoff', 1.0, 'Criterion', 'distance');");
    EXPECT_DOUBLE_EQ(evalScalar("numel(unique(T))"), 4.0);
}

// Bug 4: 'Distance' N-V was previously hardcoded to 'euclidean'.
TEST_F(ClusterdataTest, DistanceCityblock)
{
    eval("T = clusterdata(X, 'maxclust', 3, 'Distance', 'cityblock');");
    EXPECT_DOUBLE_EQ(evalScalar("numel(unique(T))"), 3.0);
}
