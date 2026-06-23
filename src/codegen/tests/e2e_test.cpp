// codegen/tests/e2e_test.cpp
//
// Brick 5: the end-to-end differential gate — the contract that the whole
// pipeline is correct, not just each unit. It transpiles biquad.m to C++
// (emitFunction), compiles it with the external compiler (aot harness),
// RUNS the native binary, and diffs its output against:
//   A. the numkit runtime's own filter() over the same input (the
//      "diff vs the interpreter" the design calls for, DESIGN.md §10); and
//   B. the exact Direct-Form-I recurrence in plain C++ (a tight-tol guard
//      that pins any emitter arithmetic/index bug precisely).
//
// Skips cleanly when no compiler is configured, so a toolchain-less CI
// keeps the baseline green.

#include <numkit/codegen/aot.hpp>
#include <numkit/codegen/emitter.hpp>
#include <numkit/codegen/transfer.hpp>

#include <numkit/core/lexer.hpp>
#include <numkit/core/parser.hpp>

#include <numkit/bundle/standard_engine.hpp>
#include <numkit/core/engine.hpp>
#include <numkit/value/value.hpp>

#include "nk_codegen_test_paths.h"  // NK_BRIDGE_DIR / NK_RT_IMPORT_LIB / NK_RT_SHARED_DLL

#include <gtest/gtest.h>

#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using numkit::ValueType;
using namespace numkit::codegen;

namespace {

const char *kBiquadSrc =
    "function y = biquad(x, b0, b1, b2, a1, a2)\n"
    "  n = numel(x);\n"
    "  y = zeros(1, n);\n"
    "  x1 = 0; x2 = 0; y1 = 0; y2 = 0;\n"
    "  for k = 1:n\n"
    "    xn = x(k);\n"
    "    yn = b0*xn + b1*x1 + b2*x2 - a1*y1 - a2*y2;\n"
    "    y(k) = yn;\n"
    "    x2 = x1; x1 = xn;\n"
    "    y2 = y1; y1 = yn;\n"
    "  end\n"
    "end\n";

std::string fwd(std::string s)  // backslashes -> forward (valid in C++ literal + ok for fopen)
{
    for (char &c : s)
        if (c == '\\') c = '/';
    return s;
}

// Compile a self-contained program, run it (no args — it writes results to
// `outTxt`, whose path the caller baked into the program), and return the
// doubles it printed. Asserts on compile/run failure.
std::vector<double> compileRunReadDoubles(const std::string &program,
                                          const std::string &exe,
                                          const std::string &outTxt)
{
    std::error_code ec;
    std::filesystem::remove(outTxt, ec);
    const auto r = numkit::codegen::aot::compileToExecutable(program, exe);
    EXPECT_EQ(r.status, numkit::codegen::aot::CompileStatus::Ok)
        << "log:\n" << r.log << "\n--- generated source ---\n" << program;
    std::vector<double> got;
    if (r.ok() && std::system(("\"" + exe + "\"").c_str()) == 0) {
        std::ifstream is(outTxt);
        double        v;
        while (is >> v) got.push_back(v);
    } else {
        ADD_FAILURE() << "compile or run failed";
    }
    return got;
}

// Transpile the single FUNCTION_DEF in `srcM` with the given param types.
EmittedFunction transpile(const std::string &srcM, const std::vector<ParamSpec> &params)
{
    TransferRegistry reg;
    registerStandardTransfers(reg);
    numkit::Lexer  lex(srcM);
    numkit::Parser parser(lex.tokenize());
    auto           root = parser.parse();
    const numkit::ASTNode *fn = nullptr;
    for (const auto &c : root->children)
        if (c && c->type == numkit::NodeType::FUNCTION_DEF) fn = c.get();
    EXPECT_NE(fn, nullptr);
    return emitFunction(*fn, params, reg);
}

}  // namespace

TEST(CodegenE2E, BiquadMatchesRuntimeFilter)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    // 1. Transpile biquad.m -> C++ (RawBuffer ABI).
    TransferRegistry reg;
    registerStandardTransfers(reg);
    numkit::Lexer  lex(kBiquadSrc);
    numkit::Parser parser(lex.tokenize());
    auto           root = parser.parse();
    const numkit::ASTNode *fn = nullptr;
    for (const auto &c : root->children)
        if (c && c->type == numkit::NodeType::FUNCTION_DEF) fn = c.get();
    ASSERT_NE(fn, nullptr);

    std::vector<ParamSpec> params = {
        {"x", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}};
    for (const char *p : {"b0", "b1", "b2", "a1", "a2"})
        params.push_back({p, InferredType::scalar(ValueType::DOUBLE)});
    const EmittedFunction emitted = emitFunction(*fn, params, reg);

    // 2. Wrap in a main() that runs it on a deterministic input and writes
    //    the result. The output path is baked in (forward slashes) so the
    //    binary takes no args — sidesteps cmd nested-quote hazards.
    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_biquad_e2e.exe").string();
    const std::string outTxt = (base / "nk_biquad_e2e_out.txt").string();
    std::error_code   ec;
    std::filesystem::remove(outTxt, ec);

    const std::size_t N = 64;
    std::string program = emitted.source;
    program +=
        "#include <cstdio>\n"
        "int main() {\n"
        "  const std::size_t N = 64;\n"
        "  double x[64], y[64];\n"
        "  for (std::size_t i = 0; i < N; ++i) x[i] = std::sin(0.01 * double(i + 1));\n"
        "  biquad(x, N, 0.0675, 0.1349, 0.0675, -1.1430, 0.4128, y, N);\n"
        "  std::FILE* f = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!f) return 2;\n"
        "  for (std::size_t i = 0; i < N; ++i) std::fprintf(f, \"%.17g\\n\", y[i]);\n"
        "  std::fclose(f);\n"
        "  return 0;\n"
        "}\n";

    // 3. Compile + run.
    const auto r = aot::compileToExecutable(program, exe);
    ASSERT_EQ(r.status, aot::CompileStatus::Ok)
        << "log:\n" << r.log << "\n--- generated source ---\n" << program;
    ASSERT_EQ(std::system(("\"" + exe + "\"").c_str()), 0);

    // 4. Read the transpiled binary's output.
    std::vector<double> got;
    {
        std::ifstream is(outTxt);
        double        v;
        while (is >> v) got.push_back(v);
    }
    ASSERT_EQ(got.size(), N) << "transpiled binary did not produce N outputs";

    // 5A. Reference: the numkit runtime's filter() over the same input.
    numkit::StandardEngine engine;
    engine.eval("import compat.*;");
    engine.eval("xs = sin(0.01*(1:64));");
    numkit::Value yv =
        engine.eval("filter([0.0675 0.1349 0.0675],[1 -1.1430 0.4128], xs);");
    ASSERT_EQ(yv.numel(), N);
    const double *yref = yv.doubleData();
    for (std::size_t i = 0; i < N; ++i)
        EXPECT_NEAR(got[i], yref[i], 1e-9) << "runtime filter() mismatch at i=" << i;

    // 5B. Reference: the exact DF-I recurrence (tight tol).
    {
        const double b0 = 0.0675, b1 = 0.1349, b2 = 0.0675, a1 = -1.1430, a2 = 0.4128;
        double       x1 = 0, x2 = 0, y1 = 0, y2 = 0;
        for (std::size_t i = 0; i < N; ++i) {
            const double xn = std::sin(0.01 * double(i + 1));
            const double yn = b0 * xn + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
            EXPECT_NEAR(got[i], yn, 1e-12) << "DF-I recurrence mismatch at i=" << i;
            x2 = x1; x1 = xn;
            y2 = y1; y1 = yn;
        }
    }
}

// The bounds-CHECKED path (the form that survives if brick-6 promotion is
// deleted — the no-kludge litmus made executable). `y(k) = x(k) + k` is
// not clean-index promotable (k in arithmetic), so the emitted code uses
// nk_rt::index / index_set; it must still run correctly.
TEST(CodegenE2E, CheckedIndexPathRunsCorrectly)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    TransferRegistry reg;
    registerStandardTransfers(reg);
    numkit::Lexer  lex("function y = f(x)\n  n = numel(x);\n  y = zeros(1, n);\n"
                       "  for k = 1:n\n    y(k) = x(k) + k;\n  end\nend\n");
    numkit::Parser parser(lex.tokenize());
    auto           root = parser.parse();
    const numkit::ASTNode *fn = nullptr;
    for (const auto &c : root->children)
        if (c && c->type == numkit::NodeType::FUNCTION_DEF) fn = c.get();
    ASSERT_NE(fn, nullptr);
    const EmittedFunction emitted =
        emitFunction(*fn, {{"x", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}},
                     reg);
    ASSERT_TRUE(emitted.source.find("nk_rt::index") != std::string::npos)
        << "expected the checked path, not promotion";

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_checked_e2e.exe").string();
    const std::string outTxt = (base / "nk_checked_e2e_out.txt").string();
    std::error_code   ec;
    std::filesystem::remove(outTxt, ec);

    const std::size_t N       = 32;
    std::string       program = emitted.source;
    program +=
        "#include <cstdio>\n"
        "int main() {\n"
        "  const std::size_t N = 32;\n"
        "  double x[32], y[32];\n"
        "  for (std::size_t i = 0; i < N; ++i) x[i] = std::sin(0.01 * double(i + 1));\n"
        "  f(x, N, y, N);\n"
        "  std::FILE* g = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!g) return 2;\n"
        "  for (std::size_t i = 0; i < N; ++i) std::fprintf(g, \"%.17g\\n\", y[i]);\n"
        "  std::fclose(g); return 0;\n"
        "}\n";

    const auto r = aot::compileToExecutable(program, exe);
    ASSERT_EQ(r.status, aot::CompileStatus::Ok)
        << "log:\n" << r.log << "\n--- generated source ---\n" << program;
    ASSERT_EQ(std::system(("\"" + exe + "\"").c_str()), 0);

    std::vector<double> got;
    {
        std::ifstream is(outTxt);
        double        v;
        while (is >> v) got.push_back(v);
    }
    ASSERT_EQ(got.size(), N);
    for (std::size_t i = 0; i < N; ++i)
        EXPECT_NEAR(got[i], std::sin(0.01 * double(i + 1)) + double(i + 1), 1e-12)
            << "checked-path mismatch at i=" << i;
}

// SOUNDNESS e2e: the loop bound is reassigned (`n = numel(x); n = 3;`) so
// the loop must run 3 times, NOT numel(x)=8. The output buffer is sized 3.
// If the stale numel fact wrongly promoted the loop to `k < _nk_x_len` (=8) it
// would write y[0..7] out of bounds. Correct emission runs k=1..3 only.
TEST(CodegenE2E, ReassignedBoundComputesCorrectly)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    TransferRegistry reg;
    registerStandardTransfers(reg);
    numkit::Lexer  lex("function y = f(x)\n  n = numel(x);\n  n = 3;\n  y = zeros(1, n);\n"
                       "  for k = 1:n\n    y(k) = x(k);\n  end\nend\n");
    numkit::Parser parser(lex.tokenize());
    auto           root = parser.parse();
    const numkit::ASTNode *fn = nullptr;
    for (const auto &c : root->children)
        if (c && c->type == numkit::NodeType::FUNCTION_DEF) fn = c.get();
    ASSERT_NE(fn, nullptr);
    const EmittedFunction emitted =
        emitFunction(*fn, {{"x", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}},
                     reg);
    // must be the checked form (stale fact -> no promotion)
    ASSERT_TRUE(emitted.source.find("for (std::size_t k = 0;") == std::string::npos);

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_reassign_e2e.exe").string();
    const std::string outTxt = (base / "nk_reassign_e2e_out.txt").string();
    std::error_code   ec;
    std::filesystem::remove(outTxt, ec);

    std::string program = emitted.source;
    program +=
        "#include <cstdio>\n"
        "int main() {\n"
        "  double x[8]; double y[3];\n"
        "  for (std::size_t i = 0; i < 8; ++i) x[i] = std::sin(0.01 * double(i + 1));\n"
        "  f(x, 8, y, 3);\n"  // _nk_x_len=8, _nk_y_len=3; loop must run 3 (n), not 8
        "  std::FILE* g = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!g) return 2;\n"
        "  for (std::size_t i = 0; i < 3; ++i) std::fprintf(g, \"%.17g\\n\", y[i]);\n"
        "  std::fclose(g); return 0;\n"
        "}\n";

    const auto r = aot::compileToExecutable(program, exe);
    ASSERT_EQ(r.status, aot::CompileStatus::Ok)
        << "log:\n" << r.log << "\n--- generated source ---\n" << program;
    ASSERT_EQ(std::system(("\"" + exe + "\"").c_str()), 0);

    std::vector<double> got;
    {
        std::ifstream is(outTxt);
        double        v;
        while (is >> v) got.push_back(v);
    }
    ASSERT_EQ(got.size(), 3u);
    for (std::size_t i = 0; i < 3; ++i)
        EXPECT_NEAR(got[i], std::sin(0.01 * double(i + 1)), 1e-12) << "at i=" << i;
}

// Control flow run end-to-end (previously only string-tested): an if/else
// function, both branches exercised.
TEST(CodegenE2E, IfElseRunsCorrectly)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const EmittedFunction emitted = transpile(
        "function y = pick(c, a, b)\n  if c\n    y = a;\n  else\n    y = b;\n  end\nend\n",
        {{"c", InferredType::scalar(ValueType::DOUBLE)},
         {"a", InferredType::scalar(ValueType::DOUBLE)},
         {"b", InferredType::scalar(ValueType::DOUBLE)}});
    ASSERT_EQ(emitted.signature, "double pick(double c, double a, double b)");

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_if_e2e.exe").string();
    const std::string outTxt = (base / "nk_if_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double r1 = pick(1.0, 7.0, 9.0);\n"
        "  double r0 = pick(0.0, 7.0, 9.0);\n"
        "  std::FILE* g = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!g) return 2;\n"
        "  std::fprintf(g, \"%.17g\\n%.17g\\n\", r1, r0);\n"
        "  std::fclose(g); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 2u);
    EXPECT_DOUBLE_EQ(got[0], 7.0);  // c true  -> a
    EXPECT_DOUBLE_EQ(got[1], 9.0);  // c false -> b
}

// A while loop run end-to-end, including the 0-iteration case (fixpoint /
// loop-carried typing).
TEST(CodegenE2E, WhileLoopRunsCorrectly)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const EmittedFunction emitted = transpile(
        "function y = acc(n)\n  y = 0;\n  i = 1;\n  while i <= n\n    y = y + i;\n"
        "    i = i + 1;\n  end\nend\n",
        {{"n", InferredType::scalar(ValueType::DOUBLE)}});
    ASSERT_EQ(emitted.signature, "double acc(double n)");

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_while_e2e.exe").string();
    const std::string outTxt = (base / "nk_while_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double s10 = acc(10.0);\n"
        "  double s0  = acc(0.0);\n"  // loop body never runs
        "  std::FILE* g = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!g) return 2;\n"
        "  std::fprintf(g, \"%.17g\\n%.17g\\n\", s10, s0);\n"
        "  std::fclose(g); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 2u);
    EXPECT_DOUBLE_EQ(got[0], 55.0);  // 1+2+...+10
    EXPECT_DOUBLE_EQ(got[1], 0.0);   // 0 iterations
}

// Class brick 6/7: a value class with a METHOD, end-to-end. The harness
// builds a Rect in C++, the transpiled run() calls obj.area() (a
// monomorphic method call -> Rect__area(obj)), and the result is checked.
TEST(CodegenE2E, ValueClassMethodCall)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const char *src =
        "classdef Rect\n  properties\n    w = 0\n    h = 0\n  end\n"
        "  methods\n    function a = area(obj)\n      a = obj.w * obj.h;\n    end\n  end\nend\n"
        "function y = run(p)\n  y = p.area();\nend\n";
    numkit::Lexer  lex(src);
    numkit::Parser parser(lex.tokenize());
    auto           root = parser.parse();
    TransferRegistry reg;
    registerStandardTransfers(reg);
    ClassRegistry creg;
    collectClasses(*root, creg, reg);
    FunctionTable ft;
    collectFunctions(*root, ft);
    registerUserFunctions(reg, ft);
    registerClassMethods(reg, creg);

    const int id = creg.idOf("Rect");
    ASSERT_GE(id, 0);
    const EmittedFunction emitted =
        emitProgram(*ft.find("run"), {{"p", InferredType::object(id)}}, ft, reg, &creg);

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_method_e2e.exe").string();
    const std::string outTxt = (base / "nk_method_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  Rect p{};\n"
        "  p.w = 3.0;\n"
        "  p.h = 4.0;\n"
        "  double r = " + emitted.name + "(p);\n"
        "  std::FILE* g = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!g) return 2;\n"
        "  std::fprintf(g, \"%.17g\\n\", r);\n"
        "  std::fclose(g); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 12.0);  // area = w * h = 3 * 4
}

// Boundary #1a: a SELF-CONTAINED class program — the object is constructed,
// mutated, and used entirely in numkit code (no C++-side construction). The
// harness only calls the entry. Default construction Rect() -> Rect{}.
TEST(CodegenE2E, SelfContainedClassProgram)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const char *src =
        "classdef Rect\n  properties\n    w = 0\n    h = 0\n  end\n"
        "  methods\n    function a = area(o)\n      a = o.w * o.h;\n    end\n  end\nend\n"
        "function y = demo()\n  r = Rect();\n  r.w = 3;\n  r.h = 4;\n  y = r.area();\nend\n";
    numkit::Lexer  lex(src);
    numkit::Parser parser(lex.tokenize());
    auto           root = parser.parse();
    TransferRegistry reg;
    registerStandardTransfers(reg);
    ClassRegistry creg;
    collectClasses(*root, creg, reg);
    FunctionTable ft;
    collectFunctions(*root, ft);
    registerUserFunctions(reg, ft);
    registerClassMethods(reg, creg);
    registerClassConstructors(reg, creg);

    const EmittedFunction emitted = emitProgram(*ft.find("demo"), {}, ft, reg, &creg);

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_selfclass_e2e.exe").string();
    const std::string outTxt = (base / "nk_selfclass_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double r = " + emitted.name + "();\n"
        "  std::FILE* g = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!g) return 2;\n"
        "  std::fprintf(g, \"%.17g\\n\", r);\n"
        "  std::fclose(g); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 12.0);  // Rect() default, w=3 h=4, area = 12
}

// 2-D indexing: a matrix param A(i,j), column-major, read end-to-end.
// tr(A) sums the 3x3 diagonal.
TEST(CodegenE2E, Matrix2DRead)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const char *src = "function s = tr(A)\n  s = A(1,1) + A(2,2) + A(3,3);\nend\n";
    numkit::Lexer  lex(src);
    numkit::Parser parser(lex.tokenize());
    auto           root = parser.parse();
    TransferRegistry reg;
    registerStandardTransfers(reg);
    FunctionTable ft;
    collectFunctions(*root, ft);
    registerUserFunctions(reg, ft);

    const EmittedFunction emitted = emitProgram(
        *ft.find("tr"), {{"A", InferredType::concrete(ValueType::DOUBLE, Shape::dims(3, 3))}},
        ft, reg);
    ASSERT_TRUE(emitted.source.find("std::size_t _nk_A_rows, std::size_t _nk_A_cols")
                != std::string::npos);

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_mat2d_e2e.exe").string();
    const std::string outTxt = (base / "nk_mat2d_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double A[9] = {1,0,0, 0,2,0, 0,0,3};\n"  // column-major; diagonal 1,2,3
        "  double r = " + emitted.name + "(A, 3, 3);\n"
        "  std::FILE* g = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!g) return 2;\n"
        "  std::fprintf(g, \"%.17g\\n\", r);\n"
        "  std::fclose(g); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 6.0);  // trace = 1 + 2 + 3
}

// INTERPROC ARRAY RETURN (typed): a compiled callee g returns a 1-D array. The
// callee returns it BY VALUE as an owned std::vector<double> (self-describing
// size — no out-size protocol; the ENTRY keeps the out-param ABI). The caller
// binds it to an array LOCAL and indexes it natively. Self-contained (no bridge).
TEST(CodegenE2E, InterprocArrayReturn)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const char *src =
        "function s = f(n)\n"
        "  v = g(n);\n"   // g returns a 1-D array by value -> v is a local std::vector
        "  s = v(2);\n"   // native index of the returned vector
        "end\n"
        "function r = g(n)\n"
        "  r = zeros(1, n);\n"
        "  for k = 1:n\n"
        "    r(k) = k * k;\n"
        "  end\n"
        "end\n";
    numkit::Lexer  lex(src);
    numkit::Parser parser(lex.tokenize());
    auto           root = parser.parse();
    TransferRegistry reg;
    registerStandardTransfers(reg);
    FunctionTable ft;
    collectFunctions(*root, ft);
    registerUserFunctions(reg, ft);

    const EmittedFunction emitted =
        emitProgram(*ft.find("f"), {{"n", InferredType::scalar(ValueType::DOUBLE)}}, ft, reg);
    ASSERT_TRUE(emitted.source.find("std::vector<double> ") != std::string::npos)
        << "expected the callee to return a 1-D array by value (std::vector)";

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_iparr_e2e.exe").string();
    const std::string outTxt = (base / "nk_iparr_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double s = " + emitted.name + "(4.0);\n"  // g(4) = [1 4 9 16]; v(2) = 4
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", s);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 4.0);  // g(4) = [1 4 9 16]; v(2) = 4
}

