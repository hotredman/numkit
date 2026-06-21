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

// Negative array dimension (caveat #2): `zeros(1, n)` with n<0. The dim
// conversion is guarded by nk_rt::dim, which throws BEFORE the float->size_t
// cast (that cast would be UB for a negative value). f(3) -> numel 3; f(-1)
// throws — matching the interpreter, which also errors on a negative dim
// (rather than silently UB-allocating). Not MATLAB's clamp-to-0: codegen's
// contract is to match the interpreter.
TEST(CodegenE2E, NegativeDimThrowsNotUB)
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
        "  double pos = " + emitted.name + "(3.0);\n"        // 1x3 -> numel 3
        "  int threw = 0;\n"
        "  try { " + emitted.name + "(-1.0); } catch (...) { threw = 1; }\n"  // negative -> throws
        "  std::FILE* g = std::fopen(\"" + fwd(outTxt) + "\", \"w\");\n"
        "  if (!g) return 2;\n"
        "  std::fprintf(g, \"%.17g\\n%d\\n\", pos, threw);\n"
        "  std::fclose(g); return 0;\n}\n";
    const std::vector<double> got = compileRunReadDoubles(program, exe, outTxt);
    ASSERT_EQ(got.size(), 2u);
    EXPECT_DOUBLE_EQ(got[0], 3.0);  // numel(zeros(1,3))
    EXPECT_DOUBLE_EQ(got[1], 1.0);  // negative dim threw (not UB)
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
