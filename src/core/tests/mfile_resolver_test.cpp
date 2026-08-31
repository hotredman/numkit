// core/tests/mfile_resolver_test.cpp
//
// Phase 9a integration tests — m-file resolver via VFS + AST cache.
// Verifies:
//   * `addpath` (C++ API) wires a directory into the resolver
//   * `<name>.m` containing `function y = name(x) ... end` resolves
//     when called by short name from a script
//   * Both TW and VM dispatch through the same resolver pass
//   * Mtime-based cache invalidation re-parses a modified file
//   * `rehashMFiles` drops the cache wholesale

#include <numkit/core/engine.hpp>
#include <numkit/fs/vfs.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <map>
#include <thread>

using namespace numkit;

namespace {

// Minimal in-memory VFS — used only by SiblingResolvesThroughTemporaryFS
// to verify sibling lookup routes through the script's origin VFS.
class TestMemoryFS final : public VirtualFS
{
public:
    explicit TestMemoryFS(std::string n) : name_(std::move(n)) {}
    std::string readFile(const std::string &path) override
    {
        auto it = files_.find(path);
        if (it == files_.end()) throw std::runtime_error(name_ + ": no such file");
        return it->second;
    }
    void writeFile(const std::string &p, const std::string &c) override { files_[p] = c; }
    bool exists(const std::string &p) override { return files_.count(p) > 0; }
    std::string name() const override { return name_; }
    std::map<std::string, std::string> files_;
private:
    std::string name_;
};

class MFileResolverTest : public ::testing::TestWithParam<Engine::Backend>
{
protected:
    StandardEngine engine;
    std::filesystem::path workDir;

    void SetUp() override
    {
        const auto *info = ::testing::UnitTest::GetInstance()->current_test_info();
        workDir = std::filesystem::temp_directory_path()
                  / (std::string{"numkit-mfile-test-"} + info->name());
        std::error_code ec;
        std::filesystem::remove_all(workDir, ec);
        std::filesystem::create_directories(workDir);

        engine.setBackend(GetParam());
            }

    void TearDown() override
    {
        std::error_code ec;
        std::filesystem::remove_all(workDir, ec);
    }

