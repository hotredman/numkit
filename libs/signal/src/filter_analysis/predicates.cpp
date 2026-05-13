// libs/signal/src/filter_analysis/predicates.cpp
//
// isfir / isstable / isminphase / ismaxphase / islinphase / isallpass.

#include <numkit/signal/filter_analysis/predicates.hpp>

#include <numkit/builtin/math/poly/polynomials.hpp>             // roots()
#include <numkit/signal/filter_analysis/frequency_response.hpp> // freqz()
#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include <algorithm>
#include <cmath>
#include <complex>

namespace numkit::signal {

namespace {

constexpr double kTol = 1e-12;

bool isTrivialA(const Value &a)
{
    if (a.isEmpty()) return true;
    const size_t n = a.numel();
    if (n == 0) return true;
    if (std::abs(a.elemAsDouble(0) - 1.0) > kTol) return false;
    for (size_t i = 1; i < n; ++i)
        if (std::abs(a.elemAsDouble(i)) > kTol) return false;
    return true;
}

double maxRootRadius(const Value &p, std::pmr::memory_resource *mr)
{
    if (p.numel() < 2) return 0.0;
    auto r = builtin::roots(p, mr);
    double mx = 0.0;
    if (r.isComplex()) {
        const Complex *src = r.complexData();
        for (size_t i = 0; i < r.numel(); ++i)
            mx = std::max(mx, std::abs(src[i]));
    } else {
        const double *src = r.doubleData();
        for (size_t i = 0; i < r.numel(); ++i)
            mx = std::max(mx, std::abs(src[i]));
    }
    return mx;
}

// Strip trailing zeros from a polynomial vector — they don't change the
// polynomial value but may confuse symmetry tests on b.
std::vector<double> trimTrailingZeros(const Value &p)
{
    std::vector<double> v;
    v.reserve(p.numel());
    for (size_t i = 0; i < p.numel(); ++i)
        v.push_back(p.elemAsDouble(i));
    while (v.size() > 1 && std::abs(v.back()) <= kTol) v.pop_back();
    return v;
}

bool isSymmetric(const std::vector<double> &v, double scale = 1.0, double tol = 1e-9)
{
    const size_t n = v.size();
    for (size_t i = 0; i < n / 2; ++i)
        if (std::abs(v[i] - scale * v[n - 1 - i]) > tol * (1.0 + std::abs(v[i])))
            return false;
    return true;
}

} // namespace

// ── isfir ─────────────────────────────────────────────────────────────
bool isfir(const Value &b)
{
    (void)b;
    return true;   // single-arg form: anything with implicit a=1 is FIR
}

bool isfir(const Value & /*b*/, const Value &a)
{
    return isTrivialA(a);
}

// ── isstable ──────────────────────────────────────────────────────────
bool isstable(const Value & /*b*/, const Value &a, std::pmr::memory_resource *mr)
{
    if (isTrivialA(a)) return true;          // FIR is always stable
    return maxRootRadius(a, mr) < 1.0 - 1e-9;
}

// ── isminphase ────────────────────────────────────────────────────────
bool isminphase(const Value &b, const Value &a, std::pmr::memory_resource *mr)
{
    if (!isstable(b, a, mr)) return false;
    return maxRootRadius(b, mr) < 1.0 - 1e-9;
}

// ── ismaxphase ────────────────────────────────────────────────────────
bool ismaxphase(const Value &b, const Value &a, std::pmr::memory_resource *mr)
{
    // All zeros must be OUTSIDE the unit circle. Implementation: invert
    // the polynomial (reverse coefficients) and check that *its* roots
    // are inside — equivalent and avoids min(abs(roots(b))) edge cases
    // when some roots are exactly on the unit circle.
    if (b.numel() < 2) return false;          // constant has no zeros
    auto coeffs = trimTrailingZeros(b);
    if (coeffs.size() < 2) return false;
    std::reverse(coeffs.begin(), coeffs.end());
    auto reversed = Value::matrix(1, coeffs.size(), ValueType::DOUBLE, mr);
    double *dst = reversed.doubleDataMut();
    for (size_t i = 0; i < coeffs.size(); ++i) dst[i] = coeffs[i];
    if (maxRootRadius(reversed, mr) >= 1.0 - 1e-9)
        return false;
    // Plus the filter must be FIR (denominator trivial) — IIR maxphase
    // is rare; MATLAB checks denominator separately.
    return isTrivialA(a);
}

// ── islinphase ────────────────────────────────────────────────────────
bool islinphase(const Value &b, const Value &a)
{
    if (!isTrivialA(a)) return false;        // IIR ≠ linear phase
    auto v = trimTrailingZeros(b);
    if (v.size() <= 1) return true;
    return isSymmetric(v, +1.0) || isSymmetric(v, -1.0);
}

// ── isallpass ─────────────────────────────────────────────────────────
// True when |H(e^{jw})| is constant. For real-coefficient transfer
// functions: a == flip(b) (up to a constant scale).
bool isallpass(const Value &b, const Value &a)
{
    auto bv = trimTrailingZeros(b);
    auto av = trimTrailingZeros(a);
    if (bv.size() != av.size() || bv.empty()) return false;
    // Find the scale factor between leading coefficient of a and trailing of b.
    if (std::abs(bv.back()) < kTol) return false;
    const double scale = av.front() / bv.back();
    for (size_t i = 0; i < bv.size(); ++i) {
        const double expected = scale * bv[bv.size() - 1 - i];
        if (std::abs(av[i] - expected) > 1e-9 * (1.0 + std::abs(av[i])))
            return false;
    }
    return true;
}

namespace detail {

static Value boolVal(bool b, std::pmr::memory_resource *mr)
{
    auto v = Value::matrix(1, 1, ValueType::LOGICAL, mr);
    v.logicalDataMut()[0] = b ? 1 : 0;
    return v;
}

void isfir_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("isfir: requires at least 1 argument (b)",
                     0, 0, "isfir", "", "m:isfir:nargin");
    const bool r = (args.size() >= 2) ? isfir(args[0], args[1]) : isfir(args[0]);
    outs[0] = boolVal(r, ctx.engine->resource());
}

void isstable_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("isstable: requires at least 1 argument (b)",
                     0, 0, "isstable", "", "m:isstable:nargin");
    const Value &b = args[0];
    Value a = (args.size() >= 2) ? args[1] : Value::scalar(1.0, ctx.engine->resource());
    outs[0] = boolVal(isstable(b, a, ctx.engine->resource()), ctx.engine->resource());
}

