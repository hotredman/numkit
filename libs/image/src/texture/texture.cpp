// libs/image/src/texture/texture.cpp
//
// Texture Analysis — graycomatrix + graycoprops.

#include <numkit/image/texture/texture.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/scratch.hpp>
#include <numkit/core/types.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <memory_resource>

namespace numkit::image {

namespace {

// Map an arbitrary numeric Value element to its double representation.
inline double elem_as_double(const Value &v, std::size_t i)
{
    return v.elemAsDouble(i);
}

// Default GrayLimits per MATLAB:
//   - uint8  -> [0, 255]
//   - uint16 -> [0, 65535]
//   - int16  -> [-32768, 32767]
//   - logical-> [0, 1]
//   - single / double -> [min(I), max(I)] (computed by caller)
inline void default_gray_limits(const Value &I, double &lo, double &hi)
{
    switch (I.type()) {
        case ValueType::UINT8:   lo = 0.0;     hi = 255.0;    return;
        case ValueType::UINT16:  lo = 0.0;     hi = 65535.0;  return;
        case ValueType::INT16:   lo = -32768;  hi = 32767;    return;
        case ValueType::LOGICAL: lo = 0.0;     hi = 1.0;      return;
        default: break;
    }
    // single / double: scan for min/max.
    const std::size_t N = I.numel();
    if (N == 0) { lo = 0.0; hi = 1.0; return; }
    double mn = elem_as_double(I, 0);
    double mx = mn;
    for (std::size_t i = 1; i < N; ++i) {
        const double v = elem_as_double(I, i);
        if (v < mn) mn = v;
        if (v > mx) mx = v;
    }
    lo = mn; hi = mx;
}

} // anonymous namespace

Value graycomatrix(const Value &I, int numLevels, int offR, int offC, double gLow, double gHigh, bool symmetric, std::pmr::memory_resource *mr)
{
    if (numLevels < 2)
        throw Error("graycomatrix: NumLevels must be >= 2",
                    0, 0, "graycomatrix", "", "numkit:graycomatrix:badLevels");

    const auto &d = I.dims();
    if (d.is3D())
        throw Error("graycomatrix: 2-D grayscale input only",
                    0, 0, "graycomatrix", "", "numkit:graycomatrix:unsupportedShape");
    const std::size_t R = d.rows();
    const std::size_t C = d.cols();

    if (gLow >= gHigh)
        throw Error("graycomatrix: GrayLimits[1] must exceed GrayLimits[0]",
                    0, 0, "graycomatrix", "", "numkit:graycomatrix:badLimits");

    // Pre-quantise the image into level indices in [0, numLevels-1].
    // MATLAB clamps out-of-range to the closest end-bin.
    ScratchArena scratch(mr);
    ScratchVec<int> q(R * C, &scratch);
    const double span = gHigh - gLow;
    const double scale = static_cast<double>(numLevels) / span;
    for (std::size_t i = 0; i < R * C; ++i) {
        const double v = elem_as_double(I, i);
        if (std::isnan(v)) {
            q[i] = -1;    // NaN — skip when forming pairs
            continue;
        }
        int level = static_cast<int>(std::floor((v - gLow) * scale));
        if (level < 0)            level = 0;
        if (level >= numLevels)   level = numLevels - 1;
        q[i] = level;
    }

    // Allocate output (numLevels × numLevels, column-major).
    Value G = Value::matrix(static_cast<std::size_t>(numLevels),
                             static_cast<std::size_t>(numLevels),
                             ValueType::DOUBLE, mr);
    double *Gd = G.doubleDataMut();
    std::memset(Gd, 0,
                sizeof(double) * static_cast<std::size_t>(numLevels) *
                                  static_cast<std::size_t>(numLevels));

    // Walk every pair (p, p+offset) that lies inside the image.
    for (std::size_t c = 0; c < C; ++c) {
        const long long c2 = static_cast<long long>(c) + offC;
        if (c2 < 0 || c2 >= static_cast<long long>(C)) continue;
        for (std::size_t r = 0; r < R; ++r) {
            const long long r2 = static_cast<long long>(r) + offR;
            if (r2 < 0 || r2 >= static_cast<long long>(R)) continue;
            const int i_lvl = q[r + c * R];                // first pixel
            const int j_lvl = q[r2 + c2 * R];              // offset pixel
            if (i_lvl < 0 || j_lvl < 0) continue;          // NaN guard
            Gd[i_lvl + j_lvl * numLevels] += 1.0;
            if (symmetric)
                Gd[j_lvl + i_lvl * numLevels] += 1.0;
        }
    }
    return G;
}

Value graycoprops(const Value &G, std::pmr::memory_resource *mr)
{
    const auto &d = G.dims();
    if (d.rows() != d.cols() || d.rows() == 0)
        throw Error("graycoprops: GLCM must be square and non-empty",
                    0, 0, "graycoprops", "", "numkit:graycoprops:badShape");
    const std::size_t L = d.rows();

    // Normalise to a joint probability.
    double total = 0.0;
    for (std::size_t i = 0; i < L * L; ++i) total += G.elemAsDouble(i);
    if (total <= 0.0)
        throw Error("graycoprops: GLCM has zero total mass",
                    0, 0, "graycoprops", "", "numkit:graycoprops:zeroSum");

    // p(i, j) — already accessible via G.elemAsDouble(i + j*L) / total.
    // Compute marginal means / variances (1-based "intensity values"
    // matching MATLAB's convention: rows / cols treated as 1..L).
    double mu_i = 0.0, mu_j = 0.0;
    for (std::size_t j = 0; j < L; ++j)
        for (std::size_t i = 0; i < L; ++i) {
            const double p = G.elemAsDouble(i + j * L) / total;
            mu_i += static_cast<double>(i + 1) * p;
            mu_j += static_cast<double>(j + 1) * p;
        }
    double var_i = 0.0, var_j = 0.0;
    for (std::size_t j = 0; j < L; ++j)
        for (std::size_t i = 0; i < L; ++i) {
            const double p = G.elemAsDouble(i + j * L) / total;
            const double di = (i + 1) - mu_i;
            const double dj = (j + 1) - mu_j;
            var_i += di * di * p;
            var_j += dj * dj * p;
        }
    const double sigma_i = std::sqrt(var_i);
    const double sigma_j = std::sqrt(var_j);

    double contrast = 0.0, energy = 0.0, homogeneity = 0.0, correlation = 0.0;
    for (std::size_t j = 0; j < L; ++j)
        for (std::size_t i = 0; i < L; ++i) {
            const double p = G.elemAsDouble(i + j * L) / total;
            const double di = static_cast<double>(i) - static_cast<double>(j);
            contrast    += di * di * p;
            energy      += p * p;
            homogeneity += p / (1.0 + std::fabs(di));
            if (sigma_i > 0.0 && sigma_j > 0.0)
                correlation += (((i + 1) - mu_i) * ((j + 1) - mu_j) * p) /
                               (sigma_i * sigma_j);
        }
    // MATLAB convention: when sigma_i or sigma_j == 0, correlation is NaN
    // (constant images). We replicate.
    if (!(sigma_i > 0.0 && sigma_j > 0.0))
        correlation = std::nan("");

    auto field_scalar = [&](double v) { return Value::scalar(v, mr); };

    Value out = Value::structure();
    out.field("Contrast")    = field_scalar(contrast);
    out.field("Correlation") = field_scalar(correlation);
    out.field("Energy")      = field_scalar(energy);
    out.field("Homogeneity") = field_scalar(homogeneity);
    return out;
}

// ── Engine adapters ──────────────────────────────────────────────────
namespace detail {

static bool eqIgnoreCase(const std::string &a, const char *b)
{
    if (a.size() != std::strlen(b)) return false;
    for (std::size_t i = 0; i < a.size(); ++i)
        if (std::tolower(a[i]) != std::tolower(b[i])) return false;
    return true;
}

void graycomatrix_reg(Span<const Value> args, std::size_t /*nargout*/,
                      Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("graycomatrix: requires (I[, NV-pairs])",
                     0, 0, "graycomatrix", "", "numkit:graycomatrix:nargin");

    const Value &I = args[0];

    int numLevels = (I.type() == ValueType::LOGICAL) ? 2
                  : (I.type() == ValueType::UINT8)   ? 8
                                                     : 8;
    int offR = 0, offC = 1;
    double gLow = 0.0, gHigh = 0.0;
    bool   limitsSet = false;
    bool   symmetric = false;

    for (std::size_t i = 1; i + 1 < args.size(); i += 2) {
        if (!args[i].isChar())
            throw Error("graycomatrix: NV-pair name must be a string",
                         0, 0, "graycomatrix", "",
                         "numkit:graycomatrix:badNVName");
        const std::string key = args[i].toString();
        const Value &v        = args[i + 1];
        if (eqIgnoreCase(key, "NumLevels"))   numLevels = static_cast<int>(v.toScalar());
        else if (eqIgnoreCase(key, "Offset")) {
            if (v.numel() < 2)
                throw Error("graycomatrix: Offset must be 2-element",
                             0, 0, "graycomatrix", "",
                             "numkit:graycomatrix:badOffset");
            offR = static_cast<int>(v.elemAsDouble(0));
            offC = static_cast<int>(v.elemAsDouble(1));
        }
        else if (eqIgnoreCase(key, "GrayLimits")) {
            if (v.numel() < 2)
                throw Error("graycomatrix: GrayLimits must be 2-element",
                             0, 0, "graycomatrix", "",
                             "numkit:graycomatrix:badLimits");
            gLow  = v.elemAsDouble(0);
            gHigh = v.elemAsDouble(1);
            limitsSet = true;
        }
        else if (eqIgnoreCase(key, "Symmetric")) {
            symmetric = (v.toScalar() != 0.0);
        }
        else
            throw Error("graycomatrix: unknown NV-pair key '" + key + "'",
                         0, 0, "graycomatrix", "",
                         "numkit:graycomatrix:badNVKey");
    }
    if (!limitsSet) default_gray_limits(I, gLow, gHigh);

    outs[0] = graycomatrix(I, numLevels, offR, offC, gLow, gHigh, symmetric, ctx.engine->resource());
}

void graycoprops_reg(Span<const Value> args, std::size_t /*nargout*/,
                     Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("graycoprops: requires (G)",
                     0, 0, "graycoprops", "", "numkit:graycoprops:nargin");
    outs[0] = graycoprops(args[0], ctx.engine->resource());
}

} // namespace detail

} // namespace numkit::image
