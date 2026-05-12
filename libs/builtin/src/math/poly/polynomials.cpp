// libs/builtin/src/math/elementary/polynomials.cpp

#include <numkit/builtin/math/poly/polynomials.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/scratch.hpp>
#include <numkit/core/types.hpp>

#include "helpers.hpp"
#include "poly_helpers.hpp"

#include <numkit/builtin/language/arrays/matrix.hpp>  // poly_of_matrix

#include <cmath>
#include <cstring>
#include <memory_resource>
#include <tuple>

namespace numkit::builtin {

Value roots(const Value &p, std::pmr::memory_resource *mr)
{
    if (p.type() == ValueType::COMPLEX)
        throw Error("roots: complex coefficient input is not supported",
                     0, 0, "roots", "", "m:roots:complex");
    if (!p.dims().isVector() && !p.isScalar() && !p.isEmpty())
        throw Error("roots: argument must be a vector",
                     0, 0, "roots", "", "m:roots:notVector");

    ScratchArena scratch(mr);

    // Read coefficients as DOUBLE (promote integer/single/logical).
    const std::size_t n = p.numel();
    auto coeffs = ScratchVec<double>(n, &scratch);
    for (std::size_t i = 0; i < n; ++i)
        coeffs[i] = p.elemAsDouble(i);

    auto rs = detail::polyRootsDurandKerner(&scratch, coeffs.data(), coeffs.size());
    const std::size_t k = rs.size();

    // If every root is real, return a real column. Otherwise return COMPLEX.
    bool anyComplex = false;
    for (const auto &r : rs)
        if (std::abs(r.imag()) > 1e-12 * (std::abs(r.real()) + 1.0)) {
            anyComplex = true;
            break;
        }

    if (!anyComplex) {
        auto out = Value::matrix(k, 1, ValueType::DOUBLE, mr);
        for (std::size_t i = 0; i < k; ++i)
            out.doubleDataMut()[i] = rs[i].real();
        return out;
    }
    auto out = Value::complexMatrix(k, 1, mr);
    for (std::size_t i = 0; i < k; ++i)
        out.complexDataMut()[i] = rs[i];
    return out;
}

// ── polyder / polyint ───────────────────────────────────────────────
namespace {

ScratchVec<double> readPolyAsDouble(const Value &p, const char *fn, std::pmr::memory_resource *mr)
{
    if (p.type() == ValueType::COMPLEX)
        throw Error(std::string(fn) + ": complex coefficient input is not supported",
                     0, 0, fn, "", std::string("m:") + fn + ":complex");
    if (!p.dims().isVector() && !p.isScalar() && !p.isEmpty())
        throw Error(std::string(fn) + ": argument must be a vector",
                     0, 0, fn, "", std::string("m:") + fn + ":notVector");
    const std::size_t n = p.numel();
    ScratchVec<double> v(n, mr);
    for (std::size_t i = 0; i < n; ++i) v[i] = p.elemAsDouble(i);
    return v;
}

// Convolve two real polynomial coefficient vectors (length-N + length-M
// → length-N+M-1). Pointer + size for inputs so the same helper composes
// with std::vector and std::pmr::vector backings.
ScratchVec<double> polyConv(const double *a, std::size_t na, const double *b, std::size_t nb, std::pmr::memory_resource *mr)
{
    if (na == 0 || nb == 0) return ScratchVec<double>(mr);
    ScratchVec<double> r(na + nb - 1, mr);
    for (std::size_t i = 0; i < na; ++i)
        for (std::size_t j = 0; j < nb; ++j)
            r[i + j] += a[i] * b[j];
    return r;
}

// d/dx of a coefficient vector in MATLAB order.
ScratchVec<double> polyderRaw(const double *p, std::size_t pn, std::pmr::memory_resource *mr)
{
    if (pn <= 1) {
        ScratchVec<double> r(1, mr);
        r[0] = 0.0;  // constant → derivative is [0].
        return r;
    }
    const std::size_t n = pn - 1;  // degree
    ScratchVec<double> r(n, mr);
    for (std::size_t i = 0; i < n; ++i) {
        const double exponent = static_cast<double>(n - i);
        r[i] = p[i] * exponent;
    }
    return r;
}

Value rowFromVec(const double *v, std::size_t n, std::pmr::memory_resource *mr)
{
    auto out = Value::matrix(1, n, ValueType::DOUBLE, mr);
    if (n > 0)
        std::memcpy(out.doubleDataMut(), v, n * sizeof(double));
    return out;
}

// Trim trailing zeros that arise from a-b cancellation in polyder(b,a).
void trimLeadingZeros(ScratchVec<double> &v)
{
    std::size_t lo = 0;
    while (lo + 1 < v.size() && v[lo] == 0.0) ++lo;
    if (lo > 0) v.erase(v.begin(), v.begin() + lo);
}

} // namespace

Value polyder(const Value &p, std::pmr::memory_resource *mr)
{
    ScratchArena scratch(mr);
    auto pv = readPolyAsDouble(p, "polyder", &scratch);
    auto deriv = polyderRaw(pv.data(), pv.size(), &scratch);
    // MATLAB convention: polynomial coefficient vectors are canonical
    // (no leading zeros), so a polyder result like [0, 1, 2] must be
    // trimmed to [1, 2]. See BUGS.md #12.
    trimLeadingZeros(deriv);
    return rowFromVec(deriv.data(), deriv.size(), mr);
}

std::tuple<Value, Value>
polyder(const Value &b, const Value &a, std::pmr::memory_resource *mr)
{
    ScratchArena scratch(mr);
    auto bv = readPolyAsDouble(b, "polyder", &scratch);
    auto av = readPolyAsDouble(a, "polyder", &scratch);
    auto bp = polyderRaw(bv.data(), bv.size(), &scratch);
    auto ap = polyderRaw(av.data(), av.size(), &scratch);
    // num = a * b' - b * a'
    auto t1 = polyConv(av.data(), av.size(), bp.data(), bp.size(), &scratch);
    auto t2 = polyConv(bv.data(), bv.size(), ap.data(), ap.size(), &scratch);
    // Align lengths (pad with leading zeros so subtraction is safe).
    if (t1.size() < t2.size()) t1.insert(t1.begin(), t2.size() - t1.size(), 0.0);
    if (t2.size() < t1.size()) t2.insert(t2.begin(), t1.size() - t2.size(), 0.0);
    ScratchVec<double> num(t1.size(), &scratch);
    for (std::size_t i = 0; i < t1.size(); ++i) num[i] = t1[i] - t2[i];
    trimLeadingZeros(num);
    auto den = polyConv(av.data(), av.size(), av.data(), av.size(), &scratch);
    trimLeadingZeros(den);
    return std::make_tuple(rowFromVec(num.data(), num.size(), mr),
                           rowFromVec(den.data(), den.size(), mr));
}

// Read a vector input of (real or COMPLEX) numbers as a Complex vector.
namespace {

ScratchVec<detail::Complex> readVecAsComplex(const Value &v, const char *fn, std::pmr::memory_resource *mr)
{
    if (!v.dims().isVector() && !v.isScalar() && !v.isEmpty())
        throw Error(std::string(fn) + ": argument must be a vector",
                     0, 0, fn, "", std::string("m:") + fn + ":notVector");
    const std::size_t n = v.numel();
    ScratchVec<detail::Complex> r(n, mr);
    if (v.type() == ValueType::COMPLEX) {
        const auto *p = v.complexData();
        for (std::size_t i = 0; i < n; ++i) r[i] = p[i];
    } else {
        for (std::size_t i = 0; i < n; ++i)
            r[i] = detail::Complex(v.elemAsDouble(i), 0.0);
    }
    return r;
}

Value complexColFromVec(const detail::Complex *v, std::size_t n, std::pmr::memory_resource *mr)
{
    auto out = Value::complexMatrix(n, 1, mr);
    for (std::size_t i = 0; i < n; ++i)
        out.complexDataMut()[i] = v[i];
    return out;
}

Value realColIfFlat(const detail::Complex *v, std::size_t n, std::pmr::memory_resource *mr)
{
    bool anyComplex = false;
    for (std::size_t i = 0; i < n; ++i)
        if (std::abs(v[i].imag()) > 1e-12 * (std::abs(v[i].real()) + 1.0)) {
            anyComplex = true;
            break;
        }
    if (!anyComplex) {
        auto out = Value::matrix(n, 1, ValueType::DOUBLE, mr);
        for (std::size_t i = 0; i < n; ++i)
            out.doubleDataMut()[i] = v[i].real();
        return out;
    }
    return complexColFromVec(v, n, mr);
}

} // namespace

Value polyint(const Value &p, double k, std::pmr::memory_resource *mr)
{
    ScratchArena scratch(mr);
    auto pv = readPolyAsDouble(p, "polyint", &scratch);
    if (pv.empty()) {
        // ∫ 0 dx = k.
        auto out = Value::matrix(1, 1, ValueType::DOUBLE, mr);
        out.doubleDataMut()[0] = k;
        return out;
    }
    const std::size_t n = pv.size();
    auto r = ScratchVec<double>(n + 1, &scratch);
    for (std::size_t i = 0; i < n; ++i) {
        const double exponent = static_cast<double>(n - i);
        r[i] = pv[i] / exponent;
    }
    r[n] = k;
    return rowFromVec(r.data(), r.size(), mr);
}

// ── tf2zp / zp2tf ───────────────────────────────────────────────────
std::tuple<Value, Value, Value>
tf2zp(const Value &b, const Value &a, std::pmr::memory_resource *mr)
{
    ScratchArena scratch(mr);
    auto bv = readPolyAsDouble(b, "tf2zp", &scratch);
    auto av = readPolyAsDouble(a, "tf2zp", &scratch);
    if (av.empty() || av[0] == 0.0)
        throw Error("tf2zp: leading denominator coefficient must be non-zero",
                     0, 0, "tf2zp", "", "m:tf2zp:badDen");
    if (bv.empty()) {
        // Numerator = 0 → no zeros, gain 0.
        auto z = Value::matrix(0, 1, ValueType::DOUBLE, mr);
        auto pRoots = detail::polyRootsDurandKerner(&scratch, av.data(), av.size());
        auto p = realColIfFlat(pRoots.data(), pRoots.size(), mr);
        auto k = Value::scalar(0.0, mr);
        return std::make_tuple(std::move(z), std::move(p), std::move(k));
    }
    auto zRoots = detail::polyRootsDurandKerner(&scratch, bv.data(), bv.size());
    auto pRoots = detail::polyRootsDurandKerner(&scratch, av.data(), av.size());
    const double k = bv[0] / av[0];

    return std::make_tuple(realColIfFlat(zRoots.data(), zRoots.size(), mr),
                           realColIfFlat(pRoots.data(), pRoots.size(), mr),
                           Value::scalar(k, mr));
}

std::tuple<Value, Value>
zp2tf(const Value &z, const Value &p, double k, std::pmr::memory_resource *mr)
{
    ScratchArena scratch(mr);
    auto zv = readVecAsComplex(z, "zp2tf", &scratch);
    auto pv = readVecAsComplex(p, "zp2tf", &scratch);
    auto bRaw = detail::polyExpandFromRoots(&scratch, zv.data(), zv.size());
    auto aRaw = detail::polyExpandFromRoots(&scratch, pv.data(), pv.size());
    for (auto &v : bRaw) v *= k;
    return std::make_tuple(rowFromVec(bRaw.data(), bRaw.size(), mr),
                           rowFromVec(aRaw.data(), aRaw.size(), mr));
}

// ── Pack 29: poly / polyvalm / polydiv ───────────────────────────────

Value poly(const Value &r, std::pmr::memory_resource *mr)
{
    // Vector-of-roots → coefficient row. Real-only path uses the
    // existing helper (which drops the imaginary residue, fine when
    // the roots have come from `roots(p)` of a real polynomial).
    if (r.isEmpty()) return rowFromVec(nullptr, 0, mr);
    ScratchArena scratch(mr);
    const size_t n = r.numel();
    auto cv = ScratchVec<Complex>(n, &scratch);
    if (r.isComplex()) {
        const Complex *p = r.complexData();
        for (size_t i = 0; i < n; ++i) cv[i] = p[i];
    } else {
        const double *p = r.doubleData();
        for (size_t i = 0; i < n; ++i) cv[i] = Complex(p[i], 0.0);
    }
    auto coeffs = detail::polyExpandFromRoots(&scratch, cv.data(), n);
    return rowFromVec(coeffs.data(), coeffs.size(), mr);
}

Value polyvalm(const Value &p, const Value &A, std::pmr::memory_resource *mr)
{
    if (A.dims().ndim() > 2 || A.dims().rows() != A.dims().cols())
        throw Error("polyvalm: A must be a square matrix",
                     0, 0, "polyvalm", "", "m:polyvalm:notSquare");
    const size_t n = A.dims().rows();
    const size_t k = p.numel();

    // Build `result = p_0 * I` (deg-0 case shortcut included).
    auto I = Value::matrix(n, n, ValueType::DOUBLE, mr);
    {
        double *d = I.doubleDataMut();
        std::fill(d, d + n * n, 0.0);
        for (size_t i = 0; i < n; ++i) d[i * n + i] = 1.0;
    }
    if (k == 0) return I;

    // Horner at the matrix level: result = result * A + p_i * I.
    // p[0] is the leading coefficient.
    auto result = Value::matrix(n, n, ValueType::DOUBLE, mr);
    {
        double *d = result.doubleDataMut();
        const double p0 = p.doubleData()[0];
        for (size_t i = 0; i < n * n; ++i) d[i] = 0.0;
        for (size_t i = 0; i < n; ++i) d[i * n + i] = p0;
    }
    for (size_t step = 1; step < k; ++step) {
        // result = result * A
        auto next = Value::matrix(n, n, ValueType::DOUBLE, mr);
        const double *Rp = result.doubleData();
        const double *Ap = A.doubleData();
        double *Np = next.doubleDataMut();
        // Column-major matmul.
        for (size_t i = 0; i < n; ++i)
            for (size_t j = 0; j < n; ++j) {
                double s = 0.0;
                for (size_t kk = 0; kk < n; ++kk)
                    s += Rp[kk * n + i] * Ap[j * n + kk];
                Np[j * n + i] = s;
            }
        // result = next + p[step] * I
        const double pi = p.doubleData()[step];
        for (size_t i = 0; i < n; ++i) Np[i * n + i] += pi;
        result = std::move(next);
    }
    return result;
}

PolyDiv polydiv(const Value &b, const Value &a, std::pmr::memory_resource *mr)
{
    if (a.isEmpty() || a.numel() == 0)
        throw Error("polydiv: divisor must be non-empty",
                     0, 0, "polydiv", "", "m:polydiv:emptyA");
    const size_t na = a.numel();
    const size_t nb = b.numel();
    // Strip leading zeros from a (matches MATLAB behaviour).
    size_t aOff = 0;
    while (aOff + 1 < na && a.doubleData()[aOff] == 0.0) ++aOff;
    const size_t aEff = na - aOff;
    if (aEff == 0 || a.doubleData()[aOff] == 0.0)
        throw Error("polydiv: divisor is zero", 0, 0, "polydiv", "",
                     "m:polydiv:zeroDivisor");

    if (nb < aEff) {
        // Quotient is 0; remainder == b.
        auto q = Value::matrix(1, 1, ValueType::DOUBLE, mr);
        q.doubleDataMut()[0] = 0.0;
        return { std::move(q), b };
    }

    ScratchArena scratch(mr);
    auto bb = ScratchVec<double>(nb, &scratch);
    for (size_t i = 0; i < nb; ++i) bb[i] = b.doubleData()[i];
    const double aLead = a.doubleData()[aOff];
    const size_t qLen = nb - aEff + 1;
    auto qv = ScratchVec<double>(qLen, &scratch);
    for (size_t i = 0; i < qLen; ++i) qv[i] = 0.0;

    for (size_t i = 0; i < qLen; ++i) {
        const double coef = bb[i] / aLead;
        qv[i] = coef;
        for (size_t j = 0; j < aEff; ++j)
            bb[i + j] -= coef * a.doubleData()[aOff + j];
    }
    // Remainder: bb[qLen .. nb-1]; trim leading zeros.
    size_t rOff = qLen;
    while (rOff < nb && bb[rOff] == 0.0) ++rOff;
    Value rOut;
    if (rOff >= nb) {
        rOut = Value::matrix(1, 1, ValueType::DOUBLE, mr);
        rOut.doubleDataMut()[0] = 0.0;
    } else {
        rOut = rowFromVec(bb.data() + rOff, nb - rOff, mr);
    }
    return { rowFromVec(qv.data(), qLen, mr), std::move(rOut) };
}

// ── Pack 36: padecoef ────────────────────────────────────────────────
//
// Coefficients of the (N,N) Padé approximant of e^{-T s}:
//
//     e^{-Ts}  ≈  P(-Ts) / P(Ts)
//     P(x)    = Σ_{k=0..N} a_k x^k
//     a_k     = (2N-k)! N! / ((2N)! k! (N-k)!)
//
// Stable forward recurrence:
//     a_0 = 1
//     a_{k+1} / a_k = (N - k) / ((2N - k) (k + 1))
//
// In MATLAB convention (descending power), the coefficient of s^{N-k}
// is `a_k * (-T)^k` for the numerator and `a_k * T^k` for the
// denominator. After computing both vectors we divide through by
// den[0] so the leading denominator coefficient is 1 (matches
// `padecoef(T,N)` in MATLAB).
PadeCoef padecoef(double T, int N, std::pmr::memory_resource *mr)
{
    if (N < 0)
        throw Error("padecoef: order N must be >= 0",
                     0, 0, "padecoef", "", "m:padecoef:badN");

    const size_t n = static_cast<size_t>(N) + 1;

    // a[k] for k = 0..N.
    ScratchArena scratch(mr);
    ScratchVec<double> a(n, &scratch);
    a[0] = 1.0;
    for (int k = 0; k < N; ++k)
        a[k + 1] = a[k] * static_cast<double>(N - k)
                       / (static_cast<double>(2 * N - k) * (k + 1));

    auto numV = Value::matrix(1, n, ValueType::DOUBLE, mr);
    auto denV = Value::matrix(1, n, ValueType::DOUBLE, mr);
    double *num = numV.doubleDataMut();
    double *den = denV.doubleDataMut();

    // Descending: index 0 is s^N, index N is s^0. So position N-k stores
    // the k-th-power-of-s coefficient.
    double powNegT = 1.0, powT = 1.0;
    for (int k = 0; k <= N; ++k) {
        num[N - k] = a[k] * powNegT;
        den[N - k] = a[k] * powT;
        powNegT *= -T;
        powT    *=  T;
    }

    // Normalize so leading denominator coefficient is 1.
    const double scale = den[0];
    if (scale != 0.0) {
        for (size_t i = 0; i < n; ++i) {
            num[i] /= scale;
            den[i] /= scale;
        }
    }
    return { std::move(numV), std::move(denV) };
}

namespace detail {

void roots_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("roots: requires 1 argument",
                     0, 0, "roots", "", "m:roots:nargin");
    outs[0] = roots(args[0], ctx.engine->resource());
}

void polyder_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("polyder: requires at least 1 argument",
                     0, 0, "polyder", "", "m:polyder:nargin");
    std::pmr::memory_resource *mr = ctx.engine->resource();
    if (args.size() == 1) {
        outs[0] = polyder(args[0], mr);
        return;
    }
    auto [num, den] = polyder(args[0], args[1], mr);
    outs[0] = std::move(num);
    if (nargout > 1) outs[1] = std::move(den);
}

