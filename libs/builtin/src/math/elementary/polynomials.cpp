// libs/builtin/src/math/elementary/polynomials.cpp

#include <numkit/builtin/math/elementary/polynomials.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/scratch.hpp>
#include <numkit/core/types.hpp>

#include "helpers.hpp"
#include "poly_helpers.hpp"

#include <cmath>
#include <cstring>
#include <memory_resource>
#include <tuple>

namespace numkit::builtin {

Value roots(std::pmr::memory_resource *mr, const Value &p)
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

ScratchVec<double> readPolyAsDouble(std::pmr::memory_resource *mr,
                                    const Value &p, const char *fn)
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
ScratchVec<double> polyConv(std::pmr::memory_resource *mr,
                            const double *a, std::size_t na,
                            const double *b, std::size_t nb)
{
    if (na == 0 || nb == 0) return ScratchVec<double>(mr);
    ScratchVec<double> r(na + nb - 1, mr);
    for (std::size_t i = 0; i < na; ++i)
        for (std::size_t j = 0; j < nb; ++j)
            r[i + j] += a[i] * b[j];
    return r;
}

// d/dx of a coefficient vector in MATLAB order.
ScratchVec<double> polyderRaw(std::pmr::memory_resource *mr,
                              const double *p, std::size_t pn)
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

Value rowFromVec(std::pmr::memory_resource *mr, const double *v, std::size_t n)
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

Value polyder(std::pmr::memory_resource *mr, const Value &p)
{
    ScratchArena scratch(mr);
    auto pv = readPolyAsDouble(&scratch, p, "polyder");
    auto deriv = polyderRaw(&scratch, pv.data(), pv.size());
    return rowFromVec(mr, deriv.data(), deriv.size());
}

std::tuple<Value, Value>
polyder(std::pmr::memory_resource *mr, const Value &b, const Value &a)
{
    ScratchArena scratch(mr);
    auto bv = readPolyAsDouble(&scratch, b, "polyder");
    auto av = readPolyAsDouble(&scratch, a, "polyder");
    auto bp = polyderRaw(&scratch, bv.data(), bv.size());
    auto ap = polyderRaw(&scratch, av.data(), av.size());
    // num = a * b' - b * a'
    auto t1 = polyConv(&scratch, av.data(), av.size(), bp.data(), bp.size());
    auto t2 = polyConv(&scratch, bv.data(), bv.size(), ap.data(), ap.size());
    // Align lengths (pad with leading zeros so subtraction is safe).
    if (t1.size() < t2.size()) t1.insert(t1.begin(), t2.size() - t1.size(), 0.0);
    if (t2.size() < t1.size()) t2.insert(t2.begin(), t1.size() - t2.size(), 0.0);
    ScratchVec<double> num(t1.size(), &scratch);
    for (std::size_t i = 0; i < t1.size(); ++i) num[i] = t1[i] - t2[i];
    trimLeadingZeros(num);
    auto den = polyConv(&scratch, av.data(), av.size(), av.data(), av.size());
    trimLeadingZeros(den);
    return std::make_tuple(rowFromVec(mr, num.data(), num.size()),
                           rowFromVec(mr, den.data(), den.size()));
}

// Read a vector input of (real or COMPLEX) numbers as a Complex vector.
namespace {

ScratchVec<detail::Complex> readVecAsComplex(std::pmr::memory_resource *mr,
                                             const Value &v, const char *fn)
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

Value complexColFromVec(std::pmr::memory_resource *mr, const detail::Complex *v, std::size_t n)
{
    auto out = Value::complexMatrix(n, 1, mr);
    for (std::size_t i = 0; i < n; ++i)
        out.complexDataMut()[i] = v[i];
    return out;
}

Value realColIfFlat(std::pmr::memory_resource *mr, const detail::Complex *v, std::size_t n)
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
    return complexColFromVec(mr, v, n);
}

} // namespace

Value polyint(std::pmr::memory_resource *mr, const Value &p, double k)
{
    ScratchArena scratch(mr);
    auto pv = readPolyAsDouble(&scratch, p, "polyint");
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
    return rowFromVec(mr, r.data(), r.size());
}

