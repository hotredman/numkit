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
                     0, 0, "roots", "", "numkit:roots:complex");
    if (!p.dims().isVector() && !p.isScalar() && !p.isEmpty())
        throw Error("roots: argument must be a vector",
                     0, 0, "roots", "", "numkit:roots:notVector");

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
                     0, 0, fn, "", std::string("numkit:") + fn + ":complex");
    if (!p.dims().isVector() && !p.isScalar() && !p.isEmpty())
        throw Error(std::string(fn) + ": argument must be a vector",
                     0, 0, fn, "", std::string("numkit:") + fn + ":notVector");
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
                     0, 0, fn, "", std::string("numkit:") + fn + ":notVector");
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
                     0, 0, "tf2zp", "", "numkit:tf2zp:badDen");
    // MATLAB strips leading zeros from the numerator before forming the
    // ZPK gain: b = [0 0 0 0 1] is the polynomial "1", so the gain is 1,
    // NOT b(1) = 0. The root finder (polyRootsDurandKerner) already ignores
    // leading zeros for the zeros, so a naive k = bv[0]/av[0] gave a bogus
    // gain of 0 whenever b was zero-padded (e.g. zp2tf output fed into
    // lp2lp/lp2bp via tf2zpk).
    size_t bnz = 0;
    while (bnz < bv.size() && bv[bnz] == 0.0) ++bnz;
    if (bv.empty() || bnz == bv.size()) {
        // Numerator is empty or all zeros → no zeros, gain 0.
        auto z = Value::matrix(0, 1, ValueType::DOUBLE, mr);
        auto pRoots = detail::polyRootsDurandKerner(&scratch, av.data(), av.size());
        auto p = realColIfFlat(pRoots.data(), pRoots.size(), mr);
        auto k = Value::scalar(0.0, mr);
        return std::make_tuple(std::move(z), std::move(p), std::move(k));
    }
    auto zRoots = detail::polyRootsDurandKerner(&scratch, bv.data(), bv.size());
    auto pRoots = detail::polyRootsDurandKerner(&scratch, av.data(), av.size());
    const double k = bv[bnz] / av[0];

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
    // MATLAB returns b with the SAME length as a: when there are fewer
    // zeros than poles, the numerator is left-padded with zeros (the
    // surplus high-order coefficients are 0). E.g. one zero, two poles ->
    // b = [0 b0 b1], not [b0 b1].
    if (bRaw.size() < aRaw.size()) {
        const size_t off = aRaw.size() - bRaw.size();
        ScratchVec<double> bPad(aRaw.size(), &scratch);
        for (size_t i = 0; i < aRaw.size(); ++i) bPad[i] = 0.0;
        for (size_t i = 0; i < bRaw.size(); ++i) bPad[off + i] = bRaw[i];
        return std::make_tuple(rowFromVec(bPad.data(), bPad.size(), mr),
                               rowFromVec(aRaw.data(), aRaw.size(), mr));
    }
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
                     0, 0, "polyvalm", "", "numkit:polyvalm:notSquare");
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
                     0, 0, "polydiv", "", "numkit:polydiv:emptyA");
    const size_t na = a.numel();
    const size_t nb = b.numel();
    // Strip leading zeros from a (matches MATLAB behaviour).
    size_t aOff = 0;
    while (aOff + 1 < na && a.doubleData()[aOff] == 0.0) ++aOff;
    const size_t aEff = na - aOff;
    if (aEff == 0 || a.doubleData()[aOff] == 0.0)
        throw Error("polydiv: divisor is zero", 0, 0, "polydiv", "",
                     "numkit:polydiv:zeroDivisor");

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
                     0, 0, "padecoef", "", "numkit:padecoef:badN");

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
                     0, 0, "roots", "", "numkit:roots:nargin");
    outs[0] = roots(args[0], ctx.engine->resource());
}