// INTERPROC ARRAY RETURN into the program OUTPUT (P1.5): f's 1-D array output is
// produced directly by an interproc call `v = g(n)`. The callee returns a
// self-describing std::vector; the body copies it into the caller-allocated
// out-param buffer (bounded by the caller's _len — the size contract the
// zeros-fill output already trusts). End-to-end.
TEST(CodegenE2E, InterprocArrayReturnIntoOutput)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const char *src =
        "function v = f(n)\n"
        "  v = g(n);\n"   // the OUTPUT array is filled directly by the interproc result
        "end\n"
        "function r = g(n)\n"
        "  r = zeros(1, n);\n"
        "  for k = 1:n\n"
        "    r(k) = k * k;\n"
        "  end\n"
        "end\n";
    numkit::Lexer  lex(src);
    numkit::Parser parser(lex.tokenize());
    auto           root = parser.parse();
    TransferRegistry reg;
    registerStandardTransfers(reg);
    FunctionTable ft;
    collectFunctions(*root, ft);
    registerUserFunctions(reg, ft);

    const EmittedFunction emitted =
        emitProgram(*ft.find("f"), {{"n", InferredType::scalar(ValueType::DOUBLE)}}, ft, reg);
    ASSERT_NE(emitted.source.find("_nk_ret"), std::string::npos)
        << "expected the interproc-result -> output-buffer copy path";

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_iparr_out_e2e.exe").string();
    const std::string outTxt = (base / "nk_iparr_out_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  const std::size_t N = 4;\n"
        "  double v[4] = {0, 0, 0, 0};\n"
        "  " + emitted.name + "(4.0, v, N);\n"  // g(4) = [1 4 9 16]
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  for (std::size_t i = 0; i < N; ++i) std::fprintf(h, \"%.17g\\n\", v[i]);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 4u);
    EXPECT_DOUBLE_EQ(got[0], 1.0);
    EXPECT_DOUBLE_EQ(got[1], 4.0);
    EXPECT_DOUBLE_EQ(got[2], 9.0);
    EXPECT_DOUBLE_EQ(got[3], 16.0);
}

// MULTI-OUTPUT with a leading 1-D array (P2.1): `[v, s] = f(n)` where v is a 1-D
// array and s a scalar. The callee returns v BY VALUE (std::vector) and writes s
// through a reference out-param; the caller binds v from the return and s from
// the reference. Routed through a caller g so the call-site binding
// (emitMultiAssign) is exercised too. End-to-end.
TEST(CodegenE2E, MultiOutputLeadingArray)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const char *src =
        "function out = g(n)\n"
        "  [v, s] = f(n);\n"   // v: 1-D array (by-value return); s: scalar (ref out-param)
        "  out = v(2) + s;\n"  // v(2) = 4, s = n+1 = 5 -> 9 for n = 4
        "end\n"
        "function [v, s] = f(n)\n"
        "  v = zeros(1, n);\n"
        "  for k = 1:n\n"
        "    v(k) = k * k;\n"
        "  end\n"
        "  s = n + 1;\n"
        "end\n";
    numkit::Lexer  lex(src);
    numkit::Parser parser(lex.tokenize());
    auto           root = parser.parse();
    TransferRegistry reg;
    registerStandardTransfers(reg);
    FunctionTable ft;
    collectFunctions(*root, ft);
    registerUserFunctions(reg, ft);

    const EmittedFunction emitted =
        emitProgram(*ft.find("g"), {{"n", InferredType::scalar(ValueType::DOUBLE)}}, ft, reg);
    ASSERT_NE(emitted.source.find("std::vector<double> "), std::string::npos)
        << "expected the callee to return its leading 1-D array output by value";

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_multiout_arr_e2e.exe").string();
    const std::string outTxt = (base / "nk_multiout_arr_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double out = " + emitted.name + "(4.0);\n"
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", out);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 9.0);  // v(2) = 4 + s = 5
}

// Interproc by-value return of a 2-D KnownDims matrix (P2.2): a callee returning a
// compile-time-sized matrix returns it as a flat std::vector (column-major); the
// caller binds it to a 2-D local and indexes it. Dims are compile-time-known on
// both sides (monomorphic) -> no runtime dims travel with the buffer. End-to-end.
TEST(CodegenE2E, InterprocReturn2DKnownDims)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const char *src =
        "function out = g(r)\n"
        "  M = f();\n"        // M: 2x2 matrix returned BY VALUE -> a 2-D local
        "  out = M(r, 2);\n"  // column-major M = [1 3; 2 4]; M(2,2) = 4
        "end\n"
        "function M = f()\n"
        "  M = zeros(2, 2);\n"
        "  M(1,1) = 1; M(2,1) = 2; M(1,2) = 3; M(2,2) = 4;\n"
        "end\n";
    numkit::Lexer  lex(src);
    numkit::Parser parser(lex.tokenize());
    auto           root = parser.parse();
    TransferRegistry reg;
    registerStandardTransfers(reg);
    FunctionTable ft;
    collectFunctions(*root, ft);
    registerUserFunctions(reg, ft);

    const EmittedFunction emitted =
        emitProgram(*ft.find("g"), {{"r", InferredType::scalar(ValueType::DOUBLE)}}, ft, reg);
    ASSERT_NE(emitted.source.find("std::vector<double> "), std::string::npos)
        << "expected the callee to return its 2-D matrix output by value";

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_iret2d_e2e.exe").string();
    const std::string outTxt = (base / "nk_iret2d_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double out = " + emitted.name + "(2.0);\n"  // M(2,2) = 4
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", out);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 4.0);
}

// MULTI-OUTPUT with a leading 2-D array (P2.3): `[M, s] = f()` where M is a 2-D
// KnownDims matrix and s a scalar. The callee returns M BY VALUE (flat
// std::vector) and writes s through a reference out-param; the caller binds M from
// the return (a 2-D local) and s from the reference. Routed through a caller g.
TEST(CodegenE2E, MultiOutputLeading2DArray)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const char *src =
        "function out = g(r)\n"
        "  [M, s] = f();\n"        // M: 2x2 matrix (by-value); s: scalar (ref out-param)
        "  out = M(r, 2) + s;\n"   // M(2,2) = 4, s = 10 -> 14 for r = 2
        "end\n"
        "function [M, s] = f()\n"
        "  M = zeros(2, 2);\n"
        "  M(1,1) = 1; M(2,1) = 2; M(1,2) = 3; M(2,2) = 4;\n"
        "  s = 10;\n"
        "end\n";
    numkit::Lexer  lex(src);
    numkit::Parser parser(lex.tokenize());
    auto           root = parser.parse();
    TransferRegistry reg;
    registerStandardTransfers(reg);
    FunctionTable ft;
    collectFunctions(*root, ft);
    registerUserFunctions(reg, ft);

    const EmittedFunction emitted =
        emitProgram(*ft.find("g"), {{"r", InferredType::scalar(ValueType::DOUBLE)}}, ft, reg);
    ASSERT_NE(emitted.source.find("std::vector<double> "), std::string::npos)
        << "expected the callee to return its leading 2-D array output by value";

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_multiout_2d_e2e.exe").string();
    const std::string outTxt = (base / "nk_multiout_2d_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double out = " + emitted.name + "(2.0);\n"  // M(2,2)=4 + s=10
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", out);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 14.0);  // M(2,2) = 4 + s = 10
}

// break / continue lower directly to C++ (control-flow coverage). A loop that
// `continue`s past one index and `break`s out early must produce MATLAB's result.
TEST(CodegenE2E, BreakAndContinueInLoop)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const EmittedFunction emitted = transpile(
        "function y = f(n)\n"
        "  y = 0;\n"
        "  for k = 1:n\n"
        "    if k == 2\n"
        "      continue;\n"   // skip k = 2
        "    end\n"
        "    if k > 4\n"
        "      break;\n"      // stop once k exceeds 4
        "    end\n"
        "    y = y + k;\n"
        "  end\n"
        "end\n",
        {{"n", InferredType::scalar(ValueType::DOUBLE)}});
    EXPECT_NE(emitted.source.find("break;"), std::string::npos);
    EXPECT_NE(emitted.source.find("continue;"), std::string::npos);

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_brkcont_e2e.exe").string();
    const std::string outTxt = (base / "nk_brkcont_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double y = f(10.0);\n"  // 1 + 3 + 4 = 8 (skip 2; break once k>4)
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", y);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 8.0);
}

// Early `return` (control-flow coverage): a conditional return mid-function plus
// the normal fall-through trailing return. Both must produce MATLAB's result.
TEST(CodegenE2E, EarlyReturnStatement)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const EmittedFunction emitted = transpile(
        "function y = f(n)\n"
        "  y = 0;\n"
        "  for k = 1:n\n"
        "    y = y + k;\n"
        "    if y > 10\n"
        "      return;\n"   // early return once the running sum exceeds 10
        "    end\n"
        "  end\n"
        "  y = -1;\n"       // only reached if the loop completes without returning
        "end\n",
        {{"n", InferredType::scalar(ValueType::DOUBLE)}});
    EXPECT_NE(emitted.source.find("return y;"), std::string::npos);

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_earlyret_e2e.exe").string();
    const std::string outTxt = (base / "nk_earlyret_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  const double a = f(100.0);\n"  // 1+2+3+4+5 = 15 (>10 at k=5 -> early return)
        "  const double b = f(3.0);\n"    // 1+2+3 = 6, never >10 -> fall through -> -1
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n%.17g\\n\", a, b);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 2u);
    EXPECT_DOUBLE_EQ(got[0], 15.0);  // early return
    EXPECT_DOUBLE_EQ(got[1], -1.0);  // fall-through trailing return
}

// switch / case / otherwise (control-flow coverage) lowers to an if-else chain
// over a selector temp; a `case {a,b}` cell-list matches any element.
TEST(CodegenE2E, SwitchStatement)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const EmittedFunction emitted = transpile(
        "function y = f(k)\n"
        "  switch k\n"
        "    case 1\n"
        "      y = 10;\n"
        "    case {2, 3}\n"     // cell-list: matches 2 or 3
        "      y = 20;\n"
        "    otherwise\n"
        "      y = 99;\n"
        "  end\n"
        "end\n",
        {{"k", InferredType::scalar(ValueType::DOUBLE)}});
    EXPECT_NE(emitted.source.find("_nk_switch0"), std::string::npos);

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_switch_e2e.exe").string();
    const std::string outTxt = (base / "nk_switch_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  const double a = f(1.0), b = f(2.0), c = f(3.0), d = f(5.0);\n"
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n%.17g\\n%.17g\\n%.17g\\n\", a, b, c, d);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 4u);
    EXPECT_DOUBLE_EQ(got[0], 10.0);  // case 1
    EXPECT_DOUBLE_EQ(got[1], 20.0);  // case {2,3}
    EXPECT_DOUBLE_EQ(got[2], 20.0);  // case {2,3}
    EXPECT_DOUBLE_EQ(got[3], 99.0);  // otherwise
}

// LOGICAL array from an elementwise comparison (P3 datatypes): `m = x > 2.5`
// produces a LOGICAL buffer; it is stored as std::vector<std::uint8_t> (not
// vector<bool>, which has no .data()). Reading its elements (clean loop-counter
// index) and accumulating counts the trues. End-to-end.
TEST(CodegenE2E, LogicalArrayFromComparison)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const EmittedFunction emitted = transpile(
        "function s = f(x)\n"
        "  m = x > 2.5;\n"      // LOGICAL array -> uint8 storage
        "  s = 0;\n"
        "  for k = 1:numel(x)\n"
        "    s = s + m(k);\n"   // read logical elements, count the trues
        "  end\n"
        "end\n",
        {{"x", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}});
    EXPECT_NE(emitted.source.find("std::vector<std::uint8_t>"), std::string::npos)
        << "a LOGICAL array local must use uint8 storage (not vector<bool>)";

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_logarr_e2e.exe").string();
    const std::string outTxt = (base / "nk_logarr_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double x[4] = {1.0, 2.0, 3.0, 4.0};\n"  // x > 2.5 -> [0 0 1 1], count = 2
        "  double s = f(x, 4);\n"
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", s);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 2.0);  // count of x > 2.5 in [1 2 3 4]
}

// LOGICAL-INDEXING READ `v = x(m)` (P3 datatypes): build a runtime-sized result
// by filtering x by the mask m. The result is a 1-D array LOCAL grown via
// push_back. End-to-end.
TEST(CodegenE2E, LogicalIndexingRead)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const EmittedFunction emitted = transpile(
        "function s = f(x)\n"
        "  m = x > 2.5;\n"        // mask: [0 0 1 1] for [1 2 3 4]
        "  v = x(m);\n"           // logical indexing -> [3 4]
        "  s = v(1) + v(2);\n"    // 3 + 4 = 7
        "end\n",
        {{"x", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}});
    EXPECT_NE(emitted.source.find(".push_back("), std::string::npos)
        << "logical indexing must build the result by filtering (push_back)";

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_logidx_e2e.exe").string();
    const std::string outTxt = (base / "nk_logidx_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double x[4] = {1.0, 2.0, 3.0, 4.0};\n"  // x(x>2.5) = [3 4]
        "  double s = f(x, 4);\n"
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", s);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 7.0);  // v(1)+v(2) = 3+4
}

// LOGICAL-INDEXING WRITE `y(m) = c` (P3 datatypes): scatter a scalar into the
// masked positions (MATLAB clamp idiom x(x<0)=0). End-to-end.
TEST(CodegenE2E, LogicalIndexingWrite)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const EmittedFunction emitted = transpile(
        "function y = f(x)\n"
        "  y = x;\n"          // copy into the writable output buffer
        "  m = y < 2.5;\n"    // mask: [1 1 0 0] for [1 2 3 4]
        "  y(m) = 0;\n"       // clamp masked elements to 0
        "end\n",
        {{"x", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}});
    EXPECT_NE(emitted.source.find("] = _nk_c;"), std::string::npos)
        << "logical-indexing write must scatter the scalar into the masked positions";

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_logidxw_e2e.exe").string();
    const std::string outTxt = (base / "nk_logidxw_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double x[4] = {1.0, 2.0, 3.0, 4.0};\n"
        "  double y[4] = {0, 0, 0, 0};\n"
        "  f(x, 4, y, 4);\n"  // y -> [0 0 3 4]
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  for (int i = 0; i < 4; ++i) std::fprintf(h, \"%.17g\\n\", y[i]);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 4u);
    EXPECT_DOUBLE_EQ(got[0], 0.0);  // 1 < 2.5 -> clamped
    EXPECT_DOUBLE_EQ(got[1], 0.0);  // 2 < 2.5 -> clamped
    EXPECT_DOUBLE_EQ(got[2], 3.0);  // 3 >= 2.5 -> kept
    EXPECT_DOUBLE_EQ(got[3], 4.0);  // 4 >= 2.5 -> kept
}

// Native any / all reductions (P3 — mask consumption, self-contained): an inline
// short-circuit loop, no runtime bridge. any([0 0 1 1])=true, all([0 0 1 1])=false.
TEST(CodegenE2E, NativeAnyAllReductions)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const EmittedFunction emitted = transpile(
        "function s = f(x)\n"
        "  m = x > 2.5;\n"
        "  a = any(m);\n"      // true  for [0 0 1 1]
        "  b = all(m);\n"      // false for [0 0 1 1]
        "  s = a * 10 + b;\n"  // 1*10 + 0 = 10
        "end\n",
        {{"x", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}});
    EXPECT_NE(emitted.source.find("_nk_acc"), std::string::npos)
        << "any/all must lower to an inline reduction loop (no bridge)";

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_anyall_e2e.exe").string();
    const std::string outTxt = (base / "nk_anyall_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double x[4] = {1.0, 2.0, 3.0, 4.0};\n"  // x > 2.5 -> [0 0 1 1]
        "  double s = f(x, 4);\n"
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", s);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 10.0);  // any=1 -> *10; all=0
}

// Native min / max reductions (P3, self-contained, NaN-skipping): an inline fold,
// no runtime bridge. range = max(x) - min(x).
TEST(CodegenE2E, NativeMinMaxReductions)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const EmittedFunction emitted = transpile(
        "function s = f(x)\n"
        "  a = max(x);\n"   // 5
        "  b = min(x);\n"   // 1
        "  s = a - b;\n"    // 4
        "end\n",
        {{"x", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}});
    EXPECT_NE(emitted.source.find("_nk_acc != _nk_acc"), std::string::npos)
        << "native min/max must NaN-skip (acc != acc) for a float dtype";

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_minmax_e2e.exe").string();
    const std::string outTxt = (base / "nk_minmax_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double x[5] = {3.0, 1.0, 4.0, 1.0, 5.0};\n"  // max 5, min 1
        "  double s = f(x, 5);\n"
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", s);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 4.0);  // max(5) - min(1)
}

// Native sum reduction (P3, self-contained / no bridge): an inline accumulation
// loop. Integer values sum exactly (no rounding) so this matches regardless of
// summation order.
TEST(CodegenE2E, NativeSumReduction)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const EmittedFunction emitted = transpile(  // transpile() emits with NO bridge
        "function s = f(x)\n"
        "  s = sum(x);\n"
        "end\n",
        {{"x", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}});
    EXPECT_NE(emitted.source.find("_nk_acc += "), std::string::npos)
        << "native sum must lower to an inline accumulation loop (no bridge)";

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_sum_e2e.exe").string();
    const std::string outTxt = (base / "nk_sum_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double x[5] = {1.0, 2.0, 3.0, 4.0, 5.0};\n"  // sum = 15
        "  double s = f(x, 5);\n"
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", s);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 15.0);  // 1+2+3+4+5
}

// Native prod / mean reductions (P3, self-contained / no bridge): inline loops,
// same !bridge_ tier as sum. Integer-exact values so order doesn't matter.
TEST(CodegenE2E, NativeProdMeanReductions)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const EmittedFunction emitted = transpile(  // transpile() emits with NO bridge
        "function s = f(x)\n"
        "  p = prod(x);\n"   // 1*2*3*4 = 24
        "  m = mean(x);\n"   // (1+2+3+4)/4 = 2.5
        "  s = p + m;\n"     // 26.5
        "end\n",
        {{"x", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}});
    EXPECT_NE(emitted.source.find("_nk_acc *= "), std::string::npos)
        << "native prod must lower to an inline product loop";

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_prodmean_e2e.exe").string();
    const std::string outTxt = (base / "nk_prodmean_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double x[4] = {1.0, 2.0, 3.0, 4.0};\n"  // prod 24, mean 2.5
        "  double s = f(x, 4);\n"
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", s);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 26.5);  // prod(24) + mean(2.5)
}

// find(mask) -> 1-based positions of the true elements (P3, self-contained): a
// native filter loop pushing (i+1) into a runtime-sized 1-D array local.
TEST(CodegenE2E, FindReturnsPositions)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const EmittedFunction emitted = transpile(
        "function s = f(x)\n"
        "  m = x > 2.5;\n"
        "  idx = find(m);\n"        // positions of x > 2.5 -> [3 4] for [1 2 3 4]
        "  s = idx(1) + idx(2);\n"  // 3 + 4 = 7
        "end\n",
        {{"x", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}});
    EXPECT_NE(emitted.source.find("_nk_i + 1"), std::string::npos)
        << "find must push 1-based positions";

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_find_e2e.exe").string();
    const std::string outTxt = (base / "nk_find_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double x[4] = {1.0, 2.0, 3.0, 4.0};\n"  // find(x>2.5) = [3 4]
        "  double s = f(x, 4);\n"
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", s);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 7.0);  // positions 3 + 4
}

// Native diff(x) -> consecutive differences (P3, self-contained, EXACT): a
// push_back loop producing a length n-1 1-D array local.
TEST(CodegenE2E, NativeDiffReduction)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const EmittedFunction emitted = transpile(
        "function s = f(x)\n"
        "  d = diff(x);\n"          // [1 4 9 16] -> [3 5 7]
        "  s = d(1) + d(2) + d(3);\n"  // 3+5+7 = 15
        "end\n",
        {{"x", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}});
    EXPECT_NE(emitted.source.find("[_nk_i] - "), std::string::npos)
        << "diff must lower to a consecutive-difference loop";

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_diff_e2e.exe").string();
    const std::string outTxt = (base / "nk_diff_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double x[4] = {1.0, 4.0, 9.0, 16.0};\n"  // diff -> [3 5 7]
        "  double s = f(x, 4);\n"
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", s);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 15.0);  // (4-1)+(9-4)+(16-9) = 3+5+7
}

// Native cumsum / cumprod (P3, self-contained / no bridge): running accumulation,
// same-length result. Integer-exact values so order doesn't matter.
TEST(CodegenE2E, NativeCumsumCumprod)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const EmittedFunction emitted = transpile(  // transpile() emits with NO bridge
        "function s = f(x)\n"
        "  c = cumsum(x);\n"        // [1 2 3 4] -> [1 3 6 10]
        "  p = cumprod(x);\n"       // [1 2 3 4] -> [1 2 6 24]
        "  s = c(4) + p(4);\n"      // 10 + 24 = 34
        "end\n",
        {{"x", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}});
    EXPECT_NE(emitted.source.find(".push_back(_nk_acc)"), std::string::npos)
        << "cumsum/cumprod must push the running accumulator";

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_cum_e2e.exe").string();
    const std::string outTxt = (base / "nk_cum_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double x[4] = {1.0, 2.0, 3.0, 4.0};\n"  // cumsum(4)=10, cumprod(4)=24
        "  double s = f(x, 4);\n"
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", s);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 34.0);  // cumsum[4]=10 + cumprod[4]=24
}

// Native dot(a,b) -> inner product (P3, self-contained / no bridge): an
// accumulation loop with a length-match guard.
TEST(CodegenE2E, NativeDotProduct)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const EmittedFunction emitted = transpile(  // transpile() emits with NO bridge
        "function s = f(a, b)\n"
        "  s = dot(a, b);\n"   // 1*4 + 2*5 + 3*6 = 32
        "end\n",
        {{"a", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())},
         {"b", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}});
    EXPECT_NE(emitted.source.find("_nk_acc += "), std::string::npos)
        << "dot must lower to an accumulation loop";

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_dot_e2e.exe").string();
    const std::string outTxt = (base / "nk_dot_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double a[3] = {1.0, 2.0, 3.0};\n"
        "  double b[3] = {4.0, 5.0, 6.0};\n"  // dot = 4+10+18 = 32
        "  double s = f(a, 3, b, 3);\n"
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", s);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 32.0);  // 1*4 + 2*5 + 3*6
}

// CHAR row-vector literal (P3 datatypes, first char brick): `c = 'abc'` becomes a
// uint16 char-array local; numel reads its length. Foundational char support.
TEST(CodegenE2E, CharRowLiteral)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const EmittedFunction emitted = transpile(
        "function n = f()\n"
        "  c = 'abcde';\n"   // 5-char char array
        "  n = numel(c);\n"  // 5
        "end\n",
        {});
    EXPECT_NE(emitted.source.find("std::vector<std::uint16_t>"), std::string::npos)
        << "a CHAR array must be stored as a uint16 buffer";

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_char_e2e.exe").string();
    const std::string outTxt = (base / "nk_char_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double n = f();\n"  // numel('abcde') = 5
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", n);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 5.0);  // numel of 'abcde'
}

// CHAR scalar + comparison (P3, 2nd char brick): a 1x1 char literal is a uint16
// code-unit scalar; reading a char element and comparing it enables char search.
TEST(CodegenE2E, CharScalarCompareCount)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const EmittedFunction emitted = transpile(
        "function n = f()\n"
        "  c = 'banana';\n"
        "  n = 0;\n"
        "  for k = 1:numel(c)\n"
        "    if c(k) == 'a'\n"   // char element vs char scalar literal
        "      n = n + 1;\n"
        "    end\n"
        "  end\n"
        "end\n",
        {});
    EXPECT_NE(emitted.source.find("std::uint16_t(97)"), std::string::npos)
        << "a 1-char literal 'a' must emit its uint16 code unit (97)";

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_charcmp_e2e.exe").string();
    const std::string outTxt = (base / "nk_charcmp_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double n = f();\n"  // count of 'a' in "banana" = 3
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", n);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 3.0);  // 'a' appears 3 times in 'banana'
}

// CHAR as a function ARG + RETURN + checked index (P3, 3rd char brick): a char
// array crosses the param ABI (const uint16_t*); a checked index (templated
// nk_rt::index) reads a char element; the char scalar is returned (uint16). All
// dtype-generic -> this confirms char interop end-to-end.
TEST(CodegenE2E, CharArgIndexReturn)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const EmittedFunction emitted = transpile(
        "function ch = f(s, k)\n"
        "  ch = s(k);\n"   // char element at position k (checked index, runtime k)
        "end\n",
        {{"s", InferredType::concrete(ValueType::CHAR, Shape::rowVector())},
         {"k", InferredType::scalar(ValueType::DOUBLE)}});
    EXPECT_NE(emitted.source.find("std::uint16_t"), std::string::npos)
        << "a char arg/return must use the uint16 code-unit type";

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_charg_e2e.exe").string();
    const std::string outTxt = (base / "nk_charg_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  std::uint16_t s[5] = {104, 101, 108, 108, 111};\n"  // 'hello'
        "  std::uint16_t r = f(s, 5, 2.0);\n"                  // s(2) = 'e' = 101
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", double(r));\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 101.0);  // 'e' in 'hello' at index 2
}

// Native strcmp (P3, 4th char brick): string equality via length + elementwise
// compare. Tests both an equal pair and a same-length-but-different pair.
TEST(CodegenE2E, NativeStrcmp)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const EmittedFunction emitted = transpile(
        "function r = f()\n"
        "  a = 'hello';\n"
        "  b = 'hello';\n"
        "  c = 'world';\n"
        "  e1 = strcmp(a, b);\n"   // equal -> 1
        "  e2 = strcmp(a, c);\n"   // same length, differ -> 0
        "  r = e1 * 10 + e2;\n"    // 10
        "end\n",
        {});
    EXPECT_NE(emitted.source.find("_nk_eq"), std::string::npos)
        << "strcmp must lower to a length + elementwise equality check";

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_strcmp_e2e.exe").string();
    const std::string outTxt = (base / "nk_strcmp_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double r = f();\n"  // strcmp(hello,hello)=1 -> *10; strcmp(hello,world)=0
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", r);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 10.0);  // equal=1*10 + differ=0
}

// Plain struct via field-flattening (P3, first struct brick): `s.a = ...` becomes
// a synthesized scalar field-local (_nk_fld_s_a); static field reads/writes work
// without any struct type. v1: scalar fields, a plain-identifier base.
TEST(CodegenE2E, StructScalarFields)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const EmittedFunction emitted = transpile(
        "function r = f()\n"
        "  s.a = 2;\n"
        "  s.b = 3;\n"
        "  r = s.a * 10 + s.b;\n"  // 2*10 + 3 = 23
        "end\n",
        {});
    EXPECT_NE(emitted.source.find("_nk_fld_s_a"), std::string::npos)
        << "a plain-struct field must flatten to a synthesized scalar local";

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_struct_e2e.exe").string();
    const std::string outTxt = (base / "nk_struct_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double r = f();\n"  // s.a*10 + s.b = 23
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", r);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 23.0);  // 2*10 + 3
}

// 1-D horzcat `c = [a b]` (P3): concatenate two array operands into a runtime-
// sized 1-D local; tests the joined length and element ordering.
TEST(CodegenE2E, HorzcatTwoArrays)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const EmittedFunction emitted = transpile(
        "function r = f(a, b)\n"
        "  c = [a b];\n"             // [1 2 3] ++ [4 5] -> [1 2 3 4 5]
        "  r = numel(c) + c(4);\n"   // 5 + 4 = 9
        "end\n",
        {{"a", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())},
         {"b", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}});
    EXPECT_NE(emitted.source.find(".push_back("), std::string::npos)
        << "horzcat must build the result by appending each operand";

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_horzcat_e2e.exe").string();
    const std::string outTxt = (base / "nk_horzcat_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double a[3] = {1.0, 2.0, 3.0};\n"
        "  double b[2] = {4.0, 5.0};\n"  // [a b] = [1 2 3 4 5]
        "  double r = f(a, 3, b, 2);\n"
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", r);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 9.0);  // numel(5) + c(4)=4
}