void polyint_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("polyint: requires at least 1 argument",
                     0, 0, "polyint", "", "m:polyint:nargin");
    std::pmr::memory_resource *mr = ctx.engine->resource();
    double k = 0.0;
    if (args.size() >= 2) k = args[1].toScalar();
    outs[0] = polyint(args[0], k, mr);
}

void tf2zp_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("tf2zp: requires 2 arguments (b, a)",
                     0, 0, "tf2zp", "", "m:tf2zp:nargin");
    auto [zr, pr, kr] = tf2zp(args[0], args[1], ctx.engine->resource());
    outs[0] = std::move(zr);
    if (nargout > 1) outs[1] = std::move(pr);
    if (nargout > 2) outs[2] = std::move(kr);
}

void zp2tf_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("zp2tf: requires 3 arguments (z, p, k)",
                     0, 0, "zp2tf", "", "m:zp2tf:nargin");
    auto [bv, av] = zp2tf(args[0], args[1], args[2].toScalar(), ctx.engine->resource());
    outs[0] = std::move(bv);
    if (nargout > 1) outs[1] = std::move(av);
}

void polyfit_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("polyfit: requires 3 arguments",
                     0, 0, "polyfit", "", "m:polyfit:nargin");
    outs[0] = polyfit(args[0], args[1], static_cast<int>(args[2].toScalar()), ctx.engine->resource());
}

