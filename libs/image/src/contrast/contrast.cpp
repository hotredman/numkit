// libs/image/src/contrast/contrast.cpp

#include <numkit/image/contrast/contrast.hpp>
#include <numkit/image/filter/filter.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <functional>
#include <limits>
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
                        "m:contrast:badtype");
    }
}

} // anonymous

std::tuple<Value, Value>
imhist(std::pmr::memory_resource *mr, const Value &I, int n)
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
    const double step = (n > 1) ? 1.0 / double(n - 1) : 0.0;
    for (int i = 0; i < n; ++i) {
        cd[i] = (double)counts[i];
        xd[i] = i * step;
    }
    return std::make_tuple(std::move(c), std::move(x));
}

Value stretchlim(std::pmr::memory_resource *mr, const Value &I,
                 double low_tol, double high_tol)
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

    std::vector<double> samples(plane);
    for (int ch = 0; ch < channels; ++ch) {
        for (size_t i = 0; i < plane; ++i) {
            samples[i] = element_to_unit(I, ch * plane + i);
        }
        std::sort(samples.begin(), samples.end());
        const size_t lo_idx = (size_t)std::floor(low_tol  * (plane - 1));
        const size_t hi_idx = (size_t)std::ceil (high_tol * (plane - 1));
        od[(size_t)ch * 2 + 0] = samples[lo_idx];
        od[(size_t)ch * 2 + 1] = samples[std::min(hi_idx, plane - 1)];
    }
    return out;
}

