// tests/stdlib/fileio_test.cpp — fopen / fclose / fprintf-to-file parity
// Parameterized: runs on both TreeWalker and VM backends.
//
// The round-trip target is always a CallbackFS-backed in-memory VirtualFS
// so the tests don't touch disk and are independent of the native FS.

#include "MLabVfs.hpp"
#include "dual_engine_fixture.hpp"

#include <map>
#include <memory>

using namespace mlab_test;

namespace {

// Minimal in-memory VFS used as a test sink for fprintf-to-file writes.
// Mirrors the pattern from vfs_test.cpp.
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

class FileIoTest : public DualEngineTest
{
public:
    MemoryFS *fs = nullptr;

    void SetUp() override
    {
        DualEngineTest::SetUp();
        auto mem = std::make_unique<MemoryFS>("temporary");
        fs = mem.get();
        engine.registerVirtualFS(std::move(mem));
        engine.pushScriptOrigin("temporary");
    }

    void TearDown() override
    {
        engine.popScriptOrigin();
        DualEngineTest::TearDown();
    }
};

// ── fopen / fprintf / fclose round-trip ────────────────────

TEST_P(FileIoTest, FopenWriteFprintfCloseRoundTrip)
{
    eval("fid = fopen('out.txt', 'w');");
    EXPECT_GE(getVar("fid"), 3.0);
    eval("fprintf(fid, 'hello %d\\n', 42);");
    EXPECT_EQ(evalScalar("s = fclose(fid);"), 0.0);

    ASSERT_EQ(fs->files().count("out.txt"), 1u);
    EXPECT_EQ(fs->files()["out.txt"], "hello 42\n");
}

TEST_P(FileIoTest, MultipleFprintfCallsAccumulate)
{
    eval("fid = fopen('log.txt', 'w');");
    eval("fprintf(fid, 'line1\\n');");
    eval("fprintf(fid, 'line2\\n');");
    eval("fprintf(fid, 'line3\\n');");
    eval("fclose(fid);");

    EXPECT_EQ(fs->files()["log.txt"], "line1\nline2\nline3\n");
}

// ── fopen mode semantics ──────────────────────────────────

TEST_P(FileIoTest, FopenWriteTruncatesExistingContent)
{
    fs->files()["target.txt"] = "pre-existing content";
    eval("fid = fopen('target.txt', 'w');");
    eval("fprintf(fid, 'fresh');");
    eval("fclose(fid);");

    EXPECT_EQ(fs->files()["target.txt"], "fresh");
}

TEST_P(FileIoTest, FopenAppendPreservesExistingContent)
{
    fs->files()["log.txt"] = "old data\n";
    eval("fid = fopen('log.txt', 'a');");
    eval("fprintf(fid, 'new data\\n');");
    eval("fclose(fid);");

    EXPECT_EQ(fs->files()["log.txt"], "old data\nnew data\n");
}

TEST_P(FileIoTest, FopenAppendOnMissingFileCreatesIt)
{
    // 'a' on a missing file should behave like 'w' (MATLAB parity).
    eval("fid = fopen('new.txt', 'a');");
    eval("fprintf(fid, 'hi');");
    eval("fclose(fid);");

    EXPECT_EQ(fs->files()["new.txt"], "hi");
}

TEST_P(FileIoTest, FopenAcceptsBinaryAndTextSuffixes)
{
    // MATLAB accepts 'wt', 'wb', 'rt', 'rb' — we treat them as aliases
    // for the base mode (no CRLF translation either way).
    eval("fid = fopen('a.txt', 'wt');");
    EXPECT_GE(getVar("fid"), 3.0);
    eval("fclose(fid);");

    eval("fid2 = fopen('a.txt', 'rb');");
    EXPECT_GE(getVar("fid2"), 3.0);
    eval("fclose(fid2);");
}

// ── fopen failure paths ───────────────────────────────────

TEST_P(FileIoTest, FopenReadOnMissingFileReturnsMinusOne)
{
    // MATLAB contract: fopen returns -1, it does NOT throw.
    EXPECT_EQ(evalScalar("fid = fopen('nowhere.txt', 'r');"), -1.0);
}

TEST_P(FileIoTest, FopenWithInvalidModeReturnsMinusOne)
{
    EXPECT_EQ(evalScalar("fid = fopen('x.txt', 'zzz');"), -1.0);
}

// ── fclose variants ───────────────────────────────────────

TEST_P(FileIoTest, FcloseOnUnknownFidReturnsMinusOne)
{
    EXPECT_EQ(evalScalar("s = fclose(999);"), -1.0);
}

TEST_P(FileIoTest, FcloseAllClosesEveryUserFid)
{
    eval("a = fopen('a.txt', 'w');");
    eval("b = fopen('b.txt', 'w');");
    eval("c = fopen('c.txt', 'w');");
    eval("fprintf(a, 'A');");
    eval("fprintf(b, 'B');");
    eval("fprintf(c, 'C');");
    EXPECT_EQ(evalScalar("s = fclose('all');"), 0.0);

    EXPECT_EQ(fs->files()["a.txt"], "A");
    EXPECT_EQ(fs->files()["b.txt"], "B");
    EXPECT_EQ(fs->files()["c.txt"], "C");

    // After fclose('all'), the fids are no longer valid.
    EXPECT_EQ(evalScalar("s2 = fclose(a);"), -1.0);
}

// ── fprintf routing ───────────────────────────────────────

TEST_P(FileIoTest, FprintfToStdoutGoesToOutput)
{
    capturedOutput.clear();
    eval("fprintf(1, 'stdout-text');");
    EXPECT_NE(capturedOutput.find("stdout-text"), std::string::npos);
}

TEST_P(FileIoTest, FprintfToStderrAlsoRoutesToOutput)
{
    // MATLAB routes stderr to the Command Window too; we follow the same
    // convention rather than splitting into a separate sink.
    capturedOutput.clear();
    eval("fprintf(2, 'stderr-text');");
    EXPECT_NE(capturedOutput.find("stderr-text"), std::string::npos);
}

TEST_P(FileIoTest, FprintfWithoutFidDefaultsToStdout)
{
    capturedOutput.clear();
    eval("fprintf('bare-fprintf\\n');");
    EXPECT_NE(capturedOutput.find("bare-fprintf"), std::string::npos);
}

TEST_P(FileIoTest, FprintfToInvalidFidThrows)
{
    EXPECT_THROW(eval("fprintf(4242, 'oops');"), std::exception);
}

TEST_P(FileIoTest, FprintfToFidZeroThrows)
{
    // 0 is reserved for stdin; writing is invalid.
    EXPECT_THROW(eval("fprintf(0, 'nope');"), std::exception);
}

// ── Format-string plumbing survives the fid detour ───────

TEST_P(FileIoTest, FprintfFormattingWorksWithFid)
{
    eval("fid = fopen('fmt.txt', 'w');");
    eval("fprintf(fid, '%d + %d = %d\\n', 2, 3, 5);");
    eval("fprintf(fid, '%.2f\\n', 3.14159);");
    eval("fprintf(fid, '%s\\n', 'str');");
    eval("fclose(fid);");

    EXPECT_EQ(fs->files()["fmt.txt"], "2 + 3 = 5\n3.14\nstr\n");
}

// ── Fid uniqueness + multiple open files ─────────────────

TEST_P(FileIoTest, EveryFopenYieldsDistinctFid)
{
    eval("a = fopen('1.txt', 'w');");
    eval("b = fopen('2.txt', 'w');");
    eval("c = fopen('3.txt', 'w');");
    EXPECT_NE(getVar("a"), getVar("b"));
    EXPECT_NE(getVar("b"), getVar("c"));
    EXPECT_NE(getVar("a"), getVar("c"));
    eval("fclose('all');");
}

// ── VFS routing via the same resolvePath as csvread/csvwrite ─

TEST_P(FileIoTest, FopenHonorsMlabFsEnvVar)
{
    // Register a second backend and redirect fopen to it via MLAB_FS —
    // the script origin stack still says 'temporary', so this verifies
    // env-var precedence for fopen specifically (same contract we
    // already test for csvread/csvwrite in vfs_test.cpp).
    auto other = std::make_unique<MemoryFS>("other");
    auto *otherPtr = other.get();
    engine.registerVirtualFS(std::move(other));

    eval("setenv('MLAB_FS', 'other');");
    eval("fid = fopen('x.txt', 'w');");
    eval("fprintf(fid, 'via other');");
    eval("fclose(fid);");
    eval("setenv('MLAB_FS', '');");

    EXPECT_EQ(otherPtr->files()["x.txt"], "via other");
    EXPECT_TRUE(fs->files().empty());
}

TEST_P(FileIoTest, FopenHonorsExplicitPathPrefix)
{
    // Explicit scheme in the path wins over every other routing input.
    auto other = std::make_unique<MemoryFS>("other");
    auto *otherPtr = other.get();
    engine.registerVirtualFS(std::move(other));

    eval("fid = fopen('other:/y.txt', 'w');");
    eval("fprintf(fid, 'prefix-wins');");
    eval("fclose(fid);");

    EXPECT_EQ(otherPtr->files()["/y.txt"], "prefix-wins");
    EXPECT_TRUE(fs->files().empty());
}

TEST_P(FileIoTest, FopenTreatsUnregisteredPrefixAsLiteralPath)
{
    // A ':' in the filename that DOESN'T match a registered FS name is
    // treated as part of a literal path (so URLs/mailto:/etc. pass
    // through untouched). The file lands in the default FS for this run.
    eval("fid = fopen('mailto:x@y.z', 'w');");
    eval("fprintf(fid, 'pass-through');");
    eval("fclose(fid);");

    // The path got routed to the script-origin FS (our MemoryFS).
    EXPECT_EQ(fs->files()["mailto:x@y.z"], "pass-through");
}

// ── fgetl / fgets / feof ─────────────────────────────────

TEST_P(FileIoTest, FgetlReadsLinesStrippingNewline)
{
    fs->files()["in.txt"] = "first\nsecond\nthird\n";
    eval("fid = fopen('in.txt', 'r');");
    EXPECT_EQ(evalString("a = fgetl(fid);"), "first");
    EXPECT_EQ(evalString("b = fgetl(fid);"), "second");
    EXPECT_EQ(evalString("c = fgetl(fid);"), "third");
    eval("fclose(fid);");
}

TEST_P(FileIoTest, FgetlReturnsMinusOneAtEof)
{
    fs->files()["one.txt"] = "only line\n";
    eval("fid = fopen('one.txt', 'r');");
    eval("a = fgetl(fid);");
    EXPECT_EQ(evalScalar("b = fgetl(fid);"), -1.0);
    eval("fclose(fid);");
}

TEST_P(FileIoTest, FgetlHandlesLineWithoutTrailingNewline)
{
    fs->files()["no-nl.txt"] = "line1\nline2";   // no final \n
    eval("fid = fopen('no-nl.txt', 'r');");
    EXPECT_EQ(evalString("a = fgetl(fid);"), "line1");
    EXPECT_EQ(evalString("b = fgetl(fid);"), "line2");
    EXPECT_EQ(evalScalar("c = fgetl(fid);"), -1.0);
    eval("fclose(fid);");
}

TEST_P(FileIoTest, FgetlStripsCarriageReturnOnCrLf)
{
    fs->files()["crlf.txt"] = "alpha\r\nbeta\r\n";
    eval("fid = fopen('crlf.txt', 'r');");
    EXPECT_EQ(evalString("a = fgetl(fid);"), "alpha");
    EXPECT_EQ(evalString("b = fgetl(fid);"), "beta");
    eval("fclose(fid);");
}

TEST_P(FileIoTest, FgetsKeepsTheNewline)
{
    fs->files()["in.txt"] = "hello\nworld\n";
    eval("fid = fopen('in.txt', 'r');");
    EXPECT_EQ(evalString("a = fgets(fid);"), "hello\n");
    EXPECT_EQ(evalString("b = fgets(fid);"), "world\n");
    EXPECT_EQ(evalScalar("c = fgets(fid);"), -1.0);
    eval("fclose(fid);");
}

TEST_P(FileIoTest, FgetsWithNcharLimitsLength)
{
    fs->files()["long.txt"] = "abcdefghij\n";
    eval("fid = fopen('long.txt', 'r');");
    // nchar=4 → first 4 chars, no newline reached.
    EXPECT_EQ(evalString("a = fgets(fid, 4);"), "abcd");
    // Next call picks up from where we stopped; nchar=3 → "efg".
    EXPECT_EQ(evalString("b = fgets(fid, 3);"), "efg");
    // Remaining up to newline (including it): "hij\n" is 4 chars, asked
    // nchar=10, we return through the newline.
    EXPECT_EQ(evalString("c = fgets(fid, 10);"), "hij\n");
    eval("fclose(fid);");
}

TEST_P(FileIoTest, FgetsNcharRespectsNewlineWhenCloser)
{
    fs->files()["short.txt"] = "hi\nworld\n";
    eval("fid = fopen('short.txt', 'r');");
    // nchar=10 but newline is at position 2 — fgets returns "hi\n".
    EXPECT_EQ(evalString("a = fgets(fid, 10);"), "hi\n");
    eval("fclose(fid);");
}

TEST_P(FileIoTest, FeofIsFalseBeforeEndTrueAfter)
{
    fs->files()["x.txt"] = "abc\n";
    eval("fid = fopen('x.txt', 'r');");
    EXPECT_FALSE(evalBool("before = feof(fid);"));
    eval("line = fgetl(fid);");
    EXPECT_TRUE(evalBool("after = feof(fid);"));
    eval("fclose(fid);");
}

TEST_P(FileIoTest, FgetlOnWriteFidThrows)
{
    eval("fid = fopen('out.txt', 'w');");
    EXPECT_THROW(eval("x = fgetl(fid);"), std::exception);
    eval("fclose(fid);");
}

TEST_P(FileIoTest, FgetsOnWriteFidThrows)
{
    eval("fid = fopen('out.txt', 'w');");
    EXPECT_THROW(eval("x = fgets(fid);"), std::exception);
    eval("fclose(fid);");
}

TEST_P(FileIoTest, FgetlOnInvalidFidThrows)
{
    EXPECT_THROW(eval("x = fgetl(999);"), std::exception);
}

TEST_P(FileIoTest, FeofOnInvalidFidThrows)
{
    EXPECT_THROW(eval("x = feof(999);"), std::exception);
}

// ── ftell / fseek / frewind ──────────────────────────────

TEST_P(FileIoTest, FtellStartsAtZeroAfterOpen)
{
    fs->files()["x.txt"] = "hello world";
    eval("fid = fopen('x.txt', 'r');");
    EXPECT_EQ(evalScalar("p = ftell(fid);"), 0.0);
    eval("fclose(fid);");
}

TEST_P(FileIoTest, FtellAdvancesAfterReads)
{
    fs->files()["x.txt"] = "line1\nline2\n";
    eval("fid = fopen('x.txt', 'r');");
    eval("fgetl(fid);");
    EXPECT_EQ(evalScalar("p = ftell(fid);"), 6.0);  // past "line1\n"
    eval("fclose(fid);");
}

TEST_P(FileIoTest, FtellOnWriteFidReturnsEndOfBuffer)
{
    eval("fid = fopen('out.txt', 'w');");
    eval("fprintf(fid, 'abc');");
    EXPECT_EQ(evalScalar("p = ftell(fid);"), 3.0);
    eval("fprintf(fid, 'de');");
    EXPECT_EQ(evalScalar("p2 = ftell(fid);"), 5.0);
    eval("fclose(fid);");
}

TEST_P(FileIoTest, FseekFromBof)
{
    fs->files()["x.txt"] = "abcdefghij";
    eval("fid = fopen('x.txt', 'r');");
    EXPECT_EQ(evalScalar("s = fseek(fid, 5, 'bof');"), 0.0);
    EXPECT_EQ(evalScalar("p = ftell(fid);"), 5.0);
    EXPECT_EQ(evalString("c = fgets(fid);"), "fghij");
    eval("fclose(fid);");
}

TEST_P(FileIoTest, FseekFromCof)
{
    fs->files()["x.txt"] = "abcdefghij";
    eval("fid = fopen('x.txt', 'r');");
    eval("fseek(fid, 3, 'bof');");
    EXPECT_EQ(evalScalar("s = fseek(fid, 2, 'cof');"), 0.0);
    EXPECT_EQ(evalScalar("p = ftell(fid);"), 5.0);
    eval("fclose(fid);");
}

TEST_P(FileIoTest, FseekFromEof)
{
    fs->files()["x.txt"] = "abcdefghij"; // 10 bytes
    eval("fid = fopen('x.txt', 'r');");
    EXPECT_EQ(evalScalar("s = fseek(fid, -3, 'eof');"), 0.0);
    EXPECT_EQ(evalScalar("p = ftell(fid);"), 7.0);
    EXPECT_EQ(evalString("c = fgets(fid);"), "hij");
    eval("fclose(fid);");
}

TEST_P(FileIoTest, FseekAcceptsIntegerOriginCodes)
{
    // MATLAB also accepts -1/0/1 as origin (bof/cof/eof).
    fs->files()["x.txt"] = "abcdefghij";
    eval("fid = fopen('x.txt', 'r');");
    EXPECT_EQ(evalScalar("s1 = fseek(fid, 2, -1);"), 0.0); // bof
    EXPECT_EQ(evalScalar("p1 = ftell(fid);"), 2.0);
    EXPECT_EQ(evalScalar("s2 = fseek(fid, 1, 0);"), 0.0);  // cof
    EXPECT_EQ(evalScalar("p2 = ftell(fid);"), 3.0);
    EXPECT_EQ(evalScalar("s3 = fseek(fid, -1, 1);"), 0.0); // eof
    EXPECT_EQ(evalScalar("p3 = ftell(fid);"), 9.0);
    eval("fclose(fid);");
}

TEST_P(FileIoTest, FseekBeyondEofReturnsMinusOne)
{
    fs->files()["x.txt"] = "abcde";
    eval("fid = fopen('x.txt', 'r');");
    EXPECT_EQ(evalScalar("s = fseek(fid, 100, 'bof');"), -1.0);
    // Cursor unchanged on failure.
    EXPECT_EQ(evalScalar("p = ftell(fid);"), 0.0);
    eval("fclose(fid);");
}

TEST_P(FileIoTest, FseekNegativeBeforeBofReturnsMinusOne)
{
    fs->files()["x.txt"] = "abcde";
    eval("fid = fopen('x.txt', 'r');");
    EXPECT_EQ(evalScalar("s = fseek(fid, -1, 'bof');"), -1.0);
    eval("fclose(fid);");
}

TEST_P(FileIoTest, FseekOnWriteFidFails)
{
    // We don't support random-access writes — our buffer is append-only.
    eval("fid = fopen('out.txt', 'w');");
    EXPECT_EQ(evalScalar("s = fseek(fid, 0, 'bof');"), -1.0);
    eval("fclose(fid);");
}

TEST_P(FileIoTest, FrewindRestartsFromBof)
{
    fs->files()["x.txt"] = "alpha\nbeta\ngamma\n";
    eval("fid = fopen('x.txt', 'r');");
    eval("fgetl(fid);");
    eval("fgetl(fid);");
    eval("frewind(fid);");
    EXPECT_EQ(evalScalar("p = ftell(fid);"), 0.0);
    EXPECT_EQ(evalString("a = fgetl(fid);"), "alpha");
    eval("fclose(fid);");
}

// ── Lifetime edge cases ──────────────────────────────────

TEST_P(FileIoTest, DestructorFlushesOpenFilesOnImplicitClose)
{
    // A script that forgets fclose must not lose its writes when the
    // Engine is destroyed — Engine::~Engine() calls closeAllFiles().
    // The test uses a CallbackFS whose write sink lives OUTSIDE the
    // engine's lifetime, so we can inspect it after engine destruction.
    std::map<std::string, std::string> persisted;
    {
        mlab::Engine local;
        StdLibrary::install(local);
        if (GetParam() == BackendParam::TreeWalker)
            local.setBackend(Engine::Backend::TreeWalker);
        else
            local.setBackend(Engine::Backend::VM);

        auto fs = std::make_unique<mlab::CallbackFS>(
            "temporary",
            [](const std::string &) -> std::string { return ""; },
            [&persisted](const std::string &p, const std::string &c) { persisted[p] = c; },
            [&persisted](const std::string &p) { return persisted.count(p) > 0; });
        local.registerVirtualFS(std::move(fs));
        local.pushScriptOrigin("temporary");

        local.eval("fid = fopen('leak.txt', 'w');");
        local.eval("fprintf(fid, 'no fclose called\\n');");
        // No explicit fclose — engine destructor runs here.
    }

    EXPECT_EQ(persisted["leak.txt"], "no fclose called\n");
}

TEST_P(FileIoTest, OpenFidSurvivesClearAll)
{
    // `clear all` wipes the workspace + user functions, but runtime state
    // like the open-file table must persist — otherwise a long-running
    // script that issues `clear all` would lose its log file handle.
    eval("fid = fopen('survive.txt', 'w');");
    const int fidVal = static_cast<int>(getVar("fid"));
    eval("fprintf(fid, 'before-clear\\n');");

    eval("clear all;");

    // The 'fid' variable is gone from the workspace, but the descriptor
    // is still valid — keep using it via its literal id.
    std::ostringstream code;
    code << "fprintf(" << fidVal << ", 'after-clear\\n'); "
         << "s = fclose(" << fidVal << ");";
    eval(code.str());
    EXPECT_EQ(getVar("s"), 0.0);

    EXPECT_EQ(fs->files()["survive.txt"], "before-clear\nafter-clear\n");
}

INSTANTIATE_DUAL(FileIoTest);
