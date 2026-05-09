// libs/stats/src/resample/resample.cpp

#include <numkit/stats/resample/resample.hpp>

#include <numkit/builtin/math/random/rng.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/scratch.hpp>
#include <numkit/core/types.hpp>

#include <algorithm>
#include <cstring>
#include <mutex>
#include <numeric>
#include <random>
#include <vector>

namespace numkit::stats {

namespace {

// Sample K indices in [0..N-1] given (optional) weights.
std::vector<int> sample_indices(int N, int K, bool with_replacement,
                                const std::vector<double> &weights,
                                numkit::builtin::detail::MatlabMT19937 &gen)
{
    std::vector<int> out;
    out.reserve((size_t)K);

    if (weights.empty()) {
        if (with_replacement) {
            std::uniform_int_distribution<int> ud(0, N - 1);
            for (int i = 0; i < K; ++i) out.push_back(ud(gen));
        } else {
            // Reservoir-style: shuffle 0..N-1 and take first K.
            std::vector<int> all(N);
            std::iota(all.begin(), all.end(), 0);
            std::shuffle(all.begin(), all.end(), gen);
            out.assign(all.begin(), all.begin() + std::min(K, N));
        }
    } else {
        // Weighted sampling.
        std::discrete_distribution<int> dd(weights.begin(), weights.end());
        if (with_replacement) {
            for (int i = 0; i < K; ++i) out.push_back(dd(gen));
        } else {
            // Without replacement: simple rejection. For small K acceptable;
            // for large K relative to N, prefer Walker / Vose alias method.
            std::vector<bool> taken((size_t)N, false);
            int attempts = 0;
            while ((int)out.size() < K && attempts < 10 * K * N) {
                int i = dd(gen);
                if (!taken[(size_t)i]) {
                    taken[(size_t)i] = true;
                    out.push_back(i);
                }
                ++attempts;
            }
        }
    }
    return out;
}

std::vector<double> read_vec(const Value &v) {
    const size_t n = v.numel();
    std::vector<double> out(n);
    for (size_t i = 0; i < n; ++i) out[i] = v.elemAsDouble(i);
    return out;
}

} // anonymous

Value randsample(std::pmr::memory_resource *mr, int N, int K,
                 bool with_replacement, const Value &weights)
{
    if (N <= 0 || K <= 0)
        return Value::matrix(K > 0 ? K : 0, 1, ValueType::DOUBLE, mr);

    std::vector<double> w;
    if (weights.numel() > 0) w = read_vec(weights);
    if (!w.empty() && (int)w.size() != N)
        throw Error("randsample: weights length must equal N",
                    0, 0, "randsample", "", "m:randsample:size");

    auto &gen = ::numkit::builtin::sharedEngine();
    auto &mtx = ::numkit::builtin::rngMutex();
    std::vector<int> idx;
    {
        std::lock_guard<std::mutex> lk(mtx);
        idx = sample_indices(N, K, with_replacement, w, gen);
    }
    Value out = Value::matrix((size_t)idx.size(), 1, ValueType::DOUBLE, mr);
    double *od = out.doubleDataMut();
    for (size_t i = 0; i < idx.size(); ++i) od[i] = double(idx[i] + 1);  // 1-based
    return out;
}

Value datasample(std::pmr::memory_resource *mr, const Value &X, int K,
                 int dim, bool with_replacement, const Value &weights)
{
    const size_t M = X.dims().rows();
    const size_t D = X.dims().cols();
    const int N = (dim == 2) ? (int)D : (int)M;
    if (N <= 0)
        throw Error("datasample: empty input", 0, 0, "datasample", "",
                    "m:datasample:empty");

    std::vector<double> w;
    if (weights.numel() > 0) w = read_vec(weights);
    if (!w.empty() && (int)w.size() != N)
        throw Error("datasample: weights length must equal sample-axis size",
                    0, 0, "datasample", "", "m:datasample:size");

    auto &gen = ::numkit::builtin::sharedEngine();
    auto &mtx = ::numkit::builtin::rngMutex();
    std::vector<int> idx;
    {
        std::lock_guard<std::mutex> lk(mtx);
        idx = sample_indices(N, K, with_replacement, w, gen);
    }

    if (dim == 2) {
        // Sample columns.
        Value out = Value::matrix(M, idx.size(), ValueType::DOUBLE, mr);
        double *od = out.doubleDataMut();
        for (size_t k = 0; k < idx.size(); ++k)
            for (size_t r = 0; r < M; ++r)
                od[k * M + r] = X.elemAsDouble((size_t)idx[k] * M + r);
        return out;
    }
    // Sample rows.
    Value out = Value::matrix(idx.size(), D, ValueType::DOUBLE, mr);
    double *od = out.doubleDataMut();
    for (size_t k = 0; k < idx.size(); ++k)
        for (size_t c = 0; c < D; ++c)
            od[c * idx.size() + k] = X.elemAsDouble(c * M + (size_t)idx[k]);
    return out;
}

// Bootstrap is more involved because we need to apply a user-supplied
// function `fn` to each bootstrap sample. For now we support the common
// case where `fn` is a function handle in the engine. The first
// bootstrap iteration determines output dimension D (must be a row).
Value bootstrp(std::pmr::memory_resource *mr, int nboot,
               const Value & /*fn*/, const Value & /*X*/)
{
    // Function-handle invocation requires Engine::call which we don't
    // have directly here. Defer until we expose a function-handle
    // helper. Return empty for now and surface a runtime error.
    (void)mr;
    throw Error("bootstrp: function-handle invocation not yet supported",
                0, 0, "bootstrp", "", "m:bootstrp:nyi");
}

Value jackknife(std::pmr::memory_resource * /*mr*/,
                const Value & /*fn*/, const Value & /*X*/)
{
    throw Error("jackknife: function-handle invocation not yet supported",
                0, 0, "jackknife", "", "m:jackknife:nyi");
}

Value combnk(std::pmr::memory_resource *mr, const Value &v, int K) {
    // Coerce v to either an N-element vector (combinations of its values)
    // or a scalar N (combinations of 1..N).
    std::vector<double> items;
    if (v.numel() == 1) {
        const int N = (int)v.toScalar();
        items.resize((size_t)N);
        for (int i = 0; i < N; ++i) items[i] = double(i + 1);
    } else {
        items = read_vec(v);
    }
    const int N = (int)items.size();
    if (K < 0 || K > N)
        throw Error("combnk: K must be in 0..N", 0, 0, "combnk", "",
                    "m:combnk:badK");

    // Number of combinations.
    long long C = 1;
    for (int i = 0; i < K; ++i) C = C * (N - i) / (i + 1);

    Value out = Value::matrix((size_t)C, (size_t)K, ValueType::DOUBLE, mr);
    if (C == 0) return out;
    double *od = out.doubleDataMut();
    if (K == 0) return out;

    // Enumerate combinations in lex order by mask (or by index recursion).
    std::vector<int> idx((size_t)K);
    for (int i = 0; i < K; ++i) idx[(size_t)i] = i;
    long long row = 0;
    while (true) {
        for (int j = 0; j < K; ++j) od[(size_t)j * (size_t)C + (size_t)row] = items[(size_t)idx[(size_t)j]];
        ++row;
        // Advance to next combination in lex order.
        int j = K - 1;
        while (j >= 0 && idx[(size_t)j] == N - K + j) --j;
        if (j < 0) break;
        ++idx[(size_t)j];
        for (int k = j + 1; k < K; ++k) idx[(size_t)k] = idx[(size_t)k - 1] + 1;
    }
    return out;
}

// ════════════════════════════════════════════════════════════════════
// Engine adapters
// ════════════════════════════════════════════════════════════════════

namespace detail {

void randsample_reg(Span<const Value> args, size_t /*nargout*/,
                    Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("randsample: requires (N, K[, replacement, weights])",
                    0, 0, "randsample", "", "m:randsample:nargin");

    bool with_replacement = false;
    Value weights;
    if (args.size() >= 3 && !args[2].isEmpty()) {
        if (args[2].numel() == 1)
            with_replacement = (args[2].toScalar() != 0.0);
        else
            weights = args[2];
    }
    if (args.size() >= 4 && args[3].numel() > 0) weights = args[3];

    // Form 1: N is a scalar count → sample integers 1..N.
    // Form 2: N is a population vector → sample its values.
    if (args[0].numel() == 1) {
        const int N = (int)args[0].toScalar();
        const int K = (int)args[1].toScalar();
        outs[0] = randsample(ctx.engine->resource(), N, K,
                             with_replacement, weights);
    } else {
        const int K = (int)args[1].toScalar();
        outs[0] = datasample(ctx.engine->resource(), args[0], K, 1,
                             with_replacement, weights);
    }
}

void datasample_reg(Span<const Value> args, size_t /*nargout*/,
                    Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("datasample: requires (X, K[, dim, ...])",
                    0, 0, "datasample", "", "m:datasample:nargin");
    const int K = (int)args[1].toScalar();
    // MATLAB datasample default dim:
    //   - For a row vector (1 x N), sample along columns (dim=2).
    //   - Otherwise, sample along rows (dim=1).
    // The user can still override with the third positional argument.
    int dim = (args[0].dims().rows() == 1 && args[0].dims().cols() > 1) ? 2 : 1;
    bool with_replacement = true;  // datasample default = with replacement
    Value weights;
    if (args.size() >= 3 && !args[2].isEmpty() && args[2].numel() == 1
        && !args[2].isChar() && !args[2].isString())
        dim = (int)args[2].toScalar();
    for (size_t i = 2; i + 1 < args.size(); ++i) {
        if (args[i].isChar() || args[i].isString()) {
            const auto k = args[i].toString();
            if      (k == "Replace")  with_replacement = (args[i + 1].toScalar() != 0.0);
            else if (k == "Weights")  weights = args[i + 1];
        }
    }
    outs[0] = datasample(ctx.engine->resource(), args[0], K, dim,
                          with_replacement, weights);
}

// Helper: draw N indices in [0, N-1] with replacement, write into idx_out.
static void drawBootstrapIndices(int N, int *idx_out)
{
    auto &gen = ::numkit::builtin::sharedEngine();
    auto &mtx = ::numkit::builtin::rngMutex();
    std::lock_guard<std::mutex> lk(mtx);
    std::uniform_int_distribution<int> dist(0, N - 1);
    for (int i = 0; i < N; ++i) idx_out[i] = dist(gen);
}

// Resample row indices of X into a same-shape Value.
static Value resampleRows(std::pmr::memory_resource *mr, const Value &X,
                          const int *idx, int N)
{
    if (X.dims().ndim() <= 1 || X.dims().dim(1) == 1) {
        // Vector input: return same-orientation vector of N samples.
        const bool col = X.dims().ndim() >= 2 && X.dims().dim(1) == 1;
        auto out = col
            ? Value::matrix(static_cast<std::size_t>(N), 1, ValueType::DOUBLE, mr)
            : Value::matrix(1, static_cast<std::size_t>(N), ValueType::DOUBLE, mr);
        const double *xd = X.doubleData();
        double *od = out.doubleDataMut();
        for (int i = 0; i < N; ++i) od[i] = xd[idx[i]];
        return out;
    }
    // Matrix input: resample rows.
    const std::size_t m = static_cast<std::size_t>(X.dims().dim(0));
    const std::size_t cols = static_cast<std::size_t>(X.dims().dim(1));
    auto out = Value::matrix(static_cast<std::size_t>(N), cols, ValueType::DOUBLE, mr);
    const double *xd = X.doubleData();
    double *od = out.doubleDataMut();
    for (int i = 0; i < N; ++i)
        for (std::size_t j = 0; j < cols; ++j)
            od[i + j * N] = xd[idx[i] + j * m];
    return out;
}

void bootstrp_reg(Span<const Value> args, size_t /*nargout*/,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("bootstrp: requires (nboot, fn, X)", 0, 0, "bootstrp", "",
                    "m:bootstrp:nargin");
    if (!args[1].isFuncHandle())
        throw Error("bootstrp: 2nd argument must be a function handle",
                    0, 0, "bootstrp", "", "m:bootstrp:notFuncHandle");
    const int nboot = (int)args[0].toScalar();
    if (nboot < 1)
        throw Error("bootstrp: nboot must be >= 1",
                    0, 0, "bootstrp", "", "m:bootstrp:badN");
    auto *mr = ctx.engine->resource();
    const Value &X = args[2];
    const int N = static_cast<int>(X.dims().dim(0));
    if (N == 0)
        throw Error("bootstrp: empty data", 0, 0, "bootstrp", "", "m:bootstrp:empty");

    ScratchArena scratch(mr);
    ScratchVec<int> idx(static_cast<std::size_t>(N), &scratch);

    // First call to determine output dimensionality of fn(sample).
    drawBootstrapIndices(N, idx.data());
    auto sample0 = resampleRows(mr, X, idx.data(), N);
    Value callArgs0[1] = { sample0 };
    auto stat0 = ctx.engine->callFunctionHandle(
        args[1], Span<const Value>(callArgs0, 1), ctx.env);
    const std::size_t K = stat0.numel();
    if (K == 0)
        throw Error("bootstrp: bootfun returned empty", 0, 0, "bootstrp", "",
                    "m:bootstrp:emptyStat");

    // Output is nboot × K (each row = one bootstrap statistic).
    auto out = Value::matrix(static_cast<std::size_t>(nboot), K,
                             ValueType::DOUBLE, mr);
    double *od = out.doubleDataMut();

    // Row 0: stat0.
    for (std::size_t j = 0; j < K; ++j)
        od[0 + j * nboot] = stat0.elemAsDouble(j);

    // Rows 1..nboot-1.
    for (int b = 1; b < nboot; ++b) {
        drawBootstrapIndices(N, idx.data());
        auto sample = resampleRows(mr, X, idx.data(), N);
        Value callArgs[1] = { sample };
        auto stat = ctx.engine->callFunctionHandle(
            args[1], Span<const Value>(callArgs, 1), ctx.env);
        if (stat.numel() != K)
            throw Error("bootstrp: bootfun returned varying-size output",
                        0, 0, "bootstrp", "", "m:bootstrp:varyingStat");
        for (std::size_t j = 0; j < K; ++j)
            od[b + j * nboot] = stat.elemAsDouble(j);
    }
    outs[0] = std::move(out);
}

// bootci(nboot, bootfun, X[, alpha]) — percentile bootstrap CI.
// Returns 2×K matrix: row 1 = lower bound, row 2 = upper bound.
void bootci_reg(Span<const Value> args, size_t /*nargout*/,
                Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("bootci: requires (nboot, fn, X[, alpha])",
                    0, 0, "bootci", "", "m:bootci:nargin");
    if (!args[1].isFuncHandle())
        throw Error("bootci: 2nd argument must be a function handle",
                    0, 0, "bootci", "", "m:bootci:notFuncHandle");
    const int nboot = (int)args[0].toScalar();
    if (nboot < 10)
        throw Error("bootci: nboot must be >= 10 for meaningful CI",
                    0, 0, "bootci", "", "m:bootci:badN");
    double alpha = 0.05;
    if (args.size() >= 4 && !args[3].isEmpty()) {
        alpha = args[3].toScalar();
        if (alpha <= 0.0 || alpha >= 1.0)
            throw Error("bootci: alpha must be in (0, 1)",
                        0, 0, "bootci", "", "m:bootci:alpha");
    }
    auto *mr = ctx.engine->resource();

    // Reuse bootstrp to get the nboot × K matrix.
    Value pseudo_args[3] = { args[0], args[1], args[2] };
    Value boot_stats;
    {
        Value local_outs[1];
        bootstrp_reg(Span<const Value>(pseudo_args, 3), 1,
                     Span<Value>(local_outs, 1), ctx);
        boot_stats = std::move(local_outs[0]);
    }

    const std::size_t K = static_cast<std::size_t>(boot_stats.dims().dim(1));
    const double *bd = boot_stats.doubleData();

    auto ci = Value::matrix(2, K, ValueType::DOUBLE, mr);
    double *cd = ci.doubleDataMut();

    ScratchArena scratch(mr);
    ScratchVec<double> col(static_cast<std::size_t>(nboot), &scratch);
    for (std::size_t j = 0; j < K; ++j) {
        for (int i = 0; i < nboot; ++i) col[i] = bd[i + j * nboot];
        std::sort(col.begin(), col.end());
        // Linear-interpolation percentile (matches MATLAB prctile default).
        auto qFn = [&](double q) {
            const double pos = q * (static_cast<double>(nboot) - 1.0);
            const std::size_t lo = static_cast<std::size_t>(std::floor(pos));
            const std::size_t hi = std::min(static_cast<std::size_t>(lo + 1),
                                              static_cast<std::size_t>(nboot - 1));
            const double frac = pos - static_cast<double>(lo);
            return col[lo] * (1.0 - frac) + col[hi] * frac;
        };
        cd[0 + j * 2] = qFn(alpha / 2.0);
        cd[1 + j * 2] = qFn(1.0 - alpha / 2.0);
    }
    outs[0] = std::move(ci);
}

void jackknife_reg(Span<const Value> args, size_t /*nargout*/,
                   Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("jackknife: requires (fn, X)", 0, 0, "jackknife", "",
                    "m:jackknife:nargin");
    outs[0] = jackknife(ctx.engine->resource(), args[0], args[1]);
}

void combnk_reg(Span<const Value> args, size_t /*nargout*/,
                Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("combnk: requires (v, K)", 0, 0, "combnk", "",
                    "m:combnk:nargin");
    const int K = (int)args[1].toScalar();
    outs[0] = combnk(ctx.engine->resource(), args[0], K);
}

} // namespace detail
} // namespace numkit::stats