// Concat with a string literal (P3, char string-building): ['Hi' s] prepends a
// char literal to a char var. Exercises a STRING_LITERAL operand in horzcat.
TEST(CodegenE2E, ConcatStringLiteral)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const EmittedFunction emitted = transpile(
        "function r = f(s)\n"
        "  c = ['Hi' s];\n"            // 'Hi' (2) ++ s
        "  r = numel(c) * 100 + c(1);\n"  // len*100 + 'H'
        "end\n",
        {{"s", InferredType::concrete(ValueType::CHAR, Shape::rowVector())}});
    EXPECT_NE(emitted.source.find("std::uint16_t(72)"), std::string::npos)
        << "a char literal operand must append its code units ('H' = 72)";

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_concatlit_e2e.exe").string();
    const std::string outTxt = (base / "nk_concatlit_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  std::uint16_t s[3] = {120, 121, 122};\n"  // 'xyz' -> c = 'Hixyz' (5)
        "  double r = f(s, 3);\n"
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", r);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 572.0);  // numel(5)*100 + c(1)='H'(72)
}

// char-ARRAY interproc RETURN (P3): a string-building helper returns a char array
// BY VALUE (std::vector<uint16>); the caller binds it. Enables string helpers.
TEST(CodegenE2E, CharArrayInterprocReturn)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const char *src =
        "function r = f()\n"
        "  c = g();\n"          // c is a char array returned by value
        "  r = numel(c);\n"     // 7
        "end\n"
        "function s = g()\n"
        "  s = ['Hi' 'there'];\n"  // build a 7-char string
        "end\n";
    numkit::Lexer  lex(src);
    numkit::Parser parser(lex.tokenize());
    auto           root = parser.parse();
    TransferRegistry reg;
    registerStandardTransfers(reg);
    FunctionTable ft;
    collectFunctions(*root, ft);
    registerUserFunctions(reg, ft);

    const EmittedFunction emitted = emitProgram(*ft.find("f"), {}, ft, reg);
    EXPECT_NE(emitted.source.find("std::vector<std::uint16_t> "), std::string::npos)
        << "the char-building helper must return its char array by value";

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_charret_e2e.exe").string();
    const std::string outTxt = (base / "nk_charret_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double r = " + emitted.name + "();\n"  // numel('Hithere') = 7
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", r);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 7.0);  // numel of 'Hithere'
}

// Native upper/lower (P3, char case transform): upper('Hi') = 'HI'. A
// self-contained transform loop over a char array.
TEST(CodegenE2E, CharUpperLower)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const EmittedFunction emitted = transpile(
        "function r = f()\n"
        "  s = 'Hi';\n"
        "  t = upper(s);\n"            // 'HI' -> [72 73]
        "  r = t(1) * 1000 + t(2);\n"  // 72*1000 + 73 = 72073
        "end\n",
        {});
    EXPECT_NE(emitted.source.find("- 32"), std::string::npos)
        << "upper must apply the ASCII case shift (-32)";

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_upper_e2e.exe").string();
    const std::string outTxt = (base / "nk_upper_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double r = f();\n"  // upper('Hi') = 'HI'; t(1)=72, t(2)=73
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", r);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 72073.0);  // 'H'(72)*1000 + 'I'(73)
}

// Struct ARRAY field (P3): whole-array field write `s.v = a` + read `w = s.v`
// (field-local is an array). Indexing s.v(k) is deferred; read the whole field.
TEST(CodegenE2E, StructArrayField)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const EmittedFunction emitted = transpile(
        "function r = f(a)\n"
        "  s.v = a;\n"               // array field write
        "  w = s.v;\n"               // whole array field read
        "  r = numel(w) + w(2);\n"   // len(3) + w(2)(20) = 23
        "end\n",
        {{"a", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}});
    EXPECT_NE(emitted.source.find("_nk_fld_s_v.assign("), std::string::npos)
        << "an array struct field must copy via the field-local vector";

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_structarr_e2e.exe").string();
    const std::string outTxt = (base / "nk_structarr_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double a[3] = {10.0, 20.0, 30.0};\n"
        "  double r = f(a, 3);\n"  // numel(3) + w(2)=20 -> 23
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", r);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 23.0);  // numel(3) + s.v(2)=20
}

// Struct array-field element INDEXING `s.v(k)` (P3): index a struct's array field
// directly (no temp), completing struct array access.
TEST(CodegenE2E, StructFieldIndex)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const EmittedFunction emitted = transpile(
        "function r = f(a)\n"
        "  s.v = a;\n"
        "  r = s.v(2) * 10;\n"   // a(2) * 10
        "end\n",
        {{"a", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}});
    EXPECT_NE(emitted.source.find("_nk_fld_s_v.size()"), std::string::npos)
        << "s.v(k) must index the field-local array";

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_structidx_e2e.exe").string();
    const std::string outTxt = (base / "nk_structidx_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double a[3] = {10.0, 20.0, 30.0};\n"
        "  double r = f(a, 3);\n"  // s.v(2)=20 * 10 = 200
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", r);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 200.0);  // s.v(2)=20 * 10
}

// isempty(x) as the canonical input-validation idiom: `if isempty(x)`. A pure
// numel==0 expression (no loop / no bridge), exercised in both branches.
TEST(CodegenE2E, IsemptyGuard)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const EmittedFunction emitted = transpile(
        "function r = f(x)\n"
        "  if isempty(x)\n"
        "    r = -1;\n"           // empty -> sentinel
        "  else\n"
        "    r = numel(x) + 100;\n"  // non-empty -> count + 100
        "  end\n"
        "end\n",
        {{"x", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}});
    EXPECT_NE(emitted.source.find("== 0"), std::string::npos)
        << "isempty must lower to a numel==0 compare, not a bridge call";

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_isempty_e2e.exe").string();
    const std::string outTxt = (base / "nk_isempty_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double d[3] = {1.0, 2.0, 3.0};\n"
        "  double e[1] = {0.0};\n"
        "  double r_empty = f(e, 0);\n"   // isempty -> true  -> -1
        "  double r_full  = f(d, 3);\n"   // isempty -> false -> 3 + 100 = 103
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n%.17g\\n\", r_empty, r_full);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 2u);
    EXPECT_DOUBLE_EQ(got[0], -1.0);
    EXPECT_DOUBLE_EQ(got[1], 103.0);
}

// isscalar / isreal as `if` conditions: isscalar(x) is a runtime numel==1 compare
// (both branches via two calls); isreal is a COMPILE-TIME constant from the static
// dtype (true for the real array x, false for the complex scalar z).
TEST(CodegenE2E, IsscalarIsrealQueries)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const EmittedFunction emitted = transpile(
        "function r = f(z, x)\n"
        "  r = 0;\n"
        "  if isscalar(x)\n"
        "    r = r + 1;\n"      // x has one element
        "  end\n"
        "  if isreal(z)\n"
        "    r = r + 100;\n"    // z is complex -> compile-time false -> never
        "  end\n"
        "  if isreal(x)\n"
        "    r = r + 1000;\n"   // x is real -> compile-time true -> always
        "  end\n"
        "end\n",
        {{"z", InferredType::scalar(ValueType::COMPLEX)},
         {"x", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}});
    EXPECT_NE(emitted.source.find("== 1"), std::string::npos)
        << "isscalar must lower to a numel==1 compare";

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_isquery_e2e.exe").string();
    const std::string outTxt = (base / "nk_isquery_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n#include <complex>\n"
        "int main() {\n"
        "  std::complex<double> z(1.0, 2.0);\n"
        "  double x1[1] = {5.0};\n"
        "  double x3[3] = {1.0, 2.0, 3.0};\n"
        "  double r1 = f(z, x1, 1);\n"   // isscalar true (+1) + isreal(x) (+1000) = 1001
        "  double r3 = f(z, x3, 3);\n"   // isscalar false + isreal(x) (+1000) = 1000
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n%.17g\\n\", r1, r3);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 2u);
    EXPECT_DOUBLE_EQ(got[0], 1001.0);
    EXPECT_DOUBLE_EQ(got[1], 1000.0);
}

// isrow / iscolumn / isvector orientation predicates. For a row vector: isrow and
// isvector are compile-time true; iscolumn is a runtime len==1 compare (both
// branches via two calls). For a scalar: iscolumn is compile-time true.
TEST(CodegenE2E, OrientationQueries)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const EmittedFunction emitted = transpile(
        "function r = f(x, s)\n"
        "  r = 0;\n"
        "  if isrow(x)\n"
        "    r = r + 1;\n"        // x is a row -> compile-time true -> always
        "  end\n"
        "  if iscolumn(x)\n"
        "    r = r + 10;\n"       // a row is a column iff length 1 (runtime)
        "  end\n"
        "  if isvector(x)\n"
        "    r = r + 100;\n"      // a 1-D buffer is always a vector -> always
        "  end\n"
        "  if iscolumn(s)\n"
        "    r = r + 1000;\n"     // s is a scalar -> compile-time true -> always
        "  end\n"
        "end\n",
        {{"x", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())},
         {"s", InferredType::scalar(ValueType::DOUBLE)}});
    EXPECT_NE(emitted.source.find("== 1"), std::string::npos)
        << "iscolumn of a row must lower to a length==1 compare";

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_orient_e2e.exe").string();
    const std::string outTxt = (base / "nk_orient_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double x3[3] = {1.0, 2.0, 3.0};\n"
        "  double x1[1] = {5.0};\n"
        "  double r3 = f(x3, 3, 7.0);\n"   // isrow(+1) + isvector(+100) + iscolumn(s)(+1000) = 1101
        "  double r1 = f(x1, 1, 7.0);\n"   // + iscolumn(x) true (+10) = 1111
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n%.17g\\n\", r3, r1);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 2u);
    EXPECT_DOUBLE_EQ(got[0], 1101.0);
    EXPECT_DOUBLE_EQ(got[1], 1111.0);
}

// dtype-classification predicates (isnumeric/isfloat/isinteger/ischar/islogical):
// compile-time constants from the static dtype, the type-dispatch idiom. Mixed-
// dtype params exercise true AND false branches; the two should-be-false guards
// (isinteger of a double, isnumeric of a char) must contribute 0.
TEST(CodegenE2E, DtypeClassification)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const EmittedFunction emitted = transpile(
        "function r = f(d, c, b, n, z)\n"
        "  r = 0;\n"
        "  if isnumeric(d), r = r + 1; end\n"        // double -> +1
        "  if isfloat(d),   r = r + 10; end\n"       // double float -> +10
        "  if isinteger(d), r = r + 5; end\n"        // double NOT integer -> 0
        "  if ischar(c),    r = r + 100; end\n"      // char -> +100
        "  if isnumeric(c), r = r + 3; end\n"        // char NOT numeric -> 0
        "  if islogical(b), r = r + 1000; end\n"     // logical -> +1000
        "  if isinteger(n), r = r + 10000; end\n"    // int32 -> +10000
        "  if isfloat(z),   r = r + 100000; end\n"   // complex IS float -> +100000
        "end\n",
        {{"d", InferredType::scalar(ValueType::DOUBLE)},
         {"c", InferredType::scalar(ValueType::CHAR)},
         {"b", InferredType::scalar(ValueType::LOGICAL)},
         {"n", InferredType::scalar(ValueType::INT32)},
         {"z", InferredType::scalar(ValueType::COMPLEX)}});
    EXPECT_EQ(emitted.source.find("isnumeric"), std::string::npos)
        << "dtype predicates must be lowered to compile-time true/false, not left as calls";

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_dtype_e2e.exe").string();
    const std::string outTxt = (base / "nk_dtype_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n#include <complex>\n#include <cstdint>\n"
        "int main() {\n"
        "  double r = f(2.5, (std::uint16_t)'A', true, (std::int32_t)7,\n"
        "               std::complex<double>(1.0, 2.0));\n"
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", r);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 111111.0);  // only the true-branch contributions
}

// linspace(a,b,n) lowered to a native fill (the size-constructor family already
// had the transfer; this adds the emitter). A runtime n -> an owned 1-D local;
// linspace(0,2,5) = [0 0.5 1 1.5 2] (all exact in double, last point forced to b),
// encoded with numel into one scalar.
TEST(CodegenE2E, LinspaceGenerative)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const EmittedFunction emitted = transpile(
        "function r = f(n)\n"
        "  y = linspace(0, 2, n);\n"   // runtime n -> 1-D local [0, 0.5, 1, 1.5, 2]
        "  r = y(1) + y(2)*10 + y(3)*100 + y(4)*1000 + y(5)*10000 + numel(y)*100000;\n"
        "end\n",
        {{"n", InferredType::scalar(ValueType::DOUBLE)}});
    EXPECT_EQ(emitted.source.find("linspace"), std::string::npos)
        << "linspace must be lowered to a native fill, not left as a call";

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_linspace_e2e.exe").string();
    const std::string outTxt = (base / "nk_linspace_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double r = f(5.0);\n"
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", r);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    // 0 + 0.5*10 + 1*100 + 1.5*1000 + 2*10000 + 5*100000
    EXPECT_DOUBLE_EQ(got[0], 0.0 + 5.0 + 100.0 + 1500.0 + 20000.0 + 500000.0);  // 521605
}

// Colon range materialised to an array: v = a:b and v = a:s:b. The for-loop header
// form was already special-cased; this is the value form (previously inferred as a
// row but unemitted -> refused). 1:5 = [1 2 3 4 5]; 0:2:8 = [0 2 4 6 8].
TEST(CodegenE2E, ColonRangeMaterialise)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const EmittedFunction emitted = transpile(
        "function r = f()\n"
        "  v = 1:5;\n"      // [1 2 3 4 5]
        "  w = 0:2:8;\n"    // [0 2 4 6 8]
        "  r = v(1) + v(5)*10 + numel(v)*100 + w(2)*1000 + w(5)*10000 + numel(w)*100000;\n"
        "end\n",
        {});
    EXPECT_NE(emitted.source.find("_nk_cnt"), std::string::npos)
        << "a colon range must materialise via a native count+fill, not refuse";

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_colon_e2e.exe").string();
    const std::string outTxt = (base / "nk_colon_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double r = f();\n"
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", r);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    // 1 + 5*10 + 5*100 + 2*1000 + 8*10000 + 5*100000
    EXPECT_DOUBLE_EQ(got[0], 1.0 + 50.0 + 500.0 + 2000.0 + 80000.0 + 500000.0);  // 582551
}

// `end` inside a 1-D scalar index: x(end), x(end-1). Previously END_VAL was
// unhandled in codegen -> refused; now it resolves to the indexed array's length.
TEST(CodegenE2E, EndInIndex)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const EmittedFunction emitted = transpile(
        "function r = f(x)\n"
        "  a = x(end);\n"       // last element
        "  b = x(end - 1);\n"   // second-to-last
        "  r = a*100 + b;\n"
        "end\n",
        {{"x", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}});

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_end_e2e.exe").string();
    const std::string outTxt = (base / "nk_end_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double x[4] = {10.0, 20.0, 30.0, 40.0};\n"
        "  double r = f(x, 4);\n"   // x(end)=40, x(end-1)=30 -> 40*100 + 30 = 4030
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", r);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 4030.0);
}

// 1-D slice read: y = x(2:4) and z = x(2:end). A range index yields a sub-array
// (previously refused -- IndexForm had no slice). end inside the slice rides on the
// end-context. x = [10 20 30 40 50]: x(2:4)=[20 30 40], x(2:end)=[20 30 40 50].
TEST(CodegenE2E, SliceRead)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const EmittedFunction emitted = transpile(
        "function r = f(x)\n"
        "  y = x(2:4);\n"      // [20 30 40]
        "  z = x(2:end);\n"    // [20 30 40 50]
        "  r = y(1) + y(3)*10 + numel(y)*100 + z(4)*1000 + numel(z)*10000;\n"
        "end\n",
        {{"x", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}});
    EXPECT_NE(emitted.source.find("_nk_s0"), std::string::npos)
        << "a range index must materialise a slice copy, not refuse";

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_slice_e2e.exe").string();
    const std::string outTxt = (base / "nk_slice_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double x[5] = {10.0, 20.0, 30.0, 40.0, 50.0};\n"
        "  double r = f(x, 5);\n"  // 20 + 40*10 + 3*100 + 50*1000 + 4*10000 = 90720
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", r);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 20.0 + 400.0 + 300.0 + 50000.0 + 40000.0);  // 90720
}

// INTEGRATION CAPSTONE (P3): one kernel composing struct array fields + logical
// masking + find + a max reduction + char literal/upper/index + numel + an if +
// arithmetic. Proves the P3 surface composes end-to-end (cross-feature guard).
TEST(CodegenE2E, IntegrationCapstone)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const EmittedFunction emitted = transpile(
        "function out = f(x, thr)\n"
        "  rec.data = x;\n"        // struct array field (write)
        "  d = rec.data;\n"        // struct array field (whole read)
        "  m = d > thr;\n"         // logical mask (array var > scalar var)
        "  c = find(m);\n"         // positions above threshold
        "  hi = max(d);\n"         // native reduction
        "  s = 'ok';\n"            // char literal
        "  tag = upper(s);\n"      // char transform -> 'OK'
        "  out = numel(c) * 1000 + hi + tag(1);\n"  // 2*1000 + 8 + 'O'(79)
        "  if hi > 5\n"           // control-flow
        "    out = out + 1;\n"
        "  end\n"
        "end\n",
        {{"x", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())},
         {"thr", InferredType::scalar(ValueType::DOUBLE)}});
    EXPECT_NE(emitted.source.find("_nk_fld_rec_data"), std::string::npos)
        << "the capstone must exercise a struct array field-local";

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_capstone_e2e.exe").string();
    const std::string outTxt = (base / "nk_capstone_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double x[5] = {1.0, 5.0, 2.0, 8.0, 3.0};\n"  // >4 at idx 2,4 -> find=[2 4]
        "  double out = f(x, 5, 4.0);\n"  // 2*1000 + max(8) + 'O'(79) + (hi>5 ? 1) = 2088
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", out);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 2088.0);  // 2000 + 8 + 79 + 1
}

// RECURSION refuses cleanly under the bridge (P5): the monomorphiser breaks a
// recursive call to Dynamic (a sound inference break); the recursive call's boxed
// result would otherwise emit call_dyn-by-NAME, which cannot resolve the compiled
// specialisation in a standalone artifact. So recursion is REFUSED (refuse-not-
// miscompile), not silently miscompiled. (A precision upgrade -> unboxed recursion
// via a Bottom-fixpoint needs Bottom to propagate through the transfer layer:
// deferred.)
TEST(CodegenE2E, RecursiveCallRefusedUnderBridge)
{
    const char *src =
        "function y = fact(n)\n"
        "  if n <= 1\n"
        "    y = 1;\n"
        "  else\n"
        "    y = n * fact(n - 1);\n"
        "  end\n"
        "end\n";
    numkit::Lexer  lex(src);
    numkit::Parser parser(lex.tokenize());
    auto           root = parser.parse();
    TransferRegistry reg;
    registerStandardTransfers(reg);
    FunctionTable ft;
    collectFunctions(*root, ft);
    registerUserFunctions(reg, ft);

    BridgeOptions bridge;
    bridge.enabled = true;
    EXPECT_THROW(
        emitProgram(*ft.find("fact"), {{"n", InferredType::scalar(ValueType::DOUBLE)}}, ft, reg,
                    nullptr, bridge),
        std::runtime_error);
}

// Interproc array RETURN, complex 1-D variant: a callee returning a complex 1-D
// array returns std::vector<std::complex<double>> by value (emit-level — the run
// pipeline is proven by InterprocArrayReturn above).
TEST(CodegenE2E, InterprocComplexArrayReturnEmits)
{
    const char *src =
        "function s = f(x)\n"
        "  v = g(x);\n"      // g returns a complex 1-D array by value -> complex local
        "  s = numel(v);\n"  // use it (dtype-agnostic) -> scalar
        "end\n"
        "function r = g(x)\n"
        "  r = x + 1i;\n"    // double 1-D + 1i -> complex 1-D (elementwise)
        "end\n";
    numkit::Lexer  lex(src);
    numkit::Parser parser(lex.tokenize());
    auto           root = parser.parse();
    TransferRegistry reg;
    registerStandardTransfers(reg);
    FunctionTable ft;
    collectFunctions(*root, ft);
    registerUserFunctions(reg, ft);
    const InferredType vec =
        InferredType::concrete(ValueType::DOUBLE, numkit::codegen::Shape::rowVector());
    const EmittedFunction emitted = emitProgram(*ft.find("f"), {{"x", vec}}, ft, reg);
    EXPECT_NE(emitted.source.find("std::vector<std::complex<double>> "), std::string::npos)
        << "complex 1-D interproc result should return std::vector<std::complex<double>>";
}

// Boundary #2b: a multi-output function `[a,b] = f(...)`, end-to-end.
// divmod returns quotient + remainder via reference out-params.
TEST(CodegenE2E, MultiOutputCall)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const char *src =
        "function [a, b] = divmod(x, d)\n  a = floor(x / d);\n  b = x - a * d;\nend\n"
        "function y = run(x, d)\n  [q, r] = divmod(x, d);\n  y = q * 100 + r;\nend\n";
    numkit::Lexer  lex(src);
    numkit::Parser parser(lex.tokenize());
    auto           root = parser.parse();
    TransferRegistry reg;
    registerStandardTransfers(reg);
    FunctionTable ft;
    collectFunctions(*root, ft);
    registerUserFunctions(reg, ft);

    const EmittedFunction emitted = emitProgram(
        *ft.find("run"), {{"x", InferredType::scalar(ValueType::DOUBLE)},
                          {"d", InferredType::scalar(ValueType::DOUBLE)}},
        ft, reg);
    ASSERT_TRUE(emitted.source.find("double& a, double& b") != std::string::npos);

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_multiout_e2e.exe").string();
    const std::string outTxt = (base / "nk_multiout_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double r = " + emitted.name + "(17.0, 5.0);\n"
        "  std::FILE* g = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!g) return 2;\n"
        "  std::fprintf(g, \"%.17g\\n\", r);\n"
        "  std::fclose(g); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 302.0);  // 17/5 -> q=3 r=2 -> 3*100+2
}

// Multi-output with an IGNORED (~) target: [~, b] = f(x). The callee writes its
// reference out-param regardless, so the ~ slot gets a throwaway local (scoped in
// a block so repeated [~,...]=f() never collide); only b is kept.
TEST(CodegenE2E, MultiOutputIgnoredTarget)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const char *src =
        "function [a, b] = f(x)\n  a = x + 1;\n  b = x * 2;\nend\n"
        "function y = run(x)\n  [~, b] = f(x);\n  y = b;\nend\n";
    numkit::Lexer  lex(src);
    numkit::Parser parser(lex.tokenize());
    auto           root = parser.parse();
    TransferRegistry reg;
    registerStandardTransfers(reg);
    FunctionTable ft;
    collectFunctions(*root, ft);
    registerUserFunctions(reg, ft);

    const EmittedFunction emitted =
        emitProgram(*ft.find("run"), {{"x", InferredType::scalar(ValueType::DOUBLE)}}, ft, reg);
    ASSERT_TRUE(emitted.source.find("_nk_ignore_0") != std::string::npos)
        << "expected a throwaway local for the ignored (~) output";

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_multiign_e2e.exe").string();
    const std::string outTxt = (base / "nk_multiign_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double y = " + emitted.name + "(5.0);\n"  // f(5): a=6 (ignored), b=10 -> y=10
        "  std::FILE* g = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!g) return 2;\n"
        "  std::fprintf(g, \"%.17g\\n\", y);\n"
        "  std::fclose(g); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 10.0);  // b = 5*2 = 10 (a discarded)
}

// Boundary #3: an ARRAY passed across an interprocedural call. run(v)
// forwards its array to mysum(v) (emitted as `ptr, len`), which sums it.
TEST(CodegenE2E, InterproceduralArrayArg)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const char *src =
        "function s = mysum(v)\n  n = numel(v);\n  s = 0;\n"
        "  for k = 1:n\n    s = s + v(k);\n  end\nend\n"
        "function y = run(v)\n  y = mysum(v);\nend\n";
    numkit::Lexer  lex(src);
    numkit::Parser parser(lex.tokenize());
    auto           root = parser.parse();
    TransferRegistry reg;
    registerStandardTransfers(reg);
    FunctionTable ft;
    collectFunctions(*root, ft);
    registerUserFunctions(reg, ft);

    const EmittedFunction emitted = emitProgram(
        *ft.find("run"), {{"v", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}},
        ft, reg, nullptr);
    ASSERT_TRUE(emitted.source.find("mysum") != std::string::npos);

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_arrarg_e2e.exe").string();
    const std::string outTxt = (base / "nk_arrarg_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double v[4] = {1.0, 2.0, 3.0, 4.0};\n"
        "  double r = " + emitted.name + "(v, 4);\n"
        "  std::FILE* g = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!g) return 2;\n"
        "  std::fprintf(g, \"%.17g\\n\", r);\n"
        "  std::fclose(g); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 10.0);  // mysum([1 2 3 4]) forwarded across the call
}

