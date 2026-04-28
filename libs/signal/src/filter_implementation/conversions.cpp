// libs/signal/src/filter_implementation/conversions.cpp
//
// zp2sos / tf2sos — convert filter representations to SOS form. The
// cascade applicator sosfilt lives in digital_filtering/sosfilt.cpp.

#include <numkit/m/signal/filter_implementation/conversions.hpp>

#include <numkit/m/core/MEngine.hpp>
#include <numkit/m/core/MTypes.hpp>

#include "MStdHelpers.hpp"
#include "MStdPolyHelpers.hpp"

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstddef>
#include <limits>
#include <utility>
#include <vector>

namespace numkit::m::signal {

using numkit::m::Complex;

namespace {

// Polynomial root finder lifted into shared MStdPolyHelpers.hpp.
using numkit::m::builtin::detail::polyRootsDurandKerner;
inline std::vector<Complex> rootsDurandKerner(const std::vector<double> &c)
{
    return polyRootsDurandKerner(c);
}

constexpr double kImagThresh = 1e-10;  // |Im(r)| below this → treat as real

inline bool isReal(Complex r)
{
    return std::abs(r.imag()) <= kImagThresh;
}

struct RootPair { Complex a, b; bool isPair; };

bool popPair(std::vector<Complex> &pool, RootPair &out)
{
    if (pool.empty()) return false;
    size_t pickIdx = 0;
    double bestMag = std::abs(pool[0]);
    for (size_t i = 1; i < pool.size(); ++i) {
        const double m = std::abs(pool[i]);
        if (m > bestMag) { bestMag = m; pickIdx = i; }
    }
    const Complex pick = pool[pickIdx];
    if (isReal(pick)) {
        size_t mateIdx = pool.size();
        double bestDist = std::numeric_limits<double>::infinity();
        for (size_t i = 0; i < pool.size(); ++i) {
            if (i == pickIdx) continue;
            if (!isReal(pool[i])) continue;
            const double d = std::abs(pool[i] - pick);
            if (d < bestDist) { bestDist = d; mateIdx = i; }
        }
        if (mateIdx < pool.size()) {
            out = { Complex(pick.real(), 0.0), Complex(pool[mateIdx].real(), 0.0), true };
            const size_t hi = std::max(pickIdx, mateIdx);
            const size_t lo = std::min(pickIdx, mateIdx);
            pool.erase(pool.begin() + hi);
            pool.erase(pool.begin() + lo);
        } else {
            out = { Complex(pick.real(), 0.0), Complex(0.0, 0.0), false };
            pool.erase(pool.begin() + pickIdx);
        }
    } else {
        const Complex conjP = std::conj(pick);
        size_t mateIdx = pool.size();
        double bestDist = std::numeric_limits<double>::infinity();
        for (size_t i = 0; i < pool.size(); ++i) {
            if (i == pickIdx) continue;
            const double d = std::abs(pool[i] - conjP);
            if (d < bestDist) { bestDist = d; mateIdx = i; }
        }
        if (mateIdx >= pool.size())
            throw MError("zp2sos: complex root has no conjugate in input",
                         0, 0, "zp2sos", "", "m:zp2sos:noConj");
        out = { pick, pool[mateIdx], true };
        const size_t hi = std::max(pickIdx, mateIdx);
        const size_t lo = std::min(pickIdx, mateIdx);
        pool.erase(pool.begin() + hi);
        pool.erase(pool.begin() + lo);
    }
    return true;
}

struct QuadCoeffs { double b1, b2; };

QuadCoeffs pairToQuad(const RootPair &p)
{
    if (!p.isPair) {
        return { -p.a.real(), 0.0 };
    }
    if (!isReal(p.a) || !isReal(p.b)) {
        return { -2.0 * p.a.real(),
                 p.a.real() * p.a.real() + p.a.imag() * p.a.imag() };
    }
    return { -(p.a.real() + p.b.real()),
             p.a.real() * p.b.real() };
}

inline Complex pairCentroid(const RootPair &p)
{
    if (!p.isPair) return p.a;
    return (p.a + p.b) * 0.5;
}

bool popClosestZeroPair(std::vector<Complex> &zeros, Complex target,
                        RootPair &out)
{
    if (zeros.empty()) return false;
    size_t pickIdx = 0;
    double bestDist = std::abs(zeros[0] - target);
    for (size_t i = 1; i < zeros.size(); ++i) {
        const double d = std::abs(zeros[i] - target);
        if (d < bestDist) { bestDist = d; pickIdx = i; }
    }
    const Complex pick = zeros[pickIdx];
    if (isReal(pick)) {
        size_t mateIdx = zeros.size();
        double bestMate = std::numeric_limits<double>::infinity();
        for (size_t i = 0; i < zeros.size(); ++i) {
            if (i == pickIdx) continue;
            if (!isReal(zeros[i])) continue;
            const double d = std::abs(zeros[i] - target);
            if (d < bestMate) { bestMate = d; mateIdx = i; }
        }
        if (mateIdx < zeros.size()) {
            out = { Complex(pick.real(), 0.0),
                    Complex(zeros[mateIdx].real(), 0.0), true };
            const size_t hi = std::max(pickIdx, mateIdx);
            const size_t lo = std::min(pickIdx, mateIdx);
            zeros.erase(zeros.begin() + hi);
            zeros.erase(zeros.begin() + lo);
        } else {
            out = { Complex(pick.real(), 0.0), Complex(0.0, 0.0), false };
            zeros.erase(zeros.begin() + pickIdx);
        }
    } else {
        const Complex conjP = std::conj(pick);
        size_t mateIdx = zeros.size();
        double bestDist2 = std::numeric_limits<double>::infinity();
        for (size_t i = 0; i < zeros.size(); ++i) {
            if (i == pickIdx) continue;
            const double d = std::abs(zeros[i] - conjP);
            if (d < bestDist2) { bestDist2 = d; mateIdx = i; }
        }
        if (mateIdx >= zeros.size())
            throw MError("zp2sos: complex zero has no conjugate in input",
                         0, 0, "zp2sos", "", "m:zp2sos:noConj");
        out = { pick, zeros[mateIdx], true };
        const size_t hi = std::max(pickIdx, mateIdx);
        const size_t lo = std::min(pickIdx, mateIdx);
        zeros.erase(zeros.begin() + hi);
        zeros.erase(zeros.begin() + lo);
    }
    return true;
}

std::vector<Complex> readComplexVec(const MValue &v, const char *fn)
{
    if (v.isEmpty()) return {};
    std::vector<Complex> out;
    out.reserve(v.numel());
    if (v.type() == MType::COMPLEX) {
        const Complex *p = v.complexData();
        for (size_t i = 0; i < v.numel(); ++i) out.push_back(p[i]);
    } else if (v.type() == MType::DOUBLE) {
        const double *p = v.doubleData();
        for (size_t i = 0; i < v.numel(); ++i) out.push_back(Complex(p[i], 0.0));
    } else {
        throw MError(std::string(fn) + ": zeros/poles must be DOUBLE or COMPLEX",
                     0, 0, fn, "", std::string("m:") + fn + ":type");
    }
    return out;
}

MValue buildSosMatrix(Allocator &alloc,
                      const std::vector<double> &b1s,
                      const std::vector<double> &b2s,
                      const std::vector<double> &a1s,
                      const std::vector<double> &a2s,
                      double leadingGain)
{
    const size_t L = a1s.size();
    auto sos = MValue::matrix(L, 6, MType::DOUBLE, &alloc);
    double *p = sos.doubleDataMut();
    for (size_t r = 0; r < L; ++r) {
        const double scale = (r == 0) ? leadingGain : 1.0;
        p[0 * L + r] = scale;             // b0
        p[1 * L + r] = scale * b1s[r];    // b1
        p[2 * L + r] = scale * b2s[r];    // b2
        p[3 * L + r] = 1.0;               // a0
        p[4 * L + r] = a1s[r];            // a1
        p[5 * L + r] = a2s[r];            // a2
    }
    return sos;
}

inline std::vector<double> coeffsAsVector(const MValue &v)
{
    if (v.type() != MType::DOUBLE || !v.dims().isVector())
        throw MError("tf2sos: b/a must be DOUBLE row/column vectors",
                     0, 0, "tf2sos", "", "m:tf2sos:type");
    const size_t n = v.numel();
    std::vector<double> out(n);
    const double *p = v.doubleData();
    for (size_t i = 0; i < n; ++i) out[i] = p[i];
    return out;
}

} // namespace

std::tuple<MValue, double>
zp2sosWithGain(Allocator &alloc, const MValue &zerosV, const MValue &polesV, double gain)
{
    if (polesV.isEmpty())
        throw MError("zp2sos: at least one pole is required",
                     0, 0, "zp2sos", "", "m:zp2sos:noPoles");

    auto zeros = readComplexVec(zerosV, "zp2sos");
    auto poles = readComplexVec(polesV, "zp2sos");

    const size_t L = (poles.size() + 1) / 2;

    std::vector<double> b1s(L), b2s(L), a1s(L), a2s(L);
    for (size_t s = 0; s < L; ++s) {
        RootPair polePair;
        if (!popPair(poles, polePair))
            throw MError("zp2sos: internal — ran out of poles",
                         0, 0, "zp2sos", "", "m:zp2sos:internal");
        const auto polQ = pairToQuad(polePair);
        a1s[s] = polQ.b1;
        a2s[s] = polQ.b2;

        if (!zeros.empty()) {
            RootPair zeroPair;
            (void) popClosestZeroPair(zeros, pairCentroid(polePair), zeroPair);
            const auto zerQ = pairToQuad(zeroPair);
            b1s[s] = zerQ.b1;
            b2s[s] = zerQ.b2;
        } else {
            b1s[s] = 0.0;
            b2s[s] = 0.0;
        }
    }
    if (!zeros.empty())
        throw MError("zp2sos: more zeros than poles is not supported",
                     0, 0, "zp2sos", "", "m:zp2sos:moreZeros");

    return std::make_tuple(buildSosMatrix(alloc, b1s, b2s, a1s, a2s, /*leadingGain=*/1.0),
                           gain);
}

MValue zp2sos(Allocator &alloc, const MValue &zerosV, const MValue &polesV, double gain)
{
    auto [sos, g] = zp2sosWithGain(alloc, zerosV, polesV, gain);
    const size_t L = sos.dims().rows();
    double *p = sos.doubleDataMut();
    p[0 * L + 0] *= g;
    p[1 * L + 0] *= g;
    p[2 * L + 0] *= g;
    return sos;
}

std::tuple<MValue, double>
tf2sosWithGain(Allocator &alloc, const MValue &b, const MValue &a)
{
    auto bv = coeffsAsVector(b);
    auto av = coeffsAsVector(a);
    if (av.empty() || av[0] == 0.0)
        throw MError("tf2sos: a(1) must be nonzero",
                     0, 0, "tf2sos", "", "m:tf2sos:zeroLead");
    if (bv.empty())
        throw MError("tf2sos: b is empty",
                     0, 0, "tf2sos", "", "m:tf2sos:emptyB");

    const double gain = bv[0] / av[0];
    auto zeros = rootsDurandKerner(bv);
    auto poles = rootsDurandKerner(av);

    auto toCplxVec = [&](const std::vector<Complex> &v) -> MValue {
        if (v.empty())
            return MValue::matrix(0, 0, MType::COMPLEX, &alloc);
        auto r = MValue::matrix(v.size(), 1, MType::COMPLEX, &alloc);
        Complex *p = r.complexDataMut();
        for (size_t i = 0; i < v.size(); ++i) p[i] = v[i];
        return r;
    };
    return zp2sosWithGain(alloc, toCplxVec(zeros), toCplxVec(poles), gain);
}

MValue tf2sos(Allocator &alloc, const MValue &b, const MValue &a)
{
    auto [sos, g] = tf2sosWithGain(alloc, b, a);
    const size_t L = sos.dims().rows();
    double *p = sos.doubleDataMut();
    p[0 * L + 0] *= g;
    p[1 * L + 0] *= g;
    p[2 * L + 0] *= g;
    return sos;
}

namespace detail {

void zp2sos_reg(Span<const MValue> args, size_t nargout,
                Span<MValue> outs, CallContext &ctx)
{
    if (args.size() < 2 || args.size() > 3)
        throw MError("zp2sos: requires (z, p[, k])",
                     0, 0, "zp2sos", "", "m:zp2sos:nargin");
    const double gain = (args.size() >= 3 && !args[2].isEmpty())
                            ? args[2].toScalar()
                            : 1.0;
    auto &alloc = ctx.engine->allocator();
    if (nargout >= 2) {
        auto [sos, g] = zp2sosWithGain(alloc, args[0], args[1], gain);
        outs[0] = std::move(sos);
        outs[1] = MValue::scalar(g, &alloc);
    } else {
        outs[0] = zp2sos(alloc, args[0], args[1], gain);
    }
}

void tf2sos_reg(Span<const MValue> args, size_t nargout,
                Span<MValue> outs, CallContext &ctx)
{
    if (args.size() != 2)
        throw MError("tf2sos: requires (b, a)",
                     0, 0, "tf2sos", "", "m:tf2sos:nargin");
    auto &alloc = ctx.engine->allocator();
    if (nargout >= 2) {
        auto [sos, g] = tf2sosWithGain(alloc, args[0], args[1]);
        outs[0] = std::move(sos);
        outs[1] = MValue::scalar(g, &alloc);
    } else {
        outs[0] = tf2sos(alloc, args[0], args[1]);
    }
}

} // namespace detail

} // namespace numkit::m::signal
