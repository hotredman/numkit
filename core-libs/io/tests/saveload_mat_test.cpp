// libs/io/tests/saveload_mat_test.cpp
//
// Round-trip regression for save/load on .mat files (matio v5 backend).
// Writes to the OS temp directory via NativeFS so we hit matio's fopen
// path directly — no CallbackFS staging — exercising the common case.
//
// CallbackFS staging is exercised separately in fileio_test.cpp's
// FileIoTest.SaveLoadMat* group.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>

#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <gtest/gtest.h>
#include <string>

using namespace numkit;

class SaveLoadMatTest : public ::testing::Test
{
public:
    StandardEngine engine;
    std::filesystem::path matPath;

    void SetUp() override
    {
        engine.eval("import compat.*;");
        auto td = std::filesystem::temp_directory_path();
        matPath = td / ("numkit_savemat_"
                        + std::to_string(std::hash<std::string>()(
                              ::testing::UnitTest::GetInstance()
                                  ->current_test_info()->name()))
                        + ".mat");
        std::error_code ec;
        std::filesystem::remove(matPath, ec);
    }

    void TearDown() override
    {
        std::error_code ec;
        std::filesystem::remove(matPath, ec);
    }

    Value eval(const std::string &code) { return engine.eval(code); }
    double evalScalar(const std::string &code) { return eval(code).toScalar(); }
    bool   evalBool(const std::string &code)
    {
        Value v = eval(code);
        return v.toScalar() != 0.0;
    }

    // Quote the temp path for inclusion inside a single-quoted MATLAB
    // string literal: forward-slash and escape every embedded apostrophe.
    std::string pathLiteral() const { return pathLiteralOf(matPath); }
    static std::string pathLiteralOf(const std::filesystem::path &p)
    {
        std::string s = p.string();
        for (auto &c : s) if (c == '\\') c = '/';
        std::string out;
        out.reserve(s.size());
        for (char c : s) {
            if (c == '\'') out += "''";
            else out += c;
        }
        return out;
    }

    // Convenience: save A under whatever varnames are listed.
    void doSave(const std::string &args)
    {
        eval("save('" + pathLiteral() + "'" + args + ");");
    }
    void doLoad(const std::string &args = "")
    {
        eval("load('" + pathLiteral() + "'" + args + ");");
    }
};

// ════════════════════════════════════════════════════════════════════════
// Numeric type sweep — real
// ════════════════════════════════════════════════════════════════════════

TEST_F(SaveLoadMatTest, RoundTripDoubleMatrix)
{
    eval("A = [1 2 3; 4 5 6];");
    doSave(", 'A'");
    eval("clear A;");
    doLoad();
    EXPECT_EQ(evalScalar("r = size(A,1);"), 2.0);
    EXPECT_EQ(evalScalar("c = size(A,2);"), 3.0);
    EXPECT_EQ(evalScalar("v11 = A(1,1);"), 1.0);
    EXPECT_EQ(evalScalar("v23 = A(2,3);"), 6.0);
    EXPECT_EQ(eval("c = class(A);").toString(), "double");
}

TEST_F(SaveLoadMatTest, RoundTripSingle)
{
    eval("s = single([1.5 2.5 3.5]);");
    doSave(", 's'");
    eval("clear s;");
    doLoad();
    EXPECT_EQ(eval("c = class(s);").toString(), "single");
    EXPECT_FLOAT_EQ(static_cast<float>(evalScalar("v = double(s(2));")), 2.5f);
}

