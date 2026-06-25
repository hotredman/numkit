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

#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
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

// 1-D slice WRITE: x(2:4) = scalar (broadcast) and x(1:2) = w (matched-length
// array copy). Previously a range lhs index refused. x = 1:5 = [1 2 3 4 5];
// x(2:4)=7 -> [1 7 7 7 5]; x(1:2)=[3 4] -> [3 4 7 7 5].
TEST(CodegenE2E, SliceWrite)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const EmittedFunction emitted = transpile(
        "function r = f(w)\n"
        "  x = 1:5;\n"        // 1-D local [1 2 3 4 5]
        "  x(2:4) = 7;\n"     // broadcast -> [1 7 7 7 5]
        "  x(1:2) = w;\n"     // array copy (w = [3 4]) -> [3 4 7 7 5]
        "  r = x(1) + x(2)*10 + x(3)*100 + x(4)*1000 + x(5)*10000;\n"
        "end\n",
        {{"w", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}});
    EXPECT_NE(emitted.source.find("_nk_v"), std::string::npos)
        << "a scalar slice write must broadcast natively, not refuse";

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_slicew_e2e.exe").string();
    const std::string outTxt = (base / "nk_slicew_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double w[2] = {3.0, 4.0};\n"
        "  double r = f(w, 2);\n"  // [3 4 7 7 5]: 3 + 40 + 700 + 7000 + 50000 = 57743
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", r);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 3.0 + 40.0 + 700.0 + 7000.0 + 50000.0);  // 57743
}

// Array reversal: flip / fliplr / flipud on a 1-D row. flip reverses a vector;
// fliplr reverses a row; flipud leaves a row unchanged (it flips rows, of which a
// row has one). x = [10 20 30 40]: flip/fliplr -> [40 30 20 10], flipud -> same.
TEST(CodegenE2E, ArrayReversal)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const EmittedFunction emitted = transpile(
        "function r = f(x)\n"
        "  a = flip(x);\n"     // [40 30 20 10]
        "  b = fliplr(x);\n"   // [40 30 20 10] (x is a row)
        "  c = flipud(x);\n"   // [10 20 30 40] (unchanged: a row has one row)
        "  r = a(1) + b(1)*10 + c(1)*100 + a(numel(a))*1000;\n"
        "end\n",
        {{"x", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}});
    EXPECT_NE(emitted.source.find("- 1 - _nk_i"), std::string::npos)
        << "flip must lower to a native reverse copy, not refuse";

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_flip_e2e.exe").string();
    const std::string outTxt = (base / "nk_flip_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double x[4] = {10.0, 20.0, 30.0, 40.0};\n"
        "  double r = f(x, 4);\n"  // 40 + 40*10 + 10*100 + 10*1000 = 11440
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", r);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 40.0 + 400.0 + 1000.0 + 10000.0);  // 11440
}

// Native sort(x) ascending (no-bridge tier): std::sort on a copy. x = [30 10 40 20]
// -> [10 20 30 40].
TEST(CodegenE2E, SortAscending)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const EmittedFunction emitted = transpile(
        "function r = f(x)\n"
        "  y = sort(x);\n"   // [10 20 30 40]
        "  r = y(1) + y(2)*10 + y(3)*100 + y(4)*1000;\n"
        "end\n",
        {{"x", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}});
    EXPECT_NE(emitted.source.find("std::sort"), std::string::npos)
        << "sort must lower to a native std::sort, not refuse";

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_sort_e2e.exe").string();
    const std::string outTxt = (base / "nk_sort_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double x[4] = {30.0, 10.0, 40.0, 20.0};\n"
        "  double r = f(x, 4);\n"  // sorted [10 20 30 40]: 10 + 200 + 3000 + 40000 = 43210
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", r);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 10.0 + 200.0 + 3000.0 + 40000.0);  // 43210
}

// Native scalar stats reductions (no-bridge tier): norm([3 4])=5, var([2 4 6])=4
// (sample, /(n-1)), std([2 4 6])=2.
TEST(CodegenE2E, StatsReductions)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const EmittedFunction emitted = transpile(
        "function r = f(x, y)\n"
        "  a = norm(x);\n"   // [3 4] -> 5
        "  b = var(y);\n"    // [2 4 6] -> 4
        "  c = std(y);\n"    // -> 2
        "  r = a + b*10 + c*100;\n"
        "end\n",
        {{"x", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())},
         {"y", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}});
    EXPECT_NE(emitted.source.find("_nk_ss"), std::string::npos)
        << "norm/var/std must lower to native accumulation, not refuse";

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_stats_e2e.exe").string();
    const std::string outTxt = (base / "nk_stats_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double x[2] = {3.0, 4.0};\n"
        "  double y[3] = {2.0, 4.0, 6.0};\n"
        "  double r = f(x, 2, y, 3);\n"  // 5 + 4*10 + 2*100 = 245
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", r);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 5.0 + 40.0 + 200.0);  // 245
}

// Native median(x) (no-bridge tier): sort a copy + middle. median([3 1 2])=2 (odd),
// median([4 1 3 2])=2.5 (even, mean of the two middles).
TEST(CodegenE2E, Median)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const EmittedFunction emitted = transpile(
        "function r = f(x, y)\n"
        "  a = median(x);\n"   // [3 1 2] -> 2
        "  b = median(y);\n"   // [4 1 3 2] -> 2.5
        "  r = a*100 + b*10;\n"
        "end\n",
        {{"x", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())},
         {"y", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}});
    EXPECT_NE(emitted.source.find("_nk_t"), std::string::npos)
        << "median must lower to a native sort+middle, not refuse";

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_median_e2e.exe").string();
    const std::string outTxt = (base / "nk_median_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double x[3] = {3.0, 1.0, 2.0};\n"
        "  double y[4] = {4.0, 1.0, 3.0, 2.0};\n"
        "  double r = f(x, 3, y, 4);\n"  // 2*100 + 2.5*10 = 225
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", r);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 225.0);  // 200 + 25
}

// sort(x,'descend') alongside ascending sort(x). x = [30 10 40 20]:
// descend -> [40 30 20 10], ascend -> [10 20 30 40].
TEST(CodegenE2E, SortDescend)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const EmittedFunction emitted = transpile(
        "function r = f(x)\n"
        "  a = sort(x, 'descend');\n"   // [40 30 20 10]
        "  b = sort(x);\n"              // [10 20 30 40]
        "  r = a(1) + b(1)*10 + a(4)*100 + b(4)*1000;\n"
        "end\n",
        {{"x", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}});
    EXPECT_NE(emitted.source.find("_a > _b"), std::string::npos)
        << "sort(x,'descend') must emit the reversed comparator";

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_sortd_e2e.exe").string();
    const std::string outTxt = (base / "nk_sortd_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double x[4] = {30.0, 10.0, 40.0, 20.0};\n"
        "  double r = f(x, 4);\n"  // 40 + 10*10 + 10*100 + 40*1000 = 41140
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", r);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 40.0 + 100.0 + 1000.0 + 40000.0);  // 41140
}

// Native cummax/cummin(x) running extrema. x = [3 1 4 1 5]:
// cummax -> [3 3 4 4 5], cummin -> [3 1 1 1 1].
TEST(CodegenE2E, CumulativeExtrema)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const EmittedFunction emitted = transpile(
        "function r = f(x)\n"
        "  a = cummax(x);\n"   // [3 3 4 4 5]
        "  b = cummin(x);\n"   // [3 1 1 1 1]
        "  r = a(2) + a(5)*10 + b(2)*100 + b(5)*1000;\n"
        "end\n",
        {{"x", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}});
    EXPECT_EQ(emitted.source.find("cummax"), std::string::npos)
        << "cummax must be lowered to a native running loop, not left as a call";

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_cumext_e2e.exe").string();
    const std::string outTxt = (base / "nk_cumext_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double x[5] = {3.0, 1.0, 4.0, 1.0, 5.0};\n"
        "  double r = f(x, 5);\n"  // 3 + 5*10 + 1*100 + 1*1000 = 1153
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", r);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 3.0 + 50.0 + 100.0 + 1000.0);  // 1153
}

// Native circshift(x, k): circular shift. x = [1 2 3 4 5]:
// circshift(x, 2) -> [4 5 1 2 3]; circshift(x, -1) -> [2 3 4 5 1].
TEST(CodegenE2E, CircShift)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const EmittedFunction emitted = transpile(
        "function r = f(x)\n"
        "  a = circshift(x, 2);\n"    // [4 5 1 2 3]
        "  b = circshift(x, -1);\n"   // [2 3 4 5 1]
        "  r = a(1) + a(3)*10 + b(1)*100 + b(5)*1000;\n"
        "end\n",
        {{"x", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}});
    EXPECT_NE(emitted.source.find("% _nk_n"), std::string::npos)
        << "circshift must lower to a native modular-index copy, not refuse";

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_circ_e2e.exe").string();
    const std::string outTxt = (base / "nk_circ_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double x[5] = {1.0, 2.0, 3.0, 4.0, 5.0};\n"
        "  double r = f(x, 5);\n"  // 4 + 1*10 + 2*100 + 1*1000 = 1214
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", r);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 4.0 + 10.0 + 200.0 + 1000.0);  // 1214
}

// [m,i]=max(x) / [m,i]=min(x): native argmax/argmin two-output (always-native, even
// under the bridge). x = [30 10 40 20]: max=40 at index 3, min=10 at index 2.
TEST(CodegenE2E, ArgMaxMin)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const EmittedFunction emitted = transpile(
        "function r = f(x)\n"
        "  [m, mi] = max(x);\n"   // 40, 3
        "  [n, ni] = min(x);\n"   // 10, 2
        "  r = m + mi*10 + n*100 + ni*1000;\n"
        "end\n",
        {{"x", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}});
    EXPECT_NE(emitted.source.find("_nk_idx"), std::string::npos)
        << "[m,i]=max must track the 1-based index natively, not refuse";

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_argmm_e2e.exe").string();
    const std::string outTxt = (base / "nk_argmm_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double x[4] = {30.0, 10.0, 40.0, 20.0};\n"
        "  double r = f(x, 4);\n"  // 40 + 3*10 + 10*100 + 2*1000 = 3070
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", r);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 40.0 + 30.0 + 1000.0 + 2000.0);  // 3070
}

// Native logspace(a,b,n) (linspace mirror, decade-spaced). logspace(0,3,4) =
// 10^[0 1 2 3] = [1 10 100 1000]; runtime n -> an owned 1-D local.
TEST(CodegenE2E, LogspaceGenerative)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const EmittedFunction emitted = transpile(
        "function r = f(n)\n"
        "  y = logspace(0, 3, n);\n"   // [1 10 100 1000]
        "  r = y(1) + y(2) + y(3) + y(4) + numel(y);\n"
        "end\n",
        {{"n", InferredType::scalar(ValueType::DOUBLE)}});
    EXPECT_NE(emitted.source.find("std::pow(10.0"), std::string::npos)
        << "logspace must lower to a native 10^(a+i*step) fill, not refuse";

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_logspace_e2e.exe").string();
    const std::string outTxt = (base / "nk_logspace_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double r = f(4.0);\n"  // 1 + 10 + 100 + 1000 + 4 = 1115
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", r);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_NEAR(got[0], 1115.0, 1e-9);  // 1 + 10 + 100 + 1000 + 4
}

// Native unique(x) (no-bridge tier): sorted distinct values. unique([3 1 4 1 5 3])
// = [1 3 4 5].
TEST(CodegenE2E, Unique)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const EmittedFunction emitted = transpile(
        "function r = f(x)\n"
        "  y = unique(x);\n"   // [1 3 4 5]
        "  r = y(1) + y(2)*10 + y(3)*100 + y(4)*1000 + numel(y)*10000;\n"
        "end\n",
        {{"x", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}});
    EXPECT_NE(emitted.source.find("push_back(_nk_t"), std::string::npos)
        << "unique must lower to a native sort+dedup, not refuse";

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_unique_e2e.exe").string();
    const std::string outTxt = (base / "nk_unique_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double x[6] = {3.0, 1.0, 4.0, 1.0, 5.0, 3.0};\n"
        "  double r = f(x, 6);\n"  // [1 3 4 5]: 1 + 30 + 400 + 5000 + 4*10000 = 45431
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", r);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 1.0 + 30.0 + 400.0 + 5000.0 + 40000.0);  // 45431
}

// Native trapz(y) (no-bridge tier): trapezoidal integral, unit spacing.
// trapz([1 2 3 4 5]) = 1.5 + 2.5 + 3.5 + 4.5 = 12.
TEST(CodegenE2E, Trapz)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const EmittedFunction emitted = transpile(
        "function r = f(x)\n"
        "  r = trapz(x);\n"
        "end\n",
        {{"x", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}});
    EXPECT_NE(emitted.source.find("* 0.5"), std::string::npos)
        << "trapz must lower to a native trapezoid accumulation, not refuse";

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_trapz_e2e.exe").string();
    const std::string outTxt = (base / "nk_trapz_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double x[5] = {1.0, 2.0, 3.0, 4.0, 5.0};\n"
        "  double r = f(x, 5);\n"  // 1.5 + 2.5 + 3.5 + 4.5 = 12
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", r);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 12.0);
}

// Native polyval(p, x) (Horner, x a vector). p = [1 0 -1] (x^2 - 1) at x = [0 1 2 3]
// -> [-1 0 3 8].
TEST(CodegenE2E, Polyval)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const EmittedFunction emitted = transpile(
        "function r = f(p, x)\n"
        "  y = polyval(p, x);\n"   // x^2 - 1 -> [-1 0 3 8]
        "  r = y(1) + y(2)*10 + y(3)*100 + y(4)*1000;\n"
        "end\n",
        {{"p", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())},
         {"x", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}});
    EXPECT_NE(emitted.source.find("_nk_acc * "), std::string::npos)
        << "polyval must lower to a native Horner loop, not refuse";

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_polyval_e2e.exe").string();
    const std::string outTxt = (base / "nk_polyval_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double p[3] = {1.0, 0.0, -1.0};\n"   // x^2 - 1
        "  double x[4] = {0.0, 1.0, 2.0, 3.0};\n"
        "  double r = f(p, 3, x, 4);\n"  // [-1 0 3 8]: -1 + 0 + 300 + 8000 = 8299
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", r);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], -1.0 + 0.0 + 300.0 + 8000.0);  // 8299
}

// Native gradient(y) (numerical gradient, unit spacing). y = [1 4 9 16]:
// forward g(1)=3, centered g(2)=4, g(3)=6, backward g(4)=7 -> [3 4 6 7].
TEST(CodegenE2E, Gradient)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const EmittedFunction emitted = transpile(
        "function r = f(y)\n"
        "  g = gradient(y);\n"   // [3 4 6 7]
        "  r = g(1) + g(2)*10 + g(3)*100 + g(4)*1000;\n"
        "end\n",
        {{"y", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}});

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_grad_e2e.exe").string();
    const std::string outTxt = (base / "nk_grad_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double y[4] = {1.0, 4.0, 9.0, 16.0};\n"
        "  double r = f(y, 4);\n"  // [3 4 6 7]: 3 + 40 + 600 + 7000 = 7643
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", r);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 3.0 + 40.0 + 600.0 + 7000.0);  // 7643
}

// x(:) flatten: a 2-D matrix to a column vector (column-major). A = [1 2 3; 4 5 6]
// (col-major {1,4,2,5,3,6}); A(:) = [1 4 2 5 3 6].
TEST(CodegenE2E, ColonFlatten2D)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const EmittedFunction emitted = transpile(
        "function r = f(A)\n"
        "  v = A(:);\n"   // column-major flatten -> [1 4 2 5 3 6]
        "  r = v(1) + v(2)*10 + v(6)*100 + numel(v)*1000;\n"
        "end\n",
        {{"A", InferredType::concrete(ValueType::DOUBLE, Shape::dims(2, 3))}});

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_flatten_e2e.exe").string();
    const std::string outTxt = (base / "nk_flatten_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double A[6] = {1, 4, 2, 5, 3, 6};\n"  // 2x3 col-major
        "  double r = f(A, 2, 3);\n"  // [1 4 2 5 3 6]: 1 + 40 + 600 + 6000 = 6641
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", r);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 1.0 + 40.0 + 600.0 + 6000.0);  // 6641
}

// A(:,j) column slice: column j of a 2-D matrix as a column vector. A = [1 2 3;
// 4 5 6] (col-major {1,4,2,5,3,6}); A(:,2) = column 2 = [2; 5].
TEST(CodegenE2E, ColumnSlice2D)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const EmittedFunction emitted = transpile(
        "function r = f(A)\n"
        "  v = A(:,2);\n"   // column 2 = [2; 5]
        "  r = v(1) + v(2)*10 + numel(v)*100;\n"
        "end\n",
        {{"A", InferredType::concrete(ValueType::DOUBLE, Shape::dims(2, 3))}});

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_colslice_e2e.exe").string();
    const std::string outTxt = (base / "nk_colslice_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double A[6] = {1, 4, 2, 5, 3, 6};\n"  // 2x3 col-major
        "  double r = f(A, 2, 3);\n"  // col 2 = [2 5]: 2 + 5*10 + 2*100 = 252
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", r);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 2.0 + 50.0 + 200.0);  // 252
}

// A(i,:) row slice: row i of a 2-D matrix as a row vector (strided in column-major).
// A = [1 2 3; 4 5 6] (col-major {1,4,2,5,3,6}); A(1,:) = row 1 = [1 2 3].
TEST(CodegenE2E, RowSlice2D)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const EmittedFunction emitted = transpile(
        "function r = f(A)\n"
        "  v = A(1,:);\n"   // row 1 = [1 2 3]
        "  r = v(1) + v(2)*10 + v(3)*100 + numel(v)*1000;\n"
        "end\n",
        {{"A", InferredType::concrete(ValueType::DOUBLE, Shape::dims(2, 3))}});

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_rowslice_e2e.exe").string();
    const std::string outTxt = (base / "nk_rowslice_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double A[6] = {1, 4, 2, 5, 3, 6};\n"  // 2x3 col-major
        "  double r = f(A, 2, 3);\n"  // row 1 = [1 2 3]: 1 + 20 + 300 + 3000 = 3321
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", r);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 1.0 + 20.0 + 300.0 + 3000.0);  // 3321
}

// reshape(x, m, n): a vector reinterpreted as an m x n matrix (column-major).
// reshape([1..6], 2, 3) = [1 3 5; 2 4 6]; verified via 2-D indexing of the result.
TEST(CodegenE2E, Reshape)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const EmittedFunction emitted = transpile(
        "function r = f(x)\n"
        "  M = reshape(x, 2, 3);\n"   // 2x3 col-major from [1..6]
        "  r = M(1,1) + M(2,1)*10 + M(1,2)*100 + M(2,3)*1000 + numel(M)*10000;\n"
        "end\n",
        {{"x", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}});
    EXPECT_NE(emitted.source.find("reshape element count mismatch"), std::string::npos)
        << "reshape must materialise a 2-D local with a numel-match check, not refuse";

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_reshape_e2e.exe").string();
    const std::string outTxt = (base / "nk_reshape_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double x[6] = {1, 2, 3, 4, 5, 6};\n"
        "  double r = f(x, 6);\n"  // M=[1 3 5; 2 4 6]: 1 + 20 + 300 + 6000 + 60000 = 66321
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", r);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 1.0 + 20.0 + 300.0 + 6000.0 + 60000.0);  // 66321
}

// Nested struct fields s.a.b (the config-struct pattern): field-flattening via the
// chain helper, generalised from single-level. s.a.b=2; s.a.c=3 -> 2*10 + 3 = 23.
TEST(CodegenE2E, NestedStruct)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const EmittedFunction emitted = transpile(
        "function r = f()\n"
        "  s.a.b = 2;\n"
        "  s.a.c = 3;\n"
        "  r = s.a.b * 10 + s.a.c;\n"
        "end\n",
        {});
    EXPECT_NE(emitted.source.find("_nk_fld_s_a_b"), std::string::npos)
        << "a nested field must flatten to a chained field-local";

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_nstruct_e2e.exe").string();
    const std::string outTxt = (base / "nk_nstruct_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double r = f();\n"  // 2*10 + 3 = 23
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", r);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 23.0);
}