// Boundary #2a: a handle class's VOID in-place mutator — the canonical
// handle pattern, previously refused by the single-output requirement.
// b.setv(7) is a statement (void call) mutating the shared object via ->;
// b.getv() reads it back.
TEST(CodegenE2E, HandleVoidMutator)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const char *src =
        "classdef Box < handle\n  properties\n    v = 0\n  end\n"
        "  methods\n"
        "    function setv(obj, x)\n      obj.v = x;\n    end\n"
        "    function r = getv(obj)\n      r = obj.v;\n    end\n"
        "  end\nend\n"
        "function y = run(b)\n  b.setv(7);\n  y = b.getv();\nend\n";
    numkit::Lexer  lex(src);
    numkit::Parser parser(lex.tokenize());
    auto           root = parser.parse();
    TransferRegistry reg;
    registerStandardTransfers(reg);
    ClassRegistry creg;
    collectClasses(*root, creg, reg);
    FunctionTable ft;
    collectFunctions(*root, ft);
    registerUserFunctions(reg, ft);
    registerClassMethods(reg, creg);
    registerClassConstructors(reg, creg);

    const int id = creg.idOf("Box");
    const EmittedFunction emitted =
        emitProgram(*ft.find("run"), {{"b", InferredType::object(id)}}, ft, reg, &creg);
    ASSERT_TRUE(emitted.source.find("void Box_0_0setv") != std::string::npos)
        << "void mutator should emit a void specialisation";

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_voidmut_e2e.exe").string();
    const std::string outTxt = (base / "nk_voidmut_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  nk_rt::handle<Box> b = nk_rt::handle<Box>::make();\n"
        "  double r = " + emitted.name + "(b);\n"
        "  std::FILE* g = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!g) return 2;\n"
        "  std::fprintf(g, \"%.17g\\n\", r);\n"
        "  std::fclose(g); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 7.0);  // setv(7) mutated the shared Box, getv read it
}

// Boundary #1b: an EXPLICIT constructor with arguments, end-to-end.
// r = Rect(3,4) calls the constructor whose output object is seeded as the
// class so its field writes type; then r.area() = 12.
TEST(CodegenE2E, ExplicitConstructor)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const char *src =
        "classdef Rect\n  properties\n    w = 0\n    h = 0\n  end\n"
        "  methods\n"
        "    function obj = Rect(a, b)\n      obj.w = a;\n      obj.h = b;\n    end\n"
        "    function r = area(o)\n      r = o.w * o.h;\n    end\n"
        "  end\nend\n"
        "function y = demo()\n  r = Rect(3, 4);\n  y = r.area();\nend\n";
    numkit::Lexer  lex(src);
    numkit::Parser parser(lex.tokenize());
    auto           root = parser.parse();
    TransferRegistry reg;
    registerStandardTransfers(reg);
    ClassRegistry creg;
    collectClasses(*root, creg, reg);
    FunctionTable ft;
    collectFunctions(*root, ft);
    registerUserFunctions(reg, ft);
    registerClassMethods(reg, creg);
    registerClassConstructors(reg, creg);

    const EmittedFunction emitted = emitProgram(*ft.find("demo"), {}, ft, reg, &creg);
    ASSERT_TRUE(emitted.source.find("Rect_0_0ctor") != std::string::npos)
        << "expected an explicit constructor specialisation";

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_ctor_e2e.exe").string();
    const std::string outTxt = (base / "nk_ctor_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double r = " + emitted.name + "();\n"
        "  std::FILE* g = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!g) return 2;\n"
        "  std::fprintf(g, \"%.17g\\n\", r);\n"
        "  std::fclose(g); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 12.0);  // Rect(3,4).area() = 12
}

// Handle class end-to-end: exercises the nk_rt::handle<T> path (shared
// reference, obj->field, handle param + method) that the value-class tests
// never touch. v1 supports a 1-output method (getter); a void in-place
// mutator is refused (single-output requirement) — a documented limit.
TEST(CodegenE2E, HandleClassMethodCall)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const char *src =
        "classdef Box < handle\n  properties\n    v = 0\n  end\n"
        "  methods\n    function r = getv(obj)\n      r = obj.v;\n    end\n  end\nend\n"
        "function y = run(b)\n  y = b.getv();\nend\n";
    numkit::Lexer  lex(src);
    numkit::Parser parser(lex.tokenize());
    auto           root = parser.parse();
    TransferRegistry reg;
    registerStandardTransfers(reg);
    ClassRegistry creg;
    collectClasses(*root, creg, reg);
    FunctionTable ft;
    collectFunctions(*root, ft);
    registerUserFunctions(reg, ft);
    registerClassMethods(reg, creg);

    const int id = creg.idOf("Box");
    ASSERT_GE(id, 0);
    const EmittedFunction emitted =
        emitProgram(*ft.find("run"), {{"b", InferredType::object(id)}}, ft, reg, &creg);
    ASSERT_TRUE(emitted.source.find("nk_rt::handle<Box>") != std::string::npos)
        << "handle class should use the handle<T> wrapper";

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_handle_e2e.exe").string();
    const std::string outTxt = (base / "nk_handle_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  nk_rt::handle<Box> b = nk_rt::handle<Box>::make();\n"
        "  b->v = 9.0;\n"
        "  double r = " + emitted.name + "(b);\n"
        "  std::FILE* g = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!g) return 2;\n"
        "  std::fprintf(g, \"%.17g\\n\", r);\n"
        "  std::fclose(g); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 9.0);  // obj->v read through the handle
}

// Class brick 5b: a value class round-trips through compiled code. The
// harness constructs a Point in C++, sets a field, and the transpiled
// getx() reads it back. Proves struct emission + object param + field read
// compile and run.
TEST(CodegenE2E, ValueClassFieldRoundTrip)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const char *src =
        "classdef Point\n  properties\n    x = 0\n    y = 0\n  end\nend\n"
        "function y = getx(p)\n  y = p.x;\nend\n";
    numkit::Lexer  lex(src);
    numkit::Parser parser(lex.tokenize());
    auto           root = parser.parse();
    TransferRegistry reg;
    registerStandardTransfers(reg);
    ClassRegistry creg;
    collectClasses(*root, creg, reg);
    const int id = creg.idOf("Point");
    ASSERT_GE(id, 0);
    const numkit::ASTNode *fn = nullptr;
    for (const auto &c : root->children)
        if (c && c->type == numkit::NodeType::FUNCTION_DEF) fn = c.get();
    ASSERT_NE(fn, nullptr);

    const EmittedFunction emitted =
        emitFunction(*fn, {{"p", InferredType::object(id)}}, reg, &creg);
    ASSERT_EQ(emitted.signature, "double getx(Point p)");

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_valueclass_e2e.exe").string();
    const std::string outTxt = (base / "nk_valueclass_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  Point p{};\n"
        "  p.x = 7.0;\n"
        "  double r = getx(p);\n"
        "  std::FILE* g = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!g) return 2;\n"
        "  std::fprintf(g, \"%.17g\\n\", r);\n"
        "  std::fclose(g); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 7.0);
}

// Engine 1b: a 2-function program (f calls g) compiles, runs, and is
// correct. f(3) = g(3) + 1 = 6 + 1 = 7.
TEST(CodegenE2E, InterproceduralRunsCorrectly)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const char *src =
        "function y = f(x)\n  y = g(x) + 1;\nend\n"
        "function y = g(x)\n  y = x*2;\nend\n";
    numkit::Lexer  lex(src);
    numkit::Parser parser(lex.tokenize());
    auto           root = parser.parse();
    FunctionTable  table;
    collectFunctions(*root, table);
    TransferRegistry reg;
    registerStandardTransfers(reg);
    registerUserFunctions(reg, table);

    const EmittedFunction emitted =
        emitProgram(*table.find("f"), {{"x", InferredType::scalar(ValueType::DOUBLE)}},
                    table, reg);

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_interproc_e2e.exe").string();
    const std::string outTxt = (base / "nk_interproc_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double r = " + emitted.name + "(3.0);\n"  // f__d(3.0)
        "  std::FILE* g = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!g) return 2;\n"
        "  std::fprintf(g, \"%.17g\\n\", r);\n"
        "  std::fclose(g); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 7.0);
}

// Array LOCALS end-to-end: an owned-vector local (`z = zeros(1,n)`), written
// then read in loops, compiled and RUN. Self-contained (no bridge). The result
// is x*2 + 1 elementwise.
TEST(CodegenE2E, ArrayLocalRunsCorrectly)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const EmittedFunction emitted = transpile(
        "function y = f(x)\n  n = numel(x);\n  z = zeros(1, n);\n"
        "  for k = 1:n\n    z(k) = x(k) * 2;\n  end\n"
        "  y = zeros(1, n);\n  for k = 1:n\n    y(k) = z(k) + 1;\n  end\nend\n",
        {{"x", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}});
    ASSERT_NE(emitted.source.find("std::vector<double> z;"), std::string::npos);

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_arrlocal_e2e.exe").string();
    const std::string outTxt = (base / "nk_arrlocal_e2e_out.txt").string();
    const std::size_t N       = 16;
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  const std::size_t N = 16;\n"
        "  double x[16], y[16];\n"
        "  for (std::size_t i = 0; i < N; ++i) x[i] = 0.5 * double(i + 1);\n"
        "  f(x, N, y, N);\n"
        "  std::FILE* g = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!g) return 2;\n"
        "  for (std::size_t i = 0; i < N; ++i) std::fprintf(g, \"%.17g\\n\", y[i]);\n"
        "  std::fclose(g); return 0;\n}\n";
    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), N);
    for (std::size_t i = 0; i < N; ++i)
        EXPECT_NEAR(got[i], 0.5 * double(i + 1) * 2.0 + 1.0, 1e-12) << "at i=" << i;
}

// Array local passed ACROSS an interprocedural call + numel(local): covers the
// changed appendCallArg path (a local is passed as `z.data(), z.size()`) and
// numel on a local. f builds z (a local copy of x), then y = g(z) + numel(z),
// where g sums its array arg -> y = sum(x) + n.
TEST(CodegenE2E, ArrayLocalAsCallArgAndNumel)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const char *src =
        "function y = f(x)\n  n = numel(x);\n  z = zeros(1, n);\n"
        "  for k = 1:n\n    z(k) = x(k);\n  end\n"
        "  y = g(z) + numel(z);\nend\n"
        "function s = g(v)\n  m = numel(v);\n  s = 0;\n"
        "  for k = 1:m\n    s = s + v(k);\n  end\nend\n";
    numkit::Lexer  lex(src);
    numkit::Parser parser(lex.tokenize());
    auto           root = parser.parse();
    TransferRegistry reg;
    registerStandardTransfers(reg);
    FunctionTable ft;
    collectFunctions(*root, ft);
    registerUserFunctions(reg, ft);

    const EmittedFunction emitted = emitProgram(
        *ft.find("f"), {{"x", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}}, ft,
        reg);
    ASSERT_NE(emitted.source.find("std::vector<double> z;"), std::string::npos);
    ASSERT_NE(emitted.source.find("z.data(), z.size()"), std::string::npos);  // local as call arg

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_arrlocal_arg_e2e.exe").string();
    const std::string outTxt = (base / "nk_arrlocal_arg_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double x[8];\n"
        "  for (std::size_t i = 0; i < 8; ++i) x[i] = double(i + 1);\n"
        "  double r = " + emitted.name + "(x, 8);\n"
        "  std::FILE* g = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!g) return 2;\n"
        "  std::fprintf(g, \"%.17g\\n\", r);\n"
        "  std::fclose(g); return 0;\n}\n";
    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 36.0 + 8.0);  // sum(1..8) + numel = 44
}

// Elementwise array ARITHMETIC end-to-end (self-contained, no bridge): a local
// z = x .* 2 (resized), then the output y = z + 1 -> y = x*2 + 1 per element.
TEST(CodegenE2E, ElementwiseArrayArithmeticRunsCorrectly)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const EmittedFunction emitted = transpile(
        "function y = f(x)\n  z = x .* 2;\n  y = z + 1;\nend\n",
        {{"x", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}});
    ASSERT_NE(emitted.source.find("std::vector<double> z;"), std::string::npos);  // array local
    ASSERT_EQ(emitted.source.find("nk_codegen_rt.h"), std::string::npos);         // self-contained

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_ewise_e2e.exe").string();
    const std::string outTxt = (base / "nk_ewise_e2e_out.txt").string();
    const std::size_t N       = 12;
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  const std::size_t N = 12;\n"
        "  double x[12], y[12];\n"
        "  for (std::size_t i = 0; i < N; ++i) x[i] = double(i + 1);\n"
        "  f(x, N, y, N);\n"
        "  std::FILE* g = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!g) return 2;\n"
        "  for (std::size_t i = 0; i < N; ++i) std::fprintf(g, \"%.17g\\n\", y[i]);\n"
        "  std::fclose(g); return 0;\n}\n";
    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), N);
    for (std::size_t i = 0; i < N; ++i)
        EXPECT_DOUBLE_EQ(got[i], double(i + 1) * 2.0 + 1.0) << "at i=" << i;
}

// Scalar scaling via `*` and `/` end-to-end: y = 2*v + v/2 (both the mtimes and
// mrdivide scalar forms in one expression). v=[10,20,30] -> 2*v=[20,40,60],
// v/2=[5,10,15], sum=[25,50,75]. Self-contained (no bridge).
TEST(CodegenE2E, ScalarScalingRunsCorrectly)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const EmittedFunction emitted = transpile(
        "function y = f(v)\n  y = 2 * v + v / 2;\nend\n",
        {{"v", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}});
    ASSERT_NE(emitted.source.find("(2.0 * v[_nk_i])"), std::string::npos);
    ASSERT_NE(emitted.source.find("(v[_nk_i] / 2.0)"), std::string::npos);
    ASSERT_EQ(emitted.source.find("nk_codegen_rt.h"), std::string::npos);  // self-contained

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_scale_e2e.exe").string();
    const std::string outTxt = (base / "nk_scale_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double v[3] = {10, 20, 30}, y[3];\n"
        "  f(v, 3, y, 3);\n"
        "  std::FILE* g = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!g) return 2;\n"
        "  for (int i = 0; i < 3; ++i) std::fprintf(g, \"%.17g\\n\", y[i]);\n"
        "  std::fclose(g); return 0;\n}\n";
    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 3u);
    const double exp[3] = {25, 50, 75};  // 2*v + v/2
    for (int i = 0; i < 3; ++i) EXPECT_DOUBLE_EQ(got[i], exp[i]) << "at " << i;
}

// 2-D elementwise scalar broadcast end-to-end: B = 2*A + 1 over a 2x3 matrix.
// A col-major {1,2,3,4,5,6}; B = 2*A+1 = {3,5,7,9,11,13} (flat, rank-agnostic).
TEST(CodegenE2E, Elementwise2DRunsCorrectly)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const EmittedFunction emitted = transpile(
        "function B = f(A)\n  B = 2 * A + 1;\nend\n",
        {{"A", InferredType::concrete(ValueType::DOUBLE, Shape::dims(2, 3))}});
    ASSERT_NE(emitted.source.find("B[_nk_i] = ((2.0 * A[_nk_i]) + 1.0);"), std::string::npos);
    ASSERT_EQ(emitted.source.find("nk_codegen_rt.h"), std::string::npos);  // self-contained

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_ewise2d_e2e.exe").string();
    const std::string outTxt = (base / "nk_ewise2d_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double A[6] = {1, 2, 3, 4, 5, 6};\n"  // 2x3 col-major
        "  double B[6];\n"
        "  " + emitted.name + "(A, 2, 3, B, 2, 3);\n"
        "  std::FILE* g = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!g) return 2;\n"
        "  for (int i = 0; i < 6; ++i) std::fprintf(g, \"%.17g\\n\", B[i]);\n"
        "  std::fclose(g); return 0;\n}\n";
    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 6u);
    const double exp[6] = {3, 5, 7, 9, 11, 13};  // 2*A + 1
    for (int i = 0; i < 6; ++i) EXPECT_DOUBLE_EQ(got[i], exp[i]) << "at " << i;
}

// Two-matrix 2-D elementwise end-to-end: B = A + C (per-dim shape guard, flat
// column-major loop). A={1..6}, C={10,20,30,40,50,60} -> B={11,22,33,44,55,66}.
TEST(CodegenE2E, Elementwise2DTwoMatrixRunsCorrectly)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const EmittedFunction emitted = transpile(
        "function B = f(A, C)\n  B = A + C;\nend\n",
        {{"A", InferredType::concrete(ValueType::DOUBLE, Shape::dims(2, 3))},
         {"C", InferredType::concrete(ValueType::DOUBLE, Shape::dims(2, 3))}});
    ASSERT_NE(emitted.source.find("B[_nk_i] = (A[_nk_i] + C[_nk_i]);"), std::string::npos);
    ASSERT_NE(emitted.source.find("_nk_A_rows != _nk_B_rows"), std::string::npos);  // per-dim guard

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_ewise2d2_e2e.exe").string();
    const std::string outTxt = (base / "nk_ewise2d2_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double A[6] = {1, 2, 3, 4, 5, 6};\n"
        "  double C[6] = {10, 20, 30, 40, 50, 60};\n"
        "  double B[6];\n"
        "  " + emitted.name + "(A, 2, 3, C, 2, 3, B, 2, 3);\n"
        "  std::FILE* g = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!g) return 2;\n"
        "  for (int i = 0; i < 6; ++i) std::fprintf(g, \"%.17g\\n\", B[i]);\n"
        "  std::fclose(g); return 0;\n}\n";
    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 6u);
    const double exp[6] = {11, 22, 33, 44, 55, 66};  // A + C
    for (int i = 0; i < 6; ++i) EXPECT_DOUBLE_EQ(got[i], exp[i]) << "at " << i;
}

// N-D elementwise (single array operand) end-to-end: B = 2*A + 1 over a rank-3
// 2x3x4 array (flat column-major loop over numel). A[i]=i -> B[i]=2i+1.
TEST(CodegenE2E, ElementwiseNDRunsCorrectly)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const EmittedFunction emitted = transpile(
        "function B = f(A)\n  B = 2 * A + 1;\nend\n",
        {{"A", InferredType::concrete(ValueType::DOUBLE, numkit::codegen::Shape::ndShape({2, 3, 4}))}});
    ASSERT_NE(emitted.source.find("B[_nk_i] = ((2.0 * A[_nk_i]) + 1.0);"), std::string::npos);
    ASSERT_EQ(emitted.source.find("nk_codegen_rt.h"), std::string::npos);  // self-contained

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_ewisend_e2e.exe").string();
    const std::string outTxt = (base / "nk_ewisend_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double A[24], B[24];\n"
        "  for (int i = 0; i < 24; ++i) A[i] = double(i);\n"
        "  f(A, 2, 3, 4, B, 2, 3, 4);\n"
        "  std::FILE* g = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!g) return 2;\n"
        "  for (int i = 0; i < 24; ++i) std::fprintf(g, \"%.17g\\n\", B[i]);\n"
        "  std::fclose(g); return 0;\n}\n";
    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 24u);
    for (int i = 0; i < 24; ++i) EXPECT_DOUBLE_EQ(got[i], 2.0 * i + 1.0) << "at " << i;
}

// Multi-operand N-D elementwise end-to-end: B = A + C over a rank-3 2x3x4
// array (per-axis shape guard + flat numel loop). A[i]=i, C[i]=10i -> B[i]=11i.
TEST(CodegenE2E, ElementwiseNDMultiOperandRunsCorrectly)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const InferredType nd =
        InferredType::concrete(ValueType::DOUBLE, numkit::codegen::Shape::ndShape({2, 3, 4}));
    const EmittedFunction emitted =
        transpile("function B = f(A, C)\n  B = A + C;\nend\n", {{"A", nd}, {"C", nd}});
    ASSERT_NE(emitted.source.find("B[_nk_i] = (A[_nk_i] + C[_nk_i]);"), std::string::npos);
    ASSERT_NE(emitted.source.find("_nk_A_d0 != _nk_B_d0"), std::string::npos);  // per-axis guard

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_ewisendmul_e2e.exe").string();
    const std::string outTxt = (base / "nk_ewisendmul_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double A[24], C[24], B[24];\n"
        "  for (int i = 0; i < 24; ++i) { A[i] = double(i); C[i] = 10.0 * i; }\n"
        "  f(A, 2, 3, 4, C, 2, 3, 4, B, 2, 3, 4);\n"
        "  std::FILE* g = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!g) return 2;\n"
        "  for (int i = 0; i < 24; ++i) std::fprintf(g, \"%.17g\\n\", B[i]);\n"
        "  std::fclose(g); return 0;\n}\n";
    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 24u);
    for (int i = 0; i < 24; ++i) EXPECT_DOUBLE_EQ(got[i], 11.0 * i) << "at " << i;
}

// 2-D matrix WRITE end-to-end: a mutable 2-D local (compile-time dims),
// element writes + reads, column-major. Self-contained. s = 5+7+9 = 21.
TEST(CodegenE2E, Matrix2DLocalWriteRunsCorrectly)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const EmittedFunction emitted = transpile(
        "function s = f()\n  A = zeros(3, 3);\n  A(1,1) = 5;\n  A(2,2) = 7;\n  A(3,3) = 9;\n"
        "  s = A(1,1) + A(2,2) + A(3,3);\nend\n",
        {});
    ASSERT_NE(emitted.source.find("std::vector<double> A;"), std::string::npos);
    ASSERT_NE(emitted.source.find("nk_rt::index2_set(A.data(), 3, 3"), std::string::npos);

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_mat2dwrite_e2e.exe").string();
    const std::string outTxt = (base / "nk_mat2dwrite_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double r = " + emitted.name + "();\n"
        "  std::FILE* g = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!g) return 2;\n"
        "  std::fprintf(g, \"%.17g\\n\", r);\n"
        "  std::fclose(g); return 0;\n}\n";
    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 21.0);  // 5 + 7 + 9
}

// N-D (rank-3) matrix WRITE end-to-end: a 3-D local (compile-time dims),
// column-major element writes + reads. Self-contained. s = 5+9+3 = 17.
TEST(CodegenE2E, NDArrayLocalWriteRunsCorrectly)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const EmittedFunction emitted = transpile(
        "function s = f()\n  A = zeros(2, 2, 2);\n  A(1,1,1) = 5;\n  A(2,2,2) = 9;\n"
        "  A(1,2,1) = 3;\n  s = A(1,1,1) + A(2,2,2) + A(1,2,1);\nend\n",
        {});
    ASSERT_NE(emitted.source.find("nk_rt::indexN_set"), std::string::npos);

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_ndwrite_e2e.exe").string();
    const std::string outTxt = (base / "nk_ndwrite_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double r = " + emitted.name + "();\n"
        "  std::FILE* g = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!g) return 2;\n"
        "  std::fprintf(g, \"%.17g\\n\", r);\n"
        "  std::fclose(g); return 0;\n}\n";
    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 17.0);  // 5 + 9 + 3
}

// N-D PARAM end-to-end: a 3-D array passed in (ptr + dim companions),
// read column-major. s = A(1,1,1) + A(2,2,2) + size(A,2).
TEST(CodegenE2E, NDParamReadRunsCorrectly)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const EmittedFunction emitted = transpile(
        "function s = f(A)\n  s = A(1,1,1) + A(2,2,2) + size(A,2);\nend\n",
        {{"A", InferredType::concrete(ValueType::DOUBLE, Shape::ndShape({2, 2, 2}))}});
    ASSERT_NE(emitted.source.find("std::size_t _nk_A_d0, std::size_t _nk_A_d1, std::size_t _nk_A_d2"),
              std::string::npos);

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_ndparam_e2e.exe").string();
    const std::string outTxt = (base / "nk_ndparam_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double A[8] = {0,0,0,0,0,0,0,0};\n"
        "  A[0] = 10.0;  // A(1,1,1) column-major offset 0\n"
        "  A[7] = 99.0;  // A(2,2,2) offset 1 + 2*(1 + 2*1) = 7\n"
        "  double r = " + emitted.name + "(A, 2, 2, 2);\n"
        "  std::FILE* g = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!g) return 2;\n"
        "  std::fprintf(g, \"%.17g\\n\", r);\n"
        "  std::fclose(g); return 0;\n}\n";
    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 10.0 + 99.0 + 2.0);  // A(1,1,1) + A(2,2,2) + size(A,2)
}

// N-D shape queries end-to-end: ndims / size(A,k) / numel on a 3-D array.
// s = ndims*1000 + size1*100 + size2*10 + size3 + numel.
TEST(CodegenE2E, NDShapeQueriesRunCorrectly)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const EmittedFunction emitted = transpile(
        "function s = f()\n  A = zeros(2, 3, 4);\n"
        "  s = ndims(A) * 1000 + size(A,1) * 100 + size(A,2) * 10 + size(A,3) + numel(A);\nend\n",
        {});

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_ndshape_e2e.exe").string();
    const std::string outTxt = (base / "nk_ndshape_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double r = " + emitted.name + "();\n"
        "  std::FILE* g = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!g) return 2;\n"
        "  std::fprintf(g, \"%.17g\\n\", r);\n"
        "  std::fclose(g); return 0;\n}\n";
    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    // ndims=3, size=(2,3,4), numel=24 -> 3000 + 200 + 30 + 4 + 24
    EXPECT_DOUBLE_EQ(got[0], 3258.0);
}

