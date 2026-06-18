// src/codegen/benchmarks/biquad_codegen_bench.cpp
//
// M0 — measure the codegen "prize" before building the inference engine.
//
// The biquad scalar recurrence (y[n] depends on y[n-1], y[n-2] — a true
// cross-iteration dependency that CANNOT be vectorised) is the canonical
// interpreter-overhead kernel. The core bench (iir_filter_bench.cpp)
// already pins raw-array native C++ vs the VM vs MATLAB. This adds the
// ONE point specific to our codegen design:
//
//   the transpiler does NOT emit a raw-array loop. For `y = <biquad>(x)`
//   with x, y inferred as double arrays, it emits an unboxed `double`
//   recurrence whose input/output ARRAYS are numkit Value containers
//   accessed through doubleData()/doubleDataMut() (the array
//   representation tier — see src/codegen/DESIGN.md §5).
//
// This benchmark confirms the Value-container array tier keeps native
// speed: "array stays a Value, elements/scalars are unboxed" should lose
// nothing versus a raw double[] loop. The Value->raw-pointer hoist
// happens once per call (as the emitter would emit it), not per sample.
//
// ns/sample = 1e9 / items_per_second. Compare against the core bench's
// BM_Biquad_NumkitLoop_VM (the boxed interpreter we are escaping) and
// BM_Biquad_NativeCpp (raw-array speed of light).
//
// Snapshot 2026-06-18, Arrow Lake, desktop-fast Release, N=131072
// (ns/sample): ValueIO (this) 1.56 | NativeCpp 1.59 | MATLAB JIT loop
// 2.70 | numkit filter() 5.85 | MATLAB filter() 7.22 | VM loop 151.4 |
// TreeWalker 336.6. The transpiler-faithful output is ~97x the VM and
// ~1.7x faster than MATLAB's JIT loop; the Value-container array tier
// costs nothing vs raw double[].
//
// Brick 7 (2026-06-19): BM_Biquad_Codegen_Generated below generates this
// loop from biquad.m via the emitter, compiles it /O2 to a DLL, loads and
// times it — the ACTUAL transpiler output, not a hand-written stand-in.
// Measured 1.71 ns/sample vs ValueIO 1.55 (same run): the ~10% delta is
// the `y = zeros(1,n)` zero-fill the MATLAB source mandates (an extra
// streaming write the hand loop omits). Still ~88x the VM loop.

#include <numkit/codegen/aot.hpp>
#include <numkit/codegen/emitter.hpp>
#include <numkit/codegen/transfer.hpp>

#include <numkit/core/lexer.hpp>
#include <numkit/core/parser.hpp>

#include <numkit/value/value.hpp>
#include <numkit/value/value_type.hpp>

#include <benchmark/benchmark.h>

#include <cmath>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#ifdef _WIN32
#  include <windows.h>
#else
#  include <dlfcn.h>
#endif

