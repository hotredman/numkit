// tests/stdlib/fileio_test.cpp — fopen / fclose / fprintf-to-file parity
// Parameterized: runs on both TreeWalker and VM backends.
//
// The round-trip target is always a CallbackFS-backed in-memory VirtualFS
// so the tests don't touch disk and are independent of the native FS.

#include "MVfs.hpp"
#include "dual_engine_fixture.hpp"

#include <map>
#include <memory>

using namespace mlab_test;

namespace {

// Minimal in-memory VFS used as a test sink for fprintf-to-file writes.
// Mirrors the pattern from vfs_test.cpp.
class MemoryFS final : public numkit::VirtualFS
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

// ── r+ / w+ / a+ combined modes ──────────────────────────

TEST_P(FileIoTest, FopenRplusOpensExistingForReadWrite)
{
    fs->files()["d.txt"] = "hello world";
    eval("fid = fopen('d.txt', 'r+');");
    EXPECT_GE(getVar("fid"), 3.0);
    // Cursor at start — fgetl reads the existing content.
    EXPECT_EQ(evalString("a = fgetl(fid);"), "hello world");
    // Rewind, overwrite first 5 bytes.
    eval("frewind(fid);");
    eval("fprintf(fid, 'HELLO');");
    eval("fclose(fid);");

    EXPECT_EQ(fs->files()["d.txt"], "HELLO world");
}

TEST_P(FileIoTest, FopenWplusTruncatesThenReadsWhatWasWritten)
{
    fs->files()["d.txt"] = "previous content";
    eval("fid = fopen('d.txt', 'w+');");
    // 'w+' truncates.
    eval("fprintf(fid, 'fresh');");
    // Rewind and read back.
    eval("frewind(fid);");
    EXPECT_EQ(evalString("s = fgetl(fid);"), "fresh");
    eval("fclose(fid);");

    EXPECT_EQ(fs->files()["d.txt"], "fresh");
}

TEST_P(FileIoTest, FopenAplusAppendsAndReadsAfterFrewind)
{
    // MATLAB / C stdio: 'a+' positions the cursor at the END of the
    // existing content. Reads require an explicit frewind or fseek.
    // Writes always go to end regardless of cursor.
    fs->files()["d.txt"] = "old\n";
    eval("fid = fopen('d.txt', 'a+');");
    // No content left to read at the initial cursor.
    EXPECT_EQ(evalScalar("a = fgetl(fid);"), -1.0);
    eval("frewind(fid);");
    EXPECT_EQ(evalString("b = fgetl(fid);"), "old");
    eval("fprintf(fid, 'new\\n');");
    eval("fclose(fid);");

    EXPECT_EQ(fs->files()["d.txt"], "old\nnew\n");
}

TEST_P(FileIoTest, FopenRplusOnMissingFileReturnsMinusOne)
{
    // 'r+' requires the file to exist (same as plain 'r'), unlike
    // 'w+' or 'a+' which create-or-open.
    EXPECT_EQ(evalScalar("fid = fopen('nope.txt', 'r+');"), -1.0);
}

TEST_P(FileIoTest, FopenWplusCreatesMissingFile)
{
    EXPECT_GE(evalScalar("fid = fopen('new.txt', 'w+');"), 3.0);
    eval("fclose(fid);");
    EXPECT_TRUE(fs->files().count("new.txt") > 0);
}

TEST_P(FileIoTest, FseekWorksOnRplusFid)
{
    // r+ has read permission, so fseek is allowed (unlike pure 'w').
    fs->files()["s.txt"] = "abcdef";
    eval("fid = fopen('s.txt', 'r+');");
    EXPECT_EQ(evalScalar("s = fseek(fid, 3, 'bof');"), 0.0);
    eval("fprintf(fid, 'XYZ');");
    eval("fclose(fid);");

    EXPECT_EQ(fs->files()["s.txt"], "abcXYZ");
}

TEST_P(FileIoTest, FprintfAtCursorOverwritesBytes)
{
    // Key difference from the old append-only impl: with 'w' cursor
    // at 0, writes grow the buffer; after seeking back via 'r+' we
    // overwrite instead of appending.
    fs->files()["x.txt"] = "0123456789";
    eval("fid = fopen('x.txt', 'r+');");
    eval("fseek(fid, 2, 'bof');");
    eval("fprintf(fid, 'ABC');");
    eval("fclose(fid);");

    EXPECT_EQ(fs->files()["x.txt"], "01ABC56789");
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

// ── fread / fwrite ───────────────────────────────────────

TEST_P(FileIoTest, FreadDefaultReadsRemainingAsUint8)
{
    // Default precision 'uint8', default size (omitted) == read everything.
    fs->files()["bytes.bin"] = std::string("\x01\x02\x03\x04\x05", 5);
    eval("fid = fopen('bytes.bin', 'r');");
    eval("A = fread(fid);");
    EXPECT_EQ(evalScalar("n = numel(A);"), 5.0);
    EXPECT_EQ(evalScalar("a0 = A(1);"), 1.0);
    EXPECT_EQ(evalScalar("a4 = A(5);"), 5.0);
    eval("fclose(fid);");
}

TEST_P(FileIoTest, FreadScalarSizeReadsExactlyN)
{
    fs->files()["bytes.bin"] = std::string("\x01\x02\x03\x04\x05", 5);
    eval("fid = fopen('bytes.bin', 'r');");
    EXPECT_EQ(evalScalar("n = numel(fread(fid, 3));"), 3.0);
    // Cursor advanced; next read returns the remaining 2.
    EXPECT_EQ(evalScalar("n2 = numel(fread(fid, Inf));"), 2.0);
    eval("fclose(fid);");
}

TEST_P(FileIoTest, FreadInt32LittleEndian)
{
    // 1 and -1 as little-endian int32.
    std::string buf;
    uint32_t one = 1, minus1 = 0xFFFFFFFF;
    buf.append(reinterpret_cast<const char *>(&one), 4);
    buf.append(reinterpret_cast<const char *>(&minus1), 4);
    fs->files()["i32.bin"] = buf;

    eval("fid = fopen('i32.bin', 'r');");
    eval("A = fread(fid, Inf, 'int32');");
    EXPECT_EQ(evalScalar("a0 = A(1);"), 1.0);
    EXPECT_EQ(evalScalar("a1 = A(2);"), -1.0);
    eval("fclose(fid);");
}

TEST_P(FileIoTest, FreadUint16Batch)
{
    std::string buf;
    for (uint16_t v : {uint16_t{0}, uint16_t{1}, uint16_t{256}, uint16_t{65535}}) {
        buf.append(reinterpret_cast<const char *>(&v), 2);
    }
    fs->files()["u16.bin"] = buf;

    eval("fid = fopen('u16.bin', 'r');");
    eval("A = fread(fid, Inf, 'uint16');");
    EXPECT_EQ(evalScalar("a3 = A(4);"), 65535.0);
    eval("fclose(fid);");
}

TEST_P(FileIoTest, FreadFloat64RoundTrip)
{
    double vals[] = {3.14159, -2.71828, 1e-10, 1e10};
    std::string buf(reinterpret_cast<const char *>(vals), sizeof(vals));
    fs->files()["f64.bin"] = buf;

    eval("fid = fopen('f64.bin', 'r');");
    eval("A = fread(fid, Inf, 'double');");
    EXPECT_DOUBLE_EQ(evalScalar("a0 = A(1);"), 3.14159);
    EXPECT_DOUBLE_EQ(evalScalar("a1 = A(2);"), -2.71828);
    EXPECT_DOUBLE_EQ(evalScalar("a2 = A(3);"), 1e-10);
    EXPECT_DOUBLE_EQ(evalScalar("a3 = A(4);"), 1e10);
    eval("fclose(fid);");
}

TEST_P(FileIoTest, FreadFloat32RoundsToFloatPrecision)
{
    float v1 = 3.14159f, v2 = -2.71828f;
    std::string buf;
    buf.append(reinterpret_cast<const char *>(&v1), 4);
    buf.append(reinterpret_cast<const char *>(&v2), 4);
    fs->files()["f32.bin"] = buf;

    eval("fid = fopen('f32.bin', 'r');");
    eval("A = fread(fid, Inf, 'single');");
    // Values come back as double but truncated to float precision.
    EXPECT_NEAR(evalScalar("a0 = A(1);"), static_cast<double>(v1), 1e-6);
    EXPECT_NEAR(evalScalar("a1 = A(2);"), static_cast<double>(v2), 1e-6);
    eval("fclose(fid);");
}

TEST_P(FileIoTest, FreadEofReturnsEmpty)
{
    fs->files()["empty.bin"] = "";
    eval("fid = fopen('empty.bin', 'r');");
    eval("A = fread(fid);");
    EXPECT_EQ(evalScalar("n = numel(A);"), 0.0);
    eval("fclose(fid);");
}

TEST_P(FileIoTest, FreadFewerBytesAvailableThanRequested)
{
    // Ask for 10 uint16 but only 6 bytes in file → get 3 elements.
    fs->files()["short.bin"] = std::string("\x01\x00\x02\x00\x03\x00", 6);
    eval("fid = fopen('short.bin', 'r');");
    eval("A = fread(fid, 10, 'uint16');");
    EXPECT_EQ(evalScalar("n = numel(A);"), 3.0);
    eval("fclose(fid);");
}

TEST_P(FileIoTest, FreadReturnsCountAsSecondOutput)
{
    fs->files()["x.bin"] = std::string("\x01\x02\x03\x04\x05", 5);
    eval("fid = fopen('x.bin', 'r');");
    eval("[A, count] = fread(fid, 3, 'uint8');");
    EXPECT_EQ(evalScalar("n = numel(A);"), 3.0);
    EXPECT_EQ(getVar("count"), 3.0);
    eval("fclose(fid);");
}

TEST_P(FileIoTest, FreadRejectsUnsupportedPrecision)
{
    fs->files()["x.bin"] = "abc";
    eval("fid = fopen('x.bin', 'r');");
    EXPECT_THROW(eval("A = fread(fid, Inf, 'not-a-type');"), std::exception);
    eval("fclose(fid);");
}

TEST_P(FileIoTest, FreadOnWriteFidThrows)
{
    eval("fid = fopen('out.bin', 'w');");
    EXPECT_THROW(eval("A = fread(fid);"), std::exception);
    eval("fclose(fid);");
}

// ── fwrite ───────────────────────────────────────────────

TEST_P(FileIoTest, FwriteDefaultUint8RoundTrip)
{
    eval("fid = fopen('out.bin', 'w');");
    eval("n = fwrite(fid, [65 66 67 68 69]);"); // ABCDE
    eval("fclose(fid);");

    EXPECT_EQ(fs->files()["out.bin"], "ABCDE");
    EXPECT_EQ(getVar("n"), 5.0);
}

TEST_P(FileIoTest, FwriteInt32LittleEndian)
{
    eval("fid = fopen('i32.bin', 'w');");
    eval("fwrite(fid, [1 -1], 'int32');");
    eval("fclose(fid);");

    const std::string &buf = fs->files()["i32.bin"];
    ASSERT_EQ(buf.size(), 8u);
    uint32_t one, minus1;
    std::memcpy(&one, buf.data(), 4);
    std::memcpy(&minus1, buf.data() + 4, 4);
    EXPECT_EQ(one, 1u);
    EXPECT_EQ(minus1, 0xFFFFFFFFu);
}

TEST_P(FileIoTest, FwriteUint16Batch)
{
    eval("fid = fopen('u16.bin', 'w');");
    eval("fwrite(fid, [0 1 256 65535], 'uint16');");
    eval("fclose(fid);");

    const std::string &buf = fs->files()["u16.bin"];
    ASSERT_EQ(buf.size(), 8u);
    uint16_t v0, v1, v2, v3;
    std::memcpy(&v0, buf.data() + 0, 2);
    std::memcpy(&v1, buf.data() + 2, 2);
    std::memcpy(&v2, buf.data() + 4, 2);
    std::memcpy(&v3, buf.data() + 6, 2);
    EXPECT_EQ(v0, 0);
    EXPECT_EQ(v1, 1);
    EXPECT_EQ(v2, 256);
    EXPECT_EQ(v3, 65535);
}

TEST_P(FileIoTest, FwriteDoubleRoundTripThroughFread)
{
    eval("fid = fopen('f64.bin', 'w');");
    eval("fwrite(fid, [3.14 -2.72 1e10], 'double');");
    eval("fclose(fid);");

    eval("rd = fopen('f64.bin', 'r');");
    eval("A = fread(rd, Inf, 'double');");
    eval("fclose(rd);");

    EXPECT_EQ(evalScalar("n = numel(A);"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("a0 = A(1);"), 3.14);
    EXPECT_DOUBLE_EQ(evalScalar("a1 = A(2);"), -2.72);
    EXPECT_DOUBLE_EQ(evalScalar("a2 = A(3);"), 1e10);
}

TEST_P(FileIoTest, FwriteAcceptsLogicalArray)
{
    // Logicals are packed as 0/1 bytes when precision is uint8.
    eval("fid = fopen('bool.bin', 'w');");
    eval("fwrite(fid, [true false true true]);");
    eval("fclose(fid);");

    const std::string &buf = fs->files()["bool.bin"];
    ASSERT_EQ(buf.size(), 4u);
    EXPECT_EQ(static_cast<uint8_t>(buf[0]), 1u);
    EXPECT_EQ(static_cast<uint8_t>(buf[1]), 0u);
    EXPECT_EQ(static_cast<uint8_t>(buf[2]), 1u);
    EXPECT_EQ(static_cast<uint8_t>(buf[3]), 1u);
}

TEST_P(FileIoTest, FwriteReturnsElementCount)
{
    eval("fid = fopen('x.bin', 'w');");
    EXPECT_EQ(evalScalar("n = fwrite(fid, [1 2 3 4 5 6 7], 'uint16');"), 7.0);
    eval("fclose(fid);");
}

TEST_P(FileIoTest, FwriteOnReadFidThrows)
{
    fs->files()["r.bin"] = "abc";
    eval("fid = fopen('r.bin', 'r');");
    EXPECT_THROW(eval("fwrite(fid, [1 2 3]);"), std::exception);
    eval("fclose(fid);");
}

TEST_P(FileIoTest, FwriteRejectsUnsupportedPrecision)
{
    eval("fid = fopen('x.bin', 'w');");
    EXPECT_THROW(eval("fwrite(fid, [1 2], 'not-a-type');"), std::exception);
    eval("fclose(fid);");
}

TEST_P(FileIoTest, FwriteTypedOutputSyntaxIsAccepted)
{
    // MATLAB's src=>dst spec: we ignore the dst part (arrays are always
    // double in our world) but the spec should still parse.
    eval("fid = fopen('x.bin', 'w');");
    eval("fwrite(fid, [1 2 3], 'uint16=>uint16');");
    eval("fclose(fid);");

    EXPECT_EQ(fs->files()["x.bin"].size(), 6u);
}

// ── fprintf / sprintf matrix cycling ─────────────────────

TEST_P(FileIoTest, FprintfCyclesFormatOverVectorArg)
{
    // Classic MATLAB idiom: '%d %d\n' over a flat vector cycles 3 times.
    capturedOutput.clear();
    eval("fprintf('%d %d\\n', [1 2 3 4 5 6]);");
    EXPECT_EQ(capturedOutput, "1 2\n3 4\n5 6\n");
}

TEST_P(FileIoTest, FprintfCyclesColumnMajorOverMatrix)
{
    // Matrix is flattened column-major: [1 3; 2 4] → stream [1 2 3 4].
    capturedOutput.clear();
    eval("fprintf('%d\\n', [1 3; 2 4]);");
    EXPECT_EQ(capturedOutput, "1\n2\n3\n4\n");
}

TEST_P(FileIoTest, FprintfCyclingEmitsLiteralsEveryIteration)
{
    capturedOutput.clear();
    eval("fprintf('[%d]', [10 20 30]);");
    EXPECT_EQ(capturedOutput, "[10][20][30]");
}

TEST_P(FileIoTest, SprintfCyclesMatrixArg)
{
    eval("s = sprintf('%d=%d\\n', [1 2 3], [10 20 30]);");
    // Stream is [1,2,3,10,20,30] (flattened in arg order),
    // format has 2 specs, cycles 3 times →
    //   1=2
    //   3=10
    //   20=30
    EXPECT_EQ(evalString("t = s;"), "1=2\n3=10\n20=30\n");
}

TEST_P(FileIoTest, FprintfScalarArgsDoNotCycle)
{
    // No cycling when every arg is a scalar — behaviour identical to
    // pre-cycling impl.
    capturedOutput.clear();
    eval("fprintf('%d plus %d is %d\\n', 2, 3, 5);");
    EXPECT_EQ(capturedOutput, "2 plus 3 is 5\n");
}

TEST_P(FileIoTest, FprintfCyclingWithLogicalArray)
{
    // Logicals get converted to 0/1 doubles in the stream.
    capturedOutput.clear();
    eval("fprintf('%d\\n', [true false true]);");
    EXPECT_EQ(capturedOutput, "1\n0\n1\n");
}

TEST_P(FileIoTest, FprintfLeftoverFewerElementsThanSpecsStopsGracefully)
{
    // 5 elements with format having 2 specs → 2 full rows + a partial
    // last row that only renders the first %d.
    capturedOutput.clear();
    eval("fprintf('%d,%d\\n', [1 2 3 4 5]);");
    // 1,2\n3,4\n5,\n — the second %d has no value; the literal comma
    // and newline still emit.
    EXPECT_EQ(capturedOutput, "1,2\n3,4\n5,\n");
}

// ── machineformat / endianness override ──────────────────

TEST_P(FileIoTest, FwriteBigEndianUint16)
{
    eval("fid = fopen('be.bin', 'w');");
    eval("fwrite(fid, [1 256 65535], 'uint16', 'ieee-be');");
    eval("fclose(fid);");

    const std::string &buf = fs->files()["be.bin"];
    ASSERT_EQ(buf.size(), 6u);
    // Big-endian 1 = 00 01, 256 = 01 00, 65535 = FF FF
    EXPECT_EQ(static_cast<uint8_t>(buf[0]), 0x00u);
    EXPECT_EQ(static_cast<uint8_t>(buf[1]), 0x01u);
    EXPECT_EQ(static_cast<uint8_t>(buf[2]), 0x01u);
    EXPECT_EQ(static_cast<uint8_t>(buf[3]), 0x00u);
    EXPECT_EQ(static_cast<uint8_t>(buf[4]), 0xFFu);
    EXPECT_EQ(static_cast<uint8_t>(buf[5]), 0xFFu);
}

TEST_P(FileIoTest, FreadBigEndianInt32)
{
    // Hand-craft a BE int32 stream: 1 and -1.
    std::string buf;
    auto pushByte = [&](unsigned v) { buf.push_back(static_cast<char>(v & 0xFFu)); };
    pushByte(0x00); pushByte(0x00); pushByte(0x00); pushByte(0x01);
    pushByte(0xFF); pushByte(0xFF); pushByte(0xFF); pushByte(0xFF);
    fs->files()["be.bin"] = buf;

    eval("fid = fopen('be.bin', 'r');");
    eval("A = fread(fid, Inf, 'int32', 'ieee-be');");
    eval("fclose(fid);");

    EXPECT_EQ(evalScalar("a = A(1);"), 1.0);
    EXPECT_EQ(evalScalar("b = A(2);"), -1.0);
}

TEST_P(FileIoTest, FreadFwriteBeRoundTrip)
{
    eval("wid = fopen('rt.bin', 'w');");
    eval("fwrite(wid, [3.14 -2.5 100], 'double', 'ieee-be');");
    eval("fclose(wid);");

    eval("rid = fopen('rt.bin', 'r');");
    eval("A = fread(rid, Inf, 'double', 'ieee-be');");
    eval("fclose(rid);");

    EXPECT_DOUBLE_EQ(evalScalar("a = A(1);"), 3.14);
    EXPECT_DOUBLE_EQ(evalScalar("b = A(2);"), -2.5);
    EXPECT_DOUBLE_EQ(evalScalar("c = A(3);"), 100.0);
}

TEST_P(FileIoTest, FreadNativeIsLittleEndian)
{
    // Our targets are all LE, so 'native' == 'ieee-le' == default.
    std::string buf;
    uint16_t x = 0x0102;
    buf.append(reinterpret_cast<const char *>(&x), 2);
    fs->files()["le.bin"] = buf;

    eval("fid = fopen('le.bin', 'r');");
    eval("A = fread(fid, 1, 'uint16', 'native');");
    eval("fclose(fid);");

    EXPECT_EQ(evalScalar("a = A;"), 0x0102);
}

TEST_P(FileIoTest, FreadRejectsUnknownMachineFormat)
{
    fs->files()["x.bin"] = "ab";
    eval("fid = fopen('x.bin', 'r');");
    EXPECT_THROW(eval("A = fread(fid, 1, 'uint8', 'not-a-format');"), std::exception);
    eval("fclose(fid);");
}

// ── sscanf ───────────────────────────────────────────────

TEST_P(FileIoTest, SscanfReadsIntegersCycled)
{
    eval("A = sscanf('1 2 3 4 5', '%d');");
    EXPECT_EQ(evalScalar("n = numel(A);"), 5.0);
    EXPECT_EQ(evalScalar("a0 = A(1);"), 1.0);
    EXPECT_EQ(evalScalar("a4 = A(5);"), 5.0);
}

TEST_P(FileIoTest, SscanfReadsFloats)
{
    eval("A = sscanf('1.5 2.5 3.25', '%f');");
    EXPECT_EQ(evalScalar("n = numel(A);"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("a0 = A(1);"), 1.5);
    EXPECT_DOUBLE_EQ(evalScalar("a2 = A(3);"), 3.25);
}

TEST_P(FileIoTest, SscanfCyclesMultiSpecFormat)
{
    // "%d %d" applied twice consumes "1 2 3 4" as four values.
    eval("A = sscanf('1 2 3 4', '%d %d');");
    EXPECT_EQ(evalScalar("n = numel(A);"), 4.0);
    EXPECT_EQ(evalScalar("a3 = A(4);"), 4.0);
}

TEST_P(FileIoTest, SscanfHonorsLiteralSeparators)
{
    eval("A = sscanf('1:2:3', '%d:%d:%d');");
    EXPECT_EQ(evalScalar("n = numel(A);"), 3.0);
    EXPECT_EQ(evalScalar("a0 = A(1);"), 1.0);
    EXPECT_EQ(evalScalar("a2 = A(3);"), 3.0);
}

TEST_P(FileIoTest, SscanfWithSizeLimitsElementCount)
{
    eval("A = sscanf('1 2 3 4 5', '%d', 3);");
    EXPECT_EQ(evalScalar("n = numel(A);"), 3.0);
}

TEST_P(FileIoTest, SscanfSupportsScientificNotation)
{
    eval("A = sscanf('1e3 2.5e-2', '%f');");
    EXPECT_DOUBLE_EQ(evalScalar("a0 = A(1);"), 1000.0);
    EXPECT_DOUBLE_EQ(evalScalar("a1 = A(2);"), 0.025);
}

TEST_P(FileIoTest, SscanfSupportsHexAndOctal)
{
    eval("A = sscanf('ff', '%x');");
    EXPECT_EQ(evalScalar("a0 = A(1);"), 255.0);
    eval("B = sscanf('10', '%o');");
    EXPECT_EQ(evalScalar("b0 = B(1);"), 8.0);
}

TEST_P(FileIoTest, SscanfHandlesNegatives)
{
    eval("A = sscanf('-5 10 -3', '%d');");
    EXPECT_EQ(evalScalar("a0 = A(1);"), -5.0);
    EXPECT_EQ(evalScalar("a2 = A(3);"), -3.0);
}

TEST_P(FileIoTest, SscanfEmptyInputReturnsEmpty)
{
    eval("A = sscanf('', '%d');");
    EXPECT_EQ(evalScalar("n = numel(A);"), 0.0);
}

TEST_P(FileIoTest, SscanfEmptyInputWithTextFormatReturnsEmptyChar)
{
    // MATLAB parity: empty input + pure-text format returns '' (empty
    // char), not [] (empty double). Output type follows the format,
    // not what actually matched.
    eval("A = sscanf('', '%s');");
    EXPECT_TRUE(evalBool("tf = ischar(A);"));
    EXPECT_EQ(evalScalar("n = numel(A);"), 0.0);
}

TEST_P(FileIoTest, SscanfStopsAtNonMatchingInput)
{
    // "1 abc" — matches 1, then fails on "abc". Return [1] and count=1.
    eval("[A, count] = sscanf('1 abc', '%d');");
    EXPECT_EQ(evalScalar("n = numel(A);"), 1.0);
    EXPECT_EQ(getVar("count"), 1.0);
}

TEST_P(FileIoTest, SscanfReturnsCountAsSecondOutput)
{
    eval("[A, count] = sscanf('1 2 3', '%d');");
    EXPECT_EQ(getVar("count"), 3.0);
}

TEST_P(FileIoTest, SscanfSuppressWithStar)
{
    // "%*d" reads and throws away an integer.
    eval("A = sscanf('10 20 30', '%*d %d');");
    EXPECT_EQ(evalScalar("n = numel(A);"), 1.0);
    EXPECT_EQ(evalScalar("a0 = A(1);"), 20.0);
}

TEST_P(FileIoTest, SscanfRejectsTrulyUnsupportedConversion)
{
    // %s and %c are supported now; but unknown letters still fault.
    EXPECT_THROW(eval("A = sscanf('1', '%k');"), std::exception);
}

// ── %s and %c ─────────────────────────────────────────

TEST_P(FileIoTest, SscanfSingleStringToken)
{
    eval("A = sscanf('hello', '%s');");
    // Pure %s → char array of the token.
    EXPECT_TRUE(evalBool("tf = ischar(A);"));
    EXPECT_EQ(evalString("s = A;"), "hello");
}

TEST_P(FileIoTest, SscanfMultipleStringTokensConcatenate)
{
    // MATLAB's quirk: pure %s concatenates tokens with no separator.
    eval("A = sscanf('hello world foo', '%s');");
    EXPECT_EQ(evalString("s = A;"), "helloworldfoo");
}

TEST_P(FileIoTest, SscanfStringWidthTruncatesToken)
{
    eval("A = sscanf('abcdefghij', '%3s');");
    // Pure %s still → char. Width limits to 3 chars per match; format
    // cycles so remaining characters become the next token.
    EXPECT_EQ(evalString("s = A;"), "abcdefghij");  // a|bcd|efg|hij packed together
}

TEST_P(FileIoTest, SscanfCharSingle)
{
    eval("A = sscanf('x', '%c');");
    EXPECT_TRUE(evalBool("tf = ischar(A);"));
    EXPECT_EQ(evalString("s = A;"), "x");
}

TEST_P(FileIoTest, SscanfCharDoesNotSkipWhitespace)
{
    // %c reads literal chars including whitespace (MATLAB parity). We
    // set size=1 so format doesn't cycle and consume the 'x' too —
    // want to assert that the FIRST char read is the space itself.
    eval("A = sscanf(' x', '%c', 1);");
    EXPECT_EQ(evalString("s = A;"), " ");
}

TEST_P(FileIoTest, SscanfCharWithWidth)
{
    eval("A = sscanf('hello', '%3c');");
    EXPECT_EQ(evalString("s = A;"), "hel");
}

TEST_P(FileIoTest, SscanfMixedNumericAndStringReturnsDoubleColumn)
{
    // MATLAB rule: any numeric conversion in the format → the result
    // is a column of doubles, and %s chars become ASCII codes.
    eval("A = sscanf('1 x 2 y', '%d %c');");
    EXPECT_FALSE(evalBool("tf = ischar(A);"));
    EXPECT_EQ(evalScalar("n = numel(A);"), 4.0);
    EXPECT_EQ(evalScalar("a0 = A(1);"), 1.0);
    EXPECT_EQ(evalScalar("a1 = A(2);"), 120.0);  // ASCII for 'x'
    EXPECT_EQ(evalScalar("a2 = A(3);"), 2.0);
    EXPECT_EQ(evalScalar("a3 = A(4);"), 121.0);  // ASCII for 'y'
}

TEST_P(FileIoTest, SscanfSuppressedStringStillConsumedNotEmitted)
{
    eval("A = sscanf('skip 42', '%*s %d');");
    EXPECT_EQ(evalScalar("n = numel(A);"), 1.0);
    EXPECT_EQ(evalScalar("a0 = A(1);"), 42.0);
}

TEST_P(FileIoTest, SscanfCountCountsCharactersForTextFormats)
{
    // MATLAB docs: "count contains the number of characters read" when
    // the format is %s/%c. Our impl pushes one element per char.
    eval("[A, count] = sscanf('hello', '%s');");
    EXPECT_EQ(getVar("count"), 5.0);
}

// ── %[set] char-class conversion ──────────────────────────

TEST_P(FileIoTest, SscanfSetLowercaseLetters)
{
    // Reads chars matching [a-z] until a non-match. Then cycles.
    eval("A = sscanf('abc123xyz', '%[a-z]');");
    // Cycle 1: 'abc' (3 chars), cycle fails on '1' → 'abc' captured.
    // Since no numeric specs → output is char row.
    EXPECT_TRUE(evalBool("tf = ischar(A);"));
    EXPECT_EQ(evalString("s = A;"), "abc");
}

TEST_P(FileIoTest, SscanfSetNegatedClass)
{
    // [^0-9] reads non-digit chars.
    eval("A = sscanf('hello123', '%[^0-9]');");
    EXPECT_EQ(evalString("s = A;"), "hello");
}

TEST_P(FileIoTest, SscanfSetExplicitChars)
{
    eval("A = sscanf('aabbccXX', '%[abc]');");
    EXPECT_EQ(evalString("s = A;"), "aabbcc");
}

TEST_P(FileIoTest, SscanfSetWithWidth)
{
    eval("A = sscanf('abcdef', '%3[a-z]');");
    // width=3 per match; cycles take 'abc' then 'def'. Concatenated.
    EXPECT_EQ(evalString("s = A;"), "abcdef");
}

TEST_P(FileIoTest, SscanfSetNoMatchReturnsEmpty)
{
    // First char doesn't match → no token, count=0.
    eval("[A, count] = sscanf('123', '%[a-z]');");
    EXPECT_TRUE(evalBool("tf = ischar(A);"));
    EXPECT_EQ(evalScalar("n = numel(A);"), 0.0);
    EXPECT_EQ(getVar("count"), 0.0);
}

TEST_P(FileIoTest, SscanfSetMixedWithNumeric)
{
    // Numeric spec present → output becomes double column with ASCII
    // codes for the %[…] match.
    eval("A = sscanf('foo 42', '%[a-z] %d');");
    EXPECT_FALSE(evalBool("tf = ischar(A);"));
    // 'foo' → 3 chars (ASCII 102,111,111), then 42.
    EXPECT_EQ(evalScalar("n = numel(A);"), 4.0);
    EXPECT_EQ(evalScalar("a0 = A(1);"), 102.0);
    EXPECT_EQ(evalScalar("a3 = A(4);"), 42.0);
}

TEST_P(FileIoTest, SscanfSetSuppressed)
{
    eval("A = sscanf('skipABC tail', '%*[a-zA-Z] %s');");
    // '%*[a-zA-Z]' reads and drops 'skipABC'. Then %s reads 'tail'.
    EXPECT_EQ(evalString("s = A;"), "tail");
}

// ── fscanf ───────────────────────────────────────────────

TEST_P(FileIoTest, FscanfReadsFromFile)
{
    fs->files()["nums.txt"] = "1 2 3\n4 5 6\n";
    eval("fid = fopen('nums.txt', 'r');");
    eval("A = fscanf(fid, '%d');");
    EXPECT_EQ(evalScalar("n = numel(A);"), 6.0);
    EXPECT_EQ(evalScalar("a5 = A(6);"), 6.0);
    eval("fclose(fid);");
}

TEST_P(FileIoTest, FscanfAdvancesFileCursor)
{
    fs->files()["x.txt"] = "10 20 30 40";
    eval("fid = fopen('x.txt', 'r');");
    eval("A = fscanf(fid, '%d', 2);");
    EXPECT_EQ(evalScalar("n = numel(A);"), 2.0);
    // Remaining content accessible via ftell advanced + a second fscanf.
    eval("B = fscanf(fid, '%d');");
    EXPECT_EQ(evalScalar("nb = numel(B);"), 2.0);
    EXPECT_EQ(evalScalar("b0 = B(1);"), 30.0);
    eval("fclose(fid);");
}

TEST_P(FileIoTest, FscanfRoundTripThroughFprintf)
{
    // Write numbers with fprintf, read them back with fscanf.
    eval("fid = fopen('rt.txt', 'w');");
    eval("fprintf(fid, '%d %.3f\\n', 42, 3.14);");
    eval("fprintf(fid, '%d %.3f\\n', 100, 2.718);");
    eval("fclose(fid);");

    eval("fid2 = fopen('rt.txt', 'r');");
    eval("A = fscanf(fid2, '%d %f');");
    eval("fclose(fid2);");

    EXPECT_EQ(evalScalar("n = numel(A);"), 4.0);
    EXPECT_EQ(evalScalar("a0 = A(1);"), 42.0);
    EXPECT_DOUBLE_EQ(evalScalar("a1 = A(2);"), 3.14);
    EXPECT_EQ(evalScalar("a2 = A(3);"), 100.0);
    EXPECT_DOUBLE_EQ(evalScalar("a3 = A(4);"), 2.718);
}

TEST_P(FileIoTest, FscanfOnWriteFidThrows)
{
    eval("fid = fopen('out.txt', 'w');");
    EXPECT_THROW(eval("A = fscanf(fid, '%d');"), std::exception);
    eval("fclose(fid);");
}

TEST_P(FileIoTest, FscanfOnInvalidFidThrows)
{
    EXPECT_THROW(eval("A = fscanf(999, '%d');"), std::exception);
}

// ── [fid, errmsg] = fopen(...) ───────────────────────────

TEST_P(FileIoTest, FopenMultiOutputReturnsEmptyErrmsgOnSuccess)
{
    eval("[fid, err] = fopen('ok.txt', 'w');");
    EXPECT_GE(getVar("fid"), 3.0);
    EXPECT_EQ(evalString("s = err;"), "");
    eval("fclose(fid);");
}

TEST_P(FileIoTest, FopenMultiOutputReportsErrmsgOnMissingFile)
{
    eval("[fid, err] = fopen('nonexistent.txt', 'r');");
    EXPECT_EQ(getVar("fid"), -1.0);
    // Message should describe the failure; exact wording depends on the
    // backend but must be non-empty so `if fid < 0, error(err); end`
    // works.
    EXPECT_FALSE(evalString("s = err;").empty());
}

TEST_P(FileIoTest, FopenMultiOutputReportsErrmsgOnBadMode)
{
    eval("[fid, err] = fopen('x.txt', 'bogus');");
    EXPECT_EQ(getVar("fid"), -1.0);
    EXPECT_NE(evalString("s = err;").find("permission"), std::string::npos);
}

TEST_P(FileIoTest, FopenErrmsgIsClearedOnNextSuccessfulOpen)
{
    // First fopen fails — errmsg populated. Second succeeds — errmsg
    // empty. This stateful behaviour matches MATLAB: the `err` out is
    // always the status of the CURRENT call, never sticky.
    eval("[f1, e1] = fopen('missing.txt', 'r');");
    EXPECT_FALSE(evalString("s = e1;").empty());
    eval("[f2, e2] = fopen('new.txt', 'w');");
    EXPECT_EQ(evalString("s = e2;"), "");
    eval("fclose(f2);");
}

// ── fopen('all') → vector of open fids ───────────────────

TEST_P(FileIoTest, FopenAllReturnsEmptyWhenNothingOpen)
{
    eval("ids = fopen('all');");
    EXPECT_EQ(evalScalar("n = numel(ids);"), 0.0);
}

TEST_P(FileIoTest, FopenAllListsUserOpenedFids)
{
    eval("a = fopen('a.txt', 'w');");
    eval("b = fopen('b.txt', 'w');");
    eval("c = fopen('c.txt', 'w');");
    eval("ids = fopen('all');");
    EXPECT_EQ(evalScalar("n = numel(ids);"), 3.0);
    // MATLAB returns a ROW vector — verify shape.
    EXPECT_EQ(evalScalar("r = size(ids, 1);"), 1.0);
    EXPECT_EQ(evalScalar("c = size(ids, 2);"), 3.0);
    eval("fclose('all');");
}

TEST_P(FileIoTest, FopenAllReflectsClosures)
{
    eval("a = fopen('a.txt', 'w');");
    eval("b = fopen('b.txt', 'w');");
    eval("fclose(a);");
    eval("ids = fopen('all');");
    EXPECT_EQ(evalScalar("n = numel(ids);"), 1.0);
    eval("fclose('all');");
}

TEST_P(FileIoTest, FopenAllDoesNotListReservedFids)
{
    // stdin/stdout/stderr (0/1/2) must not show up in fopen('all').
    eval("a = fopen('a.txt', 'w');");
    eval("ids = fopen('all');");
    // Only our one user fid (>= 3).
    EXPECT_EQ(evalScalar("n = numel(ids);"), 1.0);
    EXPECT_GE(evalScalar("i0 = ids(1);"), 3.0);
    eval("fclose('all');");
}

TEST_P(FileIoTest, FopenAllWithModeArgFallsBackToLiteralFilename)
{
    // Contract: 'all' is special ONLY as the sole arg. With a mode,
    // it's treated as a regular filename.
    eval("fid = fopen('all', 'w');");
    EXPECT_GE(getVar("fid"), 3.0);
    eval("fclose(fid);");
    EXPECT_TRUE(fs->files().count("all") > 0);
}

// ── ferror ───────────────────────────────────────────────

TEST_P(FileIoTest, FerrorIsEmptyOnFreshFid)
{
    fs->files()["x.txt"] = "abc";
    eval("fid = fopen('x.txt', 'r');");
    EXPECT_EQ(evalString("msg = ferror(fid);"), "");
    eval("fclose(fid);");
}

TEST_P(FileIoTest, FerrorReportsEofFromFgetl)
{
    fs->files()["one.txt"] = "only\n";
    eval("fid = fopen('one.txt', 'r');");
    eval("a = fgetl(fid);");
    eval("b = fgetl(fid);");  // hits EOF, returns -1
    EXPECT_EQ(evalString("msg = ferror(fid);"), "End of file reached.");
    eval("fclose(fid);");
}

TEST_P(FileIoTest, FerrorReportsShortFread)
{
    // Ask for more bytes than available — ferror should describe it.
    fs->files()["short.bin"] = std::string("\x01\x02", 2);
    eval("fid = fopen('short.bin', 'r');");
    eval("A = fread(fid, 10, 'uint8');");
    EXPECT_EQ(evalString("msg = ferror(fid);"), "End of file reached.");
    eval("fclose(fid);");
}

TEST_P(FileIoTest, FerrorSecondOutIsErrnum)
{
    fs->files()["one.txt"] = "only\n";
    eval("fid = fopen('one.txt', 'r');");
    eval("a = fgetl(fid);");
    eval("b = fgetl(fid);");
    eval("[msg, errnum] = ferror(fid);");
    EXPECT_EQ(getVar("errnum"), -1.0);
    // Successful read, no outstanding error — errnum should be 0.
    eval("fclose(fid);");
    fs->files()["ok.txt"] = "alpha\n";
    eval("fid2 = fopen('ok.txt', 'r');");
    eval("x = fgetl(fid2);");
    eval("[m2, en2] = ferror(fid2);");
    EXPECT_EQ(getVar("en2"), 0.0);
    eval("fclose(fid2);");
}

TEST_P(FileIoTest, FerrorClearResetsState)
{
    fs->files()["one.txt"] = "only\n";
    eval("fid = fopen('one.txt', 'r');");
    eval("fgetl(fid); fgetl(fid);");  // second call errors
    EXPECT_FALSE(evalString("m = ferror(fid);").empty());
    eval("ferror(fid, 'clear');");
    EXPECT_EQ(evalString("m = ferror(fid);"), "");
    eval("fclose(fid);");
}

TEST_P(FileIoTest, FerrorOnInvalidFidThrows)
{
    EXPECT_THROW(eval("x = ferror(999);"), std::exception);
}

// ── Matrix-shape size [m n] ──────────────────────────────

TEST_P(FileIoTest, FreadMatrixShapeFixedColumns)
{
    // 6 bytes → [2 3] matrix, column-major fill.
    fs->files()["x.bin"] = std::string("\x01\x02\x03\x04\x05\x06", 6);
    eval("fid = fopen('x.bin', 'r');");
    eval("A = fread(fid, [2 3], 'uint8');");
    eval("fclose(fid);");
    // Matrix is 2x3, filled column-major:
    //   [1 3 5
    //    2 4 6]
    EXPECT_EQ(evalScalar("r = size(A, 1);"), 2.0);
    EXPECT_EQ(evalScalar("c = size(A, 2);"), 3.0);
    EXPECT_EQ(evalScalar("a11 = A(1,1);"), 1.0);
    EXPECT_EQ(evalScalar("a21 = A(2,1);"), 2.0);
    EXPECT_EQ(evalScalar("a13 = A(1,3);"), 5.0);
    EXPECT_EQ(evalScalar("a23 = A(2,3);"), 6.0);
}

TEST_P(FileIoTest, FreadMatrixShapeWithInfCols)
{
    // 7 bytes with [3 Inf] → 3x3 matrix, last column partial, padded with 0.
    fs->files()["x.bin"] = std::string("\x01\x02\x03\x04\x05\x06\x07", 7);
    eval("fid = fopen('x.bin', 'r');");
    eval("A = fread(fid, [3 Inf], 'uint8');");
    eval("fclose(fid);");
    EXPECT_EQ(evalScalar("r = size(A, 1);"), 3.0);
    EXPECT_EQ(evalScalar("c = size(A, 2);"), 3.0);
    EXPECT_EQ(evalScalar("a11 = A(1,1);"), 1.0);
    EXPECT_EQ(evalScalar("a33 = A(3,3);"), 0.0);  // padded
    EXPECT_EQ(evalScalar("a13 = A(1,3);"), 7.0);
}

TEST_P(FileIoTest, FreadMatrixShapeShortFilePadsWithZeros)
{
    // 5 bytes, shape [3 2] = 6 cells → last cell zero-padded.
    fs->files()["x.bin"] = std::string("\x01\x02\x03\x04\x05", 5);
    eval("fid = fopen('x.bin', 'r');");
    eval("A = fread(fid, [3 2], 'uint8');");
    eval("fclose(fid);");
    EXPECT_EQ(evalScalar("r = size(A, 1);"), 3.0);
    EXPECT_EQ(evalScalar("c = size(A, 2);"), 2.0);
    EXPECT_EQ(evalScalar("a12 = A(1,2);"), 4.0);
    EXPECT_EQ(evalScalar("a22 = A(2,2);"), 5.0);
    EXPECT_EQ(evalScalar("a32 = A(3,2);"), 0.0);  // padded
}

TEST_P(FileIoTest, FreadMatrixShapeRejectsBadVector)
{
    fs->files()["x.bin"] = "abc";
    eval("fid = fopen('x.bin', 'r');");
    EXPECT_THROW(eval("A = fread(fid, [1 2 3], 'uint8');"), std::exception);
    eval("fclose(fid);");
}

TEST_P(FileIoTest, FscanfMatrixShapeFixed)
{
    fs->files()["x.txt"] = "1 2 3 4 5 6";
    eval("fid = fopen('x.txt', 'r');");
    eval("A = fscanf(fid, '%d', [2 3]);");
    eval("fclose(fid);");
    EXPECT_EQ(evalScalar("r = size(A, 1);"), 2.0);
    EXPECT_EQ(evalScalar("c = size(A, 2);"), 3.0);
    // Column-major fill: A(1,1)=1, A(2,1)=2, A(1,2)=3, ...
    EXPECT_EQ(evalScalar("a11 = A(1,1);"), 1.0);
    EXPECT_EQ(evalScalar("a21 = A(2,1);"), 2.0);
    EXPECT_EQ(evalScalar("a13 = A(1,3);"), 5.0);
}

TEST_P(FileIoTest, FscanfMatrixShapeInfCols)
{
    fs->files()["x.txt"] = "1 2 3 4 5 6 7";
    eval("fid = fopen('x.txt', 'r');");
    eval("A = fscanf(fid, '%d', [2 Inf]);");
    eval("fclose(fid);");
    // 7 ints into [2 Inf] → 2x4 matrix with A(2,4) = 0 (padded).
    EXPECT_EQ(evalScalar("r = size(A, 1);"), 2.0);
    EXPECT_EQ(evalScalar("c = size(A, 2);"), 4.0);
    EXPECT_EQ(evalScalar("a14 = A(1,4);"), 7.0);
    EXPECT_EQ(evalScalar("a24 = A(2,4);"), 0.0);
}

TEST_P(FileIoTest, SscanfMatrixShapeMixedNumericString)
{
    // Mixed format still shapes: %d %f repeated gives a column of
    // doubles that we reshape.
    eval("A = sscanf('1 1.5 2 2.5 3 3.5', '%d %f', [2 3]);");
    EXPECT_EQ(evalScalar("r = size(A, 1);"), 2.0);
    EXPECT_EQ(evalScalar("c = size(A, 2);"), 3.0);
    EXPECT_EQ(evalScalar("a11 = A(1,1);"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("a21 = A(2,1);"), 1.5);
}

TEST_P(FileIoTest, SscanfCharMatrixShape)
{
    // Text-only format + matrix size → char MATRIX (not flat row).
    // Column-major fill: 'abcdef' into [2 3] yields
    //   a c e
    //   b d f
    // MLab uses linear indexing for char arrays (column-major), so
    // A(1)='a' A(2)='b' A(3)='c' A(4)='d' A(5)='e' A(6)='f'.
    eval("A = sscanf('abcdef', '%c', [2 3]);");
    EXPECT_TRUE(evalBool("tf = ischar(A);"));
    EXPECT_EQ(evalScalar("r = size(A, 1);"), 2.0);
    EXPECT_EQ(evalScalar("c = size(A, 2);"), 3.0);
    EXPECT_EQ(evalString("s1 = A(1);"), "a");
    EXPECT_EQ(evalString("s4 = A(4);"), "d");
    EXPECT_EQ(evalString("s5 = A(5);"), "e");
}

TEST_P(FileIoTest, SscanfCharMatrixShapeInfCols)
{
    // 7 chars into [2 Inf] → 2x4 with last cell zero-padded.
    eval("A = sscanf('abcdefg', '%c', [2 Inf]);");
    EXPECT_TRUE(evalBool("tf = ischar(A);"));
    EXPECT_EQ(evalScalar("r = size(A, 1);"), 2.0);
    EXPECT_EQ(evalScalar("c = size(A, 2);"), 4.0);
    EXPECT_EQ(evalString("s7 = A(7);"), "g");
}

// ── textscan ─────────────────────────────────────────────

TEST_P(FileIoTest, TextscanBasicNumericPerColumn)
{
    // 2 conversions → cell array with 2 cells, each a column vector.
    eval("C = textscan('1 1.5 2 2.5 3 3.5', '%d %f');");
    EXPECT_TRUE(evalBool("tf = iscell(C);"));
    EXPECT_EQ(evalScalar("n = numel(C);"), 2.0);

    // First cell: the three integers.
    EXPECT_EQ(evalScalar("v = numel(C{1});"), 3.0);
    EXPECT_EQ(evalScalar("a = C{1}(1);"), 1.0);
    EXPECT_EQ(evalScalar("b = C{1}(3);"), 3.0);

    // Second cell: the three floats.
    EXPECT_DOUBLE_EQ(evalScalar("c = C{2}(1);"), 1.5);
    EXPECT_DOUBLE_EQ(evalScalar("d = C{2}(3);"), 3.5);
}

TEST_P(FileIoTest, TextscanStringColumnIsCellstr)
{
    eval("C = textscan('a 1 b 2 c 3', '%s %d');");
    // %s → inner cell array of strings.
    EXPECT_TRUE(evalBool("tf = iscell(C{1});"));
    EXPECT_EQ(evalScalar("n = numel(C{1});"), 3.0);
    EXPECT_EQ(evalString("s = C{1}{1};"), "a");
    EXPECT_EQ(evalString("s = C{1}{3};"), "c");
    // %d column → plain double column.
    EXPECT_EQ(evalScalar("a = C{2}(2);"), 2.0);
}

TEST_P(FileIoTest, TextscanCycleCapLimitsRows)
{
    // N=2 → stop after 2 full cycles, so 2 rows per column.
    eval("C = textscan('1 a 2 b 3 c 4 d', '%d %s', 2);");
    EXPECT_EQ(evalScalar("n = numel(C{1});"), 2.0);
    EXPECT_EQ(evalScalar("n2 = numel(C{2});"), 2.0);
    EXPECT_EQ(evalScalar("a = C{1}(2);"), 2.0);
}

TEST_P(FileIoTest, TextscanCustomDelimiter)
{
    eval("C = textscan('1,2,3,4,5', '%d', 'Delimiter', ',');");
    EXPECT_EQ(evalScalar("n = numel(C{1});"), 5.0);
    EXPECT_EQ(evalScalar("a = C{1}(5);"), 5.0);
}

TEST_P(FileIoTest, TextscanCommaSeparatedWithMixedFormat)
{
    eval("C = textscan('alpha,1.5,beta,2.5', '%s %f', 'Delimiter', ',');");
    EXPECT_EQ(evalScalar("n = numel(C{1});"), 2.0);
    EXPECT_EQ(evalString("s = C{1}{1};"), "alpha");
    EXPECT_DOUBLE_EQ(evalScalar("v = C{2}(2);"), 2.5);
}

TEST_P(FileIoTest, TextscanSkipsHeaderLines)
{
    // Multi-line content has to go through a fid — MLab single-quoted
    // strings don't carry embedded newlines.
    fs->files()["hdr.txt"] = "# metadata\n# more meta\n1 2\n3 4\n5 6\n";
    eval("fid = fopen('hdr.txt', 'r');");
    eval("C = textscan(fid, '%d %d', 'HeaderLines', 2);");
    eval("fclose(fid);");
    EXPECT_EQ(evalScalar("n = numel(C{1});"), 3.0);
    EXPECT_EQ(evalScalar("a = C{1}(1);"), 1.0);
    EXPECT_EQ(evalScalar("b = C{2}(3);"), 6.0);
}

TEST_P(FileIoTest, TextscanSuppressConversion)
{
    // %*s skips the token but doesn't produce a column.
    eval("C = textscan('skip 1 skip 2', '%*s %d');");
    EXPECT_EQ(evalScalar("n = numel(C);"), 1.0);
    EXPECT_EQ(evalScalar("a = C{1}(1);"), 1.0);
    EXPECT_EQ(evalScalar("b = C{1}(2);"), 2.0);
}

TEST_P(FileIoTest, TextscanFromFid)
{
    fs->files()["data.csv"] = "name,value\nalpha,1.5\nbeta,2.5\n";
    eval("fid = fopen('data.csv', 'r');");
    eval("C = textscan(fid, '%s %f', 'Delimiter', ',', 'HeaderLines', 1);");
    eval("fclose(fid);");

    EXPECT_EQ(evalScalar("n = numel(C{1});"), 2.0);
    EXPECT_EQ(evalString("s = C{1}{2};"), "beta");
    EXPECT_DOUBLE_EQ(evalScalar("v = C{2}(1);"), 1.5);
}

TEST_P(FileIoTest, TextscanPartialCycleRollsBack)
{
    // "1 2 3" with format "%d %d" — can do one full cycle (1,2), "3"
    // alone doesn't fit a full cycle, so it's left in the stream.
    eval("C = textscan('1 2 3', '%d %d');");
    EXPECT_EQ(evalScalar("n = numel(C{1});"), 1.0);
    EXPECT_EQ(evalScalar("a = C{1}(1);"), 1.0);
    EXPECT_EQ(evalScalar("b = C{2}(1);"), 2.0);
}

TEST_P(FileIoTest, TextscanRejectsUnsupportedOption)
{
    EXPECT_THROW(eval("C = textscan('1', '%d', 'NotAnOption', 42);"), std::exception);
}

TEST_P(FileIoTest, TextscanRejectsEmptyFormat)
{
    EXPECT_THROW(eval("C = textscan('1', '');"), std::exception);
}

TEST_P(FileIoTest, TextscanEmptyInputProducesEmptyColumns)
{
    eval("C = textscan('', '%d %s');");
    EXPECT_EQ(evalScalar("n = numel(C);"), 2.0);
    EXPECT_EQ(evalScalar("k1 = numel(C{1});"), 0.0);
    EXPECT_EQ(evalScalar("k2 = numel(C{2});"), 0.0);
}

TEST_P(FileIoTest, TextscanEndOfLineOptionIsRespected)
{
    // Explicit EndOfLine set to ';' — semicolons terminate records,
    // newlines no longer do. With delimiter ',' and EOL ';',
    // "1,2;3,4" parses into two rows of [1,2] and [3,4].
    fs->files()["semi.txt"] = "1,2;3,4;5,6";
    eval("fid = fopen('semi.txt', 'r');");
    eval("C = textscan(fid, '%d %d', 'Delimiter', ',', 'EndOfLine', ';');");
    eval("fclose(fid);");
    EXPECT_EQ(evalScalar("n = numel(C{1});"), 3.0);
    EXPECT_EQ(evalScalar("a = C{1}(3);"), 5.0);
    EXPECT_EQ(evalScalar("b = C{2}(3);"), 6.0);
}

TEST_P(FileIoTest, TextscanEndOfLineDrivesHeaderLinesSkip)
{
    // 'HeaderLines' counts lines by the EndOfLine chars, not by '\n'.
    fs->files()["semi.txt"] = "header1;header2;1 2;3 4";
    eval("fid = fopen('semi.txt', 'r');");
    eval("C = textscan(fid, '%d %d', 'EndOfLine', ';', 'HeaderLines', 2);");
    eval("fclose(fid);");
    EXPECT_EQ(evalScalar("n = numel(C{1});"), 2.0);
    EXPECT_EQ(evalScalar("a = C{1}(1);"), 1.0);
    EXPECT_EQ(evalScalar("b = C{2}(2);"), 4.0);
}

TEST_P(FileIoTest, TextscanCommentStylePercent)
{
    fs->files()["cs.txt"] = "1 2\n% this is a comment\n3 4\n5 6\n";
    eval("fid = fopen('cs.txt', 'r');");
    eval("C = textscan(fid, '%d %d', 'CommentStyle', '%');");
    eval("fclose(fid);");
    EXPECT_EQ(evalScalar("n = numel(C{1});"), 3.0);
    EXPECT_EQ(evalScalar("a = C{1}(3);"), 5.0);
}

TEST_P(FileIoTest, TextscanCommentStyleInlineTrailingComment)
{
    // Comments can appear mid-line; tokenisation stops at the marker.
    fs->files()["inl.txt"] = "1 2 // trailing\n3 4\n";
    eval("fid = fopen('inl.txt', 'r');");
    eval("C = textscan(fid, '%d %d', 'CommentStyle', '//');");
    eval("fclose(fid);");
    EXPECT_EQ(evalScalar("n = numel(C{1});"), 2.0);
    EXPECT_EQ(evalScalar("a = C{2}(1);"), 2.0);
    EXPECT_EQ(evalScalar("b = C{2}(2);"), 4.0);
}

TEST_P(FileIoTest, TextscanTreatAsEmptySubstitutesNaN)
{
    // 'NA' tokens become NaN in numeric columns.
    eval("C = textscan('1 NA 3 4 NA 6', '%f %f %f', 'TreatAsEmpty', 'NA');");
    EXPECT_EQ(evalScalar("n = numel(C{1});"), 2.0);
    // Row 1: 1, NaN, 3 — column 2 at row 1 is NaN.
    EXPECT_TRUE(evalBool("tf = isnan(C{2}(1));"));
    // Row 2: 4, NaN, 6 — column 2 at row 2 is also NaN.
    EXPECT_TRUE(evalBool("tf2 = isnan(C{2}(2));"));
    EXPECT_EQ(evalScalar("a = C{3}(2);"), 6.0);
}

TEST_P(FileIoTest, TextscanTreatAsEmptyMultipleMarkers)
{
    // Cell-array form accepts multiple empty-markers.
    eval("C = textscan('1 NA 2 NULL 3 - 4', '%d', 'TreatAsEmpty', {'NA','NULL','-'});");
    EXPECT_EQ(evalScalar("n = numel(C{1});"), 7.0);
    EXPECT_TRUE(evalBool("tf2 = isnan(C{1}(2));"));
    EXPECT_TRUE(evalBool("tf4 = isnan(C{1}(4));"));
    EXPECT_TRUE(evalBool("tf6 = isnan(C{1}(6));"));
    EXPECT_EQ(evalScalar("a7 = C{1}(7);"), 4.0);
}

TEST_P(FileIoTest, TextscanTreatAsEmptyWithStringSpec)
{
    // 'TreatAsEmpty' also affects %s — matching tokens become ''.
    eval("C = textscan('a - b -', '%s', 'TreatAsEmpty', '-');");
    EXPECT_EQ(evalScalar("n = numel(C{1});"), 4.0);
    EXPECT_EQ(evalString("s = C{1}{1};"), "a");
    EXPECT_EQ(evalString("s2 = C{1}{2};"), "");
    EXPECT_EQ(evalString("s3 = C{1}{3};"), "b");
    EXPECT_EQ(evalString("s4 = C{1}{4};"), "");
}

// ── MultipleDelimsAsOne / EmptyValue — explicit-delim empty fields ──

TEST_P(FileIoTest, TextscanEmptyFieldBecomesNaNByDefault)
{
    // MATLAB-correct: consecutive explicit delims yield empty fields,
    // filled with EmptyValue (default NaN).
    eval("C = textscan('1,,3', '%f', 'Delimiter', ',');");
    EXPECT_EQ(evalScalar("n = numel(C{1});"), 3.0);
    EXPECT_EQ(evalScalar("a1 = C{1}(1);"), 1.0);
    EXPECT_TRUE(evalBool("tf = isnan(C{1}(2));"));
    EXPECT_EQ(evalScalar("a3 = C{1}(3);"), 3.0);
}

TEST_P(FileIoTest, TextscanEmptyValueOverridesNaN)
{
    // User-supplied EmptyValue replaces the NaN default.
    eval("C = textscan('1,,3,,5', '%d', 'Delimiter', ',', 'EmptyValue', 0);");
    EXPECT_EQ(evalScalar("n = numel(C{1});"), 5.0);
    EXPECT_EQ(evalScalar("a1 = C{1}(1);"), 1.0);
    EXPECT_EQ(evalScalar("a2 = C{1}(2);"), 0.0);
    EXPECT_EQ(evalScalar("a3 = C{1}(3);"), 3.0);
    EXPECT_EQ(evalScalar("a4 = C{1}(4);"), 0.0);
    EXPECT_EQ(evalScalar("a5 = C{1}(5);"), 5.0);
}

TEST_P(FileIoTest, TextscanMultipleDelimsAsOneCollapsesRuns)
{
    // With MultipleDelimsAsOne=true, consecutive delims collapse and
    // we get back the pre-refactor "no empty fields" behaviour.
    eval("C = textscan('1,,3,,,5', '%d', 'Delimiter', ',', "
         "'MultipleDelimsAsOne', true);");
    EXPECT_EQ(evalScalar("n = numel(C{1});"), 3.0);
    EXPECT_EQ(evalScalar("a1 = C{1}(1);"), 1.0);
    EXPECT_EQ(evalScalar("a2 = C{1}(2);"), 3.0);
    EXPECT_EQ(evalScalar("a3 = C{1}(3);"), 5.0);
}

TEST_P(FileIoTest, TextscanEmptyFieldWithStringSpec)
{
    // Empty fields also work with %s — a cellstr entry of ''.
    eval("C = textscan('a,,c', '%s', 'Delimiter', ',');");
    EXPECT_EQ(evalScalar("n = numel(C{1});"), 3.0);
    EXPECT_EQ(evalString("s1 = C{1}{1};"), "a");
    EXPECT_EQ(evalString("s2 = C{1}{2};"), "");
    EXPECT_EQ(evalString("s3 = C{1}{3};"), "c");
}

TEST_P(FileIoTest, TextscanMixedFormatRespectsEmptyFieldColumn)
{
    // Format '%s %d' with 'a,,2': column 1 has 'a', column 2 sees ",2" —
    // first %d would read empty, then cycle. Detailed expected output:
    //   cycle 1: %s='a', %d=empty → NaN
    //   cycle 2 starts with '2' left over? Let's test the simplest case:
    eval("C = textscan('a,,c,,e', '%s %d', 'Delimiter', ',');");
    // Row 1: 'a' and NaN; row 2: 'c' and NaN;  then 'e' left alone → partial.
    // So 2 complete rows.
    EXPECT_EQ(evalScalar("n = numel(C{1});"), 2.0);
    EXPECT_EQ(evalString("s1 = C{1}{1};"), "a");
    EXPECT_TRUE(evalBool("tf = isnan(double(C{2}(1)));"));
}

TEST_P(FileIoTest, TextscanWhitespaceTrimmedAroundExplicitDelimFields)
{
    // With Delimiter=',', surrounding whitespace on each field must
    // be stripped (so strtoX parses cleanly).
    eval("C = textscan('1 , 2 , 3', '%d', 'Delimiter', ',');");
    EXPECT_EQ(evalScalar("n = numel(C{1});"), 3.0);
    EXPECT_EQ(evalScalar("a = C{1}(1);"), 1.0);
    EXPECT_EQ(evalScalar("b = C{1}(3);"), 3.0);
}

TEST_P(FileIoTest, TextscanWhitespaceDefaultStillCollapsesRuns)
{
    // Default mode: runs of whitespace stay collapsed regardless of
    // MultipleDelimsAsOne — MATLAB-documented.
    eval("C = textscan('1     2   3', '%d');");
    EXPECT_EQ(evalScalar("n = numel(C{1});"), 3.0);
    EXPECT_EQ(evalScalar("b = C{1}(2);"), 2.0);
}

TEST_P(FileIoTest, TextscanMultipleConsecutiveDelimsSeparateFields)
{
    // Three fields → three doubles, two empty (NaN).
    eval("C = textscan(',,', '%f', 'Delimiter', ',');");
    EXPECT_EQ(evalScalar("n = numel(C{1});"), 3.0);
    EXPECT_TRUE(evalBool("t1 = isnan(C{1}(1));"));
    EXPECT_TRUE(evalBool("t2 = isnan(C{1}(2));"));
    EXPECT_TRUE(evalBool("t3 = isnan(C{1}(3));"));
}

// ── save / load (ascii) ──────────────────────────────────

TEST_P(FileIoTest, SaveLoadRoundTripMatrix)
{
    eval("A = [1 2 3; 4 5 6];");
    eval("save('mat.txt', 'A', '-ascii');");
    eval("B = load('mat.txt');");

    EXPECT_EQ(evalScalar("r = size(B, 1);"), 2.0);
    EXPECT_EQ(evalScalar("c = size(B, 2);"), 3.0);
    EXPECT_EQ(evalScalar("a11 = B(1,1);"), 1.0);
    EXPECT_EQ(evalScalar("a23 = B(2,3);"), 6.0);
}

TEST_P(FileIoTest, SaveLoadPreservesDoublePrecision)
{
    eval("A = [0.1 0.2 0.3];");
    eval("save('prec.txt', 'A', '-ascii');");
    eval("B = load('prec.txt');");
    EXPECT_DOUBLE_EQ(evalScalar("a = B(1);"), 0.1);
    EXPECT_DOUBLE_EQ(evalScalar("b = B(2);"), 0.2);
    EXPECT_DOUBLE_EQ(evalScalar("c = B(3);"), 0.3);
}

TEST_P(FileIoTest, SaveWithoutDashAsciiStillWritesText)
{
    // Default (no flag) falls back to ascii in this build — .mat
    // binary isn't implemented.
    eval("X = [10 20; 30 40];");
    eval("save('default.txt', 'X');");
    EXPECT_TRUE(fs->files().count("default.txt") > 0);
}

TEST_P(FileIoTest, SaveRejectsBinaryMatFlags)
{
    eval("Y = [1 2];");
    EXPECT_THROW(eval("save('y.mat', 'Y', '-mat');"), std::exception);
    EXPECT_THROW(eval("save('y.mat', 'Y', '-v7.3');"), std::exception);
}

TEST_P(FileIoTest, SaveRejectsMissingVariable)
{
    EXPECT_THROW(eval("save('x.txt', 'ZZZ_does_not_exist');"), std::exception);
}

TEST_P(FileIoTest, SaveRequiresAtLeastOneVariable)
{
    EXPECT_THROW(eval("save('x.txt');"), std::exception);
}

TEST_P(FileIoTest, LoadSkipsCommentLines)
{
    fs->files()["comments.txt"] = "% header comment\n1 2 3\n# another comment\n4 5 6\n";
    eval("M = load('comments.txt');");
    EXPECT_EQ(evalScalar("r = size(M, 1);"), 2.0);
    EXPECT_EQ(evalScalar("c = size(M, 2);"), 3.0);
    EXPECT_EQ(evalScalar("a = M(2,3);"), 6.0);
}

TEST_P(FileIoTest, LoadSkipsEmptyLines)
{
    fs->files()["blanks.txt"] = "\n\n1 2\n\n3 4\n\n";
    eval("M = load('blanks.txt');");
    EXPECT_EQ(evalScalar("r = size(M, 1);"), 2.0);
    EXPECT_EQ(evalScalar("c = size(M, 2);"), 2.0);
}

TEST_P(FileIoTest, LoadWithoutLhsAssignsToFileStem)
{
    fs->files()["data.txt"] = "1 2\n3 4\n";
    eval("load('data.txt');");
    // Variable 'data' should now exist in the workspace.
    EXPECT_EQ(evalScalar("r = size(data, 1);"), 2.0);
    EXPECT_EQ(evalScalar("a = data(1,1);"), 1.0);
    EXPECT_EQ(evalScalar("b = data(2,2);"), 4.0);
}

TEST_P(FileIoTest, LoadRejectsInconsistentColumnCounts)
{
    fs->files()["bad.txt"] = "1 2 3\n4 5\n6 7 8\n";
    EXPECT_THROW(eval("M = load('bad.txt');"), std::exception);
}

TEST_P(FileIoTest, LoadRejectsEmptyFile)
{
    fs->files()["empty.txt"] = "";
    EXPECT_THROW(eval("M = load('empty.txt');"), std::exception);
}

TEST_P(FileIoTest, LoadOfSingleValueReturnsScalar)
{
    fs->files()["one.txt"] = "42\n";
    eval("x = load('one.txt');");
    EXPECT_TRUE(evalBool("tf = isscalar(x);"));
    EXPECT_EQ(evalScalar("v = x;"), 42.0);
}

TEST_P(FileIoTest, SaveMultipleVariablesConcatenated)
{
    eval("A = [1 2; 3 4];");
    eval("B = [9 8; 7 6];");
    eval("save('multi.txt', 'A', 'B', '-ascii');");
    // Loading returns a 4x2 matrix: rows of A then blank-separated rows of B.
    eval("M = load('multi.txt');");
    EXPECT_EQ(evalScalar("r = size(M, 1);"), 4.0);
    EXPECT_EQ(evalScalar("c = size(M, 2);"), 2.0);
    EXPECT_EQ(evalScalar("a11 = M(1,1);"), 1.0);
    EXPECT_EQ(evalScalar("a31 = M(3,1);"), 9.0);
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
        numkit::Engine local;
        StdLibrary::install(local);
        if (GetParam() == BackendParam::TreeWalker)
            local.setBackend(Engine::Backend::TreeWalker);
        else
            local.setBackend(Engine::Backend::VM);

        auto fs = std::make_unique<numkit::CallbackFS>(
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