// N-D OUTPUT end-to-end: f returns a 2x2x2 array via a caller-allocated
// out-param (mutable ptr + dim companions passed IN). The caller allocates 8
// doubles (pre-seeded with a -1 sentinel), passes dims 2,2,2; the body
// zero-fills (must clear the sentinel) then writes y(1,1,1)=a (offset 0) and
// y(2,2,2)=2a (offset 7). Verify both written cells + a zeroed interior.
TEST(CodegenE2E, NDOutputRunsCorrectly)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const EmittedFunction emitted = transpile(
        "function y = f(a)\n  y = zeros(2, 2, 2);\n  y(1,1,1) = a;\n  y(2,2,2) = a * 2;\nend\n",
        {{"a", InferredType::scalar(ValueType::DOUBLE)}});
    ASSERT_NE(emitted.source.find("double* __restrict y, std::size_t _nk_y_d0, std::size_t _nk_y_d1, std::size_t _nk_y_d2"),
              std::string::npos);

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_ndout_e2e.exe").string();
    const std::string outTxt = (base / "nk_ndout_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double y[8];\n"
        "  for (int i = 0; i < 8; ++i) y[i] = -1.0;  // sentinel: zeros() must clear it\n"
        "  " + emitted.name + "(3.0, y, 2, 2, 2);\n"
        "  std::FILE* g = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!g) return 2;\n"
        "  for (int i = 0; i < 8; ++i) std::fprintf(g, \"%.17g\\n\", y[i]);\n"
        "  std::fclose(g); return 0;\n}\n";
    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 8u);
    EXPECT_DOUBLE_EQ(got[0], 3.0);  // y(1,1,1) = a       (column-major offset 0)
    EXPECT_DOUBLE_EQ(got[7], 6.0);  // y(2,2,2) = a * 2   (offset 7)
    for (int i = 1; i < 7; ++i)
        EXPECT_DOUBLE_EQ(got[i], 0.0) << "zeros() must clear the interior at i=" << i;
}

// 2-D matrix OUTPUT end-to-end: f(a) returns a 2x3 matrix via a caller-allocated
// out-param (ptr + rows/cols companions, column-major). The caller allocates 6
// (seeded with a -1 sentinel that zeros() must clear), passes rows=2, cols=3;
// M(1,1)=a (offset 0), M(2,3)=2a (column-major offset (3-1)*2+(2-1)=5).
TEST(CodegenE2E, Matrix2DOutputRunsCorrectly)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const EmittedFunction emitted = transpile(
        "function M = f(a)\n  M = zeros(2, 3);\n  M(1,1) = a;\n  M(2,3) = a * 2;\nend\n",
        {{"a", InferredType::scalar(ValueType::DOUBLE)}});
    ASSERT_NE(emitted.source.find("double* __restrict M, std::size_t _nk_M_rows, std::size_t _nk_M_cols"),
              std::string::npos);

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_mat2dout_e2e.exe").string();
    const std::string outTxt = (base / "nk_mat2dout_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double M[6];\n"
        "  for (int i = 0; i < 6; ++i) M[i] = -1.0;  // sentinel: zeros() must clear it\n"
        "  " + emitted.name + "(4.0, M, 2, 3);\n"  // a=4, rows=2, cols=3
        "  std::FILE* g = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!g) return 2;\n"
        "  for (int i = 0; i < 6; ++i) std::fprintf(g, \"%.17g\\n\", M[i]);\n"
        "  std::fclose(g); return 0;\n}\n";
    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 6u);
    EXPECT_DOUBLE_EQ(got[0], 4.0);  // M(1,1) = a        (column-major offset 0)
    EXPECT_DOUBLE_EQ(got[5], 8.0);  // M(2,3) = a * 2    (offset 5)
    for (int i = 1; i < 5; ++i)
        EXPECT_DOUBLE_EQ(got[i], 0.0) << "zeros() must clear the interior at i=" << i;
}

// 1-D size(vec, dim) end-to-end on a row vector: size(x,1)=1, size(x,2)=numel.
// f(x) = size(x,1)*100 + size(x,2); x of length 5 -> 1*100 + 5 = 105. Proves
// the orientation fold compiles + runs (RawBuffer erases row/col, the type
// supplies it).
TEST(CodegenE2E, OneDSizeOrientationRunsCorrectly)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const EmittedFunction emitted = transpile(
        "function s = f(x)\n  s = size(x,1)*100 + size(x,2);\nend\n",
        {{"x", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}});
    ASSERT_NE(emitted.source.find("static_cast<double>(_nk_x_len)"), std::string::npos);  // size(.,2)
    ASSERT_NE(emitted.source.find("static_cast<double>(1) * 100.0"), std::string::npos);  // size(.,1)

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_1dsize_e2e.exe").string();
    const std::string outTxt = (base / "nk_1dsize_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double x[5] = {10, 20, 30, 40, 50};\n"
        "  double s = " + emitted.name + "(x, 5);\n"
        "  std::FILE* g = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!g) return 2;\n"
        "  std::fprintf(g, \"%.17g\\n\", s);\n"
        "  std::fclose(g); return 0;\n}\n";
    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 105.0);  // size(x,1)=1 *100 + size(x,2)=5
}

// `s = size(A)` (no-dim row) end-to-end: s is a 1x2 LOCAL filled with [1, len],
// then indexed. f(x): s = size(x); r = s(1)*1000 + s(2). x of length 5 ->
// s=[1 5] -> r = 1*1000 + 5 = 1005. Proves the array-result producer + that the
// produced row indexes correctly.
TEST(CodegenE2E, SizeNoDimRunsCorrectly)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const EmittedFunction emitted = transpile(
        "function r = f(x)\n  s = size(x);\n  r = s(1) * 1000 + s(2);\nend\n",
        {{"x", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}});
    ASSERT_NE(emitted.source.find("s.assign(2, 0.0)"), std::string::npos);  // 1x2 row local

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_sizenodim_e2e.exe").string();
    const std::string outTxt = (base / "nk_sizenodim_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double x[5] = {10, 20, 30, 40, 50};\n"
        "  double r = " + emitted.name + "(x, 5);\n"
        "  std::FILE* g = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!g) return 2;\n"
        "  std::fprintf(g, \"%.17g\\n\", r);\n"
        "  std::fclose(g); return 0;\n}\n";
    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 1005.0);  // [1 5] -> 1*1000 + 5
}

// [r, c] = size(A) end-to-end. r, c are scalar LOCALS, so this also validates
// that multi-assign targets are hoisted. 2-D: A is 3x4 -> r=3, c=4 -> 3004.
// N-D: c folds the trailing dims (3*4=12), matching MATLAB's
// [r,c]=size(rand(2,3,4)) -> 2, 12.
TEST(CodegenE2E, SizeTwoOutputRunsCorrectly)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    auto run = [](const EmittedFunction &emitted, const std::string &tag,
                  const std::string &call) -> std::vector<double> {
        auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
        std::filesystem::create_directories(base);
        const std::string exe    = (base / ("nk_" + tag + ".exe")).string();
        const std::string outTxt = (base / ("nk_" + tag + "_out.txt")).string();
        std::string       program = emitted.source +
            "#include <cstdio>\n"
            "int main() {\n"
            "  double A[24] = {0};\n"
            "  double y = " + call + ";\n"
            "  std::FILE* g = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
            "  if (!g) return 2;\n"
            "  std::fprintf(g, \"%.17g\\n\", y);\n"
            "  std::fclose(g); return 0;\n}\n";
        return compileRunReadDoubles(program, exe, outTxt);
    };

    const char *src = "function y = f(A)\n  [r, c] = size(A);\n  y = r * 1000 + c;\nend\n";
    {  // 2-D: r=rows, c=cols (both scalar locals)
        const EmittedFunction emitted =
            transpile(src, {{"A", InferredType::concrete(ValueType::DOUBLE, Shape::dims(3, 4))}});
        ASSERT_NE(emitted.source.find("r = static_cast<double>(_nk_A_rows);"), std::string::npos);
        const std::vector<double> got = run(emitted, "size2_e2e", emitted.name + "(A, 3, 4)");
        ASSERT_EQ(got.size(), 1u);
        EXPECT_DOUBLE_EQ(got[0], 3004.0);  // r=3, c=4
    }
    {  // N-D: c folds the trailing dims (d1*d2 = 3*4 = 12)
        const EmittedFunction emitted = transpile(
            src, {{"A", InferredType::concrete(ValueType::DOUBLE,
                                               numkit::codegen::Shape::ndShape({2, 3, 4}))}});
        ASSERT_NE(emitted.source.find("_nk_A_d1 * _nk_A_d2"), std::string::npos);
        const std::vector<double> got = run(emitted, "size2nd_e2e", emitted.name + "(A, 2, 3, 4)");
        ASSERT_EQ(got.size(), 1u);
        EXPECT_DOUBLE_EQ(got[0], 2012.0);  // r=2, c=3*4=12
    }
}

// RUNTIME-dim N-D LOCAL end-to-end: A = zeros(m,n,p) with the dims passed in as
// args. f(2,3,4): A is 2x3x4 (NOT a cube — proves per-axis strides), A(1,1,1)=7
// (offset 0), A(2,2,2)=9 (column-major offset 1 + 2*1 + (2*3)*1 = 9). Then
// s = 7 + 9 + size(A,2)=3 + numel=24 + ndims=3 = 46. Every dim is a runtime
// value captured at the zeros() assignment.
TEST(CodegenE2E, NDRuntimeLocalRunsCorrectly)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const EmittedFunction emitted = transpile(
        "function s = f(m, n, p)\n  A = zeros(m, n, p);\n  A(1,1,1) = 7;\n  A(2,2,2) = 9;\n"
        "  s = A(1,1,1) + A(2,2,2) + size(A,2) + numel(A) + ndims(A);\nend\n",
        {{"m", InferredType::scalar(ValueType::DOUBLE)},
         {"n", InferredType::scalar(ValueType::DOUBLE)},
         {"p", InferredType::scalar(ValueType::DOUBLE)}});
    ASSERT_NE(emitted.source.find("A.assign(_nk_A_d0 * _nk_A_d1 * _nk_A_d2"), std::string::npos);  // runtime size

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_ndrt_e2e.exe").string();
    const std::string outTxt = (base / "nk_ndrt_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double r = " + emitted.name + "(2.0, 3.0, 4.0);\n"
        "  std::FILE* g = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!g) return 2;\n"
        "  std::fprintf(g, \"%.17g\\n\", r);\n"
        "  std::fclose(g); return 0;\n}\n";
    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 7.0 + 9.0 + 3.0 + 24.0 + 3.0);  // = 46
}

// RUNTIME-dim N-D OUTPUT: the same `zeros(>=3 runtime)->NDims` inference that
// enables runtime-dim locals composes with N6 (N-D outputs). `y = zeros(m,n,2)`
// returns an m x n x 2 array; under the RawBuffer caller-pre-sizes contract the
// caller allocates m*n*2 and passes the matching companions. f(2,3): y is
// 2x3x2 (12 elems); y(1,1,1)=5 (offset 0), y(2,3,2)=8 (offset 1+2*2+6*1=11).
TEST(CodegenE2E, NDRuntimeOutputRunsCorrectly)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const EmittedFunction emitted = transpile(
        "function y = f(m, n)\n  y = zeros(m, n, 2);\n  y(1,1,1) = 5;\n  y(m,n,2) = 8;\nend\n",
        {{"m", InferredType::scalar(ValueType::DOUBLE)},
         {"n", InferredType::scalar(ValueType::DOUBLE)}});
    ASSERT_NE(emitted.source.find("double* __restrict y, std::size_t _nk_y_d0, std::size_t _nk_y_d1, std::size_t _nk_y_d2"),
              std::string::npos);

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_ndrtout_e2e.exe").string();
    const std::string outTxt = (base / "nk_ndrtout_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double y[12];\n"
        "  for (int i = 0; i < 12; ++i) y[i] = -1.0;  // sentinel; zeros() must clear\n"
        "  " + emitted.name + "(2.0, 3.0, y, 2, 3, 2);\n"  // caller pre-sizes: m=2,n=3,2
        "  std::FILE* g = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!g) return 2;\n"
        "  for (int i = 0; i < 12; ++i) std::fprintf(g, \"%.17g\\n\", y[i]);\n"
        "  std::fclose(g); return 0;\n}\n";
    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 12u);
    EXPECT_DOUBLE_EQ(got[0], 5.0);   // y(1,1,1), offset 0
    EXPECT_DOUBLE_EQ(got[11], 8.0);  // y(2,3,2), column-major offset 11
    for (int i = 1; i < 11; ++i)
        EXPECT_DOUBLE_EQ(got[i], 0.0) << "zeros() must clear the interior at i=" << i;
}

// Reserved-companion coexistence, end-to-end: a user var named exactly like a
// synthesised companion now compiles + runs (it once had to be refused). The
// `_nk_` prefix puts every companion in the underscore namespace MATLAB can't
// enter, so the param's length companion `_nk_x_len` and the user's `x_len`
// coexist. f(x) returns x(numel(x)) = last element. x=[10..50] -> 50.
TEST(CodegenE2E, CompanionUserVarCoexistRunsCorrectly)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const EmittedFunction emitted = transpile(
        "function y = f(x)\n  x_len = numel(x);\n  y = x(x_len);\nend\n",
        {{"x", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}});
    ASSERT_NE(emitted.source.find("std::size_t _nk_x_len"), std::string::npos);  // companion
    ASSERT_NE(emitted.source.find("double x_len"), std::string::npos);           // user var coexists

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_companion_coexist_e2e.exe").string();
    const std::string outTxt = (base / "nk_companion_coexist_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double x[5] = {10, 20, 30, 40, 50};\n"
        "  double r = " + emitted.name + "(x, 5);\n"  // _nk_x_len = 5
        "  std::FILE* g = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!g) return 2;\n"
        "  std::fprintf(g, \"%.17g\\n\", r);\n"
        "  std::fclose(g); return 0;\n}\n";
    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 50.0);  // x(numel(x)) = x(5)
}

// Negative array dimension: `zeros(1, n)` with n<0. After fix/zeros-size-args
// the interpreter CLAMPS a negative dim to 0 (zeros(1,-1) -> 1x0 empty, MATLAB
// parity); codegen's nk_rt::dim mirrors it (the clamp PRECEDES the float->size_t
// cast — a negative value never UB-casts). f(3) -> numel 3; f(-1) -> numel 0
// (empty), matching the interpreter. (A non-integer or too-large dim still
// errors — also like the interpreter; see toDim.)
TEST(CodegenE2E, NegativeDimClampsToEmpty)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const EmittedFunction emitted = transpile(
        "function s = f(n)\n  A = zeros(1, n);\n  s = numel(A);\nend\n",
        {{"n", InferredType::scalar(ValueType::DOUBLE)}});
    ASSERT_NE(emitted.source.find("nk_rt::dim("), std::string::npos);  // guarded conversion, no UB cast

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_negdim_e2e.exe").string();
    const std::string outTxt = (base / "nk_negdim_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double pos = " + emitted.name + "(3.0);\n"   // 1x3 -> numel 3
        "  double neg = " + emitted.name + "(-1.0);\n"  // negative dim -> clamp to 0 -> empty
        "  std::FILE* g = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!g) return 2;\n"
        "  std::fprintf(g, \"%.17g\\n%.17g\\n\", pos, neg);\n"
        "  std::fclose(g); return 0;\n}\n";
    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 2u);
    EXPECT_DOUBLE_EQ(got[0], 3.0);  // numel(zeros(1,3))
    EXPECT_DOUBLE_EQ(got[1], 0.0);  // numel(zeros(1,-1)) = numel(1x0) = 0 (clamped, no throw)

    // Cross-check: the interpreter clamps identically (codegen == interpreter).
    numkit::StandardEngine engine;
    EXPECT_DOUBLE_EQ(engine.eval("numel(zeros(1,-1))", true).toScalar(), 0.0);
}

// Native elementwise array MATH end-to-end: y = sin(x) lowers to a std::sin
// loop — SELF-CONTAINED (no runtime DLL, plain stdlib exe) — and matches
// std::sin. The win over bridging: no boxing, no runtime dependency.
TEST(CodegenE2E, ElementwiseArrayMathRunsCorrectly)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const EmittedFunction emitted = transpile(
        "function y = f(x)\n  y = sin(x);\nend\n",
        {{"x", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}});
    ASSERT_NE(emitted.source.find("std::sin("), std::string::npos);
    ASSERT_EQ(emitted.source.find("nk_codegen_rt.h"), std::string::npos);  // self-contained!

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_ewise_math_e2e.exe").string();
    const std::string outTxt = (base / "nk_ewise_math_e2e_out.txt").string();
    const std::size_t N       = 8;
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  const std::size_t N = 8;\n"
        "  double x[8], y[8];\n"
        "  for (std::size_t i = 0; i < N; ++i) x[i] = 0.3 * double(i + 1);\n"
        "  f(x, N, y, N);\n"
        "  std::FILE* g = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!g) return 2;\n"
        "  for (std::size_t i = 0; i < N; ++i) std::fprintf(g, \"%.17g\\n\", y[i]);\n"
        "  std::fclose(g); return 0;\n}\n";
    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);  // no rt link
    ASSERT_EQ(got.size(), N);
    for (std::size_t i = 0; i < N; ++i)
        EXPECT_NEAR(got[i], std::sin(0.3 * double(i + 1)), 1e-12) << "at i=" << i;
}

// CX1: complex scalar accessors end-to-end (real/imag/angle/conj/abs), SELF-
// CONTAINED (std::complex, no runtime). r = real(conj(x))*100 + imag(conj(x))*10
// + abs(x). x = 3+4i -> conj = 3-4i -> 3*100 + (-4)*10 + 5 = 265.
TEST(CodegenE2E, ComplexAccessorsRunCorrectly)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const EmittedFunction emitted = transpile(
        "function r = f(x)\n  r = real(conj(x))*100 + imag(conj(x))*10 + abs(x);\nend\n",
        {{"x", InferredType::scalar(ValueType::COMPLEX)}});
    ASSERT_NE(emitted.source.find("std::conj("), std::string::npos);
    ASSERT_EQ(emitted.source.find("nk_codegen_rt.h"), std::string::npos);  // self-contained

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_cxaccess_e2e.exe").string();
    const std::string outTxt = (base / "nk_cxaccess_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double r = " + emitted.name + "(std::complex<double>(3.0, 4.0));\n"
        "  std::FILE* g = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!g) return 2;\n"
        "  std::fprintf(g, \"%.17g\\n\", r);\n"
        "  std::fclose(g); return 0;\n}\n";
    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 265.0);  // 3*100 + (-4)*10 + 5
}

// CX2/CX3: complex 1-D array elementwise end-to-end, SELF-CONTAINED (std::complex
// buffers, no runtime). y = x .* 2 over x = [1+1i, 2+2i, 3+3i] -> [2+2i,4+4i,6+6i].
// Exercises the complex buffer ABI (param in + out-param out) and the lifted
// elementwise loop.
TEST(CodegenE2E, ComplexElementwiseRunsCorrectly)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const EmittedFunction emitted = transpile(
        "function y = f(x)\n  y = x .* 2;\nend\n",
        {{"x", InferredType::concrete(ValueType::COMPLEX, Shape::rowVector())}});
    ASSERT_NE(emitted.source.find("std::complex<double>* __restrict y"), std::string::npos);
    ASSERT_EQ(emitted.source.find("nk_codegen_rt.h"), std::string::npos);  // self-contained

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_cxewise_e2e.exe").string();
    const std::string outTxt = (base / "nk_cxewise_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  std::complex<double> x[3] = { {1,1}, {2,2}, {3,3} }, y[3];\n"
        "  " + emitted.name + "(x, 3, y, 3);\n"
        "  std::FILE* g = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!g) return 2;\n"
        "  for (int i = 0; i < 3; ++i) std::fprintf(g, \"%.17g\\n%.17g\\n\", y[i].real(), y[i].imag());\n"
        "  std::fclose(g); return 0;\n}\n";
    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 6u);  // re,im per element
    const double exp[6] = {2, 2, 4, 4, 6, 6};  // x .* 2
    for (int i = 0; i < 6; ++i)
        EXPECT_DOUBLE_EQ(got[i], exp[i]) << "at " << i;
}

// Vector transpose end-to-end. y = x.' on a real row vector copies the data
// (orientation flips, observable via size). Self-contained (no bridge).
TEST(CodegenE2E, RealVectorTransposeRunsCorrectly)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const EmittedFunction emitted = transpile(
        "function y = f(x)\n  y = x.';\nend\n",
        {{"x", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}});
    ASSERT_NE(emitted.source.find("y[_nk_i] = x[_nk_i];"), std::string::npos);
    ASSERT_EQ(emitted.source.find("nk_codegen_rt.h"), std::string::npos);  // self-contained

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_transp_e2e.exe").string();
    const std::string outTxt = (base / "nk_transp_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double x[3] = {10, 20, 30}, y[3];\n"
        "  " + emitted.name + "(x, 3, y, 3);\n"
        "  std::FILE* g = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!g) return 2;\n"
        "  for (int i = 0; i < 3; ++i) std::fprintf(g, \"%.17g\\n\", y[i]);\n"
        "  std::fclose(g); return 0;\n}\n";
    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 3u);
    const double exp[3] = {10, 20, 30};
    for (int i = 0; i < 3; ++i) EXPECT_DOUBLE_EQ(got[i], exp[i]) << "at " << i;
}

// Conjugate transpose end-to-end: y = x' on a complex vector conjugates each
// element (the ctranspose ' distinguishes itself from .' here). x = [1+2i, 3+4i]
// -> y = [1-2i, 3-4i].
TEST(CodegenE2E, ComplexConjTransposeRunsCorrectly)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const EmittedFunction emitted = transpile(
        "function y = f(x)\n  y = x';\nend\n",
        {{"x", InferredType::concrete(ValueType::COMPLEX, Shape::rowVector())}});
    ASSERT_NE(emitted.source.find("y[_nk_i] = std::conj(x[_nk_i]);"), std::string::npos);

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_conjtransp_e2e.exe").string();
    const std::string outTxt = (base / "nk_conjtransp_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  std::complex<double> x[2] = { {1,2}, {3,4} }, y[2];\n"
        "  " + emitted.name + "(x, 2, y, 2);\n"
        "  std::FILE* g = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!g) return 2;\n"
        "  for (int i = 0; i < 2; ++i) std::fprintf(g, \"%.17g\\n%.17g\\n\", y[i].real(), y[i].imag());\n"
        "  std::fclose(g); return 0;\n}\n";
    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 4u);
    const double exp[4] = {1, -2, 3, -4};  // conj([1+2i, 3+4i])
    for (int i = 0; i < 4; ++i) EXPECT_DOUBLE_EQ(got[i], exp[i]) << "at " << i;
}

// 2-D matrix transpose end-to-end: y = A.' swaps the dims, column-major. A is
// 2x3 stored column-major {1,2,3,4,5,6} = [[1,3,5],[2,4,6]]; A.' is 3x2 =
// [[1,2],[3,4],[5,6]] stored {1,3,5,2,4,6}. Validates the column-major index
// swap and the 2-D caller-allocated out-param.
TEST(CodegenE2E, MatrixTransposeRunsCorrectly)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const EmittedFunction emitted = transpile(
        "function y = f(A)\n  y = A.';\nend\n",
        {{"A", InferredType::concrete(ValueType::DOUBLE, Shape::dims(2, 3))}});
    ASSERT_NE(emitted.source.find("[_nk_i + _nk_j * _nk_y_rows] = A[_nk_j + _nk_i * _nk_A_rows];"),
              std::string::npos);

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_mattransp_e2e.exe").string();
    const std::string outTxt = (base / "nk_mattransp_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double A[6] = {1, 2, 3, 4, 5, 6};\n"  // 2x3 column-major
        "  double y[6];\n"
        "  " + emitted.name + "(A, 2, 3, y, 3, 2);\n"  // A is 2x3, y is 3x2
        "  std::FILE* g = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!g) return 2;\n"
        "  for (int i = 0; i < 6; ++i) std::fprintf(g, \"%.17g\\n\", y[i]);\n"
        "  std::fclose(g); return 0;\n}\n";
    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 6u);
    const double exp[6] = {1, 3, 5, 2, 4, 6};  // (A.') column-major
    for (int i = 0; i < 6; ++i) EXPECT_DOUBLE_EQ(got[i], exp[i]) << "at " << i;
}

