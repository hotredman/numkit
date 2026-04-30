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
#include <numkit/core/vfs.hpp>

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
    Engine engine;
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

INSTANTIATE_TEST_SUITE_P(TW_VM, PackageNamespaceTest,
                          ::testing::Values(Engine::Backend::TreeWalker,
                                            Engine::Backend::VM),
                          [](const ::testing::TestParamInfo<Engine::Backend> &info) {
                              return info.param == Engine::Backend::TreeWalker ? "TW" : "VM";
                          });

} // namespace
