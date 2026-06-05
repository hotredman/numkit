// libs/stats/src/distributions/binomial.cpp

#include <numkit/stats/distributions/binomial.hpp>

#include <numkit/builtin/math/random/rng.hpp>
#include <numkit/builtin/math/special/special.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include "dist_helpers.hpp"

#include <cmath>
#include <limits>
#include <mutex>
#include <random>

namespace numkit::stats {

namespace {

template <typename Op>
Value elementwise(const Value &x, Op op, std::pmr::memory_resource *mr)
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

inline double bino_pmf(double k, double n, double p) {
    if (k < 0.0 || k > n || std::floor(k) != k) return 0.0;
    if (p == 0.0) return (k == 0.0) ? 1.0 : 0.0;
    if (p == 1.0) return (k == n)   ? 1.0 : 0.0;
    // log f = lgamma(n+1) - lgamma(k+1) - lgamma(n-k+1) + k log p + (n-k) log(1-p)
    const double lc = std::lgamma(n + 1.0) - std::lgamma(k + 1.0) - std::lgamma(n - k + 1.0);
    return std::exp(lc + k * std::log(p) + (n - k) * std::log1p(-p));
}

inline double bino_cdf_scalar(double k, double n, double p, std::pmr::memory_resource *mr) {
    if (k < 0.0) return 0.0;
    if (k >= n) return 1.0;
    if (p == 0.0) return 1.0;            // P(K = 0) = 1
    if (p == 1.0) return 0.0;            // P(K < n) = 0
    const double kf = std::floor(k);
    // F(k) = I_{1-p}(n - kf, kf + 1)
    Value xv = Value::scalar(1.0 - p, mr);
    Value av = Value::scalar(n - kf, mr);
    Value bv = Value::scalar(kf + 1.0, mr);
    Value r = ::numkit::builtin::betainc(xv, av, bv, mr);
    return r.toScalar();
}

// True iff (n, p) is a valid binomial parameter pair (n a nonneg integer,
// p ∈ [0, 1]). Out-of-domain → the whole result is NaN, per element.
inline bool bino_params_ok(double n, double p) {
    return n >= 0.0 && std::floor(n) == n && p >= 0.0 && p <= 1.0;
}

inline double binopdfK(double k, double n, double p) {
    if (!bino_params_ok(n, p)) return std::numeric_limits<double>::quiet_NaN();
    return bino_pmf(k, n, p);
}

// Smallest integer j with F(j; n, p) ≥ pival (discrete quantile), via the pmf
// recurrence pmf(j+1)/pmf(j) = (n-j)/(j+1)·p/(1-p). (n, p) assumed valid.
inline double bino_inv_scalar(double pival, double n, double p) {
    if (!(pival >= 0.0 && pival <= 1.0)) return std::numeric_limits<double>::quiet_NaN();
    if (pival == 0.0) return 0.0;
    if (pival >= 1.0) return n;
    if (n == 0.0) return 0.0;
    const double r = p / (1.0 - p);
    double pmf = std::pow(1.0 - p, n);  // pmf(0)
    double cdf = pmf;
    const double tol = std::max(1e-13, pival * 1e-13);
    if (cdf >= pival - tol) return 0.0;
    for (double j = 0.0; j < n; j += 1.0) {
        pmf *= (n - j) / (j + 1.0) * r;
        cdf += pmf;
        if (cdf >= pival - tol) return j + 1.0;
    }
    return n;
}

} // anonymous

Value binopdf(const Value &k, double n, double p, std::pmr::memory_resource *mr)
{
    if (n < 0.0 || std::floor(n) != n || p < 0.0 || p > 1.0)
        return elementwise(k, [](double){ return std::numeric_limits<double>::quiet_NaN(); }, mr);
    return elementwise(k, [=](double ki){ return bino_pmf(ki, n, p); }, mr);
}

Value binocdf(const Value &k, double n, double p, std::pmr::memory_resource *mr)
{
    if (n < 0.0 || std::floor(n) != n || p < 0.0 || p > 1.0)
        return elementwise(k, [](double){ return std::numeric_limits<double>::quiet_NaN(); }, mr);
    return elementwise(k, [=](double ki){ return bino_cdf_scalar(ki, n, p, mr); }, mr);
}

Value binoinv(const Value &p_in, double n, double p, std::pmr::memory_resource *mr)
{
    if (!bino_params_ok(n, p))
        return elementwise(p_in, [](double){ return std::numeric_limits<double>::quiet_NaN(); }, mr);
    return elementwise(p_in, [=](double pi) { return bino_inv_scalar(pi, n, p); }, mr);
}

Value binornd(double n, double p, size_t rows, size_t cols, std::pmr::memory_resource *mr)
{
    auto &gen = ::numkit::builtin::sharedEngine();
    auto &mtx = ::numkit::builtin::rngMutex();
    auto out = Value::matrix(rows, cols, ValueType::DOUBLE, mr);
    if (n < 0.0 || std::floor(n) != n || p < 0.0 || p > 1.0 || rows * cols == 0) return out;
    double *od = out.doubleDataMut();
    const size_t cnt = rows * cols;
    std::binomial_distribution<int> bd(static_cast<int>(n), p);
    std::lock_guard<std::mutex> lk(mtx);
    for (size_t i = 0; i < cnt; ++i) od[i] = static_cast<double>(bd(gen));
    return out;
}

std::tuple<double, double> binostat(double n, double p)
{
    if (n < 0.0 || std::floor(n) != n || p < 0.0 || p > 1.0) {
        const double nan = std::numeric_limits<double>::quiet_NaN();
        return std::make_tuple(nan, nan);
    }
    return std::make_tuple(n * p, n * p * (1.0 - p));
}

// ════════════════════════════════════════════════════════════════════
// Engine adapters
// ════════════════════════════════════════════════════════════════════

namespace detail {

void binopdf_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("binopdf: requires (k, n, p)", 0, 0, "binopdf", "", "numkit:binopdf:nargin");
    auto *mr = ctx.engine->resource();
    const Value &n = args[1];
    const Value &p = args[2];
    if (n.isScalar() && p.isScalar())
        outs[0] = binopdf(args[0], n.toScalar(), p.toScalar(), mr);
    else
        outs[0] = broadcast_dist3(args[0], n, p, mr, "binopdf", binopdfK);
}

void binocdf_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    bool upper = false;
    const Span<const Value> a = args.subspan(0, stripUpperFlag(args, upper));
    if (a.size() < 3)
        throw Error("binocdf: requires (k, n, p[, 'upper'])", 0, 0, "binocdf", "", "numkit:binocdf:nargin");
    auto *mr = ctx.engine->resource();
    const Value &n = a[1];
    const Value &p = a[2];
    Value v;
    if (n.isScalar() && p.isScalar()) {
        v = binocdf(a[0], n.toScalar(), p.toScalar(), mr);
    } else {
        // Per-element F(k_i; n_i, p_i) (bino_cdf_scalar handles the k edges +
        // betainc); invalid (n, p) → NaN. Same per-element-betainc cost as the
        // existing scalar binocdf over a vector k.
        const Value &k = a[0];
        const size_t nk = k.numel(), nn = n.numel(), np = p.numel();
        if (nk == 0 || nn == 0 || np == 0) {
            v = dist_empty_like(nk == 0 ? k : (nn == 0 ? n : p), mr);
        } else {
            const size_t N = dist_match_numel({nk, nn, np}, "binocdf");
            const Value &ref = (nn == N) ? n : (np == N ? p : k);
            v = dist_empty_like(ref, mr);
            double *od = v.doubleDataMut();
            const double NaN = std::numeric_limits<double>::quiet_NaN();
            for (size_t i = 0; i < N; ++i) {
                const double ni = n.elemAsDouble(nn == 1 ? 0 : i);
                const double pi = p.elemAsDouble(np == 1 ? 0 : i);
                const double ki = k.elemAsDouble(nk == 1 ? 0 : i);
                od[i] = bino_params_ok(ni, pi) ? bino_cdf_scalar(ki, ni, pi, mr) : NaN;
            }
        }
    }
    if (upper) applyUpperInPlace(v);
    outs[0] = std::move(v);
}

