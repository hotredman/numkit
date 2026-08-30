// toolboxes/stats/tests/vartestn_test.cpp
// vartestn.

#include <numkit/core/engine.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class VartestnTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override {
                engine.eval("xg = [1.2 1.5 1.8 2.1 5.0 5.5 4.8 6.0 4.5 9.1 8.5 10.0 9.5]';");
        engine.eval("g = [1 1 1 1 2 2 2 2 2 3 3 3 3]';");
    }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// All 5 TestType variants + matrix-input form. All bit-identical to
// MATLAB R2025b reference values from

TEST_F(VartestnTest, BartlettDefault)
{
    eval("[p, st] = vartestn(xg, g, 'Display', 'off');");
    EXPECT_NEAR(evalScalar("p"), 0.710697, 1e-6);
    EXPECT_NEAR(evalScalar("st.chisqstat"), 0.683019, 1e-6);
    EXPECT_DOUBLE_EQ(evalScalar("st.df"), 2.0);
}

// 2026-05-08 — gap closure: LeveneAbsolute returns F-stat, df=[k-1, N-k].
TEST_F(VartestnTest, LeveneAbsolute)
{
    eval("[p, st] = vartestn(xg, g, 'Display', 'off', "
         "'TestType', 'LeveneAbsolute');");
    EXPECT_NEAR(evalScalar("p"), 0.567243, 1e-6);
    EXPECT_NEAR(evalScalar("st.fstat"), 0.600364, 1e-6);
    EXPECT_DOUBLE_EQ(evalScalar("st.df(1)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("st.df(2)"), 10.0);
}

TEST_F(VartestnTest, LeveneQuadratic)
{
    eval("[p, st] = vartestn(xg, g, 'Display', 'off', "
         "'TestType', 'LeveneQuadratic');");
    EXPECT_NEAR(evalScalar("p"), 0.515049, 1e-6);
    EXPECT_NEAR(evalScalar("st.fstat"), 0.709530, 1e-6);
    EXPECT_DOUBLE_EQ(evalScalar("st.df(1)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("st.df(2)"), 10.0);
}

TEST_F(VartestnTest, BrownForsythe)
{
    eval("[p, st] = vartestn(xg, g, 'Display', 'off', "
         "'TestType', 'BrownForsythe');");
    EXPECT_NEAR(evalScalar("p"), 0.706609, 1e-6);
    EXPECT_NEAR(evalScalar("st.fstat"), 0.359622, 1e-6);
    EXPECT_DOUBLE_EQ(evalScalar("st.df(1)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("st.df(2)"), 10.0);
}

TEST_F(VartestnTest, OBrien)
{
    eval("[p, st] = vartestn(xg, g, 'Display', 'off', "
         "'TestType', 'OBrien');");
    EXPECT_NEAR(evalScalar("p"), 0.635870, 1e-6);
    EXPECT_NEAR(evalScalar("st.fstat"), 0.473893, 1e-6);
    EXPECT_DOUBLE_EQ(evalScalar("st.df(1)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("st.df(2)"), 10.0);
}

// gap closure: matrix input form (each column = group).
TEST_F(VartestnTest, MatrixInputForm)
{
    eval("M = [1 5 9; 2 6 10; 3 4 11; 4 5 12];");
    eval("[p, st] = vartestn(M, 'Display', 'off');");
    EXPECT_NEAR(evalScalar("p"), 0.724328, 1e-6);
    EXPECT_NEAR(evalScalar("st.chisqstat"), 0.645021, 1e-6);
    EXPECT_DOUBLE_EQ(evalScalar("st.df"), 2.0);
}
