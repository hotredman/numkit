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