void polyval_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("polyval: requires 2 arguments",
                     0, 0, "polyval", "", "m:polyval:nargin");
    outs[0] = polyval(args[0], args[1], ctx.engine->resource());
}

void poly_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("poly: requires 1 argument",
                     0, 0, "poly", "", "m:poly:nargin");
    // Dispatch (matches MATLAB behavior):
    //   square matrix (n×n, n>1) -> characteristic polynomial via Souriau-Faddeev
    //   anything else (vector of roots) -> expand (λ - r_1)(λ - r_2)...
    const auto &A = args[0];
    auto *mr = ctx.engine->resource();
    if (A.dims().ndim() == 2
        && A.dims().dim(0) == A.dims().dim(1)
        && A.dims().dim(0) > 1) {
        outs[0] = poly_of_matrix(mr, A);
    } else {
        outs[0] = poly(A, mr);
    }
}

void polyvalm_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("polyvalm: requires (p, A)",
                     0, 0, "polyvalm", "", "m:polyvalm:nargin");
    outs[0] = polyvalm(args[0], args[1], ctx.engine->resource());
}

void padecoef_reg(Span<const Value> args, size_t nargout, Span<Value> outs,
                  CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("padecoef: requires 2 arguments (T, N)",
                     0, 0, "padecoef", "", "m:padecoef:nargin");
    const double T = args[0].toScalar();
    const int    N = static_cast<int>(args[1].toScalar());
    auto p = padecoef(T, N, ctx.engine->resource());
    outs[0] = std::move(p.num);
    if (nargout > 1) outs[1] = std::move(p.den);
}