// repmat(s, m, n) with s scalar -> an m x n matrix all = s. repmat(7, 2, 3) -> 2x3
// of 7s; indexed + numel.
TEST(CodegenE2E, RepmatScalar)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const EmittedFunction emitted = transpile(
        "function r = f()\n"
        "  M = repmat(7, 2, 3);\n"   // 2x3 all 7
        "  r = M(1,1) + M(2,3) + numel(M)*10;\n"
        "end\n",
        {});
    EXPECT_EQ(emitted.source.find("repmat"), std::string::npos)
        << "repmat must be lowered to a native fill, not left as a call";

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_repmat_e2e.exe").string();
    const std::string outTxt = (base / "nk_repmat_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double r = f();\n"  // 7 + 7 + 6*10 = 74
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", r);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 74.0);  // 7 + 7 + 60
}

// diag(A): the diagonal of a 2-D matrix as a vector. A = [1 3 5; 2 4 6] (col-major
// {1,2,3,4,5,6}); diag(A) = [1; 4] (the (1,1) and (2,2) elements).
TEST(CodegenE2E, DiagOfMatrix)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const EmittedFunction emitted = transpile(
        "function r = f(A)\n"
        "  d = diag(A);\n"   // [1 4]
        "  r = d(1) + d(2)*10 + numel(d)*100;\n"
        "end\n",
        {{"A", InferredType::concrete(ValueType::DOUBLE, Shape::dims(2, 3))}});

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_diag_e2e.exe").string();
    const std::string outTxt = (base / "nk_diag_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double A[6] = {1, 2, 3, 4, 5, 6};\n"  // 2x3 col-major
        "  double r = f(A, 2, 3);\n"  // diag = [1 4]: 1 + 40 + 2*100 = 241
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", r);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 1.0 + 40.0 + 200.0);  // 241
}

// trace(A): the sum of the diagonal of a 2-D matrix. A = diag([1 5 9]) (3x3) ->
// trace = 1 + 5 + 9 = 15.
TEST(CodegenE2E, TraceOfMatrix)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const EmittedFunction emitted = transpile(
        "function r = f(A)\n"
        "  r = trace(A);\n"
        "end\n",
        {{"A", InferredType::concrete(ValueType::DOUBLE, Shape::dims(3, 3))}});

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_trace_e2e.exe").string();
    const std::string outTxt = (base / "nk_trace_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double A[9] = {1,0,0, 0,5,0, 0,0,9};\n"  // 3x3 col-major, diagonal 1,5,9
        "  double r = f(A, 3, 3);\n"  // trace = 1 + 5 + 9 = 15
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", r);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 15.0);
}

// eye(n): the identity matrix. eye(3) -> 3x3 with 1 on the diagonal, 0 off it.
TEST(CodegenE2E, EyeIdentity)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const EmittedFunction emitted = transpile(
        "function r = f()\n"
        "  M = eye(3);\n"
        "  r = M(1,1) + M(2,2)*10 + M(3,3)*100 + M(1,2)*1000 + numel(M)*10000;\n"
        "end\n",
        {});

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_eye_e2e.exe").string();
    const std::string outTxt = (base / "nk_eye_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double r = f();\n"  // diag 1,1,1; M(1,2)=0; numel 9: 1 + 10 + 100 + 0 + 90000 = 90111
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", r);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 1.0 + 10.0 + 100.0 + 0.0 + 90000.0);  // 90111
}

// tril(A): the lower triangular part of a 2-D matrix (upper triangle zeroed).
// A = [1 2; 3 4] (col-major {1,3,2,4}); tril(A) = [1 0; 3 4].
TEST(CodegenE2E, TrilLower)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const EmittedFunction emitted = transpile(
        "function r = f(A)\n"
        "  L = tril(A);\n"   // [1 0; 3 4]
        "  r = L(1,1) + L(2,1)*10 + L(1,2)*100 + L(2,2)*1000;\n"
        "end\n",
        {{"A", InferredType::concrete(ValueType::DOUBLE, Shape::dims(2, 2))}});

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_tril_e2e.exe").string();
    const std::string outTxt = (base / "nk_tril_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double A[4] = {1, 3, 2, 4};\n"  // [1 2; 3 4] col-major
        "  double r = f(A, 2, 2);\n"  // tril=[1 0; 3 4]: 1 + 30 + 0 + 4000 = 4031
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", r);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 1.0 + 30.0 + 0.0 + 4000.0);  // 4031
}

// diag(v): a vector -> a diagonal MATRIX (runtime-dim 2-D). diag([5 6 7]) -> 3x3 with
// [5 6 7] on the diagonal, 0 off it.
TEST(CodegenE2E, DiagOfVector)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const EmittedFunction emitted = transpile(
        "function r = f(v)\n"
        "  M = diag(v);\n"   // 3x3 diagonal
        "  r = M(1,1) + M(2,2)*10 + M(3,3)*100 + M(1,2)*1000 + numel(M)*10000;\n"
        "end\n",
        {{"v", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}});

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_diagv_e2e.exe").string();
    const std::string outTxt = (base / "nk_diagv_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double v[3] = {5, 6, 7};\n"
        "  double r = f(v, 3);\n"  // diag: 5 + 60 + 700 + 0 + 9*10000 = 90765
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", r);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 5.0 + 60.0 + 700.0 + 0.0 + 90000.0);  // 90765
}

// repmat(rowVec, p, q) with p>1 -> a true 2-D p x (q*n) tiling (runtime-dim 2-D).
// repmat([10 20 30], 2, 2) -> a 2x6 matrix: each row is [10 20 30 10 20 30].
TEST(CodegenE2E, RepmatRowVectorTo2D)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const EmittedFunction emitted = transpile(
        "function r = f(v)\n"
        "  M = repmat(v, 2, 2);\n"   // 2 x 6
        "  r = M(1,1) + M(2,3)*10 + M(1,4)*100 + M(2,6)*1000 + numel(M)*100000;\n"
        "end\n",
        {{"v", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}});

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_repmat2d_e2e.exe").string();
    const std::string outTxt = (base / "nk_repmat2d_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double v[3] = {10, 20, 30};\n"
        "  double r = f(v, 3);\n"  // 10 + 30*10 + 10*100 + 30*1000 + 12*100000 = 1231310
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", r);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 10.0 + 300.0 + 1000.0 + 30000.0 + 1200000.0);  // 1231310
}

// vertcat-of-rows: `M = [a; b]` with a,b row vectors -> a k x n 2-D matrix (runtime-dim
// 2-D, stacking rows). [1 2 3; 4 5 6] -> a 2x3 matrix.
TEST(CodegenE2E, VertcatRowsTo2D)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const EmittedFunction emitted = transpile(
        "function r = f(a, b)\n"
        "  M = [a; b];\n"   // 2 x 3
        "  r = M(1,1) + M(1,2)*10 + M(1,3)*100 + M(2,1)*1000 + M(2,2)*10000 "
        "+ M(2,3)*100000 + numel(M)*1000000;\n"
        "end\n",
        {{"a", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())},
         {"b", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}});

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_vertcat2d_e2e.exe").string();
    const std::string outTxt = (base / "nk_vertcat2d_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double a[3] = {1, 2, 3};\n"
        "  double b[3] = {4, 5, 6};\n"
        "  double r = f(a, 3, b, 3);\n"  // [1 2 3; 4 5 6]: 1+20+300+4000+50000+600000+6e6
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", r);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 6654321.0);
}

// horzcat-of-columns: `M = [a b]` with a,b column vectors -> an n x k 2-D matrix
// (runtime-dim 2-D, placing columns side by side). [1;2;3] and [4;5;6] -> [1 4; 2 5; 3 6].
TEST(CodegenE2E, HorzcatColumnsTo2D)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const EmittedFunction emitted = transpile(
        "function r = f(a, b)\n"
        "  M = [a b];\n"   // 3 x 2
        "  r = M(1,1) + M(3,1)*10 + M(1,2)*100 + M(3,2)*1000 + numel(M)*10000;\n"
        "end\n",
        {{"a", InferredType::concrete(ValueType::DOUBLE, Shape::colVector())},
         {"b", InferredType::concrete(ValueType::DOUBLE, Shape::colVector())}});

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_horzcat2d_e2e.exe").string();
    const std::string outTxt = (base / "nk_horzcat2d_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double a[3] = {1, 2, 3};\n"
        "  double b[3] = {4, 5, 6};\n"
        "  double r = f(a, 3, b, 3);\n"  // [1 4; 2 5; 3 6]: 1 + 30 + 400 + 6000 + 60000 = 66431
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", r);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 66431.0);
}

// transpose of a runtime-dim 2-D matrix: M = [a; b] (2x3), T = M' (3x2). Chains
// vertcat-of-rows with transpose -> validates the runtime-dim 2-D transpose end to end.
TEST(CodegenE2E, TransposeRuntimeDim2D)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const EmittedFunction emitted = transpile(
        "function r = f(a, b)\n"
        "  M = [a; b];\n"   // 2 x 3: [1 2 3; 4 5 6]
        "  T = M';\n"       // 3 x 2: [1 4; 2 5; 3 6]
        "  r = T(1,1) + T(1,2)*10 + T(3,1)*100 + T(3,2)*1000 + numel(T)*10000;\n"
        "end\n",
        {{"a", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())},
         {"b", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}});

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_transpose2d_e2e.exe").string();
    const std::string outTxt = (base / "nk_transpose2d_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double a[3] = {1, 2, 3};\n"
        "  double b[3] = {4, 5, 6};\n"
        "  double r = f(a, 3, b, 3);\n"  // T=[1 4;2 5;3 6]: 1 + 40 + 300 + 6000 + 60000 = 66341
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", r);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 66341.0);
}

// element-wise binary on two runtime-dim 2-D matrices: A = [a;b], B = [b;a], C = A + B.
// Validates that the elementwise result's own dim companions are set (numel/indexing of C
// must be correct, not just its flat buffer). A=[1 2 3;4 5 6], B=[4 5 6;1 2 3] -> C=[5 7 9;5 7 9].
TEST(CodegenE2E, ElementwiseAddRuntimeDim2D)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const EmittedFunction emitted = transpile(
        "function r = f(a, b)\n"
        "  A = [a; b];\n"   // 2x3: [1 2 3; 4 5 6]
        "  B = [b; a];\n"   // 2x3: [4 5 6; 1 2 3]
        "  C = A + B;\n"    // 2x3: [5 7 9; 5 7 9]
        "  r = C(1,1) + C(1,2)*10 + C(1,3)*100 + C(2,1)*1000 + numel(C)*100000;\n"
        "end\n",
        {{"a", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())},
         {"b", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}});

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_ewadd2d_e2e.exe").string();
    const std::string outTxt = (base / "nk_ewadd2d_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double a[3] = {1, 2, 3};\n"
        "  double b[3] = {4, 5, 6};\n"
        "  double r = f(a, 3, b, 3);\n"  // C=[5 7 9;5 7 9]: 5 + 70 + 900 + 5000 + 600000 = 605975
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", r);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 605975.0);
}

// matmul A*B of two runtime-dim 2-D matrices. A=[1 2;3 4], B=[5 6;7 8] (built by
// vertcat-of-rows) -> C = A*B = [19 22; 43 50]. Exercises the triple-loop with
// cross-term accumulation + sets the runtime dst's dim companions.
TEST(CodegenE2E, MatmulRuntimeDim2D)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const EmittedFunction emitted = transpile(
        "function r = f(a1, a2, b1, b2)\n"
        "  A = [a1; a2];\n"   // 2x2: [1 2; 3 4]
        "  B = [b1; b2];\n"   // 2x2: [5 6; 7 8]
        "  C = A * B;\n"      // 2x2: [19 22; 43 50]
        "  r = C(1,1) + C(1,2)*10 + C(2,1)*100 + C(2,2)*1000 + numel(C)*10000;\n"
        "end\n",
        {{"a1", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())},
         {"a2", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())},
         {"b1", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())},
         {"b2", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}});

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_matmul2d_e2e.exe").string();
    const std::string outTxt = (base / "nk_matmul2d_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double a1[2] = {1, 2};\n"
        "  double a2[2] = {3, 4};\n"
        "  double b1[2] = {5, 6};\n"
        "  double b2[2] = {7, 8};\n"
        "  double r = f(a1, 2, a2, 2, b1, 2, b2, 2);\n"  // C=[19 22;43 50]: 94539
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", r);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 19.0 + 220.0 + 4300.0 + 50000.0 + 40000.0);  // 94539
}

// slicing a runtime-dim 2-D matrix: M = [a;b] (2x3), column M(:,2) and row M(1,:).
// Validates A(:,j) (contiguous) and A(i,:) (strided) on a runtime-dim 2-D operand.
TEST(CodegenE2E, SliceRuntimeDim2D)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const EmittedFunction emitted = transpile(
        "function r = f(a, b)\n"
        "  M = [a; b];\n"     // 2x3: [1 2 3; 4 5 6]
        "  c = M(:, 2);\n"    // column 2: [2; 5]
        "  rr = M(1, :);\n"   // row 1:    [1 2 3]
        "  r = c(1) + c(2)*10 + rr(1)*100 + rr(2)*1000 + rr(3)*10000;\n"
        "end\n",
        {{"a", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())},
         {"b", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}});

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_slice2d_e2e.exe").string();
    const std::string outTxt = (base / "nk_slice2d_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double a[3] = {1, 2, 3};\n"
        "  double b[3] = {4, 5, 6};\n"
        "  double r = f(a, 3, b, 3);\n"  // c=[2;5], rr=[1 2 3]: 2 + 50 + 100 + 2000 + 30000 = 32152
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", r);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 2.0 + 50.0 + 100.0 + 2000.0 + 30000.0);  // 32152
}

// matrix*vector A*x with a runtime-dim 2-D matrix. A=[1 2;3 4] (vertcat), x=[5;6]
// -> y = A*x = [17; 39]. Exercises matrix*vector lowering on a runtime-dim 2-D operand.
TEST(CodegenE2E, MatrixVectorRuntimeDim2D)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const EmittedFunction emitted = transpile(
        "function r = f(a1, a2, x)\n"
        "  A = [a1; a2];\n"  // 2x2: [1 2; 3 4]
        "  y = A * x;\n"     // 2x1: [1*5+2*6; 3*5+4*6] = [17; 39]
        "  r = y(1) + y(2)*10;\n"
        "end\n",
        {{"a1", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())},
         {"a2", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())},
         {"x", InferredType::concrete(ValueType::DOUBLE, Shape::colVector())}});

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_matvec2d_e2e.exe").string();
    const std::string outTxt = (base / "nk_matvec2d_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double a1[2] = {1, 2};\n"
        "  double a2[2] = {3, 4};\n"
        "  double x[2]  = {5, 6};\n"
        "  double r = f(a1, 2, a2, 2, x, 2);\n"  // y=[17;39]: 17 + 39*10 = 407
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", r);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 407.0);
}

// The CANONICAL matrix-fill pattern on a runtime-dim 2-D: A = zeros(m,n); nested loop
// A(i,j) = 10*i + j; then read back. Ties together construction + scalar indexed WRITE
// (nk_rt::indexN_set) + read + numel on a runtime-dim 2-D matrix.
TEST(CodegenE2E, IndexedWriteFillRuntimeDim2D)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const EmittedFunction emitted = transpile(
        "function r = f(m, n)\n"
        "  A = zeros(m, n);\n"
        "  for i = 1:m\n"
        "    for j = 1:n\n"
        "      A(i, j) = 10*i + j;\n"
        "    end\n"
        "  end\n"
        "  r = A(1,1) + A(2,3)*10 + A(2,1)*100 + numel(A)*1000;\n"
        "end\n",
        {{"m", InferredType::scalar(ValueType::DOUBLE)},
         {"n", InferredType::scalar(ValueType::DOUBLE)}});

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_fill2d_e2e.exe").string();
    const std::string outTxt = (base / "nk_fill2d_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double r = f(2, 3);\n"  // A(1,1)=11, A(2,3)=23, A(2,1)=21, numel=6: 11+230+2100+6000
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", r);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 11.0 + 230.0 + 2100.0 + 6000.0);  // 8341
}

// 2-D column slice WRITE into a runtime-dim 2-D: A = zeros(3,2); A(:,2) = col -> overwrite
// column 2 (contiguous, column-major). Builds a runtime matrix column-by-column.
TEST(CodegenE2E, ColumnSliceWriteRuntimeDim2D)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const EmittedFunction emitted = transpile(
        "function r = f(m, n, col)\n"
        "  A = zeros(m, n);\n"   // 3x2 zeros
        "  A(:, 2) = col;\n"     // overwrite column 2 with col = [7;8;9]
        "  r = A(1,1) + A(1,2)*10 + A(2,2)*100 + A(m,2)*1000 + numel(A)*10000;\n"
        "end\n",
        {{"m", InferredType::scalar(ValueType::DOUBLE)},
         {"n", InferredType::scalar(ValueType::DOUBLE)},
         {"col", InferredType::concrete(ValueType::DOUBLE, Shape::colVector())}});

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_colwrite2d_e2e.exe").string();
    const std::string outTxt = (base / "nk_colwrite2d_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double col[3] = {7, 8, 9};\n"
        "  double r = f(3, 2, col, 3);\n"  // A(:,1)=0, A(:,2)=[7;8;9], numel=6: 0+70+800+9000+60000
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", r);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 0.0 + 70.0 + 800.0 + 9000.0 + 60000.0);  // 69870
}

// 2-D row slice WRITE into a runtime-dim 2-D: A = zeros(2,3); A(2,:) = row -> overwrite
// row 2 (strided, column-major). The mirror of the column write; completes the pair.
TEST(CodegenE2E, RowSliceWriteRuntimeDim2D)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const EmittedFunction emitted = transpile(
        "function r = f(m, n, row)\n"
        "  A = zeros(m, n);\n"   // 2x3 zeros
        "  A(2, :) = row;\n"     // overwrite row 2 with row = [7 8 9]
        "  r = A(1,1) + A(2,1)*10 + A(2,2)*100 + A(2,n)*1000 + numel(A)*10000;\n"
        "end\n",
        {{"m", InferredType::scalar(ValueType::DOUBLE)},
         {"n", InferredType::scalar(ValueType::DOUBLE)},
         {"row", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}});

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_rowwrite2d_e2e.exe").string();
    const std::string outTxt = (base / "nk_rowwrite2d_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double row[3] = {7, 8, 9};\n"
        "  double r = f(2, 3, row, 3);\n"  // A(1,1)=0, A(2,:)=[7 8 9], numel=6: 0+70+800+9000+60000
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", r);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 0.0 + 70.0 + 800.0 + 9000.0 + 60000.0);  // 69870
}

// 2-D horzcat of MATRICES: C = [A B] with A,B runtime-dim 2-D (built by vertcat) ->
// a wider matrix (buffers concatenated in column-major). A=[1 2;3 4], B=[5 6;7 8] ->
// C = [1 2 5 6; 3 4 7 8] (2x4).
TEST(CodegenE2E, HorzcatMatricesRuntimeDim2D)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const EmittedFunction emitted = transpile(
        "function r = f(a1, a2, b1, b2)\n"
        "  A = [a1; a2];\n"  // 2x2: [1 2; 3 4]
        "  B = [b1; b2];\n"  // 2x2: [5 6; 7 8]
        "  C = [A B];\n"     // 2x4: [1 2 5 6; 3 4 7 8]
        "  r = C(1,1) + C(1,2)*10 + C(1,3)*100 + C(1,4)*1000 + C(2,4)*10000 + numel(C)*100000;\n"
        "end\n",
        {{"a1", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())},
         {"a2", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())},
         {"b1", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())},
         {"b2", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}});

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_horzcatmat_e2e.exe").string();
    const std::string outTxt = (base / "nk_horzcatmat_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double a1[2] = {1, 2};\n"
        "  double a2[2] = {3, 4};\n"
        "  double b1[2] = {5, 6};\n"
        "  double b2[2] = {7, 8};\n"
        "  double r = f(a1, 2, a2, 2, b1, 2, b2, 2);\n"  // C=[1 2 5 6;3 4 7 8]: 1+20+500+6000+80000+800000
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", r);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 1.0 + 20.0 + 500.0 + 6000.0 + 80000.0 + 800000.0);  // 886521
}

