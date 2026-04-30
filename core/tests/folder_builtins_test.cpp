// core/tests/folder_builtins_test.cpp
//
// Phase 9c — folder + path-utility builtins.
// Verifies cd / pwd / mkdir / rmdir / delete / dir / ls plus the pure
// path utilities tempdir / tempname / fullfile / fileparts / filesep /
// pathsep. All routed through the VFS (no direct std::filesystem in
// the engine path).

#include <numkit/core/engine.hpp>
#include <numkit/core/vfs.hpp>

#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>

using namespace numkit;

namespace {

class FolderBuiltinsTest : public ::testing::TestWithParam<Engine::Backend>
{
protected:
    Engine engine;
    std::filesystem::path workDir;

    void SetUp() override
    {
        const auto *info = ::testing::UnitTest::GetInstance()->current_test_info();
        workDir = std::filesystem::temp_directory_path()
                  / (std::string{"numkit-folder-test-"} + info->name());
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
};

TEST_P(FolderBuiltinsTest, PwdReflectsCwd)
{
    engine.setCwd(workDir.string());
    engine.eval("p = pwd;");
    auto *p = engine.getVariable("p");
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->toString(), workDir.string());
}

TEST_P(FolderBuiltinsTest, CdChangesCwd)
{
    engine.eval("cd('" + workDir.string() + "');");
    EXPECT_EQ(engine.cwd(), workDir.string());

    engine.eval("p = pwd;");
    EXPECT_EQ(engine.getVariable("p")->toString(), workDir.string());
}

TEST_P(FolderBuiltinsTest, CdReturnsPreviousWhenAssigned)
{
    auto subA = (workDir / "a").string();
    auto subB = (workDir / "b").string();
    std::filesystem::create_directories(subA);
    std::filesystem::create_directories(subB);
    engine.setCwd(subA);

    engine.eval("prev = cd('" + subB + "');");
    auto *prev = engine.getVariable("prev");
    ASSERT_NE(prev, nullptr);
    EXPECT_EQ(prev->toString(), subA);
    EXPECT_EQ(engine.cwd(), subB);
}

TEST_P(FolderBuiltinsTest, MkdirCreatesDirectory)
{
    auto target = (workDir / "fresh").string();
    engine.eval("mkdir('" + target + "');");
    EXPECT_TRUE(std::filesystem::is_directory(target));
}

TEST_P(FolderBuiltinsTest, MkdirParentNameForm)
{
    engine.eval("mkdir('" + workDir.string() + "', 'sub');");
    EXPECT_TRUE(std::filesystem::is_directory(workDir / "sub"));
}

TEST_P(FolderBuiltinsTest, RmdirRemovesEmptyDirectory)
{
    auto target = workDir / "doomed";
    std::filesystem::create_directories(target);
    engine.eval("rmdir('" + target.string() + "');");
    EXPECT_FALSE(std::filesystem::exists(target));
}

TEST_P(FolderBuiltinsTest, DeleteRemovesFile)
{
    auto file = workDir / "trash.txt";
    {
        std::ofstream o(file);
        o << "junk";
    }
    ASSERT_TRUE(std::filesystem::exists(file));
    engine.eval("delete('" + file.string() + "');");
    EXPECT_FALSE(std::filesystem::exists(file));
}

TEST_P(FolderBuiltinsTest, DirReturnsCellOfStructs)
{
    {
        std::ofstream(workDir / "alpha.txt") << "a";
        std::ofstream(workDir / "beta.txt") << "bb";
    }
    engine.eval("d = dir('" + workDir.string() + "');");
    auto *d = engine.getVariable("d");
    ASSERT_NE(d, nullptr);
    // Should be a non-empty cell (we know there are 2 entries).
    // The cellAt(i) returns each struct.
    EXPECT_GT(d->numel(), 0u);
}

TEST_P(FolderBuiltinsTest, LsListsEntries)
{
    {
        std::ofstream(workDir / "x.txt") << "x";
    }
    engine.eval("s = ls('" + workDir.string() + "');");
    auto *s = engine.getVariable("s");
    ASSERT_NE(s, nullptr);
    EXPECT_NE(s->toString().find("x.txt"), std::string::npos);
}

TEST_P(FolderBuiltinsTest, FilesepPathsep)
{
    engine.eval("a = filesep; b = pathsep;");
    auto *a = engine.getVariable("a");
    auto *b = engine.getVariable("b");
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);
#ifdef _WIN32
    EXPECT_EQ(a->toString(), "\\");
    EXPECT_EQ(b->toString(), ";");
#else
    EXPECT_EQ(a->toString(), "/");
    EXPECT_EQ(b->toString(), ":");
#endif
}

TEST_P(FolderBuiltinsTest, FullfileJoinsParts)
{
    engine.eval("p = fullfile('one', 'two', 'three.txt');");
    auto *p = engine.getVariable("p");
    ASSERT_NE(p, nullptr);
    auto s = p->toString();
    EXPECT_NE(s.find("one"), std::string::npos);
    EXPECT_NE(s.find("two"), std::string::npos);
    EXPECT_NE(s.find("three.txt"), std::string::npos);
}

TEST_P(FolderBuiltinsTest, FilepartsSplitsThreeWays)
{
    engine.eval("[d, n, e] = fileparts('/abs/path/to/file.txt');");
    auto *d = engine.getVariable("d");
    auto *n = engine.getVariable("n");
    auto *e = engine.getVariable("e");
    ASSERT_NE(d, nullptr);
    ASSERT_NE(n, nullptr);
    ASSERT_NE(e, nullptr);
    EXPECT_EQ(d->toString(), "/abs/path/to");
    EXPECT_EQ(n->toString(), "file");
    EXPECT_EQ(e->toString(), ".txt");
}

TEST_P(FolderBuiltinsTest, FilepartsHandlesNoExt)
{
    engine.eval("[d, n, e] = fileparts('readme');");
    EXPECT_EQ(engine.getVariable("d")->toString(), "");
    EXPECT_EQ(engine.getVariable("n")->toString(), "readme");
    EXPECT_EQ(engine.getVariable("e")->toString(), "");
}

TEST_P(FolderBuiltinsTest, TempdirReturnsNonEmpty)
{
    engine.eval("t = tempdir;");
    auto *t = engine.getVariable("t");
    ASSERT_NE(t, nullptr);
    EXPECT_FALSE(t->toString().empty());
}

TEST_P(FolderBuiltinsTest, TempnameIsUnique)
{
    engine.eval("a = tempname; b = tempname;");
    auto *a = engine.getVariable("a");
    auto *b = engine.getVariable("b");
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);
    EXPECT_NE(a->toString(), b->toString());
}

INSTANTIATE_TEST_SUITE_P(TW_VM, FolderBuiltinsTest,
                          ::testing::Values(Engine::Backend::TreeWalker,
                                            Engine::Backend::VM),
                          [](const ::testing::TestParamInfo<Engine::Backend> &info) {
                              return info.param == Engine::Backend::TreeWalker ? "TW" : "VM";
                          });

} // namespace
