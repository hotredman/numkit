// core/tests/vfs_extension_test.cpp
//
// Phase 8 VFS extension: listDir / stat / mkdir / rmdir / unlink / tempArea.
// Tests use NativeFS against a temp directory.

#include <numkit/core/vfs.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>

using namespace numkit;

namespace {

class NativeFsExtensionTest : public ::testing::Test
{
protected:
    NativeFS fs;
    std::filesystem::path workDir;

    void SetUp() override
    {
        // Each test gets its own temp subdir to avoid cross-pollination.
        const auto *info = ::testing::UnitTest::GetInstance()->current_test_info();
        workDir = std::filesystem::temp_directory_path()
                  / (std::string{"numkit-vfs-test-"} + info->name());
        std::error_code ec;
        std::filesystem::remove_all(workDir, ec);
        std::filesystem::create_directories(workDir);
    }

    void TearDown() override
    {
        std::error_code ec;
        std::filesystem::remove_all(workDir, ec);
    }

    std::string p(const std::string &name) const
    {
        return (workDir / name).string();
    }
};

TEST_F(NativeFsExtensionTest, MkdirCreatesDirectory)
{
    auto sub = p("sub");
    fs.mkdir(sub);
    EXPECT_TRUE(std::filesystem::is_directory(sub));
}

TEST_F(NativeFsExtensionTest, ListDirReportsEntries)
{
    fs.writeFile(p("a.txt"), "alpha");
    fs.writeFile(p("b.txt"), "beta");
    fs.mkdir(p("subdir"));

    auto entries = fs.listDir(workDir.string());
    ASSERT_EQ(entries.size(), 3u);

    std::sort(entries.begin(), entries.end(),
              [](const DirEntry &a, const DirEntry &b) { return a.name < b.name; });
    EXPECT_EQ(entries[0].name, "a.txt");
    EXPECT_FALSE(entries[0].isDirectory);
    EXPECT_EQ(entries[1].name, "b.txt");
    EXPECT_FALSE(entries[1].isDirectory);
    EXPECT_EQ(entries[2].name, "subdir");
    EXPECT_TRUE(entries[2].isDirectory);
}

TEST_F(NativeFsExtensionTest, StatReportsFileMetadata)
{
    fs.writeFile(p("data.txt"), "hello world");
    auto st = fs.stat(p("data.txt"));
    ASSERT_TRUE(st.has_value());
    EXPECT_EQ(st->kind, FileStat::Kind::File);
    EXPECT_EQ(st->size, 11);
    EXPECT_GT(st->mtime, 0);   // some non-zero mtime
}

TEST_F(NativeFsExtensionTest, StatReportsDirectoryKind)
{
    fs.mkdir(p("subdir"));
    auto st = fs.stat(p("subdir"));
    ASSERT_TRUE(st.has_value());
    EXPECT_EQ(st->kind, FileStat::Kind::Directory);
}

TEST_F(NativeFsExtensionTest, StatReturnsNulloptForMissing)
{
    auto st = fs.stat(p("nonexistent.txt"));
    EXPECT_FALSE(st.has_value());
}

TEST_F(NativeFsExtensionTest, UnlinkRemovesFile)
{
    fs.writeFile(p("doomed.txt"), "x");
    EXPECT_TRUE(fs.exists(p("doomed.txt")));
    fs.unlink(p("doomed.txt"));
    EXPECT_FALSE(fs.exists(p("doomed.txt")));
}

TEST_F(NativeFsExtensionTest, RmdirRemovesEmptyDirectory)
{
    fs.mkdir(p("temp"));
    EXPECT_TRUE(fs.exists(p("temp")));
    fs.rmdir(p("temp"));
    EXPECT_FALSE(fs.exists(p("temp")));
}

TEST_F(NativeFsExtensionTest, TempAreaReturnsNonEmpty)
{
    auto t = fs.tempArea();
    EXPECT_FALSE(t.empty());
    // Should be a directory that exists.
    EXPECT_TRUE(std::filesystem::is_directory(t));
}

// ── CallbackFS — verify hooks plumb through ──────────────────────────

TEST(CallbackFsExtensionTest, ListDirHookInvokes)
{
    CallbackFS cb("test", nullptr, nullptr, nullptr);
    bool called = false;
    cb.setListDir([&called](const std::string &p) -> std::vector<DirEntry> {
        called = true;
        EXPECT_EQ(p, "/foo");
        return {{"a.m", false}, {"+util", true}};
    });
    auto entries = cb.listDir("/foo");
    EXPECT_TRUE(called);
    ASSERT_EQ(entries.size(), 2u);
    EXPECT_EQ(entries[0].name, "a.m");
    EXPECT_EQ(entries[1].name, "+util");
    EXPECT_TRUE(entries[1].isDirectory);
}

TEST(CallbackFsExtensionTest, StatHookInvokes)
{
    CallbackFS cb("test", nullptr, nullptr, nullptr);
    cb.setStat([](const std::string &p) -> std::optional<FileStat> {
        if (p == "/exists.txt") {
            FileStat fs;
            fs.size = 42;
            fs.mtime = 1000;
            fs.kind = FileStat::Kind::File;
            return fs;
        }
        return std::nullopt;
    });
    auto a = cb.stat("/exists.txt");
    ASSERT_TRUE(a.has_value());
    EXPECT_EQ(a->size, 42);
    EXPECT_EQ(a->mtime, 1000);
    EXPECT_EQ(cb.stat("/missing.txt").has_value(), false);
}

TEST(CallbackFsExtensionTest, UnsetHooksDefaultBehavior)
{
    CallbackFS cb("test", nullptr, nullptr, nullptr);
    EXPECT_THROW(cb.listDir("/anything"), std::runtime_error);
    EXPECT_FALSE(cb.stat("/anything").has_value());
    EXPECT_THROW(cb.mkdir("/anything"), std::runtime_error);
    EXPECT_THROW(cb.rmdir("/anything"), std::runtime_error);
    EXPECT_THROW(cb.unlink("/anything"), std::runtime_error);
    EXPECT_TRUE(cb.tempArea().empty());
}

} // namespace