void polyder_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("polyder: requires at least 1 argument",
                     0, 0, "polyder", "", "numkit:polyder:nargin");
    std::pmr::memory_resource *mr = ctx.engine->resource();
    if (args.size() == 1) {
        outs[0] = polyder(args[0], mr);
        return;
    }
    // Two polynomials. MATLAB polyder(a,b):
    //   * ONE output  -> derivative of the PRODUCT a*b  ( = polyder(conv(a,b)) )
    //   * TWO outputs -> quotient rule [num,den] = d/dx(a/b)
    // The single-output form was incorrectly returning the quotient NUMERATOR
    // (conv(a',b)-conv(a,b')) instead of the product derivative
    // (conv(a',b)+conv(a,b')). bugs/builtin/polyder-product.md.
    if (nargout <= 1) {
        ScratchArena scratch(mr);
        auto av = readPolyAsDouble(args[0], "polyder", &scratch);
        auto bv = readPolyAsDouble(args[1], "polyder", &scratch);
        auto prod = polyConv(av.data(), av.size(), bv.data(), bv.size(), &scratch);
        auto deriv = polyderRaw(prod.data(), prod.size(), &scratch);
        trimLeadingZeros(deriv);
        outs[0] = rowFromVec(deriv.data(), deriv.size(), mr);
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
                     0, 0, "polyint", "", "numkit:polyint:nargin");
    std::pmr::memory_resource *mr = ctx.engine->resource();
    double k = 0.0;
    if (args.size() >= 2) k = args[1].toScalar();
    outs[0] = polyint(args[0], k, mr);
}

void tf2zp_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("tf2zp: requires 2 arguments (b, a)",
                     0, 0, "tf2zp", "", "numkit:tf2zp:nargin");
    auto [zr, pr, kr] = tf2zp(args[0], args[1], ctx.engine->resource());
    outs[0] = std::move(zr);
    if (nargout > 1) outs[1] = std::move(pr);
    if (nargout > 2) outs[2] = std::move(kr);
}

void zp2tf_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("zp2tf: requires 3 arguments (z, p, k)",
                     0, 0, "zp2tf", "", "numkit:zp2tf:nargin");
    auto [bv, av] = zp2tf(args[0], args[1], args[2].toScalar(), ctx.engine->resource());
    outs[0] = std::move(bv);
    if (nargout > 1) outs[1] = std::move(av);
}

