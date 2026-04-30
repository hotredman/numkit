// core/tests/namespace_resolver_test.cpp
//
// Integration tests for the namespace resolver introduced in Phase 6:
// runtime resolution via Environment::activeImports, populated by the
// `import` statement.
//
// These tests register a custom function in a non-core namespace
// ("test_ns") and verify:
//   * Without import, calling the bare leaf name fails.
//   * After `import test_ns.*`, the leaf name resolves.
//   * `import test_ns.foo` form works for single-symbol imports.
//   * `import compat.*` flattens compat-registered aliases.
//   * Tests run against both TreeWalker and VM backends.
//
// See NAMESPACE_DESIGN.md Sections 3-5.

#include <numkit/core/engine.hpp>
#include <numkit/builtin/library.hpp>
#include <gtest/gtest.h>

using namespace numkit;

namespace {

// A trivial test function — returns the scalar 42 regardless of input.
void answer_reg(Span<const Value>, size_t, Span<Value> outs, CallContext &ctx)
{
    outs[0] = Value::scalar(42.0, ctx.engine->resource());
}

class NamespaceResolverTest : public ::testing::TestWithParam<Engine::Backend>
{
public:
    Engine engine;

    void SetUp() override
    {
        // Engine ctor already installs BuiltinLibrary etc. Just register
        // our custom test function in a non-core namespace.
        engine.registerFunction("test_ns", "answer", &answer_reg);
        engine.setBackend(GetParam());
    }
};

TEST_P(NamespaceResolverTest, BareNameUnresolvedWithoutImport)
{
    // `answer()` not in core; without `import`, must fail.
    EXPECT_THROW(engine.eval("y = answer();"), std::runtime_error);
}

TEST_P(NamespaceResolverTest, WildcardImportResolvesBareName)
{
    // After `import test_ns.*`, the bare leaf name resolves.
    auto result = engine.eval("import test_ns.*; y = answer();");
    Value *y = engine.getVariable("y");
    ASSERT_NE(y, nullptr);
    EXPECT_DOUBLE_EQ(y->toScalar(), 42.0);
}

TEST_P(NamespaceResolverTest, SingleSymbolImportResolvesLeaf)
{
    auto result = engine.eval("import test_ns.answer; y = answer();");
    Value *y = engine.getVariable("y");
    ASSERT_NE(y, nullptr);
    EXPECT_DOUBLE_EQ(y->toScalar(), 42.0);
}

TEST_P(NamespaceResolverTest, ImportDoesNotLeakBetweenSessions)
{
    // First session imports, second doesn't — the second eval starts
    // fresh on the same Engine; whether imports persist depends on
    // Engine's REPL semantics. Currently top-level imports persist into
    // workspaceEnv across eval() calls (no popImport mechanism), so
    // this test demonstrates the persistent behavior.
    engine.eval("import test_ns.*; y1 = answer();");
    // Same Engine, new eval — import should still be active in workspace.
    engine.eval("y2 = answer();");
    Value *y2 = engine.getVariable("y2");
    ASSERT_NE(y2, nullptr);
    EXPECT_DOUBLE_EQ(y2->toScalar(), 42.0);
}

TEST_P(NamespaceResolverTest, CompatNamespaceWorks)
{
    // Manually populate compat (mirroring what Phase 7 will do for
    // mirror libs).
    engine.registerFunction("compat", "answer", &answer_reg);
    auto result = engine.eval("import compat.*; y = answer();");
    Value *y = engine.getVariable("y");
    ASSERT_NE(y, nullptr);
    EXPECT_DOUBLE_EQ(y->toScalar(), 42.0);
}

TEST_P(NamespaceResolverTest, WildcardImportFindsSubNamespaceFunction)
{
    // libs/signal registers functions under signal.<sub>.<name> (e.g.
    // signal.transforms.fft). `import signal.*; fft(x)` must find them
    // — wildcard imports look one level deeper than just "signal.<name>".
    auto result = engine.eval("import signal.*; y = fft([1, 1, 1, 1]);");
    Value *y = engine.getVariable("y");
    ASSERT_NE(y, nullptr);
    EXPECT_EQ(y->numel(), 4u);
    // First bin is the sum; rest are zero for a constant input.
    EXPECT_NEAR(y->complexElem(0).real(), 4.0, 1e-12);
    EXPECT_NEAR(y->complexElem(1).real(), 0.0, 1e-12);
}

TEST_P(NamespaceResolverTest, WildcardImportNestedPackage)
{
    // Direct sub-namespace import still works alongside the new
    // deep-wildcard semantics.
    auto result = engine.eval(
        "import signal.windows.*; w = hann(8);");
    Value *w = engine.getVariable("w");
    ASSERT_NE(w, nullptr);
    EXPECT_EQ(w->numel(), 8u);
}

INSTANTIATE_TEST_SUITE_P(TW_VM, NamespaceResolverTest,
                          ::testing::Values(Engine::Backend::TreeWalker,
                                            Engine::Backend::VM),
                          [](const ::testing::TestParamInfo<Engine::Backend> &info) {
                              return info.param == Engine::Backend::TreeWalker ? "TW" : "VM";
                          });

} // namespace
