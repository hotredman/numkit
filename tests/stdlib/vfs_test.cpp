// tests/stdlib/vfs_test.cpp — VirtualFS routing, setenv/getenv, script origin

#include "MVfs.hpp"
#include "dual_engine_fixture.hpp"

#include <map>
#include <memory>

using namespace mlab_test;

namespace {

// In-memory FS for tests — mimics what the IDE's tempFS/localFS would expose
// through CallbackFS in a real WASM build.
class MemoryFS final : public mlab::VirtualFS
{
public:
    explicit MemoryFS(std::string n) : name_(std::move(n)) {}

    std::string readFile(const std::string &path) override
    {
        auto it = files_.find(path);
        if (it == files_.end())
            throw std::runtime_error(name_ + ": no such file '" + path + "'");
        return it->second;
    }
    void writeFile(const std::string &path, const std::string &content) override
    {
        files_[path] = content;
    }
    bool exists(const std::string &path) override { return files_.count(path) > 0; }
    std::string name() const override { return name_; }

    std::map<std::string, std::string> &files() { return files_; }

private:
    std::string name_;
    std::map<std::string, std::string> files_;
};

} // namespace

class VfsTest : public DualEngineTest
{
public:
    MemoryFS *tempFs = nullptr;
    MemoryFS *localFs = nullptr;

    void SetUp() override
    {
        DualEngineTest::SetUp();
        auto temp = std::make_unique<MemoryFS>("temporary");
        auto local = std::make_unique<MemoryFS>("local");
        tempFs = temp.get();
        localFs = local.get();
        engine.registerVirtualFS(std::move(temp));
        engine.registerVirtualFS(std::move(local));
    }

    void TearDown() override
    {
        // Clear any env vars this test might have set so they don't leak.
        eval("setenv('MLAB_FS','');");
        eval("setenv('MLAB_CWD','');");
        DualEngineTest::TearDown();
    }
};

// ── setenv / getenv ─────────────────────────────────────────

TEST_P(VfsTest, SetenvGetenvRoundTrip)
{
    eval("setenv('MLAB_TEST_VAR', 'hello');");
    EXPECT_EQ(evalString("v = getenv('MLAB_TEST_VAR');"), "hello");
}

TEST_P(VfsTest, GetenvReturnsEmptyWhenUnset)
{
    EXPECT_EQ(evalString("v = getenv('MLAB_TOTALLY_UNLIKELY_NAME_XYZ');"), "");
}

TEST_P(VfsTest, SetenvWithSingleArgClearsValue)
{
    eval("setenv('MLAB_TEST_VAR_2', 'stuff');");
    eval("setenv('MLAB_TEST_VAR_2');");
    EXPECT_EQ(evalString("v = getenv('MLAB_TEST_VAR_2');"), "");
}

// ── MLAB_FS routing ────────────────────────────────────────

TEST_P(VfsTest, MlabFsRoutesCsvwriteToTemporary)
{
    eval("setenv('MLAB_FS', 'temporary');");
    eval("csvwrite('data.csv', [1 2; 3 4]);");

    ASSERT_EQ(tempFs->files().size(), 1u);
    EXPECT_EQ(tempFs->files().begin()->first, "data.csv");
    EXPECT_EQ(tempFs->files().begin()->second, "1,2\n3,4\n");
    EXPECT_TRUE(localFs->files().empty());
}

TEST_P(VfsTest, MlabFsRoutesCsvreadFromLocal)
{
    localFs->files()["m.csv"] = "7,8\n9,10\n";
    eval("setenv('MLAB_FS', 'local');");
    eval("M = csvread('m.csv');");

    auto *M = getVarPtr("M");
    ASSERT_NE(M, nullptr);
    EXPECT_EQ(rows(*M), 2u);
    EXPECT_EQ(cols(*M), 2u);
    expectElem2D(*M, 0, 0, 7.0);
    expectElem2D(*M, 1, 1, 10.0);
}

// ── Explicit path prefix wins over env var ──────────────────

TEST_P(VfsTest, ExplicitPrefixOverridesEnv)
{
    tempFs->files()["a.csv"] = "1\n";
    localFs->files()["a.csv"] = "999\n";

    eval("setenv('MLAB_FS', 'temporary');");
    // Explicit "local:" prefix: should hit localFs even though env says temporary.
    eval("M = csvread('local:a.csv');");

    auto *M = getVarPtr("M");
    ASSERT_NE(M, nullptr);
    expectElem2D(*M, 0, 0, 999.0);
}

// ── MLAB_CWD prefixes relative paths ───────────────────────

TEST_P(VfsTest, MlabCwdJoinsRelativePaths)
{
    tempFs->files()["/data/a.csv"] = "42\n";
    eval("setenv('MLAB_FS', 'temporary');");
    eval("setenv('MLAB_CWD', '/data');");
    eval("M = csvread('a.csv');");

    auto *M = getVarPtr("M");
    ASSERT_NE(M, nullptr);
    expectElem2D(*M, 0, 0, 42.0);
}

TEST_P(VfsTest, AbsolutePathIgnoresMlabCwd)
{
    tempFs->files()["/absolute/x.csv"] = "7\n";
    tempFs->files()["/data/absolute/x.csv"] = "999\n";

    eval("setenv('MLAB_FS', 'temporary');");
    eval("setenv('MLAB_CWD', '/data');");
    eval("M = csvread('/absolute/x.csv');");

    auto *M = getVarPtr("M");
    ASSERT_NE(M, nullptr);
    expectElem2D(*M, 0, 0, 7.0);
}

// ── Script origin fallback ─────────────────────────────────

TEST_P(VfsTest, ScriptOriginUsedWhenEnvUnset)
{
    tempFs->files()["origin.csv"] = "55\n";
    engine.pushScriptOrigin("temporary");
    eval("M = csvread('origin.csv');");
    engine.popScriptOrigin();

    auto *M = getVarPtr("M");
    ASSERT_NE(M, nullptr);
    expectElem2D(*M, 0, 0, 55.0);
}

TEST_P(VfsTest, EnvOverridesScriptOrigin)
{
    tempFs->files()["x.csv"] = "1\n";
    localFs->files()["x.csv"] = "2\n";

    engine.pushScriptOrigin("temporary");
    eval("setenv('MLAB_FS', 'local');");
    eval("M = csvread('x.csv');");
    engine.popScriptOrigin();

    auto *M = getVarPtr("M");
    ASSERT_NE(M, nullptr);
    expectElem2D(*M, 0, 0, 2.0);
}

// ── Unknown filesystem error ───────────────────────────────

TEST_P(VfsTest, UnknownFsNameThrows)
{
    eval("setenv('MLAB_FS', 'no_such_fs');");
    EXPECT_THROW(eval("M = csvread('x.csv');"), std::exception);
}

INSTANTIATE_DUAL(VfsTest);
