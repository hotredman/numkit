// libs/image/src/contrast/contrast.cpp

#include <numkit/image/contrast/contrast.hpp>
#include <numkit/image/filter/filter.hpp>
#include <numkit/image/type_convert/type_convert.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/figure_manager.hpp>
#include <numkit/core/scratch.hpp>
#include <numkit/core/types.hpp>
#include <numkit/core/value_type.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <functional>
#include <limits>
#include <sstream>
#include <vector>

namespace numkit::image {

namespace {

// Convert any image element to a unit-range double in [0, 1].
inline double element_to_unit(const Value &x, size_t i) {
    const double v = x.elemAsDouble(i);
    switch (x.type()) {
        case ValueType::DOUBLE:
        case ValueType::SINGLE:  return v;
        case ValueType::UINT8:   return v / 255.0;
        case ValueType::UINT16:  return v / 65535.0;
        case ValueType::INT16:   return (v + 32768.0) / 65535.0;
        case ValueType::LOGICAL: return v != 0.0 ? 1.0 : 0.0;
        default:                 return v;
    }
}

inline int default_nbins(const Value &I) {
    switch (I.type()) {
        case ValueType::UINT8:  return 256;
        case ValueType::UINT16: return 65536;
        default:                return 64;
    }
}

inline void store_classed(Value &out, size_t i, double v, ValueType t) {
    switch (t) {
        case ValueType::DOUBLE: out.doubleDataMut()[i] = v; break;
        case ValueType::SINGLE: out.singleDataMut()[i] = (float)v; break;
        case ValueType::UINT8: {
            double w = std::round(v * 255.0);
            if (w < 0) w = 0; if (w > 255) w = 255;
            out.uint8DataMut()[i] = (uint8_t)w; break;
        }
        case ValueType::UINT16: {
            double w = std::round(v * 65535.0);
            if (w < 0) w = 0; if (w > 65535) w = 65535;
            out.uint16DataMut()[i] = (uint16_t)w; break;
        }
        case ValueType::INT16: {
            double w = std::round(v * 65535.0) - 32768.0;
            if (w < -32768) w = -32768; if (w > 32767) w = 32767;
            out.int16DataMut()[i] = (int16_t)w; break;
        }
        default:
            throw Error("contrast: unsupported class", 0, 0, "contrast", "",
                        "numkit:contrast:badtype");
    }
}

} // anonymous

std::tuple<Value, Value>
imhist(const Value &I, int n, std::pmr::memory_resource *mr)
{
    if (n <= 0) n = default_nbins(I);
    std::vector<int64_t> counts(n, 0);
    const size_t N = I.numel();
    for (size_t i = 0; i < N; ++i) {
        const double u = element_to_unit(I, i);
        // Map u (∈ [0, 1]) to bin index 0..n-1 with edges spaced 1/(n-1)
        // so bin centres are 0, 1/(n-1), ..., 1 (matches MATLAB).
        if (std::isnan(u)) continue;
        int bin = (int)std::round(u * (n - 1));
        if (bin < 0) bin = 0;
        if (bin >= n) bin = n - 1;
        ++counts[bin];
    }
    Value c = Value::matrix(n, 1, ValueType::DOUBLE, mr);
    Value x = Value::matrix(n, 1, ValueType::DOUBLE, mr);
    double *cd = c.doubleDataMut();
    double *xd = x.doubleDataMut();
    // Bin locations x span the input CLASS's display range, not [0,1]:
    // double/single/logical -> [0,1]; uint8 -> [0,255]; uint16 ->
    // [0,65535]; int16 -> [-32768,32767]. (Counts above are computed on
    // the normalized value, so only the x labels depend on class.)
    double lo = 0.0, hi = 1.0;
    switch (I.type()) {
        case ValueType::UINT8:  hi = 255.0; break;
        case ValueType::UINT16: hi = 65535.0; break;
        case ValueType::INT16:  lo = -32768.0; hi = 32767.0; break;
        default: break;  // DOUBLE / SINGLE / LOGICAL / other -> [0,1]
    }
    const double step = (n > 1) ? (hi - lo) / double(n - 1) : 0.0;
    for (int i = 0; i < n; ++i) {
        cd[i] = (double)counts[i];
        xd[i] = lo + i * step;
    }
    return std::make_tuple(std::move(c), std::move(x));
}

Value stretchlim(const Value &I, double low_tol, double high_tol, std::pmr::memory_resource *mr)
{
    if (low_tol < 0.0)  low_tol  = 0.01;
    if (high_tol > 1.0 || high_tol <= low_tol) high_tol = 0.99;

    // Separate per-channel for H×W×3 RGB; otherwise single column.
    const auto &d = I.dims();
    const bool is_rgb = d.is3D() && d.pages() == 3;
    const size_t plane = d.is3D() ? d.rows() * d.cols() : I.numel();
    const int channels = is_rgb ? 3 : 1;

    Value out = Value::matrix(2, channels, ValueType::DOUBLE, mr);
    double *od = out.doubleDataMut();

    // MATLAB's stretchlim uses a histogram with strict-greater cumulative
    // thresholds, NOT sample-percentile. The bin count is class-dependent:
    // 256 bins for uint8, 65536 for uint16 / int16 / single / double. For
    // uint8 the bin index is the value directly; for the wider classes the
    // input is normalized to [0,1] and rebinned. (Previously a fixed 256
    // bins gave coarsely-quantized limits on double images — e.g.
    // stretchlim([0.1 0.2 0.9 0.95]) returned [26/255 242/255] instead of
    // [6554/65535 62258/65535].)
    const int NBINS = (I.type() == ValueType::UINT8) ? 256 : 65536;
    std::vector<std::size_t> hist(NBINS, 0);
    std::vector<std::size_t> cumhist(NBINS, 0);
    const double N = static_cast<double>(plane);

    for (int ch = 0; ch < channels; ++ch) {
        std::fill(hist.begin(), hist.end(), 0);
        for (std::size_t i = 0; i < plane; ++i) {
            const double u = element_to_unit(I, ch * plane + i);
            int bin = static_cast<int>(std::lround(u * (NBINS - 1)));
            if (bin < 0) bin = 0;
            if (bin >= NBINS) bin = NBINS - 1;
            ++hist[bin];
        }
        cumhist[0] = hist[0];
        for (int k = 1; k < NBINS; ++k) cumhist[k] = cumhist[k - 1] + hist[k];

        // low: smallest bin k where cumhist[k] > low_tol * N (strict).
        int low = NBINS - 1;
        for (int k = 0; k < NBINS; ++k) {
            if (static_cast<double>(cumhist[k]) > low_tol * N) { low = k; break; }
        }
        // high: smallest bin k where cumhist[k] >= high_tol * N.
        int high = NBINS - 1;
        for (int k = 0; k < NBINS; ++k) {
            if (static_cast<double>(cumhist[k]) >= high_tol * N) { high = k; break; }
        }
        // Guard: if histogram is degenerate (all in one bin), spread by 1.
        if (high <= low) high = std::min(NBINS - 1, low + 1);

        od[static_cast<std::size_t>(ch) * 2 + 0] =
            static_cast<double>(low)  / static_cast<double>(NBINS - 1);
        od[static_cast<std::size_t>(ch) * 2 + 1] =
            static_cast<double>(high) / static_cast<double>(NBINS - 1);
    }
    return out;
}

Value imadjust(const Value &I, double low_in, double high_in, double low_out, double high_out, double gamma, std::pmr::memory_resource *mr)
{
    // Auto-fill missing endpoints via stretchlim defaults.
    if (std::isnan(low_in) || std::isnan(high_in)) {
        Value lim = stretchlim(I, 0.01, 0.99, mr);
        if (std::isnan(low_in))  low_in  = lim.elemAsDouble(0);
        if (std::isnan(high_in)) high_in = lim.elemAsDouble(1);
    }
    if (std::isnan(low_out))  low_out  = 0.0;
    if (std::isnan(high_out)) high_out = 1.0;
    if (std::isnan(gamma) || gamma <= 0.0) gamma = 1.0;

    const size_t N = I.numel();
    Value out;
    const auto &d = I.dims();
    if (I.isScalar()) out = Value::matrix(1, 1, I.type(), mr);
    else if (d.is3D())out = Value::matrix3d(d.rows(), d.cols(), d.pages(), I.type(), mr);
    else              out = Value::matrix(d.rows(), d.cols(), I.type(), mr);

    const double range = high_in - low_in;
    const double inv_range = (range != 0.0) ? 1.0 / range : 0.0;
    const double out_span = high_out - low_out;

    for (size_t i = 0; i < N; ++i) {
        const double u = element_to_unit(I, i);
        double t = (u - low_in) * inv_range;
        if (t < 0.0) t = 0.0;
        if (t > 1.0) t = 1.0;
        const double w = std::pow(t, gamma) * out_span + low_out;
        store_classed(out, i, w, I.type());
    }
    return out;
}

Value histeq(const Value &I, int n, std::pmr::memory_resource *mr)
{
    if (n <= 0) n = 64;
    auto [counts_v, bins_v] = imhist(I, n, mr);
    const double *counts = counts_v.doubleData();
    // Cumulative distribution.
    std::vector<double> cdf(n);
    double total = 0.0;
    for (int i = 0; i < n; ++i) total += counts[i];
    if (total <= 0.0) return I;
    double acc = 0.0;
    for (int i = 0; i < n; ++i) {
        acc += counts[i];
        cdf[i] = acc / total;
    }

    const size_t N = I.numel();
    Value out;
    const auto &d = I.dims();
    if (I.isScalar()) out = Value::matrix(1, 1, I.type(), mr);
    else if (d.is3D())out = Value::matrix3d(d.rows(), d.cols(), d.pages(), I.type(), mr);
    else              out = Value::matrix(d.rows(), d.cols(), I.type(), mr);

    for (size_t i = 0; i < N; ++i) {
        const double u = element_to_unit(I, i);
        int bin = (int)std::round(u * (n - 1));
        if (bin < 0) bin = 0;
        if (bin >= n) bin = n - 1;
        store_classed(out, i, cdf[bin], I.type());
    }
    return out;
}

// ════════════════════════════════════════════════════════════════════
// adapthisteq — Contrast Limited Adaptive Histogram Equalisation
// ════════════════════════════════════════════════════════════════════
//
// Clean-room implementation of CLAHE — see cleanroom/specs/adapthisteq.md.
//
// Public references:
//   * K. Zuiderveld, "Contrast Limited Adaptive Histogram
//     Equalization", in Graphics Gems IV (P. S. Heckbert, ed.),
//     Academic Press, 1994, pp. 474-485 — tile-based CLAHE with
//     bilinear interpolation between per-tile mapping functions.
//   * S. M. Pizer et al., "Contrast-Limited Adaptive Histogram
//     Equalization: Speed and Effectiveness", Proc. 1st Conf. on
//     Visualization in Biomedical Computing, IEEE, 1990 (UNC TR
//     90-035) — the contrast-limiting step (clip + redistribute).
//   * S. M. Pizer et al., "Adaptive Histogram Equalization and Its
//     Variations", Computer Vision, Graphics, and Image Processing
//     39:355-368, 1987 — the AHE family and non-uniform distributions.
//
// The image is split into numTilesR x numTilesC tiles; each tile gets a
// clipped+redistributed histogram whose CDF, shaped by the target
// Distribution (uniform/rayleigh/exponential), is the tile mapping LUT.
// Output pixels bilinearly interpolate the four nearest tile LUTs.
// Full MATLAB argument set: NumTiles, ClipLimit, NBins, Range,
// Distribution, Alpha. 2-D greyscale input only (as MATLAB).

namespace {

// ---- intensity range of a numkit element type --------------------
// Returns the [lo, hi] span MATLAB associates with the image class.
// Floating-point images conventionally live in [0, 1]; integer
// images span the full representable range of the integer type;
// logical images span [0, 1].
static void classRange(ValueType t, double &lo, double &hi)
{
    switch (t) {
        case ValueType::DOUBLE:
        case ValueType::SINGLE:  lo = 0.0;                       hi = 1.0;                       break;
        case ValueType::LOGICAL: lo = 0.0;                       hi = 1.0;                       break;
        case ValueType::CHAR:    lo = 0.0;                       hi = 65535.0;                   break;
        case ValueType::INT8:    lo = -128.0;                    hi = 127.0;                     break;
        case ValueType::INT16:   lo = -32768.0;                  hi = 32767.0;                   break;
        case ValueType::INT32:   lo = -2147483648.0;             hi = 2147483647.0;              break;
        case ValueType::INT64:   lo = -9223372036854775808.0;    hi = 9223372036854775807.0;     break;
        case ValueType::UINT8:   lo = 0.0;                       hi = 255.0;                     break;
        case ValueType::UINT16:  lo = 0.0;                       hi = 65535.0;                   break;
        case ValueType::UINT32:  lo = 0.0;                       hi = 4294967295.0;              break;
        case ValueType::UINT64:  lo = 0.0;                       hi = 18446744073709551615.0;    break;
        default:                 lo = 0.0;                       hi = 1.0;                       break;
    }
}

// ---- per-tile clipped+redistributed histogram --------------------
// hist[]    : raw bin counts (length nBins), modified in place to the
//             clipped/redistributed counts.
// numPix    : pixels in the tile.
// clipLimit : fraction in [0,1] (0 = no clipping).
static void clipAndRedistribute(double *hist, int nBins,
                                double numPix, double clipLimit)
{
    // Flat-histogram height; the clip ceiling sits between the flat
    // height (clipLimit==0 ⇒ ordinary uniformisation) and numPix
    // (clipLimit==1 ⇒ maximum contrast).
    const double minLimit  = numPix / static_cast<double>(nBins);
    const double clipCount = minLimit + clipLimit * (numPix - minLimit);

    // Clip every bin, accumulate the removed excess.
    double excess = 0.0;
    for (int b = 0; b < nBins; ++b) {
        if (hist[b] > clipCount) {
            excess += hist[b] - clipCount;
            hist[b] = clipCount;
        }
    }

    // Redistribute the excess uniformly: a flat increment to every
    // bin plus a one-count-per-bin sweep for the remainder. A single
    // pass per Pizer 1990 — bins may end slightly above clipCount.
    if (excess > 0.0) {
        const double perBin = std::floor(excess / static_cast<double>(nBins));
        double remainder    = excess - perBin * static_cast<double>(nBins);
        for (int b = 0; b < nBins; ++b)
            hist[b] += perBin;
        for (int b = 0; b < nBins && remainder > 0.0; ++b) {
            hist[b] += 1.0;
            remainder -= 1.0;
        }
    }
}

// ---- target-distribution mapping ---------------------------------
// Converts the cumulative probability P (in [0,1]) into an output
// intensity in [outMin, outMax] according to the chosen target
// histogram shape. dist: 0=uniform, 1=rayleigh, 2=exponential.
static double mapProbability(double p, int dist, double alpha,
                             double outMin, double outMax)
{
    const double span = outMax - outMin;
    switch (dist) {
        case 1: {  // rayleigh — inverse CDF
            // y = outMin + span·sqrt( 2·alpha²·ln(1/(1-P)) ),
            // P clamped just below 1 so the log stays finite.
            double pc = p;
            if (pc > 1.0 - 1e-12) pc = 1.0 - 1e-12;
            const double v = std::sqrt(2.0 * alpha * alpha
                                       * std::log(1.0 / (1.0 - pc)));
            double y = outMin + span * v;
            if (y < outMin) y = outMin;
            if (y > outMax) y = outMax;
            return y;
        }
        case 2: {  // exponential — inverse CDF
            // y = outMin - span·(1/alpha)·ln(1-P), clamped to range.
            double pc = p;
            if (pc > 1.0 - 1e-12) pc = 1.0 - 1e-12;
            const double v = -(1.0 / alpha) * std::log(1.0 - pc);
            double y = outMin + span * v;
            if (y < outMin) y = outMin;
            if (y > outMax) y = outMax;
            return y;
        }
        default:   // uniform
            return outMin + p * span;
    }
}

// ---- bilinear blend of up to four tile LUTs ----------------------
// Looks the bin up in the four neighbouring tile LUTs and blends them
// with the given fractional weights. fr/fc in [0,1].
static double bilinearLUT(const double *lutTL, const double *lutTR,
                          const double *lutBL, const double *lutBR,
                          int bin, double fr, double fc)
{
    const double top = lutTL[bin] * (1.0 - fc) + lutTR[bin] * fc;
    const double bot = lutBL[bin] * (1.0 - fc) + lutBR[bin] * fc;
    return top * (1.0 - fr) + bot * fr;
}

}  // namespace

Value adapthisteq(const Value &I, const AdaptHistEqOptions &opts,
                  std::pmr::memory_resource *mr)
{
    // ---- 1. validate options ------------------------------------
    if (opts.numTilesR < 2 || opts.numTilesC < 2)
        throw Error("adapthisteq: NumTiles must be >= 2 in each dimension",
                    0, 0, "adapthisteq", "", "numkit:adapthisteq:badTiles");
    if (!(opts.clipLimit >= 0.0 && opts.clipLimit <= 1.0))
        throw Error("adapthisteq: ClipLimit must be in [0, 1]",
                    0, 0, "adapthisteq", "", "numkit:adapthisteq:badClip");
    if (opts.nBins < 2)
        throw Error("adapthisteq: NBins must be >= 2",
                    0, 0, "adapthisteq", "", "numkit:adapthisteq:badBins");

    int dist;
    if      (opts.distribution == "uniform")     dist = 0;
    else if (opts.distribution == "rayleigh")    dist = 1;
    else if (opts.distribution == "exponential") dist = 2;
    else
        throw Error("adapthisteq: Distribution must be 'uniform', "
                    "'rayleigh' or 'exponential'",
                    0, 0, "adapthisteq", "", "numkit:adapthisteq:badDistribution");

    bool rangeOriginal;
    if      (opts.range == "full")     rangeOriginal = false;
    else if (opts.range == "original") rangeOriginal = true;
    else
        throw Error("adapthisteq: Range must be 'full' or 'original'",
                    0, 0, "adapthisteq", "", "numkit:adapthisteq:badRange");

    // ---- 2. validate input shape --------------------------------
    const Dims &d = I.dims();
    if (d.ndim() > 2 || (d.ndim() == 3 && d.pages() > 1))
        throw Error("adapthisteq: input must be a 2-D greyscale image",
                    0, 0, "adapthisteq", "", "numkit:adapthisteq:unsupportedShape");
    if (I.isComplex() || I.isCell() || I.isStruct() ||
        I.isString() || I.isFuncHandle())
        throw Error("adapthisteq: input must be a real numeric image",
                    0, 0, "adapthisteq", "", "numkit:adapthisteq:unsupportedShape");

    const size_t H = d.rows();
    const size_t W = d.cols();
    if (H == 0 || W == 0)
        throw Error("adapthisteq: input image is empty",
                    0, 0, "adapthisteq", "", "numkit:adapthisteq:unsupportedShape");

    const ValueType outType = I.type();
    const int numTilesR = opts.numTilesR;
    const int numTilesC = opts.numTilesC;
    const int nBins     = opts.nBins;

    ScratchArena arena(mr);

    // ---- 3. read the image into a double work buffer ------------
    // (column-major, matching numkit's storage convention).
    ScratchVec<double> src(H * W, &arena);
    for (size_t c = 0; c < W; ++c)
        for (size_t r = 0; r < H; ++r)
            src[c * H + r] = I.elemAsDouble(c * H + r);

    // ---- 4. working intensity range -----------------------------
    double classLo, classHi;
    classRange(outType, classLo, classHi);

    // Determine the input span actually used for histogramming and
    // the output span the mapping is scaled into.
    double inMin = classLo, inMax = classHi;
    double outMin = classLo, outMax = classHi;
    if (rangeOriginal) {
        // [min(I), max(I)] of the real pixels.
        double mn =  std::numeric_limits<double>::infinity();
        double mx = -std::numeric_limits<double>::infinity();
        for (size_t i = 0; i < H * W; ++i) {
            const double v = src[i];
            if (v < mn) mn = v;
            if (v > mx) mx = v;
        }
        if (!std::isfinite(mn) || !std::isfinite(mx)) { mn = classLo; mx = classHi; }
        outMin = mn;
        outMax = mx;
        // Histogram over the same actual span so the limited dynamic
        // range is fully resolved by the bins.
        inMin = mn;
        inMax = mx;
    }
    // Guard against a degenerate (flat) image / zero-width range.
    if (!(inMax > inMin)) { inMin = classLo; inMax = classHi; }
    if (!(inMax > inMin)) { inMin = 0.0;     inMax = 1.0;     }

    // ---- 5. symmetric (mirror) padding --------------------------
    // Pad each dimension up to the next multiple of the tile count.
    const size_t padBottom = (numTilesR - (H % numTilesR)) % numTilesR;
    const size_t padRight  = (numTilesC - (W % numTilesC)) % numTilesC;
    const size_t Hp = H + padBottom;
    const size_t Wp = W + padRight;
    const size_t tileH = Hp / static_cast<size_t>(numTilesR);
    const size_t tileW = Wp / static_cast<size_t>(numTilesC);

    // Padded work image (column-major). Mirror reflection without
    // repeating the edge sample: pad index p (p in [0,pad)) mirrors
    // to source index  H-2-p  (clamped to [0,H-1]).
    ScratchVec<double> img(Hp * Wp, &arena);
    auto mirrorIdx = [](size_t i, size_t n) -> size_t {
        if (n == 1) return 0;
        if (i < n)  return i;
        // reflect past the edge
        size_t k = (2 * n - 2) ? ((i) % (2 * n - 2)) : 0;
        return k < n ? k : (2 * n - 2 - k);
    };
    for (size_t c = 0; c < Wp; ++c) {
        const size_t sc = mirrorIdx(c, W);
        for (size_t r = 0; r < Hp; ++r) {
            const size_t sr = mirrorIdx(r, H);
            img[c * Hp + r] = src[sc * H + sr];
        }
    }

    // ---- 6. per-tile clipped histograms + mapping LUTs ----------
    const double invSpan = static_cast<double>(nBins) / (inMax - inMin);
    auto binOf = [&](double v) -> int {
        int b = static_cast<int>((v - inMin) * invSpan);
        if (b < 0)         b = 0;
        if (b >= nBins)    b = nBins - 1;
        return b;
    };

    const int    nTiles  = numTilesR * numTilesC;
    const double numPix  = static_cast<double>(tileH * tileW);
    // luts: nTiles consecutive LUTs of nBins entries each.
    ScratchVec<double> luts(static_cast<size_t>(nTiles) * nBins, &arena);
    ScratchVec<double> hist(static_cast<size_t>(nBins), &arena);

    for (int tr = 0; tr < numTilesR; ++tr) {
        for (int tc = 0; tc < numTilesC; ++tc) {
            // histogram this tile
            std::fill(hist.begin(), hist.end(), 0.0);
            const size_t r0 = static_cast<size_t>(tr) * tileH;
            const size_t c0 = static_cast<size_t>(tc) * tileW;
            for (size_t cc = 0; cc < tileW; ++cc)
                for (size_t rr = 0; rr < tileH; ++rr)
                    hist[binOf(img[(c0 + cc) * Hp + (r0 + rr)])] += 1.0;

            // contrast-limit (clip + redistribute)
            clipAndRedistribute(hist.data(), nBins, numPix, opts.clipLimit);

            // CDF → mapping LUT, shaped by the target distribution
            double *lut = luts.data()
                          + (static_cast<size_t>(tr) * numTilesC + tc) * nBins;
            double cum = 0.0;
            for (int b = 0; b < nBins; ++b) {
                cum += hist[b];
                const double p = cum / numPix;          // P[b] in [0,1]
                lut[b] = mapProbability(p, dist, opts.alpha, outMin, outMax);
            }
        }
    }

    // ---- 7. bilinear interpolation between tile mappings --------
    // Tile centre tr sits at row  (tr + 0.5)·tileH  in padded coords.
    // A pixel at padded row r belongs between the two tile rows whose
    // centres bracket it; outside the outermost centres it clamps to
    // the single nearest tile row (handles the corner / border
    // regions uniformly).
    ScratchVec<double> outImg(Hp * Wp, &arena);

    for (size_t c = 0; c < Wp; ++c) {
        // column tile interpolation parameters
        double cx = (static_cast<double>(c) + 0.5) / tileW - 0.5;
        int    tc0;
        double fc;
        if (cx <= 0.0)                       { tc0 = 0;            fc = 0.0; }
        else if (cx >= numTilesC - 1)        { tc0 = numTilesC-1;  fc = 0.0; }
        else { tc0 = static_cast<int>(cx);   fc = cx - tc0; }
        const int tc1 = (tc0 + 1 < numTilesC) ? tc0 + 1 : tc0;

        for (size_t r = 0; r < Hp; ++r) {
            double rx = (static_cast<double>(r) + 0.5) / tileH - 0.5;
            int    tr0;
            double fr;
            if (rx <= 0.0)                   { tr0 = 0;            fr = 0.0; }
            else if (rx >= numTilesR - 1)    { tr0 = numTilesR-1;  fr = 0.0; }
            else { tr0 = static_cast<int>(rx); fr = rx - tr0; }
            const int tr1 = (tr0 + 1 < numTilesR) ? tr0 + 1 : tr0;

            const int bin = binOf(img[c * Hp + r]);

            const double *lutTL = luts.data()
                + (static_cast<size_t>(tr0) * numTilesC + tc0) * nBins;
            const double *lutTR = luts.data()
                + (static_cast<size_t>(tr0) * numTilesC + tc1) * nBins;
            const double *lutBL = luts.data()
                + (static_cast<size_t>(tr1) * numTilesC + tc0) * nBins;
            const double *lutBR = luts.data()
                + (static_cast<size_t>(tr1) * numTilesC + tc1) * nBins;

            outImg[c * Hp + r] =
                bilinearLUT(lutTL, lutTR, lutBL, lutBR, bin, fr, fc);
        }
    }

    // ---- 8. strip padding, cast back to the input element type --
    Value result = Value::matrix(H, W, outType, mr);

    const bool intOut   = isIntegerType(outType) || outType == ValueType::CHAR
                          || outType == ValueType::LOGICAL;
    const double lo = std::min(outMin, outMax);
    const double hi = std::max(outMin, outMax);

    for (size_t c = 0; c < W; ++c) {
        for (size_t r = 0; r < H; ++r) {
            double v = outImg[c * Hp + r];
            // clamp into the output span
            if (v < lo) v = lo;
            if (v > hi) v = hi;
            double stored = v;
            if (intOut) {
                // round-half-away-from-zero, then clamp to class range
                stored = (v >= 0.0) ? std::floor(v + 0.5)
                                    : std::ceil (v - 0.5);
                if (stored < classLo) stored = classLo;
                if (stored > classHi) stored = classHi;
            }
            result.elemSet(c * H + r, Value::scalar(stored, &arena));
        }
    }

    return result;
}

// ════════════════════════════════════════════════════════════════════
// Otsu thresholding
// ════════════════════════════════════════════════════════════════════

namespace {

// Single-threshold Otsu on a histogram of L bins. Returns the
// MEAN of all tied-maximum bin indices (as MATLAB's graythresh does),
// plus the effectiveness metric eta in [0, 1].
//
// MATLAB algorithm (from graythresh.m):
//   idx = mean(find(sigma_b_squared == max(sigma_b_squared)));
// then level = (idx - 1) / (num_bins - 1).
//
// We return the 0-based mean directly; the caller divides by L-1.
std::pair<double, double> otsu_one_level(const std::vector<double> &counts) {
    const int L = (int)counts.size();
    double total = 0.0;
    double sum_total = 0.0;
    for (int i = 0; i < L; ++i) { total += counts[i]; sum_total += i * counts[i]; }
    if (total <= 0.0) return {0.0, 0.0};

    // First pass: find max sigma_b^2.
    std::vector<double> sigma_b(L, 0.0);
    {
        double w0 = 0.0, sum0 = 0.0;
        for (int t = 0; t < L - 1; ++t) {
            w0 += counts[t]; sum0 += t * counts[t];
            const double w1 = total - w0;
            if (w0 == 0.0 || w1 == 0.0) continue;
            const double mu0 = sum0 / w0;
            const double mu1 = (sum_total - sum0) / w1;
            const double diff = mu0 - mu1;
            sigma_b[t] = w0 * w1 * diff * diff;
        }
    }
    double best_var = sigma_b[0];
    for (int t = 1; t < L - 1; ++t) if (sigma_b[t] > best_var) best_var = sigma_b[t];

    // Second pass: average ALL bin indices where sigma_b == best_var.
    // Tolerate tiny numerical noise; MATLAB uses exact equality on
    // normalised values.
    int n_tied = 0;
    double sum_tied = 0.0;
    for (int t = 0; t < L - 1; ++t) {
        if (sigma_b[t] == best_var) {
            sum_tied += static_cast<double>(t);
            ++n_tied;
        }
    }
    const double best_t = (n_tied > 0) ? (sum_tied / n_tied) : 0.0;

    // Effectiveness metric eta = sigma_b^2 / sigma_T^2.
    const double mu = sum_total / total;
    double total_var = 0.0;
    for (int i = 0; i < L; ++i) total_var += counts[i] * (i - mu) * (i - mu);
    const double em = (total_var > 0.0) ? (best_var / (total * total_var)) : 0.0;
    return {best_t, em};
}

} // anonymous

std::tuple<Value, Value>
otsuthresh(const Value &counts_v, std::pmr::memory_resource *mr) {
    const size_t L = counts_v.numel();
    std::vector<double> c(L);
    for (size_t i = 0; i < L; ++i) c[i] = counts_v.elemAsDouble(i);
    auto [lvl, em] = otsu_one_level(c);
    // MATLAB graythresh / otsuthresh convention: thresh = lvl / (L - 1),
    // where lvl is the mean of all bin indices that achieve the maximum
    // sigma_b^2. (multithresh, by contrast, returns midpoints of class
    // means -- intentionally different convention in MATLAB.)
    const double thresh = (L > 1) ? (lvl / static_cast<double>(L - 1)) : 0.0;
    return std::make_tuple(Value::scalar(thresh, mr), Value::scalar(em, mr));
}

std::tuple<Value, Value>
graythresh(const Value &I, std::pmr::memory_resource *mr) {
    auto [counts, _] = imhist(I, default_nbins(I), mr);
    return otsuthresh(counts, mr);
}

std::tuple<Value, Value>
multithresh(const Value &I, int N, std::pmr::memory_resource *mr) {
    if (N <= 1) {
        auto [t, em] = graythresh(I, mr);
        return std::make_tuple(std::move(t), std::move(em));
    }
    if (N > 5)
        throw Error("multithresh: N > 5 not supported (exhaustive search would be too slow)",
                    0, 0, "multithresh", "", "numkit:multithresh:tooMany");

    const int L = default_nbins(I);
    auto [counts_v, _] = imhist(I, L, mr);
    std::vector<double> counts(L);
    for (int i = 0; i < L; ++i) counts[i] = counts_v.doubleData()[i];

    double total = 0.0, sum_total = 0.0;
    for (int i = 0; i < L; ++i) { total += counts[i]; sum_total += i * counts[i]; }
    if (total <= 0.0) {
        Value t = Value::matrix(1, N, ValueType::DOUBLE, mr);
        return std::make_tuple(std::move(t), Value::scalar(0.0, mr));
    }

    // Build cumulative sums for fast w/μ computation.
    std::vector<double> P(L + 1, 0.0), S(L + 1, 0.0);
    for (int i = 0; i < L; ++i) {
        P[i + 1] = P[i] + counts[i];
        S[i + 1] = S[i] + i * counts[i];
    }

    auto class_var = [&](int lo, int hi) {
        // [lo, hi] inclusive
        const double w = P[hi + 1] - P[lo];
        if (w == 0.0) return 0.0;
        const double s = S[hi + 1] - S[lo];
        const double mu = s / w;
        return w * mu * mu;
    };

    // Exhaustive search over N thresholds t1 < t2 < ... < tN (in 0..L-2).
    std::vector<int> best(N, 0);
    double best_var = -1.0;

    std::vector<int> idx(N);
    std::function<void(int, int)> recurse = [&](int depth, int start) {
        if (depth == N) {
            // Build sum of class variances.
            double v = 0.0;
            int prev = 0;
            for (int k = 0; k < N; ++k) {
                v += class_var(prev, idx[k]);
                prev = idx[k] + 1;
            }
            v += class_var(prev, L - 1);
            if (v > best_var) { best_var = v; best = idx; }
            return;
        }
        for (int t = start; t < L - 1 - (N - 1 - depth); ++t) {
            idx[depth] = t;
            recurse(depth + 1, t + 1);
        }
    };
    recurse(0, 0);

    // MATLAB multithresh convention: return the MIDPOINTS of adjacent
    // class MEANS, not the histogram-bin boundaries. This canonicalises
    // the tied-maximum case (Otsu's between-class variance is flat over
    // any threshold strictly between cluster centres). Then scale back
    // to the input's native value range:
    //   uint8/uint16 -> integer thresholds in 0..L-1
    //   floating-point -> normalised 0..1
    auto class_mean = [&](int lo, int hi) {
        const double w = P[hi + 1] - P[lo];
        if (w == 0.0) return double(lo);
        const double s = S[hi + 1] - S[lo];
        return s / w;
    };
    std::vector<double> means(N + 1);
    int prev = 0;
    for (int k = 0; k < N; ++k) {
        means[k] = class_mean(prev, best[k]);
        prev = best[k] + 1;
    }
    means[N] = class_mean(prev, L - 1);

    Value t_out = Value::matrix(1, N, ValueType::DOUBLE, mr);
    double *td = t_out.doubleDataMut();
    const bool isInteger = (I.type() == ValueType::UINT8
                         || I.type() == ValueType::UINT16
                         || I.type() == ValueType::INT16
                         || I.type() == ValueType::INT8);
    for (int k = 0; k < N; ++k) {
        const double midpoint = 0.5 * (means[k] + means[k + 1]);
        if (isInteger) {
            // Integer input: MATLAB returns thresholds in the input's
            // native integer range, truncated (uint8 floor() of the
            // mean midpoint).
            td[k] = std::floor(midpoint);
        } else {
            // Floating-point input: normalise back to [0, 1].
            td[k] = midpoint / double(L - 1);
        }
    }

    // Effectiveness η = sigma_b^2 / sigma_T^2.
    const double mu = sum_total / total;
    double total_var = 0.0;
    for (int i = 0; i < L; ++i) total_var += counts[i] * (i - mu) * (i - mu);
    const double sigma_b2 = best_var - total * mu * mu;
    const double em = (total_var > 0.0) ? (sigma_b2 / total_var) : 0.0;
    return std::make_tuple(std::move(t_out), Value::scalar(em, mr));
}

Value imbinarize(const Value &I, double thresh, std::pmr::memory_resource *mr) {
    const size_t N = I.numel();
    Value out;
    const auto &d = I.dims();
    if (I.isScalar()) out = Value::matrix(1, 1, ValueType::LOGICAL, mr);
    else if (d.is3D())out = Value::matrix3d(d.rows(), d.cols(), d.pages(),
                                           ValueType::LOGICAL, mr);
    else              out = Value::matrix(d.rows(), d.cols(),
                                          ValueType::LOGICAL, mr);
    if (N == 0) return out;
    uint8_t *od = out.logicalDataMut();
    for (size_t i = 0; i < N; ++i) od[i] = (element_to_unit(I, i) > thresh) ? 1 : 0;
    return out;
}

// Per-pixel-threshold variant of imbinarize. T must have the same
// numel as I (typically the same H × W matrix; H × W × 3 also OK
// when I is a same-shape volume). T's element scale is interpreted
// in the same `element_to_unit` space as I, so the raw element
// values are directly comparable. Composes with `adaptthresh`.
Value imbinarize(const Value &I, const Value &T, std::pmr::memory_resource *mr)
{
    if (T.numel() != I.numel())
        throw Error("imbinarize: per-pixel T must have the same number "
                    "of elements as I",
                    0, 0, "imbinarize", "", "numkit:imbinarize:Tshape");
    const size_t N = I.numel();
    Value out;
    const auto &d = I.dims();
    if (I.isScalar()) out = Value::matrix(1, 1, ValueType::LOGICAL, mr);
    else if (d.is3D())out = Value::matrix3d(d.rows(), d.cols(), d.pages(),
                                           ValueType::LOGICAL, mr);
    else              out = Value::matrix(d.rows(), d.cols(),
                                          ValueType::LOGICAL, mr);
    if (N == 0) return out;
    uint8_t *od = out.logicalDataMut();
    for (size_t i = 0; i < N; ++i) {
        const double t = T.elemAsDouble(i);
        od[i] = (element_to_unit(I, i) > t) ? 1 : 0;
    }
    return out;
}

Value imquantize(const Value &I, const Value &levels, std::pmr::memory_resource *mr) {
    const size_t Lcount = levels.numel();
    std::vector<double> lv(Lcount);
    for (size_t i = 0; i < Lcount; ++i) lv[i] = levels.elemAsDouble(i);
    std::sort(lv.begin(), lv.end());

    const size_t N = I.numel();
    Value out;
    const auto &d = I.dims();
    if (I.isScalar()) out = Value::matrix(1, 1, ValueType::DOUBLE, mr);
    else if (d.is3D())out = Value::matrix3d(d.rows(), d.cols(), d.pages(),
                                           ValueType::DOUBLE, mr);
    else              out = Value::matrix(d.rows(), d.cols(), ValueType::DOUBLE, mr);
    if (N == 0) return out;

    double *od = out.doubleDataMut();
    for (size_t i = 0; i < N; ++i) {
        const double u = element_to_unit(I, i);
        // class index = first level we don't exceed; output is 1-indexed.
        size_t cls = 1;
        for (size_t k = 0; k < Lcount; ++k) {
            if (u <= lv[k]) break;
            ++cls;
        }
        od[i] = double(cls);
    }
    return out;
}

// ════════════════════════════════════════════════════════════════════
// imhistmatch — CDF-matching to a reference histogram
// ════════════════════════════════════════════════════════════════════
//
// Recipe (single-channel, MATLAB-canonical):
//   1. Compute nbins-bin normalised histograms of I and ref in [0, 1].
//   2. Build CDFs: cdfI[k] = sum(histI[0..k]) / N_I, similarly cdfR.
//   3. For each input bin b, the output normalised intensity is
//        LUT[b] = first k such that cdfR[k] >= cdfI[b]
//      then mapped to a representative value (k + 0.5)/nbins ∈ [0, 1].
//   4. Apply LUT to every pixel of I; cast back to the input class.
// Default nbins: 256 for uint8, 65536 for uint16, 64 otherwise — same
// rule as the existing default_nbins() helper.

Value imhistmatch(const Value &I, const Value &ref, int nbins, std::pmr::memory_resource *mr)
{
    if (nbins <= 0) nbins = std::max(default_nbins(I), default_nbins(ref));
    if (nbins < 2) nbins = 2;

    const size_t Ni = I.numel();
    const size_t Nr = ref.numel();
    if (Ni == 0 || Nr == 0)
        return Value::matrix(I.dims().rows(), I.dims().cols(), I.type(), mr);

    auto bin_index = [&](double u) {
        int b = (int)std::floor(u * nbins);
        if (b < 0) b = 0;
        if (b >= nbins) b = nbins - 1;
        return b;
    };

    // Histograms.
    std::vector<size_t> hI((size_t)nbins, 0), hR((size_t)nbins, 0);
    for (size_t i = 0; i < Ni; ++i) hI[bin_index(element_to_unit(I, i))]++;
    for (size_t i = 0; i < Nr; ++i) hR[bin_index(element_to_unit(ref, i))]++;

    // CDFs (normalised).
    std::vector<double> cI((size_t)nbins), cR((size_t)nbins);
    {
        size_t accI = 0, accR = 0;
        for (int b = 0; b < nbins; ++b) {
            accI += hI[(size_t)b];
            accR += hR[(size_t)b];
            cI[(size_t)b] = (double)accI / (double)Ni;
            cR[(size_t)b] = (double)accR / (double)Nr;
        }
    }

    // Build LUT[b] = smallest k with cR[k] ≥ cI[b], expressed as a
    // unit-range double. Walk both monotone arrays in O(nbins).
    std::vector<double> LUT((size_t)nbins, 0.0);
    int k = 0;
    for (int b = 0; b < nbins; ++b) {
        while (k + 1 < nbins && cR[(size_t)k] < cI[(size_t)b]) ++k;
        LUT[(size_t)b] = (k + 0.5) / (double)nbins;
    }

    // Apply: preserve input shape (2-D or 3-D volume).
    const auto &d = I.dims();
    Value out;
    if (d.is3D()) out = Value::matrix3d(d.rows(), d.cols(), d.pages(),
                                        I.type(), mr);
    else          out = Value::matrix(d.rows(), d.cols(), I.type(), mr);
    for (size_t i = 0; i < Ni; ++i) {
        const double u = element_to_unit(I, i);
        const double v = LUT[(size_t)bin_index(u)];
        store_classed(out, i, v, I.type());
    }
    return out;
}

// ════════════════════════════════════════════════════════════════════
// adaptthresh — locally adaptive threshold matrix
// ════════════════════════════════════════════════════════════════════
//
// Computes T(x, y) ∈ [0, 1] from a local statistic (box mean or
// Gaussian-smoothed mean) of a neighborhood centered at each pixel.
// `sensitivity` shifts the threshold above/below the local statistic:
//   sensitivity = 0.5 → threshold equals the local statistic
//   higher        → threshold lowered (more foreground after binarize)
//   lower         → threshold raised (less foreground)
// The shift offset chosen here is (0.5 − sensitivity) · 0.1 — a
// modest bias that empirically tracks MATLAB's behaviour on natural
// imagery without needing the proprietary scale-factor.

Value adaptthresh(const Value &I, double sensitivity, int neighborhood, const std::string &statistic, std::pmr::memory_resource *mr)
{
    if (!(sensitivity >= 0.0 && sensitivity <= 1.0))
        throw Error("adaptthresh: sensitivity must be in [0, 1]",
                    0, 0, "adaptthresh", "", "numkit:adaptthresh:sens");

    const int H = static_cast<int>(I.dims().rows());
    const int W = static_cast<int>(I.dims().cols());

    if (neighborhood <= 0) {
        const int dim = std::min(H, W);
        neighborhood = 2 * (dim / 16) + 1;
        if (neighborhood < 3) neighborhood = 3;
    }
    if ((neighborhood & 1) == 0) neighborhood += 1;  // force odd

    // First, normalise input intensity to [0, 1] in a double buffer.
    // We replicate the scaling rule used elsewhere in numkit:
    //   uint8/int8 / 255, uint16 / 65535, double passes through.
    Value Inorm = Value::matrix(static_cast<size_t>(H),
                                static_cast<size_t>(W),
                                ValueType::DOUBLE, mr);
    double *nd = Inorm.doubleDataMut();
    const ValueType srcT = I.type();
    for (int c = 0; c < W; ++c)
        for (int r = 0; r < H; ++r) {
            const size_t i = static_cast<size_t>(c) *
                                  static_cast<size_t>(H) +
                              static_cast<size_t>(r);
            const double v = I.elemAsDouble(i);
            double w = v;
            switch (srcT) {
                case ValueType::UINT8:  w = v / 255.0;   break;
                case ValueType::UINT16: w = v / 65535.0; break;
                case ValueType::INT16:  w = (v + 32768.0) / 65535.0; break;
                default:                w = v;            break;
            }
            nd[i] = w;
        }

    // Compute local statistic.
    Value localStat;
    const std::string s = statistic.empty() ? std::string("mean")
                                            : statistic;
    if (s == "mean" || s == "Mean" || s == "MEAN") {
        localStat = imboxfilt(Inorm, neighborhood, mr);
    } else if (s == "gaussian" || s == "Gaussian" || s == "GAUSSIAN") {
        // σ ≈ neighborhood/6: typical MATLAB default for adaptthresh's
        // Gaussian variant.
        const double sigma = double(neighborhood) / 6.0;
        localStat = imgaussfilt(Inorm, sigma, neighborhood, mr);
    } else {
        throw Error("adaptthresh: statistic must be 'mean' or 'gaussian'",
                    0, 0, "adaptthresh", "", "numkit:adaptthresh:stat");
    }

    // Apply sensitivity shift.
    Value T = Value::matrix(static_cast<size_t>(H),
                            static_cast<size_t>(W),
                            ValueType::DOUBLE, mr);
    double *Td = T.doubleDataMut();
    const double bias = (0.5 - sensitivity) * 0.1;
    for (int i = 0; i < H * W; ++i) {
        double v = localStat.elemAsDouble(static_cast<size_t>(i)) + bias;
        if (v < 0.0) v = 0.0;
        if (v > 1.0) v = 1.0;
        Td[i] = v;
    }
    return T;
}

Value imflatfield(const Value &I, double sigma, const Value &mask, std::pmr::memory_resource *mr)
{
    const ValueType classin = I.type();
    const auto &d = I.dims();
    const size_t H = d.rows();
    const size_t W = d.cols();
    const size_t pages = d.is3D() ? d.pages() : 1;
    const size_t plane = H * W;
    const size_t N = I.numel();

    // Convert source to double in [0, 1] (im2double-like).
    Value Idbl;
    if (d.is3D())  Idbl = Value::matrix3d(H, W, pages, ValueType::DOUBLE, mr);
    else           Idbl = Value::matrix(H, W, ValueType::DOUBLE, mr);
    double *idd = Idbl.doubleDataMut();
    for (size_t i = 0; i < N; ++i) idd[i] = element_to_unit(I, i);

    // Gaussian low-pass per plane. MATLAB R2025b imflatfield uses
    // symmetric padding internally — NOT the imgaussfilt default
    // (replicate). For sigma=8 on a 32×32 image the kernel (size 33)
    // exceeds the image, so the boundary mode dominates the result.
    // Build the kernel via fspecial("gaussian") and apply with
    // PadMode::Symmetric directly via imfilter.
    int filter_size = 2 * static_cast<int>(std::ceil(2.0 * sigma)) + 1;
    if (filter_size < 3) filter_size = 3;
    // fspecial("gaussian", {rows, cols, sigma}) — three-param form
    Value gk = fspecial("gaussian", {static_cast<double>(filter_size), static_cast<double>(filter_size), sigma}, mr);

    Value F;
    if (d.is3D())  F = Value::matrix3d(H, W, pages, ValueType::DOUBLE, mr);
    else           F = Value::matrix(H, W, ValueType::DOUBLE, mr);
    double *fd = F.doubleDataMut();
    for (size_t p = 0; p < pages; ++p) {
        Value plane2d;
        if (pages == 1) {
            plane2d = Idbl;
        } else {
            plane2d = Value::matrix(H, W, ValueType::DOUBLE, mr);
            std::memcpy(plane2d.doubleDataMut(), idd + p * plane,
                        plane * sizeof(double));
        }
        Value blurred = imfilter(plane2d, gk, PadMode::Symmetric, 0.0, /*full=*/false, /*flip_kernel=*/false, mr);
        std::memcpy(fd + p * plane, blurred.doubleData(),
                    plane * sizeof(double));
    }

    // Per-page mean of A (NOT F) — MATLAB normalizes by mean(A).
    // mean(F) ≈ mean(A) for full-image mean (Gaussian preserves mean),
    // but with a mask the difference matters.
    const bool have_mask = (mask.numel() > 0);
    if (have_mask && (mask.dims().rows() != H || mask.dims().cols() != W))
        throw Error("imflatfield: mask must match spatial dims of I",
                    0, 0, "imflatfield", "", "numkit:imflatfield:masksize");
    std::vector<double> meanA(pages, 0.0);
    for (size_t p = 0; p < pages; ++p) {
        const double *ap = idd + p * plane;
        if (have_mask) {
            size_t cnt = 0;
            double acc = 0.0;
            for (size_t i = 0; i < plane; ++i)
                if (mask.elemAsDouble(i) != 0.0) { acc += ap[i]; ++cnt; }
            meanA[p] = (cnt > 0) ? acc / static_cast<double>(cnt) : 0.0;
        } else if (plane > 0) {
            double acc = 0.0;
            for (size_t i = 0; i < plane; ++i) acc += ap[i];
            meanA[p] = acc / static_cast<double>(plane);
        }
    }

    // Output: per-page A * meanA / F, with eps guard for degenerate F.
    Value out;
    if (d.is3D())  out = Value::matrix3d(H, W, pages, classin, mr);
    else           out = Value::matrix(H, W, classin, mr);
    constexpr double EPS = 1e-12;
    for (size_t p = 0; p < pages; ++p) {
        const double mA = meanA[p];
        for (size_t i = 0; i < plane; ++i) {
            const size_t idx = p * plane + i;
            const double f = fd[idx];
            const double v = (f > EPS) ? idd[idx] * mA / f : idd[idx];
            store_classed(out, idx, v, classin);
        }
    }
    return out;
}

Value wcodemat(const Value &X, int nb, const std::string &opt, int absol, std::pmr::memory_resource *mr)
{
    if (nb < 1)
        throw Error("wcodemat: NB must be a positive integer",
                    0, 0, "wcodemat", "", "numkit:wcodemat:nb");

    const auto &d = X.dims();
    const size_t H = d.rows();
    const size_t W = d.cols();
    Value out = d.is3D()
        ? Value::matrix3d(H, W, d.pages(), ValueType::DOUBLE, mr)
        : Value::matrix(H, W, ValueType::DOUBLE, mr);
    const size_t N = X.numel();
    if (N == 0) return out;
    double *od = out.doubleDataMut();

    auto vget = [&](size_t i) {
        const double v = X.elemAsDouble(i);
        return absol ? std::abs(v) : v;
    };

    std::string lo;
    lo.reserve(opt.size());
    for (char c : opt) lo.push_back(static_cast<char>(std::tolower(c)));
    if (lo.empty()) lo = "mat";

    // MATLAB R2025b formula: y = floor((v - mn) / span * nb) + 1, with the
    // upper edge (v == mx) clamped from nb+1 down to nb.
    // Bug fix 2026-05-08: previous impl used `round` and multiplied by
    // `nb - 1`, producing off-by-one quantization errors on interior values
    // (e.g., wcodemat([1 -2 3; 4 -5 6], 4) → numkit [1 2 2; 3 3 4] vs
    //  MATLAB [1 1 2; 3 4 4]).
    auto encode = [&](double v, double mn, double mx) {
        const double span = mx - mn;
        if (span == 0.0) return 1.0;
        double y = std::floor((v - mn) / span * static_cast<double>(nb)) + 1.0;
        if (y < 1.0) y = 1.0;
        if (y > nb)  y = static_cast<double>(nb);
        return y;
    };

    if (lo == "mat") {
        double mn =  std::numeric_limits<double>::infinity();
        double mx = -std::numeric_limits<double>::infinity();
        for (size_t i = 0; i < N; ++i) {
            const double v = vget(i);
            if (v < mn) mn = v;
            if (v > mx) mx = v;
        }
        for (size_t i = 0; i < N; ++i)
            od[i] = encode(vget(i), mn, mx);
    } else if (lo == "row") {
        // Per-row scaling. col-major: row r elements at i = c*H + r.
        for (size_t r = 0; r < H; ++r) {
            double mn =  std::numeric_limits<double>::infinity();
            double mx = -std::numeric_limits<double>::infinity();
            for (size_t c = 0; c < W; ++c) {
                const double v = vget(c * H + r);
                if (v < mn) mn = v;
                if (v > mx) mx = v;
            }
            for (size_t c = 0; c < W; ++c)
                od[c * H + r] = encode(vget(c * H + r), mn, mx);
        }
    } else if (lo == "col") {
        for (size_t c = 0; c < W; ++c) {
            double mn =  std::numeric_limits<double>::infinity();
            double mx = -std::numeric_limits<double>::infinity();
            for (size_t r = 0; r < H; ++r) {
                const double v = vget(c * H + r);
                if (v < mn) mn = v;
                if (v > mx) mx = v;
            }
            for (size_t r = 0; r < H; ++r)
                od[c * H + r] = encode(vget(c * H + r), mn, mx);
        }
    } else {
        throw Error("wcodemat: opt must be 'mat', 'row', or 'col'",
                    0, 0, "wcodemat", "", "numkit:wcodemat:opt");
    }
    return out;
}

Value entropy(const Value &I, int nbins, std::pmr::memory_resource *mr)
{
    const ValueType ct = I.type();
    const bool isLogical = (ct == ValueType::LOGICAL);
    if (nbins <= 0) nbins = isLogical ? 2 : 256;

    Value Iu = isLogical ? I : im2uint8(I, mr);
    auto [counts, _bins] = imhist(Iu, nbins, mr);
    const double *cd = counts.doubleData();

    double total = 0.0;
    for (int i = 0; i < nbins; ++i) total += cd[i];
    if (total <= 0.0) return Value::scalar(0.0, mr);

    double H = 0.0;
    for (int i = 0; i < nbins; ++i) {
        if (cd[i] <= 0.0) continue;
        const double p = cd[i] / total;
        H -= p * std::log2(p);
    }
    return Value::scalar(H, mr);
}

namespace {
// Common quantisation tail: given a sorted threshold vector and the
// resulting level count, build the output indexed image.
Value graysliceQuantize(const Value &I, const std::vector<double> &thresh,
                        size_t n_levels, std::pmr::memory_resource *mr)
{
    const size_t N = I.numel();
    const ValueType outT = (n_levels < 256) ? ValueType::UINT8
                                            : ValueType::DOUBLE;
    const auto &d = I.dims();
    Value out;
    if (d.is3D()) out = Value::matrix3d(d.rows(), d.cols(), d.pages(),
                                        outT, mr);
    else          out = Value::matrix(d.rows(), d.cols(), outT, mr);
    if (N == 0) return out;
    const bool baseOne = (outT == ValueType::DOUBLE);
    for (size_t i = 0; i < N; ++i) {
        const double iv = I.elemAsDouble(i);
        size_t cnt = 0;
        for (size_t k = 0; k < thresh.size(); ++k) {
            if (thresh[k] <= iv) ++cnt;
            else                 break;
        }
        if (outT == ValueType::UINT8) {
            const size_t v = cnt > 255 ? 255 : cnt;
            out.uint8DataMut()[i] = static_cast<uint8_t>(v);
        } else {
            out.doubleDataMut()[i] = static_cast<double>(cnt) +
                                     (baseOne ? 1.0 : 0.0);
        }
    }
    return out;
}
} // anon

Value grayslice(const Value &I, int N, std::pmr::memory_resource *mr)
{
    if (N < 1)
        throw Error("grayslice: N must be a positive integer",
                    0, 0, "grayslice", "", "numkit:grayslice:n");
    const ValueType ct = I.type();
    const bool isInt16 = (ct == ValueType::INT16);
    const bool isFloat = (ct == ValueType::DOUBLE || ct == ValueType::SINGLE);
    const double n_scalar = static_cast<double>(N);
    const size_t k_max = static_cast<size_t>(N - 1);
    std::vector<double> thresh;
    thresh.reserve(k_max);
    const double scale =
          (ct == ValueType::UINT8)  ? 255.0
        : (ct == ValueType::UINT16) ? 65535.0
        : isInt16                   ? 65535.0
        : 1.0;
    for (size_t k = 1; k <= k_max; ++k) {
        const double v_unit = static_cast<double>(k) / n_scalar;
        double v_class;
        if (isFloat) v_class = v_unit;
        else {
            v_class = std::round(v_unit * scale);
            if (isInt16) v_class -= 32768.0;
        }
        thresh.push_back(v_class);
    }
    return graysliceQuantize(I, thresh, k_max + 1, mr);
}

Value grayslice(const Value &I, Span<const double> levels,
                std::pmr::memory_resource *mr)
{
    const ValueType ct = I.type();
    const bool isFloat = (ct == ValueType::DOUBLE || ct == ValueType::SINGLE);
    const size_t M = levels.size();
    std::vector<double> thresh(levels.begin(), levels.end());
    std::sort(thresh.begin(), thresh.end());
    const size_t N = I.numel();
    if (isFloat && N > 0) {
        double imin =  std::numeric_limits<double>::infinity();
        double imax = -std::numeric_limits<double>::infinity();
        for (size_t i = 0; i < N; ++i) {
            const double v = I.elemAsDouble(i);
            if (v < imin) imin = v;
            if (v > imax) imax = v;
        }
        for (size_t i = 0; i < M; ++i) {
            if (thresh[i] < imin) thresh[i] = imin;
            if (thresh[i] > imax) thresh[i] = imax;
        }
    }
    return graysliceQuantize(I, thresh, M + 1, mr);
}

// ════════════════════════════════════════════════════════════════════
// Engine adapters
// ════════════════════════════════════════════════════════════════════

namespace detail {

void imhistmatch_reg(Span<const Value> args, size_t /*nargout*/,
                     Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("imhistmatch: requires (I, ref [, nbins])",
                    0, 0, "imhistmatch", "", "numkit:imhistmatch:nargin");
    int n = (args.size() >= 3 && !args[2].isEmpty())
            ? (int)args[2].toScalar() : 0;
    outs[0] = imhistmatch(args[0], args[1], n, ctx.engine->resource());
}

void imhist_reg(Span<const Value> args, size_t nargout,
                Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("imhist: requires (I[, n])", 0, 0, "imhist", "",
                    "numkit:imhist:nargin");
    int n = (args.size() >= 2 && !args[1].isEmpty()) ? (int)args[1].toScalar() : 0;
    auto [c, x] = imhist(args[0], n, ctx.engine->resource());

    // Auto-plot when called without LHS — MATLAB convention.
    // imhist(I) draws a vertical bar chart of the bin counts.
    if (nargout == 0) {
        auto &fm = ctx.engine->figureManager();
        fm.prepareForPlot();
        const std::size_t nb = c.numel();
        std::ostringstream xs, ys;
        xs << '['; ys << '[';
        for (std::size_t i = 0; i < nb; ++i) {
            if (i) { xs << ','; ys << ','; }
            xs << x.elemAsDouble(i);
            ys << c.elemAsDouble(i);
        }
        xs << ']'; ys << ']';
        DatasetInfo ds;
        ds.type  = "bar";
        ds.xJson = xs.str();
        ds.yJson = ys.str();
        ds.style = "color=#7fa6c6";
        fm.pushDataset(std::move(ds));
        fm.emitModified();
        return;
    }

    if (nargout > 0) outs[0] = std::move(c);
    if (nargout > 1) outs[1] = std::move(x);
}

void stretchlim_reg(Span<const Value> args, size_t /*nargout*/,
                    Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("stretchlim: requires (I[, tol])", 0, 0, "stretchlim", "",
                    "numkit:stretchlim:nargin");
    double lo = 0.01, hi = 0.99;
    if (args.size() >= 2 && !args[1].isEmpty()) {
        const Value &t = args[1];
        if (t.numel() == 1) {
            lo = t.toScalar();
            hi = 1.0 - lo;
        } else if (t.numel() >= 2) {
            lo = t.elemAsDouble(0);
            hi = t.elemAsDouble(1);
        }
    }
    outs[0] = stretchlim(args[0], lo, hi, ctx.engine->resource());
}

void imadjust_reg(Span<const Value> args, size_t /*nargout*/,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("imadjust: requires (I[, [low_in high_in]][, [low_out high_out]][, gamma])",
                    0, 0, "imadjust", "", "numkit:imadjust:nargin");
    double low_in  = std::numeric_limits<double>::quiet_NaN();
    double high_in = std::numeric_limits<double>::quiet_NaN();
    double low_out = 0.0;
    double high_out = 1.0;
    double gamma = 1.0;

    if (args.size() >= 2 && !args[1].isEmpty()) {
        const Value &v = args[1];
        if (v.numel() >= 2) {
            low_in  = v.elemAsDouble(0);
            high_in = v.elemAsDouble(1);
        }
    }
    if (args.size() >= 3 && !args[2].isEmpty()) {
        const Value &v = args[2];
        if (v.numel() >= 2) {
            low_out  = v.elemAsDouble(0);
            high_out = v.elemAsDouble(1);
        }
    }
    if (args.size() >= 4 && !args[3].isEmpty()) gamma = args[3].toScalar();

    outs[0] = imadjust(args[0], low_in, high_in, low_out, high_out, gamma, ctx.engine->resource());
}

void histeq_reg(Span<const Value> args, size_t /*nargout*/,
                Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("histeq: requires (I[, n])", 0, 0, "histeq", "",
                    "numkit:histeq:nargin");
    int n = (args.size() >= 2 && !args[1].isEmpty()) ? (int)args[1].toScalar() : 64;
    outs[0] = histeq(args[0], n, ctx.engine->resource());
}

void adapthisteq_reg(Span<const Value> args, size_t /*nargout*/,
                     Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("adapthisteq: requires (I[, NV-pairs...])",
                     0, 0, "adapthisteq", "", "numkit:adapthisteq:nargin");

    AdaptHistEqOptions opts;

    auto eqIgnoreCase = [](const std::string &a, const char *b) {
        if (a.size() != std::strlen(b)) return false;
        for (size_t i = 0; i < a.size(); ++i)
            if (std::tolower(a[i]) != std::tolower(b[i])) return false;
        return true;
    };
    for (size_t i = 1; i + 1 < args.size(); i += 2) {
        if (!args[i].isChar())
            throw Error("adapthisteq: NV-pair name must be a string",
                         0, 0, "adapthisteq", "", "numkit:adapthisteq:badNVName");
        const std::string key = args[i].toString();
        const Value &v        = args[i + 1];
        if (eqIgnoreCase(key, "NumTiles")) {
            if (v.numel() < 2)
                throw Error("adapthisteq: NumTiles must be 2-element",
                             0, 0, "adapthisteq", "", "numkit:adapthisteq:badNumTiles");
            opts.numTilesR = (int)v.elemAsDouble(0);
            opts.numTilesC = (int)v.elemAsDouble(1);
        } else if (eqIgnoreCase(key, "ClipLimit"))    opts.clipLimit    = v.toScalar();
        else if (eqIgnoreCase(key, "NBins"))          opts.nBins        = (int)v.toScalar();
        else if (eqIgnoreCase(key, "Distribution"))   opts.distribution = v.toString();
        else if (eqIgnoreCase(key, "Alpha"))          opts.alpha        = v.toScalar();
        else if (eqIgnoreCase(key, "Range"))          opts.range        = v.toString();
        else {
            throw Error("adapthisteq: unknown NV-pair key '" + key + "'",
                         0, 0, "adapthisteq", "", "numkit:adapthisteq:badNVKey");
        }
    }
    outs[0] = adapthisteq(args[0], opts, ctx.engine->resource());
}

void graythresh_reg(Span<const Value> args, size_t nargout,
                    Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("graythresh: requires I", 0, 0, "graythresh", "",
                    "numkit:graythresh:nargin");
    auto [t, em] = graythresh(args[0], ctx.engine->resource());
    outs[0] = std::move(t);
    if (nargout > 1) outs[1] = std::move(em);
}

void otsuthresh_reg(Span<const Value> args, size_t nargout,
                    Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("otsuthresh: requires counts", 0, 0, "otsuthresh", "",
                    "numkit:otsuthresh:nargin");
    auto [t, em] = otsuthresh(args[0], ctx.engine->resource());
    outs[0] = std::move(t);
    if (nargout > 1) outs[1] = std::move(em);
}

void multithresh_reg(Span<const Value> args, size_t nargout,
                     Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("multithresh: requires (I[, N])", 0, 0, "multithresh", "",
                    "numkit:multithresh:nargin");
    int N = (args.size() >= 2 && !args[1].isEmpty()) ? (int)args[1].toScalar() : 1;
    auto [t, em] = multithresh(args[0], N, ctx.engine->resource());
    outs[0] = std::move(t);
    if (nargout > 1) outs[1] = std::move(em);
}

void imbinarize_reg(Span<const Value> args, size_t /*nargout*/,
                    Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("imbinarize: requires (I[, thresh])", 0, 0, "imbinarize", "",
                    "numkit:imbinarize:nargin");
    auto *mr = ctx.engine->resource();
    if (args.size() >= 2 && !args[1].isEmpty()) {
        // Dispatch on T's shape: scalar → fast path, matrix → per-pixel.
        if (args[1].numel() == 1) {
            outs[0] = imbinarize(args[0], args[1].toScalar(), mr);
        } else {
            outs[0] = imbinarize(args[0], args[1], mr);
        }
    } else {
        // No threshold given: pick Otsu's automatically.
        auto [t, _] = graythresh(args[0], mr);
        outs[0] = imbinarize(args[0], t.toScalar(), mr);
    }
}

void imquantize_reg(Span<const Value> args, size_t /*nargout*/,
                    Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("imquantize: requires (I, levels)", 0, 0, "imquantize", "",
                    "numkit:imquantize:nargin");
    outs[0] = imquantize(args[0], args[1], ctx.engine->resource());
}

void adaptthresh_reg(Span<const Value> args, size_t /*nargout*/,
                     Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("adaptthresh: requires (I [, sensitivity [, n [, stat]]])",
                    0, 0, "adaptthresh", "", "numkit:adaptthresh:nargin");
    const double sens = (args.size() >= 2 && !args[1].isEmpty())
                        ? args[1].toScalar() : 0.5;
    const int nbh     = (args.size() >= 3 && !args[2].isEmpty())
                        ? static_cast<int>(args[2].toScalar()) : 0;
    std::string stat  = "mean";
    if (args.size() >= 4 && !args[3].isEmpty()) {
        if (!args[3].isChar() && !args[3].isString())
            throw Error("adaptthresh: statistic must be a string",
                        0, 0, "adaptthresh", "", "numkit:adaptthresh:type");
        stat = args[3].toString();
    }
    outs[0] = adaptthresh(args[0], sens, nbh, stat, ctx.engine->resource());
}

void imflatfield_reg(Span<const Value> args, size_t /*nargout*/,
                     Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("imflatfield: requires (I, sigma [, mask])",
                    0, 0, "imflatfield", "", "numkit:imflatfield:nargin");
    const double sigma = args[1].toScalar();
    Value mask;
    if (args.size() >= 3 && !args[2].isEmpty()) mask = args[2];
    outs[0] = imflatfield(args[0], sigma, mask, ctx.engine->resource());
}

void wcodemat_reg(Span<const Value> args, size_t /*nargout*/,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("wcodemat: requires (X [, nb [, opt [, absol]]])",
                    0, 0, "wcodemat", "", "numkit:wcodemat:nargin");
    int nb = (args.size() >= 2 && !args[1].isEmpty())
             ? static_cast<int>(args[1].toScalar()) : 16;
    std::string opt = "mat";
    if (args.size() >= 3 && !args[2].isEmpty()) {
        if (!args[2].isChar() && !args[2].isString())
            throw Error("wcodemat: opt must be a string",
                        0, 0, "wcodemat", "", "numkit:wcodemat:opt");
        opt = args[2].toString();
    }
    int absol = (args.size() >= 4 && !args[3].isEmpty())
                ? static_cast<int>(args[3].toScalar()) : 1;
    outs[0] = wcodemat(args[0], nb, opt, absol, ctx.engine->resource());
}

void entropy_reg(Span<const Value> args, size_t /*nargout*/,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("entropy: requires (I [, nbins])", 0, 0, "entropy", "",
                    "numkit:entropy:nargin");
    int nbins = 0;
    if (args.size() >= 2 && !args[1].isEmpty())
        nbins = static_cast<int>(args[1].toScalar());
    outs[0] = entropy(args[0], nbins, ctx.engine->resource());
}

void grayslice_reg(Span<const Value> args, size_t /*nargout*/,
                   Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("grayslice: requires (I [, n])", 0, 0, "grayslice", "",
                    "numkit:grayslice:nargin");
    auto *mr = ctx.engine->resource();
    // Decide which overload to dispatch on, mirroring MATLAB's
    // magic-polymorphism of grayslice's 2nd argument.
    //   - missing / empty         → default 10 levels
    //   - scalar ≥ 1              → level-count overload
    //   - vector or scalar 0<n<1  → explicit-thresholds overload
    if (args.size() < 2 || args[1].isEmpty()) {
        outs[0] = grayslice(args[0], 10, mr);
        return;
    }
    const Value &n = args[1];
    if (n.numel() == 1) {
        const double nv = n.toScalar();
        if (nv >= 1.0) {
            outs[0] = grayslice(args[0], static_cast<int>(nv), mr);
        } else if (nv > 0.0) {
            const double levels[1] = { nv };
            outs[0] = grayslice(args[0], Span<const double>(levels, 1), mr);
        } else {
            throw Error("grayslice: N must be a positive number",
                        0, 0, "grayslice", "", "numkit:grayslice:n");
        }
    } else if (n.numel() > 1) {
        ScratchArena scratch(mr);
        ScratchVec<double> buf(n.numel(), &scratch);
        for (size_t i = 0; i < n.numel(); ++i) buf[i] = n.elemAsDouble(i);
        outs[0] = grayslice(args[0], Span<const double>(buf.data(), buf.size()),
                            mr);
    } else {
        throw Error("grayslice: N must be scalar >= 1 or a vector",
                    0, 0, "grayslice", "", "numkit:grayslice:nargin");
    }
}

} // namespace detail
} // namespace numkit::image