void isminphase_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("isminphase: requires at least 1 argument (b)",
                     0, 0, "isminphase", "", "m:isminphase:nargin");
    const Value &b = args[0];
    Value a = (args.size() >= 2) ? args[1] : Value::scalar(1.0, ctx.engine->resource());
    outs[0] = boolVal(isminphase(b, a, ctx.engine->resource()), ctx.engine->resource());
}

void ismaxphase_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("ismaxphase: requires at least 1 argument (b)",
                     0, 0, "ismaxphase", "", "m:ismaxphase:nargin");
    const Value &b = args[0];
    Value a = (args.size() >= 2) ? args[1] : Value::scalar(1.0, ctx.engine->resource());
    outs[0] = boolVal(ismaxphase(b, a, ctx.engine->resource()), ctx.engine->resource());
}

void islinphase_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("islinphase: requires at least 1 argument (b)",
                     0, 0, "islinphase", "", "m:islinphase:nargin");
    const Value &b = args[0];
    Value a = (args.size() >= 2) ? args[1] : Value::scalar(1.0, ctx.engine->resource());
    outs[0] = boolVal(islinphase(b, a), ctx.engine->resource());
}

void isallpass_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("isallpass: requires (b, a)",
                     0, 0, "isallpass", "", "m:isallpass:nargin");
    outs[0] = boolVal(isallpass(args[0], args[1]), ctx.engine->resource());
}

