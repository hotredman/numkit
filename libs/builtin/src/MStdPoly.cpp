// libs/builtin/src/MStdPoly.cpp

#include <numkit/m/builtin/MStdPoly.hpp>

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

} // namespace detail

} // namespace numkit::m::builtin