// 2-D vertcat of MATRICES: C = [A; B] with A,B runtime-dim 2-D (built by vertcat-of-rows)
// -> a taller matrix (strided interleave in column-major). A=[1 2;3 4], B=[5 6;7 8] ->
// C = [1 2; 3 4; 5 6; 7 8] (4x2).
TEST(CodegenE2E, VertcatMatricesRuntimeDim2D)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const EmittedFunction emitted = transpile(
        "function r = f(a1, a2, b1, b2)\n"
        "  A = [a1; a2];\n"  // 2x2: [1 2; 3 4]
        "  B = [b1; b2];\n"  // 2x2: [5 6; 7 8]
        "  C = [A; B];\n"    // 4x2: [1 2; 3 4; 5 6; 7 8]
        "  r = C(1,1) + C(2,1)*10 + C(3,1)*100 + C(4,1)*1000 + C(4,2)*10000 + numel(C)*100000;\n"
        "end\n",
        {{"a1", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())},
         {"a2", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())},
         {"b1", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())},
         {"b2", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}});

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_vertcatmat_e2e.exe").string();
    const std::string outTxt = (base / "nk_vertcatmat_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double a1[2] = {1, 2};\n"
        "  double a2[2] = {3, 4};\n"
        "  double b1[2] = {5, 6};\n"
        "  double b2[2] = {7, 8};\n"
        "  double r = f(a1, 2, a2, 2, b1, 2, b2, 2);\n"  // C=[1 2;3 4;5 6;7 8]: 1+30+500+7000+80000+800000
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", r);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 1.0 + 30.0 + 500.0 + 7000.0 + 80000.0 + 800000.0);  // 887531
}

// CAPSTONE differential: compose the whole runtime-dim 2-D tier into one kernel --
// vertcat-of-rows -> matmul -> transpose -> [C D] horzcat -> [C;D] vertcat -> slice +
// numel -- and assert the codegen'd result equals BOTH the hand-computed value AND what
// the INTERPRETER produces for the same program (DESIGN.md §10 diff-vs-interpreter,
// applied across the full feature stack). A cross-feature-interaction guard.
TEST(CodegenE2E, RuntimeDim2DCompositionMatchesInterpreter)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const char *body =
        "  A = [a1; a2];\n"   // 2x2 [1 2; 3 4]
        "  B = [b1; b2];\n"   // 2x2 [5 6; 7 8]
        "  C = A * B;\n"      // [19 22; 43 50]
        "  D = C';\n"         // [19 43; 22 50]
        "  E = [C D];\n"      // 2x4
        "  F = [C; D];\n"     // 4x2
        "  r = E(1,1) + E(2,4) + F(3,1) + F(4,2) + numel(E) + numel(F);\n";

    const EmittedFunction emitted = transpile(
        std::string("function r = f(a1, a2, b1, b2)\n") + body + "end\n",
        {{"a1", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())},
         {"a2", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())},
         {"b1", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())},
         {"b2", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}});

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_compose2d_e2e.exe").string();
    const std::string outTxt = (base / "nk_compose2d_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double a1[2] = {1, 2};\n"
        "  double a2[2] = {3, 4};\n"
        "  double b1[2] = {5, 6};\n"
        "  double b2[2] = {7, 8};\n"
        "  double r = f(a1, 2, a2, 2, b1, 2, b2, 2);\n"
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", r);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 154.0);  // 19 + 50 + 19 + 50 + 8 + 8

    // Differential: the interpreter must compute the same for the same program.
    numkit::StandardEngine engine;
    const double interp =
        engine.eval(std::string("a1=[1 2]; a2=[3 4]; b1=[5 6]; b2=[7 8];\n") + body + "r", true)
            .toScalar();
    EXPECT_DOUBLE_EQ(got[0], interp);
}

// BENCHMARK capstone: time a compute-heavy runtime-dim 2-D kernel (K matmuls of 2x2
// matrices, accumulating C(1,1)) in the codegen-compiled binary vs the numkit interpreter
// over the SAME .m. Asserts correctness (codegen result == interpreter == K*19, the real
// guard) and LOGS the speedup (informational -- machine-dependent, never asserted, per the
// CLAUDE.md perf rule). First matrix-tier perf evidence (Brick 7 was scalar biquad).
TEST(CodegenE2E, RuntimeDim2DKernelVsInterpreter)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    constexpr int K = 20000;
    const char   *body =
        "  A = [a1; a2];\n"       // 2x2 [1 2; 3 4]
        "  B = [b1; b2];\n"       // 2x2 [5 6; 7 8]
        "  s = 0;\n"
        "  for i = 1:20000\n"     // K matmuls (literal bound -> no scalar param)
        "    C = A * B;\n"
        "    s = s + C(1,1);\n"   // C(1,1) = 1*5 + 2*7 = 19 each iter
        "  end\n"
        "  r = s;\n";
    const EmittedFunction emitted = transpile(
        std::string("function r = f(a1, a2, b1, b2)\n") + body + "end\n",
        {{"a1", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())},
         {"a2", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())},
         {"b1", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())},
         {"b2", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}});

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_bench2d_e2e.exe").string();
    const std::string outTxt = (base / "nk_bench2d_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "#include <chrono>\n"
        "int main() {\n"
        "  double a1[2] = {1, 2};\n"
        "  double a2[2] = {3, 4};\n"
        "  double b1[2] = {5, 6};\n"
        "  double b2[2] = {7, 8};\n"
        "  auto t0 = std::chrono::steady_clock::now();\n"
        "  double r = f(a1, 2, a2, 2, b1, 2, b2, 2);\n"
        "  auto t1 = std::chrono::steady_clock::now();\n"
        "  double ns = std::chrono::duration<double, std::nano>(t1 - t0).count();\n"
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n%.17g\\n\", r, ns);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 2u);
    const double codegenResult = got[0];
    const double codegenNs     = got[1];
    EXPECT_DOUBLE_EQ(codegenResult, static_cast<double>(K) * 19.0);  // s = K * C(1,1)

    // Interpreter: run + time the same program.
    numkit::StandardEngine engine;
    const std::string interpSrc =
        "a1=[1 2]; a2=[3 4]; b1=[5 6]; b2=[7 8];\n" + std::string(body) + "r";
    const auto   i0          = std::chrono::steady_clock::now();
    const double interpRes   = engine.eval(interpSrc, true).toScalar();
    const auto   i1          = std::chrono::steady_clock::now();
    const double interpNs    = std::chrono::duration<double, std::nano>(i1 - i0).count();
    EXPECT_DOUBLE_EQ(codegenResult, interpRes);  // correctness diff vs interpreter (the guard)

    std::cout << "[ BENCH    ] runtime-2-D 2x2 matmul x" << K << ": codegen="
              << (codegenNs / 1e6) << " ms, interpreter=" << (interpNs / 1e6)
              << " ms, speedup=" << (codegenNs > 0 ? interpNs / codegenNs : 0.0) << "x\n";
}

// Native rem (scalar, real): truncated remainder, sign of the dividend. Lowered to
// std::fmod, bit-identical to numkit's scalar rem (misc.cpp). Asserts the codegen result
// == hand value == interpreter, including signed operands. (mod stays bridged by design.)
TEST(CodegenE2E, RemScalarNative)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const char *body = "  r = rem(a,b)*1000 + rem(b,a)*10 + rem(7, 3);\n";
    const EmittedFunction emitted = transpile(
        std::string("function r = f(a, b)\n") + body + "end\n",
        {{"a", InferredType::scalar(ValueType::DOUBLE)},
         {"b", InferredType::scalar(ValueType::DOUBLE)}});

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_rem_e2e.exe").string();
    const std::string outTxt = (base / "nk_rem_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double r = f(-7, 3);\n"  // rem(-7,3)=-1, rem(3,-7)=3, rem(7,3)=1: -1000+30+1 = -969
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", r);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], -969.0);

    numkit::StandardEngine engine;
    const double interp = engine.eval(std::string("a=-7; b=3;\n") + body + "r", true).toScalar();
    EXPECT_DOUBLE_EQ(got[0], interp);
}

// Native deg2rad / rad2deg (scalar, real): a constant scaling x*(pi/180) / x*(180/pi),
// inlined with numkit's exact constant so it is bit-identical to the interpreter. (Array/
// complex args stay bridged.)
TEST(CodegenE2E, Deg2RadRad2DegScalarNative)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const char *body = "  r = deg2rad(d)*100 + rad2deg(d);\n";
    const EmittedFunction emitted = transpile(
        std::string("function r = f(d)\n") + body + "end\n",
        {{"d", InferredType::scalar(ValueType::DOUBLE)}});

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_deg2rad_e2e.exe").string();
    const std::string outTxt = (base / "nk_deg2rad_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double r = f(90);\n"  // deg2rad(90)=pi/2, rad2deg(90)=90*180/pi
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", r);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    // independent sanity (loose) + bit-exact match vs the interpreter (the real guard).
    EXPECT_NEAR(got[0], (3.14159265358979323846 / 2.0) * 100.0 + 90.0 * 180.0 / 3.14159265358979323846,
                1e-9);
    numkit::StandardEngine engine;
    const double interp = engine.eval(std::string("d=90;\n") + body + "r", true).toScalar();
    EXPECT_DOUBLE_EQ(got[0], interp);
}

// Native gammaln = std::lgamma (real-total log-gamma). Lowers via unaryMathStd, so it
// works both scalar and elementwise; bit-identical to numkit's gammaln (special.cpp).
TEST(CodegenE2E, GammalnScalarNative)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const char *body = "  r = gammaln(a) + gammaln(b)*10;\n";
    const EmittedFunction emitted = transpile(
        std::string("function r = f(a, b)\n") + body + "end\n",
        {{"a", InferredType::scalar(ValueType::DOUBLE)},
         {"b", InferredType::scalar(ValueType::DOUBLE)}});

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_gammaln_e2e.exe").string();
    const std::string outTxt = (base / "nk_gammaln_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double r = f(5, 3);\n"  // lgamma(5)=ln(24), lgamma(3)=ln(2)
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", r);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_NEAR(got[0], std::lgamma(5.0) + std::lgamma(3.0) * 10.0, 1e-9);  // independent
    numkit::StandardEngine engine;
    const double interp = engine.eval(std::string("a=5; b=3;\n") + body + "r", true).toScalar();
    EXPECT_DOUBLE_EQ(got[0], interp);  // bit-exact vs interpreter
}

// repmat(A, p, q) of a MATRIX -> a (p*Arows) x (q*Acols) block tiling. A=[1 2;3 4] (runtime-
// dim, via vertcat), repmat(.,2,2) -> [A A; A A] (4x4). value(R,C) = A[(R%2) + (C%2)*2].
TEST(CodegenE2E, RepmatMatrixBlockTiling)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const char *body =
        "  A = [a1; a2];\n"        // 2x2 [1 2; 3 4]
        "  M = repmat(A, 2, 2);\n"  // 4x4 block tiling
        "  r = M(1,1) + M(2,2)*10 + M(3,3)*100 + M(4,4)*1000 + M(4,2)*10000 + numel(M)*100000;\n";
    const EmittedFunction emitted = transpile(
        std::string("function r = f(a1, a2)\n") + body + "end\n",
        {{"a1", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())},
         {"a2", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}});

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_repmatmat_e2e.exe").string();
    const std::string outTxt = (base / "nk_repmatmat_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double a1[2] = {1, 2};\n"
        "  double a2[2] = {3, 4};\n"
        "  double r = f(a1, 2, a2, 2);\n"  // [1 2 1 2;3 4 3 4;1 2 1 2;3 4 3 4]: 1+40+100+4000+40000+1.6e6
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", r);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    // M(1,1)=1, M(2,2)=4, M(3,3)=1, M(4,4)=4, M(4,2)=4, numel=16
    EXPECT_DOUBLE_EQ(got[0], 1.0 + 40.0 + 100.0 + 4000.0 + 40000.0 + 1600000.0);  // 1644141
    numkit::StandardEngine engine;
    const double interp =
        engine.eval(std::string("a1=[1 2]; a2=[3 4];\n") + body + "r", true).toScalar();
    EXPECT_DOUBLE_EQ(got[0], interp);
}

// repmat(colVec, p, q) with q>1 -> a (p*n) x q 2-D tiling (the mirror of the rowVec tile).
// x=[10;20;30], repmat(.,2,2) -> 6x2, each column [10;20;30;10;20;30].
TEST(CodegenE2E, RepmatColVectorTo2D)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const char *body =
        "  M = repmat(x, 2, 2);\n"  // 6 x 2
        "  r = M(1,1) + M(3,1)*10 + M(4,1)*100 + M(6,2)*1000 + numel(M)*10000;\n";
    const EmittedFunction emitted = transpile(
        std::string("function r = f(x)\n") + body + "end\n",
        {{"x", InferredType::concrete(ValueType::DOUBLE, Shape::colVector())}});

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_repmatcol_e2e.exe").string();
    const std::string outTxt = (base / "nk_repmatcol_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double x[3] = {10, 20, 30};\n"
        "  double r = f(x, 3);\n"  // col=[10;20;30;10;20;30]: 10 + 300 + 1000 + 30000 + 120000
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", r);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 10.0 + 300.0 + 1000.0 + 30000.0 + 120000.0);  // 151310
    numkit::StandardEngine engine;
    const double interp =
        engine.eval(std::string("x=[10;20;30];\n") + body + "r", true).toScalar();
    EXPECT_DOUBLE_EQ(got[0], interp);
}

// runtime reshape(x, m, n): reinterpret x's flat data as an m x n matrix at a RUNTIME
// size (column-major, same buffer). x=[1..6], reshape(.,2,3) -> [1 3 5; 2 4 6].
TEST(CodegenE2E, RuntimeReshape)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const char *body =
        "  M = reshape(x, m, n);\n"  // 2 x 3, column-major from [1..6]
        "  r = M(1,1) + M(2,1)*10 + M(1,2)*100 + M(2,3)*1000 + numel(M)*10000;\n";
    const EmittedFunction emitted = transpile(
        std::string("function r = f(x, m, n)\n") + body + "end\n",
        {{"x", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())},
         {"m", InferredType::scalar(ValueType::DOUBLE)},
         {"n", InferredType::scalar(ValueType::DOUBLE)}});

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_reshape_e2e.exe").string();
    const std::string outTxt = (base / "nk_reshape_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double x[6] = {1, 2, 3, 4, 5, 6};\n"
        "  double r = f(x, 6, 2, 3);\n"  // M=[1 3 5;2 4 6]: 1 + 20 + 300 + 6000 + 60000 = 66321
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", r);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 1.0 + 20.0 + 300.0 + 6000.0 + 60000.0);  // 66321
    numkit::StandardEngine engine;
    const double interp =
        engine.eval(std::string("x=[1 2 3 4 5 6]; m=2; n=3;\n") + body + "r", true).toScalar();
    EXPECT_DOUBLE_EQ(got[0], interp);
}

// BLOCK-matrix literal [A B; B A]: a 2x2 grid of 2x2 runtime-dim blocks -> a 4x4 matrix.
// Distinct blocks swap across the anti-diagonal so the row/col offset math is exercised.
// A=[1 2;3 4], B=[5 6;7 8] -> [1 2 5 6; 3 4 7 8; 5 6 1 2; 7 8 3 4].
TEST(CodegenE2E, BlockMatrixLiteralRuntimeDim2D)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const char *body =
        "  A = [a1; a2];\n"   // 2x2 [1 2; 3 4]
        "  B = [b1; b2];\n"   // 2x2 [5 6; 7 8]
        "  M = [A B; B A];\n"  // 4x4 block matrix
        "  r = M(1,1) + M(1,3)*10 + M(3,1)*100 + M(3,3)*1000 + M(2,4)*10000 + numel(M)*100000;\n";
    const EmittedFunction emitted = transpile(
        std::string("function r = f(a1, a2, b1, b2)\n") + body + "end\n",
        {{"a1", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())},
         {"a2", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())},
         {"b1", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())},
         {"b2", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}});

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_blockmat_e2e.exe").string();
    const std::string outTxt = (base / "nk_blockmat_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double a1[2] = {1, 2};\n"
        "  double a2[2] = {3, 4};\n"
        "  double b1[2] = {5, 6};\n"
        "  double b2[2] = {7, 8};\n"
        "  double r = f(a1, 2, a2, 2, b1, 2, b2, 2);\n"
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", r);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    // M(1,1)=1, M(1,3)=5, M(3,1)=5, M(3,3)=1, M(2,4)=8, numel=16
    EXPECT_DOUBLE_EQ(got[0], 1.0 + 50.0 + 500.0 + 1000.0 + 80000.0 + 1600000.0);  // 1681551
    numkit::StandardEngine engine;
    const double interp =
        engine.eval(std::string("a1=[1 2]; a2=[3 4]; b1=[5 6]; b2=[7 8];\n") + body + "r", true)
            .toScalar();
    EXPECT_DOUBLE_EQ(got[0], interp);
}

// whole-array scalar fill A(:) = s on a runtime-dim 2-D matrix. A=[1 2;3 4]; A(:)=7 -> all 7.
TEST(CodegenE2E, WholeArrayScalarFill)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const char *body =
        "  A = [a1; a2];\n"  // 2x2 [1 2; 3 4]
        "  A(:) = 7;\n"      // fill every element with 7
        "  r = A(1,1) + A(2,2)*10 + A(1,2)*100 + numel(A)*1000;\n";
    const EmittedFunction emitted = transpile(
        std::string("function r = f(a1, a2)\n") + body + "end\n",
        {{"a1", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())},
         {"a2", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}});

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_fillscalar_e2e.exe").string();
    const std::string outTxt = (base / "nk_fillscalar_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double a1[2] = {1, 2};\n"
        "  double a2[2] = {3, 4};\n"
        "  double r = f(a1, 2, a2, 2);\n"  // A=[7 7;7 7]: 7 + 70 + 700 + 4000 = 4777
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", r);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 7.0 + 70.0 + 700.0 + 4000.0);  // 4777
    numkit::StandardEngine engine;
    const double interp =
        engine.eval(std::string("a1=[1 2]; a2=[3 4];\n") + body + "r", true).toScalar();
    EXPECT_DOUBLE_EQ(got[0], interp);
}

// whole-array copy B = A for a runtime-dim 2-D matrix: B is an independent value copy.
// A=[1 2;3 4] -> B=A, read back. (Also exercises the dim-companion copy.)
TEST(CodegenE2E, WholeMatrixCopy)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const char *body =
        "  A = [a1; a2];\n"  // 2x2 [1 2; 3 4]
        "  B = A;\n"         // whole-matrix value copy
        "  r = B(1,1) + B(2,2)*10 + B(1,2)*100 + numel(B)*1000;\n";
    const EmittedFunction emitted = transpile(
        std::string("function r = f(a1, a2)\n") + body + "end\n",
        {{"a1", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())},
         {"a2", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}});

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_matcopy_e2e.exe").string();
    const std::string outTxt = (base / "nk_matcopy_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double a1[2] = {1, 2};\n"
        "  double a2[2] = {3, 4};\n"
        "  double r = f(a1, 2, a2, 2);\n"  // B=[1 2;3 4]: 1 + 40 + 200 + 4000 = 4241
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", r);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 1.0 + 40.0 + 200.0 + 4000.0);  // 4241
    numkit::StandardEngine engine;
    const double interp =
        engine.eval(std::string("a1=[1 2]; a2=[3 4];\n") + body + "r", true).toScalar();
    EXPECT_DOUBLE_EQ(got[0], interp);
}

// row/column slice fill with a SCALAR (broadcast): A(:,j)=s fills column j, A(i,:)=s fills
// row i. A=[1 2;3 4] -> A(:,2)=9 -> [1 9;3 9] -> A(1,:)=5 -> [5 5;3 9].
TEST(CodegenE2E, RowColScalarFill)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const char *body =
        "  A = [a1; a2];\n"  // 2x2 [1 2; 3 4]
        "  A(:,2) = 9;\n"    // fill column 2 -> [1 9; 3 9]
        "  A(1,:) = 5;\n"    // fill row 1    -> [5 5; 3 9]
        "  r = A(1,1) + A(2,1)*10 + A(1,2)*100 + A(2,2)*1000 + numel(A)*10000;\n";
    const EmittedFunction emitted = transpile(
        std::string("function r = f(a1, a2)\n") + body + "end\n",
        {{"a1", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())},
         {"a2", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}});

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_rowcolfill_e2e.exe").string();
    const std::string outTxt = (base / "nk_rowcolfill_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double a1[2] = {1, 2};\n"
        "  double a2[2] = {3, 4};\n"
        "  double r = f(a1, 2, a2, 2);\n"  // 5 + 30 + 500 + 9000 + 40000 = 49535
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", r);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 5.0 + 30.0 + 500.0 + 9000.0 + 40000.0);  // 49535
    numkit::StandardEngine engine;
    const double interp =
        engine.eval(std::string("a1=[1 2]; a2=[3 4];\n") + body + "r", true).toScalar();
    EXPECT_DOUBLE_EQ(got[0], interp);
}