    void writeMFile(const std::string &name, const std::string &content) const
    {
        std::ofstream f(workDir / name, std::ios::binary);
        f << content;
    }
};

TEST_P(MFileResolverTest, AddPathFindsMFile)
{
    writeMFile("hello.m", "function y = hello(x)\n  y = x + 100;\nend\n");
    engine.addPath(workDir.string());

    auto result = engine.eval("y = hello(7);");
    Value *y = engine.getVariable("y");
    ASSERT_NE(y, nullptr);
    EXPECT_DOUBLE_EQ(y->toScalar(), 107.0);
}

TEST_P(MFileResolverTest, MultiOutputMFileResolves)
{
    writeMFile("split.m",
               "function [a, b] = split(x)\n  a = x;\n  b = x + 1;\nend\n");
    engine.addPath(workDir.string());

    engine.eval("[a, b] = split(5);");
    EXPECT_DOUBLE_EQ(engine.getVariable("a")->toScalar(), 5.0);
    EXPECT_DOUBLE_EQ(engine.getVariable("b")->toScalar(), 6.0);
}

TEST_P(MFileResolverTest, MFileCallsBuiltin)
{
    // Verify the m-file's body can call core builtins (the body env's
    // parent is constantsEnv so workspace imports must propagate via
    // findExternal's workspace-fallback).
    writeMFile("mag.m",
               "function r = mag(z)\n  r = abs(z);\nend\n");
    engine.addPath(workDir.string());

    engine.eval("r = mag(-3);");
    EXPECT_DOUBLE_EQ(engine.getVariable("r")->toScalar(), 3.0);
}

TEST_P(MFileResolverTest, UnresolvedNameThrows)
{
    engine.addPath(workDir.string());
    EXPECT_THROW(engine.eval("y = no_such_function(1);"), std::exception);
}

TEST_P(MFileResolverTest, RmPathDropsResolution)
{
    writeMFile("zap.m", "function y = zap(x)\n  y = x * 2;\nend\n");
    engine.addPath(workDir.string());
    engine.eval("y = zap(4);");
    EXPECT_DOUBLE_EQ(engine.getVariable("y")->toScalar(), 8.0);

    // After rmPath + rehash, a fresh name (not yet cached) should fail.
    engine.rmPath(workDir.string());
    engine.rehashMFiles();
    EXPECT_THROW(engine.eval("y = zap(4);"), std::exception);
}

TEST_P(MFileResolverTest, RehashClearsCache)
{
    writeMFile("freshen.m", "function y = freshen(x)\n  y = x + 1;\nend\n");
    engine.addPath(workDir.string());
    engine.eval("a = freshen(10);");
    EXPECT_DOUBLE_EQ(engine.getVariable("a")->toScalar(), 11.0);

    // Modify the file: function body returns x + 999.
    writeMFile("freshen.m", "function y = freshen(x)\n  y = x + 999;\nend\n");
    engine.rehashMFiles();
    engine.eval("b = freshen(10);");
    EXPECT_DOUBLE_EQ(engine.getVariable("b")->toScalar(), 1009.0);
}

TEST_P(MFileResolverTest, AutomaticRefreshOnMFileModification)
{
    writeMFile("autofresh.m", "function y = autofresh(x)\n  y = x + 1;\nend\n");
    engine.addPath(workDir.string());
    engine.eval("a = autofresh(10);");
    EXPECT_DOUBLE_EQ(engine.getVariable("a")->toScalar(), 11.0);

    // Modify the file without calling rehash
    writeMFile("autofresh.m", "function y = autofresh(x)\n  y = x + 500;\nend\n");
    engine.eval("b = autofresh(10);");
    EXPECT_DOUBLE_EQ(engine.getVariable("b")->toScalar(), 510.0);
}

// ── Phase 9b — builtin wrappers (addpath / rmpath / path / which / exist / run / rehash) ──

TEST_P(MFileResolverTest, AddpathBuiltinWiresPath)
{
    writeMFile("addpath_test.m", "function y = addpath_test(x)\n  y = x * 3;\nend\n");
    engine.eval("addpath('" + workDir.string() + "');");
    engine.eval("y = addpath_test(5);");
    EXPECT_DOUBLE_EQ(engine.getVariable("y")->toScalar(), 15.0);
}

TEST_P(MFileResolverTest, ExistFileTypeFilter)
{
    writeMFile("hi.m", "function y = hi(x)\n  y = x;\nend\n");
    engine.addPath(workDir.string());

    engine.eval("c1 = exist('hi', 'file');");
    EXPECT_DOUBLE_EQ(engine.getVariable("c1")->toScalar(), 2.0);

    engine.eval("c2 = exist('nonexistent', 'file');");
    EXPECT_DOUBLE_EQ(engine.getVariable("c2")->toScalar(), 0.0);

    engine.eval("c3 = exist('sin', 'builtin');");
    EXPECT_DOUBLE_EQ(engine.getVariable("c3")->toScalar(), 5.0);
}

TEST_P(MFileResolverTest, RehashBuiltinWorks)
{
    writeMFile("ver.m", "function y = ver(x)\n  y = x + 1;\nend\n");
    engine.addPath(workDir.string());
    engine.eval("a = ver(10);");
    EXPECT_DOUBLE_EQ(engine.getVariable("a")->toScalar(), 11.0);

    writeMFile("ver.m", "function y = ver(x)\n  y = x + 99;\nend\n");
    engine.eval("rehash;");
    engine.eval("b = ver(10);");
    EXPECT_DOUBLE_EQ(engine.getVariable("b")->toScalar(), 109.0);
}

TEST_P(MFileResolverTest, RunBuiltinExecutesScript)
{
    writeMFile("script.m", "g_result = 42;");
    auto p = (workDir / "script.m").string();
    engine.eval("run('" + p + "');");
    EXPECT_DOUBLE_EQ(engine.getVariable("g_result")->toScalar(), 42.0);
}

// ── Sibling resolution: script calls helper in same dir ─────────
//
// MATLAB-standard behaviour: when `script.m` runs from a directory
// that also contains `helper.m`, calling `helper(x)` from inside
// `script.m` resolves automatically — no addpath needed. The
// script's containing directory is implicitly part of the search
// path for the duration of the run.
//
// This block pins what works today and what doesn't.

TEST_P(MFileResolverTest, RunScriptCallsSiblingFunctionWithoutAddpath)
{
    writeMFile("helper.m", "function y = helper(x)\n  y = x * 10;\nend\n");
    writeMFile("caller.m", "g = helper(7);\n");
    auto p = (workDir / "caller.m").string();
    // No engine.addPath — the script's own directory should be
    // implicit. Currently fails because resolveMFile_ pushes the
    // filesystem-origin NAME (e.g. "native") into searchDirs and
    // never the script's directory.
    EXPECT_NO_THROW(engine.eval("run('" + p + "');"));
    auto *g = engine.getVariable("g");
    ASSERT_NE(g, nullptr);
    EXPECT_DOUBLE_EQ(g->toScalar(), 70.0);
}

TEST_P(MFileResolverTest, RunScriptCallsSiblingFunctionResolvesViaAddpath)
{
    // Control: with explicit addpath, the same sibling call works.
    // If this passes while the previous one fails, we've localized
    // the bug to "script-dir not auto-added to search path".
    writeMFile("helper.m", "function y = helper(x)\n  y = x * 10;\nend\n");
    writeMFile("caller.m", "g = helper(7);\n");
    engine.addPath(workDir.string());
    auto p = (workDir / "caller.m").string();
    EXPECT_NO_THROW(engine.eval("run('" + p + "');"));
    auto *g = engine.getVariable("g");
    ASSERT_NE(g, nullptr);
    EXPECT_DOUBLE_EQ(g->toScalar(), 70.0);
}

// ── #1 — Transitive run(): nested run inherits its own script-dir ─────
//
// caller.m runs inner.m via run(). inner.m must see ITS OWN sibling
// (inner_helper.m), not caller.m's. Stack push/pop semantics for
// scriptOriginStack_ must isolate per-frame.
TEST_P(MFileResolverTest, TransitiveRunResolvesSiblingsAtEachLevel)
{
    auto outer = workDir / "outer";
    auto inner = workDir / "inner";
    std::filesystem::create_directories(outer);
    std::filesystem::create_directories(inner);

    {
        std::ofstream f(outer / "caller.m");
        f << "outer_g = outer_helper(2);\n"
          << "run('" << (inner / "inner.m").generic_string() << "');\n";
    }
    {
        std::ofstream f(outer / "outer_helper.m");
        f << "function y = outer_helper(x)\n  y = x * 100;\nend\n";
    }
    {
        std::ofstream f(inner / "inner.m");
        f << "inner_g = inner_helper(3);\n";
    }
    {
        std::ofstream f(inner / "inner_helper.m");
        f << "function y = inner_helper(x)\n  y = x * 1000;\nend\n";
    }

    auto p = (outer / "caller.m").generic_string();
    EXPECT_NO_THROW(engine.eval("run('" + p + "');"));
    EXPECT_DOUBLE_EQ(engine.getVariable("outer_g")->toScalar(), 200.0);
    EXPECT_DOUBLE_EQ(engine.getVariable("inner_g")->toScalar(), 3000.0);
}

// ── #2 — Origin stack pops on exception thrown by the script ──
//
// run('throws.m') propagates the exception out, but must still leave
// scriptOriginStack_ in its prior state. Catch the exception, then
// verify a subsequent name lookup matches the prior scope (not the
// throwing-script's dir).
TEST_P(MFileResolverTest, ScriptOriginPoppedOnException)
{
    auto good = workDir / "good";
    auto bad = workDir / "bad";
    std::filesystem::create_directories(good);
    std::filesystem::create_directories(bad);
    {
        std::ofstream f(bad / "throws.m");
        f << "error('intentional failure');\n";
    }
    {
        std::ofstream f(bad / "should_not_resolve.m");
        f << "function y = should_not_resolve(x)\n  y = x;\nend\n";
    }

    auto p = (bad / "throws.m").generic_string();
    EXPECT_THROW(engine.eval("run('" + p + "');"), std::exception);

    // After the throw, the bad/ dir must NOT be on the implicit search
    // path. should_not_resolve sits in bad/ — calling it from base
    // workspace should fail.
    EXPECT_THROW(engine.eval("z = should_not_resolve(5);"), std::exception);
}

// ── #3 — Cleanup after run: script-dir is not visible from base ─
//
// After a successful run('a.m') returns, the implicit script-dir is
// gone. A bare top-level call to a name that only resolves via that
// dir must fail.
TEST_P(MFileResolverTest, SiblingNotVisibleAfterRunReturns)
{
    writeMFile("ran_caller.m", "g_x = 1;\n");
    writeMFile("only_via_dir.m",
               "function y = only_via_dir(x)\n  y = x + 7;\nend\n");

    auto p = (workDir / "ran_caller.m").generic_string();
    EXPECT_NO_THROW(engine.eval("run('" + p + "');"));
    EXPECT_DOUBLE_EQ(engine.getVariable("g_x")->toScalar(), 1.0);

    // Stack popped on return — the helper next to ran_caller.m is no
    // longer reachable from the base workspace.
    EXPECT_THROW(engine.eval("y = only_via_dir(3);"), std::exception);
}

// ── #4 — Sibling resolution routes through the script's VFS ───
//
// caller.m + helper.m live entirely in a non-native VFS ("temporary").
// resolveMFile_ must use the script-origin's FS for the implicit
// sibling lookup, not native disk.
TEST_P(MFileResolverTest, SiblingResolvesThroughTemporaryFS)
{
    auto vfs = std::make_unique<TestMemoryFS>("temporary");
    auto *vfsRaw = vfs.get();
    engine.registerVirtualFS(std::move(vfs));

    vfsRaw->files_["/scripts/caller.m"] = "g = vfs_helper(4);\n";
    vfsRaw->files_["/scripts/vfs_helper.m"] =
        "function y = vfs_helper(x)\n  y = x * 11;\nend\n";

    EXPECT_NO_THROW(engine.eval("run('temporary:/scripts/caller.m');"));
    auto *g = engine.getVariable("g");
    ASSERT_NE(g, nullptr);
    EXPECT_DOUBLE_EQ(g->toScalar(), 44.0);
}

// ── #5 — scriptDir and addpath coexist; both lookups work ─────
//
// caller.m sits next to near_helper.m in workDir, but ALSO calls
// far_helper which lives in a separate addpath'd dir. Both must
// resolve from the same script.
TEST_P(MFileResolverTest, ScriptDirAndAddpathCoexist)
{
    auto far = workDir / "far";
    std::filesystem::create_directories(far);

    writeMFile("coexist_caller.m",
               "g_near = near_helper(2);\n"
               "g_far  = far_helper(3);\n");
    writeMFile("near_helper.m",
               "function y = near_helper(x)\n  y = x + 100;\nend\n");
    {
        std::ofstream f(far / "far_helper.m");
        f << "function y = far_helper(x)\n  y = x + 200;\nend\n";
    }

    engine.addPath(far.string());
    auto p = (workDir / "coexist_caller.m").generic_string();
    EXPECT_NO_THROW(engine.eval("run('" + p + "');"));
    EXPECT_DOUBLE_EQ(engine.getVariable("g_near")->toScalar(), 102.0);
    EXPECT_DOUBLE_EQ(engine.getVariable("g_far")->toScalar(), 203.0);
}

// ── IDE-style flow: manual pushScriptOrigin(fs, dir) + raw eval ──
//
// The IDE Run button doesn't go through the `run()` builtin. It
// pushes the script origin (FS + dir) directly via the WASM
// binding, then calls engine.eval(buffer). This test exercises
// that exact code path: push (fs, dir) by hand, then eval source
// that calls a sibling. The engine's resolveMFile_ must still
// pick up scriptDir from currentScriptDir().
TEST_P(MFileResolverTest, ManualPushScriptOriginEnablesSiblingLookup)
{
    writeMFile("ide_helper.m",
               "function y = ide_helper(x)\n  y = x * 5;\nend\n");
    engine.pushScriptOrigin("native", workDir.string());
    try {
        engine.eval("ide_g = ide_helper(8);");
    } catch (...) {
        engine.popScriptOrigin();
        throw;
    }
    engine.popScriptOrigin();
    auto *g = engine.getVariable("ide_g");
    ASSERT_NE(g, nullptr);
    EXPECT_DOUBLE_EQ(g->toScalar(), 40.0);
}

// ── Root-level scriptDir resolves siblings at FS root ─────────
//
// run('temporary:/foo.m') — file at the FS root. The `run` builtin
// extracts scriptDir from rp.path; for "/foo.m" the directory is
// "/" (one char), NOT "" (which would silently disable sibling
// lookup). Pins that root-level files in a VFS resolve siblings.
TEST_P(MFileResolverTest, RunBuiltinRootLevelFileResolvesSibling)
{
    auto vfs = std::make_unique<TestMemoryFS>("temporary");
    auto *vfsRaw = vfs.get();
    engine.registerVirtualFS(std::move(vfs));

    vfsRaw->files_["/root_caller.m"] = "g = root_helper(6);\n";
    vfsRaw->files_["/root_helper.m"] =
        "function y = root_helper(x)\n  y = x + 100;\nend\n";

    EXPECT_NO_THROW(engine.eval("run('temporary:/root_caller.m');"));
    auto *g = engine.getVariable("g");
    ASSERT_NE(g, nullptr);
    EXPECT_DOUBLE_EQ(g->toScalar(), 106.0);
}

// ── Same thing via the IDE-style direct push of scriptDir = "/" ─
TEST_P(MFileResolverTest, ManualPushRootScriptDirResolvesSibling)
{
    auto vfs = std::make_unique<TestMemoryFS>("temporary");
    auto *vfsRaw = vfs.get();
    engine.registerVirtualFS(std::move(vfs));
    vfsRaw->files_["/h.m"] = "function y = h(x)\n  y = x + 9;\nend\n";

    engine.pushScriptOrigin("temporary", "/");
    try { engine.eval("z = h(1);"); }
    catch (...) { engine.popScriptOrigin(); throw; }
    engine.popScriptOrigin();

    EXPECT_DOUBLE_EQ(engine.getVariable("z")->toScalar(), 10.0);
}

// ── 1-arg pushScriptOrigin (legacy) leaves sibling lookup off ──
//
// The old 1-arg form (FS only, no dir) is still supported for
// callers that don't have a script path. With it, sibling lookup
// must NOT engage — empty scriptDir means no implicit search dir.
TEST_P(MFileResolverTest, LegacyPushScriptOriginNoSiblingLookup)
{
    writeMFile("legacy_helper.m",
               "function y = legacy_helper(x)\n  y = x;\nend\n");
    engine.pushScriptOrigin("native");          // 1-arg, no dir
    bool threw = false;
    try { engine.eval("z = legacy_helper(1);"); }
    catch (const std::exception &) { threw = true; }
    engine.popScriptOrigin();
    EXPECT_TRUE(threw) << "1-arg pushScriptOrigin must not implicitly add a search dir";
}

TEST_P(MFileResolverTest, PathReturnsRegisteredDirs)
{
    auto a = (workDir / "a").string();
    auto b = (workDir / "b").string();
    std::filesystem::create_directories(a);
    std::filesystem::create_directories(b);
    engine.addPath(a);
    engine.addPath(b);
    auto p = engine.path();
    ASSERT_EQ(p.size(), 2u);
    EXPECT_EQ(p[0], a);
    EXPECT_EQ(p[1], b);

    // De-dup: re-adding doesn't duplicate.
    engine.addPath(a);
    EXPECT_EQ(engine.path().size(), 2u);
}

// A .m file may define a main function followed by local (sub)functions it
// calls — a core MATLAB feature. Loading the main via the path must also
// register its locals so the internal call chain resolves.
TEST_P(MFileResolverTest, LocalSubfunctionsResolve)
{
    writeMFile("mainf.m",
               "function y = mainf(x)\n"
               "  y = helper1(x) + helper2(x);\n"
               "end\n"
               "function a = helper1(x)\n"
               "  a = x * 2;\n"
               "end\n"
               "function b = helper2(x)\n"
               "  b = x + 10;\n"
               "end\n");
    engine.addPath(workDir.string());
    engine.eval("y = mainf(5);");   // 5*2 + (5+10) = 25
    ASSERT_NE(engine.getVariable("y"), nullptr);
    EXPECT_DOUBLE_EQ(engine.getVariable("y")->toScalar(), 25.0);

    // A local must not leak over an existing builtin of the same name: a
    // file whose local is named like a builtin keeps the builtin globally.
    writeMFile("usesmax.m",
               "function y = usesmax(x)\n  y = mymaxhelper(x);\nend\n"
               "function m = mymaxhelper(x)\n  m = max(x) + 1;\nend\n");
    engine.eval("y2 = usesmax([3 7 2]);");
    EXPECT_DOUBLE_EQ(engine.getVariable("y2")->toScalar(), 8.0);
}

// A function declaring an output variable (e.g. `function x = myfun()`) but
// never assigning it, when invoked as a statement without output capture
// (`myfun();`, nargout=0), must succeed on BOTH the first run (cold m-file
// lookup) and subsequent runs.
TEST_P(MFileResolverTest, UnassignedOutputWithZeroNargoutFirstRun)
{
    writeMFile("myfun.m", "function x = myfun()\n  disp('hello');\nend\n");
    engine.addPath(workDir.string());

    // Cold call on first run
    EXPECT_NO_THROW(engine.eval("clear; myfun();"));
    // Subsequent warm call
    EXPECT_NO_THROW(engine.eval("myfun();"));
}

INSTANTIATE_TEST_SUITE_P(TW_VM, MFileResolverTest,
                          ::testing::Values(Engine::Backend::TreeWalker,
                                            Engine::Backend::VM),
                          [](const ::testing::TestParamInfo<Engine::Backend> &info) {
                              return info.param == Engine::Backend::TreeWalker ? "TW" : "VM";
                          });

} // namespace

// --- bugs/closed/lang/handle-to-file-function-unresolved.md (FIXED) ---
// A handle to a sibling FILE function resolves exactly like a direct
// call — the VM's indirect-call path falls back to lookupUserFunction
// (script-origin dirs included), and the native runScript pushes the
// script origin.
TEST_P(MFileResolverTest, HandleToSiblingFunctionResolves)
{
    writeMFile("hfn.m", "function y = hfn(x)\n  y = 6 * x;\nend\n");
    writeMFile("hcaller.m", "h = @hfn;\ng = h(7);\n");
    auto p = (workDir / "hcaller.m").string();
    engine.eval("run('" + p + "');");
    auto *g = engine.getVariable("g");
    ASSERT_NE(g, nullptr);
    EXPECT_DOUBLE_EQ(g->toScalar(), 42.0);
}
