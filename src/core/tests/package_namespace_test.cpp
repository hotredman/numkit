// core/tests/package_namespace_test.cpp
//
// Phase 10 — `+packagedir/` user namespaces via VFS.
// Verifies:
//   * `+pkg/foo.m` resolves through `import pkg.*; foo(...)`
//   * Single-symbol `import pkg.foo; foo(...)`
//   * Nested `+pkg/+sub/bar.m` via `import pkg.sub.*`
//   * Imports do NOT leak the qualified name into the global resolver
//     (calling bare `foo()` without import still fails)
//   * Mtime-based re-parse after editing a +pkg file

#include <numkit/core/engine.hpp>
#include <numkit/fs/vfs.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <thread>

using namespace numkit;

namespace {

class PackageNamespaceTest : public ::testing::TestWithParam<Engine::Backend>
{
protected:
    StandardEngine engine;
    std::filesystem::path workDir;

    void SetUp() override
    {
        const auto *info = ::testing::UnitTest::GetInstance()->current_test_info();
        workDir = std::filesystem::temp_directory_path()
                  / (std::string{"numkit-pkg-test-"} + info->name());
        std::error_code ec;
        std::filesystem::remove_all(workDir, ec);
        std::filesystem::create_directories(workDir);
        engine.setBackend(GetParam());
        engine.eval("import compat.*;");
    }

    void TearDown() override
    {
        std::error_code ec;
        std::filesystem::remove_all(workDir, ec);
    }