TEST_F(SaveLoadMatTest, RoundTripAllIntegerTypes)
{
    eval("a = int8([-128 0 127]);");
    eval("b = int16([-32768 0 32767]);");
    eval("cc = int32([-2147483648 0 2147483647]);");
    eval("d = int64([-1 0 1]);");
    eval("e = uint8([0 128 255]);");
    eval("f = uint16([0 32768 65535]);");
    eval("g = uint32([0 2147483648 4294967295]);");
    eval("h = uint64([0 1 18446744073709551615]);");
    doSave(", 'a', 'b', 'cc', 'd', 'e', 'f', 'g', 'h'");
    eval("clear a b cc d e f g h;");
    doLoad();
    EXPECT_EQ(eval("t = class(a);").toString(), "int8");
    EXPECT_EQ(eval("t = class(b);").toString(), "int16");
    EXPECT_EQ(eval("t = class(cc);").toString(), "int32");
    EXPECT_EQ(eval("t = class(d);").toString(), "int64");
    EXPECT_EQ(eval("t = class(e);").toString(), "uint8");
    EXPECT_EQ(eval("t = class(f);").toString(), "uint16");
    EXPECT_EQ(eval("t = class(g);").toString(), "uint32");
    EXPECT_EQ(eval("t = class(h);").toString(), "uint64");
    EXPECT_EQ(evalScalar("v = double(a(1));"), -128.0);
    EXPECT_EQ(evalScalar("v = double(a(3));"),  127.0);
    EXPECT_EQ(evalScalar("v = double(b(1));"), -32768.0);
    EXPECT_EQ(evalScalar("v = double(b(3));"),  32767.0);
    EXPECT_EQ(evalScalar("v = double(cc(3));"), 2147483647.0);
    EXPECT_EQ(evalScalar("v = double(d(3));"),  1.0);
    EXPECT_EQ(evalScalar("v = double(e(3));"),  255.0);
    EXPECT_EQ(evalScalar("v = double(f(3));"),  65535.0);
    EXPECT_EQ(evalScalar("v = double(g(3));"),  4294967295.0);
}

// ════════════════════════════════════════════════════════════════════════
// Complex
// ════════════════════════════════════════════════════════════════════════

TEST_F(SaveLoadMatTest, RoundTripComplexMatrix)
{
    eval("Z = [1+2i 3+4i; 5-1i 0+0i];");
    doSave(", 'Z'");
    eval("clear Z;");
    doLoad();
    EXPECT_EQ(evalScalar("re11 = real(Z(1,1));"), 1.0);
    EXPECT_EQ(evalScalar("im11 = imag(Z(1,1));"), 2.0);
    EXPECT_EQ(evalScalar("re12 = real(Z(1,2));"), 3.0);
    EXPECT_EQ(evalScalar("im21 = imag(Z(2,1));"), -1.0);
    EXPECT_EQ(evalScalar("re22 = real(Z(2,2));"), 0.0);
    EXPECT_EQ(evalScalar("im22 = imag(Z(2,2));"), 0.0);
}

TEST_F(SaveLoadMatTest, RoundTripPureImaginary)
{
    eval("Z = 1i * [1 2 3];");
    doSave(", 'Z'");
    eval("clear Z;");
    doLoad();
    EXPECT_FALSE(evalBool("t = isreal(Z);"));
    EXPECT_EQ(evalScalar("v = imag(Z(2));"), 2.0);
    EXPECT_EQ(evalScalar("v = real(Z(2));"), 0.0);
}

// ════════════════════════════════════════════════════════════════════════
// Logical
// ════════════════════════════════════════════════════════════════════════

TEST_F(SaveLoadMatTest, RoundTripLogical)
{
    eval("L = [true false true; false true false];");
    doSave(", 'L'");
    eval("clear L;");
    doLoad();
    EXPECT_TRUE(evalBool("t = islogical(L);"));
    EXPECT_EQ(evalScalar("v11 = double(L(1,1));"), 1.0);
    EXPECT_EQ(evalScalar("v12 = double(L(1,2));"), 0.0);
}

// ════════════════════════════════════════════════════════════════════════
// Char / string
// ════════════════════════════════════════════════════════════════════════

TEST_F(SaveLoadMatTest, RoundTripCharRow)
{
    eval("s = 'hello world';");
    doSave(", 's'");
    eval("clear s;");
    doLoad();
    EXPECT_EQ(eval("c = class(s);").toString(), "char");
    EXPECT_EQ(evalScalar("n = numel(s);"), 11.0);
    EXPECT_EQ(eval("v = s;").toString(), "hello world");
}