// whole-array fill from a MATCHING-NUMEL array: A(:) = b copies b column-major into A,
// preserving A's shape. A=[1 2;3 4] (2x2), b=[10 20 30 40] -> A=[10 30; 20 40].
TEST(CodegenE2E, WholeArrayArrayFill)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const char *body =
        "  A = [a1; a2];\n"        // 2x2 [1 2; 3 4]
        "  b = [10 20 30 40];\n"   // 1x4, numel 4 == numel(A)
        "  A(:) = b;\n"            // column-major fill -> A=[10 30; 20 40]
        "  r = A(1,1) + A(2,1)*10 + A(1,2)*100 + A(2,2)*1000 + numel(A)*10000;\n";
    const EmittedFunction emitted = transpile(
        std::string("function r = f(a1, a2)\n") + body + "end\n",
        {{"a1", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())},
         {"a2", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}});

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_fillarray_e2e.exe").string();
    const std::string outTxt = (base / "nk_fillarray_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double a1[2] = {1, 2};\n"
        "  double a2[2] = {3, 4};\n"
        "  double r = f(a1, 2, a2, 2);\n"  // 10 + 200 + 3000 + 40000 + 40000 = 83210
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", r);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 10.0 + 200.0 + 3000.0 + 40000.0 + 40000.0);  // 83210
    numkit::StandardEngine engine;
    const double interp =
        engine.eval(std::string("a1=[1 2]; a2=[3 4];\n") + body + "r", true).toScalar();
    EXPECT_DOUBLE_EQ(got[0], interp);
}

// circshift on a 2-D matrix with a scalar shift: shifts along dim 1 (rows), MATLAB
// semantics. A=[1 2;3 4;5 6] (3x2); circshift(A,1) -> [5 6;1 2;3 4]. Done IN-PLACE
// (A = circshift(A,...)) to exercise the aliasing-safe temp.
TEST(CodegenE2E, CircshiftMatrixDim1)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const char *body =
        "  A = [a1; a2; a3];\n"     // 3x2 [1 2; 3 4; 5 6]
        "  A = circshift(A, 1);\n"  // in-place, shift rows down 1 -> [5 6; 1 2; 3 4]
        "  r = A(1,1) + A(2,1)*10 + A(3,1)*100 + A(1,2)*1000 + A(2,2)*10000"
        " + A(3,2)*100000 + numel(A)*1000000;\n";
    const EmittedFunction emitted = transpile(
        std::string("function r = f(a1, a2, a3)\n") + body + "end\n",
        {{"a1", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())},
         {"a2", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())},
         {"a3", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}});

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_circshift2d_e2e.exe").string();
    const std::string outTxt = (base / "nk_circshift2d_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double a1[2] = {1, 2};\n"
        "  double a2[2] = {3, 4};\n"
        "  double a3[2] = {5, 6};\n"
        "  double r = f(a1, 2, a2, 2, a3, 2);\n"  // 5+10+300+6000+20000+400000+6000000 = 6426315
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", r);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 6426315.0);
    numkit::StandardEngine engine;
    const double interp =
        engine.eval(std::string("a1=[1 2]; a2=[3 4]; a3=[5 6];\n") + body + "r", true).toScalar();
    EXPECT_DOUBLE_EQ(got[0], interp);
}

// circshift(A, k, 2): column shift on a 2-D matrix (dim-2 companion to dim-1 above). MATLAB
// semantics. A=[1 2 3;4 5 6]; circshift(A,1,2) -> [3 1 2;6 4 5] (columns shifted right 1).
// Done IN-PLACE to exercise the aliasing-safe temp.
TEST(CodegenE2E, CircshiftMatrixDim2)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const char *body =
        "  A = [a1; a2];\n"          // 2x3 [1 2 3; 4 5 6]
        "  A = circshift(A, 1, 2);\n"  // shift columns right 1 -> [3 1 2; 6 4 5]
        "  r = A(1,1) + A(2,2)*10 + A(1,3)*100 + numel(A)*1000;\n";
    const EmittedFunction emitted = transpile(
        std::string("function r = f(a1, a2)\n") + body + "end\n",
        {{"a1", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())},
         {"a2", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}});

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_circshift2d_dim2_e2e.exe").string();
    const std::string outTxt = (base / "nk_circshift2d_dim2_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double a1[3] = {1, 2, 3};\n"
        "  double a2[3] = {4, 5, 6};\n"
        "  double r = f(a1, 3, a2, 3);\n"  // [3 1 2;6 4 5]: 3 + 40 + 200 + 6000 = 6243
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", r);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 6243.0);
    numkit::StandardEngine engine;
    const double interp =
        engine.eval(std::string("a1=[1 2 3]; a2=[4 5 6];\n") + body + "r", true).toScalar();
    EXPECT_DOUBLE_EQ(got[0], interp);
}

// 2-arg elementwise max/min (NaN-ignoring, == fmax/fmin): scalar max(a,b), ReLU max(x,0)
// (array+scalar broadcast), and elementwise min(A,B). x=[-2 3 -4], y=[5 -1 6].
TEST(CodegenE2E, MaxMinTwoArg)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const char *body =
        "  s = max(x(1), y(1));\n"  // scalar: max(-2,5) = 5
        "  t = min(x(1), y(1));\n"  // scalar: min(-2,5) = -2
        "  R = max(x, 0);\n"        // ReLU (array+scalar): [0 3 0]
        "  C = min(x, y);\n"        // elementwise: [-2 -1 -4]
        "  r = s + t*10 + R(1)*100 + R(2)*1000 + R(3)*10000"
        " + C(1)*100000 + C(2)*1000000 + C(3)*10000000;\n";
    const EmittedFunction emitted = transpile(
        std::string("function r = f(x, y)\n") + body + "end\n",
        {{"x", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())},
         {"y", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}});

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_maxmin2_e2e.exe").string();
    const std::string outTxt = (base / "nk_maxmin2_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double x[3] = {-2, 3, -4};\n"
        "  double y[3] = {5, -1, 6};\n"
        "  double r = f(x, 3, y, 3);\n"
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", r);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], -41197015.0);
    numkit::StandardEngine engine;
    const double interp =
        engine.eval(std::string("x=[-2 3 -4]; y=[5 -1 6];\n") + body + "r", true).toScalar();
    EXPECT_DOUBLE_EQ(got[0], interp);
}

// native sign(x): real signum -> -1/0/1, both elementwise over an array and scalar.
// x=[-3 0 5 -0.5] -> sign(x)=[-1 0 1 -1]; sign(0)=0; sign(7.5)=1.
TEST(CodegenE2E, SignRealElementwise)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const char *body =
        "  S = sign(x);\n"        // elementwise: [-1 0 1 -1]
        "  z = sign(x(2));\n"     // scalar sign(0) = 0
        "  p = sign(7.5);\n"      // scalar literal -> 1
        "  r = S(1) + S(2)*10 + S(3)*100 + S(4)*1000 + z*10000 + p*100000;\n";
    const EmittedFunction emitted = transpile(
        std::string("function r = f(x)\n") + body + "end\n",
        {{"x", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}});

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_sign_e2e.exe").string();
    const std::string outTxt = (base / "nk_sign_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double x[4] = {-3, 0, 5, -0.5};\n"
        "  double r = f(x, 4);\n"  // -1 + 0 + 100 - 1000 + 0 + 100000 = 99099
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", r);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 99099.0);
    numkit::StandardEngine engine;
    const double interp =
        engine.eval(std::string("x=[-3 0 5 -0.5];\n") + body + "r", true).toScalar();
    EXPECT_DOUBLE_EQ(got[0], interp);
}

// logical-indexing WRITE with an inline relational mask: x(x>0)=0 (zero positives) then
// x(x<-3)=-1 (clamp). a=[-2 3 -4 5 -6] -> x=[-2 0 -4 0 -6] -> [-2 0 -1 0 -1].
TEST(CodegenE2E, MaskedWriteInlineRelational)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const char *body =
        "  x = a;\n"            // local copy of the input
        "  x(x > 0) = 0;\n"     // zero positives -> [-2 0 -4 0 -6]
        "  x(x < -3) = -1;\n"   // clamp the very-negative -> [-2 0 -1 0 -1]
        "  r = x(1) + x(2)*10 + x(3)*100 + x(4)*1000 + x(5)*10000;\n";
    const EmittedFunction emitted = transpile(
        std::string("function r = f(a)\n") + body + "end\n",
        {{"a", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}});

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_maskwrite_e2e.exe").string();
    const std::string outTxt = (base / "nk_maskwrite_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double a[5] = {-2, 3, -4, 5, -6};\n"
        "  double r = f(a, 5);\n"  // -2 + 0 - 100 + 0 - 10000 = -10102
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", r);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], -10102.0);
    numkit::StandardEngine engine;
    const double interp =
        engine.eval(std::string("a=[-2 3 -4 5 -6];\n") + body + "r", true).toScalar();
    EXPECT_DOUBLE_EQ(got[0], interp);
}

// 2-D masked write (matrix clamp): A(A>0)=0 zeroes positives over a runtime-dim 2-D matrix.
// A=[-1 2 -3;4 -5 6] -> [-1 0 -3;0 -5 0]. The mask fuses flat over the column-major buffer.
TEST(CodegenE2E, MaskedWrite2DMatrix)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const char *body =
        "  A = [a1; a2];\n"   // 2x3 [-1 2 -3; 4 -5 6]
        "  A(A > 0) = 0;\n"   // zero positives -> [-1 0 -3; 0 -5 0]
        "  r = A(1,1) + A(1,2)*10 + A(1,3)*100 + A(2,1)*1000 + A(2,2)*10000"
        " + A(2,3)*100000 + numel(A)*1000000;\n";
    const EmittedFunction emitted = transpile(
        std::string("function r = f(a1, a2)\n") + body + "end\n",
        {{"a1", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())},
         {"a2", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}});

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_maskwrite2d_e2e.exe").string();
    const std::string outTxt = (base / "nk_maskwrite2d_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double a1[3] = {-1, 2, -3};\n"
        "  double a2[3] = {4, -5, 6};\n"
        "  double r = f(a1, 3, a2, 3);\n"  // -1 -300 -50000 +6000000 = 5949699
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", r);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 5949699.0);
    numkit::StandardEngine engine;
    const double interp =
        engine.eval(std::string("a1=[-1 2 -3]; a2=[4 -5 6];\n") + body + "r", true).toScalar();
    EXPECT_DOUBLE_EQ(got[0], interp);
}

// 2-D masked READ: y = A(A>0) selects A's positives in column-major (MATLAB linear) order.
// A=[-1 2 -3;4 -5 6] -> flat col-major [-1 4 2 -5 -3 6] -> positives [4 2 6].
TEST(CodegenE2E, MaskedRead2DMatrix)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const char *body =
        "  A = [a1; a2];\n"   // 2x3 [-1 2 -3; 4 -5 6]
        "  y = A(A > 0);\n"   // positives, column-major -> [4; 2; 6]
        "  r = y(1) + y(2)*10 + y(3)*100 + numel(y)*1000;\n";
    const EmittedFunction emitted = transpile(
        std::string("function r = f(a1, a2)\n") + body + "end\n",
        {{"a1", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())},
         {"a2", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}});

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_maskread2d_e2e.exe").string();
    const std::string outTxt = (base / "nk_maskread2d_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double a1[3] = {-1, 2, -3};\n"
        "  double a2[3] = {4, -5, 6};\n"
        "  double r = f(a1, 3, a2, 3);\n"  // y=[4 2 6]: 4 + 20 + 600 + 3000 = 3624
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", r);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 3624.0);
    numkit::StandardEngine engine;
    const double interp =
        engine.eval(std::string("a1=[-1 2 -3]; a2=[4 -5 6];\n") + body + "r", true).toScalar();
    EXPECT_DOUBLE_EQ(got[0], interp);
}

// find on a 2-D matrix: idx = find(A>0) -> column-major LINEAR indices where A>0.
// A=[-1 2 -3;4 -5 6] -> flat [-1 4 2 -5 -3 6] -> positives at linear positions [2 3 6].
TEST(CodegenE2E, FindInlineRelational2DMatrix)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const char *body =
        "  A = [a1; a2];\n"      // 2x3 [-1 2 -3; 4 -5 6]
        "  idx = find(A > 0);\n" // linear indices of positives -> [2; 3; 6]
        "  r = idx(1) + idx(2)*10 + idx(3)*100 + numel(idx)*1000;\n";
    const EmittedFunction emitted = transpile(
        std::string("function r = f(a1, a2)\n") + body + "end\n",
        {{"a1", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())},
         {"a2", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}});

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_find2d_e2e.exe").string();
    const std::string outTxt = (base / "nk_find2d_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double a1[3] = {-1, 2, -3};\n"
        "  double a2[3] = {4, -5, 6};\n"
        "  double r = f(a1, 3, a2, 3);\n"  // idx=[2 3 6]: 2 + 30 + 600 + 3000 = 3632
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", r);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 3632.0);
    numkit::StandardEngine engine;
    const double interp =
        engine.eval(std::string("a1=[-1 2 -3]; a2=[4 -5 6];\n") + body + "r", true).toScalar();
    EXPECT_DOUBLE_EQ(got[0], interp);
}

// min/max over an inline elementwise expr -> scalar: min(abs(x)), max(x.^2). x=[-2 3 -4 5 -6]
// -> abs=[2 3 4 5 6] -> min 2; x.^2=[4 9 16 25 36] -> max 36.
TEST(CodegenE2E, MinMaxInlineExpr)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const char *body =
        "  r = min(abs(x)) + max(x.^2)*100;\n";  // 2 + 36*100 = 3602
    const EmittedFunction emitted = transpile(
        std::string("function r = f(x)\n") + body + "end\n",
        {{"x", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}});

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_minmaxinline_e2e.exe").string();
    const std::string outTxt = (base / "nk_minmaxinline_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double x[5] = {-2, 3, -4, 5, -6};\n"
        "  double r = f(x, 5);\n"  // 2 + 3600 = 3602
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", r);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 3602.0);
    numkit::StandardEngine engine;
    const double interp =
        engine.eval(std::string("x=[-2 3 -4 5 -6];\n") + body + "r", true).toScalar();
    EXPECT_DOUBLE_EQ(got[0], interp);
}

// any/all with an inline relational: any(x>0), all(x>0) over a vector -> LOGICAL scalar.
// x=[-2 3 -4 5 -6]: any(x>0)=1, all(x>0)=0, any(x>100)=0, all(x>-100)=1.
TEST(CodegenE2E, AnyAllInlineRelational)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const char *body =
        "  r = any(x > 0) + all(x > 0)*10 + any(x > 100)*100 + all(x > -100)*1000;\n";
    const EmittedFunction emitted = transpile(
        std::string("function r = f(x)\n") + body + "end\n",
        {{"x", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}});

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_anyall_e2e.exe").string();
    const std::string outTxt = (base / "nk_anyall_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double x[5] = {-2, 3, -4, 5, -6};\n"
        "  double r = f(x, 5);\n"  // 1 + 0 + 0 + 1000 = 1001
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", r);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 1001.0);
    numkit::StandardEngine engine;
    const double interp =
        engine.eval(std::string("x=[-2 3 -4 5 -6];\n") + body + "r", true).toScalar();
    EXPECT_DOUBLE_EQ(got[0], interp);
}

// logical-indexing READ with an inline relational mask: y = x(x>0) filters x to its
// positives. x=[-2 3 -4 5 -6] -> y=[3 5] -> numel 2. Variable-length result (push_back).
TEST(CodegenE2E, MaskedReadInlineRelational)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const char *body =
        "  y = x(x > 0);\n"   // filter positives -> [3 5]
        "  r = y(1) + y(2)*10 + numel(y)*1000;\n";
    const EmittedFunction emitted = transpile(
        std::string("function r = f(x)\n") + body + "end\n",
        {{"x", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}});

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_maskread_e2e.exe").string();
    const std::string outTxt = (base / "nk_maskread_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double x[5] = {-2, 3, -4, 5, -6};\n"
        "  double r = f(x, 5);\n"  // y=[3 5]: 3 + 50 + 2000 = 2053
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", r);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 2053.0);
    numkit::StandardEngine engine;
    const double interp =
        engine.eval(std::string("x=[-2 3 -4 5 -6];\n") + body + "r", true).toScalar();
    EXPECT_DOUBLE_EQ(got[0], interp);
}

// numeric GATHER read: y = x(idx) reindexes/selects x by a numeric index vector.
// x=[10 20 30 40 50], idx=[3 1 2] -> y=[30 10 20] (1-based, reorder+select).
TEST(CodegenE2E, NumericGatherRead)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const char *body =
        "  y = x(idx);\n"   // gather: [x(3) x(1) x(2)] = [30 10 20]
        "  r = y(1) + y(2)*10 + y(3)*100 + numel(y)*1000;\n";
    const EmittedFunction emitted = transpile(
        std::string("function r = f(x, idx)\n") + body + "end\n",
        {{"x", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())},
         {"idx", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}});

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_gather_e2e.exe").string();
    const std::string outTxt = (base / "nk_gather_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double x[5] = {10, 20, 30, 40, 50};\n"
        "  double idx[3] = {3, 1, 2};\n"
        "  double r = f(x, 5, idx, 3);\n"  // y=[30 10 20]: 30 + 100 + 2000 + 3000 = 5130
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", r);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 5130.0);
    numkit::StandardEngine engine;
    const double interp =
        engine.eval(std::string("x=[10 20 30 40 50]; idx=[3 1 2];\n") + body + "r", true)
            .toScalar();
    EXPECT_DOUBLE_EQ(got[0], interp);
}

// find with an inline relational: idx = find(x>0) -> 1-based positions where x>0.
// x=[-2 3 -4 5 -6] -> find(x>0) = [2 4] (positions of 3 and 5).
TEST(CodegenE2E, FindInlineRelational)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const char *body =
        "  idx = find(x > 0);\n"  // positions where x>0 -> [2 4]
        "  r = idx(1) + idx(2)*10 + numel(idx)*1000;\n";
    const EmittedFunction emitted = transpile(
        std::string("function r = f(x)\n") + body + "end\n",
        {{"x", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}});

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_findinline_e2e.exe").string();
    const std::string outTxt = (base / "nk_findinline_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double x[5] = {-2, 3, -4, 5, -6};\n"
        "  double r = f(x, 5);\n"  // idx=[2 4]: 2 + 40 + 2000 = 2042
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", r);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 2042.0);
    numkit::StandardEngine engine;
    const double interp =
        engine.eval(std::string("x=[-2 3 -4 5 -6];\n") + body + "r", true).toScalar();
    EXPECT_DOUBLE_EQ(got[0], interp);
}

// A(:) = <elementwise expr>: a self-referencing compound expression filled in place over a
// runtime-dim 2-D matrix. A=[1 2 3;4 5 6]; A(:)=A.*2+1 -> [3 5 7;9 11 13].
TEST(CodegenE2E, WholeArrayElementwiseExprFill)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const char *body =
        "  A = [a1; a2];\n"      // 2x3 [1 2 3; 4 5 6]
        "  A(:) = A .* 2 + 1;\n" // elementwise self-ref, in place -> [3 5 7; 9 11 13]
        "  r = A(1,1) + A(2,2)*10 + A(1,3)*100 + numel(A)*1000;\n";
    const EmittedFunction emitted = transpile(
        std::string("function r = f(a1, a2)\n") + body + "end\n",
        {{"a1", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())},
         {"a2", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}});

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_colonexpr_e2e.exe").string();
    const std::string outTxt = (base / "nk_colonexpr_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double a1[3] = {1, 2, 3};\n"
        "  double a2[3] = {4, 5, 6};\n"
        "  double r = f(a1, 3, a2, 3);\n"  // A(1,1)=3,A(2,2)=11,A(1,3)=7: 3+110+700+6000 = 6813
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", r);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 6813.0);
    numkit::StandardEngine engine;
    const double interp =
        engine.eval(std::string("a1=[1 2 3]; a2=[4 5 6];\n") + body + "r", true).toScalar();
    EXPECT_DOUBLE_EQ(got[0], interp);
}