Value imadjust(std::pmr::memory_resource *mr, const Value &I,
               double low_in, double high_in,
               double low_out, double high_out, double gamma)
{
    // Auto-fill missing endpoints via stretchlim defaults.
    if (std::isnan(low_in) || std::isnan(high_in)) {
        Value lim = stretchlim(mr, I, 0.01, 0.99);
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

Value histeq(std::pmr::memory_resource *mr, const Value &I, int n)
{
    if (n <= 0) n = 64;
    auto [counts_v, bins_v] = imhist(mr, I, n);
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
// Otsu thresholding
// ════════════════════════════════════════════════════════════════════

namespace {

// Single-threshold Otsu on a histogram of L bins. Returns (level ∈ [0,L-1],
// em ∈ [0, 1]).
std::pair<int, double> otsu_one_level(const std::vector<double> &counts) {
    const int L = (int)counts.size();
    double total = 0.0;
    double sum_total = 0.0;
    for (int i = 0; i < L; ++i) { total += counts[i]; sum_total += i * counts[i]; }
    if (total <= 0.0) return {0, 0.0};

    double w0 = 0.0, sum0 = 0.0;
    double best_var = -1.0;
    int best_t = 0;
    for (int t = 0; t < L - 1; ++t) {
        w0 += counts[t]; sum0 += t * counts[t];
        const double w1 = total - w0;
        if (w0 == 0.0 || w1 == 0.0) continue;
        const double mu0 = sum0 / w0;
        const double mu1 = (sum_total - sum0) / w1;
        const double diff = mu0 - mu1;
        const double inter_var = w0 * w1 * diff * diff;
        if (inter_var > best_var) { best_var = inter_var; best_t = t; }
    }
    // Effectiveness metric η = σ_b² / σ_T² (matches MATLAB graythresh's
    // 2nd output). With unnormalised counts: σ_b² = best_var / T²,
    // σ_T² = total_var / T, so η = best_var / (T · total_var).
    const double mu = sum_total / total;
    double total_var = 0.0;
    for (int i = 0; i < L; ++i) total_var += counts[i] * (i - mu) * (i - mu);
    const double em = (total_var > 0.0) ? (best_var / (total * total_var)) : 0.0;
    return {best_t, em};
}

} // anonymous

std::tuple<Value, Value>
otsuthresh(std::pmr::memory_resource *mr, const Value &counts_v) {
    const size_t L = counts_v.numel();
    std::vector<double> c(L);
    for (size_t i = 0; i < L; ++i) c[i] = counts_v.elemAsDouble(i);
    auto [lvl, em] = otsu_one_level(c);
    const double thresh = (L > 1) ? double(lvl) / double(L - 1) : 0.0;
    return std::make_tuple(Value::scalar(thresh, mr), Value::scalar(em, mr));
}

std::tuple<Value, Value>
graythresh(std::pmr::memory_resource *mr, const Value &I) {
    auto [counts, _] = imhist(mr, I, default_nbins(I));
    return otsuthresh(mr, counts);
}

std::tuple<Value, Value>
multithresh(std::pmr::memory_resource *mr, const Value &I, int N) {
    if (N <= 1) {
        auto [t, em] = graythresh(mr, I);
        return std::make_tuple(std::move(t), std::move(em));
    }
    if (N > 5)
        throw Error("multithresh: N > 5 not supported (exhaustive search would be too slow)",
                    0, 0, "multithresh", "", "m:multithresh:tooMany");

    const int L = default_nbins(I);
    auto [counts_v, _] = imhist(mr, I, L);
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

    Value t_out = Value::matrix(1, N, ValueType::DOUBLE, mr);
    double *td = t_out.doubleDataMut();
    for (int k = 0; k < N; ++k) td[k] = double(best[k]) / double(L - 1);
    // Effectiveness η = sigma_b^2 / sigma_T^2.
    const double mu = sum_total / total;
    double total_var = 0.0;
    for (int i = 0; i < L; ++i) total_var += counts[i] * (i - mu) * (i - mu);
    // best_var is sum of w·μ² over classes; sigma_b² = sum w·μ² − total·mu².
    const double sigma_b2 = best_var - total * mu * mu;
    const double em = (total_var > 0.0) ? (sigma_b2 / total_var) : 0.0;
    return std::make_tuple(std::move(t_out), Value::scalar(em, mr));
}

Value imbinarize(std::pmr::memory_resource *mr, const Value &I, double thresh) {
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
Value imbinarize(std::pmr::memory_resource *mr, const Value &I,
                 const Value &T)
{
    if (T.numel() != I.numel())
        throw Error("imbinarize: per-pixel T must have the same number "
                    "of elements as I",
                    0, 0, "imbinarize", "", "m:imbinarize:Tshape");
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

Value imquantize(std::pmr::memory_resource *mr, const Value &I, const Value &levels) {
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

Value adaptthresh(std::pmr::memory_resource *mr, const Value &I,
                  double sensitivity, int neighborhood,
                  const std::string &statistic)
{
    if (!(sensitivity >= 0.0 && sensitivity <= 1.0))
        throw Error("adaptthresh: sensitivity must be in [0, 1]",
                    0, 0, "adaptthresh", "", "m:adaptthresh:sens");

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
        localStat = imboxfilt(mr, Inorm, neighborhood);
    } else if (s == "gaussian" || s == "Gaussian" || s == "GAUSSIAN") {
        // σ ≈ neighborhood/6: typical MATLAB default for adaptthresh's
        // Gaussian variant.
        const double sigma = double(neighborhood) / 6.0;
        localStat = imgaussfilt(mr, Inorm, sigma, neighborhood);
    } else {
        throw Error("adaptthresh: statistic must be 'mean' or 'gaussian'",
                    0, 0, "adaptthresh", "", "m:adaptthresh:stat");
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

// ════════════════════════════════════════════════════════════════════
// Engine adapters
// ════════════════════════════════════════════════════════════════════

namespace detail {

void imhist_reg(Span<const Value> args, size_t nargout,
                Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("imhist: requires (I[, n])", 0, 0, "imhist", "",
                    "m:imhist:nargin");
    int n = (args.size() >= 2 && !args[1].isEmpty()) ? (int)args[1].toScalar() : 0;
    auto [c, x] = imhist(ctx.engine->resource(), args[0], n);
    outs[0] = std::move(c);
    if (nargout > 1) outs[1] = std::move(x);
}

void stretchlim_reg(Span<const Value> args, size_t /*nargout*/,
                    Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("stretchlim: requires (I[, tol])", 0, 0, "stretchlim", "",
                    "m:stretchlim:nargin");
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
    outs[0] = stretchlim(ctx.engine->resource(), args[0], lo, hi);
}

void imadjust_reg(Span<const Value> args, size_t /*nargout*/,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("imadjust: requires (I[, [low_in high_in]][, [low_out high_out]][, gamma])",
                    0, 0, "imadjust", "", "m:imadjust:nargin");
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

    outs[0] = imadjust(ctx.engine->resource(), args[0],
                       low_in, high_in, low_out, high_out, gamma);
}

void histeq_reg(Span<const Value> args, size_t /*nargout*/,
                Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("histeq: requires (I[, n])", 0, 0, "histeq", "",
                    "m:histeq:nargin");
    int n = (args.size() >= 2 && !args[1].isEmpty()) ? (int)args[1].toScalar() : 64;
    outs[0] = histeq(ctx.engine->resource(), args[0], n);
}

void graythresh_reg(Span<const Value> args, size_t nargout,
                    Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("graythresh: requires I", 0, 0, "graythresh", "",
                    "m:graythresh:nargin");
    auto [t, em] = graythresh(ctx.engine->resource(), args[0]);
    outs[0] = std::move(t);
    if (nargout > 1) outs[1] = std::move(em);
}

void otsuthresh_reg(Span<const Value> args, size_t nargout,
                    Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("otsuthresh: requires counts", 0, 0, "otsuthresh", "",
                    "m:otsuthresh:nargin");
    auto [t, em] = otsuthresh(ctx.engine->resource(), args[0]);
    outs[0] = std::move(t);
    if (nargout > 1) outs[1] = std::move(em);
}

void multithresh_reg(Span<const Value> args, size_t nargout,
                     Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("multithresh: requires (I[, N])", 0, 0, "multithresh", "",
                    "m:multithresh:nargin");
    int N = (args.size() >= 2 && !args[1].isEmpty()) ? (int)args[1].toScalar() : 1;
    auto [t, em] = multithresh(ctx.engine->resource(), args[0], N);
    outs[0] = std::move(t);
    if (nargout > 1) outs[1] = std::move(em);
}

void imbinarize_reg(Span<const Value> args, size_t /*nargout*/,
                    Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("imbinarize: requires (I[, thresh])", 0, 0, "imbinarize", "",
                    "m:imbinarize:nargin");
    auto *mr = ctx.engine->resource();
    if (args.size() >= 2 && !args[1].isEmpty()) {
        // Dispatch on T's shape: scalar → fast path, matrix → per-pixel.
        if (args[1].numel() == 1) {
            outs[0] = imbinarize(mr, args[0], args[1].toScalar());
        } else {
            outs[0] = imbinarize(mr, args[0], args[1]);
        }
    } else {
        // No threshold given: pick Otsu's automatically.
        auto [t, _] = graythresh(mr, args[0]);
        outs[0] = imbinarize(mr, args[0], t.toScalar());
    }
}

void imquantize_reg(Span<const Value> args, size_t /*nargout*/,
                    Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("imquantize: requires (I, levels)", 0, 0, "imquantize", "",
                    "m:imquantize:nargin");
    outs[0] = imquantize(ctx.engine->resource(), args[0], args[1]);
}

void adaptthresh_reg(Span<const Value> args, size_t /*nargout*/,
                     Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("adaptthresh: requires (I [, sensitivity [, n [, stat]]])",
                    0, 0, "adaptthresh", "", "m:adaptthresh:nargin");
    const double sens = (args.size() >= 2 && !args[1].isEmpty())
                        ? args[1].toScalar() : 0.5;
    const int nbh     = (args.size() >= 3 && !args[2].isEmpty())
                        ? static_cast<int>(args[2].toScalar()) : 0;
    std::string stat  = "mean";
    if (args.size() >= 4 && !args[3].isEmpty()) {
        if (!args[3].isChar() && !args[3].isString())
            throw Error("adaptthresh: statistic must be a string",
                        0, 0, "adaptthresh", "", "m:adaptthresh:type");
        stat = args[3].toString();
    }
    outs[0] = adaptthresh(ctx.engine->resource(), args[0], sens, nbh, stat);
}

} // namespace detail
} // namespace numkit::image
