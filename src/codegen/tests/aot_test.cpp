// codegen/tests/aot_test.cpp
//
// Brick 4: the AOT compile harness. When an external compiler is
// configured (it is on this MSVC build), a self-contained C++ source
// string compiles to a runnable executable; a broken source reports a
// CompileError with a log. When no compiler is configured the harness
// returns Unavailable and these tests skip (the green baseline is never
// broken by a missing toolchain).

#include <numkit/codegen/aot.hpp>

#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>

namespace {

std::filesystem::path tmpBase()
{
    auto p = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(p);
    return p;
}

}  // namespace

TEST(Aot, TrivialCompileAndRun)
{
    if (!numkit::codegen::aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const std::string exe = (tmpBase() / "nk_trivial.exe").string();
    const char       *src =
        "#include <cstdio>\n"
        "int main() { std::printf(\"ok\\n\"); return 0; }\n";

    const auto r = numkit::codegen::aot::compileToExecutable(src, exe);
    ASSERT_EQ(r.status, numkit::codegen::aot::CompileStatus::Ok)
        << "log:\n" << r.log << "\ncmd: " << r.command;
    ASSERT_TRUE(std::filesystem::exists(exe));
    EXPECT_EQ(std::system(("\"" + exe + "\"").c_str()), 0);
}

TEST(Aot, CompileErrorReported)
{
    if (!numkit::codegen::aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const std::string exe = (tmpBase() / "nk_broken.exe").string();
    const auto        r =
        numkit::codegen::aot::compileToExecutable("@ this is not valid c++ @\n", exe);

    EXPECT_EQ(r.status, numkit::codegen::aot::CompileStatus::CompileError);
    EXPECT_FALSE(r.log.empty());
}