// numeric SCATTER write x(idx)=v: array-rhs form (with a REPEATED index -> last write wins)
// and scalar-broadcast form. x=[10 20 30 40 50], idx=[4 1 4], vals=[100 200 300].
TEST(CodegenE2E, NumericScatterWrite)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const char *body =
        "  x = a;\n"          // local copy (an input param is read-only in the RawBuffer ABI)
        "  x(idx) = vals;\n"  // array scatter: x(4)=100, x(1)=200, x(4)=300(last) -> [200 20 30 300 50]
        "  t = x(2) + x(5);\n"  // untouched positions: 20 + 50 = 70
        "  x(idx) = 0;\n"     // scalar broadcast on idx -> x(1)=0, x(4)=0 -> [0 20 30 0 50]
        "  r = x(1) + x(2)*10 + x(3)*100 + x(4)*1000 + x(5)*10000 + t*100000;\n";
    const EmittedFunction emitted = transpile(
        std::string("function r = f(a, idx, vals)\n") + body + "end\n",
        {{"a", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())},
         {"idx", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())},
         {"vals", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}});

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_scatter_e2e.exe").string();
    const std::string outTxt = (base / "nk_scatter_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double x[5] = {10, 20, 30, 40, 50};\n"
        "  double idx[3] = {4, 1, 4};\n"
        "  double vals[3] = {100, 200, 300};\n"
        "  double r = f(x, 5, idx, 3, vals, 3);\n"  // 0+200+3000+0+500000+7000000 = 7503200
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", r);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 7503200.0);
    numkit::StandardEngine engine;
    const double interp =
        engine.eval(std::string("a=[10 20 30 40 50]; idx=[4 1 4]; vals=[100 200 300];\n")
                    + body + "r", true)
            .toScalar();
    EXPECT_DOUBLE_EQ(got[0], interp);
}

// cat(dim, A, B) with a literal dim: cat(2,..) horizontal (like [A B]), cat(1,..) vertical
// (like [A;B]). A=[1 2;3 4], B=[5 6;7 8] -> cat(2)=[1 2 5 6;3 4 7 8], cat(1)=[1 2;3 4;5 6;7 8].
TEST(CodegenE2E, CatDimMatrices)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const char *body =
        "  A = [a1; a2];\n"      // 2x2 [1 2; 3 4]
        "  B = [b1; b2];\n"      // 2x2 [5 6; 7 8]
        "  H = cat(2, A, B);\n"  // 2x4
        "  V = cat(1, A, B);\n"  // 4x2
        "  r = H(1,3) + H(2,4)*10 + V(3,1)*100 + V(4,2)*1000 + numel(H)*10000;\n";
    const EmittedFunction emitted = transpile(
        std::string("function r = f(a1, a2, b1, b2)\n") + body + "end\n",
        {{"a1", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())},
         {"a2", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())},
         {"b1", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())},
         {"b2", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}});

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_cat_e2e.exe").string();
    const std::string outTxt = (base / "nk_cat_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double a1[2] = {1, 2};\n"
        "  double a2[2] = {3, 4};\n"
        "  double b1[2] = {5, 6};\n"
        "  double b2[2] = {7, 8};\n"
        "  double r = f(a1, 2, a2, 2, b1, 2, b2, 2);\n"  // H(1,3)=5,H(2,4)=8,V(3,1)=5,V(4,2)=8,8: 88585
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", r);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 5.0 + 80.0 + 500.0 + 8000.0 + 80000.0);  // 88585
    numkit::StandardEngine engine;
    const double interp =
        engine.eval(std::string("a1=[1 2]; a2=[3 4]; b1=[5 6]; b2=[7 8];\n") + body + "r", true)
            .toScalar();
    EXPECT_DOUBLE_EQ(got[0], interp);
}

// kron(A,B): Kronecker product. A=[1 2;3 4], B=[0 1;1 0] -> a 4x4 with A(i,j)*B blocks:
// [0 1 0 2; 1 0 2 0; 0 3 0 4; 3 0 4 0]. (kron ships in the linalg toolbox, so this checks
// the codegen-inlined result against a hand-computed value -- no interpreter diff.)
TEST(CodegenE2E, KronMatrix)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const EmittedFunction emitted = transpile(
        "function r = f(a1, a2, b1, b2)\n"
        "  A = [a1; a2];\n"   // 2x2 [1 2; 3 4]
        "  B = [b1; b2];\n"   // 2x2 [0 1; 1 0]
        "  C = kron(A, B);\n"  // 4x4
        "  r = C(1,1) + C(1,2)*10 + C(1,4)*100 + C(2,3)*1000 + C(4,3)*10000 + numel(C)*100000;\n"
        "end\n",
        {{"a1", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())},
         {"a2", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())},
         {"b1", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())},
         {"b2", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}});

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_kron_e2e.exe").string();
    const std::string outTxt = (base / "nk_kron_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double a1[2] = {1, 2};\n"
        "  double a2[2] = {3, 4};\n"
        "  double b1[2] = {0, 1};\n"
        "  double b2[2] = {1, 0};\n"
        "  double r = f(a1, 2, a2, 2, b1, 2, b2, 2);\n"  // C(1,1)=0,C(1,2)=1,C(1,4)=2,C(2,3)=2,C(4,3)=4,16
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", r);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    // 0 + 10 + 200 + 2000 + 40000 + 1600000 = 1642210
    EXPECT_DOUBLE_EQ(got[0], 0.0 + 10.0 + 200.0 + 2000.0 + 40000.0 + 1600000.0);
}

// diag(v,k) with a non-negative literal offset: place v on the k-th super-diagonal of an
// N x N matrix (N = numel(v) + k). v=[5 6 7]: diag(v,1) -> 4x4, diag(v,2) -> 5x5.
TEST(CodegenE2E, DiagVectorOffset)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const char *body =
        "  S = diag(v, 1);\n"    // 4x4, v on the 1st super-diagonal
        "  U = diag(v, 2);\n"    // 5x5, v on the 2nd super-diagonal
        "  r = S(1,2) + S(3,4)*10 + U(1,3)*100 + U(3,5)*1000 + numel(S)*10000 + numel(U)*100000;\n";
    const EmittedFunction emitted = transpile(
        std::string("function r = f(v)\n") + body + "end\n",
        {{"v", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}});

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_diagoff_e2e.exe").string();
    const std::string outTxt = (base / "nk_diagoff_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double v[3] = {5, 6, 7};\n"
        "  double r = f(v, 3);\n"  // S(1,2)=5,S(3,4)=7,U(1,3)=5,U(3,5)=7,16,25: 5+70+500+7000+160000+2.5e6
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", r);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 5.0 + 70.0 + 500.0 + 7000.0 + 160000.0 + 2500000.0);  // 2667575
    numkit::StandardEngine engine;
    const double interp = engine.eval(std::string("v=[5 6 7];\n") + body + "r", true).toScalar();
    EXPECT_DOUBLE_EQ(got[0], interp);
}

// tril/triu on a runtime-dim 2-D matrix + the diagonal-offset k. A=[1 2 3;4 5 6;7 8 9]:
// tril(A)=[1 0 0;4 5 0;7 8 9], tril(A,1) keeps one super-diagonal, triu(A)=[1 2 3;0 5 6;0 0 9].
TEST(CodegenE2E, TrilTriuOffsetRuntimeDim2D)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const char *body =
        "  A = [a1; a2; a3];\n"  // 3x3 [1 2 3; 4 5 6; 7 8 9]
        "  L0 = tril(A);\n"      // k=0 lower
        "  L1 = tril(A, 1);\n"   // one super-diagonal kept
        "  U0 = triu(A);\n"      // k=0 upper
        "  r = L0(1,1) + L0(3,1)*10 + L1(1,2)*100 + L1(1,3)*1000 + U0(1,3)*10000 + U0(2,1)*100000;\n";
    const EmittedFunction emitted = transpile(
        std::string("function r = f(a1, a2, a3)\n") + body + "end\n",
        {{"a1", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())},
         {"a2", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())},
         {"a3", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}});

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_triltriu_e2e.exe").string();
    const std::string outTxt = (base / "nk_triltriu_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double a1[3] = {1, 2, 3};\n"
        "  double a2[3] = {4, 5, 6};\n"
        "  double a3[3] = {7, 8, 9};\n"
        "  double r = f(a1, 3, a2, 3, a3, 3);\n"  // 1 + 70 + 200 + 0 + 30000 + 0 = 30271
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", r);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 1.0 + 70.0 + 200.0 + 0.0 + 30000.0 + 0.0);  // 30271
    numkit::StandardEngine engine;
    const double interp =
        engine.eval(std::string("a1=[1 2 3]; a2=[4 5 6]; a3=[7 8 9];\n") + body + "r", true)
            .toScalar();
    EXPECT_DOUBLE_EQ(got[0], interp);
}

// rot90(A): rotate a 2-D matrix 90deg CCW -> dims swap. A=[1 2 3;4 5 6] (2x3) ->
// rot90 = [3 6; 2 5; 1 4] (3x2). (Runtime-dim 2-D, built by vertcat.)
TEST(CodegenE2E, Rot90Matrix)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const char *body =
        "  A = [a1; a2];\n"    // 2x3 [1 2 3; 4 5 6]
        "  B = rot90(A);\n"    // 3x2 [3 6; 2 5; 1 4]
        "  r = B(1,1) + B(1,2)*10 + B(2,1)*100 + B(3,1)*1000 + B(3,2)*10000 + numel(B)*100000;\n";
    const EmittedFunction emitted = transpile(
        std::string("function r = f(a1, a2)\n") + body + "end\n",
        {{"a1", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())},
         {"a2", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}});

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_rot90_e2e.exe").string();
    const std::string outTxt = (base / "nk_rot90_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double a1[3] = {1, 2, 3};\n"
        "  double a2[3] = {4, 5, 6};\n"
        "  double r = f(a1, 3, a2, 3);\n"  // B=[3 6;2 5;1 4]: 3+60+200+1000+40000+600000 = 641263
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", r);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 3.0 + 60.0 + 200.0 + 1000.0 + 40000.0 + 600000.0);  // 641263
    numkit::StandardEngine engine;
    const double interp =
        engine.eval(std::string("a1=[1 2 3]; a2=[4 5 6];\n") + body + "r", true).toScalar();
    EXPECT_DOUBLE_EQ(got[0], interp);
}

// fliplr / flipud on a 2-D matrix: reverse the column / row order. A=[1 2 3;4 5 6] ->
// fliplr=[3 2 1;6 5 4], flipud=[4 5 6;1 2 3]. (Runtime-dim 2-D, built by vertcat.)
TEST(CodegenE2E, FliplrFlipudMatrix)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const char *body =
        "  A = [a1; a2];\n"   // 2x3 [1 2 3; 4 5 6]
        "  L = fliplr(A);\n"  // [3 2 1; 6 5 4]
        "  U = flipud(A);\n"  // [4 5 6; 1 2 3]
        "  r = L(1,1) + L(1,3)*10 + U(1,1)*100 + U(2,3)*1000 + numel(L)*10000;\n";
    const EmittedFunction emitted = transpile(
        std::string("function r = f(a1, a2)\n") + body + "end\n",
        {{"a1", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())},
         {"a2", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}});

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_flip2d_e2e.exe").string();
    const std::string outTxt = (base / "nk_flip2d_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double a1[3] = {1, 2, 3};\n"
        "  double a2[3] = {4, 5, 6};\n"
        "  double r = f(a1, 3, a2, 3);\n"  // L(1,1)=3,L(1,3)=1,U(1,1)=4,U(2,3)=3,numel=6: 3+10+400+3000+60000
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", r);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 3.0 + 10.0 + 400.0 + 3000.0 + 60000.0);  // 63413
    numkit::StandardEngine engine;
    const double interp =
        engine.eval(std::string("a1=[1 2 3]; a2=[4 5 6];\n") + body + "r", true).toScalar();
    EXPECT_DOUBLE_EQ(got[0], interp);
}

// mixed vertcat [A; r]: a matrix with a row vector appended -> a taller matrix. The
// mirror of the augmented-matrix horzcat. A=[1 2;3 4], r=[5 6] -> [1 2; 3 4; 5 6] (3x2).
TEST(CodegenE2E, VertcatMatrixWithRowAppended)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const char *body =
        "  A = [a1; a2];\n"  // 2x2 [1 2; 3 4]
        "  M = [A; r];\n"    // 3x2 [1 2; 3 4; 5 6]
        "  s = M(1,1) + M(2,1)*10 + M(3,1)*100 + M(3,2)*1000 + numel(M)*10000;\n";
    const EmittedFunction emitted = transpile(
        std::string("function s = f(a1, a2, r)\n") + body + "end\n",
        {{"a1", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())},
         {"a2", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())},
         {"r", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}});

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_vrowapp_e2e.exe").string();
    const std::string outTxt = (base / "nk_vrowapp_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double a1[2] = {1, 2};\n"
        "  double a2[2] = {3, 4};\n"
        "  double r[2]  = {5, 6};\n"
        "  double s = f(a1, 2, a2, 2, r, 2);\n"  // [1 2;3 4;5 6]: 1 + 30 + 500 + 6000 + 60000 = 66531
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", s);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 1.0 + 30.0 + 500.0 + 6000.0 + 60000.0);  // 66531
    numkit::StandardEngine engine;
    const double interp =
        engine.eval(std::string("a1=[1 2]; a2=[3 4]; r=[5 6];\n") + body + "s", true).toScalar();
    EXPECT_DOUBLE_EQ(got[0], interp);
}

// mixed horzcat [A b] (augmented matrix): a matrix concatenated with a column vector ->
// a wider matrix. A=[1 2;3 4] (vertcat), b=[5;6] (col) -> [1 2 5; 3 4 6] (2x3).
TEST(CodegenE2E, HorzcatMatrixWithColumnAugmented)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const char *body =
        "  A = [a1; a2];\n"  // 2x2 [1 2; 3 4]
        "  M = [A b];\n"     // 2x3 augmented [1 2 5; 3 4 6]
        "  r = M(1,1) + M(2,2)*10 + M(1,3)*100 + M(2,3)*1000 + numel(M)*10000;\n";
    const EmittedFunction emitted = transpile(
        std::string("function r = f(a1, a2, b)\n") + body + "end\n",
        {{"a1", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())},
         {"a2", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())},
         {"b", InferredType::concrete(ValueType::DOUBLE, Shape::colVector())}});

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_augmat_e2e.exe").string();
    const std::string outTxt = (base / "nk_augmat_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double a1[2] = {1, 2};\n"
        "  double a2[2] = {3, 4};\n"
        "  double b[2]  = {5, 6};\n"
        "  double r = f(a1, 2, a2, 2, b, 2);\n"  // [1 2 5;3 4 6]: 1 + 40 + 500 + 6000 + 60000 = 66541
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", r);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 1.0 + 40.0 + 500.0 + 6000.0 + 60000.0);  // 66541
    numkit::StandardEngine engine;
    const double interp =
        engine.eval(std::string("a1=[1 2]; a2=[3 4]; b=[5;6];\n") + body + "r", true).toScalar();
    EXPECT_DOUBLE_EQ(got[0], interp);
}

// runtime eye(n) / eye(m,n): identity at a RUNTIME size -> a runtime-dim 2-D matrix.
// eye(3) -> 3x3 identity; eye(2,3) -> 2x3 (diagonal ones). The runtime mirror of the
// KnownDims eye + zeros(m,n) runtime.
TEST(CodegenE2E, RuntimeEye)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const char *body =
        "  A = eye(n);\n"        // n x n identity
        "  B = eye(2, n);\n"     // 2 x n (diagonal ones, rest 0)
        "  r = A(1,1) + A(2,2)*10 + A(1,2)*100 + numel(A)*1000 "
        "+ B(2,2)*100000 + numel(B)*1000000;\n";
    const EmittedFunction emitted = transpile(
        std::string("function r = f(n)\n") + body + "end\n",
        {{"n", InferredType::scalar(ValueType::DOUBLE)}});

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_eye_e2e.exe").string();
    const std::string outTxt = (base / "nk_eye_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double r = f(3);\n"  // A=eye(3): A11=1,A22=1,A12=0,numel=9; B=eye(2,3): B22=1,numel=6
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", r);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    // 1 + 10 + 0 + 9000 + 100000 + 6000000 = 6109011
    EXPECT_DOUBLE_EQ(got[0], 1.0 + 10.0 + 0.0 + 9000.0 + 100000.0 + 6000000.0);
    numkit::StandardEngine engine;
    const double interp = engine.eval(std::string("n=3;\n") + body + "r", true).toScalar();
    EXPECT_DOUBLE_EQ(got[0], interp);
}

// RUNTIME-DIM 2-D foundation: zeros/ones(m, n) with RUNTIME m,n -> a runtime-dim 2-D
// (a rank-2 ndRuntimeLocal). ones(3,4) all 1: A(2,3)=1, numel=12 -> 1 + 120 = 121.
TEST(CodegenE2E, RuntimeDim2DConstructor)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const EmittedFunction emitted = transpile(
        "function r = f(m, n)\n"
        "  A = ones(m, n);\n"   // runtime-dim 2-D
        "  r = A(2,3) + numel(A)*10;\n"
        "end\n",
        {{"m", InferredType::scalar(ValueType::DOUBLE)},
         {"n", InferredType::scalar(ValueType::DOUBLE)}});

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_rt2d_e2e.exe").string();
    const std::string outTxt = (base / "nk_rt2d_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double r = f(3.0, 4.0);\n"  // ones(3,4): A(2,3)=1, numel=12 -> 1 + 120 = 121
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", r);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 121.0);  // 1 + 12*10
}

// cross(a,b): the 3-D vector cross product. cross([1 2 3],[4 5 6]) = [-3 6 -3].
TEST(CodegenE2E, CrossProduct)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const EmittedFunction emitted = transpile(
        "function r = f(a, b)\n"
        "  c = cross(a, b);\n"   // [-3 6 -3]
        "  r = c(1) + c(2)*10 + c(3)*100 + numel(c)*1000;\n"
        "end\n",
        {{"a", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())},
         {"b", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}});

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_cross_e2e.exe").string();
    const std::string outTxt = (base / "nk_cross_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double a[3] = {1, 2, 3};\n"
        "  double b[3] = {4, 5, 6};\n"
        "  double r = f(a, 3, b, 3);\n"  // [-3 6 -3]: -3 + 60 - 300 + 3000 = 2757
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", r);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], -3.0 + 60.0 - 300.0 + 3000.0);  // 2757
}

// vertcat of scalars [a; b; c] -> a 1-D column (the column counterpart of horzcat).
// [10; 20; 30] -> [10 20 30].
TEST(CodegenE2E, VertcatScalars)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const EmittedFunction emitted = transpile(
        "function r = f()\n"
        "  v = [10; 20; 30];\n"
        "  r = v(1) + v(2)*10 + v(3)*100 + numel(v)*1000;\n"
        "end\n",
        {});
    EXPECT_NE(emitted.source.find(".push_back("), std::string::npos)
        << "vertcat of scalars must build a 1-D column via push_back, not refuse";

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_vcat_e2e.exe").string();
    const std::string outTxt = (base / "nk_vcat_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double r = f();\n"  // [10 20 30]: 10 + 200 + 3000 + 3*1000 = 6210
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", r);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 10.0 + 200.0 + 3000.0 + 3000.0);  // 6210
}

// repmat(x, p, 1) col tiling: p copies of a column stacked. x = [10; 20; 30] (col);
// repmat(x, 2, 1) -> [10; 20; 30; 10; 20; 30].
TEST(CodegenE2E, RepmatColTile)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const EmittedFunction emitted = transpile(
        "function r = f(x)\n"
        "  y = repmat(x, 2, 1);\n"   // [10 20 30 10 20 30]
        "  r = y(1) + y(4)*10 + y(6)*100 + numel(y)*1000;\n"
        "end\n",
        {{"x", InferredType::concrete(ValueType::DOUBLE, Shape::colVector())}});

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_repmatc_e2e.exe").string();
    const std::string outTxt = (base / "nk_repmatc_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double x[3] = {10, 20, 30};\n"  // column
        "  double r = f(x, 3);\n"  // [10 20 30 10 20 30]: 10 + 10*10 + 30*100 + 6*1000 = 9110
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", r);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 10.0 + 100.0 + 3000.0 + 6000.0);  // 9110
}

// repmat(x, 1, q) row tiling: q copies of a row concatenated. repmat([1 2 3], 1, 2)
// -> [1 2 3 1 2 3].
TEST(CodegenE2E, RepmatRowTile)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const EmittedFunction emitted = transpile(
        "function r = f(x)\n"
        "  y = repmat(x, 1, 2);\n"   // [1 2 3 1 2 3]
        "  r = y(1) + y(4)*10 + y(6)*100 + numel(y)*1000;\n"
        "end\n",
        {{"x", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}});

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_repmatv_e2e.exe").string();
    const std::string outTxt = (base / "nk_repmatv_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double x[3] = {1, 2, 3};\n"
        "  double r = f(x, 3);\n"  // [1 2 3 1 2 3]: 1 + 1*10 + 3*100 + 6*1000 = 6311
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", r);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 1.0 + 10.0 + 300.0 + 6000.0);  // 6311
}

