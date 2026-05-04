// libs/stats/src/resample/resample.cpp

#include <numkit/stats/resample/resample.hpp>

#include <numkit/builtin/math/random/rng.hpp>

#include <numkit/core/engine.hpp>
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
                                std::mt19937 &gen)
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
    int dim = 1;
    bool with_replacement = true;  // datasample default = with replacement
    Value weights;
    if (args.size() >= 3 && !args[2].isEmpty() && args[2].numel() == 1)
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

void bootstrp_reg(Span<const Value> args, size_t /*nargout*/,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("bootstrp: requires (nboot, fn, X)", 0, 0, "bootstrp", "",
                    "m:bootstrp:nargin");
    const int nboot = (int)args[0].toScalar();
    outs[0] = bootstrp(ctx.engine->resource(), nboot, args[1], args[2]);
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