namespace {

// Least-squares polynomial fit of degree `deg` to (xx, y) over `m` points
// (xx already centered/scaled if requested). Writes coefficients
// (np = deg+1, highest power first) into pOut, the upper-triangular
// Cholesky factor R of V'V (np×np, col-major R[row + col*np]) into Rout,
// the residual norm into normr, and the degrees of freedom into df.
void polyfitCore(const double *xx, const double *yd, size_t m, int deg,
                 double *pOut, double *Rout, double &normr, double &df,
                 ScratchArena &scratch)
{
    const int np = deg + 1;
    auto V = ScratchVec<double>(m * static_cast<size_t>(np), &scratch);
    for (size_t i = 0; i < m; ++i)
        for (int j = 0; j < np; ++j)
            V[static_cast<size_t>(j) * m + i] = std::pow(xx[i], deg - j);

    auto VtV = ScratchVec<double>(static_cast<size_t>(np) * np, &scratch);  // col-major
    for (int r = 0; r < np; ++r)
        for (int c = 0; c < np; ++c) {
            double s = 0.0;
            for (size_t i = 0; i < m; ++i)
                s += V[static_cast<size_t>(r) * m + i] * V[static_cast<size_t>(c) * m + i];
            VtV[static_cast<size_t>(c) * np + r] = s;
        }
    auto Vty = ScratchVec<double>(static_cast<size_t>(np), &scratch);
    for (int r = 0; r < np; ++r) {
        double s = 0.0;
        for (size_t i = 0; i < m; ++i) s += V[static_cast<size_t>(r) * m + i] * yd[i];
        Vty[r] = s;
    }

    // Solve VtV p = Vty (Gaussian elimination with partial pivoting).
    auto aug = ScratchVec<double>(static_cast<size_t>(np) * (np + 1), &scratch);
    for (int r = 0; r < np; ++r) {
        for (int c = 0; c < np; ++c) aug[r * (np + 1) + c] = VtV[static_cast<size_t>(c) * np + r];
        aug[r * (np + 1) + np] = Vty[r];
    }
    for (int k = 0; k < np; ++k) {
        int maxRow = k;
        double maxVal = std::abs(aug[k * (np + 1) + k]);
        for (int r = k + 1; r < np; ++r) {
            const double v = std::abs(aug[r * (np + 1) + k]);
            if (v > maxVal) { maxVal = v; maxRow = r; }
        }
        if (maxRow != k)
            for (int c = 0; c <= np; ++c)
                std::swap(aug[k * (np + 1) + c], aug[maxRow * (np + 1) + c]);
        const double pivot = aug[k * (np + 1) + k];
        if (std::abs(pivot) < 1e-15)
            throw Error("polyfit: singular matrix",
                         0, 0, "polyfit", "", "numkit:polyfit:singular");
        for (int c = k; c <= np; ++c) aug[k * (np + 1) + c] /= pivot;
        for (int r = 0; r < np; ++r) {
            if (r == k) continue;
            const double f = aug[r * (np + 1) + k];
            for (int c = k; c <= np; ++c) aug[r * (np + 1) + c] -= f * aug[k * (np + 1) + c];
        }
    }
    for (int j = 0; j < np; ++j) pOut[j] = aug[j * (np + 1) + np];

    // R = chol(VtV): upper-triangular, R[row + col*np], R'R = VtV. Sign
    // convention differs from MATLAB's qr-R but R'R is identical, so the
    // polyval delta estimate (which uses only V·inv(R), i.e. inv(V'V))
    // matches MATLAB bit-for-bit.
    for (int i = 0; i < np * np; ++i) Rout[i] = 0.0;
    for (int j = 0; j < np; ++j) {
        double d = VtV[static_cast<size_t>(j) * np + j];
        for (int k = 0; k < j; ++k)
            d -= Rout[k + j * np] * Rout[k + j * np];
        d = (d > 0.0) ? std::sqrt(d) : 0.0;
        Rout[j + j * np] = d;
        for (int i = j + 1; i < np; ++i) {
            double s = VtV[static_cast<size_t>(i) * np + j];  // symmetric
            for (int k = 0; k < j; ++k)
                s -= Rout[k + j * np] * Rout[k + i * np];
            Rout[j + i * np] = (d != 0.0) ? s / d : 0.0;
        }
    }

    // Residual norm and degrees of freedom.
    double ss = 0.0;
    for (size_t i = 0; i < m; ++i) {
        double yi = 0.0;
        for (int j = 0; j < np; ++j) yi += V[static_cast<size_t>(j) * m + i] * pOut[j];
        const double e = yd[i] - yi;
        ss += e * e;
    }
    normr = std::sqrt(ss);
    df = (m > static_cast<size_t>(np)) ? static_cast<double>(m - np) : 0.0;
}

} // namespace