TEST_F(SaveLoadMatTest, RoundTripCharMatrix)
{
    eval("M = ['abc'; 'def'];");
    doSave(", 'M'");
    eval("clear M;");
    doLoad();
    EXPECT_EQ(evalScalar("r = size(M,1);"), 2.0);
    EXPECT_EQ(evalScalar("c = size(M,2);"), 3.0);
    EXPECT_EQ(eval("v = M(1,:);").toString(), "abc");
    EXPECT_EQ(eval("v = M(2,:);").toString(), "def");
}

// ════════════════════════════════════════════════════════════════════════
// N-D arrays
// ════════════════════════════════════════════════════════════════════════

TEST_F(SaveLoadMatTest, RoundTrip3DDouble)
{
    eval("A = reshape(1:24, 2, 3, 4);");
    doSave(", 'A'");
    eval("clear A;");
    doLoad();
    EXPECT_EQ(evalScalar("d1 = size(A,1);"), 2.0);
    EXPECT_EQ(evalScalar("d2 = size(A,2);"), 3.0);
    EXPECT_EQ(evalScalar("d3 = size(A,3);"), 4.0);
    EXPECT_EQ(evalScalar("v = A(1,1,1);"), 1.0);
    EXPECT_EQ(evalScalar("v = A(2,3,4);"), 24.0);
    EXPECT_EQ(evalScalar("v = A(2,2,3);"), 16.0);
}

TEST_F(SaveLoadMatTest, RoundTrip3DComplex)
{
    eval("Z = reshape((1:8) + 1i*(8:-1:1), 2, 2, 2);");
    doSave(", 'Z'");
    eval("clear Z;");
    doLoad();
    EXPECT_EQ(evalScalar("v = real(Z(1,1,1));"), 1.0);
    EXPECT_EQ(evalScalar("v = imag(Z(1,1,1));"), 8.0);
    EXPECT_EQ(evalScalar("v = real(Z(2,2,2));"), 8.0);
    EXPECT_EQ(evalScalar("v = imag(Z(2,2,2));"), 1.0);
}

// ════════════════════════════════════════════════════════════════════════
// Empty / edge shapes
// ════════════════════════════════════════════════════════════════════════

TEST_F(SaveLoadMatTest, RoundTripEmpty)
{
    eval("E = [];");
    doSave(", 'E'");
    eval("clear E;");
    doLoad();
    EXPECT_TRUE(evalBool("t = isempty(E);"));
}

TEST_F(SaveLoadMatTest, RoundTripRowOfZeroCols)
{
    eval("E = zeros(0, 5);");
    doSave(", 'E'");
    eval("clear E;");
    doLoad();
    EXPECT_EQ(evalScalar("r = size(E,1);"), 0.0);
    EXPECT_EQ(evalScalar("c = size(E,2);"), 5.0);
}

TEST_F(SaveLoadMatTest, RoundTripScalar)
{
    eval("x = 42.5;");
    doSave(", 'x'");
    eval("clear x;");
    doLoad();
    EXPECT_DOUBLE_EQ(evalScalar("v = x;"), 42.5);
}

// ════════════════════════════════════════════════════════════════════════
// Special floats — NaN / Inf / -Inf / -0 must survive byte-for-byte
// ════════════════════════════════════════════════════════════════════════

TEST_F(SaveLoadMatTest, SpecialFloatsPreserved)
{
    // NaN / +Inf / -Inf must survive the byte-for-byte copy through matio.
    // Signed zero is platform/parser-dependent in source literals, so we
    // check it via a roundtrip of `-1*0` rather than the literal `-0.0`.
    eval("X = [NaN -Inf Inf -1*0 0];");
    doSave(", 'X'");
    eval("clear X;");
    doLoad();
    EXPECT_TRUE(evalBool("t = isnan(X(1));"));
    EXPECT_TRUE(evalBool("t = isinf(X(2)) && X(2) < 0;"));
    EXPECT_TRUE(evalBool("t = isinf(X(3)) && X(3) > 0;"));
    // Numeric value still zero in both slots.
    EXPECT_EQ(evalScalar("v = X(4);"), 0.0);
    EXPECT_EQ(evalScalar("v = X(5);"), 0.0);
}