void polydiv_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("polydiv: requires (b, a)",
                     0, 0, "polydiv", "", "m:polydiv:nargin");
    auto res = polydiv(args[0], args[1], ctx.engine->resource());
    outs[0] = std::move(res.q);
    if (nargout > 1) outs[1] = std::move(res.r);
}

} // namespace detail

// ════════════════════════════════════════════════════════════════════════
// Curve fitting / evaluation (moved from libs/fit)
// ════════════════════════════════════════════════════════════════════════

Value polyfit(const Value &x, const Value &y, int deg, std::pmr::memory_resource *mr)
{
    const size_t m = x.numel();
    const int np = deg + 1;

    if (static_cast<size_t>(np) > m)
        throw Error("polyfit: not enough data points",
                     0, 0, "polyfit", "", "m:polyfit:tooFewPoints");

    const double *xd = x.doubleData();
    const double *yd = y.doubleData();

    ScratchArena scratch(mr);

    // Vandermonde matrix A[j, i] = x[i]^(deg - j), stored column-major.
    auto A = ScratchVec<double>(m * np, &scratch);
    for (size_t i = 0; i < m; ++i)
        for (int j = 0; j < np; ++j)
            A[j * m + i] = std::pow(xd[i], deg - j);

    // Normal equations: ATA * p = AT * y.
    auto ATA = ScratchVec<double>(static_cast<std::size_t>(np * np), &scratch);
    for (int r = 0; r < np; ++r)
        for (int c = 0; c < np; ++c)
            for (size_t i = 0; i < m; ++i)
                ATA[c * np + r] += A[r * m + i] * A[c * m + i];

    auto ATy = ScratchVec<double>(static_cast<std::size_t>(np), &scratch);
    for (int r = 0; r < np; ++r)
        for (size_t i = 0; i < m; ++i)
            ATy[r] += A[r * m + i] * yd[i];

    // Gaussian elimination with partial pivoting on [ATA | ATy].
    auto aug = ScratchVec<double>(static_cast<std::size_t>(np * (np + 1)), &scratch);
    for (int r = 0; r < np; ++r) {
        for (int c = 0; c < np; ++c)
            aug[r * (np + 1) + c] = ATA[c * np + r];
        aug[r * (np + 1) + np] = ATy[r];
    }

    for (int k = 0; k < np; ++k) {
        int maxRow = k;
        double maxVal = std::abs(aug[k * (np + 1) + k]);
        for (int r = k + 1; r < np; ++r) {
            const double v = std::abs(aug[r * (np + 1) + k]);
            if (v > maxVal) {
                maxVal = v;
                maxRow = r;
            }
        }
        if (maxRow != k) {
            for (int c = 0; c <= np; ++c)
                std::swap(aug[k * (np + 1) + c], aug[maxRow * (np + 1) + c]);
        }

        const double pivot = aug[k * (np + 1) + k];
        if (std::abs(pivot) < 1e-15)
            throw Error("polyfit: singular matrix",
                         0, 0, "polyfit", "", "m:polyfit:singular");

        for (int c = k; c <= np; ++c)
            aug[k * (np + 1) + c] /= pivot;
        for (int r = 0; r < np; ++r) {
            if (r == k)
                continue;
            const double f = aug[r * (np + 1) + k];
            for (int c = k; c <= np; ++c)
                aug[r * (np + 1) + c] -= f * aug[k * (np + 1) + c];
        }
    }

    auto p = Value::matrix(1, np, ValueType::DOUBLE, mr);
    for (int j = 0; j < np; ++j)
        p.doubleDataMut()[j] = aug[j * (np + 1) + np];
    return p;
}

Value polyval(const Value &p, const Value &x, std::pmr::memory_resource *mr)
{
    const double *pd = p.doubleData();
    const size_t np = p.numel();
    const size_t nx = x.numel();
    const double *xd = x.doubleData();

    auto r = createLike(x, ValueType::DOUBLE, mr);
    for (size_t i = 0; i < nx; ++i) {
        double val = pd[0];
        for (size_t j = 1; j < np; ++j)
            val = val * xd[i] + pd[j];
        r.doubleDataMut()[i] = val;
    }
    return r;
}

} // namespace numkit::builtin