void filtord_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("filtord: requires at least 1 argument (b)",
                     0, 0, "filtord", "", "m:filtord:nargin");
    int n = (args.size() >= 2)
        ? filtord(args[0], args[1])
        : filtord(args[0]);
    outs[0] = Value::scalar(static_cast<double>(n), ctx.engine->resource());
}

void firtype_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("firtype: requires 1 argument (b)",
                     0, 0, "firtype", "", "m:firtype:nargin");
    outs[0] = Value::scalar(static_cast<double>(firtype(args[0])), ctx.engine->resource());
}

void filternorm_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("filternorm: requires (b, a [, pnorm])",
                     0, 0, "filternorm", "", "m:filternorm:nargin");
    double p = 2.0;
    if (args.size() >= 3 && !args[2].isEmpty()) p = args[2].toScalar();
    outs[0] = Value::scalar(filternorm(args[0], args[1], p, ctx.engine->resource()),
                            ctx.engine->resource());
}

} // namespace detail

// ── filtord ────────────────────────────────────────────────────────
int filtord(const Value &b)
{
    auto bv = trimTrailingZeros(b);
    if (bv.empty()) return 0;
    return static_cast<int>(bv.size()) - 1;
}

int filtord(const Value &b, const Value &a)
{
    auto bv = trimTrailingZeros(b);
    auto av = trimTrailingZeros(a);
    const size_t lb = bv.empty() ? 0 : bv.size();
    const size_t la = av.empty() ? 0 : av.size();
    const size_t n  = std::max(lb, la);
    return (n == 0) ? 0 : static_cast<int>(n) - 1;
}

// ── firtype ────────────────────────────────────────────────────────
int firtype(const Value &b)
{
    auto v = trimTrailingZeros(b);
    if (v.size() < 2)
        throw Error("firtype: filter must have at least 2 coefficients",
                     0, 0, "firtype", "", "m:firtype:short");
    const bool sym  = isSymmetric(v, +1.0);
    const bool anti = isSymmetric(v, -1.0);
    if (!sym && !anti)
        throw Error("firtype: filter is not (anti)symmetric — not a "
                    "linear-phase FIR",
                     0, 0, "firtype", "", "m:firtype:notlinphase");
    const int order = static_cast<int>(v.size()) - 1;  // L = order + 1
    const bool order_even = (order % 2 == 0);
    if (sym)  return order_even ? 1 : 2;
    /* anti */ return order_even ? 3 : 4;
}

// ── filternorm ─────────────────────────────────────────────────────
double filternorm(const Value &b, const Value &a, double pnorm, std::pmr::memory_resource *mr)
{
    constexpr size_t kNpts = 8192;
    auto [H, W] = freqz(b, a, kNpts, mr);
    const Complex *hd = H.complexData();

    if (std::isinf(pnorm)) {
        // L∞: max |H(e^{jw})| over the freqz grid.
        double mx = 0.0;
        for (size_t k = 0; k < kNpts; ++k) {
            const double m = std::abs(hd[k]);
            if (m > mx) mx = m;
        }
        return mx;
    }
    if (pnorm != 2.0)
        throw Error("filternorm: only pnorm = 2 or inf supported",
                     0, 0, "filternorm", "", "m:filternorm:p");
    // L2: sqrt((1/π) ∫_0^π |H|² dw) via trapezoidal rule on freqz grid.
    // freqz returns npts samples on [0, π] inclusive — Δw = π/(npts-1).
    double sum = 0.0;
    for (size_t k = 0; k < kNpts; ++k) {
        const double mag2 = std::norm(hd[k]);  // |z|² for complex
        const double w = (k == 0 || k == kNpts - 1) ? 0.5 : 1.0;
        sum += w * mag2;
    }
    return std::sqrt(sum / static_cast<double>(kNpts - 1));
}

} // namespace numkit::signal