// 2-D INTEGRATION CAPSTONE: one kernel composing reshape + a column slice + a row
// slice + diag + trace + eye + tril -- proving the 2-D/matrix surface composes
// end-to-end (a cross-feature 2-D regression guard, meaningful at this breadth).
TEST(CodegenE2E, Integration2DCapstone)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const EmittedFunction emitted = transpile(
        "function out = f(x)\n"
        "  M = reshape(x, 3, 3);\n"     // 3x3 col-major from [1..9]: [1 4 7; 2 5 8; 3 6 9]
        "  c = M(:,2);\n"              // column 2 = [4; 5; 6]
        "  rrow = M(2,:);\n"          // row 2 = [2 5 8]
        "  d = diag(M);\n"            // [1 5 9]
        "  t = trace(M);\n"          // 1+5+9 = 15
        "  L = tril(M);\n"           // lower triangle; L(3,1) = M(3,1) = 3
        "  I = eye(3);\n"            // identity; I(2,2) = 1
        "  out = c(1) + rrow(3)*10 + d(3)*100 + t*1000 + L(3,1)*10000 + I(2,2)*100000;\n"
        "end\n",
        {{"x", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}});
    EXPECT_NE(emitted.source.find("_nk_off"), std::string::npos)
        << "the 2-D capstone must exercise a native column slice";

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_cap2d_e2e.exe").string();
    const std::string outTxt = (base / "nk_cap2d_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double x[9] = {1,2,3,4,5,6,7,8,9};\n"  // reshape(.,3,3) col-major
        "  double out = f(x, 9);\n"
        // c(1)=4, rrow(3)=8, d(3)=9, t=15, L(3,1)=3, I(2,2)=1:
        // 4 + 80 + 900 + 15000 + 30000 + 100000 = 145984
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", out);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 4.0 + 80.0 + 900.0 + 15000.0 + 30000.0 + 100000.0);  // 145984
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
// cannot lower (`gamma` -- std::tgamma diverges from MATLAB at the poles, so it is
// never native) compiles in BRIDGED mode, links the nk_codegen_rt shared lib, RUNS,
// and matches the interpreter. Proves the C-ABI bridge end to end: generated native
// code -> runtime DLL -> result. The opaque handle design keeps all Value alloc/free
// inside the DLL (no cross-module heap).
TEST(CodegenBridge, BridgedScalarCallRunsAndMatchesInterpreter)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    TransferRegistry reg;
    registerStandardTransfers(reg);
    numkit::Lexer          lex("function y = f(x)\n  y = gamma(x);\nend\n");
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
    ASSERT_NE(emitted.source.find("nk_rt::bridge_scalar(\"gamma\""), std::string::npos);

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
        "  const double xs[3] = {1.5, 3.0, 4.5};\n"
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

    // Reference: the interpreter's gamma over the same inputs (the bridge calls the same
    // runtime gamma, so this is the canonical diff-vs-interpreter check).
    numkit::StandardEngine engine;
    EXPECT_DOUBLE_EQ(got[0], engine.eval("gamma(1.5)", true).toScalar());
    EXPECT_DOUBLE_EQ(got[1], engine.eval("gamma(3.0)", true).toScalar());
    EXPECT_DOUBLE_EQ(got[2], engine.eval("gamma(4.5)", true).toScalar());
    EXPECT_DOUBLE_EQ(got[1], 2.0);  // gamma(3) = 2! exactly
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

// BRIDGED BUILTIN MULTI-OUTPUT (DESIGN.md §10 C1): a `[a, b] = builtin(x)` idiom
// where the builtin has NO codegen multi-transfer, so a,b stay Dynamic and the
// runtime owns nargout. Uses `[s, idx] = sort(x)` -- a core 2-output builtin (the
// outputs are ARRAYS, kept Dynamic); `[m,i]=max` is now lowered NATIVELY (see
// CodegenE2E.ArgMaxMin) so it no longer exercises the bridge. The outputs flow
// through the Dynamic tier (Dynamic indexing s(1)/idx(1) + arithmetic). Compiles,
// links the runtime DLL, runs, matches the interpreter.
TEST(CodegenBridge, BridgedBuiltinMultiOutputRunsAndMatchesInterpreter)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    TransferRegistry reg;
    registerStandardTransfers(reg);
    numkit::Lexer          lex("function y = f(x)\n"
                               "  [s, idx] = sort(x);\n"  // bridged 2-output -> s,idx Dynamic
                               "  y = s(1) + idx(1);\n"    // Dynamic indexing + arithmetic
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
    ASSERT_NE(emitted.source.find("nk_rt::call_dyn_multi(\"sort\""), std::string::npos);

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_multiout_bridge_e2e.exe").string();
    const std::string outTxt = (base / "nk_multiout_bridge_e2e_out.txt").string();
    std::error_code   ec;
    std::filesystem::remove(outTxt, ec);

    std::string program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  const double xs[5] = {3, 1, 4, 5, 2};\n"  // distinct; sort -> s(1)=1, idx(1)=2
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
    EXPECT_DOUBLE_EQ(got[0], 3.0);  // sort: s(1)=1 (min), idx(1)=2 (its position); 1 + 2 = 3
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

// N-D permute (phase N1): B = permute(A, [2 3 1]) on a rank-3 2x2x2 array. B(i1,i2,i3) =
// A(i3,i1,i2). A flat (col-major) = 1..8 -> read a few B elements + numel. Differential.
TEST(CodegenE2E, PermuteRank3)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const char *body =
        "  A = zeros(2,2,2);\n"
        "  A(1,1,1)=1; A(2,1,1)=2; A(1,2,1)=3; A(2,2,1)=4;\n"
        "  A(1,1,2)=5; A(2,1,2)=6; A(1,2,2)=7; A(2,2,2)=8;\n"
        "  B = permute(A, [2 3 1]);\n"  // B(i1,i2,i3) = A(i3,i1,i2)
        "  s = B(1,1,1) + B(2,1,1)*10 + B(1,2,1)*100 + B(1,1,2)*1000 + numel(B)*100000;\n";
    const EmittedFunction emitted =
        transpile(std::string("function s = f()\n") + body + "end\n", {});

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_permute3_e2e.exe").string();
    const std::string outTxt = (base / "nk_permute3_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double s = f();\n"  // 1 + 30 + 500 + 2000 + 800000 = 802531
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", s);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 802531.0);
    numkit::StandardEngine engine;
    const double interp = engine.eval(std::string(body) + "s", true).toScalar();
    EXPECT_DOUBLE_EQ(got[0], interp);
}

// N-D cat(3) (phase N2): C = cat(3, A, B) stacks two 2x2 matrices into a 2x2x2 array (page 1
// = A, page 2 = B). A=[1 2;3 4], B=[5 6;7 8] -> C(:,:,1)=A, C(:,:,2)=B.
TEST(CodegenE2E, Cat3StackMatrices)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const char *body =
        "  A = [a1; a2];\n"       // 2x2 [1 2; 3 4]
        "  B = [b1; b2];\n"       // 2x2 [5 6; 7 8]
        "  C = cat(3, A, B);\n"   // 2x2x2
        "  r = C(1,1,1) + C(2,2,1)*10 + C(1,1,2)*100 + C(2,2,2)*1000 + numel(C)*10000;\n";
    const EmittedFunction emitted = transpile(
        std::string("function r = f(a1, a2, b1, b2)\n") + body + "end\n",
        {{"a1", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())},
         {"a2", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())},
         {"b1", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())},
         {"b2", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}});

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_cat3_e2e.exe").string();
    const std::string outTxt = (base / "nk_cat3_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double a1[2] = {1, 2};\n"
        "  double a2[2] = {3, 4};\n"
        "  double b1[2] = {5, 6};\n"
        "  double b2[2] = {7, 8};\n"
        "  double r = f(a1, 2, a2, 2, b1, 2, b2, 2);\n"  // 1+40+500+8000+80000 = 88541
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", r);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 88541.0);
    numkit::StandardEngine engine;
    const double interp =
        engine.eval(std::string("a1=[1 2]; a2=[3 4]; b1=[5 6]; b2=[7 8];\n") + body + "r", true)
            .toScalar();
    EXPECT_DOUBLE_EQ(got[0], interp);
}

// N-D page-slice read (phase N3): P = C(:,:,2) extracts page 2 of a 2x2x2 array as a 2-D
// matrix. C built via cat(3) (chains N2+N3): page 2 = B = [5 6;7 8].
TEST(CodegenE2E, PageSliceReadRank3)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const char *body =
        "  A = [a1; a2];\n"          // 2x2 [1 2; 3 4]
        "  B = [b1; b2];\n"          // 2x2 [5 6; 7 8]
        "  C = cat(3, A, B);\n"      // 2x2x2
        "  P = C(:,:,2);\n"          // page 2 -> [5 6; 7 8]
        "  r = P(1,1) + P(2,2)*10 + P(1,2)*100 + numel(P)*1000;\n";
    const EmittedFunction emitted = transpile(
        std::string("function r = f(a1, a2, b1, b2)\n") + body + "end\n",
        {{"a1", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())},
         {"a2", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())},
         {"b1", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())},
         {"b2", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}});

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_pageslice_e2e.exe").string();
    const std::string outTxt = (base / "nk_pageslice_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double a1[2] = {1, 2};\n"
        "  double a2[2] = {3, 4};\n"
        "  double b1[2] = {5, 6};\n"
        "  double b2[2] = {7, 8};\n"
        "  double r = f(a1, 2, a2, 2, b1, 2, b2, 2);\n"  // 5+80+600+4000 = 4685
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", r);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 4685.0);
    numkit::StandardEngine engine;
    const double interp =
        engine.eval(std::string("a1=[1 2]; a2=[3 4]; b1=[5 6]; b2=[7 8];\n") + body + "r", true)
            .toScalar();
    EXPECT_DOUBLE_EQ(got[0], interp);
}

// N-D page-slice WRITE (phase N4): A(:,:,2) = M writes a 2-D matrix into page 2 of a 2x2x2
// array. A=zeros, M=[10 20;30 40] -> page1 zeros, page2 = M.
TEST(CodegenE2E, PageSliceWriteRank3)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const char *body =
        "  A = zeros(2,2,2);\n"
        "  M = [m1; m2];\n"        // 2x2 [10 20; 30 40]
        "  A(:,:,2) = M;\n"        // write page 2
        "  r = A(1,1,1) + A(1,1,2) + A(2,2,2)*10 + A(1,2,2)*100 + numel(A)*1000;\n";
    const EmittedFunction emitted = transpile(
        std::string("function r = f(m1, m2)\n") + body + "end\n",
        {{"m1", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())},
         {"m2", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}});

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_pagewrite_e2e.exe").string();
    const std::string outTxt = (base / "nk_pagewrite_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double m1[2] = {10, 20};\n"
        "  double m2[2] = {30, 40};\n"
        "  double r = f(m1, 2, m2, 2);\n"  // 0 + 10 + 400 + 2000 + 8000 = 10410
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", r);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 10410.0);
    numkit::StandardEngine engine;
    const double interp =
        engine.eval(std::string("m1=[10 20]; m2=[30 40];\n") + body + "r", true).toScalar();
    EXPECT_DOUBLE_EQ(got[0], interp);
}

// N-D reshape (phase N5): B = reshape(x, 2, 2, 2) reinterprets an 8-element vector as a 2x2x2
// array (same column-major buffer). x=[1..8] -> B flat = x.
TEST(CodegenE2E, ReshapeToRank3)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const char *body =
        "  B = reshape(x, 2, 2, 2);\n"  // 2x2x2, B flat = x
        "  r = B(1,1,1) + B(2,1,1)*10 + B(1,1,2)*100 + B(2,2,2)*1000 + numel(B)*10000;\n";
    const EmittedFunction emitted = transpile(
        std::string("function r = f(x)\n") + body + "end\n",
        {{"x", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}});

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_reshape3_e2e.exe").string();
    const std::string outTxt = (base / "nk_reshape3_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double x[8] = {1, 2, 3, 4, 5, 6, 7, 8};\n"
        "  double r = f(x, 8);\n"  // 1 + 20 + 500 + 8000 + 80000 = 88521
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", r);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 88521.0);
    numkit::StandardEngine engine;
    const double interp =
        engine.eval(std::string("x=[1 2 3 4 5 6 7 8];\n") + body + "r", true).toScalar();
    EXPECT_DOUBLE_EQ(got[0], interp);
}

// N-D cat(3) of RANK-3 operands (N6): A = cat(3,P,Q) is 2x2x2; C = cat(3,A,A) appends pages ->
// 2x2x4 with pages [P Q P Q]. Generalizes piece 55 (which did two 2-D matrices -> m x n x 2).
TEST(CodegenE2E, Cat3Rank3Operands)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const char *body =
        "  P = [p1; p2];\n"        // 2x2 [1 2; 3 4]
        "  Q = [q1; q2];\n"        // 2x2 [5 6; 7 8]
        "  A = cat(3, P, Q);\n"    // 2x2x2
        "  C = cat(3, A, A);\n"    // 2x2x4, pages [P Q P Q]
        "  r = C(1,1,1) + C(2,2,2)*10 + C(1,1,3)*100 + C(2,2,4)*1000 + numel(C)*10000;\n";
    const EmittedFunction emitted = transpile(
        std::string("function r = f(p1, p2, q1, q2)\n") + body + "end\n",
        {{"p1", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())},
         {"p2", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())},
         {"q1", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())},
         {"q2", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}});

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_cat3r3_e2e.exe").string();
    const std::string outTxt = (base / "nk_cat3r3_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double p1[2] = {1, 2};\n"
        "  double p2[2] = {3, 4};\n"
        "  double q1[2] = {5, 6};\n"
        "  double q2[2] = {7, 8};\n"
        "  double r = f(p1, 2, p2, 2, q1, 2, q2, 2);\n"  // 1+80+100+8000+160000 = 168181
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", r);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 168181.0);
    numkit::StandardEngine engine;
    const double interp =
        engine.eval(std::string("p1=[1 2]; p2=[3 4]; q1=[5 6]; q2=[7 8];\n") + body + "r", true)
            .toScalar();
    EXPECT_DOUBLE_EQ(got[0], interp);
}

// N-D page-RANGE read (phase N7): B = A(:,:,2:3) extracts pages 2..3 of a 2x2x3 array as a
// rank-3 2x2x2 sub-array. A=reshape([1..12],2,2,3) (chains N5+N7); pages 2,3 are contiguous.
TEST(CodegenE2E, PageRangeReadRank3)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const char *body =
        "  A = reshape(x, 2, 2, 3);\n"  // 2x2x3, pages [1 3;2 4],[5 7;6 8],[9 11;10 12]
        "  B = A(:,:,2:3);\n"           // pages 2,3 -> 2x2x2
        "  r = B(1,1,1) + B(2,2,1)*10 + B(1,1,2)*100 + B(2,2,2)*1000 + numel(B)*10000;\n";
    const EmittedFunction emitted = transpile(
        std::string("function r = f(x)\n") + body + "end\n",
        {{"x", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}});

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_pagerange_e2e.exe").string();
    const std::string outTxt = (base / "nk_pagerange_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double x[12] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};\n"
        "  double r = f(x, 12);\n"  // 5 + 80 + 900 + 12000 + 80000 = 92985
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", r);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 92985.0);
    numkit::StandardEngine engine;
    const double interp =
        engine.eval(std::string("x=[1 2 3 4 5 6 7 8 9 10 11 12];\n") + body + "r", true).toScalar();
    EXPECT_DOUBLE_EQ(got[0], interp);
}

// N-D leading-scalar strided slice (phase N8): B = A(2,:,:) of a 2x2x3 -> rank-3 [1,2,3] (row 2,
// kept as a singleton first dim). Strided gather. A=reshape([1..12],2,2,3).
TEST(CodegenE2E, LeadingScalarSliceRank3)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const char *body =
        "  A = reshape(x, 2, 2, 3);\n"  // 2x2x3
        "  B = A(2,:,:);\n"             // [1,2,3] (row 2 across cols/pages)
        "  r = B(1,1,1) + B(1,2,1)*10 + B(1,1,2)*100 + B(1,2,3)*1000 + numel(B)*10000;\n";
    const EmittedFunction emitted = transpile(
        std::string("function r = f(x)\n") + body + "end\n",
        {{"x", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}});

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_leadslice_e2e.exe").string();
    const std::string outTxt = (base / "nk_leadslice_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double x[12] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};\n"
        "  double r = f(x, 12);\n"  // 2 + 40 + 600 + 12000 + 60000 = 72642
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", r);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 72642.0);
    numkit::StandardEngine engine;
    const double interp =
        engine.eval(std::string("x=[1 2 3 4 5 6 7 8 9 10 11 12];\n") + body + "r", true).toScalar();
    EXPECT_DOUBLE_EQ(got[0], interp);
}

// N-D middle-scalar strided slice (phase N9): B = A(:,2,:) of a 2x2x3 -> rank-3 [2,1,3] (col 2,
// kept as a singleton middle dim). Sibling of N8. A=reshape([1..12],2,2,3).
TEST(CodegenE2E, MiddleScalarSliceRank3)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const char *body =
        "  A = reshape(x, 2, 2, 3);\n"  // 2x2x3
        "  B = A(:,2,:);\n"             // [2,1,3] (col 2 across rows/pages)
        "  r = B(1,1,1) + B(2,1,1)*10 + B(1,1,2)*100 + B(2,1,3)*1000 + numel(B)*10000;\n";
    const EmittedFunction emitted = transpile(
        std::string("function r = f(x)\n") + body + "end\n",
        {{"x", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}});

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_midslice_e2e.exe").string();
    const std::string outTxt = (base / "nk_midslice_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double x[12] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};\n"
        "  double r = f(x, 12);\n"  // 3 + 40 + 700 + 12000 + 60000 = 72743
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", r);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 72743.0);
    numkit::StandardEngine engine;
    const double interp =
        engine.eval(std::string("x=[1 2 3 4 5 6 7 8 9 10 11 12];\n") + body + "r", true).toScalar();
    EXPECT_DOUBLE_EQ(got[0], interp);
}

// N-D fiber (phase N10): B = A(2,1,:) of a 2x2x3 -> rank-3 [1,1,3] (two fixed dims kept as
// singletons). Strided. A=reshape([1..12],2,2,3); A(2,1,:) = [2 6 10] along dim 3.
TEST(CodegenE2E, FiberSliceRank3)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const char *body =
        "  A = reshape(x, 2, 2, 3);\n"  // 2x2x3
        "  B = A(2,1,:);\n"             // [1,1,3] fiber
        "  r = B(1,1,1) + B(1,1,2)*10 + B(1,1,3)*100 + numel(B)*1000;\n";
    const EmittedFunction emitted = transpile(
        std::string("function r = f(x)\n") + body + "end\n",
        {{"x", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}});

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_fiber_e2e.exe").string();
    const std::string outTxt = (base / "nk_fiber_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double x[12] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};\n"
        "  double r = f(x, 12);\n"  // 2 + 60 + 1000 + 3000 = 4062
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", r);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 4062.0);
    numkit::StandardEngine engine;
    const double interp =
        engine.eval(std::string("x=[1 2 3 4 5 6 7 8 9 10 11 12];\n") + body + "r", true).toScalar();
    EXPECT_DOUBLE_EQ(got[0], interp);
}

// N-D (scalar,colon,scalar) slice (phase N11): B = A(2,:,3) of a 2x2x3 -> 2-D [1,2] (row 2 of
// page 3; trailing scalar drops the page dim). A=reshape([1..12],2,2,3); A(2,:,3) = [10 12].
TEST(CodegenE2E, RowOfPageSliceRank3)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const char *body =
        "  A = reshape(x, 2, 2, 3);\n"  // 2x2x3
        "  B = A(2,:,3);\n"             // [1,2] (row 2 of page 3)
        "  r = B(1,1) + B(1,2)*10 + numel(B)*100;\n";
    const EmittedFunction emitted = transpile(
        std::string("function r = f(x)\n") + body + "end\n",
        {{"x", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}});

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_rowofpage_e2e.exe").string();
    const std::string outTxt = (base / "nk_rowofpage_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double x[12] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};\n"
        "  double r = f(x, 12);\n"  // 10 + 120 + 200 = 330
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", r);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 330.0);
    numkit::StandardEngine engine;
    const double interp =
        engine.eval(std::string("x=[1 2 3 4 5 6 7 8 9 10 11 12];\n") + body + "r", true).toScalar();
    EXPECT_DOUBLE_EQ(got[0], interp);
}