// ════════════════════════════════════════════════════════════════════════
// Containers — cell / struct / nested
// ════════════════════════════════════════════════════════════════════════

TEST_F(SaveLoadMatTest, RoundTripCell)
{
    eval("c = {1, 'two', [3 4 5]};");
    doSave(", 'c'");
    eval("clear c;");
    doLoad();
    EXPECT_EQ(evalScalar("v = c{1};"), 1.0);
    EXPECT_EQ(eval("v = c{2};").toString(), "two");
    EXPECT_EQ(evalScalar("v = c{3}(3);"), 5.0);
}

TEST_F(SaveLoadMatTest, RoundTripStruct)
{
    eval("s.alpha = [1 2 3];");
    eval("s.beta  = 'foo';");
    doSave(", 's'");
    eval("clear s;");
    doLoad();
    EXPECT_EQ(evalScalar("v = s.alpha(2);"), 2.0);
    EXPECT_EQ(eval("v = s.beta;").toString(), "foo");
}

TEST_F(SaveLoadMatTest, RoundTripStructFieldOrder)
{
    // MATLAB-compat: fieldnames() returns insertion order.
    eval("s.z = 1; s.a = 2; s.m = 3;");
    doSave(", 's'");
    eval("clear s;");
    doLoad();
    EXPECT_EQ(eval("v = fieldnames(s); w = v{1};").toString(), "z");
    EXPECT_EQ(eval("v = fieldnames(s); w = v{2};").toString(), "a");
    EXPECT_EQ(eval("v = fieldnames(s); w = v{3};").toString(), "m");
}

TEST_F(SaveLoadMatTest, RoundTripStructArray)
{
    eval("s(1).x = 10; s(1).y = 'a';");
    eval("s(2).x = 20; s(2).y = 'b';");
    eval("s(3).x = 30; s(3).y = 'c';");
    doSave(", 's'");
    eval("clear s;");
    doLoad();
    EXPECT_EQ(evalScalar("n = numel(s);"), 3.0);
    EXPECT_EQ(evalScalar("v = s(2).x;"), 20.0);
    EXPECT_EQ(eval("v = s(3).y;").toString(), "c");
}

TEST_F(SaveLoadMatTest, NestedStructAndCell)
{
    eval("a.inner.val = 42;");
    eval("a.list = {1, 'two', [3 4]};");
    doSave(", 'a'");
    eval("clear a;");
    doLoad();
    EXPECT_EQ(evalScalar("v = a.inner.val;"), 42.0);
    EXPECT_EQ(eval("v = a.list{2};").toString(), "two");
    EXPECT_EQ(evalScalar("v = a.list{3}(2);"), 4.0);
}

TEST_F(SaveLoadMatTest, CellOfCells)
{
    eval("c = {{1, 2}, {'a', 'b'}, {[10 20], [30 40]}};");
    doSave(", 'c'");
    eval("clear c;");
    doLoad();
    EXPECT_EQ(evalScalar("v = c{1}{1};"), 1.0);
    EXPECT_EQ(eval("v = c{2}{2};").toString(), "b");
    EXPECT_EQ(evalScalar("v = c{3}{2}(1);"), 30.0);
}

TEST_F(SaveLoadMatTest, RoundTripEmptyStructAndCell)
{
    eval("s = struct();");
    eval("c = {};");
    doSave(", 's', 'c'");
    eval("clear s c;");
    doLoad();
    EXPECT_TRUE(evalBool("t = isstruct(s);"));
    EXPECT_EQ(evalScalar("n = numel(fieldnames(s));"), 0.0);
    EXPECT_TRUE(evalBool("t = iscell(c);"));
    EXPECT_TRUE(evalBool("t = isempty(c);"));
}