void polyfit_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("polyfit: requires 3 arguments",
                     0, 0, "polyfit", "", "numkit:polyfit:nargin");
    auto *mr = ctx.engine->resource();
    const Value &x = args[0], &y = args[1];
    const int deg = static_cast<int>(args[2].toScalar());

    if (nargout <= 1) {                       // coefficients only — fast path
        outs[0] = polyfit(x, y, deg, mr);
        return;
    }

    const size_t m = x.numel();
    const int np = deg + 1;
    if (static_cast<size_t>(np) > m)
        throw Error("polyfit: not enough data points",
                     0, 0, "polyfit", "", "numkit:polyfit:tooFewPoints");

    ScratchArena scratch(mr);
    const bool center = (nargout >= 3);       // centering is gated on mu (3rd output)
    const double *xraw = x.doubleData();
    const double *yd = y.doubleData();
    double mu0 = 0.0, mu1 = 1.0;
    auto xx = ScratchVec<double>(m, &scratch);
    if (center) {
        double s = 0.0;
        for (size_t i = 0; i < m; ++i) s += xraw[i];
        mu0 = s / static_cast<double>(m);
        double v = 0.0;
        for (size_t i = 0; i < m; ++i) { const double d = xraw[i] - mu0; v += d * d; }
        mu1 = (m > 1) ? std::sqrt(v / static_cast<double>(m - 1)) : 0.0;
        if (mu1 == 0.0) mu1 = 1.0;            // constant x → avoid /0
        for (size_t i = 0; i < m; ++i) xx[i] = (xraw[i] - mu0) / mu1;
    } else {
        for (size_t i = 0; i < m; ++i) xx[i] = xraw[i];
    }

    auto pbuf = ScratchVec<double>(static_cast<size_t>(np), &scratch);
    auto Rbuf = ScratchVec<double>(static_cast<size_t>(np) * np, &scratch);
    double normr = 0.0, df = 0.0;
    polyfitCore(xx.data(), yd, m, deg, pbuf.data(), Rbuf.data(), normr, df, scratch);

    auto p = Value::matrix(1, np, ValueType::DOUBLE, mr);
    for (int j = 0; j < np; ++j) p.doubleDataMut()[j] = pbuf[j];
    outs[0] = std::move(p);

    Value S = Value::structure(mr);
    auto Rmat = Value::matrix(np, np, ValueType::DOUBLE, mr);
    std::memcpy(Rmat.doubleDataMut(), Rbuf.data(),
                static_cast<size_t>(np) * np * sizeof(double));
    S.setFieldAll("R", Rmat);
    S.setFieldAll("df", Value::scalar(df, mr));
    S.setFieldAll("normr", Value::scalar(normr, mr));
    // S.rsquared = 1 - (normr / norm(y - mean(y)))^2  (MATLAB R2025b's 4th
    // S field; the data is already to hand, it was just never exposed).
    double yMean = 0.0;
    for (size_t i = 0; i < m; ++i) yMean += yd[i];
    yMean /= static_cast<double>(m);
    double sstot = 0.0;
    for (size_t i = 0; i < m; ++i) { const double d = yd[i] - yMean; sstot += d * d; }
    S.setFieldAll("rsquared", Value::scalar(1.0 - (normr * normr) / sstot, mr));
    outs[1] = std::move(S);

    if (nargout >= 3) {
        auto mu = Value::matrix(2, 1, ValueType::DOUBLE, mr);
        mu.doubleDataMut()[0] = mu0;
        mu.doubleDataMut()[1] = mu1;
        outs[2] = std::move(mu);
    }
}