// N-D (colon,scalar,scalar) slice (phase N12): B = A(:,2,3) of a 2x2x3 -> 2-D [2,1] (col 2 of
// page 3; contiguous block). A=reshape([1..12],2,2,3); A(:,2,3) = [11; 12].
TEST(CodegenE2E, ColOfPageSliceRank3)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const char *body =
        "  A = reshape(x, 2, 2, 3);\n"  // 2x2x3
        "  B = A(:,2,3);\n"             // [2,1] (col 2 of page 3, contiguous)
        "  r = B(1,1) + B(2,1)*10 + numel(B)*100;\n";
    const EmittedFunction emitted = transpile(
        std::string("function r = f(x)\n") + body + "end\n",
        {{"x", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}});

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_colofpage_e2e.exe").string();
    const std::string outTxt = (base / "nk_colofpage_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double x[12] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};\n"
        "  double r = f(x, 12);\n"  // 11 + 120 + 200 = 331
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", r);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 331.0);
    numkit::StandardEngine engine;
    const double interp =
        engine.eval(std::string("x=[1 2 3 4 5 6 7 8 9 10 11 12];\n") + body + "r", true).toScalar();
    EXPECT_DOUBLE_EQ(got[0], interp);
}

// RANK-4 N-D (phase N13): zeros(2,2,2,2) + element r/w + permute + ndims/numel all at rank 4.
// B = permute(A, [2 3 4 1]) -> B(i1,i2,i3,i4) = A(i4,i1,i2,i3). Verifies the rank-agnostic
// foundation + rank-4 permute (the emit handles rank 2-4).
TEST(CodegenE2E, Rank4FoundationAndPermute)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const char *body =
        "  A = zeros(2,2,2,2);\n"
        "  A(1,1,1,1) = 1;\n"
        "  A(2,1,1,1) = 2;\n"
        "  A(2,2,2,2) = 16;\n"
        "  B = permute(A, [2 3 4 1]);\n"  // B(i1,i2,i3,i4) = A(i4,i1,i2,i3)
        "  s = B(1,1,1,1) + B(1,1,1,2)*10 + B(2,2,2,2)*100 + numel(B)*1000 + ndims(B)*100000;\n";
    const EmittedFunction emitted =
        transpile(std::string("function s = f()\n") + body + "end\n", {});

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_rank4_e2e.exe").string();
    const std::string outTxt = (base / "nk_rank4_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double s = f();\n"  // 1 + 20 + 1600 + 16000 + 400000 = 417621
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", s);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 417621.0);
    numkit::StandardEngine engine;
    const double interp = engine.eval(std::string(body) + "s", true).toScalar();
    EXPECT_DOUBLE_EQ(got[0], interp);
}

// reshape to RANK-4 (phase N14): B = reshape(x, 2, 2, 2, 2) reinterprets a 16-element vector as
// a 2x2x2x2 array (same column-major buffer). x=[1..16] -> B flat = x. Generalized rank-N reshape.
TEST(CodegenE2E, ReshapeToRank4)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const char *body =
        "  B = reshape(x, 2, 2, 2, 2);\n"  // 2x2x2x2, B flat = x
        "  r = B(1,1,1,1) + B(2,1,1,1)*10 + B(1,1,1,2)*100 + B(2,2,2,2)*1000 + numel(B)*10000;\n";
    const EmittedFunction emitted = transpile(
        std::string("function r = f(x)\n") + body + "end\n",
        {{"x", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}});

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_reshape4_e2e.exe").string();
    const std::string outTxt = (base / "nk_reshape4_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double x[16];\n"
        "  for (int i = 0; i < 16; ++i) x[i] = i + 1;\n"
        "  double r = f(x, 16);\n"  // 1 + 20 + 900 + 16000 + 160000 = 176921
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", r);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 176921.0);
    numkit::StandardEngine engine;
    const double interp =
        engine.eval(std::string("x=[1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16];\n") + body + "r", true)
            .toScalar();
    EXPECT_DOUBLE_EQ(got[0], interp);
}

// cat(4) of rank-3 operands (phase N15): C = cat(4, A, B) stacks two 2x2x2 arrays along dim 4
// -> 2x2x2x2. C(:,:,:,1)=A, C(:,:,:,2)=B. A,B built via reshape (chains N14+N15).
TEST(CodegenE2E, Cat4StackRank3)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const char *body =
        "  A = reshape(x, 2, 2, 2);\n"   // 2x2x2 from [1..8]
        "  B = reshape(y, 2, 2, 2);\n"   // 2x2x2 from [11..18]
        "  C = cat(4, A, B);\n"          // 2x2x2x2
        "  r = C(1,1,1,1) + C(2,2,2,1)*10 + C(1,1,1,2)*100 + C(2,2,2,2)*1000 + numel(C)*10000;\n";
    const EmittedFunction emitted = transpile(
        std::string("function r = f(x, y)\n") + body + "end\n",
        {{"x", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())},
         {"y", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}});

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_cat4_e2e.exe").string();
    const std::string outTxt = (base / "nk_cat4_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double x[8] = {1, 2, 3, 4, 5, 6, 7, 8};\n"
        "  double y[8] = {11, 12, 13, 14, 15, 16, 17, 18};\n"
        "  double r = f(x, 8, y, 8);\n"  // 1 + 80 + 1100 + 18000 + 160000 = 179181
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", r);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 179181.0);
    numkit::StandardEngine engine;
    const double interp =
        engine.eval(std::string("x=[1 2 3 4 5 6 7 8]; y=[11 12 13 14 15 16 17 18];\n") + body + "r",
                    true)
            .toScalar();
    EXPECT_DOUBLE_EQ(got[0], interp);
}

// GENERAL rank-4 scalar/colon slice (phase N16): A(:,:,:,2) drops the trailing scalar -> rank-3
// [2,2,2] slab; A(2,:,:,:) keeps the leading scalar -> rank-4 [1,2,2,2]. A=reshape([1..16],2,2,2,2).
TEST(CodegenE2E, GeneralSliceRank4)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const char *body =
        "  A = reshape(x, 2, 2, 2, 2);\n"  // 2x2x2x2
        "  B = A(:,:,:,2);\n"              // [2,2,2] (slab 2 along dim 4; trailing scalar drops)
        "  C = A(2,:,:,:);\n"             // [1,2,2,2] (leading scalar kept)
        "  r = B(1,1,1) + B(2,2,2)*10 + C(1,1,1,1)*100 + C(1,2,2,2)*1000"
        " + numel(B)*10000 + numel(C)*100000;\n";
    const EmittedFunction emitted = transpile(
        std::string("function r = f(x)\n") + body + "end\n",
        {{"x", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}});

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_genslice4_e2e.exe").string();
    const std::string outTxt = (base / "nk_genslice4_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double x[16];\n"
        "  for (int i = 0; i < 16; ++i) x[i] = i + 1;\n"
        "  double r = f(x, 16);\n"  // 9 + 160 + 200 + 16000 + 80000 + 800000 = 896369
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", r);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 896369.0);
    numkit::StandardEngine engine;
    const double interp =
        engine.eval(std::string("x=[1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16];\n") + body + "r", true)
            .toScalar();
    EXPECT_DOUBLE_EQ(got[0], interp);
}

// GENERAL rank-4 scalar/colon slice WRITE (phase N17, the write sibling of N16): A(:,:,:,2)=M
// writes a [2,2,2] slab (trailing scalar drops); B(2,:,:,:)=N scatters into the leading-scalar
// slice [1,2,2,2]. Two distinct arrays so the writes do not overlap (clean hand value).
TEST(CodegenE2E, GeneralSliceWriteRank4)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const char *body =
        "  A = zeros(2,2,2,2);\n"
        "  M = reshape(x, 2, 2, 2);\n"        // [2,2,2], flat 1..8
        "  A(:,:,:,2) = M;\n"                 // A slab 2 (flat[8..15]) <- M
        "  B = zeros(2,2,2,2);\n"
        "  y = x + 100;\n"                    // 101..108
        "  N = reshape(y, 2, 2, 2);\n"
        "  B(2,:,:,:) = N;\n"                 // B odd flat indices <- N (column-major)
        "  r = A(1,1,1,2) + A(2,2,2,2)*10 + B(2,1,1,1)*100 + B(2,2,2,2)*1000"
        " + numel(A)*10000;\n";
    const EmittedFunction emitted = transpile(
        std::string("function r = f(x)\n") + body + "end\n",
        {{"x", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}});

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_slicewr4_e2e.exe").string();
    const std::string outTxt = (base / "nk_slicewr4_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double x[8];\n"
        "  for (int i = 0; i < 8; ++i) x[i] = i + 1;\n"
        "  double r = f(x, 8);\n"  // 1 + 80 + 10100 + 108000 + 160000 = 278181
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", r);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 278181.0);
    numkit::StandardEngine engine;
    const double interp =
        engine.eval(std::string("x=[1 2 3 4 5 6 7 8];\n") + body + "r", true).toScalar();
    EXPECT_DOUBLE_EQ(got[0], interp);
}

// N-operand cat along dim 3 (phase N18): cat(3, A, B, C) stacks THREE 2x2 matrices into a
// 2x2x3 array (page1=A, page2=B, page3=C) -- the trailing-dim contiguous append generalized
// past two operands. Verified bit-exact against the interpreter.
TEST(CodegenE2E, CatDim3ThreeOperands)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const char *body =
        "  A = reshape(x, 2, 2);\n"   // 2x2, flat 1..4
        "  B = A + 10;\n"            // flat 11..14
        "  C = A + 20;\n"            // flat 21..24
        "  M = cat(3, A, B, C);\n"   // 2x2x3, pages A|B|C
        "  r = M(1,1,1) + M(2,2,1)*10 + M(1,1,2)*100 + M(2,2,3)*1000 + numel(M)*10000;\n";
    const EmittedFunction emitted = transpile(
        std::string("function r = f(x)\n") + body + "end\n",
        {{"x", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}});

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_cat3x3_e2e.exe").string();
    const std::string outTxt = (base / "nk_cat3x3_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double x[4];\n"
        "  for (int i = 0; i < 4; ++i) x[i] = i + 1;\n"
        "  double r = f(x, 4);\n"  // 1 + 40 + 1100 + 24000 + 120000 = 145141
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", r);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 145141.0);
    numkit::StandardEngine engine;
    const double interp =
        engine.eval(std::string("x=[1 2 3 4];\n") + body + "r", true).toScalar();
    EXPECT_DOUBLE_EQ(got[0], interp);
}

// N-operand cat along dim 4 (phase N19): cat(4, A, B, C) appends THREE 2x2x2 arrays into a
// 2x2x2x3 array (slab1=A, slab2=B, slab3=C) -- the trailing-dim contiguous append one rank
// above N18. Verified bit-exact against the interpreter.
TEST(CodegenE2E, CatDim4ThreeOperands)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const char *body =
        "  A = reshape(x, 2, 2, 2);\n"  // 2x2x2, flat 1..8
        "  B = A + 10;\n"              // flat 11..18
        "  C = A + 20;\n"             // flat 21..28
        "  M = cat(4, A, B, C);\n"    // 2x2x2x3, slabs A|B|C
        "  r = M(1,1,1,1) + M(2,2,2,1)*10 + M(1,1,1,2)*100 + M(2,2,2,3)*1000 + numel(M)*10000;\n";
    const EmittedFunction emitted = transpile(
        std::string("function r = f(x)\n") + body + "end\n",
        {{"x", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}});

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_cat4x3_e2e.exe").string();
    const std::string outTxt = (base / "nk_cat4x3_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double x[8];\n"
        "  for (int i = 0; i < 8; ++i) x[i] = i + 1;\n"
        "  double r = f(x, 8);\n"  // 1 + 80 + 1100 + 28000 + 240000 = 269181
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", r);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 269181.0);
    numkit::StandardEngine engine;
    const double interp =
        engine.eval(std::string("x=[1 2 3 4 5 6 7 8];\n") + body + "r", true).toScalar();
    EXPECT_DOUBLE_EQ(got[0], interp);
}

// page-RANGE write (phase N20, the write sibling of the N7 page-range read): A(:,:,2:3)=B
// writes a 2x2x2 array into the contiguous page block 2..3 of a 2x2x4 A (a straight buffer
// copy). Verified bit-exact against the interpreter.
TEST(CodegenE2E, PageRangeWriteRank3)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const char *body =
        "  A = zeros(2,2,4);\n"          // 2x2x4, all zero
        "  B = reshape(x, 2, 2, 2);\n"   // 2x2x2, flat 1..8
        "  A(:,:,2:3) = B;\n"            // pages 2,3 (A flat[4..12)) <- B
        "  r = A(1,1,2) + A(2,2,3)*10 + A(1,1,1)*100 + A(1,1,4)*1000 + numel(A)*10000;\n";
    const EmittedFunction emitted = transpile(
        std::string("function r = f(x)\n") + body + "end\n",
        {{"x", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}});

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_pgrangewr_e2e.exe").string();
    const std::string outTxt = (base / "nk_pgrangewr_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double x[8];\n"
        "  for (int i = 0; i < 8; ++i) x[i] = i + 1;\n"
        "  double r = f(x, 8);\n"  // 1 + 80 + 0 + 0 + 160000 = 160081
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", r);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 160081.0);
    numkit::StandardEngine engine;
    const double interp =
        engine.eval(std::string("x=[1 2 3 4 5 6 7 8];\n") + body + "r", true).toScalar();
    EXPECT_DOUBLE_EQ(got[0], interp);
}

// rank-4 slab-RANGE read+write (phase N21, the trailing-range producers one rank above N7/N20):
// B = A(:,:,:,2:3) extracts slabs 2..3 of a 2x2x2x4 into a 2x2x2x2; C(:,:,:,1:2)=B writes them
// into the slab block 1..2 of a fresh array. Both contiguous-block copies, bit-exact vs interp.
TEST(CodegenE2E, SlabRangeReadWriteRank4)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const char *body =
        "  A = reshape(x, 2, 2, 2, 4);\n"  // 2x2x2x4, flat 1..32
        "  B = A(:,:,:,2:3);\n"            // slab-range READ -> 2x2x2x2 (slabs 2,3 = flat[8..24))
        "  C = zeros(2,2,2,4);\n"
        "  C(:,:,:,1:2) = B;\n"            // slab-range WRITE -> C flat[0..16) <- B
        "  r = B(1,1,1,1) + B(2,2,2,2)*10 + C(1,1,1,1)*100 + C(1,1,1,3)*1000"
        " + numel(B)*100000 + numel(C)*1000000;\n";
    const EmittedFunction emitted = transpile(
        std::string("function r = f(x)\n") + body + "end\n",
        {{"x", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}});

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_slabrange4_e2e.exe").string();
    const std::string outTxt = (base / "nk_slabrange4_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double x[32];\n"
        "  for (int i = 0; i < 32; ++i) x[i] = i + 1;\n"
        "  double r = f(x, 32);\n"  // 9 + 240 + 900 + 0 + 1600000 + 32000000 = 33601149
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", r);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 33601149.0);
    numkit::StandardEngine engine;
    const double interp =
        engine.eval(std::string("x=1:32;\n") + body + "r", true).toScalar();
    EXPECT_DOUBLE_EQ(got[0], interp);
}

// 2-D column-RANGE read+write (phase N22): B = A(:, 2:3) extracts columns 2..3 of a 3x4 into a
// 3x2; C(:, 3:4)=B writes them into columns 3..4 of a fresh 3x4. Both contiguous column-major
// block copies (the common 2-D column-block slice). Verified bit-exact against the interpreter.
TEST(CodegenE2E, ColumnRangeReadWrite2D)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const char *body =
        "  A = reshape(x, 3, 4);\n"  // 3x4, flat 1..12
        "  B = A(:, 2:3);\n"         // column-range READ -> 3x2 (cols 2,3 = flat[3..9))
        "  C = zeros(3, 4);\n"
        "  C(:, 3:4) = B;\n"         // column-range WRITE -> C cols 3,4 (flat[6..12)) <- B
        "  r = B(1,1) + B(3,2)*10 + C(1,3)*100 + C(3,4)*1000 + C(1,1)*10000"
        " + numel(B)*100000 + numel(C)*1000000;\n";
    const EmittedFunction emitted = transpile(
        std::string("function r = f(x)\n") + body + "end\n",
        {{"x", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}});

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_colrange2d_e2e.exe").string();
    const std::string outTxt = (base / "nk_colrange2d_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double x[12];\n"
        "  for (int i = 0; i < 12; ++i) x[i] = i + 1;\n"
        "  double r = f(x, 12);\n"  // 4 + 90 + 400 + 9000 + 0 + 600000 + 12000000 = 12609494
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", r);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 12609494.0);
    numkit::StandardEngine engine;
    const double interp =
        engine.eval(std::string("x=1:12;\n") + body + "r", true).toScalar();
    EXPECT_DOUBLE_EQ(got[0], interp);
}

// 2-D row-RANGE read+write (phase N23): B = A(2:3, :) extracts rows 2..3 of a 4x3 into a 2x3;
// C(1:2, :)=B writes them into rows 1..2 of a fresh 4x3. STRIDED (a row block is not contiguous
// in column-major -- per-column sub-run copy). Verified bit-exact against the interpreter.
TEST(CodegenE2E, RowRangeReadWrite2D)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const char *body =
        "  A = reshape(x, 4, 3);\n"  // 4x3, flat 1..12
        "  B = A(2:3, :);\n"         // row-range READ -> 2x3 (rows 2,3 of each column)
        "  C = zeros(4, 3);\n"
        "  C(1:2, :) = B;\n"         // row-range WRITE -> rows 1,2 of each column of C <- B
        "  r = B(1,1) + B(2,3)*10 + C(1,1)*100 + C(2,3)*1000 + C(4,3)*10000"
        " + numel(B)*100000 + numel(C)*1000000;\n";
    const EmittedFunction emitted = transpile(
        std::string("function r = f(x)\n") + body + "end\n",
        {{"x", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}});

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_rowrange2d_e2e.exe").string();
    const std::string outTxt = (base / "nk_rowrange2d_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double x[12];\n"
        "  for (int i = 0; i < 12; ++i) x[i] = i + 1;\n"
        "  double r = f(x, 12);\n"  // 2 + 110 + 200 + 11000 + 0 + 600000 + 12000000 = 12611312
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", r);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 12611312.0);
    numkit::StandardEngine engine;
    const double interp =
        engine.eval(std::string("x=1:12;\n") + body + "r", true).toScalar();
    EXPECT_DOUBLE_EQ(got[0], interp);
}

// 2-D both-RANGE read+write (phase N24): B = A(2:3, 2:4) extracts the 2x3 sub-block (rows 2..3 x
// cols 2..4) of a 4x4; C(1:2, 1:3)=B writes it into the top-left 2x3 block of a fresh 4x4. STRIDED
// block copy (per kept column, row sub-run). Verified bit-exact against the interpreter.
TEST(CodegenE2E, BlockRangeReadWrite2D)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const char *body =
        "  A = reshape(x, 4, 4);\n"  // 4x4, flat 1..16
        "  B = A(2:3, 2:4);\n"       // both-range READ -> 2x3 (rows 2,3 x cols 2,3,4)
        "  C = zeros(4, 4);\n"
        "  C(1:2, 1:3) = B;\n"       // both-range WRITE -> C rows 1,2 x cols 1,2,3 <- B
        "  r = B(1,1) + B(2,3)*10 + C(1,1)*100 + C(2,3)*1000 + C(4,4)*10000"
        " + numel(B)*100000 + numel(C)*1000000;\n";
    const EmittedFunction emitted = transpile(
        std::string("function r = f(x)\n") + body + "end\n",
        {{"x", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}});

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_blockrange2d_e2e.exe").string();
    const std::string outTxt = (base / "nk_blockrange2d_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double x[16];\n"
        "  for (int i = 0; i < 16; ++i) x[i] = i + 1;\n"
        "  double r = f(x, 16);\n"  // 6 + 150 + 600 + 15000 + 0 + 600000 + 16000000 = 16615756
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", r);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 16615756.0);
    numkit::StandardEngine engine;
    const double interp =
        engine.eval(std::string("x=1:16;\n") + body + "r", true).toScalar();
    EXPECT_DOUBLE_EQ(got[0], interp);
}

