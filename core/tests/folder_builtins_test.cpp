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

TEST_P(FolderBuiltinsTest, PwdFallsBackToBackendCwd)
{
    // When the engine has no explicit cwd, pwd surfaces the active
    // backend's notion of the current directory (NativeFS uses
    // std::filesystem::current_path).
    engine.setCwd("");
    engine.eval("p = pwd;");
    auto *p = engine.getVariable("p");
    ASSERT_NE(p, nullptr);
    EXPECT_FALSE(p->toString().empty());
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

TEST_P(FolderBuiltinsTest, DirReturnsStructArray)
{
    {
        std::ofstream(workDir / "alpha.txt") << "a";
        std::ofstream(workDir / "beta.txt") << "bb";
    }
    engine.eval("d = dir('" + workDir.string() + "');");
    auto *d = engine.getVariable("d");
    ASSERT_NE(d, nullptr);
    EXPECT_TRUE(d->isStructArray());
    EXPECT_EQ(d->numel(), 2u);

    // d(1).name — paren-indexed struct-array element + field access.
    engine.eval("nm1 = d(1).name; nm2 = d(2).name;");
    auto *nm1 = engine.getVariable("nm1");
    auto *nm2 = engine.getVariable("nm2");
    ASSERT_NE(nm1, nullptr);
    ASSERT_NE(nm2, nullptr);
    std::string s1 = nm1->toString();
    std::string s2 = nm2->toString();
    // Order from listDir is platform-defined; just check both present.
    EXPECT_TRUE((s1 == "alpha.txt" && s2 == "beta.txt")
             || (s1 == "beta.txt"  && s2 == "alpha.txt"));

    // numel(d) and fieldnames(d) work on the array.
    engine.eval("nf = numel(fieldnames(d));");
    auto *nf = engine.getVariable("nf");
    ASSERT_NE(nf, nullptr);
    EXPECT_GE(nf->toScalar(), 5.0);  // name/folder/isdir/bytes/datenum/date
}

TEST_P(FolderBuiltinsTest, DirEachElementHasFields)
{
    {
        std::ofstream(workDir / "f1.txt") << "x";
        std::ofstream(workDir / "f2.txt") << "yy";
        std::ofstream(workDir / "f3.txt") << "zzz";
    }
    engine.eval("d = dir('" + workDir.string() + "');");
    engine.eval("b1 = d(1).bytes; b2 = d(2).bytes; b3 = d(3).bytes;");
    auto *b1 = engine.getVariable("b1");
    auto *b2 = engine.getVariable("b2");
    auto *b3 = engine.getVariable("b3");
    ASSERT_NE(b1, nullptr);
    ASSERT_NE(b2, nullptr);
    ASSERT_NE(b3, nullptr);
    // Sum of bytes across f1+f2+f3 = 1+2+3 = 6.
    EXPECT_DOUBLE_EQ(b1->toScalar() + b2->toScalar() + b3->toScalar(), 6.0);
}

TEST_P(FolderBuiltinsTest, DirDottedAccessOnArrayThrows)
{
    {
        std::ofstream(workDir / "x.txt") << "x";
        std::ofstream(workDir / "y.txt") << "y";
    }
    engine.eval("d = dir('" + workDir.string() + "');");
    // Direct .name on a non-scalar struct array should error with a
    // helpful message — caller must use d(i).name.
    EXPECT_THROW(engine.eval("nm = d.name;"), std::exception);
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
