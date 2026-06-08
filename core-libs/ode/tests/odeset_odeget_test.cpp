// toolboxes/ode/tests/odeset_odeget_test.cpp
//
// Regression guard for odeset / odeget — options struct constructor
// and getter for the ODE solver family. Pinned against MATLAB R2025b
// (tools/parity/specs/odeset.json, odeget.json).

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class OdeOptionsTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// ── odeset ──────────────────────────────────────────────────────────

// odeset() with no args returns a struct of defaults (all fields
// empty per MATLAB convention).
TEST_F(OdeOptionsTest, OdesetNoArgsReturnsStruct)
{
    eval("o = odeset();");
    EXPECT_EQ(static_cast<int>(evalScalar("isstruct(o)")), 1);
    EXPECT_EQ(static_cast<int>(evalScalar("isempty(o.RelTol)")), 1);
    EXPECT_EQ(static_cast<int>(evalScalar("isempty(o.AbsTol)")), 1);
}

TEST_F(OdeOptionsTest, OdesetNameValuePairs)
{
    eval("o = odeset('RelTol', 1e-9, 'AbsTol', 1e-12, 'MaxStep', 0.05, 'Refine', 6);");
    EXPECT_DOUBLE_EQ(evalScalar("o.RelTol"),  1e-9);
    EXPECT_DOUBLE_EQ(evalScalar("o.AbsTol"),  1e-12);
    EXPECT_DOUBLE_EQ(evalScalar("o.MaxStep"), 0.05);
    EXPECT_DOUBLE_EQ(evalScalar("o.Refine"),  6.0);
}

// Case-insensitive name lookup — stored under canonical capitalisation.
TEST_F(OdeOptionsTest, OdesetCaseInsensitiveNames)
{
    eval("o = odeset('reltol', 1e-7, 'AbStOl', 1e-9, 'NORMCONTROL', 'on');");
    EXPECT_DOUBLE_EQ(evalScalar("o.RelTol"), 1e-7);
    EXPECT_DOUBLE_EQ(evalScalar("o.AbsTol"), 1e-9);
    EXPECT_EQ(static_cast<int>(evalScalar("ischar(o.NormControl) || isstring(o.NormControl)")), 1);
}

// odeset(oldstruct, ...) merges new values onto old.
TEST_F(OdeOptionsTest, OdesetMergesExistingStruct)
{
    eval("o1 = odeset('RelTol', 1e-9, 'AbsTol', 1e-12);"
         "o2 = odeset(o1, 'RelTol', 1e-6, 'MaxStep', 0.1);");
    EXPECT_DOUBLE_EQ(evalScalar("o2.RelTol"),  1e-6);   // overridden
    EXPECT_DOUBLE_EQ(evalScalar("o2.AbsTol"),  1e-12);  // preserved
    EXPECT_DOUBLE_EQ(evalScalar("o2.MaxStep"), 0.1);    // new
}

// Unknown option name raises.
TEST_F(OdeOptionsTest, OdesetUnknownNameThrows)
{
    EXPECT_THROW(eval("odeset('NotARealOption', 1);"), std::exception);
}

// Trailing un-paired argument raises.
TEST_F(OdeOptionsTest, OdesetTrailingUnpairedThrows)
{
    EXPECT_THROW(eval("odeset('RelTol', 1e-3, 'AbsTol');"), std::exception);
}

// ── odeget ──────────────────────────────────────────────────────────

TEST_F(OdeOptionsTest, OdegetReturnsStoredValue)
{
    eval("o = odeset('RelTol', 1e-9, 'AbsTol', 1e-12, 'MaxStep', 0.05);"
         "rt = odeget(o, 'RelTol');"
         "at = odeget(o, 'AbsTol');"
         "ms = odeget(o, 'MaxStep');");
    EXPECT_DOUBLE_EQ(evalScalar("rt"), 1e-9);
    EXPECT_DOUBLE_EQ(evalScalar("at"), 1e-12);
    EXPECT_DOUBLE_EQ(evalScalar("ms"), 0.05);
}

TEST_F(OdeOptionsTest, OdegetCaseInsensitiveLookup)
{
    eval("o = odeset('RelTol', 1e-9);"
         "v = odeget(o, 'reltol');");
    EXPECT_DOUBLE_EQ(evalScalar("v"), 1e-9);
}

TEST_F(OdeOptionsTest, OdegetFallsBackToDefaultWhenEmpty)
{
    eval("o = odeset('RelTol', 1e-9);"
         "j = odeget(o, 'Jacobian', 42);");
    EXPECT_DOUBLE_EQ(evalScalar("j"), 42.0);
}

TEST_F(OdeOptionsTest, OdegetNoDefaultReturnsEmpty)
{
    eval("o = odeset('RelTol', 1e-9);"
         "j = odeget(o, 'Jacobian');");
    EXPECT_EQ(static_cast<int>(evalScalar("isempty(j)")), 1);
}

TEST_F(OdeOptionsTest, OdegetUnknownNameThrows)
{
    eval("o = odeset('RelTol', 1e-9);");
    EXPECT_THROW(eval("odeget(o, 'NotARealOption');"), std::exception);
}
