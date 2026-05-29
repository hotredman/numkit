// libs/control/tests/c2d_foh_test.cpp
//
// c2d 'foh' (first-order / triangle hold). Values verified against
// MATLAB R2025b.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class C2dFohTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(C2dFohTest, FohTransferFunction)
{
    eval("sys = tf(1, [1 2 1]);"
         "function [a,b] = c2dfoh(s, Ts)\n"
         "  d = c2d(s, Ts, 'foh'); [a,b] = tfdata(d, 'v');\n"
         "end");
    eval("[b, a] = c2dfoh(sys, 0.1);");
    // FOH introduces a feedthrough → nonzero leading numerator coefficient.
    EXPECT_NEAR(evalScalar("b(1)"), 0.001585778755151036, 1e-10);
    EXPECT_NEAR(evalScalar("b(2)"), 0.006035266296524563, 1e-10);
    EXPECT_NEAR(evalScalar("b(3)"), 0.001434871954387142, 1e-10);
    EXPECT_NEAR(evalScalar("a(2)"), -1.809674836071919,   1e-9);
    EXPECT_NEAR(evalScalar("a(3)"),  0.8187307530779817,  1e-9);
}

TEST_F(C2dFohTest, FohStateSpace)
{
    eval("sc = ss([-2 -1; 1 0], [1; 0], [0 1], 0); dd = c2d(sc, 0.1, 'foh');");
    EXPECT_NEAR(evalScalar("dd.B(1)"), 0.081654159855301, 1e-10);
    EXPECT_NEAR(evalScalar("dd.B(2)"), 0.0089050102052998, 1e-10);
    EXPECT_NEAR(evalScalar("dd.D"),    0.0015857787551439, 1e-10);
    // Ad equals the ZOH Ad (same matrix exponential of A·Ts).
    EXPECT_NEAR(evalScalar("dd.A(1,1)"), 0.81435367623225, 1e-10);
}

TEST_F(C2dFohTest, FohMatchedStillDeferred)
{
    EXPECT_THROW(eval("c2d(tf(1, [1 1]), 0.1, 'matched');"), std::exception);
}
