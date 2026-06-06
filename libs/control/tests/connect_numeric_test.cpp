// libs/control/tests/connect_numeric_test.cpp
//
// Regression guard: feedback/series/parallel accept a NUMERIC scalar as a
// static gain system (K = K/1), like MATLAB. Previously the connect helper
// threw "expected tf/zpk/ss struct" for e.g. feedback(sys,1) (unity
// feedback). Expected dcgain values verified vs MATLAB R2025b.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class ConnectNumericTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(ConnectNumericTest, FeedbackUnityGain)
{
    // feedback(1/(s+1), 1) = 1/(s+2); dcgain 0.5.
    EXPECT_NEAR(evalScalar("dcgain(feedback(tf(1,[1 1]),1))"), 0.5, 1e-9);
}

TEST_F(ConnectNumericTest, FeedbackScalarGain)
{
    // feedback(1/(s+1), 2) = 1/(s+3); dcgain 1/3.
    EXPECT_NEAR(evalScalar("dcgain(feedback(tf(1,[1 1]),2))"), 1.0 / 3.0, 1e-9);
}

TEST_F(ConnectNumericTest, FeedbackNumericFirstArg)
{
    // feedback(2, 1/(s+1)) = 2(s+1)/(s+3); dcgain 2/3.
    EXPECT_NEAR(evalScalar("dcgain(feedback(2,tf(1,[1 1])))"), 2.0 / 3.0, 1e-9);
}

TEST_F(ConnectNumericTest, SeriesScalarGain)
{
    // series(2/(s+1), 3) -> 6/(s+1); dcgain 6.
    EXPECT_NEAR(evalScalar("dcgain(series(tf(2,[1 1]),3))"), 6.0, 1e-9);
}

TEST_F(ConnectNumericTest, ParallelScalarGain)
{
    // parallel(2/(s+1), 3) dcgain 2 + 3 = 5.
    EXPECT_NEAR(evalScalar("dcgain(parallel(tf(2,[1 1]),3))"), 5.0, 1e-9);
}

TEST_F(ConnectNumericTest, DiscreteInheritsSampleTime)
{
    // A numeric gain has unspecified Ts -> inherits the discrete system's Ts
    // (no "sample times must match" error).
    EXPECT_NO_THROW(eval("feedback(tf(1,[1 -0.5],0.1),1);"));
}

TEST_F(ConnectNumericTest, TfTimesTfUnchanged)
{
    // The struct x struct path is unchanged: dcgain 2 * 1.5 = 3.
    EXPECT_NEAR(evalScalar("dcgain(series(tf(2,[1 1]),tf(3,[1 2])))"), 3.0, 1e-9);
}

TEST_F(ConnectNumericTest, NonScalarNumericThrows)
{
    EXPECT_THROW(eval("series(tf(1,[1 1]),[1 2 3]);"), std::exception);
}