// 2-arg flip(A, dim) on a 2-D matrix (phase N25): flip(A,2) reverses columns (= fliplr),
// flip(A,1) reverses rows (= flipud). The explicit-dim form was bridged (identityShapeTransfer
// is strict-1-arg); now native. Verified bit-exact against the interpreter.
TEST(CodegenE2E, FlipWithDim2D)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const char *body =
        "  A = reshape(x, 2, 3);\n"  // 2x3: [1 3 5; 2 4 6]
        "  B = flip(A, 2);\n"        // reverse columns -> [5 3 1; 6 4 2]
        "  C = flip(A, 1);\n"        // reverse rows    -> [2 4 6; 1 3 5]
        "  r = B(1,1) + B(1,3)*10 + C(1,1)*100 + C(2,1)*1000 + numel(B)*10000;\n";
    const EmittedFunction emitted = transpile(
        std::string("function r = f(x)\n") + body + "end\n",
        {{"x", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}});

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_flipdim2d_e2e.exe").string();
    const std::string outTxt = (base / "nk_flipdim2d_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double x[6];\n"
        "  for (int i = 0; i < 6; ++i) x[i] = i + 1;\n"
        "  double r = f(x, 6);\n"  // 5 + 10 + 200 + 1000 + 60000 = 61215
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", r);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 61215.0);
    numkit::StandardEngine engine;
    const double interp =
        engine.eval(std::string("x=1:6;\n") + body + "r", true).toScalar();
    EXPECT_DOUBLE_EQ(got[0], interp);
}

// 2-D cumsum/cumprod (phase N26): cumsum(A) accumulates down each column (dim 1, the matrix
// default), cumsum(A,2)/cumprod(A,2) across each row (dim 2). The 1-D producer only matched
// vectors -> a matrix cumsum was bridged; now native (per-column / per-row prefix scan).
TEST(CodegenE2E, Cumsum2D)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const char *body =
        "  A = reshape(x, 2, 3);\n"  // [1 3 5; 2 4 6]
        "  B = cumsum(A);\n"         // dim 1 (down columns) -> [1 3 5; 3 7 11]
        "  C = cumsum(A, 2);\n"      // dim 2 (across rows)  -> [1 4 9; 2 6 12]
        "  D = cumprod(A, 2);\n"     // dim 2 cumprod        -> [1 3 15; 2 8 48]
        "  r = B(2,1) + B(2,3)*10 + C(1,3)*100 + C(2,3)*1000 + D(1,3)*10000 + numel(B)*100000;\n";
    const EmittedFunction emitted = transpile(
        std::string("function r = f(x)\n") + body + "end\n",
        {{"x", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}});

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_cumsum2d_e2e.exe").string();
    const std::string outTxt = (base / "nk_cumsum2d_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double x[6];\n"
        "  for (int i = 0; i < 6; ++i) x[i] = i + 1;\n"
        "  double r = f(x, 6);\n"  // 3 + 110 + 900 + 12000 + 150000 + 600000 = 763013
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", r);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 763013.0);
    numkit::StandardEngine engine;
    const double interp =
        engine.eval(std::string("x=1:6;\n") + body + "r", true).toScalar();
    EXPECT_DOUBLE_EQ(got[0], interp);
}

// 2-D cummax/cummin (phase N27): cummax(A) runs down each column (dim 1, the matrix default),
// cummin(A,2) across each row (dim 2). The 1-D producer only matched vectors -> a matrix cummax
// was bridged; now native. Non-monotonic input so the scans actually clamp. Bit-exact vs interp.
TEST(CodegenE2E, Cummax2D)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const char *body =
        "  A = reshape(x, 2, 3);\n"  // [3 4 5; 1 1 9]
        "  B = cummax(A);\n"         // dim 1 (down columns) -> [3 4 5; 3 4 9]
        "  C = cummin(A, 2);\n"      // dim 2 (across rows)  -> [3 3 3; 1 1 1]
        "  r = B(2,1) + B(2,3)*10 + C(1,3)*100 + C(2,1)*1000 + numel(B)*10000;\n";
    const EmittedFunction emitted = transpile(
        std::string("function r = f(x)\n") + body + "end\n",
        {{"x", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}});

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_cummax2d_e2e.exe").string();
    const std::string outTxt = (base / "nk_cummax2d_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double x[6] = {3, 1, 4, 1, 5, 9};\n"  // col-major 2x3: [3 4 5; 1 1 9]
        "  double r = f(x, 6);\n"  // 3 + 90 + 300 + 1000 + 60000 = 61393
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", r);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 61393.0);
    numkit::StandardEngine engine;
    const double interp =
        engine.eval(std::string("x=[3 1 4 1 5 9];\n") + body + "r", true).toScalar();
    EXPECT_DOUBLE_EQ(got[0], interp);
}

// 2-D diff (phase N28): diff(A,1,1) takes column differences (dim 1 -> (m-1) x n), diff(A,1,2)
// row differences (dim 2 -> m x (n-1)). The 1-D diff producer only matched vectors -> a matrix
// diff was bridged; now native. Powers-of-2 input so the differences vary. Bit-exact vs interp.
TEST(CodegenE2E, Diff2D)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const char *body =
        "  A = reshape(x, 3, 3);\n"  // [1 8 64; 2 16 128; 4 32 256]
        "  B = diff(A, 1, 1);\n"     // dim 1 -> 2x3 [1 8 64; 2 16 128]
        "  C = diff(A, 1, 2);\n"     // dim 2 -> 3x2 [7 56; 14 112; 28 224]
        "  r = B(1,1) + B(2,3)*10 + C(1,1)*100 + C(3,2)*1000 + numel(B)*10000 + numel(C)*100000;\n";
    const EmittedFunction emitted = transpile(
        std::string("function r = f(x)\n") + body + "end\n",
        {{"x", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}});

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_diff2d_e2e.exe").string();
    const std::string outTxt = (base / "nk_diff2d_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double x[9] = {1, 2, 4, 8, 16, 32, 64, 128, 256};\n"  // 3x3 col-major
        "  double r = f(x, 9);\n"  // 1 + 1280 + 700 + 224000 + 60000 + 600000 = 885981
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", r);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 885981.0);
    numkit::StandardEngine engine;
    const double interp =
        engine.eval(std::string("x=[1 2 4 8 16 32 64 128 256];\n") + body + "r", true).toScalar();
    EXPECT_DOUBLE_EQ(got[0], interp);
}

// 2-D column-wise max(A)/min(A) (phase N29): max(A) reduces each column to its maximum -> a 1 x n
// row vector (min(A) likewise). The 1-D->scalar IIFE only handled vectors -> a matrix max(A) was
// bridged; now native (exact, NaN-skipping). Non-monotonic columns. Bit-exact vs the interpreter.
TEST(CodegenE2E, ColumnMaxMin2D)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const char *body =
        "  A = reshape(x, 3, 3);\n"  // [3 1 2; 1 5 6; 4 9 5]
        "  B = max(A);\n"            // column maxima -> [4 9 6]
        "  C = min(A);\n"            // column minima -> [1 1 2]
        "  r = B(1) + B(3)*10 + C(1)*100 + C(3)*1000 + numel(B)*100000;\n";
    const EmittedFunction emitted = transpile(
        std::string("function r = f(x)\n") + body + "end\n",
        {{"x", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}});

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_colmaxmin2d_e2e.exe").string();
    const std::string outTxt = (base / "nk_colmaxmin2d_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double x[9] = {3, 1, 4, 1, 5, 9, 2, 6, 5};\n"  // 3x3 col-major
        "  double r = f(x, 9);\n"  // 4 + 60 + 100 + 2000 + 300000 = 302164
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", r);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 302164.0);
    numkit::StandardEngine engine;
    const double interp =
        engine.eval(std::string("x=[3 1 4 1 5 9 2 6 5];\n") + body + "r", true).toScalar();
    EXPECT_DOUBLE_EQ(got[0], interp);
}

// 2-D column-wise any(A)/all(A) (phase N30): any(A) is true per column that has a nonzero, all(A)
// per column with no zero -> a LOGICAL 1 x n row vector. The 1-D->scalar IIFE only did vectors -> a
// matrix any/all was bridged; now native (exact). Zero-containing columns. Bit-exact vs interp.
TEST(CodegenE2E, ColumnAnyAll2D)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const char *body =
        "  A = reshape(x, 3, 3);\n"  // [0 1 4; 0 2 0; 0 3 5]
        "  B = any(A);\n"            // columns with any nonzero -> [0 1 1]
        "  C = all(A);\n"            // columns all nonzero      -> [0 1 0]
        "  r = B(1) + B(2)*10 + C(1)*100 + C(2)*1000 + C(3)*10000 + numel(B)*100000;\n";
    const EmittedFunction emitted = transpile(
        std::string("function r = f(x)\n") + body + "end\n",
        {{"x", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}});

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_anyall2d_e2e.exe").string();
    const std::string outTxt = (base / "nk_anyall2d_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double x[9] = {0, 0, 0, 1, 2, 3, 4, 0, 5};\n"  // 3x3 col-major
        "  double r = f(x, 9);\n"  // 0 + 10 + 0 + 1000 + 0 + 300000 = 301010
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", r);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 301010.0);
    numkit::StandardEngine engine;
    const double interp =
        engine.eval(std::string("x=[0 0 0 1 2 3 4 0 5];\n") + body + "r", true).toScalar();
    EXPECT_DOUBLE_EQ(got[0], interp);
}

// rank-3 flip(A, dim) (phase N31): flip(A,3) reverses page order, flip(A,1) reverses rows within
// each page, flip(A,2) reverses columns. The flip producers capped at 2-D -> a rank-3 flip was
// bridged; now native (explicit-dim form). Verified bit-exact against the interpreter.
TEST(CodegenE2E, FlipRank3)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const char *body =
        "  A = reshape(x, 2, 2, 2);\n"  // page1 [1 3; 2 4], page2 [5 7; 6 8]
        "  B = flip(A, 3);\n"           // reverse pages
        "  C = flip(A, 1);\n"          // reverse rows
        "  D = flip(A, 2);\n"          // reverse columns
        "  r = B(1,1,1) + B(1,1,2)*10 + C(1,1,1)*100 + C(2,1,1)*1000 + D(1,1,1)*10000"
        " + D(1,2,1)*100000 + numel(A)*1000000;\n";
    const EmittedFunction emitted = transpile(
        std::string("function r = f(x)\n") + body + "end\n",
        {{"x", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}});

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_fliprank3_e2e.exe").string();
    const std::string outTxt = (base / "nk_fliprank3_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double x[8];\n"
        "  for (int i = 0; i < 8; ++i) x[i] = i + 1;\n"
        "  double r = f(x, 8);\n"  // 5 + 10 + 200 + 1000 + 30000 + 100000 + 8000000 = 8131215
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", r);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 8131215.0);
    numkit::StandardEngine engine;
    const double interp =
        engine.eval(std::string("x=[1 2 3 4 5 6 7 8];\n") + body + "r", true).toScalar();
    EXPECT_DOUBLE_EQ(got[0], interp);
}

// 2-D sort(A) (phase N32): sort(A) sorts each column ascending, sort(A,'descend') descending --
// a same-shape matrix. The sort producer was 1-D only -> a matrix sort was bridged; now native
// (per-column std::sort on the contiguous column blocks). Bit-exact vs the interpreter.
TEST(CodegenE2E, Sort2D)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const char *body =
        "  A = reshape(x, 3, 2);\n"     // [3 6; 1 4; 2 5]
        "  B = sort(A);\n"             // each column ascending  -> [1 4; 2 5; 3 6]
        "  C = sort(A, 'descend');\n"  // each column descending -> [3 6; 2 5; 1 4]
        "  r = B(1,1) + B(3,1)*10 + B(1,2)*100 + C(1,1)*1000 + C(3,2)*10000 + numel(B)*100000;\n";
    const EmittedFunction emitted = transpile(
        std::string("function r = f(x)\n") + body + "end\n",
        {{"x", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}});

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_sort2d_e2e.exe").string();
    const std::string outTxt = (base / "nk_sort2d_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double x[6] = {3, 1, 2, 6, 4, 5};\n"  // 3x2 col-major
        "  double r = f(x, 6);\n"  // 1 + 30 + 400 + 3000 + 40000 + 600000 = 643431
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", r);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 643431.0);
    numkit::StandardEngine engine;
    const double interp =
        engine.eval(std::string("x=[3 1 2 6 4 5];\n") + body + "r", true).toScalar();
    EXPECT_DOUBLE_EQ(got[0], interp);
}

// 2-D unique(A) (phase N33): unique(A) flattens the matrix (column-major) and returns the sorted
// distinct values as a 1-D column. The unique producer was 1-D only -> a matrix unique was bridged;
// now native (sort the flat m*n buffer + dedupe). Duplicated input. Bit-exact vs the interpreter.
TEST(CodegenE2E, Unique2D)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const char *body =
        "  A = reshape(x, 2, 3);\n"  // flat [3 1 3 2 1 5] (duplicates)
        "  B = unique(A);\n"         // sorted distinct -> [1; 2; 3; 5]
        "  r = B(1) + B(2)*10 + B(3)*100 + B(4)*1000 + numel(B)*10000;\n";
    const EmittedFunction emitted = transpile(
        std::string("function r = f(x)\n") + body + "end\n",
        {{"x", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}});

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_unique2d_e2e.exe").string();
    const std::string outTxt = (base / "nk_unique2d_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double x[6] = {3, 1, 3, 2, 1, 5};\n"  // 2x3 col-major, duplicated
        "  double r = f(x, 6);\n"  // 1 + 20 + 300 + 5000 + 40000 = 45321
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", r);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 45321.0);
    numkit::StandardEngine engine;
    const double interp =
        engine.eval(std::string("x=[3 1 3 2 1 5];\n") + body + "r", true).toScalar();
    EXPECT_DOUBLE_EQ(got[0], interp);
}

// rank-3 fliplr/flipud (phase N34): fliplr(A) reverses columns (dim 2) within each page, flipud(A)
// reverses rows (dim 1) -- the 1-arg unambiguous siblings of the N31 rank-3 flip(A,dim). They were
// bridged on a rank-3; now native. Verified bit-exact against the interpreter.
TEST(CodegenE2E, FliplrFlipudRank3)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const char *body =
        "  A = reshape(x, 2, 2, 2);\n"  // page1 [1 3; 2 4], page2 [5 7; 6 8]
        "  B = fliplr(A);\n"            // reverse columns of each page
        "  C = flipud(A);\n"           // reverse rows of each page
        "  r = B(1,1,1) + B(1,2,1)*10 + C(1,1,1)*100 + C(2,1,1)*1000 + numel(A)*10000;\n";
    const EmittedFunction emitted = transpile(
        std::string("function r = f(x)\n") + body + "end\n",
        {{"x", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}});

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_fliplrud3_e2e.exe").string();
    const std::string outTxt = (base / "nk_fliplrud3_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double x[8];\n"
        "  for (int i = 0; i < 8; ++i) x[i] = i + 1;\n"
        "  double r = f(x, 8);\n"  // 3 + 10 + 200 + 1000 + 80000 = 81213
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", r);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 81213.0);
    numkit::StandardEngine engine;
    const double interp =
        engine.eval(std::string("x=[1 2 3 4 5 6 7 8];\n") + body + "r", true).toScalar();
    EXPECT_DOUBLE_EQ(got[0], interp);
}

// rank-3 rot90(A) (phase N35): rotate each page 90deg CCW in the (dim1,dim2) plane (dim 3 fixed),
// A m x n x p -> B n x m x p. The rot90 producer was 2-D only -> a rank-3 rot90 was bridged; now
// native (per-page rotation). Verified bit-exact against the interpreter.
TEST(CodegenE2E, Rot90Rank3)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const char *body =
        "  A = reshape(x, 2, 2, 2);\n"  // page1 [1 3; 2 4], page2 [5 7; 6 8]
        "  B = rot90(A);\n"             // each page rotated -> page1 [3 4; 1 2], page2 [7 8; 5 6]
        "  r = B(1,1,1) + B(2,1,1)*10 + B(1,1,2)*100 + B(2,2,2)*1000 + numel(B)*10000;\n";
    const EmittedFunction emitted = transpile(
        std::string("function r = f(x)\n") + body + "end\n",
        {{"x", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}});

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_rot90rank3_e2e.exe").string();
    const std::string outTxt = (base / "nk_rot90rank3_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double x[8];\n"
        "  for (int i = 0; i < 8; ++i) x[i] = i + 1;\n"
        "  double r = f(x, 8);\n"  // 3 + 10 + 700 + 6000 + 80000 = 86713
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", r);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 86713.0);
    numkit::StandardEngine engine;
    const double interp =
        engine.eval(std::string("x=[1 2 3 4 5 6 7 8];\n") + body + "r", true).toScalar();
    EXPECT_DOUBLE_EQ(got[0], interp);
}

// rot90(A, k) with a literal k (phase N36): k=2 is 180deg (same shape), k=1 is 90 CCW and k=3 is
// 270 CCW (both swap dims). The 2-arg rot90 was bridged (rot90Transfer was 1-arg only); now native
// for a non-negative literal k. Verified bit-exact against the interpreter.
TEST(CodegenE2E, Rot90WithK)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const char *body =
        "  A = reshape(x, 2, 3);\n"  // [1 3 5; 2 4 6]
        "  B = rot90(A, 2);\n"       // 180 -> [6 4 2; 5 3 1] (2x3)
        "  C = rot90(A, 1);\n"       // 90 CCW -> [5 6; 3 4; 1 2] (3x2)
        "  D = rot90(A, 3);\n"       // 270 CCW -> [2 1; 4 3; 6 5] (3x2)
        "  r = B(1,1) + B(2,3)*10 + C(1,1)*100 + C(3,2)*1000 + D(1,1)*10000 + D(3,2)*100000"
        " + numel(C)*1000000;\n";
    const EmittedFunction emitted = transpile(
        std::string("function r = f(x)\n") + body + "end\n",
        {{"x", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}});

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_rot90k_e2e.exe").string();
    const std::string outTxt = (base / "nk_rot90k_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double x[6];\n"
        "  for (int i = 0; i < 6; ++i) x[i] = i + 1;\n"
        "  double r = f(x, 6);\n"  // 6 + 10 + 500 + 2000 + 20000 + 500000 + 6000000 = 6522516
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", r);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 6522516.0);
    numkit::StandardEngine engine;
    const double interp =
        engine.eval(std::string("x=[1 2 3 4 5 6];\n") + body + "r", true).toScalar();
    EXPECT_DOUBLE_EQ(got[0], interp);
}

// 2-D column-wise [m,i]=max(A)/[mn,in]=min(A) (phase N37): each column reduces to its extremum m(j)
// and 1-based argmax index i(j) (first occurrence on ties) -> two 1 x n row vectors. The multi-
// output max/min was 1-D->scalar only -> a matrix form was bridged; now native. Bit-exact vs interp.
TEST(CodegenE2E, ColumnArgMaxMin2D)
{
    if (!aot::available())
        GTEST_SKIP() << "no external compiler configured for this build";

    const char *body =
        "  A = reshape(x, 3, 3);\n"  // [3 1 2; 1 5 6; 4 9 5]
        "  [m, i] = max(A);\n"       // m=[4 9 6], i=[3 3 2]
        "  [mn, in] = min(A);\n"     // mn=[1 1 2], in=[2 1 1]
        "  r = m(1) + i(1)*10 + m(3)*100 + i(3)*1000 + mn(2)*10000 + in(2)*100000"
        " + numel(m)*1000000;\n";
    const EmittedFunction emitted = transpile(
        std::string("function r = f(x)\n") + body + "end\n",
        {{"x", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}});

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
    const std::string exe    = (base / "nk_argmaxmin2d_e2e.exe").string();
    const std::string outTxt = (base / "nk_argmaxmin2d_e2e_out.txt").string();
    std::string       program = emitted.source +
        "#include <cstdio>\n"
        "int main() {\n"
        "  double x[9] = {3, 1, 4, 1, 5, 9, 2, 6, 5};\n"  // 3x3 col-major
        "  double r = f(x, 9);\n"  // 4 + 30 + 600 + 2000 + 10000 + 100000 + 3000000 = 3112634
        "  std::FILE* h = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!h) return 2;\n"
        "  std::fprintf(h, \"%.17g\\n\", r);\n"
        "  std::fclose(h); return 0;\n}\n";

    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 1u);
    EXPECT_DOUBLE_EQ(got[0], 3112634.0);
    numkit::StandardEngine engine;
    const double interp =
        engine.eval(std::string("x=[3 1 4 1 5 9 2 6 5];\n") + body + "r", true).toScalar();
    EXPECT_DOUBLE_EQ(got[0], interp);
}