// Matrix product end-to-end: C = A * B. A (2x3) col-major {1,2,3,4,5,6} =
// [[1,3,5],[2,4,6]]; B (3x2) col-major {1,2,3,4,5,6} = [[1,4],[2,5],[3,6]];
// C = A*B (2x2) = [[22,49],[28,64]], col-major {22,28,49,64}. Validates the
// column-major triple loop + the shared-dim guard.
TEST(CodegenE2E, MatrixProductRunsCorrectly)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const EmittedFunction emitted = transpile(
        "function C = f(A, B)\n  C = A * B;\nend\n",
        {{"A", InferredType::concrete(ValueType::DOUBLE, Shape::dims(2, 3))},
         {"B", InferredType::concrete(ValueType::DOUBLE, Shape::dims(3, 2))}});
    ASSERT_NE(emitted.source.find("_nk_acc += "), std::string::npos);
    ASSERT_EQ(emitted.source.find("nk_codegen_rt.h"), std::string::npos);  // self-contained

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_matmul_e2e.exe").string();
    const std::string outTxt = (base / "nk_matmul_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double A[6] = {1, 2, 3, 4, 5, 6};\n"  // 2x3 col-major
        "  double B[6] = {1, 2, 3, 4, 5, 6};\n"  // 3x2 col-major
        "  double C[4];\n"
        "  " + emitted.name + "(A, 2, 3, B, 3, 2, C, 2, 2);\n"
        "  std::FILE* g = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!g) return 2;\n"
        "  for (int i = 0; i < 4; ++i) std::fprintf(g, \"%.17g\\n\", C[i]);\n"
        "  std::fclose(g); return 0;\n}\n";
    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 4u);
    const double exp[4] = {22, 28, 49, 64};  // (A*B) column-major
    for (int i = 0; i < 4; ++i) EXPECT_DOUBLE_EQ(got[i], exp[i]) << "at " << i;
}

// Matrix * column vector end-to-end: y = A*x. A (2x3) col-major {1,2,3,4,5,6} =
// [[1,3,5],[2,4,6]]; x = [1;2;3]; y = A*x = [22;28].
TEST(CodegenE2E, MatrixTimesVectorRunsCorrectly)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const EmittedFunction emitted = transpile(
        "function y = f(A, x)\n  y = A * x;\nend\n",
        {{"A", InferredType::concrete(ValueType::DOUBLE, Shape::dims(2, 3))},
         {"x", InferredType::concrete(ValueType::DOUBLE, Shape::colVector())}});
    ASSERT_NE(emitted.source.find("_nk_acc += A[_nk_i + _nk_l * _nk_A_rows] * x[_nk_l];"),
              std::string::npos);
    ASSERT_EQ(emitted.source.find("nk_codegen_rt.h"), std::string::npos);  // self-contained

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_matvec_e2e.exe").string();
    const std::string outTxt = (base / "nk_matvec_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double A[6] = {1, 2, 3, 4, 5, 6};\n"  // 2x3 col-major
        "  double x[3] = {1, 2, 3}, y[2];\n"
        "  " + emitted.name + "(A, 2, 3, x, 3, y, 2);\n"
        "  std::FILE* g = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!g) return 2;\n"
        "  for (int i = 0; i < 2; ++i) std::fprintf(g, \"%.17g\\n\", y[i]);\n"
        "  std::fclose(g); return 0;\n}\n";
    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 2u);
    EXPECT_DOUBLE_EQ(got[0], 22.0);
    EXPECT_DOUBLE_EQ(got[1], 28.0);
}

// Row vector * matrix end-to-end: y = r*A. r = [1 2 3]; A (3x2) col-major
// {1,2,3,4,5,6} = [[1,4],[2,5],[3,6]]; y = r*A = [14 32].
TEST(CodegenE2E, VectorTimesMatrixRunsCorrectly)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const EmittedFunction emitted = transpile(
        "function y = f(r, A)\n  y = r * A;\nend\n",
        {{"r", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())},
         {"A", InferredType::concrete(ValueType::DOUBLE, Shape::dims(3, 2))}});
    ASSERT_NE(emitted.source.find("_nk_acc += r[_nk_l] * A[_nk_l + _nk_j * _nk_A_rows];"),
              std::string::npos);

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_vecmat_e2e.exe").string();
    const std::string outTxt = (base / "nk_vecmat_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double r[3] = {1, 2, 3};\n"
        "  double A[6] = {1, 2, 3, 4, 5, 6};\n"  // 3x2 col-major
        "  double y[2];\n"
        "  " + emitted.name + "(r, 3, A, 3, 2, y, 2);\n"
        "  std::FILE* g = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!g) return 2;\n"
        "  for (int i = 0; i < 2; ++i) std::fprintf(g, \"%.17g\\n\", y[i]);\n"
        "  std::fclose(g); return 0;\n}\n";
    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 2u);
    EXPECT_DOUBLE_EQ(got[0], 14.0);
    EXPECT_DOUBLE_EQ(got[1], 32.0);
}

// Inner / dot product end-to-end: s = r*c, r=[1 2 3] (row), c=[4;5;6] (col) ->
// 1*4 + 2*5 + 3*6 = 32. A scalar reduction; the function returns the scalar.
TEST(CodegenE2E, InnerProductRunsCorrectly)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const EmittedFunction emitted = transpile(
        "function s = f(r, c)\n  s = r * c;\nend\n",
        {{"r", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())},
         {"c", InferredType::concrete(ValueType::DOUBLE, Shape::colVector())}});
    ASSERT_NE(emitted.source.find("_nk_acc += r[_nk_l] * c[_nk_l];"), std::string::npos);
    ASSERT_EQ(emitted.source.find("nk_codegen_rt.h"), std::string::npos);  // self-contained

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_dot_e2e.exe").string();
    const std::string outTxt = (base / "nk_dot_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double r[3] = {1, 2, 3}, c[3] = {4, 5, 6};\n"
        "  double s = " + emitted.name + "(r, 3, c, 3);\n"
        "  std::FILE* g = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!g) return 2;\n"
        "  std::fprintf(g, \"%.17g\\n\", s);\n"
        "  std::fclose(g); return 0;\n}\n";
    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 32.0);  // 1*4 + 2*5 + 3*6
}

// Outer product end-to-end: y = c*r (col * row) -> an m x n matrix. c=[1;2;3],
// r=[10 20] -> y is 3x2, column-major {10,20,30, 20,40,60}. Completes the *
// operator (scalar-scale, A*B, A*x, r*A, r*c, c*r).
TEST(CodegenE2E, OuterProductRunsCorrectly)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const EmittedFunction emitted = transpile(
        "function y = f(c, r)\n  y = c * r;\nend\n",
        {{"c", InferredType::concrete(ValueType::DOUBLE, Shape::colVector())},
         {"r", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}});
    ASSERT_NE(emitted.source.find("y[_nk_i + _nk_j * _nk_y_d0] = c[_nk_i] * r[_nk_j];"),
              std::string::npos);

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_outer_e2e.exe").string();
    const std::string outTxt = (base / "nk_outer_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double c[3] = {1, 2, 3}, r[2] = {10, 20}, y[6];\n"
        "  f(c, 3, r, 2, y, 3, 2);\n"  // c len 3, r len 2, y is 3x2
        "  std::FILE* g = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!g) return 2;\n"
        "  for (int i = 0; i < 6; ++i) std::fprintf(g, \"%.17g\\n\", y[i]);\n"
        "  std::fclose(g); return 0;\n}\n";
    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 6u);
    const double exp[6] = {10, 20, 30, 20, 40, 60};  // (c*r) column-major
    for (int i = 0; i < 6; ++i) EXPECT_DOUBLE_EQ(got[i], exp[i]) << "at " << i;
}

// MULTI-array elementwise: y = x + w .* 2 (two array operands) -> a length
// guard + per-element loop. y[i] = x[i] + w[i]*2.
TEST(CodegenE2E, MultiArrayElementwiseRunsCorrectly)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const EmittedFunction emitted = transpile(
        "function y = f(x, w)\n  y = x + w .* 2;\nend\n",
        {{"x", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())},
         {"w", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}});
    ASSERT_NE(emitted.source.find("array dimensions must match"), std::string::npos);  // guard

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_multiewise_e2e.exe").string();
    const std::string outTxt = (base / "nk_multiewise_e2e_out.txt").string();
    const std::size_t N       = 10;
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  const std::size_t N = 10;\n"
        "  double x[10], w[10], y[10];\n"
        "  for (std::size_t i = 0; i < N; ++i) { x[i] = double(i + 1); w[i] = 10.0; }\n"
        "  f(x, N, w, N, y, N);\n"
        "  std::FILE* g = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!g) return 2;\n"
        "  for (std::size_t i = 0; i < N; ++i) std::fprintf(g, \"%.17g\\n\", y[i]);\n"
        "  std::fclose(g); return 0;\n}\n";
    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), N);
    for (std::size_t i = 0; i < N; ++i)
        EXPECT_DOUBLE_EQ(got[i], double(i + 1) + 20.0) << "at i=" << i;  // x[i] + w[i]*2
}

// BRIDGED e2e (DESIGN.md §6a brick 4): a program calling a builtin the emitter
// cannot lower (`sign`) compiles in BRIDGED mode, links the nk_codegen_rt
// shared lib, RUNS, and matches the interpreter. Proves the C-ABI bridge end
// to end: generated native code -> runtime DLL -> result. The opaque handle
// design keeps all Value alloc/free inside the DLL (no cross-module heap).
TEST(CodegenBridge, BridgedScalarCallRunsAndMatchesInterpreter)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    TransferRegistry reg;
    registerStandardTransfers(reg);
    numkit::Lexer          lex("function y = f(x)\n  y = sign(x);\nend\n");
    numkit::Parser         parser(lex.tokenize());
    auto                   root = parser.parse();
    const numkit::ASTNode *fn   = nullptr;
    for (const auto &c : root->children)
        if (c && c->type == numkit::NodeType::FUNCTION_DEF) fn = c.get();
    ASSERT_NE(fn, nullptr);

    BridgeOptions bridge;
    bridge.enabled       = true;
    bridge.runtimeHeader = "nk_codegen_rt.h";  // resolved via the -I below
    const EmittedFunction emitted =
        emitFunction(*fn, {{"x", InferredType::scalar(ValueType::DOUBLE)}}, reg, nullptr, bridge);
    ASSERT_NE(emitted.source.find("nk_rt::bridge_scalar(\"sign\""), std::string::npos);

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_bridged_e2e.exe").string();
    const std::string outTxt = (base / "nk_bridged_e2e_out.txt").string();
    std::error_code   ec;
    std::filesystem::remove(outTxt, ec);

    std::string program = emitted.source;
    program +=
        "#include <cstdio>\n"
        "int main() {\n"
        "  const double xs[3] = {-3.5, 0.0, 2.75};\n"
        "  std::FILE* g = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!g) return 2;\n"
        "  for (int i = 0; i < 3; ++i) std::fprintf(g, \"%.17g\\n\", f(xs[i]));\n"
        "  std::fclose(g); return 0;\n}\n";

    // Bridged compile: find nk_codegen_rt.h, request dllimport linkage, and
    // link the runtime import lib.
    aot::CompileOptions opts;
    opts.includeDirs = {NK_BRIDGE_DIR};
    opts.defines     = {"NK_RT_USE_DLL"};
    opts.linkLibs    = {NK_RT_IMPORT_LIB};
    const auto r = aot::compileToExecutable(program, exe, opts);
    ASSERT_EQ(r.status, aot::CompileStatus::Ok)
        << "log:\n" << r.log << "\n--- generated source ---\n" << program;

    // The runtime DLL must sit beside the artifact so it loads at run time.
    std::filesystem::copy_file(NK_RT_SHARED_DLL, base / "nk_codegen_rt.dll",
                               std::filesystem::copy_options::overwrite_existing, ec);
    ASSERT_FALSE(ec) << "copy nk_codegen_rt.dll: " << ec.message();

    ASSERT_EQ(std::system(("\"" + exe + "\"").c_str()), 0);

    std::vector<double> got;
    {
        std::ifstream is(outTxt);
        double        v;
        while (is >> v) got.push_back(v);
    }
    ASSERT_EQ(got.size(), 3u);

    // Reference: the interpreter's sign over the same inputs.
    numkit::StandardEngine engine;
    EXPECT_DOUBLE_EQ(got[0], engine.eval("sign(-3.5)", true).toScalar());  // -1
    EXPECT_DOUBLE_EQ(got[1], engine.eval("sign(0)", true).toScalar());     //  0
    EXPECT_DOUBLE_EQ(got[2], engine.eval("sign(2.75)", true).toScalar());  //  1
    EXPECT_DOUBLE_EQ(got[0], -1.0);
    EXPECT_DOUBLE_EQ(got[1], 0.0);
    EXPECT_DOUBLE_EQ(got[2], 1.0);
}

// DYNAMIC TIER end-to-end (DESIGN.md §10 C1): an un-typeable value stays BOXED
// (nk_rt::val) and its operations dispatch to the runtime. `mod` has no codegen
// transfer -> z is Dynamic; a Value-tier comparison steers a typed branch (the
// condition `truth()` sink), so the result is a typed double the RawBuffer ABI
// returns. Compiles -> runs -> links the runtime DLL -> matches the interpreter.
TEST(CodegenBridge, DynamicScalarTierRunsAndMatchesInterpreter)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    TransferRegistry reg;
    registerStandardTransfers(reg);
    numkit::Lexer          lex("function y = f(x)\n"
                               "  z = mod(x, 3);\n"   // no transfer -> z Dynamic (boxed)
                               "  w = z;\n"            // Dynamic-to-Dynamic copy (val clone)
                               "  if w > 1.5\n"        // Value-tier comparison -> truth sink
                               "    y = 10.0;\n"
                               "  else\n"
                               "    y = 20.0;\n"
                               "  end\n"
                               "end\n");
    numkit::Parser         parser(lex.tokenize());
    auto                   root = parser.parse();
    const numkit::ASTNode *fn   = nullptr;
    for (const auto &c : root->children)
        if (c && c->type == numkit::NodeType::FUNCTION_DEF) fn = c.get();
    ASSERT_NE(fn, nullptr);

    BridgeOptions bridge;
    bridge.enabled       = true;
    bridge.runtimeHeader = "nk_codegen_rt.h";
    const EmittedFunction emitted =
        emitFunction(*fn, {{"x", InferredType::scalar(ValueType::DOUBLE)}}, reg, nullptr, bridge);
    ASSERT_NE(emitted.source.find("nk_rt::call_dyn(\"mod\""), std::string::npos);
    ASSERT_NE(emitted.source.find(".truth()"), std::string::npos);

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_dyntier_e2e.exe").string();
    const std::string outTxt = (base / "nk_dyntier_e2e_out.txt").string();
    std::error_code   ec;
    std::filesystem::remove(outTxt, ec);

    std::string program = emitted.source;
    program +=
        "#include <cstdio>\n"
        "int main() {\n"
        "  const double xs[4] = {5.0, 3.0, 8.0, 2.0};\n"  // mod(.,3)=2,0,2,2
        "  std::FILE* g = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!g) return 2;\n"
        "  for (int i = 0; i < 4; ++i) std::fprintf(g, \"%.17g\\n\", f(xs[i]));\n"
        "  std::fclose(g); return 0;\n}\n";

    aot::CompileOptions opts;
    opts.includeDirs = {NK_BRIDGE_DIR};
    opts.defines     = {"NK_RT_USE_DLL"};
    opts.linkLibs    = {NK_RT_IMPORT_LIB};
    const auto r = aot::compileToExecutable(program, exe, opts);
    ASSERT_EQ(r.status, aot::CompileStatus::Ok)
        << "log:\n" << r.log << "\n--- generated source ---\n" << program;

    std::filesystem::copy_file(NK_RT_SHARED_DLL, base / "nk_codegen_rt.dll",
                               std::filesystem::copy_options::overwrite_existing, ec);
    ASSERT_FALSE(ec) << "copy nk_codegen_rt.dll: " << ec.message();

    ASSERT_EQ(std::system(("\"" + exe + "\"").c_str()), 0);

    std::vector<double> got;
    {
        std::ifstream is(outTxt);
        double        v;
        while (is >> v) got.push_back(v);
    }
    ASSERT_EQ(got.size(), 4u);
    EXPECT_DOUBLE_EQ(got[0], 10.0);  // mod(5,3)=2 > 1.5 -> 10
    EXPECT_DOUBLE_EQ(got[1], 20.0);  // mod(3,3)=0      -> 20
    EXPECT_DOUBLE_EQ(got[2], 10.0);  // mod(8,3)=2      -> 10
    EXPECT_DOUBLE_EQ(got[3], 10.0);  // mod(2,3)=2      -> 10

    // Cross-check: the interpreter computes the same (mod + the same branch).
    numkit::StandardEngine engine;
    EXPECT_DOUBLE_EQ(
        engine.eval("z=mod(5,3); y=20; if z>1.5, y=10; end; y", true).toScalar(), got[0]);
}

// try / catch (control-flow coverage) under the bridge. The try/catch-assigned
// var z is Dynamic (markAssignedDynamic — a throw mid-try leaves it uncertain),
// so every assignment to it boxes (the Dynamic-hoisted-local discipline: there is
// no nk_val = <typed>); a Dynamic condition sink then yields a typed output.
// Compiles, links the runtime DLL, runs, matches the interpreter.
TEST(CodegenBridge, TryCatchRunsAndMatchesInterpreter)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    TransferRegistry reg;
    registerStandardTransfers(reg);
    numkit::Lexer  lex("function y = f(n)\n"
                       "  z = 0;\n"
                       "  try\n"
                       "    z = n + 1;\n"
                       "  catch\n"
                       "    z = -1;\n"
                       "  end\n"
                       "  if z > 3.5\n"
                       "    y = 10;\n"
                       "  else\n"
                       "    y = 20;\n"
                       "  end\n"
                       "end\n");
    numkit::Parser parser(lex.tokenize());
    auto           root = parser.parse();
    const numkit::ASTNode *fn = nullptr;
    for (const auto &c : root->children)
        if (c && c->type == numkit::NodeType::FUNCTION_DEF) fn = c.get();
    ASSERT_NE(fn, nullptr);

    BridgeOptions bridge;
    bridge.enabled       = true;
    bridge.runtimeHeader = "nk_codegen_rt.h";
    const EmittedFunction emitted =
        emitFunction(*fn, {{"n", InferredType::scalar(ValueType::DOUBLE)}}, reg, nullptr, bridge);
    ASSERT_NE(emitted.source.find("try {"), std::string::npos);
    ASSERT_NE(emitted.source.find("catch (...)"), std::string::npos);

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_trycatch_e2e.exe").string();
    const std::string outTxt = (base / "nk_trycatch_e2e_out.txt").string();
    std::error_code   ec;
    std::filesystem::remove(outTxt, ec);

    std::string program = emitted.source;
    program +=
        "#include <cstdio>\n"
        "int main() {\n"
        "  const double xs[2] = {5.0, 1.0};\n"  // z=6 -> 10 ; z=2 -> 20
        "  std::FILE* g = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!g) return 2;\n"
        "  for (int i = 0; i < 2; ++i) std::fprintf(g, \"%.17g\\n\", f(xs[i]));\n"
        "  std::fclose(g); return 0;\n}\n";

    aot::CompileOptions opts;
    opts.includeDirs = {NK_BRIDGE_DIR};
    opts.defines     = {"NK_RT_USE_DLL"};
    opts.linkLibs    = {NK_RT_IMPORT_LIB};
    const auto r = aot::compileToExecutable(program, exe, opts);
    ASSERT_EQ(r.status, aot::CompileStatus::Ok)
        << "log:\n" << r.log << "\n--- generated source ---\n" << program;

    std::filesystem::copy_file(NK_RT_SHARED_DLL, base / "nk_codegen_rt.dll",
                               std::filesystem::copy_options::overwrite_existing, ec);
    ASSERT_FALSE(ec) << "copy nk_codegen_rt.dll: " << ec.message();
    ASSERT_EQ(std::system(("\"" + exe + "\"").c_str()), 0);

    std::vector<double> got;
    {
        std::ifstream is(outTxt);
        double        v;
        while (is >> v) got.push_back(v);
    }
    ASSERT_EQ(got.size(), 2u);
    EXPECT_DOUBLE_EQ(got[0], 10.0);  // try z = 6 > 3.5 -> 10
    EXPECT_DOUBLE_EQ(got[1], 20.0);  // try z = 2 <= 3.5 -> 20
}

// try/catch with a bound exception variable (`catch err`) is refused: MATLAB
// binds an MException object, not represented in v1 (emit-level).
TEST(CodegenE2E, TryCatchBoundVarRefused)
{
    TransferRegistry reg;
    registerStandardTransfers(reg);
    numkit::Lexer  lex("function y = f(n)\n  try\n    y = n + 1;\n"
                       "  catch err\n    y = -1;\n  end\nend\n");
    numkit::Parser parser(lex.tokenize());
    auto           root = parser.parse();
    const numkit::ASTNode *fn = nullptr;
    for (const auto &c : root->children)
        if (c && c->type == numkit::NodeType::FUNCTION_DEF) fn = c.get();
    ASSERT_NE(fn, nullptr);
    BridgeOptions bridge;
    bridge.enabled = true;
    EXPECT_THROW(
        emitFunction(*fn, {{"n", InferredType::scalar(ValueType::DOUBLE)}}, reg, nullptr, bridge),
        std::runtime_error);
}

// DYNAMIC CALL ARGUMENT (DESIGN.md §10 C1, A3): a Dynamic value passed AS an
// argument to another call. mod(x,3) is Dynamic (z); mod(z,2) passes the boxed
// z -> call_dynv (a val argument list), result Dynamic again; a condition sink
// returns a typed double. Compiles, links the runtime DLL, runs, matches.
TEST(CodegenBridge, DynamicCallArgRunsAndMatchesInterpreter)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    TransferRegistry reg;
    registerStandardTransfers(reg);
    numkit::Lexer          lex("function y = f(x)\n"
                               "  z = mod(x, 3);\n"   // Dynamic
                               "  w = mod(z, 2);\n"   // Dynamic ARG z -> call_dynv
                               "  if w > 0.5\n"
                               "    y = 1.0;\n"
                               "  else\n"
                               "    y = 0.0;\n"
                               "  end\n"
                               "end\n");
    numkit::Parser         parser(lex.tokenize());
    auto                   root = parser.parse();
    const numkit::ASTNode *fn   = nullptr;
    for (const auto &c : root->children)
        if (c && c->type == numkit::NodeType::FUNCTION_DEF) fn = c.get();
    ASSERT_NE(fn, nullptr);

    BridgeOptions bridge;
    bridge.enabled       = true;
    bridge.runtimeHeader = "nk_codegen_rt.h";
    const EmittedFunction emitted =
        emitFunction(*fn, {{"x", InferredType::scalar(ValueType::DOUBLE)}}, reg, nullptr, bridge);
    ASSERT_NE(emitted.source.find("nk_rt::call_dyn(\"mod\", {x, 3.0})"), std::string::npos);
    ASSERT_NE(emitted.source.find("nk_rt::call_dynv(\"mod\", {z, nk_rt::val::scalar(2.0)})"),
              std::string::npos);

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_dynarg_e2e.exe").string();
    const std::string outTxt = (base / "nk_dynarg_e2e_out.txt").string();
    std::error_code   ec;
    std::filesystem::remove(outTxt, ec);

    std::string program = emitted.source;
    program +=
        "#include <cstdio>\n"
        "int main() {\n"
        "  const double xs[4] = {5.0, 7.0, 4.0, 6.0};\n"
        "  std::FILE* g = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!g) return 2;\n"
        "  for (int i = 0; i < 4; ++i) std::fprintf(g, \"%.17g\\n\", f(xs[i]));\n"
        "  std::fclose(g); return 0;\n}\n";

    aot::CompileOptions opts;
    opts.includeDirs = {NK_BRIDGE_DIR};
    opts.defines     = {"NK_RT_USE_DLL"};
    opts.linkLibs    = {NK_RT_IMPORT_LIB};
    const auto r = aot::compileToExecutable(program, exe, opts);
    ASSERT_EQ(r.status, aot::CompileStatus::Ok)
        << "log:\n" << r.log << "\n--- generated source ---\n" << program;

    std::filesystem::copy_file(NK_RT_SHARED_DLL, base / "nk_codegen_rt.dll",
                               std::filesystem::copy_options::overwrite_existing, ec);
    ASSERT_FALSE(ec) << "copy nk_codegen_rt.dll: " << ec.message();
    ASSERT_EQ(std::system(("\"" + exe + "\"").c_str()), 0);

    std::vector<double> got;
    {
        std::ifstream is(outTxt);
        double        v;
        while (is >> v) got.push_back(v);
    }
    ASSERT_EQ(got.size(), 4u);
    EXPECT_DOUBLE_EQ(got[0], 0.0);  // mod(mod(5,3),2)=mod(2,2)=0
    EXPECT_DOUBLE_EQ(got[1], 1.0);  // mod(mod(7,3),2)=mod(1,2)=1
    EXPECT_DOUBLE_EQ(got[2], 1.0);  // mod(mod(4,3),2)=mod(1,2)=1
    EXPECT_DOUBLE_EQ(got[3], 0.0);  // mod(mod(6,3),2)=mod(0,2)=0
}

