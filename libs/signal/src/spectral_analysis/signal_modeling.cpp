// libs/signal/src/spectral_analysis/signal_modeling.cpp
//
// Parametric signal modelling: Levinson-Durbin recursion, Yule-Walker
// / Burg AR estimation, linear-prediction coefficients, plus the
// representation-conversion utilities the toolbox documents (poly ↔
// rc ↔ ac ↔ is ↔ lar). All implementations follow the standard real-
// data formulations; complex AR isn't covered here.

#include <numkit/signal/spectral_analysis/signal_modeling.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

namespace numkit::signal {

namespace {

std::vector<double> readVec(const Value &x)
{
    const size_t n = x.numel();
    std::vector<double> v(n);
    for (size_t i = 0; i < n; ++i) v[i] = x.elemAsDouble(i);
    return v;
}

Value rowVec(std::pmr::memory_resource *mr, const std::vector<double> &v)
{
    auto out = Value::matrix(1, v.size(), ValueType::DOUBLE, mr);
    if (!v.empty()) std::memcpy(out.doubleDataMut(), v.data(), v.size() * sizeof(double));
    return out;
}

Value colVec(std::pmr::memory_resource *mr, const std::vector<double> &v)
{
    auto out = Value::matrix(v.size(), 1, ValueType::DOUBLE, mr);
    if (!v.empty()) std::memcpy(out.doubleDataMut(), v.data(), v.size() * sizeof(double));
    return out;
}

// Biased autocorrelation r[k] = (1/N) * sum_{i=0..N-1-k} x[i] * x[i+k],
// for k in 0..maxLag. Returns vector of length maxLag+1.
std::vector<double> biasedAutocorr(const std::vector<double> &x, int maxLag)
{
    const size_t N = x.size();
    std::vector<double> R(maxLag + 1, 0.0);
    for (int k = 0; k <= maxLag; ++k) {
        double s = 0.0;
        for (size_t i = 0; i + k < N; ++i) s += x[i] * x[i + k];
        R[k] = s / static_cast<double>(N);
    }
    return R;
}

// Core Levinson-Durbin recursion. Given r (length p+1), produces:
//   a (length p+1, a[0] = 1)
//   e (final prediction error variance)
//   k (reflection coefficients, length p)
// Standard Yule-Walker form: a is the coefficient vector of the AR
// poly 1 + a[1]*z^-1 + ... + a[p]*z^-p such that the filter
// 1/A(z) models the input autocorrelation.
struct LevinsonResult { std::vector<double> a; double e; std::vector<double> k; };

LevinsonResult levinsonCore(const std::vector<double> &r, int p)
{
    LevinsonResult res;
    res.a.assign(p + 1, 0.0);
    res.k.assign(p, 0.0);
    res.a[0] = 1.0;
    if (p == 0) { res.e = (r.empty() ? 0.0 : r[0]); return res; }
    double e = r[0];
    if (e <= 0.0) { res.e = 0.0; return res; }
    std::vector<double> a_prev(p + 1, 0.0); a_prev[0] = 1.0;

    for (int i = 1; i <= p; ++i) {
        // Numerator of the i-th reflection coefficient.
        double num = r[i];
        for (int j = 1; j < i; ++j) num += a_prev[j] * r[i - j];
        const double k = -num / e;
        res.k[i - 1] = k;

        // Update a using a_prev: a_new[j] = a_prev[j] + k * a_prev[i-j]
        std::vector<double> a_new(p + 1, 0.0);
        a_new[0] = 1.0;
        for (int j = 1; j < i; ++j)
            a_new[j] = a_prev[j] + k * a_prev[i - j];
        a_new[i] = k;
        a_prev = std::move(a_new);
        e *= (1.0 - k * k);
        if (e <= 0.0) { e = 0.0; break; }
    }
    res.a = a_prev;
    res.e = e;
    return res;
}

} // anonymous

// ── Levinson-Durbin ────────────────────────────────────────────────

std::tuple<Value, Value, Value>
levinson(std::pmr::memory_resource *mr, const Value &r, int n)
{
    auto rv = readVec(r);
    if (n < 0) n = static_cast<int>(rv.size()) - 1;
    if (n < 0) n = 0;
    if (n + 1 > static_cast<int>(rv.size())) rv.resize(n + 1, 0.0);
    auto res = levinsonCore(rv, n);
    return std::make_tuple(rowVec(mr, res.a),
                           Value::scalar(res.e, mr),
                           colVec(mr, res.k));
}

// rlevinson(a, e): given AR poly a and final prediction error e,
// recover the autocorrelation sequence + reflection coefficients via
// step-down. Returns (R, k).
std::tuple<Value, Value>
rlevinson(std::pmr::memory_resource *mr, const Value &a, double efinal)
{
    auto av = readVec(a);
    const int p = static_cast<int>(av.size()) - 1;
    std::vector<double> R(p + 1, 0.0);
    std::vector<double> k(p, 0.0);

    // Step-down to recover reflection coefficients k[0..p-1].
    auto cur = av;
    double e = efinal;
    for (int i = p; i >= 1; --i) {
        const double ki = cur[i];
        k[i - 1] = ki;
        if (std::abs(1.0 - ki * ki) < 1e-300) break;
        std::vector<double> prev(i + 1, 0.0);  // a_{i-1}
        prev[0] = 1.0;
        const double denom = 1.0 - ki * ki;
        for (int j = 1; j < i; ++j)
            prev[j] = (cur[j] - ki * cur[i - j]) / denom;
        cur = prev;
        e /= denom;  // step e back up
    }

    // Forward Levinson on the reflection coeffs, recovering R via
    // r[i] = -(a[1]*r[i-1] + ... + a[i-1]*r[1] + a[i]*r[0]).
    R[0] = e;  // reconstruct r[0] from prediction error climb
    for (int i = 1; i <= p; ++i) {
        double s = 0.0;
        for (int j = 1; j <= i - 1; ++j) s += av[j] * R[i - j];
        R[i] = -av[i] * R[0] - s;
    }
    return std::make_tuple(rowVec(mr, R), colVec(mr, k));
}

// ── AR estimation ─────────────────────────────────────────────────

std::tuple<Value, Value, Value>
aryule(std::pmr::memory_resource *mr, const Value &x, int p)
{
    auto v = readVec(x);
    auto R = biasedAutocorr(v, p);
    auto res = levinsonCore(R, p);
    return std::make_tuple(rowVec(mr, res.a),
                           Value::scalar(res.e, mr),
                           colVec(mr, res.k));
}

std::tuple<Value, Value, Value>
arburg(std::pmr::memory_resource *mr, const Value &x, int p)
{
    // Burg's algorithm. Maintains forward / backward prediction
    // errors and updates AR coefficients via reflection coefficients
    // chosen to minimise the sum of forward + backward residual energy.
    auto v = readVec(x);
    const size_t N = v.size();
    std::vector<double> ef = v;
    std::vector<double> eb = v;
    std::vector<double> a(p + 1, 0.0); a[0] = 1.0;
    std::vector<double> k(p, 0.0);

    double E = 0.0;
    for (double y : v) E += y * y;
    E /= static_cast<double>(N);

    for (int i = 0; i < p; ++i) {
        // Reflection coefficient: maximise correlation of forward and
        // backward errors.
        double num = 0.0, den = 0.0;
        for (size_t n = i + 1; n < N; ++n) {
            num += ef[n] * eb[n - 1];
            den += ef[n] * ef[n] + eb[n - 1] * eb[n - 1];
        }
        const double ki = (den > 0) ? -2.0 * num / den : 0.0;
        k[i] = ki;

        // Update prediction errors in-place.
        for (size_t n = N - 1; n > static_cast<size_t>(i); --n) {
            const double f = ef[n];
            const double b = eb[n - 1];
            ef[n] = f + ki * b;
            eb[n] = b + ki * f;
        }

        // Update AR coefficients via the Levinson update.
        std::vector<double> a_new = a;
        for (int j = 1; j <= i; ++j)
            a_new[j] = a[j] + ki * a[i + 1 - j];
        a_new[i + 1] = ki;
        a = std::move(a_new);
        E *= (1.0 - ki * ki);
    }

    return std::make_tuple(rowVec(mr, a),
                           Value::scalar(E, mr),
                           colVec(mr, k));
}

std::tuple<Value, Value>
lpc(std::pmr::memory_resource *mr, const Value &x, int p)
{
    // MATLAB's lpc returns (a, g) where g is the prediction error
    // variance (NOT its sqrt — the docs are explicit). Same numbers
    // as aryule, just without the reflection-coef return.
    auto [a, e, k] = aryule(mr, x, p);
    (void)k;
    return std::make_tuple(std::move(a), std::move(e));
}

// ── Representation conversions ────────────────────────────────────

std::tuple<Value, Value>
ac2poly(std::pmr::memory_resource *mr, const Value &R)
{
    auto rv = readVec(R);
    const int p = static_cast<int>(rv.size()) - 1;
    auto res = levinsonCore(rv, std::max(p, 0));
    return std::make_tuple(rowVec(mr, res.a), Value::scalar(res.e, mr));
}

Value poly2ac(std::pmr::memory_resource *mr, const Value &a, double e)
{
    auto [Rv, kv] = rlevinson(mr, a, e);
    (void)kv;
    return Rv;
}

std::tuple<Value, Value>
ac2rc(std::pmr::memory_resource *mr, const Value &R)
{
    auto rv = readVec(R);
    const int p = static_cast<int>(rv.size()) - 1;
    auto res = levinsonCore(rv, std::max(p, 0));
    return std::make_tuple(colVec(mr, res.k),
                           Value::scalar(rv.empty() ? 0.0 : rv[0], mr));
}

Value rc2ac(std::pmr::memory_resource *mr, const Value &k, double r0)
{
    auto kv = readVec(k);
    const int p = static_cast<int>(kv.size());
    // Build R from k via the inverse Levinson: r0 known, then
    // r[i] = -(a_{i-1}[1]*r[i-1] + ... + a_{i-1}[i-1]*r[1]) - k[i-1]*e_{i-1}
    std::vector<double> R(p + 1, 0.0);
    R[0] = r0;
    std::vector<double> a(p + 1, 0.0); a[0] = 1.0;
    double e = r0;
    for (int i = 1; i <= p; ++i) {
        const double ki = kv[i - 1];
        double s = 0.0;
        for (int j = 1; j < i; ++j) s += a[j] * R[i - j];
        R[i] = -ki * e - s;
        // Update a by step-up.
        std::vector<double> a_new(p + 1, 0.0);
        a_new[0] = 1.0;
        for (int j = 1; j < i; ++j) a_new[j] = a[j] + ki * a[i - j];
        a_new[i] = ki;
        a = std::move(a_new);
        e *= (1.0 - ki * ki);
    }
    return rowVec(mr, R);
}

Value poly2rc(std::pmr::memory_resource *mr, const Value &a)
{
    auto av = readVec(a);
    const int p = static_cast<int>(av.size()) - 1;
    std::vector<double> k(p, 0.0);
    auto cur = av;
    for (int i = p; i >= 1; --i) {
        const double ki = cur[i];
        k[i - 1] = ki;
        const double denom = 1.0 - ki * ki;
        if (std::abs(denom) < 1e-300) break;
        std::vector<double> prev(i + 1, 0.0); prev[0] = 1.0;
        for (int j = 1; j < i; ++j)
            prev[j] = (cur[j] - ki * cur[i - j]) / denom;
        cur = prev;
    }
    return colVec(mr, k);
}

Value rc2poly(std::pmr::memory_resource *mr, const Value &k)
{
    auto kv = readVec(k);
    const int p = static_cast<int>(kv.size());
    std::vector<double> a(p + 1, 0.0); a[0] = 1.0;
    for (int i = 1; i <= p; ++i) {
        const double ki = kv[i - 1];
        std::vector<double> a_new(p + 1, 0.0);
        a_new[0] = 1.0;
        for (int j = 1; j < i; ++j) a_new[j] = a[j] + ki * a[i - j];
        a_new[i] = ki;
        a = std::move(a_new);
    }
    return rowVec(mr, a);
}

// is2rc / rc2is — inverse-sine parameterisation, k = sin(is).
Value is2rc(std::pmr::memory_resource *mr, const Value &is)
{
    auto v = readVec(is);
    for (auto &y : v) y = std::sin(y);
    return colVec(mr, v);
}
Value rc2is(std::pmr::memory_resource *mr, const Value &k)
{
    auto v = readVec(k);
    for (auto &y : v) {
        // Clamp into [-1, 1] to avoid asin domain errors on numerical
        // overshoots.
        const double c = std::max(-1.0, std::min(1.0, y));
        y = std::asin(c);
    }
    return colVec(mr, v);
}

// lar2rc / rc2lar — log-area-ratio: lar = log((1+k)/(1-k)).
Value lar2rc(std::pmr::memory_resource *mr, const Value &g)
{
    auto v = readVec(g);
    for (auto &y : v) {
        const double e = std::exp(y);
        y = (e - 1.0) / (e + 1.0);
    }
    return colVec(mr, v);
}
Value rc2lar(std::pmr::memory_resource *mr, const Value &k)
{
    auto v = readVec(k);
    for (auto &y : v) {
        const double c = std::max(-0.99999999, std::min(0.99999999, y));
        y = std::log((1.0 + c) / (1.0 - c));
    }
    return colVec(mr, v);
}

// ════════════════════════════════════════════════════════════════════
// Engine adapters
// ════════════════════════════════════════════════════════════════════

namespace detail {

void levinson_reg(Span<const Value> args, size_t nargout,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("levinson: requires at least 1 argument",
                     0, 0, "levinson", "", "m:levinson:nargin");
    int n = -1;
    if (args.size() >= 2 && !args[1].isEmpty()) n = static_cast<int>(args[1].toScalar());
    auto [a, e, k] = levinson(ctx.engine->resource(), args[0], n);
    outs[0] = std::move(a);
    if (nargout > 1) outs[1] = std::move(e);
    if (nargout > 2) outs[2] = std::move(k);
}

void rlevinson_reg(Span<const Value> args, size_t nargout,
                   Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("rlevinson: requires (a, e)",
                     0, 0, "rlevinson", "", "m:rlevinson:nargin");
    auto [R, k] = rlevinson(ctx.engine->resource(), args[0], args[1].toScalar());
    outs[0] = std::move(R);
    if (nargout > 1) outs[1] = std::move(k);
}

#define NK_AR_REG(name, fn)                                                     \
    void name##_reg(Span<const Value> args, size_t nargout,                    \
                    Span<Value> outs, CallContext &ctx)                        \
    {                                                                            \
        if (args.size() < 2)                                                     \
            throw Error(#name ": requires (x, p)",                              \
                         0, 0, #name, "", "m:" #name ":nargin");                 \
        const int p = static_cast<int>(args[1].toScalar());                     \
        auto [a, e, k] = fn(ctx.engine->resource(), args[0], p);                \
        outs[0] = std::move(a);                                                  \
        if (nargout > 1) outs[1] = std::move(e);                                 \
        if (nargout > 2) outs[2] = std::move(k);                                 \
    }

NK_AR_REG(aryule, aryule)
NK_AR_REG(arburg, arburg)

#undef NK_AR_REG

void lpc_reg(Span<const Value> args, size_t nargout,
             Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("lpc: requires (x, p)",
                     0, 0, "lpc", "", "m:lpc:nargin");
    const int p = static_cast<int>(args[1].toScalar());
    auto [a, g] = lpc(ctx.engine->resource(), args[0], p);
    outs[0] = std::move(a);
    if (nargout > 1) outs[1] = std::move(g);
}

void ac2poly_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.empty()) throw Error("ac2poly: requires 1 argument", 0, 0, "ac2poly", "", "m:ac2poly:nargin");
    auto [a, e] = ac2poly(ctx.engine->resource(), args[0]);
    outs[0] = std::move(a);
    if (nargout > 1) outs[1] = std::move(e);
}