void polyval_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("polyval: requires 2 arguments",
                     0, 0, "polyval", "", "numkit:polyval:nargin");
    auto *mr = ctx.engine->resource();
    const Value &p = args[0];
    const Value &x = args[1];

    const bool hasS  = (args.size() >= 3 && args[2].type() == ValueType::STRUCT);
    const bool hasMu = (args.size() >= 4 && !args[3].isEmpty());

    // Centre x by mu = [mean; std] when supplied.
    Value xUsed = x;
    if (hasMu) {
        const double *md = args[3].doubleData();
        const double mu0 = md[0];
        const double mu1 = (md[1] != 0.0) ? md[1] : 1.0;
        xUsed = createLike(x, ValueType::DOUBLE, mr);
        const double *xd = x.doubleData();
        double *xc = xUsed.doubleDataMut();
        for (size_t i = 0; i < x.numel(); ++i) xc[i] = (xd[i] - mu0) / mu1;
    }

    outs[0] = polyval(p, xUsed, mr);
    if (nargout <= 1) return;

    if (!hasS)
        throw Error("polyval: the error-estimate output requires the S structure",
                     0, 0, "polyval", "", "numkit:polyval:noStruct");

    // delta = normr/sqrt(df) · sqrt(1 + rowSum((V·inv(R)).^2)).
    const Value &S = args[2];
    const double normr = S.field("normr").toScalar();
    const double df    = S.field("df").toScalar();
    const Value &Rm    = S.field("R");
    const double *R    = Rm.doubleData();
    const size_t np    = p.numel();
    const int    deg   = static_cast<int>(np) - 1;

    auto delta = createLike(xUsed, ValueType::DOUBLE, mr);
    double *dd = delta.doubleDataMut();
    const double *xd = xUsed.doubleData();
    const size_t nx = xUsed.numel();
    const double scale = (df > 0.0) ? normr / std::sqrt(df) : 0.0;

    ScratchArena scratch(mr);
    auto vrow = ScratchVec<double>(np, &scratch);
    auto erow = ScratchVec<double>(np, &scratch);
    for (size_t i = 0; i < nx; ++i) {
        for (size_t j = 0; j < np; ++j)
            vrow[j] = std::pow(xd[i], static_cast<double>(deg - static_cast<int>(j)));
        // Solve e·R = v with R upper-triangular: e[c] = (v[c] - Σ_{k<c} e[k]·R[k][c]) / R[c][c].
        for (size_t c = 0; c < np; ++c) {
            double s = vrow[c];
            for (size_t k = 0; k < c; ++k) s -= erow[k] * R[k + c * np];
            const double rcc = R[c + c * np];
            erow[c] = (rcc != 0.0) ? s / rcc : 0.0;
        }
        double ss = 0.0;
        for (size_t j = 0; j < np; ++j) ss += erow[j] * erow[j];
        dd[i] = scale * std::sqrt(1.0 + ss);
    }
    outs[1] = std::move(delta);
}

void poly_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("poly: requires 1 argument",
                     0, 0, "poly", "", "numkit:poly:nargin");
    // Dispatch (matches MATLAB behavior):
    //   square matrix (n×n, n>1) -> characteristic polynomial via Souriau-Faddeev
    //   anything else (vector of roots) -> expand (λ - r_1)(λ - r_2)...
    const auto &A = args[0];
    auto *mr = ctx.engine->resource();
    if (A.dims().ndim() == 2
        && A.dims().dim(0) == A.dims().dim(1)
        && A.dims().dim(0) > 1) {
        outs[0] = poly_of_matrix(A, mr);
    } else {
        outs[0] = poly(A, mr);
    }
}

void polyvalm_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("polyvalm: requires (p, A)",
                     0, 0, "polyvalm", "", "numkit:polyvalm:nargin");
    outs[0] = polyvalm(args[0], args[1], ctx.engine->resource());
}

void padecoef_reg(Span<const Value> args, size_t nargout, Span<Value> outs,
                  CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("padecoef: requires 2 arguments (T, N)",
                     0, 0, "padecoef", "", "numkit:padecoef:nargin");
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
                     0, 0, "polydiv", "", "numkit:polydiv:nargin");
    auto res = polydiv(args[0], args[1], ctx.engine->resource());
    outs[0] = std::move(res.q);
    if (nargout > 1) outs[1] = std::move(res.r);
}

} // namespace detail

