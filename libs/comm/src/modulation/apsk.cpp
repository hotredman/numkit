// libs/comm/src/modulation/apsk.cpp
//
// Amplitude-Phase Shift Keying (multi-ring constellation):
//   y = apskmod(x, M, radii [, phaseoffset [, mapping]])
//   z = apskdemod(y, M, radii [, phaseoffset [, mapping]])
//
// `M`     : per-ring symbol counts (vector); total constellation size N = sum(M).
// `radii` : per-ring radius (same length as M).
// `phaseoffset` : per-ring phase offset (defaults to pi ./ M).
// `mapping` : optional length-N permutation of indices [0..N-1].
//             Default identity (`0:N-1`). Gray defaults are deferred --
//             MATLAB's per-ring Gray for non-power-of-2 needs more probing.
//
// Constellation point at logical index `idx` (after mapping):
//   ring r is the smallest with idx < cum(r);    k = idx - cum(r-1)
//   point = radii[r] * exp(i * (phaseoffset[r] + 2π·k / M[r]))

#include <numkit/comm/modulation/apsk.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include <cmath>
#include <complex>
#include <limits>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace numkit::comm {

using Cd = std::complex<double>;

namespace {

// Build the natural-order constellation: for each ring r, its M[r]
// points at radii[r] * exp(i * (phaseoffset[r] + 2π·k/M[r])).
std::vector<Cd> buildConstellation(const std::vector<int> &M,
                                   const std::vector<double> &radii,
                                   const std::vector<double> &phaseoffset)
{
    const size_t R = M.size();
    int N = 0;
    for (int m : M) N += m;
    std::vector<Cd> C;
    C.reserve(N);
    for (size_t r = 0; r < R; ++r) {
        const int Mr = M[r];
        const double radr = radii[r];
        const double pho  = phaseoffset[r];
        for (int k = 0; k < Mr; ++k) {
            const double theta = pho + 2.0 * M_PI * k / static_cast<double>(Mr);
            C.emplace_back(radr * std::cos(theta), radr * std::sin(theta));
        }
    }
    return C;
}

void readVecInt(const Value &v, std::vector<int> &out)
{
    const size_t n = v.numel();
    out.resize(n);
    for (size_t i = 0; i < n; ++i)
        out[i] = static_cast<int>(v.elemAsDouble(i));
}

void readVec(const Value &v, std::vector<double> &out)
{
    const size_t n = v.numel();
    out.resize(n);
    for (size_t i = 0; i < n; ++i) out[i] = v.elemAsDouble(i);
}

// `mapping` is a length-N permutation: when the user wants symbol idx
// to map to constellation point `mapping(idx) + 1` (1-based MATLAB)
// or `mapping[idx]` (0-based). We invert it so that
// constellation_index = invMapping[user_index].
std::vector<int> invertMapping(const std::vector<int> &mapping)
{
    const size_t N = mapping.size();
    std::vector<int> inv(N, -1);
    for (size_t i = 0; i < N; ++i) {
        if (mapping[i] < 0 || static_cast<size_t>(mapping[i]) >= N)
            throw Error("apsk: SymbolMapping out of range",
                        0, 0, "apsk", "", "m:apsk:Mapping");
        inv[static_cast<size_t>(mapping[i])] = static_cast<int>(i);
    }
    return inv;
}

} // namespace

Value apskmod(std::pmr::memory_resource *mr, const Value &x,
              const Value &M_v, const Value &radii_v,
              const Value *phaseoffset_v, const Value *mapping_v)
{
    std::vector<int>    M;       readVecInt(M_v, M);
    std::vector<double> radii;   readVec(radii_v, radii);
    if (M.empty() || M.size() != radii.size())
        throw Error("apskmod: M and radii must be vectors of same length",
                    0, 0, "apskmod", "", "m:apskmod:DimMismatch");
    std::vector<double> phaseoffset(M.size());
    if (phaseoffset_v && !phaseoffset_v->isEmpty()) {
        readVec(*phaseoffset_v, phaseoffset);
        if (phaseoffset.size() == 1) {
            // Scalar broadcast.
            const double v = phaseoffset[0];
            phaseoffset.assign(M.size(), v);
        } else if (phaseoffset.size() != M.size()) {
            throw Error("apskmod: phaseoffset length must match M",
                        0, 0, "apskmod", "", "m:apskmod:PhaseLen");
        }
    } else {
        for (size_t r = 0; r < M.size(); ++r)
            phaseoffset[r] = M_PI / static_cast<double>(M[r]);
    }

    int Ntot = 0;
    for (int m : M) Ntot += m;

    std::vector<Cd> C = buildConstellation(M, radii, phaseoffset);

    // Mapping: identity by default; otherwise length-Ntot permutation.
    std::vector<int> invmap;
    if (mapping_v && !mapping_v->isEmpty()) {
        std::vector<int> mapping;
        readVecInt(*mapping_v, mapping);
        if (mapping.size() != static_cast<size_t>(Ntot))
            throw Error("apskmod: SymbolMapping length must equal sum(M)",
                        0, 0, "apskmod", "", "m:apskmod:MappingLen");
        invmap = invertMapping(mapping);
    } else {
        invmap.resize(static_cast<size_t>(Ntot));
        for (int i = 0; i < Ntot; ++i) invmap[i] = i;
    }

    Value out = Value::matrix(x.dims().rows(), x.dims().cols(),
                              ValueType::COMPLEX, mr);
    Cd *o = out.complexDataMut();
    const size_t Nx = x.numel();
    for (size_t i = 0; i < Nx; ++i) {
        const int xi = static_cast<int>(x.elemAsDouble(i));
        if (xi < 0 || xi >= Ntot)
            throw Error("apskmod: input index out of range [0, sum(M)-1]",
                        0, 0, "apskmod", "", "m:apskmod:OutOfRange");
        o[i] = C[static_cast<size_t>(invmap[static_cast<size_t>(xi)])];
    }
    return out;
}