// BOXED DYNAMIC RETURN (DESIGN.md §10 C1, keystone): an un-typeable RESULT
// crosses the function boundary BOXED (the function returns nk_val). y=mod(x,3)
// is Dynamic, so f returns an owned nk_val the caller unboxes — the path for a
// Dynamic VALUE itself to reach an output (vs only its typed consequences). The
// caller owns the handle; main() unboxes + releases it.
TEST(CodegenBridge, DynamicBoxedReturnRunsAndMatchesInterpreter)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    TransferRegistry reg;
    registerStandardTransfers(reg);
    numkit::Lexer          lex("function y = f(x)\n  y = mod(x, 3);\nend\n");  // y Dynamic
    numkit::Parser         parser(lex.tokenize());
    auto                   root = parser.parse();
    const numkit::ASTNode *fn   = nullptr;
    for (const auto &c : root->children)
        if (c && c->type == numkit::NodeType::FUNCTION_DEF) fn = c.get();
    ASSERT_NE(fn, nullptr);

    BridgeOptions bridge;
    bridge.enabled       = true;
    bridge.runtimeHeader = "nk_codegen_rt.h";
    const EmittedFunction emitted =
        emitFunction(*fn, {{"x", InferredType::scalar(ValueType::DOUBLE)}}, reg, nullptr, bridge);
    ASSERT_NE(emitted.source.find("nk_val f("), std::string::npos);   // boxed return signature
    ASSERT_NE(emitted.source.find("return y.take();"), std::string::npos);

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_dynret_e2e.exe").string();
    const std::string outTxt = (base / "nk_dynret_e2e_out.txt").string();
    std::error_code   ec;
    std::filesystem::remove(outTxt, ec);

    std::string program = emitted.source;
    program +=
        "#include <cstdio>\n"
        "int main() {\n"
        "  const double xs[4] = {5.0, 7.0, 4.0, 9.0};\n"  // mod(.,3) = 2,1,1,0
        "  std::FILE* g = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!g) return 2;\n"
        "  for (int i = 0; i < 4; ++i) {\n"
        "    nk_val r = f(xs[i]);\n"             // owned boxed result
        "    std::fprintf(g, \"%.17g\\n\", nk_unbox_scalar(r));\n"
        "    nk_release(r);\n"
        "  }\n"
        "  std::fclose(g); return 0;\n}\n";

    aot::CompileOptions opts;
    opts.includeDirs = {NK_BRIDGE_DIR};
    opts.defines     = {"NK_RT_USE_DLL"};
    opts.linkLibs    = {NK_RT_IMPORT_LIB};
    const auto r = aot::compileToExecutable(program, exe, opts);
    ASSERT_EQ(r.status, aot::CompileStatus::Ok)
        << "log:\n" << r.log << "\n--- generated source ---\n" << program;

    std::filesystem::copy_file(NK_RT_SHARED_DLL, base / "nk_codegen_rt.dll",
                               std::filesystem::copy_options::overwrite_existing, ec);
    ASSERT_FALSE(ec) << "copy nk_codegen_rt.dll: " << ec.message();
    ASSERT_EQ(std::system(("\"" + exe + "\"").c_str()), 0);

    std::vector<double> got;
    {
        std::ifstream is(outTxt);
        double        v;
        while (is >> v) got.push_back(v);
    }
    ASSERT_EQ(got.size(), 4u);
    EXPECT_DOUBLE_EQ(got[0], 2.0);  // mod(5,3)
    EXPECT_DOUBLE_EQ(got[1], 1.0);  // mod(7,3)
    EXPECT_DOUBLE_EQ(got[2], 1.0);  // mod(4,3)
    EXPECT_DOUBLE_EQ(got[3], 0.0);  // mod(9,3)
}

// DYNAMIC ARRAY (DESIGN.md §10 C1, A4): an un-typeable ARRAY value. mod over a
// 1-D array arg has no transfer -> y is a Dynamic ARRAY; the array argument is
// boxed at the boundary (val::array), the call dispatches to the runtime
// (call_dynv), and the Dynamic array crosses the boundary via the boxed return
// (A3.5). Compiles, links the runtime DLL, runs, matches the interpreter.
TEST(CodegenBridge, DynamicArrayArgAndReturnRunsAndMatchesInterpreter)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    TransferRegistry reg;
    registerStandardTransfers(reg);
    numkit::Lexer          lex("function y = f(x)\n  y = mod(x, 3);\nend\n");  // x ARRAY -> y Dynamic array
    numkit::Parser         parser(lex.tokenize());
    auto                   root = parser.parse();
    const numkit::ASTNode *fn   = nullptr;
    for (const auto &c : root->children)
        if (c && c->type == numkit::NodeType::FUNCTION_DEF) fn = c.get();
    ASSERT_NE(fn, nullptr);

    BridgeOptions bridge;
    bridge.enabled       = true;
    bridge.runtimeHeader = "nk_codegen_rt.h";
    const EmittedFunction emitted = emitFunction(
        *fn, {{"x", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}}, reg, nullptr,
        bridge);
    ASSERT_NE(emitted.source.find("nk_rt::val::array(x, _nk_x_len)"), std::string::npos);
    ASSERT_NE(emitted.source.find("nk_rt::call_dynv(\"mod\""), std::string::npos);
    ASSERT_NE(emitted.source.find("return y.take();"), std::string::npos);

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_dynarr_e2e.exe").string();
    const std::string outTxt = (base / "nk_dynarr_e2e_out.txt").string();
    std::error_code   ec;
    std::filesystem::remove(outTxt, ec);

    std::string program = emitted.source;
    program +=
        "#include <cstdio>\n"
        "int main() {\n"
        "  const double xs[4] = {5.0, 7.0, 4.0, 9.0};\n"  // mod(.,3) = 2,1,1,0
        "  nk_val r = f(xs, 4);\n"                          // owned boxed ARRAY result
        "  double out[4] = {0, 0, 0, 0};\n"
        "  nk_unbox_array(r, out, 4);\n"
        "  nk_release(r);\n"
        "  std::FILE* g = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!g) return 2;\n"
        "  for (int i = 0; i < 4; ++i) std::fprintf(g, \"%.17g\\n\", out[i]);\n"
        "  std::fclose(g); return 0;\n}\n";

    aot::CompileOptions opts;
    opts.includeDirs = {NK_BRIDGE_DIR};
    opts.defines     = {"NK_RT_USE_DLL"};
    opts.linkLibs    = {NK_RT_IMPORT_LIB};
    const auto r = aot::compileToExecutable(program, exe, opts);
    ASSERT_EQ(r.status, aot::CompileStatus::Ok)
        << "log:\n" << r.log << "\n--- generated source ---\n" << program;

    std::filesystem::copy_file(NK_RT_SHARED_DLL, base / "nk_codegen_rt.dll",
                               std::filesystem::copy_options::overwrite_existing, ec);
    ASSERT_FALSE(ec) << "copy nk_codegen_rt.dll: " << ec.message();
    ASSERT_EQ(std::system(("\"" + exe + "\"").c_str()), 0);

    std::vector<double> got;
    {
        std::ifstream is(outTxt);
        double        v;
        while (is >> v) got.push_back(v);
    }
    ASSERT_EQ(got.size(), 4u);
    EXPECT_DOUBLE_EQ(got[0], 2.0);  // mod(5,3)
    EXPECT_DOUBLE_EQ(got[1], 1.0);  // mod(7,3)
    EXPECT_DOUBLE_EQ(got[2], 1.0);  // mod(4,3)
    EXPECT_DOUBLE_EQ(got[3], 0.0);  // mod(9,3)
}

// INDEXING A DYNAMIC (DESIGN.md §10 C1, A4): z is a Dynamic array (mod over an
// array arg); z(2) indexes it via the runtime (index_dyn -> nk_index), which
// resolves the index/call ambiguity soundly. The element is Dynamic and crosses
// the boundary boxed (A3.5). Compiles, links the runtime DLL, runs, matches.
TEST(CodegenBridge, DynamicIndexRunsAndMatchesInterpreter)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    TransferRegistry reg;
    registerStandardTransfers(reg);
    numkit::Lexer          lex("function y = f(x)\n"
                               "  z = mod(x, 3);\n"   // Dynamic array
                               "  y = z(2);\n"         // index a Dynamic -> Dynamic scalar
                               "end\n");
    numkit::Parser         parser(lex.tokenize());
    auto                   root = parser.parse();
    const numkit::ASTNode *fn   = nullptr;
    for (const auto &c : root->children)
        if (c && c->type == numkit::NodeType::FUNCTION_DEF) fn = c.get();
    ASSERT_NE(fn, nullptr);

    BridgeOptions bridge;
    bridge.enabled       = true;
    bridge.runtimeHeader = "nk_codegen_rt.h";
    const EmittedFunction emitted = emitFunction(
        *fn, {{"x", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}}, reg, nullptr,
        bridge);
    ASSERT_NE(emitted.source.find("nk_rt::index_dyn(z, {nk_rt::val::scalar(2.0)})"),
              std::string::npos);

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_dynidx_e2e.exe").string();
    const std::string outTxt = (base / "nk_dynidx_e2e_out.txt").string();
    std::error_code   ec;
    std::filesystem::remove(outTxt, ec);

    std::string program = emitted.source;
    program +=
        "#include <cstdio>\n"
        "int main() {\n"
        "  const double xs[4] = {5.0, 7.0, 4.0, 9.0};\n"  // mod(.,3)=[2 1 1 0], (2)->1
        "  nk_val r = f(xs, 4);\n"
        "  const double v = nk_unbox_scalar(r);\n"
        "  nk_release(r);\n"
        "  std::FILE* g = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!g) return 2;\n"
        "  std::fprintf(g, \"%.17g\\n\", v);\n"
        "  std::fclose(g); return 0;\n}\n";

    aot::CompileOptions opts;
    opts.includeDirs = {NK_BRIDGE_DIR};
    opts.defines     = {"NK_RT_USE_DLL"};
    opts.linkLibs    = {NK_RT_IMPORT_LIB};
    const auto r = aot::compileToExecutable(program, exe, opts);
    ASSERT_EQ(r.status, aot::CompileStatus::Ok)
        << "log:\n" << r.log << "\n--- generated source ---\n" << program;

    std::filesystem::copy_file(NK_RT_SHARED_DLL, base / "nk_codegen_rt.dll",
                               std::filesystem::copy_options::overwrite_existing, ec);
    ASSERT_FALSE(ec) << "copy nk_codegen_rt.dll: " << ec.message();
    ASSERT_EQ(std::system(("\"" + exe + "\"").c_str()), 0);

    std::vector<double> got;
    {
        std::ifstream is(outTxt);
        double        v;
        while (is >> v) got.push_back(v);
    }
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 1.0);  // mod([5 7 4 9],3) = [2 1 1 0]; (2) = 1
}

// DYNAMIC PARAMETER (DESIGN.md §10 C1): a function taking an un-typeable input
// — the IN side of the Dynamic boundary (boxed return is the OUT side, A3.5).
// The param is an nk_rt::val by value (MATLAB pass-by-value = a copy); the body
// operates on it in the Value tier. Completes the Dynamic in/out symmetry.
TEST(CodegenBridge, DynamicParamRunsAndMatchesInterpreter)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    TransferRegistry reg;
    registerStandardTransfers(reg);
    numkit::Lexer          lex("function y = f(z)\n  y = z + 1;\nend\n");  // z Dynamic param
    numkit::Parser         parser(lex.tokenize());
    auto                   root = parser.parse();
    const numkit::ASTNode *fn   = nullptr;
    for (const auto &c : root->children)
        if (c && c->type == numkit::NodeType::FUNCTION_DEF) fn = c.get();
    ASSERT_NE(fn, nullptr);

    BridgeOptions bridge;
    bridge.enabled       = true;
    bridge.runtimeHeader = "nk_codegen_rt.h";
    const EmittedFunction emitted =
        emitFunction(*fn, {{"z", InferredType::dynamic()}}, reg, nullptr, bridge);
    ASSERT_NE(emitted.source.find("nk_rt::val z"), std::string::npos);  // by-value Dynamic param
    ASSERT_NE(emitted.source.find("nk_val f("), std::string::npos);     // boxed Dynamic return

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_dynparam_e2e.exe").string();
    const std::string outTxt = (base / "nk_dynparam_e2e_out.txt").string();
    std::error_code   ec;
    std::filesystem::remove(outTxt, ec);

    std::string program = emitted.source;
    program +=
        "#include <cstdio>\n"
        "int main() {\n"
        "  nk_val r = f(nk_rt::val::scalar(5.0));\n"   // pass a boxed Dynamic value in
        "  const double v = nk_unbox_scalar(r);\n"
        "  nk_release(r);\n"
        "  std::FILE* g = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!g) return 2;\n"
        "  std::fprintf(g, \"%.17g\\n\", v);\n"
        "  std::fclose(g); return 0;\n}\n";

    aot::CompileOptions opts;
    opts.includeDirs = {NK_BRIDGE_DIR};
    opts.defines     = {"NK_RT_USE_DLL"};
    opts.linkLibs    = {NK_RT_IMPORT_LIB};
    const auto r = aot::compileToExecutable(program, exe, opts);
    ASSERT_EQ(r.status, aot::CompileStatus::Ok)
        << "log:\n" << r.log << "\n--- generated source ---\n" << program;

    std::filesystem::copy_file(NK_RT_SHARED_DLL, base / "nk_codegen_rt.dll",
                               std::filesystem::copy_options::overwrite_existing, ec);
    ASSERT_FALSE(ec) << "copy nk_codegen_rt.dll: " << ec.message();
    ASSERT_EQ(std::system(("\"" + exe + "\"").c_str()), 0);

    std::vector<double> got;
    {
        std::ifstream is(outTxt);
        double        v;
        while (is >> v) got.push_back(v);
    }
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 6.0);  // f(5) = 5 + 1
}

// 2-D DYNAMIC ARRAY (DESIGN.md §10 C1): a Dynamic value that is a MATRIX. mod
// over a 2-D param has no transfer -> y is a Dynamic 2-D array; the matrix arg
// is boxed shape-preserving (val::matrix -> nk_box_matrix, column-major) and the
// result crosses the boundary boxed (A3.5). Compiles, links the runtime DLL,
// runs, matches the interpreter element-for-element.
TEST(CodegenBridge, DynamicMatrixRunsAndMatchesInterpreter)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    TransferRegistry reg;
    registerStandardTransfers(reg);
    numkit::Lexer          lex("function y = f(M)\n  y = mod(M, 3);\nend\n");  // M 2-D -> y Dynamic matrix
    numkit::Parser         parser(lex.tokenize());
    auto                   root = parser.parse();
    const numkit::ASTNode *fn   = nullptr;
    for (const auto &c : root->children)
        if (c && c->type == numkit::NodeType::FUNCTION_DEF) fn = c.get();
    ASSERT_NE(fn, nullptr);

    BridgeOptions bridge;
    bridge.enabled       = true;
    bridge.runtimeHeader = "nk_codegen_rt.h";
    const EmittedFunction emitted = emitFunction(
        *fn, {{"M", InferredType::concrete(ValueType::DOUBLE, Shape::dims(2, 3))}}, reg, nullptr,
        bridge);
    ASSERT_NE(emitted.source.find("nk_rt::val::matrix(M, _nk_M_rows, _nk_M_cols)"),
              std::string::npos);

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_dynmat_e2e.exe").string();
    const std::string outTxt = (base / "nk_dynmat_e2e_out.txt").string();
    std::error_code   ec;
    std::filesystem::remove(outTxt, ec);

    std::string program = emitted.source;
    program +=
        "#include <cstdio>\n"
        "int main() {\n"
        "  const double M[6] = {1, 4, 2, 5, 3, 6};\n"  // 2x3 column-major = [1 2 3; 4 5 6]
        "  nk_val r = f(M, 2, 3);\n"
        "  double out[6] = {0, 0, 0, 0, 0, 0};\n"
        "  nk_unbox_array(r, out, 6);\n"               // flat column-major
        "  nk_release(r);\n"
        "  std::FILE* g = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!g) return 2;\n"
        "  for (int i = 0; i < 6; ++i) std::fprintf(g, \"%.17g\\n\", out[i]);\n"
        "  std::fclose(g); return 0;\n}\n";

    aot::CompileOptions opts;
    opts.includeDirs = {NK_BRIDGE_DIR};
    opts.defines     = {"NK_RT_USE_DLL"};
    opts.linkLibs    = {NK_RT_IMPORT_LIB};
    const auto r = aot::compileToExecutable(program, exe, opts);
    ASSERT_EQ(r.status, aot::CompileStatus::Ok)
        << "log:\n" << r.log << "\n--- generated source ---\n" << program;

    std::filesystem::copy_file(NK_RT_SHARED_DLL, base / "nk_codegen_rt.dll",
                               std::filesystem::copy_options::overwrite_existing, ec);
    ASSERT_FALSE(ec) << "copy nk_codegen_rt.dll: " << ec.message();
    ASSERT_EQ(std::system(("\"" + exe + "\"").c_str()), 0);

    std::vector<double> got;
    {
        std::ifstream is(outTxt);
        double        v;
        while (is >> v) got.push_back(v);
    }
    ASSERT_EQ(got.size(), 6u);
    // mod([1 4 2 5 3 6], 3) column-major = [1 1 2 2 0 0]
    const double expect[6] = {1, 1, 2, 2, 0, 0};
    for (int i = 0; i < 6; ++i) EXPECT_DOUBLE_EQ(got[i], expect[i]) << "at i=" << i;
}

// BRIDGED BUILTIN MULTI-OUTPUT (DESIGN.md §10 C1): the common `[m, i] = max(x)`
// idiom (also sort/min/unique with an index). The runtime owns nargout and
// computes both outputs; each is kept BOXED as a Dynamic nk_rt::val, then flows
// through the Dynamic tier (here m+i). Compiles, links the runtime DLL, runs,
// matches the interpreter. (Without this the whole program would refuse->fall
// back to the interpreter; the bridge lets it compile.)
TEST(CodegenBridge, BridgedBuiltinMultiOutputRunsAndMatchesInterpreter)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    TransferRegistry reg;
    registerStandardTransfers(reg);
    numkit::Lexer          lex("function y = f(x)\n"
                               "  [m, i] = max(x);\n"  // bridged 2-output builtin -> m,i Dynamic
                               "  y = m + i;\n"         // Dynamic arithmetic -> boxed return
                               "end\n");
    numkit::Parser         parser(lex.tokenize());
    auto                   root = parser.parse();
    const numkit::ASTNode *fn   = nullptr;
    for (const auto &c : root->children)
        if (c && c->type == numkit::NodeType::FUNCTION_DEF) fn = c.get();
    ASSERT_NE(fn, nullptr);

    BridgeOptions bridge;
    bridge.enabled       = true;
    bridge.runtimeHeader = "nk_codegen_rt.h";
    const EmittedFunction emitted = emitFunction(
        *fn, {{"x", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}}, reg, nullptr,
        bridge);
    ASSERT_NE(emitted.source.find("nk_rt::call_dyn_multi(\"max\""), std::string::npos);

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_multiout_bridge_e2e.exe").string();
    const std::string outTxt = (base / "nk_multiout_bridge_e2e_out.txt").string();
    std::error_code   ec;
    std::filesystem::remove(outTxt, ec);

    std::string program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  const double xs[5] = {3, 1, 4, 1, 5};\n"  // max = 5 at index 5 (1-based)
        "  nk_val r = f(xs, 5);\n"
        "  const double v = nk_unbox_scalar(r);\n"
        "  nk_release(r);\n"
        "  std::FILE* g = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!g) return 2;\n"
        "  std::fprintf(g, \"%.17g\\n\", v);\n"
        "  std::fclose(g); return 0;\n}\n";

    aot::CompileOptions opts;
    opts.includeDirs = {NK_BRIDGE_DIR};
    opts.defines     = {"NK_RT_USE_DLL"};
    opts.linkLibs    = {NK_RT_IMPORT_LIB};
    const auto r = aot::compileToExecutable(program, exe, opts);
    ASSERT_EQ(r.status, aot::CompileStatus::Ok)
        << "log:\n" << r.log << "\n--- generated source ---\n" << program;

    std::filesystem::copy_file(NK_RT_SHARED_DLL, base / "nk_codegen_rt.dll",
                               std::filesystem::copy_options::overwrite_existing, ec);
    ASSERT_FALSE(ec) << "copy nk_codegen_rt.dll: " << ec.message();
    ASSERT_EQ(std::system(("\"" + exe + "\"").c_str()), 0);

    std::vector<double> got;
    {
        std::ifstream is(outTxt);
        double        v;
        while (is >> v) got.push_back(v);
    }
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 10.0);  // max=5 at index 5; m + i = 10
}

// BRIDGED ARRAY result (DESIGN.md §6a, array layer): y = sign(x). `sign` is
// typed as an array (realMathUnaryTransfer) but has no std form, so it CANNOT
// lower natively — it bridges: box the array arg -> nk_call -> unbox into the
// caller-allocated output buffer. (sin/cos/erf/… now lower natively, see
// ElementwiseArrayMath; sign stays the bridged-array demo.)
TEST(CodegenBridge, BridgedArrayResultRunsAndMatchesInterpreter)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    TransferRegistry reg;
    registerStandardTransfers(reg);
    numkit::Lexer          lex("function y = f(x)\n  y = sign(x);\nend\n");
    numkit::Parser         parser(lex.tokenize());
    auto                   root = parser.parse();
    const numkit::ASTNode *fn   = nullptr;
    for (const auto &c : root->children)
        if (c && c->type == numkit::NodeType::FUNCTION_DEF) fn = c.get();
    ASSERT_NE(fn, nullptr);

    BridgeOptions bridge;
    bridge.enabled       = true;
    bridge.runtimeHeader = "nk_codegen_rt.h";
    const EmittedFunction emitted = emitFunction(
        *fn, {{"x", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}}, reg, nullptr,
        bridge);
    ASSERT_NE(emitted.source.find("nk_rt::bridge_into(\"sign\""), std::string::npos);

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_bridged_arr_e2e.exe").string();
    const std::string outTxt = (base / "nk_bridged_arr_e2e_out.txt").string();
    std::error_code   ec;
    std::filesystem::remove(outTxt, ec);

    const std::size_t N       = 8;
    std::string       program = emitted.source;
    program +=
        "#include <cstdio>\n"
        "int main() {\n"
        "  const std::size_t N = 8;\n"
        "  double x[8], y[8];\n"
        "  for (std::size_t i = 0; i < N; ++i) x[i] = double(i) - 4.0;\n"  // mixed sign
        "  f(x, N, y, N);\n"
        "  std::FILE* g = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!g) return 2;\n"
        "  for (std::size_t i = 0; i < N; ++i) std::fprintf(g, \"%.17g\\n\", y[i]);\n"
        "  std::fclose(g); return 0;\n}\n";

    aot::CompileOptions opts;
    opts.includeDirs = {NK_BRIDGE_DIR};
    opts.defines     = {"NK_RT_USE_DLL"};
    opts.linkLibs    = {NK_RT_IMPORT_LIB};
    const auto r = aot::compileToExecutable(program, exe, opts);
    ASSERT_EQ(r.status, aot::CompileStatus::Ok)
        << "log:\n" << r.log << "\n--- generated source ---\n" << program;

    std::filesystem::copy_file(NK_RT_SHARED_DLL, base / "nk_codegen_rt.dll",
                               std::filesystem::copy_options::overwrite_existing, ec);
    ASSERT_FALSE(ec) << "copy nk_codegen_rt.dll: " << ec.message();
    ASSERT_EQ(std::system(("\"" + exe + "\"").c_str()), 0);

    std::vector<double> got;
    {
        std::ifstream is(outTxt);
        double        v;
        while (is >> v) got.push_back(v);
    }
    ASSERT_EQ(got.size(), N);
    for (std::size_t i = 0; i < N; ++i) {
        const double xv  = double(i) - 4.0;
        const double ref = (xv > 0.0) - (xv < 0.0);  // sign(x)
        EXPECT_DOUBLE_EQ(got[i], ref) << "at i=" << i;
    }
}

