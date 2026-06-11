// toolboxes/image/src/quality/quality.cpp

#include <numkit/image/quality/quality.hpp>

#include <numkit/image/filter/filter.hpp>

#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>

#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

namespace numkit::image {

namespace {

double class_peak(ValueType t) {
    switch (t) {
        case ValueType::UINT8:  return 255.0;
        case ValueType::UINT16: return 65535.0;
        case ValueType::INT16:  return 32767.0;
        default:                return 1.0;
    }
}

// Element-wise difference squared, summed.
double sum_squared_error(const Value &A, const Value &B) {
    const size_t N = A.numel();
    double s = 0.0;
    for (size_t i = 0; i < N; ++i) {
        const double d = A.elemAsDouble(i) - B.elemAsDouble(i);
        s += d * d;
    }
    return s;
}

} // anonymous

Value immse(const Value &A, const Value &B, std::pmr::memory_resource *mr) {
    if (A.numel() != B.numel())
        throw Error("immse: A and B must have the same number of elements",
                    0, 0, "immse", "", "numkit:immse:size");
    const double mse = (A.numel() > 0)
                       ? sum_squared_error(A, B) / double(A.numel())
                       : 0.0;
    return Value::scalar(mse, mr);
}

Value psnr(const Value &A, const Value &B, double peak, std::pmr::memory_resource *mr) {
    if (A.numel() != B.numel())
        throw Error("psnr: A and B must have the same number of elements",
                    0, 0, "psnr", "", "numkit:psnr:size");
    if (std::isnan(peak)) peak = class_peak(A.type());
    if (A.numel() == 0) return Value::scalar(std::numeric_limits<double>::infinity(), mr);
    const double mse = sum_squared_error(A, B) / double(A.numel());
    if (mse == 0.0) return Value::scalar(std::numeric_limits<double>::infinity(), mr);
    const double db = 10.0 * std::log10(peak * peak / mse);
    return Value::scalar(db, mr);
}

Value ssim(const Value &A, const Value &B, std::pmr::memory_resource *mr) {
    if (A.numel() != B.numel())
        throw Error("ssim: A and B must have the same number of elements",
                    0, 0, "ssim", "", "numkit:ssim:size");
    const size_t H = A.dims().rows();
    const size_t W = A.dims().cols();
    if (H == 0 || W == 0) return Value::scalar(1.0, mr);

    // Convert to DOUBLE in [0, 1] for stable SSIM computation.
    auto unit = [&](const Value &X) {
        std::vector<double> out(X.numel());
        const double scale = 1.0 / class_peak(X.type());
        for (size_t i = 0; i < X.numel(); ++i)
            out[i] = X.elemAsDouble(i) * scale;
        return out;
    };
    auto Au = unit(A), Bu = unit(B);

    // Gaussian window kernel: 11×11, σ=1.5.
    Value gK = fspecial("gaussian", { 11.0, 11.0, 1.5 }, mr);

    // Wrap Au and Bu back as Values so we can re-use imfilter.
    auto pack = [&](const std::vector<double> &v) {
        Value V = Value::matrix(H, W, ValueType::DOUBLE, mr);
        double *vd = V.doubleDataMut();
        // Au is in column-major (because Value is column-major and we
        // copied via elemAsDouble which respects col-major linear index).
        for (size_t i = 0; i < v.size(); ++i) vd[i] = v[i];
        return V;
    };
    Value Av = pack(Au), Bv = pack(Bu);

    // Filtered means.
    auto box_filt = [&](const Value &X) {
        return imfilter(X, gK, PadMode::Replicate, 0.0, /*full=*/false, /*flip_kernel=*/false, mr);
    };

    Value mu_a = box_filt(Av);
    Value mu_b = box_filt(Bv);

    // Variances and covariance.
    Value Aa = Value::matrix(H, W, ValueType::DOUBLE, mr);
    Value Bb = Value::matrix(H, W, ValueType::DOUBLE, mr);
    Value Ab = Value::matrix(H, W, ValueType::DOUBLE, mr);
    {
        double *aa = Aa.doubleDataMut();
        double *bb = Bb.doubleDataMut();
        double *ab = Ab.doubleDataMut();
        for (size_t i = 0; i < H * W; ++i) {
            const double a = Av.doubleData()[i];
            const double b = Bv.doubleData()[i];
            aa[i] = a * a;
            bb[i] = b * b;
            ab[i] = a * b;
        }
    }
    Value sa2 = box_filt(Aa);
    Value sb2 = box_filt(Bb);
    Value sab = box_filt(Ab);

    const double K1 = 0.01, K2 = 0.03, L = 1.0;  // L = dynamic range (we scaled to [0,1])
    const double C1 = (K1 * L) * (K1 * L);
    const double C2 = (K2 * L) * (K2 * L);

    double total = 0.0;
    size_t cnt = 0;
    for (size_t i = 0; i < H * W; ++i) {
        const double mua = mu_a.doubleData()[i];
        const double mub = mu_b.doubleData()[i];
        const double va  = sa2.doubleData()[i] - mua * mua;
        const double vb  = sb2.doubleData()[i] - mub * mub;
        const double cov = sab.doubleData()[i] - mua * mub;

        const double num = (2.0 * mua * mub + C1) * (2.0 * cov + C2);
        const double den = (mua * mua + mub * mub + C1) * (va + vb + C2);
        if (den > 0.0) { total += num / den; ++cnt; }
    }
    const double s = (cnt > 0) ? total / double(cnt) : 1.0;
    return Value::scalar(s, mr);
}

Value mean2(const Value &A, std::pmr::memory_resource *mr)
{
    const size_t N = A.numel();
    if (N == 0) return Value::scalar(std::nan(""), mr);
    long double s = 0.0L;
    for (size_t i = 0; i < N; ++i) s += A.elemAsDouble(i);
    return Value::scalar(static_cast<double>(s / static_cast<long double>(N)),
                         mr);
}

Value std2(const Value &A, std::pmr::memory_resource *mr)
{
    const size_t N = A.numel();
    if (N == 0) return Value::scalar(std::nan(""), mr);
    if (N == 1) return Value::scalar(0.0, mr);
    long double s = 0.0L;
    for (size_t i = 0; i < N; ++i) s += A.elemAsDouble(i);
    const long double mu = s / static_cast<long double>(N);
    long double v = 0.0L;
    for (size_t i = 0; i < N; ++i) {
        const long double d = A.elemAsDouble(i) - mu;
        v += d * d;
    }
    // std2 wraps std(I(:)) → sample std (normalize by N-1).
    v /= static_cast<long double>(N - 1);
    return Value::scalar(static_cast<double>(std::sqrt((double)v)), mr);
}

// ════════════════════════════════════════════════════════════════════
// multissim — Multi-scale SSIM (Wang/Simoncelli/Bovik 2003)
// ════════════════════════════════════════════════════════════════════
//
// Algorithm transliterated verbatim from MATLAB R2025b
//   toolbox/images/images/multissim.m + algmultissim.m + parserMultissim.m
//
// Pipeline:
//   1. Promote integer inputs to single (int16 has -intmin offset).
//   2. Build Gaussian kernel: filtSize = 2·ceil(3σ) + 1, σ user-set.
//   3. For each scale i = 1..numScales-1:
//      a. compute SSIM map without luminance:
//           ssimmap = (2σxy + C2) / (σx² + σy² + C2)
//         clamp to ≤ 1.
//      b. score[i] = mean(ssimmap)^scaleWeights[i] (clamp base to 0
//         for non-integer exponent).
//      c. lowpass (2×2 box) + downsample by 2.
//   4. At coarsest scale, compute SSIM with luminance:
//        ssimmap = (2μxμy+C1)(2σxy+C2) / ((μx²+μy²+C1)(σx²+σy²+C2))
//        clamp to ≤ 1.
//      score[numScales] = mean(ssimmap)^scaleWeights[numScales].
//   5. final score = prod(score[1..numScales]).
//
// Reference: Wang, Z., Simoncelli, E.P., Bovik, A.C., "Multiscale
//   structural similarity for image quality assessment", Asilomar
//   Conference on Signals, Systems & Computers, 2003.

namespace {

double class_dynamic_range(ValueType t)
{
    switch (t) {
        case ValueType::UINT8:  return 255.0;
        case ValueType::UINT16: return 65535.0;
        case ValueType::INT16:  return 65535.0;   // diff(int16 range)
        default:                return 1.0;       // single, double
    }
}

// Default ScaleWeights: fspecial('gaussian', [1 N], 1) — sample
// gaussian at positions (0..N-1) - (N-1)/2 with σ=1, normalised to
// sum to 1. For N=1 returns [1.0]. For N=5 returns
// [0.0545, 0.2442, 0.4026, 0.2442, 0.0545].
std::vector<double> default_scale_weights(int N)
{
    if (N < 1) N = 1;
    std::vector<double> w(static_cast<std::size_t>(N));
    if (N == 1) { w[0] = 1.0; return w; }
    const double half = (N - 1) / 2.0;
    double s = 0.0;
    for (int i = 0; i < N; ++i) {
        const double x = i - half;
        w[i] = std::exp(-0.5 * x * x);
        s += w[i];
    }
    for (double &v : w) v /= s;
    return w;
}

// Convert input image to DOUBLE for stable computation. Integer
// inputs are promoted: uint8/uint16 → x (NOT divided — multissim
// uses the raw integer pixel values plus the DynamicRange scaling
// for C constants). int16 → x - intmin = x + 32768.
Value to_double_image(const Value &A, std::pmr::memory_resource *mr)
{
    const std::size_t H = A.dims().rows();
    const std::size_t W = A.dims().cols();
    Value out = Value::matrix(H, W, ValueType::DOUBLE, mr);
    double *od = out.doubleDataMut();
    const ValueType t = A.type();
    for (std::size_t i = 0; i < H * W; ++i) {
        double v = A.elemAsDouble(i);
        if (t == ValueType::INT16) v += 32768.0;
        // uint8 / uint16 / single / double all keep raw value.
        od[i] = v;
    }
    return out;
}

// 2×2 box lowpass (ones/4) with replicate boundary, anchor at the
// kernel TOP-LEFT (matches MATLAB's imfilter anchor convention for
// even-sized kernels: anchor = floor((sz+1)/2) − 1 = 0 in 0-based).
//
// Out(r,c) = 0.25 · (X(r,c) + X(r+1,c) + X(r,c+1) + X(r+1,c+1))
// with row r+1 / col c+1 clamped via replicate at the bottom/right edges.
Value lowpass_2x2(const Value &X, std::pmr::memory_resource *mr)
{
    const std::size_t H = X.dims().rows();
    const std::size_t W = X.dims().cols();
    Value out = Value::matrix(H, W, ValueType::DOUBLE, mr);
    double *od = out.doubleDataMut();
    const double *xd = X.doubleData();
    for (std::size_t c = 0; c < W; ++c) {
        const std::size_t cn = (c + 1 < W) ? (c + 1) : (W - 1);  // replicate
        for (std::size_t r = 0; r < H; ++r) {
            const std::size_t rn = (r + 1 < H) ? (r + 1) : (H - 1);
            od[c * H + r] = 0.25 * (xd[c  * H + r ]
                                  + xd[c  * H + rn]
                                  + xd[cn * H + r ]
                                  + xd[cn * H + rn]);
        }
    }
    return out;
}

// Take every other row/column (1:2:end semantics, 0-based: rows/cols
// 0, 2, 4, ...). Returns a new H' × W' Value.
Value downsample_2(const Value &X, std::pmr::memory_resource *mr)
{
    const std::size_t H = X.dims().rows();
    const std::size_t W = X.dims().cols();
    const std::size_t Ho = (H + 1) / 2;
    const std::size_t Wo = (W + 1) / 2;
    Value out = Value::matrix(Ho, Wo, ValueType::DOUBLE, mr);
    double *od = out.doubleDataMut();
    const double *xd = X.doubleData();
    for (std::size_t c = 0; c < Wo; ++c)
        for (std::size_t r = 0; r < Ho; ++r)
            od[c * Ho + r] = xd[(c * 2) * H + (r * 2)];
    return out;
}

// Compute per-pixel SSIM map for one scale. If include_luminance is
// false, the map is (2σxy+C2)/(σx²+σy²+C2). If true, full SSIM
// (Wang-Bovik Eq. 13) with the luminance term.
Value ssim_map_double(const Value &A, const Value &B,
                      const Value &gK, double C1, double C2,
                      bool include_luminance,
                      std::pmr::memory_resource *mr)
{
    const std::size_t H = A.dims().rows();
    const std::size_t W = A.dims().cols();
    auto box = [&](const Value &X) {
        return imfilter(X, gK, PadMode::Replicate, 0.0, /*full=*/false,
                        /*flip_kernel=*/false, mr);
    };
    Value mu_a = box(A);
    Value mu_b = box(B);
    // Pre-products.
    Value AA = Value::matrix(H, W, ValueType::DOUBLE, mr);
    Value BB = Value::matrix(H, W, ValueType::DOUBLE, mr);
    Value AB = Value::matrix(H, W, ValueType::DOUBLE, mr);
    const double *ad = A.doubleData();
    const double *bd = B.doubleData();
    double *aa = AA.doubleDataMut();
    double *bb = BB.doubleDataMut();
    double *ab = AB.doubleDataMut();
    for (std::size_t i = 0; i < H * W; ++i) {
        const double a = ad[i], b = bd[i];
        aa[i] = a * a;
        bb[i] = b * b;
        ab[i] = a * b;
    }
    Value sa2 = box(AA);
    Value sb2 = box(BB);
    Value sab = box(AB);

    Value out = Value::matrix(H, W, ValueType::DOUBLE, mr);
    double *od = out.doubleDataMut();
    const double *mua_d = mu_a.doubleData();
    const double *mub_d = mu_b.doubleData();
    const double *sa2_d = sa2.doubleData();
    const double *sb2_d = sb2.doubleData();
    const double *sab_d = sab.doubleData();
    for (std::size_t i = 0; i < H * W; ++i) {
        const double mua = mua_d[i];
        const double mub = mub_d[i];
        const double mu_xy = mua * mub;
        const double mux2 = mua * mua;
        const double muy2 = mub * mub;
        double vx = sa2_d[i] - mux2;
        double vy = sb2_d[i] - muy2;
        if (vx < 0.0) vx = 0.0;
        if (vy < 0.0) vy = 0.0;
        const double cov = sab_d[i] - mu_xy;
        const double num2 = 2.0 * cov + C2;
        const double den2 = vx + vy + C2;
        if (include_luminance) {
            const double num1 = 2.0 * mu_xy + C1;
            const double den1 = mux2 + muy2 + C1;
            od[i] = (den1 != 0.0 && den2 != 0.0)
                  ? (num1 * num2) / (den1 * den2) : 1.0;
        } else {
            od[i] = (den2 != 0.0) ? num2 / den2 : 1.0;
        }
        // Clamp to <= 1 per MATLAB.
        if (od[i] > 1.0) od[i] = 1.0;
    }
    return out;
}

// Cast a DOUBLE Value to SINGLE (or pass through for DOUBLE).
Value cast_score_value(double v, ValueType originalClass,
                       std::pmr::memory_resource *mr)
{
    if (originalClass == ValueType::DOUBLE)
        return Value::scalar(v, mr);
    Value out = Value::matrix(1, 1, ValueType::SINGLE, mr);
    out.singleDataMut()[0] = static_cast<float>(v);
    return out;
}

} // namespace

Value multissim(const Value &A, const Value &Iref,
                int num_scales,
                const std::vector<double> &scale_weights_in,
                double sigma, double dynamic_range_in,
                std::vector<Value> *quality_maps_out,
                std::pmr::memory_resource *mr)
{
    // ── Validate inputs ──────────────────────────────────────────
    if (A.dims().rows() != Iref.dims().rows()
        || A.dims().cols() != Iref.dims().cols())
        throw Error("multissim: I and Iref must have the same size",
                    0, 0, "multissim", "", "numkit:multissim:size");
    if (A.type() != Iref.type())
        throw Error("multissim: I and Iref must have the same class",
                    0, 0, "multissim", "", "numkit:multissim:class");
    if (num_scales < 1)
        throw Error("multissim: NumScales must be a positive integer",
                    0, 0, "multissim", "", "numkit:multissim:numScales");
    if (!std::isfinite(sigma) || sigma <= 0.0)
        throw Error("multissim: Sigma must be a positive scalar",
                    0, 0, "multissim", "", "numkit:multissim:sigma");

    const ValueType origClass = A.type();
    const std::size_t H = A.dims().rows();
    const std::size_t W = A.dims().cols();

    // Verify downsampleability.
    {
        std::size_t h = H, w = W;
        for (int i = 1; i < num_scales; ++i) {
            if (h <= 1 || w <= 1)
                throw Error("multissim: image too small for NumScales="
                            + std::to_string(num_scales),
                            0, 0, "multissim", "",
                            "numkit:multissim:tooSmall");
            h = (h + 1) / 2;
            w = (w + 1) / 2;
        }
    }

    // ── Resolve dynamic range / C constants ─────────────────────
    const double L = (dynamic_range_in > 0.0)
                   ? dynamic_range_in
                   : class_dynamic_range(origClass);
    const double C1 = (0.01 * L) * (0.01 * L);
    const double C2 = (0.03 * L) * (0.03 * L);

    // ── Resolve / normalize scale weights ───────────────────────
    std::vector<double> sw = scale_weights_in.empty()
        ? default_scale_weights(num_scales)
        : scale_weights_in;
    if (static_cast<int>(sw.size()) != num_scales)
        throw Error("multissim: length(ScaleWeights) must equal NumScales",
                    0, 0, "multissim", "",
                    "numkit:multissim:swLen");
    double swsum = 0.0;
    for (double v : sw) {
        if (v < 0.0)
            throw Error("multissim: ScaleWeights must be non-negative",
                        0, 0, "multissim", "",
                        "numkit:multissim:swNeg");
        swsum += v;
    }
    if (swsum <= 0.0)
        throw Error("multissim: ScaleWeights must have at least one "
                    "positive element",
                    0, 0, "multissim", "",
                    "numkit:multissim:swZero");
    for (double &v : sw) v /= swsum;

    // ── Gaussian kernel ────────────────────────────────────────
    const int filtRadius = static_cast<int>(std::ceil(sigma * 3.0));
    const int filtSize = 2 * filtRadius + 1;
    Value gK = fspecial("gaussian",
                        { static_cast<double>(filtSize),
                          static_cast<double>(filtSize), sigma }, mr);

    // ── Promote to DOUBLE for computation ───────────────────────
    Value Ad = to_double_image(A, mr);
    Value Bd = to_double_image(Iref, mr);

    // ── Multi-scale loop ────────────────────────────────────────
    if (quality_maps_out) quality_maps_out->clear();
    double total_log_score = 0.0;
    bool has_log = true;            // accumulate in log domain for stability
    double total_score = 1.0;       // fallback (in case log fails)

    auto apply_weight = [](double base, double exponent) -> double {
        // base may be slightly negative due to float noise; clamp.
        if (exponent != std::floor(exponent) && base < 0.0) base = 0.0;
        return std::pow(base, exponent);
    };

    for (int i = 0; i < num_scales; ++i) {
        const bool last = (i == num_scales - 1);
        Value ssimmap = ssim_map_double(Ad, Bd, gK, C1, C2,
                                        /*include_luminance=*/last, mr);
        // Mean of ssimmap.
        const std::size_t Nm = ssimmap.numel();
        long double s = 0.0L;
        const double *sd = ssimmap.doubleData();
        for (std::size_t k = 0; k < Nm; ++k) s += sd[k];
        const double mean_map = static_cast<double>(
            s / static_cast<long double>(Nm));
        const double per_scale = apply_weight(mean_map, sw[i]);
        total_score *= per_scale;
        if (mean_map > 0.0) {
            total_log_score += sw[i] * std::log(mean_map);
        } else {
            has_log = false;
        }
        if (quality_maps_out) quality_maps_out->push_back(ssimmap);
        if (!last) {
            Ad = downsample_2(lowpass_2x2(Ad, mr), mr);
            Bd = downsample_2(lowpass_2x2(Bd, mr), mr);
        }
    }
    double score = has_log ? std::exp(total_log_score) : total_score;

    return cast_score_value(score, origClass, mr);
}

// ════════════════════════════════════════════════════════════════════
// multissim3 — 3-D multi-scale SSIM (Wang/Simoncelli/Bovik 2003)
// ════════════════════════════════════════════════════════════════════
//
// Volumetric extension of multissim. Same algorithm but:
//   - Gaussian smoothing is 3-D separable (3 successive 1-D convs
//     along rows / cols / slices) with the same filter size
//     2·ceil(3σ)+1.
//   - Lowpass is a 2×2×2 box (ones/8) with replicate boundary,
//     anchor at the top-front-left corner of the kernel (matches
//     MATLAB's imfilter even-size convention).
//   - Downsample factor 2 in ALL three dims (rows/cols/slices).
//
// Transliterated verbatim from MATLAB R2025b multissim3.m +
// algmultissim.m (numSpatialDims=3 branch) + parserMultissim.m.

namespace {

// Volume buffer mirror of the 2-D "image-in-DOUBLE" used by multissim.
struct Vol {
    std::size_t H = 0, W = 0, D = 0;
    std::vector<double> v;  // column-major: idx = z*H*W + c*H + r
    Vol() = default;
    Vol(std::size_t h, std::size_t w, std::size_t d)
        : H(h), W(w), D(d), v(h * w * d, 0.0) {}
    double &at(std::size_t r, std::size_t c, std::size_t z) {
        return v[z * H * W + c * H + r];
    }
    double at(std::size_t r, std::size_t c, std::size_t z) const {
        return v[z * H * W + c * H + r];
    }
};

Vol to_double_vol(const Value &V)
{
    const std::size_t H = V.dims().rows();
    const std::size_t W = V.dims().cols();
    const std::size_t D = V.dims().is3D() ? V.dims().pages() : 1;
    Vol out(H, W, D);
    const ValueType t = V.type();
    const std::size_t N = H * W * D;
    for (std::size_t i = 0; i < N; ++i) {
        double v = V.elemAsDouble(i);
        if (t == ValueType::INT16) v += 32768.0;
        out.v[i] = v;
    }
    return out;
}

Value vol_to_value(const Vol &V, std::pmr::memory_resource *mr)
{
    Value out = V.D == 1
        ? Value::matrix(V.H, V.W, ValueType::DOUBLE, mr)
        : Value::matrix3d(V.H, V.W, V.D, ValueType::DOUBLE, mr);
    std::memcpy(out.doubleDataMut(), V.v.data(),
                V.v.size() * sizeof(double));
    return out;
}

// Build a 1-D Gaussian kernel with `filtSize` elements, mean = (filtSize-1)/2.
std::vector<double> gauss1d(double sigma, int filtSize)
{
    std::vector<double> k(static_cast<std::size_t>(filtSize));
    const double half = (filtSize - 1) / 2.0;
    double s = 0.0;
    for (int i = 0; i < filtSize; ++i) {
        const double x = i - half;
        k[i] = std::exp(-0.5 * (x / sigma) * (x / sigma));
        s += k[i];
    }
    for (double &v : k) v /= s;
    return k;
}

inline std::size_t reflect_clamp(long i, std::size_t n)
{
    // Replicate boundary: clamp to [0, n-1].
    if (i < 0) return 0;
    if (i >= static_cast<long>(n)) return n - 1;
    return static_cast<std::size_t>(i);
}

// Separable 3-D Gaussian: convolve along rows, then cols, then slices.
// All passes use replicate boundary.
Vol gauss3d_filter(const Vol &In, const std::vector<double> &k)
{
    const std::size_t H = In.H;
    const std::size_t W = In.W;
    const std::size_t D = In.D;
    const int kn = static_cast<int>(k.size());
    const int half = kn / 2;
    Vol tmp(H, W, D), out(H, W, D);
    // Rows pass: for each (c, z), convolve along r.
    for (std::size_t z = 0; z < D; ++z)
        for (std::size_t c = 0; c < W; ++c)
            for (std::size_t r = 0; r < H; ++r) {
                double s = 0.0;
                for (int j = -half; j <= half; ++j) {
                    const std::size_t rr = reflect_clamp(
                        static_cast<long>(r) + j, H);
                    s += k[static_cast<std::size_t>(j + half)] * In.at(rr, c, z);
                }
                tmp.at(r, c, z) = s;
            }
    // Cols pass.
    Vol tmp2(H, W, D);
    for (std::size_t z = 0; z < D; ++z)
        for (std::size_t c = 0; c < W; ++c)
            for (std::size_t r = 0; r < H; ++r) {
                double s = 0.0;
                for (int j = -half; j <= half; ++j) {
                    const std::size_t cc = reflect_clamp(
                        static_cast<long>(c) + j, W);
                    s += k[static_cast<std::size_t>(j + half)] * tmp.at(r, cc, z);
                }
                tmp2.at(r, c, z) = s;
            }
    // Slices pass.
    for (std::size_t z = 0; z < D; ++z)
        for (std::size_t c = 0; c < W; ++c)
            for (std::size_t r = 0; r < H; ++r) {
                double s = 0.0;
                for (int j = -half; j <= half; ++j) {
                    const std::size_t zz = reflect_clamp(
                        static_cast<long>(z) + j, D);
                    s += k[static_cast<std::size_t>(j + half)] * tmp2.at(r, c, zz);
                }
                out.at(r, c, z) = s;
            }
    return out;
}

// 2×2×2 box lowpass with replicate boundary, anchor at top-front-left.
//   Out(r,c,z) = (1/8) * sum_(i,j,k) In(r+i, c+j, z+k), i,j,k ∈ {0,1}
Vol lowpass_2x2x2(const Vol &In)
{
    const std::size_t H = In.H;
    const std::size_t W = In.W;
    const std::size_t D = In.D;
    Vol out(H, W, D);
    for (std::size_t z = 0; z < D; ++z) {
        const std::size_t zn = (z + 1 < D) ? (z + 1) : (D - 1);
        for (std::size_t c = 0; c < W; ++c) {
            const std::size_t cn = (c + 1 < W) ? (c + 1) : (W - 1);
            for (std::size_t r = 0; r < H; ++r) {
                const std::size_t rn = (r + 1 < H) ? (r + 1) : (H - 1);
                out.at(r, c, z) =
                    0.125 * (In.at(r, c, z) + In.at(rn, c, z)
                          + In.at(r, cn, z) + In.at(rn, cn, z)
                          + In.at(r, c, zn) + In.at(rn, c, zn)
                          + In.at(r, cn, zn) + In.at(rn, cn, zn));
            }
        }
    }
    return out;
}

Vol downsample_2x2x2(const Vol &In)
{
    const std::size_t H = (In.H + 1) / 2;
    const std::size_t W = (In.W + 1) / 2;
    const std::size_t D = (In.D + 1) / 2;
    Vol out(H, W, D);
    for (std::size_t z = 0; z < D; ++z)
        for (std::size_t c = 0; c < W; ++c)
            for (std::size_t r = 0; r < H; ++r)
                out.at(r, c, z) = In.at(r * 2, c * 2, z * 2);
    return out;
}

// 3-D SSIM map. Same formula as ssim_map_double but operates on Vol.
Vol ssim_map_vol(const Vol &A, const Vol &B,
                 const std::vector<double> &k,
                 double C1, double C2, bool include_luminance)
{
    const std::size_t H = A.H, W = A.W, D = A.D;
    Vol AA(H, W, D), BB(H, W, D), AB(H, W, D);
    for (std::size_t i = 0; i < A.v.size(); ++i) {
        const double a = A.v[i], b = B.v[i];
        AA.v[i] = a * a;
        BB.v[i] = b * b;
        AB.v[i] = a * b;
    }
    Vol mu_a = gauss3d_filter(A, k);
    Vol mu_b = gauss3d_filter(B, k);
    Vol sa2  = gauss3d_filter(AA, k);
    Vol sb2  = gauss3d_filter(BB, k);
    Vol sab  = gauss3d_filter(AB, k);
    Vol out(H, W, D);
    for (std::size_t i = 0; i < A.v.size(); ++i) {
        const double mua = mu_a.v[i], mub = mu_b.v[i];
        const double mu_xy = mua * mub;
        const double mux2 = mua * mua;
        const double muy2 = mub * mub;
        double vx = sa2.v[i] - mux2;
        double vy = sb2.v[i] - muy2;
        if (vx < 0.0) vx = 0.0;
        if (vy < 0.0) vy = 0.0;
        const double cov = sab.v[i] - mu_xy;
        const double num2 = 2.0 * cov + C2;
        const double den2 = vx + vy + C2;
        double m;
        if (include_luminance) {
            const double num1 = 2.0 * mu_xy + C1;
            const double den1 = mux2 + muy2 + C1;
            m = (den1 != 0.0 && den2 != 0.0) ? (num1 * num2) / (den1 * den2) : 1.0;
        } else {
            m = (den2 != 0.0) ? num2 / den2 : 1.0;
        }
        if (m > 1.0) m = 1.0;
        out.v[i] = m;
    }
    return out;
}

} // namespace

Value multissim3(const Value &V, const Value &Vref,
                 int num_scales,
                 const std::vector<double> &scale_weights_in,
                 double sigma, double dynamic_range_in,
                 std::vector<Value> *quality_maps_out,
                 std::pmr::memory_resource *mr)
{
    if (V.dims().rows() != Vref.dims().rows()
        || V.dims().cols() != Vref.dims().cols()
        || V.dims().pages() != Vref.dims().pages())
        throw Error("multissim3: V and Vref must have the same size",
                    0, 0, "multissim3", "", "numkit:multissim3:size");
    if (V.type() != Vref.type())
        throw Error("multissim3: V and Vref must have the same class",
                    0, 0, "multissim3", "", "numkit:multissim3:class");
    if (num_scales < 1)
        throw Error("multissim3: NumScales must be a positive integer",
                    0, 0, "multissim3", "",
                    "numkit:multissim3:numScales");
    if (!std::isfinite(sigma) || sigma <= 0.0)
        throw Error("multissim3: Sigma must be a positive scalar",
                    0, 0, "multissim3", "",
                    "numkit:multissim3:sigma");

    const ValueType origClass = V.type();
    Vol Ad = to_double_vol(V);
    Vol Bd = to_double_vol(Vref);

    // Verify downsampleability.
    {
        std::size_t h = Ad.H, w = Ad.W, d = Ad.D;
        for (int i = 1; i < num_scales; ++i) {
            if (h <= 1 || w <= 1 || d <= 1)
                throw Error("multissim3: volume too small for NumScales="
                            + std::to_string(num_scales),
                            0, 0, "multissim3", "",
                            "numkit:multissim3:tooSmall");
            h = (h + 1) / 2;
            w = (w + 1) / 2;
            d = (d + 1) / 2;
        }
    }

    const double L = (dynamic_range_in > 0.0)
                   ? dynamic_range_in
                   : class_dynamic_range(origClass);
    const double C1 = (0.01 * L) * (0.01 * L);
    const double C2 = (0.03 * L) * (0.03 * L);

    std::vector<double> sw = scale_weights_in.empty()
        ? default_scale_weights(num_scales)
        : scale_weights_in;
    if (static_cast<int>(sw.size()) != num_scales)
        throw Error("multissim3: length(ScaleWeights) must equal NumScales",
                    0, 0, "multissim3", "", "numkit:multissim3:swLen");
    double swsum = 0.0;
    for (double v : sw) {
        if (v < 0.0)
            throw Error("multissim3: ScaleWeights must be non-negative",
                        0, 0, "multissim3", "", "numkit:multissim3:swNeg");
        swsum += v;
    }
    if (swsum <= 0.0)
        throw Error("multissim3: ScaleWeights must have at least one "
                    "positive element",
                    0, 0, "multissim3", "", "numkit:multissim3:swZero");
    for (double &v : sw) v /= swsum;

    const int filtRadius = static_cast<int>(std::ceil(sigma * 3.0));
    const int filtSize = 2 * filtRadius + 1;
    std::vector<double> gk = gauss1d(sigma, filtSize);

    if (quality_maps_out) quality_maps_out->clear();

    double total_log_score = 0.0;
    bool has_log = true;
    double total_score = 1.0;
    auto apply_weight = [](double base, double exp) -> double {
        if (exp != std::floor(exp) && base < 0.0) base = 0.0;
        return std::pow(base, exp);
    };

    for (int i = 0; i < num_scales; ++i) {
        const bool last = (i == num_scales - 1);
        Vol ssimmap = ssim_map_vol(Ad, Bd, gk, C1, C2,
                                   /*include_luminance=*/last);
        const std::size_t Nm = ssimmap.v.size();
        long double s = 0.0L;
        for (double x : ssimmap.v) s += x;
        const double mean_map = static_cast<double>(
            s / static_cast<long double>(Nm));
        total_score *= apply_weight(mean_map, sw[i]);
        if (mean_map > 0.0) total_log_score += sw[i] * std::log(mean_map);
        else                has_log = false;
        if (quality_maps_out) quality_maps_out->push_back(vol_to_value(ssimmap, mr));
        if (!last) {
            Vol Al = lowpass_2x2x2(Ad);
            Vol Bl = lowpass_2x2x2(Bd);
            Ad = downsample_2x2x2(Al);
            Bd = downsample_2x2x2(Bl);
        }
    }
    double score = has_log ? std::exp(total_log_score) : total_score;
    return cast_score_value(score, origClass, mr);
}

Value corr2(const Value &A, const Value &B, std::pmr::memory_resource *mr)
{
    const size_t N = A.numel();
    if (B.numel() != N)
        throw Error("corr2: A and B must have the same number of elements",
                    0, 0, "corr2", "", "numkit:corr2:size");
    if (N == 0) return Value::scalar(std::nan(""), mr);
    long double sa = 0.0L, sb = 0.0L;
    for (size_t i = 0; i < N; ++i) {
        sa += A.elemAsDouble(i);
        sb += B.elemAsDouble(i);
    }
    const long double ma = sa / static_cast<long double>(N);
    const long double mb = sb / static_cast<long double>(N);
    long double cov = 0.0L, va = 0.0L, vb = 0.0L;
    for (size_t i = 0; i < N; ++i) {
        const long double da = A.elemAsDouble(i) - ma;
        const long double db = B.elemAsDouble(i) - mb;
        cov += da * db;
        va  += da * da;
        vb  += db * db;
    }
    const long double denom = std::sqrt((double)(va * vb));
    if (denom == 0.0L) return Value::scalar(std::nan(""), mr);
    return Value::scalar(static_cast<double>(cov / denom), mr);
}

} // namespace numkit::image