Value apskdemod(std::pmr::memory_resource *mr, const Value &y,
                const Value &M_v, const Value &radii_v,
                const Value *phaseoffset_v, const Value *mapping_v)
{
    std::vector<int>    M;       readVecInt(M_v, M);
    std::vector<double> radii;   readVec(radii_v, radii);
    if (M.empty() || M.size() != radii.size())
        throw Error("apskdemod: M and radii must be vectors of same length",
                    0, 0, "apskdemod", "", "m:apskdemod:DimMismatch");
    std::vector<double> phaseoffset(M.size());
    if (phaseoffset_v && !phaseoffset_v->isEmpty()) {
        readVec(*phaseoffset_v, phaseoffset);
        if (phaseoffset.size() == 1) {
            const double v = phaseoffset[0];
            phaseoffset.assign(M.size(), v);
        } else if (phaseoffset.size() != M.size()) {
            throw Error("apskdemod: phaseoffset length must match M",
                        0, 0, "apskdemod", "", "m:apskdemod:PhaseLen");
        }
    } else {
        for (size_t r = 0; r < M.size(); ++r)
            phaseoffset[r] = M_PI / static_cast<double>(M[r]);
    }

    int Ntot = 0;
    for (int m : M) Ntot += m;
    std::vector<Cd> C = buildConstellation(M, radii, phaseoffset);

    std::vector<int> mapping;
    if (mapping_v && !mapping_v->isEmpty()) {
        readVecInt(*mapping_v, mapping);
        if (mapping.size() != static_cast<size_t>(Ntot))
            throw Error("apskdemod: SymbolMapping length must equal sum(M)",
                        0, 0, "apskdemod", "", "m:apskdemod:MappingLen");
    } else {
        mapping.resize(static_cast<size_t>(Ntot));
        for (int i = 0; i < Ntot; ++i) mapping[i] = i;
    }

    Value out = Value::matrix(y.dims().rows(), y.dims().cols(),
                              ValueType::DOUBLE, mr);
    double *o = out.doubleDataMut();
    const size_t Ny = y.numel();
    const bool y_complex = y.isComplex();
    for (size_t i = 0; i < Ny; ++i) {
        const Cd yi = y_complex
                          ? y.complexData()[i]
                          : Cd(y.elemAsDouble(i), 0.0);
        // Find nearest constellation point (by squared distance).
        size_t best_k = 0;
        double best_d2 = std::numeric_limits<double>::infinity();
        for (size_t k = 0; k < C.size(); ++k) {
            const double dr = yi.real() - C[k].real();
            const double di = yi.imag() - C[k].imag();
            const double d2 = dr * dr + di * di;
            if (d2 < best_d2) { best_d2 = d2; best_k = k; }
        }
        // Map constellation index back through user mapping.
        o[i] = static_cast<double>(mapping[best_k]);
    }
    return out;
}

namespace detail {

// Helpers to extract optional phase / mapping arguments, treating
// 'SymbolMapping' name-value pairs as a positional 5th arg.
const Value *optArg(Span<const Value> args, size_t pos)
{
    if (args.size() > pos && !args[pos].isEmpty()) return &args[pos];
    return nullptr;
}

void apskmod_reg(Span<const Value> args, size_t /*nargout*/,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("apskmod: requires (x, M, radii [, phaseoffset [, mapping]])",
                    0, 0, "apskmod", "", "m:apskmod:nargin");
    auto *mr = ctx.engine->resource();
    const Value *po = optArg(args, 3);
    const Value *mp = optArg(args, 4);
    outs[0] = apskmod(mr, args[0], args[1], args[2], po, mp);
}

void apskdemod_reg(Span<const Value> args, size_t /*nargout*/,
                   Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("apskdemod: requires (y, M, radii [, phaseoffset [, mapping]])",
                    0, 0, "apskdemod", "", "m:apskdemod:nargin");
    auto *mr = ctx.engine->resource();
    const Value *po = optArg(args, 3);
    const Value *mp = optArg(args, 4);
    outs[0] = apskdemod(mr, args[0], args[1], args[2], po, mp);
}

} // namespace detail

} // namespace numkit::comm