// Bridged array REDUCTION -> scalar: s = sum(x). The array arg is boxed and the
// scalar result unboxed via bridge_scalar_arr; the value is the runtime's own
// sum (exact summation order / NaN handling). Compiled bridged, run, matched
// to the interpreter's sum.
TEST(CodegenBridge, ReductionSumBridgedMatchesInterpreter)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    TransferRegistry reg;
    registerStandardTransfers(reg);
    numkit::Lexer          lex("function s = f(x)\n  s = sum(x);\nend\n");
    numkit::Parser         parser(lex.tokenize());
    auto                   root = parser.parse();
    const numkit::ASTNode *fn   = nullptr;
    for (const auto &c : root->children)
        if (c && c->type == numkit::NodeType::FUNCTION_DEF) fn = c.get();
    ASSERT_NE(fn, nullptr);

    BridgeOptions bridge;
    bridge.enabled       = true;
    bridge.runtimeHeader = "nk_codegen_rt.h";
    const EmittedFunction emitted = emitFunction(
        *fn, {{"x", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}}, reg, nullptr,
        bridge);
    ASSERT_NE(emitted.source.find("nk_rt::bridge_scalar_arr(\"sum\""), std::string::npos);

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_redsum_e2e.exe").string();
    const std::string outTxt = (base / "nk_redsum_e2e_out.txt").string();
    std::error_code   ec;
    std::filesystem::remove(outTxt, ec);

    std::string program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double x[5] = {1.5, 2.5, 3.5, 4.5, 5.5};\n"
        "  double s = " + emitted.name + "(x, 5);\n"
        "  std::FILE* g = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!g) return 2;\n"
        "  std::fprintf(g, \"%.17g\\n\", s);\n"
        "  std::fclose(g); return 0;\n}\n";

    aot::CompileOptions opts;
    opts.includeDirs = {NK_BRIDGE_DIR};
    opts.defines     = {"NK_RT_USE_DLL"};
    opts.linkLibs    = {NK_RT_IMPORT_LIB};
    const auto r = aot::compileToExecutable(program, exe, opts);
    ASSERT_EQ(r.status, aot::CompileStatus::Ok)
        << "log:\n" << r.log << "\n--- generated source ---\n" << program;

    std::filesystem::copy_file(NK_RT_SHARED_DLL, base / "nk_codegen_rt.dll",
                               std::filesystem::copy_options::overwrite_existing, ec);
    ASSERT_FALSE(ec) << "copy nk_codegen_rt.dll: " << ec.message();
    ASSERT_EQ(std::system(("\"" + exe + "\"").c_str()), 0);

    std::vector<double> got;
    {
        std::ifstream is(outTxt);
        double        v;
        while (is >> v) got.push_back(v);
    }
    ASSERT_EQ(got.size(), 1u);

    numkit::StandardEngine engine;
    engine.eval("import compat.*;");
    const double ref = engine.eval("sum([1.5 2.5 3.5 4.5 5.5]);").toScalar();
    EXPECT_NEAR(got[0], ref, 1e-12);  // == runtime sum (here 17.5)
}

// Bridged shape-preserving array builtin: y = cumsum(x). Array arg boxed, array
// result unboxed into the caller out-param (bridge_into); value == interpreter.
TEST(CodegenBridge, CumsumBridgedMatchesInterpreter)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    TransferRegistry reg;
    registerStandardTransfers(reg);
    numkit::Lexer          lex("function y = f(x)\n  y = cumsum(x);\nend\n");
    numkit::Parser         parser(lex.tokenize());
    auto                   root = parser.parse();
    const numkit::ASTNode *fn   = nullptr;
    for (const auto &c : root->children)
        if (c && c->type == numkit::NodeType::FUNCTION_DEF) fn = c.get();
    ASSERT_NE(fn, nullptr);

    BridgeOptions bridge;
    bridge.enabled       = true;
    bridge.runtimeHeader = "nk_codegen_rt.h";
    const EmittedFunction emitted = emitFunction(
        *fn, {{"x", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}}, reg, nullptr,
        bridge);
    ASSERT_NE(emitted.source.find("nk_rt::bridge_into(\"cumsum\""), std::string::npos);

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_cumsum_e2e.exe").string();
    const std::string outTxt = (base / "nk_cumsum_e2e_out.txt").string();
    std::error_code   ec;
    std::filesystem::remove(outTxt, ec);

    std::string program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double x[4] = {1, 2, 3, 4}, y[4];\n"
        "  " + emitted.name + "(x, 4, y, 4);\n"
        "  std::FILE* g = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!g) return 2;\n"
        "  for (int i = 0; i < 4; ++i) std::fprintf(g, \"%.17g\\n\", y[i]);\n"
        "  std::fclose(g); return 0;\n}\n";

    aot::CompileOptions opts;
    opts.includeDirs = {NK_BRIDGE_DIR};
    opts.defines     = {"NK_RT_USE_DLL"};
    opts.linkLibs    = {NK_RT_IMPORT_LIB};
    const auto r = aot::compileToExecutable(program, exe, opts);
    ASSERT_EQ(r.status, aot::CompileStatus::Ok)
        << "log:\n" << r.log << "\n--- generated source ---\n" << program;

    std::filesystem::copy_file(NK_RT_SHARED_DLL, base / "nk_codegen_rt.dll",
                               std::filesystem::copy_options::overwrite_existing, ec);
    ASSERT_FALSE(ec) << "copy nk_codegen_rt.dll: " << ec.message();
    ASSERT_EQ(std::system(("\"" + exe + "\"").c_str()), 0);

    std::vector<double> got;
    {
        std::ifstream is(outTxt);
        double        v;
        while (is >> v) got.push_back(v);
    }
    ASSERT_EQ(got.size(), 4u);
    const double exp[4] = {1, 3, 6, 10};  // cumsum([1 2 3 4])
    for (int i = 0; i < 4; ++i) EXPECT_DOUBLE_EQ(got[i], exp[i]) << "at " << i;
}

// Ops-kernel lowering end-to-end: C = A*B with OpsKernelOptions enabled emits
// numkit::ops::matmulDouble and links the nk_ops_kernels shared facade (one
// import lib + DLL, mirroring nk_codegen_rt — no static transitive-dep
// enumeration). Proves the whole "codegen takes a kernel from ops" path links
// and runs. A (2x3) col-major {1..6}, B (3x2) {1..6} -> C (2x2) {22,28,49,64}.
TEST(CodegenOpsKernel, MatrixProductViaOpsKernelRunsCorrectly)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    TransferRegistry reg;
    registerStandardTransfers(reg);
    numkit::Lexer          lex("function C = f(A, B)\n  C = A * B;\nend\n");
    numkit::Parser         parser(lex.tokenize());
    auto                   root = parser.parse();
    const numkit::ASTNode *fn   = nullptr;
    for (const auto &c : root->children)
        if (c && c->type == numkit::NodeType::FUNCTION_DEF) fn = c.get();
    ASSERT_NE(fn, nullptr);

    OpsKernelOptions      ops{true};
    const EmittedFunction emitted = emitFunction(
        *fn, {{"A", InferredType::concrete(ValueType::DOUBLE, Shape::dims(2, 3))},
              {"B", InferredType::concrete(ValueType::DOUBLE, Shape::dims(3, 2))}},
        reg, nullptr, {}, ops);
    ASSERT_NE(emitted.source.find("numkit::ops::matmulDouble("), std::string::npos);
    ASSERT_NE(emitted.source.find("#include <numkit/ops/kernels.hpp>"), std::string::npos);

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_opskernel_e2e.exe").string();
    const std::string outTxt = (base / "nk_opskernel_e2e_out.txt").string();
    std::error_code   ec;
    std::filesystem::remove(outTxt, ec);

    std::string program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double A[6] = {1, 2, 3, 4, 5, 6};\n"  // 2x3 col-major
        "  double B[6] = {1, 2, 3, 4, 5, 6};\n"  // 3x2 col-major
        "  double C[4];\n"
        "  f(A, 2, 3, B, 3, 2, C, 2, 2);\n"
        "  std::FILE* g = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!g) return 2;\n"
        "  for (int i = 0; i < 4; ++i) std::fprintf(g, \"%.17g\\n\", C[i]);\n"
        "  std::fclose(g); return 0;\n}\n";

    aot::CompileOptions opts;
    opts.includeDirs = {NK_OPS_INCLUDE_DIR};
    opts.defines     = {"NK_OPS_USE_DLL"};
    opts.linkLibs    = {NK_OPS_IMPORT_LIB};
    const auto r = aot::compileToExecutable(program, exe, opts);
    ASSERT_EQ(r.status, aot::CompileStatus::Ok)
        << "log:\n" << r.log << "\n--- generated source ---\n" << program;

    std::filesystem::copy_file(NK_OPS_SHARED_DLL, base / "nk_ops_kernels.dll",
                               std::filesystem::copy_options::overwrite_existing, ec);
    ASSERT_FALSE(ec) << "copy nk_ops_kernels.dll: " << ec.message();
    ASSERT_EQ(std::system(("\"" + exe + "\"").c_str()), 0);

    std::vector<double> got;
    {
        std::ifstream is(outTxt);
        double        v;
        while (is >> v) got.push_back(v);
    }
    ASSERT_EQ(got.size(), 4u);
    const double exp[4] = {22, 28, 49, 64};  // (A*B) column-major
    for (int i = 0; i < 4; ++i) EXPECT_DOUBLE_EQ(got[i], exp[i]) << "at " << i;
}

// Element-wise binary op via the ops kernel: z = x + y -> numkit::ops::plusDouble,
// linked through the same nk_ops_kernels shared facade. x=[1,2,3], y=[10,20,30]
// -> z=[11,22,33].
TEST(CodegenOpsKernel, ElementwiseBinaryViaOpsKernelRunsCorrectly)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    TransferRegistry reg;
    registerStandardTransfers(reg);
    numkit::Lexer          lex("function z = f(x, y)\n  z = x + y;\nend\n");
    numkit::Parser         parser(lex.tokenize());
    auto                   root = parser.parse();
    const numkit::ASTNode *fn   = nullptr;
    for (const auto &c : root->children)
        if (c && c->type == numkit::NodeType::FUNCTION_DEF) fn = c.get();
    ASSERT_NE(fn, nullptr);

    OpsKernelOptions      ops{true};
    const InferredType    row = InferredType::concrete(ValueType::DOUBLE, Shape::rowVector());
    const EmittedFunction emitted =
        emitFunction(*fn, {{"x", row}, {"y", row}}, reg, nullptr, {}, ops);
    ASSERT_NE(emitted.source.find("numkit::ops::plusDouble(x, y, z,"), std::string::npos);

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_opsewise_e2e.exe").string();
    const std::string outTxt = (base / "nk_opsewise_e2e_out.txt").string();
    std::error_code   ec;
    std::filesystem::remove(outTxt, ec);

    std::string program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double x[3] = {1, 2, 3}, y[3] = {10, 20, 30}, z[3];\n"
        "  f(x, 3, y, 3, z, 3);\n"
        "  std::FILE* g = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!g) return 2;\n"
        "  for (int i = 0; i < 3; ++i) std::fprintf(g, \"%.17g\\n\", z[i]);\n"
        "  std::fclose(g); return 0;\n}\n";

    aot::CompileOptions opts;
    opts.includeDirs = {NK_OPS_INCLUDE_DIR};
    opts.defines     = {"NK_OPS_USE_DLL"};
    opts.linkLibs    = {NK_OPS_IMPORT_LIB};
    const auto r = aot::compileToExecutable(program, exe, opts);
    ASSERT_EQ(r.status, aot::CompileStatus::Ok)
        << "log:\n" << r.log << "\n--- generated source ---\n" << program;

    std::filesystem::copy_file(NK_OPS_SHARED_DLL, base / "nk_ops_kernels.dll",
                               std::filesystem::copy_options::overwrite_existing, ec);
    ASSERT_FALSE(ec) << "copy nk_ops_kernels.dll: " << ec.message();
    ASSERT_EQ(std::system(("\"" + exe + "\"").c_str()), 0);

    std::vector<double> got;
    {
        std::ifstream is(outTxt);
        double        v;
        while (is >> v) got.push_back(v);
    }
    ASSERT_EQ(got.size(), 3u);
    const double exp[3] = {11, 22, 33};  // x + y
    for (int i = 0; i < 3; ++i) EXPECT_DOUBLE_EQ(got[i], exp[i]) << "at " << i;
}

// Complex matrix product via the ops kernel: C = A*B (complex) ->
// numkit::ops::matmulComplex. A=B=diag(i) (2x2) -> C=diag(i*i)=diag(-1),
// exercising complex multiply across the kernel + shared facade.
TEST(CodegenOpsKernel, ComplexMatrixProductViaOpsKernelRunsCorrectly)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    TransferRegistry reg;
    registerStandardTransfers(reg);
    numkit::Lexer          lex("function C = f(A, B)\n  C = A * B;\nend\n");
    numkit::Parser         parser(lex.tokenize());
    auto                   root = parser.parse();
    const numkit::ASTNode *fn   = nullptr;
    for (const auto &c : root->children)
        if (c && c->type == numkit::NodeType::FUNCTION_DEF) fn = c.get();
    ASSERT_NE(fn, nullptr);

    OpsKernelOptions      ops{true};
    const InferredType    mat = InferredType::concrete(ValueType::COMPLEX, Shape::dims(2, 2));
    const EmittedFunction emitted =
        emitFunction(*fn, {{"A", mat}, {"B", mat}}, reg, nullptr, {}, ops);
    ASSERT_NE(emitted.source.find("numkit::ops::matmulComplex("), std::string::npos);

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_opscxmm_e2e.exe").string();
    const std::string outTxt = (base / "nk_opscxmm_e2e_out.txt").string();
    std::error_code   ec;
    std::filesystem::remove(outTxt, ec);

    std::string program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  std::complex<double> A[4] = { {0,1}, {0,0}, {0,0}, {0,1} };\n"  // col-major diag(i)
        "  std::complex<double> B[4] = { {0,1}, {0,0}, {0,0}, {0,1} };\n"
        "  std::complex<double> C[4];\n"
        "  f(A, 2, 2, B, 2, 2, C, 2, 2);\n"
        "  std::FILE* g = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!g) return 2;\n"
        "  for (int i = 0; i < 4; ++i) std::fprintf(g, \"%.17g\\n%.17g\\n\", C[i].real(), C[i].imag());\n"
        "  std::fclose(g); return 0;\n}\n";

    aot::CompileOptions opts;
    opts.includeDirs = {NK_OPS_INCLUDE_DIR};
    opts.defines     = {"NK_OPS_USE_DLL"};
    opts.linkLibs    = {NK_OPS_IMPORT_LIB};
    const auto r = aot::compileToExecutable(program, exe, opts);
    ASSERT_EQ(r.status, aot::CompileStatus::Ok)
        << "log:\n" << r.log << "\n--- generated source ---\n" << program;

    std::filesystem::copy_file(NK_OPS_SHARED_DLL, base / "nk_ops_kernels.dll",
                               std::filesystem::copy_options::overwrite_existing, ec);
    ASSERT_FALSE(ec) << "copy nk_ops_kernels.dll: " << ec.message();
    ASSERT_EQ(std::system(("\"" + exe + "\"").c_str()), 0);

    std::vector<double> got;
    {
        std::ifstream is(outTxt);
        double        v;
        while (is >> v) got.push_back(v);
    }
    ASSERT_EQ(got.size(), 8u);                            // re,im per element
    const double exp[8] = {-1, 0, 0, 0, 0, 0, -1, 0};  // diag(-1) col-major
    for (int i = 0; i < 8; ++i) EXPECT_DOUBLE_EQ(got[i], exp[i]) << "at " << i;
}

// SIMD transcendental via the ops kernel: y = sin(x) -> numkit::ops::sinDouble
// (the same fusedTransAffine the runtime's fusion uses). Compared bin-for-bin to
// the interpreter's own sin — the SIMD kernel matches to high precision.
TEST(CodegenOpsKernel, TranscendentalViaOpsKernelMatchesInterpreter)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    TransferRegistry reg;
    registerStandardTransfers(reg);
    numkit::Lexer          lex("function y = f(x)\n  y = sin(x);\nend\n");
    numkit::Parser         parser(lex.tokenize());
    auto                   root = parser.parse();
    const numkit::ASTNode *fn   = nullptr;
    for (const auto &c : root->children)
        if (c && c->type == numkit::NodeType::FUNCTION_DEF) fn = c.get();
    ASSERT_NE(fn, nullptr);

    OpsKernelOptions      ops{true};
    const InferredType    row = InferredType::concrete(ValueType::DOUBLE, Shape::rowVector());
    const EmittedFunction emitted = emitFunction(*fn, {{"x", row}}, reg, nullptr, {}, ops);
    ASSERT_NE(emitted.source.find("numkit::ops::sinDouble(x, y,"), std::string::npos);

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_opssin_e2e.exe").string();
    const std::string outTxt = (base / "nk_opssin_e2e_out.txt").string();
    std::error_code   ec;
    std::filesystem::remove(outTxt, ec);

    std::string program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double x[4] = {0.5, 1.0, 1.5, 2.0}, y[4];\n"
        "  f(x, 4, y, 4);\n"
        "  std::FILE* g = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!g) return 2;\n"
        "  for (int i = 0; i < 4; ++i) std::fprintf(g, \"%.17g\\n\", y[i]);\n"
        "  std::fclose(g); return 0;\n}\n";

    aot::CompileOptions opts;
    opts.includeDirs = {NK_OPS_INCLUDE_DIR};
    opts.defines     = {"NK_OPS_USE_DLL"};
    opts.linkLibs    = {NK_OPS_IMPORT_LIB};
    const auto r = aot::compileToExecutable(program, exe, opts);
    ASSERT_EQ(r.status, aot::CompileStatus::Ok)
        << "log:\n" << r.log << "\n--- generated source ---\n" << program;

    std::filesystem::copy_file(NK_OPS_SHARED_DLL, base / "nk_ops_kernels.dll",
                               std::filesystem::copy_options::overwrite_existing, ec);
    ASSERT_FALSE(ec) << "copy nk_ops_kernels.dll: " << ec.message();
    ASSERT_EQ(std::system(("\"" + exe + "\"").c_str()), 0);

    std::vector<double> got;
    {
        std::ifstream is(outTxt);
        double        v;
        while (is >> v) got.push_back(v);
    }
    ASSERT_EQ(got.size(), 4u);

    numkit::StandardEngine engine;
    engine.eval("import compat.*;");
    numkit::Value yv = engine.eval("sin([0.5 1.0 1.5 2.0]);");
    ASSERT_EQ(yv.numel(), 4u);
    for (int i = 0; i < 4; ++i)
        EXPECT_NEAR(got[i], yv.doubleData()[i], 1e-12) << "at " << i;
}

// CX4b: a bridged call returning a COMPLEX array — y = fft(x). The headline of
// the complex pipeline: x (real) is boxed, fft runs in the runtime, and the
// complex result is unboxed into a std::complex<double> out-param via
// bridge_into_cx. Compiled bridged, run, and the complex output matched against
// the interpreter's own fft.
TEST(CodegenBridge, ComplexFftBridgedMatchesInterpreter)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    TransferRegistry reg;
    registerStandardTransfers(reg);
    numkit::Lexer          lex("function y = f(x)\n  y = fft(x);\nend\n");
    numkit::Parser         parser(lex.tokenize());
    auto                   root = parser.parse();
    const numkit::ASTNode *fn   = nullptr;
    for (const auto &c : root->children)
        if (c && c->type == numkit::NodeType::FUNCTION_DEF) fn = c.get();
    ASSERT_NE(fn, nullptr);

    BridgeOptions bridge;
    bridge.enabled       = true;
    bridge.runtimeHeader = "nk_codegen_rt.h";
    const EmittedFunction emitted = emitFunction(
        *fn, {{"x", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}}, reg, nullptr,
        bridge);
    ASSERT_NE(emitted.source.find("nk_rt::bridge_into_cx(\"fft\""), std::string::npos);

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_cxfft_e2e.exe").string();
    const std::string outTxt = (base / "nk_cxfft_e2e_out.txt").string();
    std::error_code   ec;
    std::filesystem::remove(outTxt, ec);

    std::string program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double x[4] = {1, 2, 3, 4};\n"
        "  std::complex<double> y[4];\n"
        "  f(x, 4, y, 4);\n"
        "  std::FILE* g = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!g) return 2;\n"
        "  for (int i = 0; i < 4; ++i) std::fprintf(g, \"%.17g\\n%.17g\\n\", y[i].real(), y[i].imag());\n"
        "  std::fclose(g); return 0;\n}\n";

    aot::CompileOptions opts;
    opts.includeDirs = {NK_BRIDGE_DIR};
    opts.defines     = {"NK_RT_USE_DLL"};
    opts.linkLibs    = {NK_RT_IMPORT_LIB};
    const auto r = aot::compileToExecutable(program, exe, opts);
    ASSERT_EQ(r.status, aot::CompileStatus::Ok)
        << "log:\n" << r.log << "\n--- generated source ---\n" << program;
    std::filesystem::copy_file(NK_RT_SHARED_DLL, base / "nk_codegen_rt.dll",
                               std::filesystem::copy_options::overwrite_existing, ec);
    ASSERT_FALSE(ec) << "copy nk_codegen_rt.dll: " << ec.message();
    ASSERT_EQ(std::system(("\"" + exe + "\"").c_str()), 0);

    std::vector<double> got;
    {
        std::ifstream is(outTxt);
        double        v;
        while (is >> v) got.push_back(v);
    }
    ASSERT_EQ(got.size(), 8u);  // re,im per element

    numkit::StandardEngine engine;
    engine.eval("import compat.*;");
    numkit::Value yv = engine.eval("fft([1 2 3 4]);");
    ASSERT_EQ(yv.numel(), 4u);
    for (int i = 0; i < 4; ++i) {
        const std::complex<double> ref =
            yv.isComplex() ? yv.complexData()[i] : std::complex<double>(yv.doubleData()[i], 0.0);
        EXPECT_NEAR(got[2 * i], ref.real(), 1e-9) << "re at " << i;
        EXPECT_NEAR(got[2 * i + 1], ref.imag(), 1e-9) << "im at " << i;
    }
}

// Bridged array LOCAL: `z = sign(x)` fills an owned-vector local (resized to
// the result's numel via bridge_to_vec), then z is read element-wise. The
// payoff of array locals + bridging: a general intermediate array expression.
TEST(CodegenBridge, BridgedArrayLocalRunsAndMatchesInterpreter)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    TransferRegistry reg;
    registerStandardTransfers(reg);
    numkit::Lexer          lex("function y = f(x)\n  n = numel(x);\n  z = sign(x);\n"
                               "  y = zeros(1, n);\n  for k = 1:n\n    y(k) = z(k) * 2;\n  end\nend\n");
    numkit::Parser         parser(lex.tokenize());
    auto                   root = parser.parse();
    const numkit::ASTNode *fn   = nullptr;
    for (const auto &c : root->children)
        if (c && c->type == numkit::NodeType::FUNCTION_DEF) fn = c.get();
    ASSERT_NE(fn, nullptr);

    BridgeOptions bridge;
    bridge.enabled       = true;
    bridge.runtimeHeader = "nk_codegen_rt.h";
    const EmittedFunction emitted = emitFunction(
        *fn, {{"x", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}}, reg, nullptr,
        bridge);
    ASSERT_NE(emitted.source.find("std::vector<double> z;"), std::string::npos);   // owned local
    ASSERT_NE(emitted.source.find("nk_rt::bridge_to_vec(\"sign\""), std::string::npos);

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_bridged_local_e2e.exe").string();
    const std::string outTxt = (base / "nk_bridged_local_e2e_out.txt").string();
    std::error_code   ec;
    std::filesystem::remove(outTxt, ec);

    const std::size_t N       = 8;
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  const std::size_t N = 8;\n"
        "  double x[8], y[8];\n"
        "  for (std::size_t i = 0; i < N; ++i) x[i] = double(i) - 4.0;\n"  // mixed sign
        "  f(x, N, y, N);\n"
        "  std::FILE* g = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!g) return 2;\n"
        "  for (std::size_t i = 0; i < N; ++i) std::fprintf(g, \"%.17g\\n\", y[i]);\n"
        "  std::fclose(g); return 0;\n}\n";

    aot::CompileOptions opts;
    opts.includeDirs = {NK_BRIDGE_DIR};
    opts.defines     = {"NK_RT_USE_DLL"};
    opts.linkLibs    = {NK_RT_IMPORT_LIB};
    const auto r = aot::compileToExecutable(program, exe, opts);
    ASSERT_EQ(r.status, aot::CompileStatus::Ok)
        << "log:\n" << r.log << "\n--- generated source ---\n" << program;

    std::filesystem::copy_file(NK_RT_SHARED_DLL, base / "nk_codegen_rt.dll",
                               std::filesystem::copy_options::overwrite_existing, ec);
    ASSERT_FALSE(ec) << "copy nk_codegen_rt.dll: " << ec.message();
    ASSERT_EQ(std::system(("\"" + exe + "\"").c_str()), 0);

    std::vector<double> got;
    {
        std::ifstream is(outTxt);
        double        v;
        while (is >> v) got.push_back(v);
    }
    ASSERT_EQ(got.size(), N);
    for (std::size_t i = 0; i < N; ++i) {
        const double xv  = double(i) - 4.0;
        const double ref = ((xv > 0.0) - (xv < 0.0)) * 2.0;  // sign(x) * 2
        EXPECT_DOUBLE_EQ(got[i], ref) << "at i=" << i;
    }
}
