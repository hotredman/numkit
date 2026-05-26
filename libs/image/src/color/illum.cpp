// libs/image/src/color/illum.cpp
//
// White-balance illumination estimation:
//   illum = illumwhite(A [, P] [, 'Mask', M])
//   illum = illumgray (A [, P] [, 'Mask', M] [, 'Norm', n])
//
// Returns a 1×3 RGB row vector that approximates the scene illuminant.
//
// References:
//   • Land, E.H. & McCann, J.J. (1971), "Lightness and Retinex Theory",
//     J. Opt. Soc. Am. 61(1): 1-11 — White-Patch retinex (illumwhite,
//     P = 0 case).
//   • Banić, N. & Lončarić, S. (2014), "Improving the white patch
//     method by subsampling", IEEE ICIP, pp. 605-609 — the
//     top-percentile per-channel variant adopted by MATLAB R2025b
//     illumwhite.
//   • Buchsbaum, G. (1980), "A spatial processor model for object
//     colour perception", J. Franklin Inst. 310(1): 1-26 — Grey-World
//     baseline behind illumgray.
//   • Ebner, M. (2007), "The Gray World Assumption", in *Color
//     Constancy*, John Wiley & Sons — referenced by MATLAB's own
//     `help illumgray`.
//
// Per-channel histogram algorithm (matches MATLAB R2025b source we
// inspected — `toolbox/images/colorspaces/illumwhite.m`):
//
//   For each channel k = 1..3:
//     1. Apply mask → vector `plane`.
//     2. illumwhite: sort `plane` ascending; the channel value is the
//        smallest x such that count(plane >= x) > N · P/100. With
//        P = 0 → max(plane). With P > 0, P · N / 100 = K → take the
//        (N - K)-th smallest (0-based index N-K-1), i.e. the largest
//        value such that strictly more than K pixels lie at-or-above
//        it. MATLAB uses imhist with 2^16 bins for float input, which
//        adds ~1.5 × 10⁻⁵ quantisation; our direct sort matches the
//        algorithm to within that tolerance.
//     3. illumgray: trim the bottom `p_lo`% and the top `p_hi`% (the
//        scalar form uses the same value for both ends — both
//        default to 1). Then form `mean(|x|^n)^(1/n) / count` over
//        the surviving pixels, where n = 'Norm' (default 1, i.e.
//        plain arithmetic mean).

#include <numkit/image/color/color.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/scratch.hpp>
#include <numkit/core/types.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace numkit::image {

namespace {

inline double pixel_at(const Value &A, std::size_t H, std::size_t W,
                       std::size_t i, std::size_t j, std::size_t c)
{
    return A.elemAsDouble(c * H * W + j * H + i);
}

void validate_image_and_mask(const Value &A, const Value &mask,
                             std::size_t &H, std::size_t &W,
                             std::vector<unsigned char> &maskFlat,
                             const char *fn)
{
    if (!A.dims().is3D() || A.dims().pages() != 3)
        throw Error(std::string(fn) + ": input must be H×W×3",
                    0, 0, fn, "", "m:image:shape");
    H = A.dims().rows();
    W = A.dims().cols();
    if (mask.isEmpty()) {
        maskFlat.assign(H * W, 1);
        return;
    }
    const auto &md = mask.dims();
    if (md.rows() != H || md.cols() != W || md.pages() > 1)
        throw Error(std::string(fn) + ": Mask must be H×W matching the image",
                    0, 0, fn, "", "m:image:maskShape");
    maskFlat.resize(H * W);
    for (std::size_t k = 0; k < H * W; ++k)
        maskFlat[k] = (mask.elemAsDouble(k) != 0.0) ? 1 : 0;
}

void collect_channel(const Value &A, std::size_t H, std::size_t W,
                     std::size_t c, const std::vector<unsigned char> &mask,
                     std::vector<double> &out)
{
    out.clear();
    out.reserve(H * W);
    for (std::size_t j = 0; j < W; ++j) {
        for (std::size_t i = 0; i < H; ++i) {
            const std::size_t k = j * H + i;
            if (!mask[k]) continue;
            out.push_back(pixel_at(A, H, W, i, j, c));
        }
    }
}

Value make_row3(double r, double g, double b, std::pmr::memory_resource *mr)
{
    Value out = Value::matrix(1, 3, ValueType::DOUBLE, mr);
    out.doubleDataMut()[0] = r;
    out.doubleDataMut()[1] = g;
    out.doubleDataMut()[2] = b;
    return out;
}

} // anonymous