namespace {

constexpr std::size_t kN = 1u << 17;  // 131072 — same as iir_filter_bench
constexpr double kB0 = 0.0675, kB1 = 0.1349, kB2 = 0.0675;
constexpr double kA1 = -1.1430, kA2 = 0.4128;

// Transpiler-faithful: x and y are Value arrays (the array tier); the
// inner recurrence is pure unboxed double over their raw buffers.
void BM_Biquad_Codegen_ValueIO(benchmark::State &state)
{
    numkit::Value x = numkit::Value::matrix(1, kN, numkit::ValueType::DOUBLE, nullptr);
    numkit::Value y = numkit::Value::matrix(1, kN, numkit::ValueType::DOUBLE, nullptr);
    {
        double *xd = x.doubleDataMut();
        for (std::size_t n = 0; n < kN; ++n) xd[n] = std::sin(0.01 * double(n + 1));
    }

    for (auto _ : state) {
        const double *xp = x.doubleData();    // Value -> raw ptr, hoisted once
        double *yp = y.doubleDataMut();
        double x1 = 0, x2 = 0, y1 = 0, y2 = 0;
        for (std::size_t n = 0; n < kN; ++n) {
            const double xn = xp[n];
            const double yn = kB0 * xn + kB1 * x1 + kB2 * x2 - kA1 * y1 - kA2 * y2;
            yp[n] = yn;
            x2 = x1; x1 = xn;
            y2 = y1; y1 = yn;
        }
        benchmark::DoNotOptimize(yp);
        benchmark::ClobberMemory();
    }
    state.SetItemsProcessed(std::int64_t(state.iterations()) * kN);
}
BENCHMARK(BM_Biquad_Codegen_ValueIO);

// ── brick 7: re-measure the ACTUAL transpiler output ──────────────────
// Generate biquad's C++ from biquad.m via the emitter (the promoted,
// clean-index form), compile it with the external compiler at -O2 into a
// shared library, load it, and benchmark calling it. This closes the loop
// on M0: M0 measured a HAND-WRITTEN transpiler-faithful loop (1.56
// ns/sample); this measures the code the emitter actually produces, so
// the two should agree (the generated promoted loop is structurally the
// same recurrence, plus the y = zeros(1,n) zero-fill the source asks for).
// Skips cleanly when no compiler is configured.

using BiquadFn = void (*)(const double *, std::size_t, double, double, double, double,
                          double, double *, std::size_t);

BiquadFn loadGeneratedBiquad()
{
    using namespace numkit::codegen;
    if (!aot::available()) return nullptr;

    TransferRegistry reg;
    registerStandardTransfers(reg);
    const char *srcM =
        "function y = biquad(x, b0, b1, b2, a1, a2)\n"
        "  n = numel(x);\n  y = zeros(1, n);\n"
        "  x1 = 0; x2 = 0; y1 = 0; y2 = 0;\n"
        "  for k = 1:n\n"
        "    xn = x(k);\n"
        "    yn = b0*xn + b1*x1 + b2*x2 - a1*y1 - a2*y2;\n"
        "    y(k) = yn;\n    x2 = x1; x1 = xn;\n    y2 = y1; y1 = yn;\n  end\n"
        "end\n";
    numkit::Lexer  lex(srcM);
    numkit::Parser parser(lex.tokenize());
    auto           root = parser.parse();
    const numkit::ASTNode *fn = nullptr;
    for (const auto &c : root->children)
        if (c && c->type == numkit::NodeType::FUNCTION_DEF) fn = c.get();
    if (!fn) return nullptr;

    std::vector<ParamSpec> params = {
        {"x", InferredType::concrete(numkit::ValueType::DOUBLE, Shape::rowVector())}};
    for (const char *p : {"b0", "b1", "b2", "a1", "a2"})
        params.push_back({p, InferredType::scalar(numkit::ValueType::DOUBLE)});

    std::string program = emitFunction(*fn, params, reg).source;
    program +=
#ifdef _WIN32
        "extern \"C\" __declspec(dllexport)\n"
#else
        "extern \"C\"\n"
#endif
        "void nk_biquad_entry(const double* x, std::size_t xl, double b0, double b1,\n"
        "                     double b2, double a1, double a2, double* y, std::size_t yl) {\n"
        "  biquad(x, xl, b0, b1, b2, a1, a2, y, yl);\n}\n";

    auto base = std::filesystem::temp_directory_path() / "numkit_codegen_aot";
    std::filesystem::create_directories(base);
#ifdef _WIN32
    const std::string lib = (base / "nk_biquad_gen.dll").string();
#else
    const std::string lib = (base / "libnk_biquad_gen.so").string();
#endif
    if (!aot::compileToSharedLibrary(program, lib).ok()) return nullptr;

#ifdef _WIN32
    HMODULE h = ::LoadLibraryA(lib.c_str());
    if (!h) return nullptr;
    return reinterpret_cast<BiquadFn>(::GetProcAddress(h, "nk_biquad_entry"));
#else
    void *h = ::dlopen(lib.c_str(), RTLD_NOW);
    if (!h) return nullptr;
    return reinterpret_cast<BiquadFn>(::dlsym(h, "nk_biquad_entry"));
#endif
}

void BM_Biquad_Codegen_Generated(benchmark::State &state)
{
    static BiquadFn fn = loadGeneratedBiquad();
    if (!fn) {
        state.SkipWithError("codegen AOT unavailable / compile failed");
        return;
    }

    numkit::Value x = numkit::Value::matrix(1, kN, numkit::ValueType::DOUBLE, nullptr);
    numkit::Value y = numkit::Value::matrix(1, kN, numkit::ValueType::DOUBLE, nullptr);
    {
        double *xd = x.doubleDataMut();
        for (std::size_t n = 0; n < kN; ++n) xd[n] = std::sin(0.01 * double(n + 1));
    }

    for (auto _ : state) {
        fn(x.doubleData(), kN, kB0, kB1, kB2, kA1, kA2, y.doubleDataMut(), kN);
        benchmark::DoNotOptimize(y.doubleData());
        benchmark::ClobberMemory();
    }
    state.SetItemsProcessed(std::int64_t(state.iterations()) * kN);
}
BENCHMARK(BM_Biquad_Codegen_Generated);

} // namespace
