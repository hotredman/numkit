// tests/gtest/integration/containers_test.cpp
//
// Key-value container classes on the engine object model: the modern
// value-semantics `dictionary` (R2022b+) and the legacy handle-semantics
// `containers.Map`. Validates construction, indexing (subsref/subsasgn),
// methods, properties, and — crucially — that value vs handle semantics
// fall out of the registry isHandle flag + COW clone rule.

#include "dual_engine_fixture.hpp"

using namespace m_test;
using namespace numkit;

class ContainersTest : public DualEngineTest {};

// ── dictionary (value semantics) ─────────────────────────────
TEST_P(ContainersTest, DictionaryConstructAndLookup)
{
    eval("clear; d = dictionary([\"a\" \"b\" \"c\"], [10 20 30]);");
    EXPECT_EQ(evalString("class(d)"), "dictionary");
    EXPECT_DOUBLE_EQ(evalScalar("d(\"b\")"), 20.0);
    EXPECT_DOUBLE_EQ(evalScalar("numEntries(d)"), 3.0);
}

TEST_P(ContainersTest, DictionaryInsertAndIsKey)
{
    eval("clear; d = dictionary([\"a\" \"b\"], [1 2]); d(\"c\") = 3;");
    EXPECT_DOUBLE_EQ(evalScalar("numEntries(d)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("d(\"c\")"), 3.0);
    EXPECT_TRUE(evalBool("isKey(d, \"a\")"));
    EXPECT_FALSE(evalBool("isKey(d, \"z\")"));
}

TEST_P(ContainersTest, DictionaryValueSemantics)
{
    // d2 = d is an independent copy — mutating it must not touch d.
    eval("clear; d = dictionary([\"a\"], [10]); d2 = d; d2(\"a\") = 99;");
    EXPECT_DOUBLE_EQ(evalScalar("d(\"a\")"), 10.0) << "dictionary is a value class";
    EXPECT_DOUBLE_EQ(evalScalar("d2(\"a\")"), 99.0);
}

TEST_P(ContainersTest, DictionaryNumericKeys)
{
    eval("clear; dn = dictionary([1 2 3], [100 200 300]);");
    EXPECT_DOUBLE_EQ(evalScalar("dn(2)"), 200.0);
    EXPECT_DOUBLE_EQ(evalScalar("numEntries(dn)"), 3.0);
}

TEST_P(ContainersTest, DictionaryEmpty)
{
    eval("clear; e = dictionary();");
    EXPECT_DOUBLE_EQ(evalScalar("numEntries(e)"), 0.0);
    EXPECT_FALSE(evalBool("isConfigured(e)"));
}

TEST_P(ContainersTest, DictionaryKeysValues)
{
    eval("clear; d = dictionary([\"a\" \"b\"], [10 20]);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(keys(d))"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("sum(values(d))"), 30.0);
}

// ── containers.Map (handle semantics) ────────────────────────
TEST_P(ContainersTest, MapConstructAndLookup)
{
    eval("clear; m = containers.Map({'a','b'}, {10, 20});");
    EXPECT_EQ(evalString("class(m)"), "containers.Map");
    EXPECT_DOUBLE_EQ(evalScalar("m('a')"), 10.0);
    EXPECT_DOUBLE_EQ(evalScalar("m.Count"), 2.0);
}

TEST_P(ContainersTest, MapInsertAndKeyType)
{
    eval("clear; m = containers.Map(); m('x') = 5;");
    EXPECT_DOUBLE_EQ(evalScalar("m('x')"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("m.Count"), 1.0);
    EXPECT_TRUE(evalBool("isKey(m, 'x')"));
}

TEST_P(ContainersTest, MapHandleSemantics)
{
    // m2 = m aliases the same map — mutation IS visible through m.
    eval("clear; m = containers.Map(); m('a') = 1; m2 = m; m2('a') = 99;");
    EXPECT_DOUBLE_EQ(evalScalar("m('a')"), 99.0) << "containers.Map is a handle class";
}

TEST_P(ContainersTest, MapKeysValuesLength)
{
    eval("clear; m = containers.Map({'a','b','c'}, {1,2,3});");
    EXPECT_DOUBLE_EQ(evalScalar("length(m)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("numel(keys(m))"), 3.0);
}

TEST_P(ContainersTest, MapRemove)
{
    eval("clear; m = containers.Map({'a','b'}, {1,2}); remove(m, 'a');");
    EXPECT_FALSE(evalBool("isKey(m, 'a')"));
    EXPECT_DOUBLE_EQ(evalScalar("m.Count"), 1.0);
}

// ── object display (disp / auto-display) ─────────────────────
TEST_P(ContainersTest, DictionaryDisplay)
{
    capturedOutput.clear();
    eval("clear; d = dictionary([\"a\" \"b\"], [10 20]); d");
    EXPECT_NE(capturedOutput.find("dictionary"), std::string::npos) << capturedOutput;
    EXPECT_NE(capturedOutput.find("\"a\" --> 10"), std::string::npos) << capturedOutput;
}

TEST_P(ContainersTest, MapDispBuiltin)
{
    capturedOutput.clear();
    eval("clear; m = containers.Map(); m('a') = 1; disp(m)");
    EXPECT_NE(capturedOutput.find("Map"), std::string::npos) << capturedOutput;
    EXPECT_NE(capturedOutput.find("Count"), std::string::npos) << capturedOutput;
}

INSTANTIATE_DUAL(ContainersTest);