// ── illumwhite ─────────────────────────────────────────────────────
Value illumwhite(const Value &A, double P, const Value &mask,
                 std::pmr::memory_resource *mr)
{
    if (P < 0.0 || P >= 100.0)
        throw Error("illumwhite: percentile must satisfy 0 <= P < 100",
                    0, 0, "illumwhite", "", "m:illumwhite:percentile");

    std::size_t H = 0, W = 0;
    std::vector<unsigned char> maskFlat;
    validate_image_and_mask(A, mask, H, W, maskFlat, "illumwhite");
    if (H == 0 || W == 0)
        throw Error("illumwhite: image is empty",
                    0, 0, "illumwhite", "", "m:illumwhite:empty");

    double out[3] = {0.0, 0.0, 0.0};
    std::vector<double> plane;
    for (std::size_t c = 0; c < 3; ++c) {
        collect_channel(A, H, W, c, maskFlat, plane);
        if (plane.empty())
            throw Error("illumwhite: no pixels selected by Mask",
                        0, 0, "illumwhite", "", "m:illumwhite:emptyMask");
        std::sort(plane.begin(), plane.end());
        const std::size_t N = plane.size();
        // K = floor(N · P / 100). We want the largest value such that
        // strictly more than K pixels lie at-or-above it. After sorting
        // ascending, that is element at index (N - K - 1). For K = 0
        // this is the max (index N-1). For K = N this clamps to index 0
        // (lowest), but P < 100 guarantees K < N so we never hit that.
        std::size_t K = static_cast<std::size_t>(
            std::floor(P * 0.01 * static_cast<double>(N)));
        if (K >= N) K = N - 1;
        out[c] = plane[N - K - 1];
    }
    return make_row3(out[0], out[1], out[2], mr);
}

// ── illumgray ──────────────────────────────────────────────────────
//
// Public API matches MATLAB R2025b: scalar or 2-vector percentile,
// optional Mask + Norm exponent. We expose Norm via the engine
// adapter's name-value parser below; the typed entry-point fixes
// `norm_exp = 1` (the default).
static Value illumgray_impl(const Value &A, const std::vector<double> &P,
                            const Value &mask, double norm_exp,
                            std::pmr::memory_resource *mr)
{
    double p_lo = 1.0, p_hi = 1.0;   // MATLAB defaults
    if (P.size() == 1) {
        p_lo = P[0]; p_hi = P[0];
    } else if (P.size() == 2) {
        p_lo = P[0]; p_hi = P[1];
    } else if (!P.empty()) {
        throw Error("illumgray: percentile must be scalar or 2-vector",
                    0, 0, "illumgray", "", "m:illumgray:percentile");
    }
    if (p_lo < 0.0 || p_lo >= 100.0 || p_hi < 0.0 || p_hi >= 100.0
        || p_lo + p_hi > 100.0)
        throw Error("illumgray: percentiles must satisfy 0 <= P < 100 and "
                    "p_lo + p_hi <= 100",
                    0, 0, "illumgray", "", "m:illumgray:percentile");
    if (!(norm_exp > 0.0))
        throw Error("illumgray: Norm must be a positive scalar",
                    0, 0, "illumgray", "", "m:illumgray:norm");

    std::size_t H = 0, W = 0;
    std::vector<unsigned char> maskFlat;
    validate_image_and_mask(A, mask, H, W, maskFlat, "illumgray");
    if (H == 0 || W == 0)
        throw Error("illumgray: image is empty",
                    0, 0, "illumgray", "", "m:illumgray:empty");

    double out[3] = {0.0, 0.0, 0.0};
    std::vector<double> plane;
    for (std::size_t c = 0; c < 3; ++c) {
        collect_channel(A, H, W, c, maskFlat, plane);
        if (plane.empty())
            throw Error("illumgray: no pixels selected by Mask",
                        0, 0, "illumgray", "", "m:illumgray:emptyMask");
        std::sort(plane.begin(), plane.end());
        const std::size_t N = plane.size();
        const std::size_t K_lo = static_cast<std::size_t>(
            std::floor(p_lo * 0.01 * static_cast<double>(N)));
        const std::size_t K_hi = static_cast<std::size_t>(
            std::floor(p_hi * 0.01 * static_cast<double>(N)));
        if (K_lo + K_hi >= N)
            throw Error("illumgray: percentiles trim all pixels",
                        0, 0, "illumgray", "", "m:illumgray:emptyTrim");
        // MATLAB picks min/max bin values from the histogram and then
        // masks `plane >= minVal-eps & plane <= maxVal+eps`. For
        // strictly-ascending unique values that is equivalent to
        // trimming `K_lo` from the bottom and `K_hi` from the top.
        const std::size_t lo = K_lo;
        const std::size_t hi = N - K_hi;          // exclusive
        long double sum = 0.0L;
        for (std::size_t k = lo; k < hi; ++k) {
            const double v = plane[k];
            if (norm_exp == 1.0)       sum += std::fabs(v);
            else                       sum += std::pow(std::fabs(v), norm_exp);
        }
        const std::size_t cnt = hi - lo;
        long double mean;
        if (norm_exp == 1.0) mean = sum / static_cast<long double>(cnt);
        else                 mean = std::pow(static_cast<double>(sum), 1.0 / norm_exp)
                                  / static_cast<long double>(cnt);
        out[c] = static_cast<double>(mean);
    }
    return make_row3(out[0], out[1], out[2], mr);
}

