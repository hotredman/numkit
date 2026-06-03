// libs/control/tests/known_bugs_test.cpp
//
// One DISABLED_ test per OPEN bug in bugs/control/*.md. Disabled until
// fixed; remove `DISABLED_` to turn into a live regression guard.
// MATLAB R2025b reference values.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class ControlKnownBug : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// bugs/control/lqr-hinfnorm.md — LQR gain via the CARE.
TEST_F(ControlKnownBug, DISABLED_Lqr)
{
    eval("K = lqr([0 1; 0 0], [0; 1], eye(2), 1);");
    EXPECT_NEAR(evalScalar("K(1)"), 1.000000, 1e-5);
    EXPECT_NEAR(evalScalar("K(2)"), 1.732051, 1e-5);
}

// bugs/control/lqr-hinfnorm.md — H-infinity norm (Inf for poles on jω axis).
TEST_F(ControlKnownBug, DISABLED_Hinfnorm)
{
    eval("g = hinfnorm(ss([0 1; -1 0], [0; 1], [1 0], 0));");
    EXPECT_TRUE(std::isinf(evalScalar("g")));
}