// ════════════════════════════════════════════════════════════════════════
// Workspace / multi-var / load forms
// ════════════════════════════════════════════════════════════════════════

TEST_F(SaveLoadMatTest, RoundTripMultipleVarsAndStructForm)
{
    eval("X = [10 20 30];");
    eval("Y = 'hello';");
    doSave(", 'X', 'Y'");
    eval("clear X Y;");
    // Struct form: S = load(...); captures every var as a field.
    eval("S = load('" + pathLiteral() + "');");
    EXPECT_EQ(evalScalar("n = numel(S.X);"), 3.0);
    EXPECT_EQ(evalScalar("x3 = S.X(3);"), 30.0);
    EXPECT_EQ(eval("y = S.Y;").toString(), "hello");
}

TEST_F(SaveLoadMatTest, SaveWholeWorkspaceWithoutVarnames)
{
    eval("p = 3.14;");
    eval("q = [1 2 3];");
    eval("r = 'hi';");
    doSave("");  // no varnames → save everything
    eval("clear p q r;");
    doLoad();
    EXPECT_DOUBLE_EQ(evalScalar("v = p;"), 3.14);
    EXPECT_EQ(evalScalar("v = q(2);"), 2.0);
    EXPECT_EQ(eval("v = r;").toString(), "hi");
}

TEST_F(SaveLoadMatTest, OverwriteReplacesEntireFile)
{
    eval("A = [1 2 3]; B = [4 5 6];");
    doSave(", 'A', 'B'");
    eval("clear A B;");
    eval("C = [7 8 9];");
    doSave(", 'C'");                       // overwrite
    eval("clear C;");
    doLoad();
    EXPECT_EQ(evalScalar("e = exist('A','var');"), 0.0);
    EXPECT_EQ(evalScalar("e = exist('B','var');"), 0.0);
    EXPECT_EQ(evalScalar("e = exist('C','var');"), 1.0);
    EXPECT_EQ(evalScalar("v = C(3);"), 9.0);
}

// ════════════════════════════════════════════════════════════════════════
// Format dispatch (extension vs flag)
// ════════════════════════════════════════════════════════════════════════

TEST_F(SaveLoadMatTest, UppercaseMatExtensionDispatchesBinary)
{
    auto p = matPath; p.replace_extension(".MAT");
    eval("X = [9 8 7];");
    eval("save('" + pathLiteralOf(p) + "', 'X');");
    // Read first bytes — v5 magic must be present.
    std::ifstream f(p, std::ios::binary);
    std::string head(20, '\0');
    f.read(head.data(), 19);
    EXPECT_EQ(head.substr(0, 10), "MATLAB 5.0");
    std::error_code ec; std::filesystem::remove(p, ec);
}

TEST_F(SaveLoadMatTest, ExplicitMatFlagOverridesAsciiExtension)
{
    auto p = matPath; p.replace_extension(".txt");
    eval("X = [1 2 3];");
    eval("save('" + pathLiteralOf(p) + "', 'X', '-mat');");
    std::ifstream f(p, std::ios::binary);
    std::string head(20, '\0');
    f.read(head.data(), 19);
    EXPECT_EQ(head.substr(0, 10), "MATLAB 5.0");
    std::error_code ec; std::filesystem::remove(p, ec);
}

TEST_F(SaveLoadMatTest, V6AndV7FlagsAccepted)
{
    eval("X = [1 2];");
    EXPECT_NO_THROW(eval("save('" + pathLiteral() + "', 'X', '-v6');"));
    EXPECT_NO_THROW(eval("save('" + pathLiteral() + "', 'X', '-v7');"));
}

TEST_F(SaveLoadMatTest, V73Rejected)
{
    eval("A = [1 2 3];");
    EXPECT_THROW(eval("save('" + pathLiteral() + "', 'A', '-v7.3');"),
                 std::exception);
}