void poly2ac_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty()) throw Error("poly2ac: requires 1 argument", 0, 0, "poly2ac", "", "m:poly2ac:nargin");
    const double e = (args.size() >= 2 && !args[1].isEmpty()) ? args[1].toScalar() : 1.0;
    outs[0] = poly2ac(ctx.engine->resource(), args[0], e);
}

void ac2rc_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.empty()) throw Error("ac2rc: requires 1 argument", 0, 0, "ac2rc", "", "m:ac2rc:nargin");
    auto [k, r0] = ac2rc(ctx.engine->resource(), args[0]);
    outs[0] = std::move(k);
    if (nargout > 1) outs[1] = std::move(r0);
}

void rc2ac_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2) throw Error("rc2ac: requires (k, r0)", 0, 0, "rc2ac", "", "m:rc2ac:nargin");
    outs[0] = rc2ac(ctx.engine->resource(), args[0], args[1].toScalar());
}

#define NK_UNARY_CONV_REG(name)                                                 \
    void name##_reg(Span<const Value> args, size_t /*nargout*/,                \
                    Span<Value> outs, CallContext &ctx)                        \
    {                                                                            \
        if (args.empty())                                                        \
            throw Error(#name ": requires 1 argument",                          \
                         0, 0, #name, "", "m:" #name ":nargin");                 \
        outs[0] = name(ctx.engine->resource(), args[0]);                         \
    }

NK_UNARY_CONV_REG(poly2rc)
NK_UNARY_CONV_REG(rc2poly)
NK_UNARY_CONV_REG(is2rc)
NK_UNARY_CONV_REG(rc2is)
NK_UNARY_CONV_REG(lar2rc)
NK_UNARY_CONV_REG(rc2lar)

#undef NK_UNARY_CONV_REG

} // namespace detail
} // namespace numkit::signal