void binoinv_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("binoinv: requires (p, n, prob)", 0, 0, "binoinv", "", "numkit:binoinv:nargin");
    auto *mr = ctx.engine->resource();
    const Value &n = args[1];
    const Value &p = args[2];
    if (n.isScalar() && p.isScalar()) {
        outs[0] = binoinv(args[0], n.toScalar(), p.toScalar(), mr);
        return;
    }
    const Value &pin = args[0];
    const size_t nq = pin.numel(), nn = n.numel(), np = p.numel();
    if (nq == 0 || nn == 0 || np == 0) {
        outs[0] = dist_empty_like(nq == 0 ? pin : (nn == 0 ? n : p), mr);
        return;
    }
    const size_t N = dist_match_numel({nq, nn, np}, "binoinv");
    const Value &ref = (nn == N) ? n : (np == N ? p : pin);
    Value out = dist_empty_like(ref, mr);
    double *od = out.doubleDataMut();
    const double NaN = std::numeric_limits<double>::quiet_NaN();
    for (size_t i = 0; i < N; ++i) {
        const double ni = n.elemAsDouble(nn == 1 ? 0 : i);
        const double pi = p.elemAsDouble(np == 1 ? 0 : i);
        const double qi = pin.elemAsDouble(nq == 1 ? 0 : i);
        od[i] = bino_params_ok(ni, pi) ? bino_inv_scalar(qi, ni, pi) : NaN;
    }
    outs[0] = std::move(out);
}

void binornd_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("binornd: requires (n, p[, sz...])", 0, 0, "binornd", "", "numkit:binornd:nargin");
    const double n = args[0].toScalar();
    const double p = args[1].toScalar();
    size_t rows, cols;
    parse_rng_size(args, 2, rows, cols);
    outs[0] = binornd(n, p, rows, cols, ctx.engine->resource());
}

void binostat_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    emit_vec_stat_2arg(args, nargout, outs, ctx, "binostat",
                       [](double n, double p) { return binostat(n, p); });
}

} // namespace detail
} // namespace numkit::stats
