// libs/signal/src/filter_implementation/conversions.cpp
//
// zp2sos / tf2sos — convert filter representations to SOS form. The
// cascade applicator sosfilt lives in digital_filtering/sosfilt.cpp.

#include <numkit/signal/filter_implementation/conversions.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/scratch.hpp>
#include <numkit/core/types.hpp>

#include "../dsp_helpers.hpp"
#include "poly_helpers.hpp"

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory_resource>
#include <utility>

namespace numkit::signal {

using numkit::Complex;

namespace {

using numkit::builtin::detail::polyRootsDurandKerner;

constexpr double kImagThresh = 1e-10;  // |Im(r)| below this → treat as real

inline bool isReal(Complex r)
{
    return std::abs(r.imag()) <= kImagThresh;
}

struct RootPair { Complex a, b; bool isPair; };

bool popPair(ScratchVec<Complex> &pool, RootPair &out)
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
            throw Error("zp2sos: complex root has no conjugate in input",
                         0, 0, "zp2sos", "", "numkit:zp2sos:noConj");
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

bool popClosestZeroPair(ScratchVec<Complex> &zeros, Complex target,
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
            throw Error("zp2sos: complex zero has no conjugate in input",
                         0, 0, "zp2sos", "", "numkit:zp2sos:noConj");
        out = { pick, zeros[mateIdx], true };
        const size_t hi = std::max(pickIdx, mateIdx);
        const size_t lo = std::min(pickIdx, mateIdx);
        zeros.erase(zeros.begin() + hi);
        zeros.erase(zeros.begin() + lo);
    }
    return true;
}

ScratchVec<Complex> readComplexVec(const Value &v, const char *fn, std::pmr::memory_resource *mr)
{
    if (v.isEmpty()) return ScratchVec<Complex>(mr);
    ScratchVec<Complex> out(mr);
    out.reserve(v.numel());
    if (v.type() == ValueType::COMPLEX) {
        const Complex *p = v.complexData();
        for (size_t i = 0; i < v.numel(); ++i) out.push_back(p[i]);
    } else if (v.type() == ValueType::DOUBLE) {
        const double *p = v.doubleData();
        for (size_t i = 0; i < v.numel(); ++i) out.push_back(Complex(p[i], 0.0));
    } else {
        throw Error(std::string(fn) + ": zeros/poles must be DOUBLE or COMPLEX",
                     0, 0, fn, "", std::string("numkit:") + fn + ":type");
    }
    return out;
}

Value buildSosMatrix(const double *b1s, const double *b2s, const double *a1s, const double *a2s, const std::uint8_t *noZeros, bool surplusAtOrigin, std::size_t L, double leadingGain, std::pmr::memory_resource *mr)
{
    auto sos = Value::matrix(L, 6, ValueType::DOUBLE, mr);
    double *p = sos.doubleDataMut();
    for (size_t r = 0; r < L; ++r) {
        const double scale = (r == 0) ? leadingGain : 1.0;
        if (noZeros && noZeros[r] && surplusAtOrigin) {
            // zp2sos: a section carrying no finite zeros gets MATLAB's
            // surplus zeros at the ORIGIN, i.e. numerator = scale*z^-2 ->
            // [0 0 scale]. Reconstructing the transfer function then keeps
            // the z^-2 delay (degree-deficient numerator) MATLAB produces.
            p[0 * L + r] = 0.0;
            p[1 * L + r] = 0.0;
            p[2 * L + r] = scale;
        } else {
            // Normal section, or tf2sos (surplusAtOrigin=false): keep the
            // numerator left-aligned [scale  scale*b1  scale*b2] so the
            // section product reproduces the ORIGINAL b polynomial exactly
            // (b2s/b1s are 0 for a no-zero section -> [scale 0 0]).
            p[0 * L + r] = scale;             // b0
            p[1 * L + r] = scale * b1s[r];    // b1
            p[2 * L + r] = scale * b2s[r];    // b2
        }
        p[3 * L + r] = 1.0;               // a0
        p[4 * L + r] = a1s[r];            // a1
        p[5 * L + r] = a2s[r];            // a2
    }
    return sos;
}

inline ScratchVec<double> coeffsAsVector(const Value &v, std::pmr::memory_resource *mr)
{
    if (v.type() != ValueType::DOUBLE || !v.dims().isVector())
        throw Error("tf2sos: b/a must be DOUBLE row/column vectors",
                     0, 0, "tf2sos", "", "numkit:tf2sos:type");
    const size_t n = v.numel();
    ScratchVec<double> out(n, mr);
    const double *p = v.doubleData();
    for (size_t i = 0; i < n; ++i) out[i] = p[i];
    return out;
}

} // namespace

