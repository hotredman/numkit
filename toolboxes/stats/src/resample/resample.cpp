// toolboxes/stats/src/resample/resample.cpp

#include <numkit/stats/resample/resample.hpp>

#include <numkit/builtin/math/random/rng.hpp>

#include <numkit/value/value.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/error.hpp>

#include <algorithm>
#include <cstring>
#include <mutex>
#include <numeric>
#include <random>
#include <vector>

#include "resample_detail.hpp"

namespace numkit::stats {


Value randsample(int N, int K, bool with_replacement, const Value &weights, std::pmr::memory_resource *mr)
{
    if (N <= 0 || K <= 0)
        return Value::matrix(K > 0 ? K : 0, 1, ValueType::DOUBLE, mr);

    std::vector<double> w;
    if (weights.numel() > 0) w = read_vec(weights);
    if (!w.empty() && (int)w.size() != N)
        throw Error("randsample: weights length must equal N",
                    0, 0, "randsample", "", "numkit:randsample:size");

    auto &gen = ::numkit::math::sharedEngine();
    auto &mtx = ::numkit::math::rngMutex();
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

Value datasample(const Value &X, int K, int dim, bool with_replacement, const Value &weights, std::pmr::memory_resource *mr)
{
    const size_t M = X.dims().rows();
    const size_t D = X.dims().cols();
    const int N = (dim == 2) ? (int)D : (int)M;
    if (N <= 0)
        throw Error("datasample: empty input", 0, 0, "datasample", "",
                    "numkit:datasample:empty");

    std::vector<double> w;
    if (weights.numel() > 0) w = read_vec(weights);
    if (!w.empty() && (int)w.size() != N)
        throw Error("datasample: weights length must equal sample-axis size",
                    0, 0, "datasample", "", "numkit:datasample:size");

    auto &gen = ::numkit::math::sharedEngine();
    auto &mtx = ::numkit::math::rngMutex();
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
Value bootstrp(int nboot, const Value & /*fn*/, const Value & /*X*/, std::pmr::memory_resource *mr)
{
    // Function-handle invocation requires Engine::call which we don't
    // have directly here. Defer until we expose a function-handle
    // helper. Return empty for now and surface a runtime error.
    (void)mr;
    throw Error("bootstrp: function-handle invocation not yet supported",
                0, 0, "bootstrp", "", "numkit:bootstrp:nyi");
}

Value jackknife(const Value & /*fn*/, const Value & /*X*/, std::pmr::memory_resource * /*mr*/)
{
    throw Error("jackknife: function-handle invocation not yet supported",
                0, 0, "jackknife", "", "numkit:jackknife:nyi");
}


Value combnk(int N, int K, std::pmr::memory_resource *mr)
{
    if (N < 0)
        throw Error("combnk: N must be non-negative", 0, 0, "combnk", "",
                    "numkit:combnk:badN");
    std::vector<double> items((size_t)N);
    for (int i = 0; i < N; ++i) items[(size_t)i] = double(i + 1);
    return combnkImpl(items, K, mr);
}

Value combnk(Span<const double> v, int K, std::pmr::memory_resource *mr)
{
    std::vector<double> items(v.begin(), v.end());
    return combnkImpl(items, K, mr);
}

// ════════════════════════════════════════════════════════════════════
// Engine adapters
// ════════════════════════════════════════════════════════════════════

} // namespace numkit::stats