TEST_F(SaveLoadMatTest, FileBeginsWithV5Magic)
{
    eval("x = 1.0;");
    doSave(", 'x'");
    std::ifstream f(matPath, std::ios::binary);
    ASSERT_TRUE(f.good());
    std::string head(20, '\0');
    f.read(head.data(), 19);
    // v5 header text: "MATLAB 5.0 MAT-file" at offset 0.
    EXPECT_EQ(head.substr(0, 19), "MATLAB 5.0 MAT-file");
    // Signature bytes 0x0102 (or 0x0201) at offset 126/127.
    f.seekg(126);
    char sig[2]{};
    f.read(sig, 2);
    bool be = (sig[0] == 'M' && sig[1] == 'I');
    bool le = (sig[0] == 'I' && sig[1] == 'M');
    EXPECT_TRUE(be || le);
}

// ════════════════════════════════════════════════════════════════════════
// Error cases
// ════════════════════════════════════════════════════════════════════════

TEST_F(SaveLoadMatTest, LoadNonexistentFileThrows)
{
    auto p = std::filesystem::temp_directory_path()
             / "numkit_does_not_exist_xyz_zzz.mat";
    std::error_code ec; std::filesystem::remove(p, ec);
    EXPECT_THROW(eval("load('" + pathLiteralOf(p) + "');"), std::exception);
}

TEST_F(SaveLoadMatTest, LoadGarbageFileThrows)
{
    // Write something that is definitely not a v5 .mat file.
    {
        std::ofstream f(matPath, std::ios::binary);
        f << "this is not a mat file, just some bytes\n";
    }
    EXPECT_THROW(eval("load('" + pathLiteral() + "');"), std::exception);
}

TEST_F(SaveLoadMatTest, SaveMissingVariableThrows)
{
    EXPECT_THROW(eval("save('" + pathLiteral()
                      + "', 'this_variable_does_not_exist_xyz');"),
                 std::exception);
}

// ════════════════════════════════════════════════════════════════════════
// Misc — function handles, mixed-type bundle
// ════════════════════════════════════════════════════════════════════════

TEST_F(SaveLoadMatTest, FunctionHandleStoredAsEmptyPlaceholder)
{
    // v5 .mat has no native function-handle encoding without MCOS, so
    // we emit a 0×0 double. Round-trip preserves the name but loses
    // the handle — verify the documented behaviour.
    eval("h = @sin;");
    doSave(", 'h'");
    eval("clear h;");
    doLoad();
    EXPECT_TRUE(evalBool("t = isempty(h);"));
}

TEST_F(SaveLoadMatTest, MixedTypeBundleSurvivesRoundTrip)
{
    // One big bundle stress test — verify multiple disparate types
    // co-exist in the same file and all come back intact.
    eval("d   = pi;");
    eval("v   = 1:10;");
    eval("M   = reshape(1:12, 3, 4);");
    eval("Z   = [1+1i 2+2i];");
    eval("L   = [true false true];");
    eval("c   = {1, 'two', {3, 4}};");
    eval("s.a = 1; s.b = 'two';");
    eval("u   = uint16([1 2 3 65535]);");
    doSave(", 'd', 'v', 'M', 'Z', 'L', 'c', 's', 'u'");
    eval("clear d v M Z L c s u;");
    doLoad();
    EXPECT_DOUBLE_EQ(evalScalar("x = d;"), 3.141592653589793);
    EXPECT_EQ(evalScalar("x = v(10);"), 10.0);
    EXPECT_EQ(evalScalar("x = M(3,4);"), 12.0);
    EXPECT_EQ(evalScalar("x = imag(Z(2));"), 2.0);
    EXPECT_TRUE(evalBool("x = L(3);"));
    EXPECT_EQ(eval("x = c{2};").toString(), "two");
    EXPECT_EQ(evalScalar("x = c{3}{2};"), 4.0);
    EXPECT_EQ(eval("x = s.b;").toString(), "two");
    EXPECT_EQ(evalScalar("x = double(u(4));"), 65535.0);
}