// ── tf2zp / zp2tf ───────────────────────────────────────────────────
std::tuple<Value, Value, Value>
tf2zp(std::pmr::memory_resource *mr, const Value &b, const Value &a)
{
    ScratchArena scratch(mr);
    auto bv = readPolyAsDouble(&scratch, b, "tf2zp");
    auto av = readPolyAsDouble(&scratch, a, "tf2zp");
    if (av.empty() || av[0] == 0.0)
        throw Error("tf2zp: leading denominator coefficient must be non-zero",
                     0, 0, "tf2zp", "", "m:tf2zp:badDen");
    if (bv.empty()) {
        // Numerator = 0 → no zeros, gain 0.
        auto z = Value::matrix(0, 1, ValueType::DOUBLE, mr);
        auto pRoots = detail::polyRootsDurandKerner(&scratch, av.data(), av.size());
        auto p = realColIfFlat(mr, pRoots.data(), pRoots.size());
        auto k = Value::scalar(0.0, mr);
        return std::make_tuple(std::move(z), std::move(p), std::move(k));
    }
    auto zRoots = detail::polyRootsDurandKerner(&scratch, bv.data(), bv.size());
    auto pRoots = detail::polyRootsDurandKerner(&scratch, av.data(), av.size());
    const double k = bv[0] / av[0];

    return std::make_tuple(realColIfFlat(mr, zRoots.data(), zRoots.size()),
                           realColIfFlat(mr, pRoots.data(), pRoots.size()),
                           Value::scalar(k, mr));
}

std::tuple<Value, Value>
zp2tf(std::pmr::memory_resource *mr, const Value &z, const Value &p, double k)
{
    ScratchArena scratch(mr);
    auto zv = readVecAsComplex(&scratch, z, "zp2tf");
    auto pv = readVecAsComplex(&scratch, p, "zp2tf");
    auto bRaw = detail::polyExpandFromRoots(&scratch, zv.data(), zv.size());
    auto aRaw = detail::polyExpandFromRoots(&scratch, pv.data(), pv.size());
    for (auto &v : bRaw) v *= k;
    return std::make_tuple(rowFromVec(mr, bRaw.data(), bRaw.size()),
                           rowFromVec(mr, aRaw.data(), aRaw.size()));
}

namespace detail {

void roots_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("roots: requires 1 argument",
                     0, 0, "roots", "", "m:roots:nargin");
    outs[0] = roots(ctx.engine->resource(), args[0]);
}

void polyder_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("polyder: requires at least 1 argument",
                     0, 0, "polyder", "", "m:polyder:nargin");
    std::pmr::memory_resource *mr = ctx.engine->resource();
    if (args.size() == 1) {
        outs[0] = polyder(mr, args[0]);
        return;
    }
    auto [num, den] = polyder(mr, args[0], args[1]);
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
    outs[0] = polyint(mr, args[0], k);
}

void tf2zp_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("tf2zp: requires 2 arguments (b, a)",
                     0, 0, "tf2zp", "", "m:tf2zp:nargin");
    auto [zr, pr, kr] = tf2zp(ctx.engine->resource(), args[0], args[1]);
    outs[0] = std::move(zr);
    if (nargout > 1) outs[1] = std::move(pr);
    if (nargout > 2) outs[2] = std::move(kr);
}

void zp2tf_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("zp2tf: requires 3 arguments (z, p, k)",
                     0, 0, "zp2tf", "", "m:zp2tf:nargin");
    auto [bv, av] = zp2tf(ctx.engine->resource(), args[0], args[1],
                          args[2].toScalar());
    outs[0] = std::move(bv);
    if (nargout > 1) outs[1] = std::move(av);
}

void polyfit_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("polyfit: requires 3 arguments",
                     0, 0, "polyfit", "", "m:polyfit:nargin");
    outs[0] = polyfit(ctx.engine->resource(),
                      args[0], args[1],
                      static_cast<int>(args[2].toScalar()));
}

void polyval_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("polyval: requires 2 arguments",
                     0, 0, "polyval", "", "m:polyval:nargin");
    outs[0] = polyval(ctx.engine->resource(), args[0], args[1]);
}

} // namespace detail

// ════════════════════════════════════════════════════════════════════════
// Curve fitting / evaluation (moved from libs/fit)
// ════════════════════════════════════════════════════════════════════════

Value polyfit(std::pmr::memory_resource *mr, const Value &x, const Value &y, int deg)
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

Value polyval(std::pmr::memory_resource *mr, const Value &p, const Value &x)
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
