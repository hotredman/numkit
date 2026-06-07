// libs/image/src/contrast/contrast.cpp

#include <numkit/image/contrast/contrast.hpp>
#include <numkit/image/filter/filter.hpp>
#include <numkit/image/type_convert/type_convert.hpp>

#include <numkit/value/value.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/value_type.hpp>

#include <algorithm>
#include <cctype>
#include <tuple>
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
// Clean-room implementation of CLAHE.
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
// MATLAB algorithm (from graythresh.m):
//   idx = mean(find(sigma_b_squared == max(sigma_b_squared)));
// then level = (idx - 1) / (num_bins - 1).
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
    // MATLAB graythresh always builds a 256-bin histogram (NPTS=256),
    // independent of the input class. numkit's default_nbins gives 64 for
    // floating-point and 65536 for uint16, which produced a coarser/finer
    // Otsu level than MATLAB for those classes (uint8 already used 256).
    auto [counts, _] = imhist(I, 256, mr);
    return otsuthresh(counts, mr);
}

std::tuple<Value, Value>
multithresh(const Value &I, int N, std::pmr::memory_resource *mr) {
    if (N < 1) N = 1;
    if (N > 5)
        throw Error("multithresh: N > 5 not supported (exhaustive search would be too slow)",
                    0, 0, "multithresh", "", "numkit:multithresh:tooMany");

    // ── getpdf: 256-bin histogram over the data range, MATLAB-style ──
    // Floating-point data is scaled to [0,1] by (x-minA)/(maxA-minA) then
    // quantised to uint8 (grayto8); integer data is mapped from its type
    // range to uint8. minA/maxA are the finite data extrema; the final
    // thresholds are mapped back with map2OriginalScale (normFactor 255).
    const int B = 256;
    const ValueType ty = I.type();
    const bool isInt = (ty == ValueType::UINT8 || ty == ValueType::UINT16
                     || ty == ValueType::INT16 || ty == ValueType::INT8
                     || ty == ValueType::LOGICAL);
    const size_t M = I.numel();

    double minA = 0.0, maxA = 0.0;
    bool anyFinite = false;
    for (size_t i = 0; i < M; ++i) {
        const double x = I.elemAsDouble(i);
        if (std::isnan(x) || !std::isfinite(x)) continue;
        if (!anyFinite) { minA = maxA = x; anyFinite = true; }
        else { if (x < minA) minA = x; if (x > maxA) maxA = x; }
    }

    auto clampBin = [](long b) -> int {
        if (b < 0) return 0;
        if (b > 255) return 255;
        return static_cast<int>(b);
    };
    // getpdf scales the data to [0,1] by (x-minA)/(maxA-minA) and quantises
    // to uint8. Both float and integer data are scaled this way; the only
    // difference is that MATLAB does the integer scaling in single precision.
    auto toBin = [&](double x) -> int {
        if (std::isnan(x)) return -1;
        if (maxA == minA) return 0;
        if (std::isinf(x)) return x > 0 ? 255 : 0;
        if (isInt) {
            // MATLAB scales + quantises integer data in single precision.
            const float fs = static_cast<float>(x - minA)
                           / static_cast<float>(maxA - minA);
            return clampBin(std::lround(static_cast<double>(fs * 255.0f)));
        }
        const double s = (x - minA) / (maxA - minA);
        return clampBin(std::lround(s * 255.0));
    };

    std::vector<double> counts(B, 0.0);
    double total = 0.0;
    for (size_t i = 0; i < M; ++i) {
        const int b = toBin(I.elemAsDouble(i));
        if (b >= 0) { counts[b] += 1.0; total += 1.0; }
    }

    // Degenerate input (no spread): MATLAB returns thresholds at the value.
    if (total <= 0.0 || maxA == minA) {
        Value t = Value::matrix(1, N, ValueType::DOUBLE, mr);
        double *td = t.doubleDataMut();
        for (int k = 0; k < N; ++k) td[k] = minA;
        return std::make_tuple(std::move(t), Value::scalar(0.0, mr));
    }

    // omega = cumsum(p); mu = cumsum(p .* (1:B)'); mu_t = mu(end).
    std::vector<double> omega(B), mu(B);
    double cum = 0.0, cumm = 0.0;
    for (int j = 0; j < B; ++j) {
        const double p = counts[j] / total;
        cum += p;
        cumm += p * (j + 1);
        omega[j] = cum;
        mu[j] = cumm;
    }
    const double mu_t = mu[B - 1];

    std::vector<double> threshBins(N, 0.0);   // 0-based bin thresholds
    double bestVar = 0.0;

    if (N == 1) {
        // sigma_b^2(t) = (mu_t*omega - mu)^2 / (omega*(1-omega)); arg-max bin.
        double best = -std::numeric_limits<double>::infinity();
        std::vector<int> ties;
        for (int t = 0; t < B; ++t) {
            const double o = omega[t];
            if (o <= 0.0 || o >= 1.0) continue;
            const double num = mu_t * o - mu[t];
            const double s = num * num / (o * (1.0 - o));
            if (!std::isfinite(s)) continue;
            if (s > best) { best = s; ties.clear(); ties.push_back(t); }
            else if (s == best) ties.push_back(t);
        }
        double m = 0.0;
        for (int t : ties) m += t;
        threshBins[0] = ties.empty() ? 0.0 : m / ties.size();
        bestVar = best;
    } else if (N == 2) {
        double best = -std::numeric_limits<double>::infinity();
        std::vector<std::pair<int, int>> ties;
        for (int r = 0; r < B; ++r) {
            const double o0 = omega[r];
            if (o0 <= 0.0) continue;
            const double mu0t = mu_t - mu[r] / o0;
            for (int c = r + 1; c < B; ++c) {
                const double o1 = omega[c] - omega[r];
                if (o1 <= 0.0) continue;
                const double mu1t = mu_t - (mu[c] - mu[r]) / o1;
                const double o2 = 1.0 - (o0 + o1);
                if (o2 <= 0.0) continue;
                const double term1 = o0 * mu0t * mu0t;
                const double term2 = o1 * mu1t * mu1t;
                const double q = o0 * mu0t + o1 * mu1t;
                const double term3 = q * q / o2;
                const double s = term1 + term2 + term3;
                if (!std::isfinite(s)) continue;
                if (s > best) { best = s; ties.clear(); ties.push_back({r, c}); }
                else if (s == best) ties.push_back({r, c});
            }
        }
        double sr = 0.0, sc = 0.0;
        for (auto &pr : ties) { sr += pr.first; sc += pr.second; }
        const double n = ties.empty() ? 1.0 : double(ties.size());
        threshBins[0] = sr / n;
        threshBins[1] = sc / n;
        bestVar = best;
    } else {
        // N >= 3: exhaustive over 256 bins is infeasible and MATLAB uses
        // fminsearch (a local optimiser). We instead solve the GLOBAL
        // multilevel-Otsu optimum by dynamic programming over the 256-bin
        // histogram (maximise sum_k s_k^2 / w_k). This is correct-scale and
        // a valid set of thresholds, but — being global rather than
        // fminsearch's local optimum — may differ from MATLAB for N >= 3.
        std::vector<double> Omega(B + 1, 0.0), Mu(B + 1, 0.0);
        for (int j = 0; j < B; ++j) {
            Omega[j + 1] = omega[j];
            Mu[j + 1] = mu[j];
        }
        auto segScore = [&](int lo, int hi) -> double {   // bins [lo,hi] 0-based
            const double w = Omega[hi + 1] - Omega[lo];
            if (w <= 0.0) return 0.0;
            const double s = Mu[hi + 1] - Mu[lo];
            return s * s / w;
        };
        const int K = N + 1;                              // number of classes
        const double NEG = -std::numeric_limits<double>::infinity();
        // dp[k][t] = best score for first k classes, class k ending at bin t.
        std::vector<std::vector<double>> dp(K + 1,
            std::vector<double>(B, NEG));
        std::vector<std::vector<int>> back(K + 1,
            std::vector<int>(B, -1));
        for (int t = 0; t < B; ++t) dp[1][t] = segScore(0, t);
        for (int k = 2; k <= K; ++k) {
            for (int t = k - 1; t < B; ++t) {
                double bestv = NEG; int bestp = -1;
                for (int tp = k - 2; tp < t; ++tp) {
                    if (dp[k - 1][tp] == NEG) continue;
                    const double v = dp[k - 1][tp] + segScore(tp + 1, t);
                    if (v > bestv) { bestv = v; bestp = tp; }
                }
                dp[k][t] = bestv;
                back[k][t] = bestp;
            }
        }
        // Last class must end at bin B-1; backtrack the N boundaries.
        std::vector<int> bounds(K, 0);
        bounds[K - 1] = B - 1;
        for (int k = K; k >= 2; --k)
            bounds[k - 2] = back[k][bounds[k - 1]];
        for (int k = 0; k < N; ++k) threshBins[k] = bounds[k];
        bestVar = dp[K][B - 1] - mu_t * mu_t;
    }

    // map2OriginalScale: minA + thresh/255 * (maxA - minA). For integer
    // input MATLAB casts the result back to the integer type (round).
    Value t_out = Value::matrix(1, N, ValueType::DOUBLE, mr);
    double *td = t_out.doubleDataMut();
    for (int k = 0; k < N; ++k) {
        double v = minA + threshBins[k] / 255.0 * (maxA - minA);
        if (isInt) v = std::round(v);
        td[k] = v;
    }

    // metric = maxval / sum(p .* ((1:B)' - mu_t)^2).
    double denom = 0.0;
    for (int j = 0; j < B; ++j) {
        const double p = counts[j] / total;
        const double d = (j + 1) - mu_t;
        denom += p * d * d;
    }
    const double em = (denom > 0.0 && std::isfinite(bestVar))
                          ? bestVar / denom : 0.0;
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
// imhistmatch — histogram matching to a reference image
// ════════════════════════════════════════════════════════════════════
// MATLAB R2025b: J = imhistmatch(I, ref, nbins) is exactly
// histeq(I, imhist(ref, nbins)). The transform is built at the input
// class's FULL resolution NPTS (256 for uint8/float, 65536 for
// uint16/int16), NOT at nbins, then applied per input level via
// grayxform. nbins (default 64, all classes) only sets the resolution of
// the TARGET histogram. Algorithm (Gonzalez histogram matching):
//   1. hgram = imhist(ref, nbins)            // raw counts, also 2nd output
//   2. normalise hgram so sum == numel(I); cumd = cumsum(hgram)
//   3. nn = imhist(I, NPTS); cum = cumsum(nn)
//   4. tol(j) = nn(j)/2 for interior j (0 at the two ends)
//   5. T(j) = argmin_i [ cumd(i) − cum(j) + tol(j) ], with errors below
//      −numel(I)·sqrt(eps) clamped to +numel(I); output level (i−1)/(nbins−1)
//   6. apply T to each pixel by its NPTS-level, cast back to input class.

Value imhistmatch(const Value &I, const Value &ref, int nbins,
                  Value *hgramOut, std::pmr::memory_resource *mr)
{
    if (nbins <= 0) nbins = 64;   // MATLAB imhistmatch default (all classes)
    if (nbins < 2) nbins = 2;

    const size_t Ni = I.numel();
    const size_t Nr = ref.numel();
    if (Ni == 0 || Nr == 0) {
        if (hgramOut)
            *hgramOut = Value::matrix(1, nbins, ValueType::DOUBLE, mr);
        return Value::matrix(I.dims().rows(), I.dims().cols(), I.type(), mr);
    }

    // Full-resolution level count of the input class (getrangefromclass).
    int NPTS;
    switch (I.type()) {
        case ValueType::UINT16:
        case ValueType::INT16:   NPTS = 65536; break;
        case ValueType::LOGICAL: NPTS = 2;     break;
        default:                 NPTS = 256;   break;  // UINT8/DOUBLE/SINGLE
    }

    // MATLAB imhist centred binning: u ∈ [0,1] → round(u·(bins−1)).
    auto level_of = [](double u, int bins) {
        int b = (int)std::lround(u * (bins - 1));
        if (b < 0) b = 0;
        if (b >= bins) b = bins - 1;
        return b;
    };

    // 1. Reference histogram (raw counts) = imhist(ref, nbins); 2nd output.
    std::vector<double> hgram((size_t)nbins, 0.0);
    for (size_t i = 0; i < Nr; ++i)
        hgram[(size_t)level_of(element_to_unit(ref, i), nbins)] += 1.0;

    // 2. Normalise to sum == numel(I), then cumulate (cumd, length nbins).
    double sumH = 0.0;
    for (int b = 0; b < nbins; ++b) sumH += hgram[(size_t)b];
    const double scale = (sumH > 0.0) ? (double)Ni / sumH : 0.0;
    std::vector<double> cumd((size_t)nbins, 0.0);
    {
        double acc = 0.0;
        for (int b = 0; b < nbins; ++b) { acc += hgram[(size_t)b] * scale;
                                          cumd[(size_t)b] = acc; }
    }

    // 3. Input histogram at full resolution = imhist(I, NPTS), cumulated.
    std::vector<double> nn((size_t)NPTS, 0.0);
    for (size_t i = 0; i < Ni; ++i)
        nn[(size_t)level_of(element_to_unit(I, i), NPTS)] += 1.0;
    std::vector<double> cum((size_t)NPTS, 0.0);
    { double acc = 0.0;
      for (int j = 0; j < NPTS; ++j) { acc += nn[(size_t)j]; cum[(size_t)j] = acc; } }

    // 5. Transform T (length NPTS), values in [0,1]. For each input level j
    // pick the target bin i minimising the matching error (first on ties,
    // matching MATLAB's min); large-negative errors are clamped to +Ni.
    const double clampThr = -(double)Ni * std::sqrt(2.2204460492503131e-16);
    const double denom = (nbins > 1) ? (double)(nbins - 1) : 1.0;
    std::vector<double> T((size_t)NPTS, 0.0);
    for (int j = 0; j < NPTS; ++j) {
        const double tol = (j == 0 || j == NPTS - 1) ? 0.0
                                                     : nn[(size_t)j] * 0.5;
        const double base = tol - cum[(size_t)j];
        double best = 0.0; int bi = 0; bool have = false;
        for (int i = 0; i < nbins; ++i) {
            double e = cumd[(size_t)i] + base;
            if (e < clampThr) e = (double)Ni;
            if (!have || e < best) { best = e; bi = i; have = true; }
        }
        T[(size_t)j] = (double)bi / denom;
    }

    // 6. Apply: map each pixel by its NPTS-level, cast back to input class.
    const auto &d = I.dims();
    Value out;
    if (d.is3D()) out = Value::matrix3d(d.rows(), d.cols(), d.pages(),
                                        I.type(), mr);
    else          out = Value::matrix(d.rows(), d.cols(), I.type(), mr);
    for (size_t i = 0; i < Ni; ++i) {
        const int lvl = level_of(element_to_unit(I, i), NPTS);
        store_classed(out, i, T[(size_t)lvl], I.type());
    }

    // 2nd output: ref's histogram as a 1×nbins double row (= MATLAB hgram).
    if (hgramOut) {
        Value hg = Value::matrix(1, nbins, ValueType::DOUBLE, mr);
        double *hd = hg.doubleDataMut();
        for (int b = 0; b < nbins; ++b) hd[b] = hgram[(size_t)b];
        *hgramOut = std::move(hg);
    }
    return out;
}

// ════════════════════════════════════════════════════════════════════
// adaptthresh — locally adaptive threshold matrix
// ════════════════════════════════════════════════════════════════════
// Computes T(x, y) ∈ [0, 1] from a local statistic (box mean or
// Gaussian-smoothed mean) of a neighborhood centered at each pixel.
// `sensitivity` scales the threshold relative to the local statistic.
// MATLAB R2025b maps it linearly: T = clip(localStat · (1.6 − s), 0, 1).
// So sensitivity = 0.5 → T = 1.1 · localStat (threshold 10 % above the
// local mean); higher sensitivity lowers the threshold (more foreground
// after binarize), lower raises it. (Verified exactly against MATLAB on
// constant images across s ∈ {0, .25, .5, .75, 1}.)

Value adaptthresh(const Value &I, double sensitivity, int neighborhood, const std::string &statistic, std::pmr::memory_resource *mr)
{
    if (!(sensitivity >= 0.0 && sensitivity <= 1.0))
        throw Error("adaptthresh: sensitivity must be in [0, 1]",
                    0, 0, "adaptthresh", "", "numkit:adaptthresh:sens");

    const int H = static_cast<int>(I.dims().rows());
    const int W = static_cast<int>(I.dims().cols());

    if (neighborhood <= 0) {
        // MATLAB default NeighborhoodSize = 2*floor(size(I)/16)+1, which is
        // 1 (no smoothing — the local statistic is the pixel itself) for any
        // dimension below 16. Do NOT clamp up to 3: that over-smooths small
        // images and diverges from MATLAB.
        const int dim = std::min(H, W);
        neighborhood = 2 * (dim / 16) + 1;
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

    // Apply the MATLAB sensitivity scale: T = clip(localStat·(1.6−s), 0, 1).
    Value T = Value::matrix(static_cast<size_t>(H),
                            static_cast<size_t>(W),
                            ValueType::DOUBLE, mr);
    double *Td = T.doubleDataMut();
    const double scale = 1.6 - sensitivity;
    for (int i = 0; i < H * W; ++i) {
        double v = localStat.elemAsDouble(static_cast<size_t>(i)) * scale;
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

} // namespace numkit::image
