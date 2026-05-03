// libs/stats/src/distributions/hypergeom.cpp
//
// Hypergeometric distribution: drawing N items without replacement from a
// population of M with K successes. f(k; M, K, N) = C(K, k)·C(M-K, N-k)/C(M, N).

#include <numkit/stats/distributions/hypergeom.hpp>

#include <numkit/builtin/math/random/rng.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <mutex>
#include <random>

namespace numkit::stats {

namespace {

template <typename Op>
Value elementwise(std::pmr::memory_resource *mr, const Value &x, Op op)
{
    if (x.isScalar()) return Value::scalar(op(x.toScalar()), mr);
    const auto &d = x.dims();
    Value out;
    if (d.is3D()) out = Value::matrix3d(d.rows(), d.cols(), d.pages(), ValueType::DOUBLE, mr);
    else          out = Value::matrix(d.rows(), d.cols(), ValueType::DOUBLE, mr);
    const size_t n = x.numel();
    if (n == 0) return out;
    double *od = out.doubleDataMut();
    for (size_t i = 0; i < n; ++i) od[i] = op(x.elemAsDouble(i));
    return out;
}

inline double log_choose(double a, double b) {
    if (b < 0.0 || b > a) return -std::numeric_limits<double>::infinity();
    return std::lgamma(a + 1.0) - std::lgamma(b + 1.0) - std::lgamma(a - b + 1.0);
}

inline bool params_valid(double M, double K, double N) {
    return M >= 0.0 && K >= 0.0 && N >= 0.0
        && std::floor(M) == M && std::floor(K) == K && std::floor(N) == N
        && K <= M && N <= M;
}

inline double hyge_pmf(double k, double M, double K, double N) {
    const double k_min = std::max(0.0, N - (M - K));
    const double k_max = std::min(N, K);
    if (k < k_min || k > k_max || std::floor(k) != k) return 0.0;
    const double lp = log_choose(K, k) + log_choose(M - K, N - k) - log_choose(M, N);
    return std::exp(lp);
}

// Forward sum from k_min, with one-ULP tolerance.
inline double hyge_cdf_scalar(double k, double M, double K, double N) {
    if (k < 0.0) return 0.0;
    const double k_max = std::min(N, K);
    if (k >= k_max) return 1.0;
    const double k_min = std::max(0.0, N - (M - K));
    if (k < k_min) return 0.0;
    // pmf(j+1) / pmf(j) = (K - j)/(j + 1) · (N - j)/(M - K - N + j + 1)
    double f = hyge_pmf(k_min, M, K, N);
    double s = f;
    const double kf = std::floor(k);
    for (double j = k_min; j < kf; j += 1.0) {
        const double num = (K - j) * (N - j);
        const double den = (j + 1.0) * (M - K - N + j + 1.0);
        if (den == 0.0) { f = 0.0; }
        else            { f *= num / den; }
        s += f;
    }
    return std::min(1.0, std::max(0.0, s));
}

inline double hyge_inv_scalar(double q, double M, double K, double N) {
    if (!(q >= 0.0 && q <= 1.0)) return std::numeric_limits<double>::quiet_NaN();
    const double k_min = std::max(0.0, N - (M - K));
    const double k_max = std::min(N, K);
    if (q == 0.0) return k_min;
    if (q >= 1.0) return k_max;
    const double tol = std::max(1e-13, q * 1e-13);
    double f = hyge_pmf(k_min, M, K, N);
    double s = f;
    if (s >= q - tol) return k_min;
    for (double j = k_min; j < k_max; j += 1.0) {
        const double num = (K - j) * (N - j);
        const double den = (j + 1.0) * (M - K - N + j + 1.0);
        if (den == 0.0) { f = 0.0; }
        else            { f *= num / den; }
        s += f;
        if (s >= q - tol) return j + 1.0;
    }
    return k_max;
}

} // anonymous

Value hygepdf(std::pmr::memory_resource *mr, const Value &k, double M, double K, double N)
{
    if (!params_valid(M, K, N))
        return elementwise(mr, k, [](double){ return std::numeric_limits<double>::quiet_NaN(); });
    return elementwise(mr, k, [=](double ki){ return hyge_pmf(ki, M, K, N); });
}

Value hygecdf(std::pmr::memory_resource *mr, const Value &k, double M, double K, double N)
{
    if (!params_valid(M, K, N))
        return elementwise(mr, k, [](double){ return std::numeric_limits<double>::quiet_NaN(); });
    return elementwise(mr, k, [=](double ki){ return hyge_cdf_scalar(ki, M, K, N); });
}

Value hygeinv(std::pmr::memory_resource *mr, const Value &q, double M, double K, double N)
{
    if (!params_valid(M, K, N))
        return elementwise(mr, q, [](double){ return std::numeric_limits<double>::quiet_NaN(); });
    return elementwise(mr, q, [=](double qi){ return hyge_inv_scalar(qi, M, K, N); });
}

Value hygernd(std::pmr::memory_resource *mr, double M, double K, double N, size_t rows, size_t cols)
{
    auto &gen = ::numkit::builtin::sharedEngine();
    auto &mtx = ::numkit::builtin::rngMutex();
    auto out = Value::matrix(rows, cols, ValueType::DOUBLE, mr);
    if (!params_valid(M, K, N) || rows * cols == 0) return out;
    double *od = out.doubleDataMut();
    const size_t cnt = rows * cols;
    // Inverse-cdf walk per draw. M, K, N fixed so the walk is fast.
    std::uniform_real_distribution<double> ud(0.0, 1.0);
    std::lock_guard<std::mutex> lk(mtx);
    for (size_t i = 0; i < cnt; ++i) od[i] = hyge_inv_scalar(ud(gen), M, K, N);
    return out;
}

std::tuple<double, double> hygestat(double M, double K, double N)
{
    if (!params_valid(M, K, N) || M < 1.0) {
        const double nan = std::numeric_limits<double>::quiet_NaN();
        return std::make_tuple(nan, nan);
    }
    const double mean = N * K / M;
    if (M < 2.0) return std::make_tuple(mean, std::numeric_limits<double>::quiet_NaN());
    const double var = N * K * (M - K) * (M - N) / (M * M * (M - 1.0));
    return std::make_tuple(mean, var);
}

// ════════════════════════════════════════════════════════════════════
// Engine adapters
// ════════════════════════════════════════════════════════════════════

namespace detail {

void hygepdf_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 4)
        throw Error("hygepdf: requires (k, M, K, N)", 0, 0, "hygepdf", "", "m:hygepdf:nargin");
    outs[0] = hygepdf(ctx.engine->resource(), args[0],
                      args[1].toScalar(), args[2].toScalar(), args[3].toScalar());
}