std::tuple<Value, double>
zp2sosWithGain(const Value &zerosV, const Value &polesV, double gain, std::pmr::memory_resource *mr, bool surplusAtOrigin)
{
    if (polesV.isEmpty())
        throw Error("zp2sos: at least one pole is required",
                     0, 0, "zp2sos", "", "numkit:zp2sos:noPoles");

    ScratchArena scratch(mr);
    auto zeros = readComplexVec(zerosV, "zp2sos", &scratch);
    auto poles = readComplexVec(polesV, "zp2sos", &scratch);

    const size_t L = (poles.size() + 1) / 2;

    auto b1s = ScratchVec<double>(L, &scratch);
    auto b2s = ScratchVec<double>(L, &scratch);
    auto a1s = ScratchVec<double>(L, &scratch);
    auto a2s = ScratchVec<double>(L, &scratch);
    auto noZeros = ScratchVec<std::uint8_t>(L, &scratch);
    for (size_t s = 0; s < L; ++s) {
        RootPair polePair;
        if (!popPair(poles, polePair))
            throw Error("zp2sos: internal — ran out of poles",
                         0, 0, "zp2sos", "", "numkit:zp2sos:internal");
        const auto polQ = pairToQuad(polePair);
        a1s[s] = polQ.b1;
        a2s[s] = polQ.b2;

        if (!zeros.empty()) {
            RootPair zeroPair;
            (void) popClosestZeroPair(zeros, pairCentroid(polePair), zeroPair);
            const auto zerQ = pairToQuad(zeroPair);
            b1s[s] = zerQ.b1;
            b2s[s] = zerQ.b2;
            noZeros[s] = 0;
        } else {
            b1s[s] = 0.0;
            b2s[s] = 0.0;
            noZeros[s] = 1;   // surplus zeros -> placed at origin ([0 0 g])
        }
    }
    if (!zeros.empty())
        throw Error("zp2sos: more zeros than poles is not supported",
                     0, 0, "zp2sos", "", "numkit:zp2sos:moreZeros");

    // MATLAB's default 'up' section ordering: sections sorted by ASCENDING
    // pole radius — poles nearest the ORIGIN first, nearest the unit circle
    // last. popPair pairs poles largest-magnitude-first (correct pairing),
    // so the section arrays are currently in DESCENDING radius; reverse them
    // to match MATLAB. The overall gain is applied by the caller to row 0,
    // which after the reversal is the origin-nearest section — exactly where
    // MATLAB folds it in. ('down' ordering is a non-default option: deferred.)
    std::reverse(b1s.begin(), b1s.end());
    std::reverse(b2s.begin(), b2s.end());
    std::reverse(a1s.begin(), a1s.end());
    std::reverse(a2s.begin(), a2s.end());
    std::reverse(noZeros.begin(), noZeros.end());

    return std::make_tuple(buildSosMatrix(b1s.data(), b2s.data(), a1s.data(), a2s.data(), noZeros.data(), surplusAtOrigin, L, /*leadingGain=*/1.0, mr),
                           gain);
}

Value zp2sos(const Value &zerosV, const Value &polesV, double gain, std::pmr::memory_resource *mr)
{
    auto [sos, g] = zp2sosWithGain(zerosV, polesV, gain, mr);
    const size_t L = sos.dims().rows();
    double *p = sos.doubleDataMut();
    p[0 * L + 0] *= g;
    p[1 * L + 0] *= g;
    p[2 * L + 0] *= g;
    return sos;
}

std::tuple<Value, double>
tf2sosWithGain(const Value &b, const Value &a, std::pmr::memory_resource *mr)
{
    ScratchArena scratch(mr);
    auto bv = coeffsAsVector(b, &scratch);
    auto av = coeffsAsVector(a, &scratch);
    if (av.empty() || av[0] == 0.0)
        throw Error("tf2sos: a(1) must be nonzero",
                     0, 0, "tf2sos", "", "numkit:tf2sos:zeroLead");
    if (bv.empty())
        throw Error("tf2sos: b is empty",
                     0, 0, "tf2sos", "", "numkit:tf2sos:emptyB");

    const double gain = bv[0] / av[0];
    auto zeros = polyRootsDurandKerner(&scratch, bv.data(), bv.size());
    auto poles = polyRootsDurandKerner(&scratch, av.data(), av.size());

    auto toCplxVec = [&](const ScratchVec<Complex> &v) -> Value {
        if (v.empty())
            return Value::matrix(0, 0, ValueType::COMPLEX, mr);
        auto r = Value::matrix(v.size(), 1, ValueType::COMPLEX, mr);
        Complex *p = r.complexDataMut();
        for (size_t i = 0; i < v.size(); ++i) p[i] = v[i];
        return r;
    };
    // tf2sos must reproduce the ORIGINAL b polynomial: surplus zeros stay
    // left-aligned at infinity ([g 0 0]), not pushed to the origin.
    return zp2sosWithGain(toCplxVec(zeros), toCplxVec(poles), gain, mr, /*surplusAtOrigin=*/false);
}

Value tf2sos(const Value &b, const Value &a, std::pmr::memory_resource *mr)
{
    auto [sos, g] = tf2sosWithGain(b, a, mr);
    const size_t L = sos.dims().rows();
    double *p = sos.doubleDataMut();
    p[0 * L + 0] *= g;
    p[1 * L + 0] *= g;
    p[2 * L + 0] *= g;
    return sos;
}

namespace detail {

void zp2sos_reg(Span<const Value> args, size_t nargout,
                Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2 || args.size() > 3)
        throw Error("zp2sos: requires (z, p[, k])",
                     0, 0, "zp2sos", "", "numkit:zp2sos:nargin");
    const double gain = (args.size() >= 3 && !args[2].isEmpty())
                            ? args[2].toScalar()
                            : 1.0;
    auto *mr = ctx.engine->resource();
    if (nargout >= 2) {
        auto [sos, g] = zp2sosWithGain(args[0], args[1], gain, mr);
        outs[0] = std::move(sos);
        outs[1] = Value::scalar(g, mr);
    } else {
        outs[0] = zp2sos(args[0], args[1], gain, mr);
    }
}

void tf2sos_reg(Span<const Value> args, size_t nargout,
                Span<Value> outs, CallContext &ctx)
{
    if (args.size() != 2)
        throw Error("tf2sos: requires (b, a)",
                     0, 0, "tf2sos", "", "numkit:tf2sos:nargin");
    auto *mr = ctx.engine->resource();
    if (nargout >= 2) {
        auto [sos, g] = tf2sosWithGain(args[0], args[1], mr);
        outs[0] = std::move(sos);
        outs[1] = Value::scalar(g, mr);
    } else {
        outs[0] = tf2sos(args[0], args[1], mr);
    }
}

} // namespace detail

} // namespace numkit::signal
