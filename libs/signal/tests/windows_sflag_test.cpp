// libs/signal/tests/windows_sflag_test.cpp
//
// MATLAB R2025b 'periodic'/'symmetric' sflag for the 6 windows that
// accept it, and rejection of 'periodic' on the 6 windows that only
// accept 'double'/'single' typeName.
//
// Closes 12 audit ТЗ in signal.windows.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class WindowSflagTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// ── 6 windows that DO accept 'periodic' ───────────────────────────────

TEST_F(WindowSflagTest, HammingPeriodicDiffersFromSymmetric)
{
    // sym(2) = 0.253195, per(2) = 0.214731
    EXPECT_NEAR(evalScalar("ws = hamming(8); ws(2)"), 0.253195, 1e-5);
    EXPECT_NEAR(evalScalar("wp = hamming(8, 'periodic'); wp(2)"), 0.214731, 1e-5);
}

TEST_F(WindowSflagTest, HannPeriodic)
{
    EXPECT_NEAR(evalScalar("ws = hann(8); ws(2)"), 0.188255, 1e-5);
    EXPECT_NEAR(evalScalar("wp = hann(8, 'periodic'); wp(2)"), 0.146447, 1e-5);
}

TEST_F(WindowSflagTest, BlackmanPeriodic)
{
    EXPECT_NEAR(evalScalar("ws = blackman(8); ws(2)"), 0.090453, 1e-5);
    EXPECT_NEAR(evalScalar("wp = blackman(8, 'periodic'); wp(2)"), 0.066447, 1e-5);
}

TEST_F(WindowSflagTest, BlackmanharrisPeriodic)
{
    EXPECT_NEAR(evalScalar("ws = blackmanharris(8); ws(2)"), 0.033392, 1e-5);
    EXPECT_NEAR(evalScalar("wp = blackmanharris(8, 'periodic'); wp(2)"), 0.021736, 1e-5);
}

TEST_F(WindowSflagTest, FlattopwinPeriodic)
{
    EXPECT_NEAR(evalScalar("ws = flattopwin(8); ws(2)"), -0.036841, 1e-5);
    EXPECT_NEAR(evalScalar("wp = flattopwin(8, 'periodic'); wp(2)"), -0.026872, 1e-5);
}

TEST_F(WindowSflagTest, NuttallwinPeriodic)
{
    EXPECT_NEAR(evalScalar("ws = nuttallwin(8); ws(2)"), 0.037776, 1e-5);
    EXPECT_NEAR(evalScalar("wp = nuttallwin(8, 'periodic'); wp(2)"), 0.025206, 1e-5);
}

TEST_F(WindowSflagTest, ExplicitSymmetricMatchesDefault)
{
    eval("a = hann(8); b = hann(8, 'symmetric');");
    EXPECT_DOUBLE_EQ(evalScalar("max(abs(a - b))"), 0.0);
}

// ── 6 windows that REJECT 'periodic' (typeName-only) ──────────────────

TEST_F(WindowSflagTest, BartlettRejectsPeriodic)
{
    EXPECT_THROW(eval("bartlett(8, 'periodic');"), numkit::Error);
}

TEST_F(WindowSflagTest, TriangRejectsPeriodic)
{
    EXPECT_THROW(eval("triang(8, 'periodic');"), numkit::Error);
}

TEST_F(WindowSflagTest, ParzenwinRejectsPeriodic)
{
    EXPECT_THROW(eval("parzenwin(8, 'periodic');"), numkit::Error);
}

TEST_F(WindowSflagTest, BohmanwinRejectsPeriodic)
{
    EXPECT_THROW(eval("bohmanwin(8, 'periodic');"), numkit::Error);
}

TEST_F(WindowSflagTest, BarthannwinRejectsPeriodic)
{
    EXPECT_THROW(eval("barthannwin(8, 'periodic');"), numkit::Error);
}

TEST_F(WindowSflagTest, RectwinRejectsPeriodic)
{
    EXPECT_THROW(eval("rectwin(8, 'periodic');"), numkit::Error);
}

TEST_F(WindowSflagTest, BartlettAcceptsDoubleTypeName)
{
    // 'double' (default) and 'single' must not throw
    EXPECT_NO_THROW(eval("bartlett(8, 'double');"));
}