void hygecdf_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 4)
        throw Error("hygecdf: requires (k, M, K, N)", 0, 0, "hygecdf", "", "m:hygecdf:nargin");
    outs[0] = hygecdf(ctx.engine->resource(), args[0],
                      args[1].toScalar(), args[2].toScalar(), args[3].toScalar());
}

void hygeinv_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 4)
        throw Error("hygeinv: requires (q, M, K, N)", 0, 0, "hygeinv", "", "m:hygeinv:nargin");
    outs[0] = hygeinv(ctx.engine->resource(), args[0],
                      args[1].toScalar(), args[2].toScalar(), args[3].toScalar());
}

void hygernd_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("hygernd: requires (M, K, N[, m, n])", 0, 0, "hygernd", "", "m:hygernd:nargin");
    const double M = args[0].toScalar();
    const double K = args[1].toScalar();
    const double N = args[2].toScalar();
    size_t rows = 1, cols = 1;
    if (args.size() >= 4 && !args[3].isEmpty()) rows = static_cast<size_t>(args[3].toScalar());
    if (args.size() >= 5 && !args[4].isEmpty()) cols = static_cast<size_t>(args[4].toScalar());
    else if (args.size() >= 4) cols = rows;
    outs[0] = hygernd(ctx.engine->resource(), M, K, N, rows, cols);
}

void hygestat_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("hygestat: requires (M, K, N)", 0, 0, "hygestat", "", "m:hygestat:nargin");
    auto [m, v] = hygestat(args[0].toScalar(), args[1].toScalar(), args[2].toScalar());
    outs[0] = Value::scalar(m, ctx.engine->resource());
    if (nargout > 1) outs[1] = Value::scalar(v, ctx.engine->resource());
}

} // namespace detail
} // namespace numkit::stats
