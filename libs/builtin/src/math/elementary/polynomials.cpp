// libs/builtin/src/MStdPoly.cpp

#include <numkit/m/builtin/math/elementary/polynomials.hpp>

#include <numkit/m/core/MEngine.hpp>
#include <numkit/m/core/MTypes.hpp>

#include "MStdHelpers.hpp"
#include "MStdPolyHelpers.hpp"

#include <cmath>
#include <cstring>
#include <tuple>
#include <vector>

namespace numkit::m::builtin {

MValue roots(Allocator &alloc, const MValue &p)
{
    if (p.type() == MType::COMPLEX)
        throw MError("roots: complex coefficient input is not supported",
                     0, 0, "roots", "", "m:roots:complex");
    if (!p.dims().isVector() && !p.isScalar() && !p.isEmpty())
        throw MError("roots: argument must be a vector",
                     0, 0, "roots", "", "m:roots:notVector");

    // Read coefficients as DOUBLE (promote integer/single/logical).
    const std::size_t n = p.numel();
    std::vector<double> coeffs(n);
    for (std::size_t i = 0; i < n; ++i)
        coeffs[i] = p.elemAsDouble(i);

    auto rs = detail::polyRootsDurandKerner(coeffs);
    const std::size_t k = rs.size();

    // If every root is real, return a real column. Otherwise return COMPLEX.
    bool anyComplex = false;
    for (const auto &r : rs)
        if (std::abs(r.imag()) > 1e-12 * (std::abs(r.real()) + 1.0)) {
            anyComplex = true;
            break;
        }

    if (!anyComplex) {
        auto out = MValue::matrix(k, 1, MType::DOUBLE, &alloc);
        for (std::size_t i = 0; i < k; ++i)
            out.doubleDataMut()[i] = rs[i].real();
        return out;
    }
    auto out = MValue::complexMatrix(k, 1, &alloc);
    for (std::size_t i = 0; i < k; ++i)
        out.complexDataMut()[i] = rs[i];
    return out;
}

// ── polyder / polyint ───────────────────────────────────────────────
namespace {

std::vector<double> readPolyAsDouble(const MValue &p, const char *fn)
{
    if (p.type() == MType::COMPLEX)
        throw MError(std::string(fn) + ": complex coefficient input is not supported",
                     0, 0, fn, "", std::string("m:") + fn + ":complex");
    if (!p.dims().isVector() && !p.isScalar() && !p.isEmpty())
        throw MError(std::string(fn) + ": argument must be a vector",
                     0, 0, fn, "", std::string("m:") + fn + ":notVector");
    const std::size_t n = p.numel();
    std::vector<double> v(n);
    for (std::size_t i = 0; i < n; ++i) v[i] = p.elemAsDouble(i);
    return v;
}

// Convolve two real polynomial coefficient vectors (length-N + length-M
// → length-N+M-1).
std::vector<double> polyConv(const std::vector<double> &a, const std::vector<double> &b)
{
    if (a.empty() || b.empty()) return {};
    std::vector<double> r(a.size() + b.size() - 1, 0.0);
    for (std::size_t i = 0; i < a.size(); ++i)
        for (std::size_t j = 0; j < b.size(); ++j)
            r[i + j] += a[i] * b[j];
    return r;
}

// d/dx of a coefficient vector in MATLAB order.
std::vector<double> polyderRaw(const std::vector<double> &p)
{
    if (p.size() <= 1) return {0.0};  // constant → derivative is [0].
    const std::size_t n = p.size() - 1;  // degree
    std::vector<double> r(n);
    for (std::size_t i = 0; i < n; ++i) {
        const double exponent = static_cast<double>(n - i);
        r[i] = p[i] * exponent;
    }
    return r;
}

MValue rowFromVec(Allocator &alloc, const std::vector<double> &v)
{
    auto out = MValue::matrix(1, v.size(), MType::DOUBLE, &alloc);
    if (!v.empty())
        std::memcpy(out.doubleDataMut(), v.data(), v.size() * sizeof(double));
    return out;
}

// Trim trailing zeros that arise from a-b cancellation in polyder(b,a).
void trimLeadingZeros(std::vector<double> &v)
{
    std::size_t lo = 0;
    while (lo + 1 < v.size() && v[lo] == 0.0) ++lo;
    if (lo > 0) v.erase(v.begin(), v.begin() + lo);
}

} // namespace

MValue polyder(Allocator &alloc, const MValue &p)
{
    auto pv = readPolyAsDouble(p, "polyder");
    return rowFromVec(alloc, polyderRaw(pv));
}

std::tuple<MValue, MValue>
polyder(Allocator &alloc, const MValue &b, const MValue &a)
{
    auto bv = readPolyAsDouble(b, "polyder");
    auto av = readPolyAsDouble(a, "polyder");
    auto bp = polyderRaw(bv);
    auto ap = polyderRaw(av);
    // num = a * b' - b * a'
    auto t1 = polyConv(av, bp);
    auto t2 = polyConv(bv, ap);
    // Align lengths (pad with leading zeros so subtraction is safe).
    if (t1.size() < t2.size()) t1.insert(t1.begin(), t2.size() - t1.size(), 0.0);
    if (t2.size() < t1.size()) t2.insert(t2.begin(), t1.size() - t2.size(), 0.0);
    std::vector<double> num(t1.size());
    for (std::size_t i = 0; i < t1.size(); ++i) num[i] = t1[i] - t2[i];
    trimLeadingZeros(num);
    auto den = polyConv(av, av);
    trimLeadingZeros(den);
    return std::make_tuple(rowFromVec(alloc, num), rowFromVec(alloc, den));
}

// Read a vector input of (real or COMPLEX) numbers as a Complex vector.
namespace {

std::vector<detail::Complex> readVecAsComplex(const MValue &v, const char *fn)
{
    if (!v.dims().isVector() && !v.isScalar() && !v.isEmpty())
        throw MError(std::string(fn) + ": argument must be a vector",
                     0, 0, fn, "", std::string("m:") + fn + ":notVector");
    const std::size_t n = v.numel();
    std::vector<detail::Complex> r(n);
    if (v.type() == MType::COMPLEX) {
        const auto *p = v.complexData();
        for (std::size_t i = 0; i < n; ++i) r[i] = p[i];
    } else {
        for (std::size_t i = 0; i < n; ++i)
            r[i] = detail::Complex(v.elemAsDouble(i), 0.0);
    }
    return r;
}

MValue complexColFromVec(Allocator &alloc, const std::vector<detail::Complex> &v)
{
    auto out = MValue::complexMatrix(v.size(), 1, &alloc);
    for (std::size_t i = 0; i < v.size(); ++i)
        out.complexDataMut()[i] = v[i];
    return out;
}

MValue realColIfFlat(Allocator &alloc, const std::vector<detail::Complex> &v)
{
    bool anyComplex = false;
    for (const auto &c : v)
        if (std::abs(c.imag()) > 1e-12 * (std::abs(c.real()) + 1.0)) {
            anyComplex = true;
            break;
        }
    if (!anyComplex) {
        auto out = MValue::matrix(v.size(), 1, MType::DOUBLE, &alloc);
        for (std::size_t i = 0; i < v.size(); ++i)
            out.doubleDataMut()[i] = v[i].real();
        return out;
    }
    return complexColFromVec(alloc, v);
}

} // namespace

MValue polyint(Allocator &alloc, const MValue &p, double k)
{
    auto pv = readPolyAsDouble(p, "polyint");
    if (pv.empty()) {
        // ∫ 0 dx = k.
        auto out = MValue::matrix(1, 1, MType::DOUBLE, &alloc);
        out.doubleDataMut()[0] = k;
        return out;
    }
    const std::size_t n = pv.size();
    std::vector<double> r(n + 1);
    for (std::size_t i = 0; i < n; ++i) {
        const double exponent = static_cast<double>(n - i);
        r[i] = pv[i] / exponent;
    }
    r[n] = k;
    return rowFromVec(alloc, r);
}

// ── tf2zp / zp2tf ───────────────────────────────────────────────────
std::tuple<MValue, MValue, MValue>
tf2zp(Allocator &alloc, const MValue &b, const MValue &a)
{
    auto bv = readPolyAsDouble(b, "tf2zp");
    auto av = readPolyAsDouble(a, "tf2zp");
    if (av.empty() || av[0] == 0.0)
        throw MError("tf2zp: leading denominator coefficient must be non-zero",
                     0, 0, "tf2zp", "", "m:tf2zp:badDen");
    if (bv.empty()) {
        // Numerator = 0 → no zeros, gain 0.
        auto z = MValue::matrix(0, 1, MType::DOUBLE, &alloc);
        auto p = realColIfFlat(alloc, detail::polyRootsDurandKerner(av));
        auto k = MValue::scalar(0.0, &alloc);
        return std::make_tuple(std::move(z), std::move(p), std::move(k));
    }
    auto zRoots = detail::polyRootsDurandKerner(bv);
    auto pRoots = detail::polyRootsDurandKerner(av);
    const double k = bv[0] / av[0];

    return std::make_tuple(realColIfFlat(alloc, zRoots),
                           realColIfFlat(alloc, pRoots),
                           MValue::scalar(k, &alloc));
}

std::tuple<MValue, MValue>
zp2tf(Allocator &alloc, const MValue &z, const MValue &p, double k)
{
    auto zv = readVecAsComplex(z, "zp2tf");
    auto pv = readVecAsComplex(p, "zp2tf");
    auto bRaw = detail::polyExpandFromRoots(zv);
    auto aRaw = detail::polyExpandFromRoots(pv);
    for (auto &v : bRaw) v *= k;
    return std::make_tuple(rowFromVec(alloc, bRaw),
                           rowFromVec(alloc, aRaw));
}

namespace detail {

void roots_reg(Span<const MValue> args, size_t /*nargout*/, Span<MValue> outs, CallContext &ctx)
{
    if (args.empty())
        throw MError("roots: requires 1 argument",
                     0, 0, "roots", "", "m:roots:nargin");
    outs[0] = roots(ctx.engine->allocator(), args[0]);
}

void polyder_reg(Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx)
{
    if (args.empty())
        throw MError("polyder: requires at least 1 argument",
                     0, 0, "polyder", "", "m:polyder:nargin");
    Allocator &alloc = ctx.engine->allocator();
    if (args.size() == 1) {
        outs[0] = polyder(alloc, args[0]);
        return;
    }
    auto [num, den] = polyder(alloc, args[0], args[1]);
    outs[0] = std::move(num);
    if (nargout > 1) outs[1] = std::move(den);
}

void polyint_reg(Span<const MValue> args, size_t /*nargout*/, Span<MValue> outs, CallContext &ctx)
{
    if (args.empty())
        throw MError("polyint: requires at least 1 argument",
                     0, 0, "polyint", "", "m:polyint:nargin");
    Allocator &alloc = ctx.engine->allocator();
    double k = 0.0;
    if (args.size() >= 2) k = args[1].toScalar();
    outs[0] = polyint(alloc, args[0], k);
}

void tf2zp_reg(Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw MError("tf2zp: requires 2 arguments (b, a)",
                     0, 0, "tf2zp", "", "m:tf2zp:nargin");
    auto [zr, pr, kr] = tf2zp(ctx.engine->allocator(), args[0], args[1]);
    outs[0] = std::move(zr);
    if (nargout > 1) outs[1] = std::move(pr);
    if (nargout > 2) outs[2] = std::move(kr);
}

void zp2tf_reg(Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw MError("zp2tf: requires 3 arguments (z, p, k)",
                     0, 0, "zp2tf", "", "m:zp2tf:nargin");
    auto [bv, av] = zp2tf(ctx.engine->allocator(), args[0], args[1],
                          args[2].toScalar());
    outs[0] = std::move(bv);
    if (nargout > 1) outs[1] = std::move(av);
}

void polyfit_reg(Span<const MValue> args, size_t /*nargout*/, Span<MValue> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw MError("polyfit: requires 3 arguments",
                     0, 0, "polyfit", "", "m:polyfit:nargin");
    outs[0] = polyfit(ctx.engine->allocator(),
                      args[0], args[1],
                      static_cast<int>(args[2].toScalar()));
}

void polyval_reg(Span<const MValue> args, size_t /*nargout*/, Span<MValue> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw MError("polyval: requires 2 arguments",
                     0, 0, "polyval", "", "m:polyval:nargin");
    outs[0] = polyval(ctx.engine->allocator(), args[0], args[1]);
}

} // namespace detail

// ════════════════════════════════════════════════════════════════════════
// Curve fitting / evaluation (moved from libs/fit)
// ════════════════════════════════════════════════════════════════════════

MValue polyfit(Allocator &alloc, const MValue &x, const MValue &y, int deg)
{
    const size_t m = x.numel();
    const int np = deg + 1;

    if (static_cast<size_t>(np) > m)
        throw MError("polyfit: not enough data points",
                     0, 0, "polyfit", "", "m:polyfit:tooFewPoints");

    const double *xd = x.doubleData();
    const double *yd = y.doubleData();

    // Vandermonde matrix A[j, i] = x[i]^(deg - j), stored column-major.
    std::vector<double> A(m * np);
    for (size_t i = 0; i < m; ++i)
        for (int j = 0; j < np; ++j)
            A[j * m + i] = std::pow(xd[i], deg - j);

    // Normal equations: ATA * p = AT * y.
    std::vector<double> ATA(np * np, 0.0);
    for (int r = 0; r < np; ++r)
        for (int c = 0; c < np; ++c)
            for (size_t i = 0; i < m; ++i)
                ATA[c * np + r] += A[r * m + i] * A[c * m + i];

    std::vector<double> ATy(np, 0.0);
    for (int r = 0; r < np; ++r)
        for (size_t i = 0; i < m; ++i)
            ATy[r] += A[r * m + i] * yd[i];

    // Gaussian elimination with partial pivoting on [ATA | ATy].
    std::vector<double> aug(np * (np + 1));
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
            throw MError("polyfit: singular matrix",
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

    auto p = MValue::matrix(1, np, MType::DOUBLE, &alloc);
    for (int j = 0; j < np; ++j)
        p.doubleDataMut()[j] = aug[j * (np + 1) + np];
    return p;
}

MValue polyval(Allocator &alloc, const MValue &p, const MValue &x)
{
    const double *pd = p.doubleData();
    const size_t np = p.numel();
    const size_t nx = x.numel();
    const double *xd = x.doubleData();

    auto r = createLike(x, MType::DOUBLE, &alloc);
    for (size_t i = 0; i < nx; ++i) {
        double val = pd[0];
        for (size_t j = 1; j < np; ++j)
            val = val * xd[i] + pd[j];
        r.doubleDataMut()[i] = val;
    }
    return r;
}

} // namespace numkit::m::builtin
