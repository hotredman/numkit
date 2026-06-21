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
// If the stale numel fact wrongly promoted the loop to `k < x_len` (=8) it
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
        "  f(x, 8, y, 3);\n"  // x_len=8, y_len=3; loop must run 3 (n), not 8
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
    ASSERT_TRUE(emitted.source.find("std::size_t A_rows, std::size_t A_cols")
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
    ASSERT_TRUE(emitted.source.find("void Box__setv") != std::string::npos)
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
    ASSERT_TRUE(emitted.source.find("Rect__ctor") != std::string::npos)
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