Value illumgray(const Value &A, const std::vector<double> &P,
                const Value &mask, std::pmr::memory_resource *mr)
{
    return illumgray_impl(A, P, mask, 1.0, mr);
}

// ── Engine adapters ─────────────────────────────────────────────────

namespace detail {

// Name-value parser shared by illumwhite/illumgray; supports the
// two MATLAB options 'Mask' and 'Norm' (the latter is illumgray-only,
// caller passes `allow_norm = false` for illumwhite).
static void parse_nv_pairs(Span<const Value> args, std::size_t start_idx,
                           bool allow_norm, Value &mask, double &norm_exp,
                           const char *fn)
{
    while (start_idx + 1 < args.size()) {
        if (!args[start_idx].isChar() && !args[start_idx].isString())
            throw Error(std::string(fn) + ": expected option name string",
                        0, 0, fn, "", "m:image:badNvArg");
        std::string name = args[start_idx].toString();
        for (auto &c : name) c = static_cast<char>(std::tolower(
            static_cast<unsigned char>(c)));
        if (name == "mask") {
            mask = args[start_idx + 1];
        } else if (allow_norm && name == "norm") {
            norm_exp = args[start_idx + 1].toScalar();
        } else {
            throw Error(std::string(fn) + ": unknown option '" + name + "'",
                        0, 0, fn, "", "m:image:unknownNvArg");
        }
        start_idx += 2;
    }
    if (start_idx < args.size())
        throw Error(std::string(fn) + ": trailing unpaired name-value arg",
                    0, 0, fn, "", "m:image:unpairedNv");
}

void illumwhite_reg(Span<const Value> args, size_t /*nargout*/,
                    Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 1)
        throw Error("illumwhite: requires (A [, P] [, 'Mask', M])",
                    0, 0, "illumwhite", "", "m:illumwhite:nargin");
    auto *mr = ctx.engine->resource();
    double P = 1.0;       // MATLAB default percentile = 1%.
    std::size_t i = 1;
    if (args.size() >= 2 && !args[1].isChar() && !args[1].isString()) {
        P = args[1].toScalar();
        i = 2;
    }
    Value mask = Value::Empty;
    double dummy_norm = 1.0;
    parse_nv_pairs(args, i, /*allow_norm=*/false, mask, dummy_norm,
                   "illumwhite");
    outs[0] = illumwhite(args[0], P, mask, mr);
}

void illumgray_reg(Span<const Value> args, size_t /*nargout*/,
                   Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 1)
        throw Error("illumgray: requires (A [, P] [, 'Mask', M] [, 'Norm', n])",
                    0, 0, "illumgray", "", "m:illumgray:nargin");
    auto *mr = ctx.engine->resource();
    std::vector<double> P;
    std::size_t i = 1;
    if (args.size() >= 2 && !args[1].isChar() && !args[1].isString()) {
        const std::size_t n = args[1].numel();
        if (n == 1) P.push_back(args[1].toScalar());
        else if (n == 2) {
            P.push_back(args[1].elemAsDouble(0));
            P.push_back(args[1].elemAsDouble(1));
        } else if (n != 0) {
            throw Error("illumgray: percentile must be scalar or 2-vector",
                        0, 0, "illumgray", "", "m:illumgray:percentile");
        }
        i = 2;
    }
    Value mask = Value::Empty;
    double norm_exp = 1.0;
    parse_nv_pairs(args, i, /*allow_norm=*/true, mask, norm_exp,
                   "illumgray");
    outs[0] = illumgray_impl(args[0], P, mask, norm_exp, mr);
}

} // namespace detail
} // namespace numkit::image