    void writeFile(const std::filesystem::path &p, const std::string &content)
    {
        std::filesystem::create_directories(p.parent_path());
        std::ofstream f(p, std::ios::binary);
        f << content;
    }
};

TEST_P(PackageNamespaceTest, WildcardImportResolvesPackageMFile)
{
    writeFile(workDir / "+mypkg" / "double_it.m",
              "function y = double_it(x)\n  y = 2 * x;\nend\n");
    engine.addPath(workDir.string());

    engine.eval("import mypkg.*; y = double_it(7);");
    auto *y = engine.getVariable("y");
    ASSERT_NE(y, nullptr);
    EXPECT_DOUBLE_EQ(y->toScalar(), 14.0);
}

TEST_P(PackageNamespaceTest, SingleSymbolImportResolvesPackageMFile)
{
    writeFile(workDir / "+mypkg" / "triple.m",
              "function y = triple(x)\n  y = 3 * x;\nend\n");
    engine.addPath(workDir.string());

    engine.eval("import mypkg.triple; y = triple(4);");
    auto *y = engine.getVariable("y");
    ASSERT_NE(y, nullptr);
    EXPECT_DOUBLE_EQ(y->toScalar(), 12.0);
}

TEST_P(PackageNamespaceTest, NestedPackageResolves)
{
    writeFile(workDir / "+mypkg" / "+sub" / "quad.m",
              "function y = quad(x)\n  y = 4 * x;\nend\n");
    engine.addPath(workDir.string());

    engine.eval("import mypkg.sub.*; y = quad(5);");
    auto *y = engine.getVariable("y");
    ASSERT_NE(y, nullptr);
    EXPECT_DOUBLE_EQ(y->toScalar(), 20.0);
}

TEST_P(PackageNamespaceTest, DirectQualifiedCallResolves)
{
    writeFile(workDir / "+mathx" / "shift.m",
              "function y = shift(x)\n  y = x + 100;\nend\n");
    engine.addPath(workDir.string());

    engine.eval("y = mathx.shift(7);");
    auto *y = engine.getVariable("y");
    ASSERT_NE(y, nullptr);
    EXPECT_DOUBLE_EQ(y->toScalar(), 107.0);
}

TEST_P(PackageNamespaceTest, DirectNestedQualifiedCallResolves)
{
    writeFile(workDir / "+nx" / "+geom" / "tri.m",
              "function y = tri(x)\n  y = 3 * x;\nend\n");
    engine.addPath(workDir.string());

    engine.eval("y = nx.geom.tri(8);");
    auto *y = engine.getVariable("y");
    ASSERT_NE(y, nullptr);
    EXPECT_DOUBLE_EQ(y->toScalar(), 24.0);
}

TEST_P(PackageNamespaceTest, LocalVariableShadowsNamespace)
{
    // If a workspace variable named `mathx` exists, `mathx.field` must
    // resolve as struct/dot access, not as a namespace.
    writeFile(workDir / "+mathx" / "shift.m",
              "function y = shift(x)\n  y = x + 100;\nend\n");
    engine.addPath(workDir.string());

    // Make `mathx` a struct in the workspace.
    engine.eval("mathx = struct('shift', 999);");
    engine.eval("v = mathx.shift;");
    auto *v = engine.getVariable("v");
    ASSERT_NE(v, nullptr);
    EXPECT_DOUBLE_EQ(v->toScalar(), 999.0);
}

TEST_P(PackageNamespaceTest, BareCallWithoutImportFails)
{
    writeFile(workDir / "+secrets" / "answer.m",
              "function y = answer(x)\n  y = 42;\nend\n");
    engine.addPath(workDir.string());

    // Without `import secrets.*`, bare `answer()` should fail — the
    // package namespace is not auto-flattened.
    EXPECT_THROW(engine.eval("y = answer(0);"), std::exception);
}

TEST_P(PackageNamespaceTest, ImportPersistsAcrossEvals)
{
    writeFile(workDir / "+keep" / "f.m",
              "function y = f(x)\n  y = x + 1;\nend\n");
    engine.addPath(workDir.string());
    engine.eval("import keep.*; a = f(10);");
    EXPECT_DOUBLE_EQ(engine.getVariable("a")->toScalar(), 11.0);
    // Second eval, no re-import — does workspaceEnv carry the import?
    engine.eval("b = f(20);");
    EXPECT_DOUBLE_EQ(engine.getVariable("b")->toScalar(), 21.0);
}

TEST_P(PackageNamespaceTest, RehashPicksUpEditedPackageFile)
{
    auto vfile = workDir / "+livepkg" / "ver_fn.m";
    writeFile(vfile, "function v = ver_fn()\n  v = 1;\nend\n");
    engine.addPath(workDir.string());
    engine.eval("import livepkg.*; v1 = ver_fn();");
    EXPECT_DOUBLE_EQ(engine.getVariable("v1")->toScalar(), 1.0);

    // Disk-level: mtime granularity is OS/FS dependent. Sleep past it
    // and remove-then-write so the new file is unambiguously distinct.
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    std::filesystem::remove(vfile);
    writeFile(vfile, "function v = ver_fn()\n  v = 2;\nend\n");
    engine.rehashMFiles();
    engine.eval("v2 = ver_fn();");
    EXPECT_DOUBLE_EQ(engine.getVariable("v2")->toScalar(), 2.0);
}

// ── Edge cases (Batch A audit) ─────────────────────────────

TEST_P(PackageNamespaceTest, RecursivePackageCallSelf)
{
    writeFile(workDir / "+rec" / "fact.m",
              "function y = fact(n)\n"
              "  if n <= 1\n"
              "    y = 1;\n"
              "  else\n"
              "    y = n * rec.fact(n - 1);\n"
              "  end\n"
              "end\n");
    engine.addPath(workDir.string());
    engine.eval("y = rec.fact(5);");
    auto *y = engine.getVariable("y");
    ASSERT_NE(y, nullptr);
    EXPECT_DOUBLE_EQ(y->toScalar(), 120.0);
}

TEST_P(PackageNamespaceTest, CrossPackageCall)
{
    writeFile(workDir / "+lib_a" / "double_x.m",
              "function y = double_x(x)\n  y = 2 * x;\nend\n");
    writeFile(workDir / "+lib_b" / "via_a.m",
              "function y = via_a(x)\n  y = lib_a.double_x(x) + 1;\nend\n");
    engine.addPath(workDir.string());
    engine.eval("y = lib_b.via_a(10);");
    auto *y = engine.getVariable("y");
    ASSERT_NE(y, nullptr);
    EXPECT_DOUBLE_EQ(y->toScalar(), 21.0);
}

TEST_P(PackageNamespaceTest, LocalFunctionVariableShadowsNamespace)
{
    // Inside a function body, a local variable named like the namespace
    // root must shadow the namespace just like at workspace scope.
    writeFile(workDir / "+nsx" / "answer.m",
              "function y = answer()\n  y = 7;\nend\n");
    writeFile(workDir / "use_local.m",
              "function y = use_local()\n"
              "  nsx = struct('answer', 99);\n"
              "  y = nsx.answer;\n"
              "end\n");
    engine.addPath(workDir.string());
    engine.eval("y = use_local();");
    auto *y = engine.getVariable("y");
    ASSERT_NE(y, nullptr);
    EXPECT_DOUBLE_EQ(y->toScalar(), 99.0);
}

TEST_P(PackageNamespaceTest, MultiImportSameLeafInnermostWins)
{
    // Two packages both define `target.m`. With both imported as
    // wildcards, MATLAB resolves "innermost-import wins" — the
    // last-pushed import shadows the earlier one when looking up the
    // bare name. Our env walks innermost-first, so behaviour matches.
    writeFile(workDir / "+lib_x" / "target.m",
              "function y = target(z)\n  y = z + 1;\nend\n");
    writeFile(workDir / "+lib_y" / "target.m",
              "function y = target(z)\n  y = z + 100;\nend\n");
    engine.addPath(workDir.string());
    engine.eval("import lib_x.*; import lib_y.*; r = target(0);");
    auto *r = engine.getVariable("r");
    ASSERT_NE(r, nullptr);
    EXPECT_DOUBLE_EQ(r->toScalar(), 100.0);  // lib_y (last imported) wins
}

TEST_P(PackageNamespaceTest, AliasImportResolvesUserPackage)
{
    // `import a.b as alias` then `alias.foo(x)` rewrites the alias
    // prefix and resolves the user-package m-file as `a.b.foo`.
    writeFile(workDir / "+aliaspkg" / "fn.m",
              "function y = fn(x)\n  y = x;\nend\n");
    engine.addPath(workDir.string());
    auto result = engine.eval("import aliaspkg as ap; y = ap.fn(7);");
    Value *y = engine.getVariable("y");
    ASSERT_NE(y, nullptr);
    EXPECT_DOUBLE_EQ(y->toScalar(), 7.0);
}

TEST_P(PackageNamespaceTest, LargeStructArrayAutoGrow)
{
    engine.eval("clear g;");
    // 50 incremental writes. Each grows the array if needed.
    for (int i = 1; i <= 50; ++i)
        engine.eval("g(" + std::to_string(i) + ").x = "
                    + std::to_string(i * 2) + ";");
    auto *g = engine.getVariable("g");
    ASSERT_NE(g, nullptr);
    EXPECT_EQ(g->numel(), 50u);
    engine.eval("v = g(50).x;");
    EXPECT_DOUBLE_EQ(engine.getVariable("v")->toScalar(), 100.0);
}

TEST_P(PackageNamespaceTest, QualifiedCallFromFunctionBody)
{
    // Direct qualified call originating from inside another function's
    // body — root-identifier shadow check must consult the local
    // scope, not just workspace.
    writeFile(workDir / "+nspkg" / "double_it.m",
              "function y = double_it(x)\n  y = 2*x;\nend\n");
    writeFile(workDir / "caller.m",
              "function y = caller(z)\n  y = nspkg.double_it(z) + 1;\nend\n");
    engine.addPath(workDir.string());
    engine.eval("y = caller(10);");
    auto *y = engine.getVariable("y");
    ASSERT_NE(y, nullptr);
    EXPECT_DOUBLE_EQ(y->toScalar(), 21.0);
}

TEST_P(PackageNamespaceTest, SameLeafInTwoPackagesNoCollision)
{
    // +pkg_a/foo.m returns input + 1.
    // +pkg_b/foo.m returns input + 100.
    // Both leaves are "foo" — they MUST not collide in the compiler's
    // chunk map (which historically keyed by leaf).
    writeFile(workDir / "+pkg_a" / "foo.m",
              "function y = foo(x)\n  y = x + 1;\nend\n");
    writeFile(workDir / "+pkg_b" / "foo.m",
              "function y = foo(x)\n  y = x + 100;\nend\n");
    engine.addPath(workDir.string());

    engine.eval("a = pkg_a.foo(10);");
    EXPECT_DOUBLE_EQ(engine.getVariable("a")->toScalar(), 11.0);

    engine.eval("b = pkg_b.foo(10);");
    EXPECT_DOUBLE_EQ(engine.getVariable("b")->toScalar(), 110.0);

    // Now call pkg_a.foo AGAIN — must still hit the +1 version, not
    // get clobbered by the +100 chunk.
    engine.eval("c = pkg_a.foo(10);");
    EXPECT_DOUBLE_EQ(engine.getVariable("c")->toScalar(), 11.0);
}

INSTANTIATE_TEST_SUITE_P(TW_VM, PackageNamespaceTest,
                          ::testing::Values(Engine::Backend::TreeWalker,
                                            Engine::Backend::VM),
                          [](const ::testing::TestParamInfo<Engine::Backend> &info) {
                              return info.param == Engine::Backend::TreeWalker ? "TW" : "VM";
                          });

} // namespace