// ── residue / residuez — partial fraction expansion ─────────────────
//
// Forward form `[r, p, k] = residue(b, a)` only. The inverse form
// `[b, a] = residue(r, p, k)` is a documented v1 gap.
//
// Algorithm (distinct poles):
//   1. polydiv b/a  → quotient k (direct term), remainder b_rem (deg < deg a)
//   2. roots(a)     → poles p (complex)
//   3. Detect repeated poles within `1e-6 · max(1,|p|)` — throw if any.
//   4. a'(s) = polyder(a)
//   5. For each pole p_i, r_i = b_rem(p_i) / a'(p_i)  (Horner, complex)
//
// residuez is the same machinery in z-domain. We use the substitution
// from MATLAB's `residuez.m`: reverse coefficient order to convert
// between B(z^-1) / A(z^-1) (z-domain convention) and the standard
// polynomial form, then apply the same residue formula. Direct term k
// in z-domain is the polynomial in z^-1 — we return it in the same
// MATLAB convention.

namespace {

// Horner's method in complex domain. Coefficients in descending order.
Complex hornerCx(const double *coeffs, std::size_t n, Complex x)
{
    if (n == 0) return Complex(0, 0);
    Complex acc(coeffs[0], 0.0);
    for (std::size_t i = 1; i < n; ++i)
        acc = acc * x + Complex(coeffs[i], 0.0);
    return acc;
}

// Strip leading zeros and verify result is non-empty.
std::vector<double> readPolyStripped(const Value &p, const char *fn)
{
    if (p.type() == ValueType::COMPLEX)
        throw Error(std::string(fn) + ": complex coefficients not supported in v1",
                     0, 0, fn, "", std::string("numkit:") + fn + ":complex");
    if (!p.isEmpty() && !p.dims().isVector() && !p.isScalar())
        throw Error(std::string(fn) + ": arguments must be vectors",
                     0, 0, fn, "", std::string("numkit:") + fn + ":notVector");
    std::vector<double> v(p.numel());
    for (std::size_t i = 0; i < v.size(); ++i) v[i] = p.elemAsDouble(i);
    std::size_t lo = 0;
    while (lo + 1 < v.size() && v[lo] == 0.0) ++lo;
    return std::vector<double>(v.begin() + lo, v.end());
}

// Returns true if any pair of poles is within tol·max(1,|p|).
bool hasRepeatedPoles(const std::vector<Complex> &p)
{
    for (std::size_t i = 0; i < p.size(); ++i)
        for (std::size_t j = i + 1; j < p.size(); ++j) {
            const double scale = std::max(1.0, std::max(std::abs(p[i]), std::abs(p[j])));
            if (std::abs(p[i] - p[j]) < 1e-6 * scale)
                return true;
        }
    return false;
}

// Pack a complex column. If all entries are real (within tol), return
// a real column instead.
Value packComplexOrReal(const std::vector<Complex> &v, std::pmr::memory_resource *mr)
{
    bool anyComplex = false;
    for (const auto &z : v)
        if (std::abs(z.imag()) > 1e-10 * (std::abs(z.real()) + 1.0)) {
            anyComplex = true; break;
        }
    if (!anyComplex) {
        auto out = Value::matrix(v.size(), 1, ValueType::DOUBLE, mr);
        for (std::size_t i = 0; i < v.size(); ++i)
            out.doubleDataMut()[i] = v[i].real();
        return out;
    }
    auto out = Value::complexMatrix(v.size(), 1, mr);
    for (std::size_t i = 0; i < v.size(); ++i)
        out.complexDataMut()[i] = v[i];
    return out;
}

Value packDirectTerm(const std::vector<double> &k, std::pmr::memory_resource *mr)
{
    if (k.empty() || (k.size() == 1 && k[0] == 0.0))
        return Value::matrix(0, 0, ValueType::DOUBLE, mr);
    auto out = Value::matrix(1, k.size(), ValueType::DOUBLE, mr);
    std::memcpy(out.doubleDataMut(), k.data(), k.size() * sizeof(double));
    return out;
}

// Evaluate B(z) at z = p, where B is in z^-1 ascending coefficient order
// (MATLAB z-domain convention: b[0] + b[1]·z^-1 + ... + b[n]·z^-n).
Complex evalZPolyCx(const double *b, std::size_t n, Complex p)
{
    if (n == 0) return Complex(0, 0);
    Complex acc(0, 0);
    const Complex inv = Complex(1, 0) / p;
    Complex term(1, 0);   // (1/p)^k
    for (std::size_t k = 0; k < n; ++k) {
        acc += Complex(b[k], 0) * term;
        term *= inv;
    }
    return acc;
}

ResidueResult residueS(const Value &b, const Value &a,
                       std::pmr::memory_resource *mr)
{
    const char *fn = "residue";
    auto A = readPolyStripped(a, fn);
    auto B = readPolyStripped(b, fn);
    if (A.empty() || A[0] == 0.0)
        throw Error("residue: denominator must be non-zero",
                     0, 0, fn, "", "numkit:residue:zeroDenom");

    // Polynomial division: B = K·A + R   (deg R < deg A).
    std::vector<double> K;
    std::vector<double> R = B;
    if (B.size() >= A.size()) {
        const std::size_t qLen = B.size() - A.size() + 1;
        K.assign(qLen, 0.0);
        std::vector<double> bb = B;
        const double aLead = A[0];
        for (std::size_t i = 0; i < qLen; ++i) {
            const double coef = bb[i] / aLead;
            K[i] = coef;
            for (std::size_t j = 0; j < A.size(); ++j)
                bb[i + j] -= coef * A[j];
        }
        std::size_t rOff = qLen;
        while (rOff < bb.size() && bb[rOff] == 0.0) ++rOff;
        R.assign(bb.begin() + rOff, bb.end());
    }

    // Roots of A → s-domain poles.
    ScratchArena scratch(mr);
    auto rs = detail::polyRootsDurandKerner(&scratch, A.data(), A.size());
    std::vector<Complex> poles(rs.begin(), rs.end());

    if (hasRepeatedPoles(poles))
        throw Error("residue: repeated-pole case not yet supported "
                    "(v1 distinct-poles only — see KNOWN GAP)",
                     0, 0, fn, "", "numkit:residue:repeatedPole");

    // Derivative coefficients (descending).
    std::vector<double> Aprime;
    if (A.size() > 1) {
        const std::size_t n = A.size() - 1;
        Aprime.assign(n, 0.0);
        for (std::size_t i = 0; i < n; ++i)
            Aprime[i] = A[i] * static_cast<double>(n - i);
    }

    // Residues via standard cover-up: r_i = R(p_i) / A'(p_i).
    std::vector<Complex> residues(poles.size(), Complex(0, 0));
    if (!R.empty()) {
        for (std::size_t i = 0; i < poles.size(); ++i) {
            const Complex num = hornerCx(R.data(), R.size(), poles[i]);
            const Complex den = hornerCx(Aprime.data(), Aprime.size(), poles[i]);
            if (std::abs(den) < 1e-300)
                throw Error("residue: derivative vanishes at a pole — "
                            "likely repeated pole undetected by tolerance",
                             0, 0, fn, "", "numkit:residue:denomZero");
            residues[i] = num / den;
        }
    }

    return {
        packComplexOrReal(residues, mr),
        packComplexOrReal(poles, mr),
        packDirectTerm(K, mr),
    };
}

ResidueResult residueZ(const Value &b, const Value &a,
                       std::pmr::memory_resource *mr)
{
    const char *fn = "residuez";
    // z-domain convention: a[0] + a[1]·z^-1 + ... + a[m]·z^-m. The
    // leading scalar is a[0] (z^0 coefficient), not a[m].
    auto av = readPolyStripped(a, fn);
    auto bv = readPolyStripped(b, fn);
    if (av.empty() || av[0] == 0.0)
        throw Error("residuez: denominator a[0] must be non-zero",
                     0, 0, fn, "", "numkit:residuez:zeroDenom");

    // Normalise: divide A, B by a[0] so a[0] becomes 1. Residues come
    // out in MATLAB's residuez convention without further scaling.
    const double a0 = av[0];
    std::vector<double> A = av, B = bv;
    for (auto &x : A) x /= a0;
    for (auto &x : B) x /= a0;

    // z-domain poles = roots(A). For a polynomial in z^-1 ascending,
    // multiplying through by z^m gives a polynomial in z whose roots
    // ARE the z-domain poles. polyRootsDurandKerner treats its input
    // as MATLAB-descending; for our purpose both readings have the
    // same roots (a + b·z^-1 mult by z → a·z + b, roots match s-form).
    ScratchArena scratch(mr);
    auto rs = detail::polyRootsDurandKerner(&scratch, A.data(), A.size());
    std::vector<Complex> poles(rs.begin(), rs.end());

    if (hasRepeatedPoles(poles))
        throw Error("residuez: repeated-pole case not yet supported "
                    "(v1 distinct-poles only — see KNOWN GAP)",
                     0, 0, fn, "", "numkit:residuez:repeatedPole");

    const std::size_t m = poles.size();

    // Direct term: only the proper case (numel(B) <= numel(A)) is
    // supported in v1 — k is empty. The general polynomial-in-z^-1
    // quotient for improper TFs is a documented gap.
    std::vector<double> K;
    if (B.size() > A.size())
        throw Error("residuez: improper transfer functions "
                    "(numel(b) > numel(a)) not yet supported — direct "
                    "term in z^-1 polynomial form is a v1 KNOWN GAP",
                     0, 0, fn, "", "numkit:residuez:improperTF");

    // Residue formula for distinct z-poles (Oppenheim & Schafer 3e §3.4):
    //
    //   r_i = B(p_i) · p_i^(m-1) / prod_{j ≠ i} (p_i - p_j)
    //
    // where B(p_i) is evaluated treating B as a polynomial in z^-1.
    std::vector<Complex> residues(m, Complex(0, 0));
    for (std::size_t i = 0; i < m; ++i) {
        const Complex Bpi = evalZPolyCx(B.data(), B.size(), poles[i]);
        Complex pPow(1, 0);
        for (std::size_t k = 0; k + 1 < m; ++k) pPow *= poles[i];
        Complex denom(1, 0);
        for (std::size_t j = 0; j < m; ++j) {
            if (j == i) continue;
            denom *= (poles[i] - poles[j]);
        }
        if (std::abs(denom) < 1e-300)
            throw Error("residuez: denominator vanishes at a pole — "
                        "likely repeated pole undetected by tolerance",
                         0, 0, fn, "", "numkit:residuez:denomZero");
        residues[i] = Bpi * pPow / denom;
    }

    return {
        packComplexOrReal(residues, mr),
        packComplexOrReal(poles, mr),
        packDirectTerm(K, mr),
    };
}

} // namespace

ResidueResult residue(const Value &b, const Value &a,
                      std::pmr::memory_resource *mr)
{
    return residueS(b, a, mr);
}

ResidueResult residuez(const Value &b, const Value &a,
                       std::pmr::memory_resource *mr)
{
    return residueZ(b, a, mr);
}

namespace detail {

void residue_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("residue: requires (b, a)",
                     0, 0, "residue", "", "numkit:residue:nargin");
    auto res = residue(args[0], args[1], ctx.engine->resource());
    outs[0] = std::move(res.r);
    if (nargout > 1) outs[1] = std::move(res.p);
    if (nargout > 2) outs[2] = std::move(res.k);
}

void residuez_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("residuez: requires (b, a)",
                     0, 0, "residuez", "", "numkit:residuez:nargin");
    auto res = residuez(args[0], args[1], ctx.engine->resource());
    outs[0] = std::move(res.r);
    if (nargout > 1) outs[1] = std::move(res.p);
    if (nargout > 2) outs[2] = std::move(res.k);
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
                     0, 0, "polyfit", "", "numkit:polyfit:tooFewPoints");

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
                         0, 0, "polyfit", "", "numkit:polyfit:singular");

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
