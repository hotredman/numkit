// toolboxes/control/tests/initial_test.cpp
//
// Initial-condition response initial(sys, x0[, t]) — zero-input
// simulation from x0, y = C·x. bugs/control/initial.md. Reference values
// from MATLAB R2025b. The explicit-grid form matches to machine
// precision; the auto-grid horizon is a heuristic (matched to ~1e-7).

#include <numkit/bundle/standard_engine.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class InitialTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// Explicit grid → exact: G with A=-2, x0=1 ⇒ y = e^{-2t}.
TEST_F(InitialTest, FirstOrderExplicitGrid)
{
    eval("[y, t] = initial(ss(-2, 0, 1, 0), 1, 0:0.1:3);");
    EXPECT_NEAR(evalScalar("y(1)"),   1.0,              1e-12);
    EXPECT_NEAR(evalScalar("y(end)"), 0.002478752177,  1e-10);   // e^{-6}
    EXPECT_NEAR(evalScalar("y(11)"),  0.135335283237,  1e-10);   // e^{-2}
    EXPECT_EQ(static_cast<int>(evalScalar("numel(t)")), 31);
}

// Auto grid → matches MATLAB's horizon to ~1e-7 (heuristic, not exact).
TEST_F(InitialTest, FirstOrderAutoGrid)
{
    eval("[y, t] = initial(ss(-2, 0, 1, 0), 1);");
    EXPECT_NEAR(evalScalar("y(1)"),   1.0,                 1e-12);
    EXPECT_NEAR(evalScalar("y(end)"), 0.00301995172040398, 1e-6);
}

// Two-state, x0=[1;0]: y(t)=first component of expm(A t)·x0.
TEST_F(InitialTest, TwoStateExplicit)
{
    eval("[y, t] = initial(ss([0 1; -2 -3], [0; 0], [1 0], 0), [1; 0], 0:0.05:5);");
    EXPECT_NEAR(evalScalar("y(1)"),   1.0,             1e-12);
    EXPECT_NEAR(evalScalar("y(21)"),  0.6004235991,    1e-9);   // t = 1
    EXPECT_NEAR(evalScalar("y(end)"), 0.013430494068,  1e-9);
}

// Third output is the state trajectory (one row per sample, n columns).
TEST_F(InitialTest, StateTrajectory)
{
    eval("[y, t, x] = initial(ss([0 1; -2 -3], [0; 0], [1 0], 0), [1; 0], 0:0.05:5);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(x, 2)")), 2);     // two states
    EXPECT_NEAR(evalScalar("x(1, 1)"), 1.0, 1e-12);              // x1(0) = 1
    EXPECT_NEAR(evalScalar("x(1, 2)"), 0.0, 1e-12);              // x2(0) = 0
}

// Wrong-length x0 → clear error.
TEST_F(InitialTest, BadX0Throws)
{
    EXPECT_THROW(eval("initial(ss([0 1; -2 -3], [0; 0], [1 0], 0), [1; 2; 3], 0:0.1:1);"),
                 std::exception);
}
