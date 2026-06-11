// toolboxes/stats/tests/jbtest_test.cpp
// jbtest.

#include <numkit/core/engine.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class JbtestTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override {
        engine.eval("import compat.*;");
        // Reference dataset from
        engine.eval("xn = [-0.5 0.3 0.7 1.1 -0.2 0.1 -0.4 0.8 -0.1 0.5]';");
    }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// 2026-05-08 — gap closure: small-sample p-value via Monte Carlo under
// H₀ (matches MATLAB R2025b's tabulated-p behavior, including the
// p=0.5 cap for small JB stats).

TEST_F(JbtestTest, SmallSamplePCappedAt05)
{
    eval("[h, p, JB, cv] = jbtest(xn);");
    EXPECT_DOUBLE_EQ(evalScalar("h"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("p"), 0.5);  // MATLAB cap
    EXPECT_NEAR(evalScalar("JB"), 0.6648, 1e-3);
    // cv is MC-estimated (deterministic seed) → matches MATLAB's
    // tabulated value to within MC error (~0.01 at 1e6 reps).
    EXPECT_NEAR(evalScalar("cv"), 2.5239, 0.05);
}

TEST_F(JbtestTest, SmallSampleAlpha01)
{
    eval("[h, p, JB, cv] = jbtest(xn, 0.01);");
    EXPECT_DOUBLE_EQ(evalScalar("p"), 0.5);
    EXPECT_NEAR(evalScalar("cv"), 5.7077, 0.1);
}

// gap closure: 3rd arg `mctol` parsed and respected.
TEST_F(JbtestTest, MctolArgumentParsed)
{
    eval("[h, p, JB, cv] = jbtest(xn, 0.05, 0.01);");
    // mctol=0.01 → fewer MC iterations → noisier cv estimate, but still
    // p=0.5 cap and JB unchanged.
    EXPECT_DOUBLE_EQ(evalScalar("p"), 0.5);
    EXPECT_NEAR(evalScalar("JB"), 0.6648, 1e-3);
}

// JB statistic itself is closed-form and bit-identical regardless of path.
TEST_F(JbtestTest, JBStatIsDeterministic)
{
    eval("[h, p, JB1, cv1] = jbtest(xn);");
    eval("[h, p, JB2, cv2] = jbtest(xn, 0.05, 0.005);");
    EXPECT_DOUBLE_EQ(evalScalar("JB1"), evalScalar("JB2"));
    EXPECT_NEAR(evalScalar("JB1"), 0.6648, 1e-3);
}

// Large-n path: asymptotic χ²(2) (same as before — back-compat).
TEST_F(JbtestTest, LargeNUsesAsymptotic)
{
    // Disable MC by setting mctol = NaN (asymptotic χ²(2)).
    eval("xb = ones(3000, 1);");  // degenerate: var=0 → JB=0
    eval("[h, p, JB, cv] = jbtest(xb, 0.05, NaN);");
    EXPECT_NEAR(evalScalar("cv"), 5.9914645, 1e-6);  // chi2inv(0.95, 2)
}
