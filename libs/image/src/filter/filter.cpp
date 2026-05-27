// libs/image/src/filter/filter.cpp

#include <numkit/image/filter/filter.hpp>
#include <numkit/image/type_convert/type_convert.hpp>

#include <numkit/builtin/math/random/rng.hpp>
#include <numkit/signal/convolution/convolution.hpp>
#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include <cctype>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <mutex>
#include <random>
#include <string>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace numkit::image {

namespace {

// Index folder for non-constant pad modes. Returns the source index
// corresponding to the requested (possibly out-of-range) destination
// index. For Replicate, clamp to [0, n-1]; Symmetric mirror without
// duplicating edge; Circular wrap.
inline int fold_index(int i, int n, PadMode m) {
    if (n <= 0) return 0;
    if (i >= 0 && i < n) return i;
    switch (m) {
        case PadMode::Replicate:
            return std::clamp(i, 0, n - 1);
        case PadMode::Symmetric: {
            // Reflect with edge included: ...a b c | c b a | a b c | c b a...
            const int period = 2 * n;
            int j = i % period;
            if (j < 0) j += period;
            if (j >= n) j = period - 1 - j;
            return j;
        }
        case PadMode::Circular: {
            int j = i % n;
            if (j < 0) j += n;
            return j;
        }
        default:
            return std::clamp(i, 0, n - 1);
    }
}

} // anonymous

Value padarray(const Value &x, const std::vector<int> &padsize, PadMode mode, double pad_value, const std::string &direction, std::pmr::memory_resource *mr)
{
    const auto &d = x.dims();
    int H = static_cast<int>(d.rows());
    int W = static_cast<int>(d.cols());
    int P = d.is3D() ? static_cast<int>(d.pages()) : 1;

    int padR = padsize.size() >= 1 ? padsize[0] : 0;
    int padC = padsize.size() >= 2 ? padsize[1] : 0;
    int padP = padsize.size() >= 3 ? padsize[2] : 0;

    bool pre = (direction == "pre"),
         post = (direction == "post");
    bool both = !pre && !post;

    int prR = both ? padR : (pre ? padR : 0);
    int poR = both ? padR : (post ? padR : 0);
    int prC = both ? padC : (pre ? padC : 0);
    int poC = both ? padC : (post ? padC : 0);
    int prP = both ? padP : (pre ? padP : 0);
    int poP = both ? padP : (post ? padP : 0);

    int H2 = H + prR + poR;
    int W2 = W + prC + poC;
    int P2 = P + prP + poP;

    Value out;
    if (P2 == 1) out = Value::matrix(H2, W2, x.type(), mr);
    else         out = Value::matrix3d(H2, W2, P2, x.type(), mr);
    if (H2 == 0 || W2 == 0 || P2 == 0) return out;

    // Read source value at (r, c, p) folded according to mode; for
    // Constant mode, return pad_value when out of range.
    auto src = [&](int r, int c, int p) -> double {
        if (mode == PadMode::Constant) {
            if (r < 0 || r >= H || c < 0 || c >= W || p < 0 || p >= P) return pad_value;
            const size_t plane = static_cast<size_t>(H) * static_cast<size_t>(W);
            const size_t idx = (size_t)p * plane + (size_t)c * H + (size_t)r;
            return x.elemAsDouble(idx);
        }
        const int rs = fold_index(r, H, mode);
        const int cs = fold_index(c, W, mode);
        const int ps = (P == 1) ? 0 : fold_index(p, P, mode);
        const size_t plane = static_cast<size_t>(H) * static_cast<size_t>(W);
        const size_t idx = (size_t)ps * plane + (size_t)cs * H + (size_t)rs;
        return x.elemAsDouble(idx);
    };

    // Storage helpers per output type.
    auto store = [&](size_t outIdx, double v) {
        switch (x.type()) {
            case ValueType::DOUBLE: out.doubleDataMut()[outIdx] = v; break;
            case ValueType::SINGLE: out.singleDataMut()[outIdx] = (float)v; break;
            case ValueType::UINT8: {
                if (v < 0) v = 0; if (v > 255) v = 255;
                out.uint8DataMut()[outIdx] = (uint8_t)std::lround(v);
                break;
            }
            case ValueType::UINT16: {
                if (v < 0) v = 0; if (v > 65535) v = 65535;
                out.uint16DataMut()[outIdx] = (uint16_t)std::lround(v);
                break;
            }
            case ValueType::INT16: {
                if (v < -32768) v = -32768; if (v > 32767) v = 32767;
                out.int16DataMut()[outIdx] = (int16_t)std::lround(v);
                break;
            }
            case ValueType::LOGICAL: {
                out.logicalDataMut()[outIdx] = v != 0.0 ? 1 : 0;
                break;
            }
            default:
                throw Error("padarray: unsupported class", 0, 0, "padarray", "",
                            "m:padarray:badtype");
        }
    };

    const size_t plane2 = (size_t)H2 * (size_t)W2;
    for (int p = 0; p < P2; ++p) {
        for (int c = 0; c < W2; ++c) {
            for (int r = 0; r < H2; ++r) {
                const double v = src(r - prR, c - prC, P == 1 ? 0 : p - prP);
                const size_t outIdx = (size_t)p * plane2 + (size_t)c * H2 + (size_t)r;
                store(outIdx, v);
            }
        }
    }
    return out;
}

// ════════════════════════════════════════════════════════════════════
// fspecial — kernel factory
// ════════════════════════════════════════════════════════════════════

namespace {

Value mat_double(const std::vector<double> &v, int rows, int cols, std::pmr::memory_resource *mr) {
    Value out = Value::matrix(rows, cols, ValueType::DOUBLE, mr);
    if (!v.empty()) std::memcpy(out.doubleDataMut(), v.data(), v.size() * sizeof(double));
    return out;
}

Value fspecial_average(int rows, int cols, std::pmr::memory_resource *mr) {
    const double w = 1.0 / (double(rows) * double(cols));
    std::vector<double> data(size_t(rows) * size_t(cols), w);
    return mat_double(data, rows, cols, mr);
}

Value fspecial_gaussian(int rows, int cols, double sigma, std::pmr::memory_resource *mr) {
    if (sigma <= 0.0) throw Error("fspecial: sigma must be positive",
                                  0, 0, "fspecial", "", "m:fspecial:sigma");
    const double cy = (rows - 1) / 2.0;
    const double cx = (cols - 1) / 2.0;
    const double inv2s2 = 1.0 / (2.0 * sigma * sigma);
    std::vector<double> k(size_t(rows) * size_t(cols), 0.0);
    double sum = 0.0;
    for (int c = 0; c < cols; ++c)
        for (int r = 0; r < rows; ++r) {
            const double dy = r - cy, dx = c - cx;
            const double v = std::exp(-(dx * dx + dy * dy) * inv2s2);
            k[size_t(c) * size_t(rows) + size_t(r)] = v;
            sum += v;
        }
    if (sum > 0.0) for (auto &v : k) v /= sum;
    return mat_double(k, rows, cols, mr);
}

Value fspecial_laplacian(double alpha, std::pmr::memory_resource *mr) {
    // 3×3 Laplacian, controlled by alpha ∈ [0, 1] (default 0.2 in MATLAB).
    if (alpha < 0.0) alpha = 0.0; if (alpha > 1.0) alpha = 1.0;
    const double a = alpha;
    const double s = 1.0 / (a + 1.0);
    std::vector<double> k = {
        s * a / 4.0,         s * (1.0 - a) / 4.0, s * a / 4.0,         // col 0
        s * (1.0 - a) / 4.0, s * (-1.0),          s * (1.0 - a) / 4.0, // col 1
        s * a / 4.0,         s * (1.0 - a) / 4.0, s * a / 4.0,         // col 2
    };
    return mat_double(k, 3, 3, mr);
}

Value fspecial_log(int rows, int cols, double sigma, std::pmr::memory_resource *mr) {
    // Laplacian-of-Gaussian. Convention from MATLAB: zero-mean, normalized.
    const double cy = (rows - 1) / 2.0;
    const double cx = (cols - 1) / 2.0;
    const double s2 = sigma * sigma;
    std::vector<double> g(size_t(rows) * size_t(cols), 0.0);
    std::vector<double> log_k(size_t(rows) * size_t(cols), 0.0);
    double gsum = 0.0;
    for (int c = 0; c < cols; ++c)
        for (int r = 0; r < rows; ++r) {
            const double dy = r - cy, dx = c - cx;
            const double rsq = dx * dx + dy * dy;
            const double e = std::exp(-rsq / (2.0 * s2));
            g[size_t(c) * size_t(rows) + size_t(r)] = e;
            gsum += e;
        }
    if (gsum > 0.0) for (auto &v : g) v /= gsum;
    for (int c = 0; c < cols; ++c)
        for (int r = 0; r < rows; ++r) {
            const double dy = r - cy, dx = c - cx;
            const double rsq = dx * dx + dy * dy;
            const size_t i = size_t(c) * size_t(rows) + size_t(r);
            log_k[i] = (rsq - 2.0 * s2) / (s2 * s2) * g[i];
        }
    // Mean subtraction so kernel sums to zero.
    double mean = 0.0;
    for (auto v : log_k) mean += v;
    mean /= double(log_k.size());
    for (auto &v : log_k) v -= mean;
    return mat_double(log_k, rows, cols, mr);
}

Value fspecial_sobel(std::pmr::memory_resource *mr) {
    // MATLAB convention: [1 2 1; 0 0 0; -1 -2 -1]. Column-major storage:
    // col0 = [1, 0, -1], col1 = [2, 0, -2], col2 = [1, 0, -1].
    std::vector<double> k = {1, 0, -1,   2, 0, -2,   1, 0, -1};
    return mat_double(k, 3, 3, mr);
}

Value fspecial_prewitt(std::pmr::memory_resource *mr) {
    // [1 1 1; 0 0 0; -1 -1 -1] column-major.
    std::vector<double> k = {1, 0, -1,   1, 0, -1,   1, 0, -1};
    return mat_double(k, 3, 3, mr);
}

Value fspecial_disk(double radius, std::pmr::memory_resource *mr) {
    if (radius <= 0.0) throw Error("fspecial: radius must be positive",
                                   0, 0, "fspecial", "", "m:fspecial:radius");
    const int side = 2 * int(std::ceil(radius)) + 1;
    const double c = (side - 1) / 2.0;
    std::vector<double> k(size_t(side) * size_t(side), 0.0);
    double sum = 0.0;
    for (int j = 0; j < side; ++j)
        for (int i = 0; i < side; ++i) {
            const double dy = i - c, dx = j - c;
            const double r = std::hypot(dy, dx);
            // Approximate area inside disk for each cell using simple
            // sampling: 1.0 if r < radius - 0.5, 0.0 if r > radius + 0.5,
            // smooth between (linear taper).
            double w = 0.0;
            if (r < radius - 0.5) w = 1.0;
            else if (r < radius + 0.5) w = (radius + 0.5 - r);
            k[size_t(j) * size_t(side) + size_t(i)] = w;
            sum += w;
        }
    if (sum > 0.0) for (auto &v : k) v /= sum;
    return mat_double(k, side, side, mr);
}

} // anonymous

Value fspecial(const std::string &type, const std::vector<double> &params, std::pmr::memory_resource *mr)
{
    if (type == "average") {
        int rows = params.size() >= 1 ? int(params[0]) : 3;
        int cols = params.size() >= 2 ? int(params[1]) : rows;
        return fspecial_average(rows, cols, mr);
    }
    if (type == "gaussian") {
        int rows = params.size() >= 1 ? int(params[0]) : 3;
        int cols = params.size() >= 2 ? int(params[1]) : rows;
        double sigma = params.size() >= 3 ? params[2] : 0.5;
        return fspecial_gaussian(rows, cols, sigma, mr);
    }
    if (type == "laplacian") {
        double alpha = params.size() >= 1 ? params[0] : 0.2;
        return fspecial_laplacian(alpha, mr);
    }
    if (type == "log") {
        int rows = params.size() >= 1 ? int(params[0]) : 5;
        int cols = params.size() >= 2 ? int(params[1]) : rows;
        double sigma = params.size() >= 3 ? params[2] : 0.5;
        return fspecial_log(rows, cols, sigma, mr);
    }
    if (type == "sobel")   return fspecial_sobel(mr);
    if (type == "prewitt") return fspecial_prewitt(mr);
    if (type == "disk") {
        double radius = params.size() >= 1 ? params[0] : 5.0;
        return fspecial_disk(radius, mr);
    }
    throw Error("fspecial: unknown filter type '" + type + "'",
                0, 0, "fspecial", "", "m:fspecial:badtype");
}

// ════════════════════════════════════════════════════════════════════
// imfilter / imgaussfilt / imboxfilt / medfilt2
// ════════════════════════════════════════════════════════════════════

namespace {

inline double sample_padded(const Value &I, int r, int c, int H, int W,
                            PadMode mode, double pv) {
    if (mode == PadMode::Constant) {
        if (r < 0 || r >= H || c < 0 || c >= W) return pv;
    } else {
        r = fold_index(r, H, mode);
        c = fold_index(c, W, mode);
    }
    return I.elemAsDouble((size_t)c * (size_t)H + (size_t)r);
}

inline void store_classed(Value &out, size_t i, double v, ValueType t) {
    switch (t) {
        case ValueType::DOUBLE: out.doubleDataMut()[i] = v; break;
        case ValueType::SINGLE: out.singleDataMut()[i] = (float)v; break;
        case ValueType::UINT8: {
            if (v < 0) v = 0; if (v > 255) v = 255;
            out.uint8DataMut()[i] = (uint8_t)std::lround(v); break;
        }
        case ValueType::UINT16: {
            if (v < 0) v = 0; if (v > 65535) v = 65535;
            out.uint16DataMut()[i] = (uint16_t)std::lround(v); break;
        }
        case ValueType::INT16: {
            if (v < -32768) v = -32768; if (v > 32767) v = 32767;
            out.int16DataMut()[i] = (int16_t)std::lround(v); break;
        }
        default:
            throw Error("imfilter: unsupported class", 0, 0, "imfilter", "",
                        "m:imfilter:badtype");
    }
}

} // anonymous

Value imfilter(const Value &I, const Value &h, PadMode boundary, double pad_value, bool full, bool flip_kernel, std::pmr::memory_resource *mr)
{
    const int H = (int)I.dims().rows();
    const int W = (int)I.dims().cols();
    const int kH = (int)h.dims().rows();
    const int kW = (int)h.dims().cols();
    const int half_r = kH / 2;
    const int half_c = kW / 2;

    int outH, outW;
    if (full) { outH = H + kH - 1; outW = W + kW - 1; }
    else      { outH = H; outW = W; }

    Value out = Value::matrix(outH, outW, I.type(), mr);
    if (outH == 0 || outW == 0) return out;

    // Read kernel into a contiguous double buffer.
    std::vector<double> kbuf((size_t)kH * (size_t)kW);
    for (size_t i = 0; i < kbuf.size(); ++i) kbuf[i] = h.elemAsDouble(i);

    // For 'same' correlation: output(r,c) = Σ_{ki, kj} kernel[ki, kj]
    //                                       · I[r + ki - half_r, c + kj - half_c].
    // For 'conv': flip the kernel by 180° before applying.
    auto getK = [&](int ki, int kj) -> double {
        const int kki = flip_kernel ? (kH - 1 - ki) : ki;
        const int kkj = flip_kernel ? (kW - 1 - kj) : kj;
        return kbuf[(size_t)kkj * (size_t)kH + (size_t)kki];
    };

    if (full) {
        // 'full' mode: index translation inside the larger output grid.
        for (int oc = 0; oc < outW; ++oc) {
            for (int orow = 0; orow < outH; ++orow) {
                double s = 0.0;
                for (int kj = 0; kj < kW; ++kj) {
                    const int c_in = oc - kj;
                    if (c_in < 0 || c_in >= W) continue;
                    for (int ki = 0; ki < kH; ++ki) {
                        const int r_in = orow - ki;
                        if (r_in < 0 || r_in >= H) continue;
                        s += getK(ki, kj) * I.elemAsDouble((size_t)c_in * (size_t)H + (size_t)r_in);
                    }
                }
                store_classed(out, (size_t)oc * (size_t)outH + (size_t)orow, s, I.type());
            }
        }
    } else {
        for (int oc = 0; oc < W; ++oc) {
            for (int orow = 0; orow < H; ++orow) {
                double s = 0.0;
                for (int kj = 0; kj < kW; ++kj) {
                    const int c_in = oc + kj - half_c;
                    for (int ki = 0; ki < kH; ++ki) {
                        const int r_in = orow + ki - half_r;
                        s += getK(ki, kj) * sample_padded(I, r_in, c_in, H, W,
                                                          boundary, pad_value);
                    }
                }
                store_classed(out, (size_t)oc * (size_t)H + (size_t)orow, s, I.type());
            }
        }
    }
    return out;
}

Value imgaussfilt(const Value &I, double sigma, int filter_size, std::pmr::memory_resource *mr)
{
    if (filter_size <= 0) {
        // Default: 2·ceil(2σ) + 1.
        filter_size = 2 * (int)std::ceil(2.0 * sigma) + 1;
        if (filter_size < 3) filter_size = 3;
    }
    if (filter_size % 2 == 0) filter_size += 1;
    Value k = fspecial_gaussian(filter_size, filter_size, sigma, mr);
    return imfilter(I, k, PadMode::Replicate, 0.0, /*full=*/false, /*flip_kernel=*/false, mr);
}

Value imboxfilt(const Value &I, int filter_size, std::pmr::memory_resource *mr)
{
    if (filter_size <= 0) filter_size = 3;
    if (filter_size % 2 == 0) filter_size += 1;
    Value k = fspecial_average(filter_size, filter_size, mr);
    return imfilter(I, k, PadMode::Replicate, 0.0, /*full=*/false, /*flip_kernel=*/false, mr);
}

Value medfilt2(const Value &I, int rows, int cols, std::pmr::memory_resource *mr)
{
    const int H = (int)I.dims().rows();
    const int W = (int)I.dims().cols();
    const int half_r = rows / 2;
    const int half_c = cols / 2;
    Value out = Value::matrix(H, W, I.type(), mr);
    if (H == 0 || W == 0) return out;

    std::vector<double> window;
    window.reserve((size_t)rows * (size_t)cols);

    for (int oc = 0; oc < W; ++oc) {
        for (int orow = 0; orow < H; ++orow) {
            window.clear();
            for (int kj = 0; kj < cols; ++kj) {
                for (int ki = 0; ki < rows; ++ki) {
                    const int r_in = orow + ki - half_r;
                    const int c_in = oc + kj - half_c;
                    // MATLAB medfilt2 default: zero-pad outside.
                    if (r_in < 0 || r_in >= H || c_in < 0 || c_in >= W) {
                        window.push_back(0.0);
                    } else {
                        window.push_back(I.elemAsDouble((size_t)c_in * (size_t)H + (size_t)r_in));
                    }
                }
            }
            std::nth_element(window.begin(),
                             window.begin() + window.size() / 2,
                             window.end());
            double med = window[window.size() / 2];
            if ((window.size() & 1) == 0) {
                // Even count: average of two middle values. nth_element put
                // the upper-middle in place; find max of the lower half.
                double left_max = -std::numeric_limits<double>::infinity();
                for (auto it = window.begin();
                     it != window.begin() + window.size() / 2; ++it)
                    if (*it > left_max) left_max = *it;
                med = 0.5 * (left_max + med);
            }
            store_classed(out, (size_t)oc * (size_t)H + (size_t)orow, med, I.type());
        }
    }
    return out;
}

// ════════════════════════════════════════════════════════════════════
// imsharpen — unsharp masking
// ════════════════════════════════════════════════════════════════════
//
//   high  = I − imgaussfilt(I, sigma=radius)
//   mask  = (|high| ≥ threshold · max|high|)            // 1 if no thresh
//   B     = saturate(I + amount · mask · high)
//
// MATLAB R2025b defaults: radius=1, amount=0.8, threshold=0. Output
// has the same class as the input (saturating cast for ints).

Value imsharpen(const Value &I, double radius, double amount, double threshold, std::pmr::memory_resource *mr)
{
    if (!(radius > 0.0)) radius = 1.0;
    if (!(threshold >= 0.0 && threshold <= 1.0))
        throw Error("imsharpen: threshold must be in [0, 1]",
                    0, 0, "imsharpen", "", "m:imsharpen:threshold");

    // Gaussian filter size: 2*ceil(2*sigma)+1 — MATLAB default.
    int fs = 2 * (int)std::ceil(2.0 * radius) + 1;
    if (fs < 3) fs = 3;
    if (fs % 2 == 0) fs += 1;

    Value blurred = imgaussfilt(I, radius, fs, mr);

    const size_t Hh = I.dims().rows();
    const size_t Ww = I.dims().cols();
    const size_t N = I.numel();

    // High-pass component, in DOUBLE for headroom.
    std::vector<double> Hbuf(N);
    double max_abs = 0.0;
    for (size_t i = 0; i < N; ++i) {
        const double v = I.elemAsDouble(i) - blurred.elemAsDouble(i);
        Hbuf[i] = v;
        const double a = std::abs(v);
        if (a > max_abs) max_abs = a;
    }
    const double thr_abs = threshold * max_abs;

    Value out = Value::matrix(Hh, Ww, I.type(), mr);
    for (size_t i = 0; i < N; ++i) {
        double h = Hbuf[i];
        if (threshold > 0.0 && std::abs(h) < thr_abs) h = 0.0;
        const double v = I.elemAsDouble(i) + amount * h;
        store_classed(out, i, v, I.type());
    }
    return out;
}

// ════════════════════════════════════════════════════════════════════
// im2col — rearrange neighborhoods into matrix columns
// ════════════════════════════════════════════════════════════════════
//
// MATLAB convention: input A is H×W column-major; block size is m×n
// (m=rows, n=cols).
//
// "sliding": output has m·n rows × (H − m + 1)·(W − n + 1) cols.
//   Column index iterates column-major over top-left block positions
//   (r0=0..H−m, c0=0..W−n with r0 fastest); within each column the
//   block is laid out column-major (br=0..m−1, bc=0..n−1, br fastest).
// "distinct": A is partitioned into ceil(H/m) × ceil(W/n) tiles,
//   zero-padded at the right/bottom when needed. Output is m·n ×
//   ceil(H/m)·ceil(W/n). Same column-major ordering rules.

namespace {

template <typename T>
inline T elem_typed(const Value &A, size_t i);

template <> inline double  elem_typed<double>(const Value &A, size_t i)
                          { return A.doubleData()[i]; }
template <> inline float   elem_typed<float> (const Value &A, size_t i)
                          { return A.singleData()[i]; }
template <> inline std::uint8_t elem_typed<std::uint8_t>(const Value &A, size_t i)
                          { return A.uint8Data()[i]; }
template <> inline std::uint16_t elem_typed<std::uint16_t>(const Value &A, size_t i)
                          { return A.uint16Data()[i]; }
template <> inline std::int16_t  elem_typed<std::int16_t>(const Value &A, size_t i)
                          { return A.int16Data()[i]; }

template <typename T>
inline T *out_typed_mut(Value &B);
template <> inline double  *out_typed_mut<double>(Value &B) { return B.doubleDataMut(); }
template <> inline float   *out_typed_mut<float> (Value &B) { return B.singleDataMut(); }
template <> inline std::uint8_t *out_typed_mut<std::uint8_t>(Value &B)
                                                       { return B.uint8DataMut(); }
template <> inline std::uint16_t *out_typed_mut<std::uint16_t>(Value &B)
                                                        { return B.uint16DataMut(); }
template <> inline std::int16_t  *out_typed_mut<std::int16_t>(Value &B)
                                                       { return B.int16DataMut(); }

template <typename T>
void im2col_sliding_typed(const Value &A, Value &B, size_t H, size_t W,
                           int m, int n)
{
    const size_t outH = (size_t)(m * n);
    const size_t Hp = H - (size_t)m + 1;
    const size_t Wp = W - (size_t)n + 1;
    T *bd = out_typed_mut<T>(B);
    size_t col = 0;
    for (size_t c0 = 0; c0 < Wp; ++c0) {
        for (size_t r0 = 0; r0 < Hp; ++r0) {
            // Within-block: column-major (bc outer, br inner).
            size_t off = col * outH;
            for (int bc = 0; bc < n; ++bc) {
                const size_t Acol = (c0 + (size_t)bc) * H;
                for (int br = 0; br < m; ++br) {
                    bd[off++] = elem_typed<T>(A, Acol + r0 + (size_t)br);
                }
            }
            ++col;
        }
    }
}

template <typename T>
void im2col_distinct_typed(const Value &A, Value &B, size_t H, size_t W,
                            int m, int n)
{
    const size_t outH = (size_t)(m * n);
    const size_t Hb = (H + (size_t)m - 1) / (size_t)m;
    const size_t Wb = (W + (size_t)n - 1) / (size_t)n;
    T *bd = out_typed_mut<T>(B);
    // Output starts zero-initialised by Value::matrix; we just write
    // the in-bounds elements.
    size_t col = 0;
    for (size_t bj = 0; bj < Wb; ++bj) {
        for (size_t bi = 0; bi < Hb; ++bi) {
            size_t off = col * outH;
            for (int bc = 0; bc < n; ++bc) {
                const size_t cc = bj * (size_t)n + (size_t)bc;
                if (cc >= W) { off += (size_t)m; continue; }
                const size_t Acol = cc * H;
                for (int br = 0; br < m; ++br) {
                    const size_t rr = bi * (size_t)m + (size_t)br;
                    if (rr < H) bd[off] = elem_typed<T>(A, Acol + rr);
                    ++off;
                }
            }
            ++col;
        }
    }
}

} // anonymous

Value im2col(const Value &A, int m, int n, const std::string &block_type, std::pmr::memory_resource *mr)
{
    if (m <= 0 || n <= 0)
        throw Error("im2col: block size must be positive",
                    0, 0, "im2col", "", "m:im2col:size");
    const size_t H = A.dims().rows();
    const size_t W = A.dims().cols();
    const ValueType T = A.type();

    size_t outH = (size_t)(m * n);
    size_t outW;
    bool sliding = (block_type == "sliding" || block_type.empty());
    if (sliding) {
        if ((size_t)m > H || (size_t)n > W)
            throw Error("im2col(sliding): block larger than image",
                        0, 0, "im2col", "", "m:im2col:size");
        outW = (H - (size_t)m + 1) * (W - (size_t)n + 1);
    } else if (block_type == "distinct") {
        const size_t Hb = (H + (size_t)m - 1) / (size_t)m;
        const size_t Wb = (W + (size_t)n - 1) / (size_t)n;
        outW = Hb * Wb;
    } else {
        throw Error("im2col: block_type must be 'sliding' or 'distinct'",
                    0, 0, "im2col", "", "m:im2col:type");
    }

    Value B = Value::matrix(outH, outW, T, mr);
    if (outH == 0 || outW == 0) return B;

    auto run = [&](auto tag) {
        using ET = decltype(tag);
        if (sliding) im2col_sliding_typed<ET>(A, B, H, W, m, n);
        else         im2col_distinct_typed<ET>(A, B, H, W, m, n);
    };

    switch (T) {
        case ValueType::DOUBLE: run(double{}); break;
        case ValueType::SINGLE: run(float{});  break;
        case ValueType::UINT8:  run(std::uint8_t{});  break;
        case ValueType::UINT16: run(std::uint16_t{}); break;
        case ValueType::INT16:  run(std::int16_t{});  break;
        case ValueType::LOGICAL: {
            // Treat LOGICAL as uint8 (same storage).
            run(std::uint8_t{});
            break;
        }
        default:
            throw Error("im2col: unsupported class",
                        0, 0, "im2col", "", "m:im2col:badtype");
    }
    return B;
}

// ════════════════════════════════════════════════════════════════════
// imbilatfilt — bilateral (edge-preserving) filter
// ════════════════════════════════════════════════════════════════════
//
// Per-pixel weighted average where the weight is the product of a
// spatial Gaussian and a range Gaussian over intensity difference:
//
//   J(x) = (1/W) · sum_y[ exp(−d²/(2σ_s²)) · exp(−Δ²/(2·dos)) · I(y) ]
//   W    = sum_y[ exp(−d²/(2σ_s²)) · exp(−Δ²/(2·dos)) ]
//
// where d is the spatial offset, Δ = I(x) − I(y), σ_s = spatialSigma,
// dos = degreeOfSmoothing (interpreted as the range Gaussian's
// VARIANCE, matching MATLAB R2025b). Boundary mode = replicate.
//
// Window: 2·ceil(2·σ_s) + 1 in each axis. Spatial-weight table is
// precomputed once per call. Range weight is per (centre, neighbour)
// pair — we eat the per-pixel exp; same cost as MATLAB's reference.

Value imbilatfilt(const Value &I, double degreeOfSmoothing, double spatialSigma, std::pmr::memory_resource *mr)
{
    if (!(spatialSigma > 0.0)) spatialSigma = 1.0;
    if (!(degreeOfSmoothing > 0.0))
        throw Error("imbilatfilt: degreeOfSmoothing must be > 0",
                    0, 0, "imbilatfilt", "", "m:imbilatfilt:dos");

    const size_t H = I.dims().rows();
    const size_t W = I.dims().cols();
    const size_t N = I.numel();
    Value out = Value::matrix(H, W, I.type(), mr);
    if (N == 0) return out;

    const int half = std::max(1, (int)std::ceil(2.0 * spatialSigma));
    const int win = 2 * half + 1;
    const double inv2ss = 0.5 / (spatialSigma * spatialSigma);
    const double inv2dos = 0.5 / degreeOfSmoothing;

    // Precompute spatial weights — flat (win*win) row-major (di, dj).
    std::vector<double> sw((size_t)win * (size_t)win);
    for (int di = -half; di <= half; ++di)
        for (int dj = -half; dj <= half; ++dj)
            sw[(size_t)(di + half) * (size_t)win + (size_t)(dj + half)]
                = std::exp(-(double)(di*di + dj*dj) * inv2ss);

    auto clamp = [](int v, int lo, int hi) {
        if (v < lo) return lo;
        if (v > hi) return hi;
        return v;
    };

    for (size_t oc = 0; oc < W; ++oc) {
        for (size_t orow = 0; orow < H; ++orow) {
            const double Ic = I.elemAsDouble(oc * H + orow);
            double sum_w = 0.0, sum_v = 0.0;
            for (int dj = -half; dj <= half; ++dj) {
                const int c = clamp((int)oc + dj, 0, (int)W - 1);
                for (int di = -half; di <= half; ++di) {
                    const int r = clamp((int)orow + di, 0, (int)H - 1);
                    const double v = I.elemAsDouble((size_t)c * H + (size_t)r);
                    const double dI = v - Ic;
                    const double rw = std::exp(-(dI * dI) * inv2dos);
                    const double w = sw[(size_t)(di + half) * (size_t)win
                                          + (size_t)(dj + half)] * rw;
                    sum_w += w;
                    sum_v += w * v;
                }
            }
            const double j = (sum_w > 0.0) ? sum_v / sum_w : Ic;
            store_classed(out, oc * H + orow, j, I.type());
        }
    }
    return out;
}

// ════════════════════════════════════════════════════════════════════
// col2im — inverse of im2col
// ════════════════════════════════════════════════════════════════════
//
// Two modes mirroring im2col:
//
// "sliding"  — B is 1 × (mm−m+1)·(nn−n+1). Each column of B is a
//              scalar (already-reduced) filter response for one
//              window. Output is just a reshape into (mm−m+1) ×
//              (nn−n+1), column-major. (im2col + correlation per
//              column = col2im(sliding) of the row of dot-products.)
//
// "distinct" — B is m·n × Hb·Wb where Hb=⌈mm/m⌉, Wb=⌈nn/n⌉. Each
//              column was an m×n tile (column-major). Output is mm
//              × nn — we drop the trailing rows/cols of the last
//              row-tile / col-tile, which is where im2col padded
//              with zeros.

namespace {

template <typename T>
inline T elem_typed_data(const Value &B, size_t i);
template <> inline double  elem_typed_data<double>(const Value &B, size_t i)
                          { return B.doubleData()[i]; }
template <> inline float   elem_typed_data<float> (const Value &B, size_t i)
                          { return B.singleData()[i]; }
template <> inline std::uint8_t elem_typed_data<std::uint8_t>(const Value &B, size_t i)
                          { return B.uint8Data()[i]; }
template <> inline std::uint16_t elem_typed_data<std::uint16_t>(const Value &B, size_t i)
                          { return B.uint16Data()[i]; }
template <> inline std::int16_t  elem_typed_data<std::int16_t>(const Value &B, size_t i)
                          { return B.int16Data()[i]; }

template <typename T>
void col2im_distinct_typed(const Value &B, Value &A, size_t mm, size_t nn,
                            int m, int n)
{
    const size_t Hb = (mm + (size_t)m - 1) / (size_t)m;
    T *ad = out_typed_mut<T>(A);
    const size_t outH = (size_t)(m * n);
    size_t col = 0;
    for (size_t bj = 0; bj < (nn + (size_t)n - 1) / (size_t)n; ++bj) {
        for (size_t bi = 0; bi < Hb; ++bi) {
            size_t off = col * outH;
            for (int bc = 0; bc < n; ++bc) {
                const size_t cc = bj * (size_t)n + (size_t)bc;
                if (cc >= nn) { off += (size_t)m; continue; }
                const size_t Acol = cc * mm;
                for (int br = 0; br < m; ++br) {
                    const size_t rr = bi * (size_t)m + (size_t)br;
                    if (rr < mm) ad[Acol + rr] = elem_typed_data<T>(B, off);
                    ++off;
                }
            }
            ++col;
        }
    }
}

} // anonymous

Value col2im(const Value &B, int m, int n, int mm, int nn, const std::string &block_type, std::pmr::memory_resource *mr)
{
    if (m <= 0 || n <= 0 || mm <= 0 || nn <= 0)
        throw Error("col2im: dimensions must be positive",
                    0, 0, "col2im", "", "m:col2im:size");
    const size_t MM = (size_t)mm;
    const size_t NN = (size_t)nn;
    const ValueType T = B.type();

    bool sliding = (block_type == "sliding" || block_type.empty());
    if (sliding) {
        if ((size_t)m > MM || (size_t)n > NN)
            throw Error("col2im(sliding): block larger than output image",
                        0, 0, "col2im", "", "m:col2im:size");
        const size_t Hp = MM - (size_t)m + 1;
        const size_t Wp = NN - (size_t)n + 1;
        if (B.numel() != Hp * Wp)
            throw Error("col2im(sliding): B size mismatch — expected 1×(mm−m+1)·(nn−n+1)",
                        0, 0, "col2im", "", "m:col2im:nelems");
        Value A = Value::matrix(Hp, Wp, T, mr);
        // Both A and B are column-major linearised; values flow 1:1.
        const size_t N = Hp * Wp;
        switch (T) {
            case ValueType::DOUBLE:
                std::memcpy(A.doubleDataMut(), B.doubleData(),
                            N * sizeof(double)); break;
            case ValueType::SINGLE:
                std::memcpy(A.singleDataMut(), B.singleData(),
                            N * sizeof(float)); break;
            case ValueType::UINT8:
                std::memcpy(A.uint8DataMut(), B.uint8Data(), N); break;
            case ValueType::UINT16:
                std::memcpy(A.uint16DataMut(), B.uint16Data(),
                            N * sizeof(std::uint16_t)); break;
            case ValueType::INT16:
                std::memcpy(A.int16DataMut(), B.int16Data(),
                            N * sizeof(std::int16_t)); break;
            case ValueType::LOGICAL:
                std::memcpy(A.logicalDataMut(), B.logicalData(), N); break;
            default:
                throw Error("col2im: unsupported class",
                            0, 0, "col2im", "", "m:col2im:badtype");
        }
        return A;
    }

    if (block_type != "distinct")
        throw Error("col2im: block_type must be 'sliding' or 'distinct'",
                    0, 0, "col2im", "", "m:col2im:type");

    const size_t Hb = (MM + (size_t)m - 1) / (size_t)m;
    const size_t Wb = (NN + (size_t)n - 1) / (size_t)n;
    const size_t expRows = (size_t)(m * n);
    const size_t expCols = Hb * Wb;
    if (B.dims().rows() != expRows || B.dims().cols() != expCols)
        throw Error("col2im(distinct): B shape mismatch — expected m·n × ⌈mm/m⌉·⌈nn/n⌉",
                    0, 0, "col2im", "", "m:col2im:shape");

    Value A = Value::matrix(MM, NN, T, mr);
    if (MM == 0 || NN == 0) return A;

    auto run = [&](auto tag) {
        using ET = decltype(tag);
        col2im_distinct_typed<ET>(B, A, MM, NN, m, n);
    };
    switch (T) {
        case ValueType::DOUBLE: run(double{}); break;
        case ValueType::SINGLE: run(float{});  break;
        case ValueType::UINT8:  run(std::uint8_t{});  break;
        case ValueType::UINT16: run(std::uint16_t{}); break;
        case ValueType::INT16:  run(std::int16_t{});  break;
        case ValueType::LOGICAL: run(std::uint8_t{}); break;
        default:
            throw Error("col2im: unsupported class",
                        0, 0, "col2im", "", "m:col2im:badtype");
    }
    return A;
}

// ════════════════════════════════════════════════════════════════════
// imnoise — additive / multiplicative / shot noise
// ════════════════════════════════════════════════════════════════════
//
// All modes operate on the unit-range [0, 1] interpretation of I:
//   integer classes are scaled by their max (255 for uint8, 65535 for
//   uint16, 32768±32767 for int16); double/single are taken as-is.
// Output is saturated back to the input class.
//
// Modes (matching MATLAB R2025b imnoise semantics):
//   gaussian       J = I + m + sqrt(var) * N(0, 1)
//   localvar       J = I + sqrt(V[x]) * N(0, 1)         V is variance map
//   salt & pepper  fraction d / 2 of pixels each set to 0 and 1
//   speckle        J = I + I * sqrt(var) * N(0, 1)      multiplicative
//   poisson        J = Poisson(I * scale) / scale       scale per class

Value imnoise(const Value &I, const std::string &mode, const Value &p1, const Value &p2, std::pmr::memory_resource *mr)
{
    const ValueType T = I.type();
    const size_t H = I.dims().rows();
    const size_t W = I.dims().cols();
    const size_t N = I.numel();

    auto toUnit = [&](double v) -> double {
        switch (T) {
            case ValueType::UINT8:   return v / 255.0;
            case ValueType::UINT16:  return v / 65535.0;
            case ValueType::INT16:   return (v + 32768.0) / 65535.0;
            case ValueType::LOGICAL: return v != 0.0 ? 1.0 : 0.0;
            default:                 return v;
        }
    };
    auto fromUnit = [&](Value &out, size_t i, double v) {
        if (v < 0.0) v = 0.0;
        if (v > 1.0) v = 1.0;
        switch (T) {
            case ValueType::UINT8:
                out.uint8DataMut()[i] =
                    (std::uint8_t)std::lround(v * 255.0); break;
            case ValueType::UINT16:
                out.uint16DataMut()[i] =
                    (std::uint16_t)std::lround(v * 65535.0); break;
            case ValueType::INT16: {
                double w = std::lround(v * 65535.0) - 32768.0;
                if (w < -32768.0) w = -32768.0;
                if (w >  32767.0) w =  32767.0;
                out.int16DataMut()[i] = (std::int16_t)w; break;
            }
            case ValueType::LOGICAL:
                out.logicalDataMut()[i] = v >= 0.5 ? 1u : 0u; break;
            case ValueType::SINGLE:
                out.singleDataMut()[i] = (float)v; break;
            default:
                out.doubleDataMut()[i] = v; break;
        }
    };

    Value out = Value::matrix(H, W, T, mr);

    auto &rng = numkit::builtin::sharedEngine();
    std::lock_guard<std::mutex> lk(numkit::builtin::rngMutex());

    if (mode == "gaussian") {
        const double m = (p1.numel() > 0) ? p1.toScalar() : 0.0;
        const double v = (p2.numel() > 0) ? p2.toScalar() : 0.01;
        std::normal_distribution<double> Z(0.0, std::sqrt(std::max(v, 0.0)));
        for (size_t i = 0; i < N; ++i) {
            const double x = toUnit(I.elemAsDouble(i));
            fromUnit(out, i, x + m + Z(rng));
        }
    }
    else if (mode == "salt & pepper" || mode == "salt&pepper") {
        const double d = (p1.numel() > 0) ? p1.toScalar() : 0.05;
        std::uniform_real_distribution<double> U(0.0, 1.0);
        for (size_t i = 0; i < N; ++i) {
            double x = toUnit(I.elemAsDouble(i));
            const double u = U(rng);
            if      (u < d * 0.5) x = 0.0;       // pepper
            else if (u < d)       x = 1.0;       // salt
            fromUnit(out, i, x);
        }
    }
    else if (mode == "speckle") {
        const double v = (p1.numel() > 0) ? p1.toScalar() : 0.04;
        std::normal_distribution<double> Z(0.0, std::sqrt(std::max(v, 0.0)));
        for (size_t i = 0; i < N; ++i) {
            const double x = toUnit(I.elemAsDouble(i));
            fromUnit(out, i, x + x * Z(rng));
        }
    }
    else if (mode == "poisson") {
        // Scale per class: integer classes use their max as the count
        // unit; double/single use a large fixed scale so noise is small.
        double scale;
        switch (T) {
            case ValueType::UINT8:   scale = 255.0;   break;
            case ValueType::UINT16:  scale = 65535.0; break;
            default:                 scale = 1e12;    break;
        }
        for (size_t i = 0; i < N; ++i) {
            const double x = toUnit(I.elemAsDouble(i));
            const double mean = std::max(x * scale, 0.0);
            std::poisson_distribution<long long> P(mean);
            const long long k = P(rng);
            fromUnit(out, i, (double)k / scale);
        }
    }
    else if (mode == "localvar") {
        if (p1.numel() != N)
            throw Error("imnoise('localvar', V): V must match I in size",
                        0, 0, "imnoise", "", "m:imnoise:localvar");
        for (size_t i = 0; i < N; ++i) {
            const double x = toUnit(I.elemAsDouble(i));
            const double v = std::max(p1.elemAsDouble(i), 0.0);
            std::normal_distribution<double> Z(0.0, std::sqrt(v));
            fromUnit(out, i, x + Z(rng));
        }
    }
    else {
        throw Error("imnoise: unknown mode '" + mode + "'",
                    0, 0, "imnoise", "", "m:imnoise:mode");
    }
    return out;
}

namespace {

// Build a list of (dr, dc) offsets for the non-zero entries of a
// 2-D `domain` mask, plus the kernel half-extents.
struct DomainOffsets {
    std::vector<std::pair<int, int>> offs;
    int rh = 0, ch = 0;
};

DomainOffsets domain_offsets(const Value &domain) {
    DomainOffsets r;
    const size_t kH = domain.dims().rows();
    const size_t kW = domain.dims().cols();
    r.rh = static_cast<int>(kH) / 2;
    r.ch = static_cast<int>(kW) / 2;
    r.offs.reserve(kH * kW);
    for (size_t c = 0; c < kW; ++c)
        for (size_t row = 0; row < kH; ++row)
            if (domain.elemAsDouble(c * kH + row) != 0.0)
                r.offs.push_back({static_cast<int>(row) - r.rh,
                                  static_cast<int>(c)   - r.ch});
    return r;
}

inline double sample_sym(const Value &I, int r, int c, int H, int W) {
    r = fold_index(r, H, PadMode::Symmetric);
    c = fold_index(c, W, PadMode::Symmetric);
    return I.elemAsDouble(static_cast<size_t>(c) *
                          static_cast<size_t>(H) +
                          static_cast<size_t>(r));
}

} // anonymous

Value stdfilt(const Value &I, const Value &domain, std::pmr::memory_resource *mr)
{
    const int H = static_cast<int>(I.dims().rows());
    const int W = static_cast<int>(I.dims().cols());
    Value out = Value::matrix((size_t)H, (size_t)W, ValueType::DOUBLE, mr);
    if (H == 0 || W == 0) return out;

    Value dom_local;
    const Value *dom = &domain;
    if (domain.numel() == 0) {
        dom_local = Value::matrix(3, 3, ValueType::LOGICAL, mr);
        for (size_t i = 0; i < 9; ++i) dom_local.logicalDataMut()[i] = 1;
        dom = &dom_local;
    }
    const DomainOffsets dom_offs = domain_offsets(*dom);
    const size_t M = dom_offs.offs.size();
    if (M == 0) return out;

    double *od = out.doubleDataMut();
    for (int c = 0; c < W; ++c) {
        for (int r = 0; r < H; ++r) {
            double sum = 0.0;
            for (size_t k = 0; k < M; ++k)
                sum += sample_sym(I, r + dom_offs.offs[k].first,
                                     c + dom_offs.offs[k].second, H, W);
            const double mu = sum / static_cast<double>(M);
            double v = 0.0;
            for (size_t k = 0; k < M; ++k) {
                const double d = sample_sym(I,
                                            r + dom_offs.offs[k].first,
                                            c + dom_offs.offs[k].second,
                                            H, W) - mu;
                v += d * d;
            }
            od[(size_t)c * (size_t)H + (size_t)r] =
                (M > 1) ? std::sqrt(v / static_cast<double>(M - 1)) : 0.0;
        }
    }
    return out;
}

Value ordfilt2(const Value &A, int nth, const Value &domain, const Value &S, PadMode boundary, double pad_value, std::pmr::memory_resource *mr)
{
    const int H = static_cast<int>(A.dims().rows());
    const int W = static_cast<int>(A.dims().cols());
    Value out = Value::matrix((size_t)H, (size_t)W, A.type(), mr);
    if (H == 0 || W == 0) return out;

    const size_t kH = domain.dims().rows();
    const size_t kW = domain.dims().cols();
    const int rh = static_cast<int>(kH) / 2;
    const int ch = static_cast<int>(kW) / 2;
    const bool has_S = (S.numel() == domain.numel());

    struct Off { int dr; int dc; double s; };
    std::vector<Off> offs;
    offs.reserve(kH * kW);
    for (size_t c = 0; c < kW; ++c)
        for (size_t r = 0; r < kH; ++r)
            if (domain.elemAsDouble(c * kH + r) != 0.0) {
                const double s = has_S ? S.elemAsDouble(c * kH + r) : 0.0;
                offs.push_back({static_cast<int>(r) - rh,
                                static_cast<int>(c) - ch, s});
            }
    const int M = static_cast<int>(offs.size());
    if (M == 0) return out;
    if (nth < 1 || nth > M)
        throw Error("ordfilt2: nth-order index out of range",
                    0, 0, "ordfilt2", "", "m:ordfilt2:nth");

    auto sample = [&](int r, int c) -> double {
        if (boundary == PadMode::Constant) {
            if (r < 0 || c < 0 || r >= H || c >= W) return pad_value;
        } else {
            r = fold_index(r, H, boundary);
            c = fold_index(c, W, boundary);
        }
        return A.elemAsDouble((size_t)c * (size_t)H + (size_t)r);
    };

    std::vector<double> nb(static_cast<size_t>(M));
    for (int c = 0; c < W; ++c) {
        for (int r = 0; r < H; ++r) {
            for (int k = 0; k < M; ++k) {
                nb[(size_t)k] = sample(r + offs[(size_t)k].dr,
                                       c + offs[(size_t)k].dc)
                                + offs[(size_t)k].s;
            }
            std::nth_element(nb.begin(),
                             nb.begin() + (nth - 1),
                             nb.end());
            store_classed(out, (size_t)c * (size_t)H + (size_t)r,
                          nb[(size_t)(nth - 1)], A.type());
        }
    }
    return out;
}

std::tuple<Value, Value>
wiener2(const Value &I, size_t nh, size_t nw, double noise, std::pmr::memory_resource *mr)
{
    const ValueType cls = I.type();
    const size_t H = I.dims().rows();
    const size_t W = I.dims().cols();
    if (H == 0 || W == 0) {
        Value out = Value::matrix(H, W, cls, mr);
        return {std::move(out), Value::scalar(0.0, mr)};
    }

    // Promote to double in [0, 1] for processing.
    Value Id = (cls == ValueType::DOUBLE)
        ? I
        : (cls == ValueType::SINGLE
           ? im2double(I, mr)
           : im2double(I, mr));
    if (Id.type() != ValueType::DOUBLE)
        Id = im2double(Id, mr);

    if (nh == 0) nh = 3;
    if (nw == 0) nw = 3;

    // Build h×w box kernel.
    Value k = Value::matrix(nh, nw, ValueType::DOUBLE, mr);
    {
        const double a = 1.0 / static_cast<double>(nh * nw);
        double *kd = k.doubleDataMut();
        for (size_t i = 0; i < nh * nw; ++i) kd[i] = a;
    }

    // Squared image.
    Value Id_sq = Value::matrix(H, W, ValueType::DOUBLE, mr);
    {
        const double *id = Id.doubleData();
        double *od = Id_sq.doubleDataMut();
        for (size_t i = 0; i < H * W; ++i) od[i] = id[i] * id[i];
    }

    // Local mean and mean-of-squares via zero-pad conv2 'same'.
    Value mean_im = signal::conv2(Id, k, "same", mr);
    Value mean_sq = signal::conv2(Id_sq, k, "same", mr);

    // variance_im = mean_sq - mean_im^2.
    Value var_im = Value::matrix(H, W, ValueType::DOUBLE, mr);
    {
        double *vd = var_im.doubleDataMut();
        const double *m  = mean_im.doubleData();
        const double *ms = mean_sq.doubleData();
        for (size_t i = 0; i < H * W; ++i)
            vd[i] = ms[i] - m[i] * m[i];
    }

    // Estimate noise as mean of variance if not supplied.
    if (std::isnan(noise)) {
        long double s = 0.0L;
        for (size_t i = 0; i < H * W; ++i)
            s += var_im.doubleData()[i];
        noise = static_cast<double>(s / static_cast<long double>(H * W));
    }

    // var_orig = max(0, var_im - noise).
    // out = mean + var_orig / (var_orig + noise) * (Id - mean).
    Value out_d = Value::matrix(H, W, ValueType::DOUBLE, mr);
    {
        const double *id = Id.doubleData();
        const double *m  = mean_im.doubleData();
        double *vd       = var_im.doubleDataMut();
        double *od       = out_d.doubleDataMut();
        for (size_t i = 0; i < H * W; ++i) {
            double vo = vd[i] - noise;
            if (vo < 0.0) vo = 0.0;
            const double denom = vo + noise;
            const double w = (denom > 0.0) ? vo / denom : 0.0;
            od[i] = m[i] + w * (id[i] - m[i]);
        }
    }

    // Cast back to input class.
    Value out;
    if (cls == ValueType::DOUBLE) out = std::move(out_d);
    else if (cls == ValueType::SINGLE) {
        out = Value::matrix(H, W, ValueType::SINGLE, mr);
        const double *o = out_d.doubleData();
        float *of = out.singleDataMut();
        for (size_t i = 0; i < H * W; ++i) of[i] = static_cast<float>(o[i]);
    } else if (cls == ValueType::UINT8) out = im2uint8(out_d, mr);
    else if (cls == ValueType::UINT16) out = im2uint16(out_d, mr);
    else if (cls == ValueType::INT16)  out = im2int16(out_d, mr);
    else                                out = std::move(out_d);

    return {std::move(out), Value::scalar(noise, mr)};
}

Value imsmooth(const Value &I, const std::string &name, double sigma, std::pmr::memory_resource *mr)
{
    std::string lo;
    lo.reserve(name.size());
    for (char c : name) lo.push_back(static_cast<char>(std::tolower(c)));
    if (lo != "gaussian")
        throw Error("imsmooth: only 'Gaussian' mode is implemented",
                    0, 0, "imsmooth", "", "m:imsmooth:mode");
    if (!(sigma > 0.0))
        throw Error("imsmooth: sigma must be positive",
                    0, 0, "imsmooth", "", "m:imsmooth:sigma");

    const ValueType cls = I.type();
    const int H = static_cast<int>(I.dims().rows());
    const int W = static_cast<int>(I.dims().cols());
    const int P = I.dims().is3D() ? static_cast<int>(I.dims().pages()) : 1;
    const int h = static_cast<int>(std::ceil(3.0 * sigma));
    const int K = 2 * h + 1;

    std::vector<double> f((size_t)K);
    double s = 0.0;
    for (int i = -h; i <= h; ++i) {
        f[(size_t)(i + h)] = std::exp(-(double)i * i / (2.0 * sigma * sigma));
        s += f[(size_t)(i + h)];
    }
    for (int i = 0; i < K; ++i) f[(size_t)i] /= s;

    Value out = (P > 1) ? Value::matrix3d(H, W, P, cls, mr)
                        : Value::matrix(H, W, cls, mr);
    if (H == 0 || W == 0) return out;

    for (int p = 0; p < P; ++p) {
        std::vector<double> tmp((size_t)H * (size_t)W, 0.0);
        for (int r = 0; r < H; ++r) {
            for (int c = 0; c < W; ++c) {
                double acc = 0.0;
                for (int k = -h; k <= h; ++k) {
                    const int cs = fold_index(c + k, W, PadMode::Symmetric);
                    const size_t srcIdx =
                        (size_t)p * (size_t)H * (size_t)W +
                        (size_t)cs * (size_t)H + (size_t)r;
                    acc += f[(size_t)(k + h)] * I.elemAsDouble(srcIdx);
                }
                tmp[(size_t)c * (size_t)H + (size_t)r] = acc;
            }
        }
        for (int c = 0; c < W; ++c) {
            for (int r = 0; r < H; ++r) {
                double acc = 0.0;
                for (int k = -h; k <= h; ++k) {
                    const int rs = fold_index(r + k, H, PadMode::Symmetric);
                    acc += f[(size_t)(k + h)] *
                           tmp[(size_t)c * (size_t)H + (size_t)rs];
                }
                store_classed(out,
                              (size_t)p * (size_t)H * (size_t)W +
                                  (size_t)c * (size_t)H + (size_t)r,
                              acc, cls);
            }
        }
    }
    return out;
}

// ════════════════════════════════════════════════════════════════════
// imboxfilt3 — 3-D box (mean) filter, separable + replicate boundary
// ════════════════════════════════════════════════════════════════════
Value imboxfilt3(const Value &V, int fH, int fW, int fP, std::pmr::memory_resource *mr)
{
    if (fH <= 0) fH = 3;
    if (fW <= 0) fW = 3;
    if (fP <= 0) fP = 3;
    if (fH % 2 == 0) fH += 1;
    if (fW % 2 == 0) fW += 1;
    if (fP % 2 == 0) fP += 1;

    const ValueType cls = V.type();
    const int H = static_cast<int>(V.dims().rows());
    const int W = static_cast<int>(V.dims().cols());
    const int P = V.dims().is3D() ? static_cast<int>(V.dims().pages()) : 1;
    const int hh = fH / 2;
    const int hw = fW / 2;
    const int hp = fP / 2;

    Value out = (P > 1) ? Value::matrix3d(H, W, P, cls, mr)
                        : Value::matrix(H, W, cls, mr);
    if (H == 0 || W == 0 || P == 0) return out;

    const size_t total = (size_t)H * (size_t)W * (size_t)P;
    std::vector<double> a(total), b(total);
    // Load V into a (double).
    for (size_t i = 0; i < total; ++i) a[i] = V.elemAsDouble(i);

    // Pass 1: average along rows (H axis).
    for (int p = 0; p < P; ++p) {
        for (int c = 0; c < W; ++c) {
            for (int r = 0; r < H; ++r) {
                double acc = 0.0;
                for (int k = -hh; k <= hh; ++k) {
                    const int rs = fold_index(r + k, H, PadMode::Replicate);
                    acc += a[(size_t)p * (size_t)H * (size_t)W +
                              (size_t)c * (size_t)H + (size_t)rs];
                }
                b[(size_t)p * (size_t)H * (size_t)W +
                  (size_t)c * (size_t)H + (size_t)r] = acc / (double)fH;
            }
        }
    }
    // Pass 2: average along cols (W axis).
    for (int p = 0; p < P; ++p) {
        for (int r = 0; r < H; ++r) {
            for (int c = 0; c < W; ++c) {
                double acc = 0.0;
                for (int k = -hw; k <= hw; ++k) {
                    const int cs = fold_index(c + k, W, PadMode::Replicate);
                    acc += b[(size_t)p * (size_t)H * (size_t)W +
                              (size_t)cs * (size_t)H + (size_t)r];
                }
                a[(size_t)p * (size_t)H * (size_t)W +
                  (size_t)c * (size_t)H + (size_t)r] = acc / (double)fW;
            }
        }
    }
    // Pass 3: average along pages (P axis).
    for (int p = 0; p < P; ++p) {
        for (int c = 0; c < W; ++c) {
            for (int r = 0; r < H; ++r) {
                double acc = 0.0;
                for (int k = -hp; k <= hp; ++k) {
                    const int ps = fold_index(p + k, P, PadMode::Replicate);
                    acc += a[(size_t)ps * (size_t)H * (size_t)W +
                              (size_t)c * (size_t)H + (size_t)r];
                }
                store_classed(out,
                              (size_t)p * (size_t)H * (size_t)W +
                                  (size_t)c * (size_t)H + (size_t)r,
                              acc / (double)fP, cls);
            }
        }
    }
    return out;
}

// 1-D Gaussian kernel of length 2*ceil(2σ)+1, normalised.
static std::vector<double> gauss1d_kernel(double sigma) {
    if (!(sigma > 0.0)) sigma = 0.5;
    int half = static_cast<int>(std::ceil(2.0 * sigma));
    if (half < 1) half = 1;
    const int K = 2 * half + 1;
    std::vector<double> k(static_cast<size_t>(K));
    double sum = 0.0;
    const double inv2s2 = 0.5 / (sigma * sigma);
    for (int i = -half; i <= half; ++i) {
        const double w = std::exp(-static_cast<double>(i * i) * inv2s2);
        k[static_cast<size_t>(i + half)] = w;
        sum += w;
    }
    const double inv = 1.0 / sum;
    for (double &x : k) x *= inv;
    return k;
}

Value imgaussfilt3(const Value &V, double sigH, double sigW, double sigP, std::pmr::memory_resource *mr)
{
    const ValueType cls = V.type();
    const int H = static_cast<int>(V.dims().rows());
    const int W = static_cast<int>(V.dims().cols());
    const int P = V.dims().is3D() ? static_cast<int>(V.dims().pages()) : 1;
    Value out = (P > 1) ? Value::matrix3d(H, W, P, cls, mr)
                        : Value::matrix(H, W, cls, mr);
    if (H == 0 || W == 0 || P == 0) return out;

    const auto kH = gauss1d_kernel(sigH);
    const auto kW = gauss1d_kernel(sigW);
    const auto kP = gauss1d_kernel(sigP);
    const int hH = static_cast<int>(kH.size()) / 2;
    const int hW = static_cast<int>(kW.size()) / 2;
    const int hP = static_cast<int>(kP.size()) / 2;

    const size_t total = static_cast<size_t>(H)
                       * static_cast<size_t>(W)
                       * static_cast<size_t>(P);
    std::vector<double> a(total), b(total);
    for (size_t i = 0; i < total; ++i) a[i] = V.elemAsDouble(i);

    // Pass 1: along H axis.
    for (int p = 0; p < P; ++p) {
        for (int c = 0; c < W; ++c) {
            for (int r = 0; r < H; ++r) {
                double acc = 0.0;
                for (int k = -hH; k <= hH; ++k) {
                    const int rs = fold_index(r + k, H, PadMode::Replicate);
                    acc += kH[static_cast<size_t>(k + hH)] *
                           a[(size_t)p * H * W + (size_t)c * H + (size_t)rs];
                }
                b[(size_t)p * H * W + (size_t)c * H + (size_t)r] = acc;
            }
        }
    }
    // Pass 2: along W axis.
    for (int p = 0; p < P; ++p) {
        for (int r = 0; r < H; ++r) {
            for (int c = 0; c < W; ++c) {
                double acc = 0.0;
                for (int k = -hW; k <= hW; ++k) {
                    const int cs = fold_index(c + k, W, PadMode::Replicate);
                    acc += kW[static_cast<size_t>(k + hW)] *
                           b[(size_t)p * H * W + (size_t)cs * H + (size_t)r];
                }
                a[(size_t)p * H * W + (size_t)c * H + (size_t)r] = acc;
            }
        }
    }
    // Pass 3: along P axis.
    for (int p = 0; p < P; ++p) {
        for (int c = 0; c < W; ++c) {
            for (int r = 0; r < H; ++r) {
                double acc = 0.0;
                for (int k = -hP; k <= hP; ++k) {
                    const int ps = fold_index(p + k, P, PadMode::Replicate);
                    acc += kP[static_cast<size_t>(k + hP)] *
                           a[(size_t)ps * H * W + (size_t)c * H + (size_t)r];
                }
                store_classed(out,
                              (size_t)p * H * W + (size_t)c * H + (size_t)r,
                              acc, cls);
            }
        }
    }
    return out;
}

Value medfilt3(const Value &V, int M, int N, int P, std::pmr::memory_resource *mr)
{
    if (M <= 0) M = 3;
    if (N <= 0) N = 3;
    if (P <= 0) P = 3;
    if (M % 2 == 0 || N % 2 == 0 || P % 2 == 0)
        throw Error("medfilt3: filter sizes must be odd",
                    0, 0, "medfilt3", "", "m:medfilt3:size");

    const ValueType cls = V.type();
    const int H = static_cast<int>(V.dims().rows());
    const int W = static_cast<int>(V.dims().cols());
    const int D = V.dims().is3D() ? static_cast<int>(V.dims().pages()) : 1;

    Value out = (D > 1) ? Value::matrix3d(H, W, D, cls, mr)
                        : Value::matrix(H, W, cls, mr);
    if (H == 0 || W == 0 || D == 0) return out;

    const int hM = M / 2;
    const int hN = N / 2;
    const int hP = P / 2;
    const size_t plane = static_cast<size_t>(H) * static_cast<size_t>(W);
    const size_t nbhd = static_cast<size_t>(M) * static_cast<size_t>(N)
                      * static_cast<size_t>(P);

    std::vector<double> window;
    window.reserve(nbhd);
    for (int p = 0; p < D; ++p) {
        for (int c = 0; c < W; ++c) {
            for (int r = 0; r < H; ++r) {
                window.clear();
                for (int kp = -hP; kp <= hP; ++kp) {
                    const int ps = fold_index(p + kp, D, PadMode::Symmetric);
                    for (int kc = -hN; kc <= hN; ++kc) {
                        const int cs = fold_index(c + kc, W, PadMode::Symmetric);
                        for (int kr = -hM; kr <= hM; ++kr) {
                            const int rs = fold_index(r + kr, H, PadMode::Symmetric);
                            const size_t idx = static_cast<size_t>(ps) * plane
                                             + static_cast<size_t>(cs) * H
                                             + static_cast<size_t>(rs);
                            window.push_back(V.elemAsDouble(idx));
                        }
                    }
                }
                std::nth_element(window.begin(),
                                 window.begin() + window.size() / 2,
                                 window.end());
                double med = window[window.size() / 2];
                if ((window.size() & 1) == 0) {
                    double left_max = -std::numeric_limits<double>::infinity();
                    for (auto it = window.begin();
                         it != window.begin() + window.size() / 2; ++it)
                        if (*it > left_max) left_max = *it;
                    med = 0.5 * (left_max + med);
                }
                store_classed(out,
                              static_cast<size_t>(p) * plane
                                + static_cast<size_t>(c) * H
                                + static_cast<size_t>(r),
                              med, cls);
            }
        }
    }
    return out;
}

Value convmtx2(const Value &h, int m, int n, std::pmr::memory_resource *mr)
{
    const auto &dh = h.dims();
    if (dh.is3D())
        throw Error("convmtx2: kernel must be 2-D",
                    0, 0, "convmtx2", "", "m:convmtx2:dims");
    if (m <= 0 || n <= 0)
        throw Error("convmtx2: m, n must be positive integers",
                    0, 0, "convmtx2", "", "m:convmtx2:size");
    const int M = static_cast<int>(dh.rows());
    const int N = static_cast<int>(dh.cols());
    const size_t out_rows = static_cast<size_t>(m + M - 1);
    const size_t out_cols = static_cast<size_t>(n + N - 1);
    const size_t P = out_rows * out_cols;
    const size_t Q = static_cast<size_t>(m) * static_cast<size_t>(n);

    Value T = Value::matrix(P, Q, ValueType::DOUBLE, mr);
    if (P == 0 || Q == 0) return T;
    double *Td = T.doubleDataMut();
    // T is col-major; column k corresponds to input position (r_in, c_in)
    // (1-based). For each kernel cell (i, j), set
    // T(out_r + (out_c-1)*out_rows, k) = h(i, j),
    // where out_r = r_in + i - 1, out_c = c_in + j - 1.
    for (int c_in = 0; c_in < n; ++c_in) {
        for (int r_in = 0; r_in < m; ++r_in) {
            const size_t k = static_cast<size_t>(c_in) * static_cast<size_t>(m)
                           + static_cast<size_t>(r_in);
            for (int j = 0; j < N; ++j) {
                for (int i = 0; i < M; ++i) {
                    const double hv = h.elemAsDouble(
                        static_cast<size_t>(j) * static_cast<size_t>(M)
                        + static_cast<size_t>(i));
                    if (hv == 0.0) continue;
                    const size_t out_r = static_cast<size_t>(r_in + i);
                    const size_t out_c = static_cast<size_t>(c_in + j);
                    const size_t p = out_c * out_rows + out_r;
                    Td[k * P + p] = hv;
                }
            }
        }
    }
    return T;
}

Value entropyfilt(const Value &I, const Value &domain, std::pmr::memory_resource *mr)
{
    const bool isLogical = (I.type() == ValueType::LOGICAL);
    const int nbins = isLogical ? 2 : 256;
    // For non-logical, non-uint8 inputs, cast through im2uint8 to match
    // Octave-image's "do this for everything except logical/uint8" rule.
    Value Iu = (isLogical || I.type() == ValueType::UINT8)
        ? I : im2uint8(I, mr);

    const int H = static_cast<int>(Iu.dims().rows());
    const int W = static_cast<int>(Iu.dims().cols());
    Value out = Value::matrix((size_t)H, (size_t)W, ValueType::DOUBLE, mr);
    if (H == 0 || W == 0) return out;

    Value dom_local;
    const Value *dom = &domain;
    if (domain.numel() == 0) {
        // Default: ones(9).
        dom_local = Value::matrix(9, 9, ValueType::LOGICAL, mr);
        for (size_t i = 0; i < 81; ++i) dom_local.logicalDataMut()[i] = 1;
        dom = &dom_local;
    }
    const DomainOffsets dom_offs = domain_offsets(*dom);
    const size_t M = dom_offs.offs.size();
    if (M == 0) return out;

    double *od = out.doubleDataMut();
    std::vector<int> hist(static_cast<size_t>(nbins), 0);
    for (int c = 0; c < W; ++c) {
        for (int r = 0; r < H; ++r) {
            std::fill(hist.begin(), hist.end(), 0);
            for (size_t k = 0; k < M; ++k) {
                const double v = sample_sym(Iu,
                                            r + dom_offs.offs[k].first,
                                            c + dom_offs.offs[k].second,
                                            H, W);
                int b = static_cast<int>(v);
                if (b < 0)        b = 0;
                if (b >= nbins)   b = nbins - 1;
                ++hist[(size_t)b];
            }
            const double total = static_cast<double>(M);
            double H_bits = 0.0;
            for (int b = 0; b < nbins; ++b) {
                const int n = hist[(size_t)b];
                if (n <= 0) continue;
                const double p = static_cast<double>(n) / total;
                H_bits -= p * std::log2(p);
            }
            od[(size_t)c * (size_t)H + (size_t)r] = H_bits;
        }
    }
    return out;
}

Value rangefilt(const Value &I, const Value &domain, std::pmr::memory_resource *mr)
{
    const int H = static_cast<int>(I.dims().rows());
    const int W = static_cast<int>(I.dims().cols());
    Value out = Value::matrix((size_t)H, (size_t)W, I.type(), mr);
    if (H == 0 || W == 0) return out;

    Value dom_local;
    const Value *dom = &domain;
    if (domain.numel() == 0) {
        dom_local = Value::matrix(3, 3, ValueType::LOGICAL, mr);
        for (size_t i = 0; i < 9; ++i) dom_local.logicalDataMut()[i] = 1;
        dom = &dom_local;
    }
    const DomainOffsets dom_offs = domain_offsets(*dom);
    const size_t M = dom_offs.offs.size();
    if (M == 0) return out;

    for (int c = 0; c < W; ++c) {
        for (int r = 0; r < H; ++r) {
            double mn =  std::numeric_limits<double>::infinity();
            double mx = -std::numeric_limits<double>::infinity();
            for (size_t k = 0; k < M; ++k) {
                const double v = sample_sym(I,
                                            r + dom_offs.offs[k].first,
                                            c + dom_offs.offs[k].second,
                                            H, W);
                if (v < mn) mn = v;
                if (v > mx) mx = v;
            }
            store_classed(out, (size_t)c * (size_t)H + (size_t)r,
                          mx - mn, I.type());
        }
    }
    return out;
}

std::tuple<Value, Value, Value>
freqz2(const Value &h, size_t M, size_t N, std::pmr::memory_resource *mr)
{
    if (h.dims().is3D())
        throw Error("freqz2: kernel must be 2-D",
                    0, 0, "freqz2", "", "m:freqz2:dims");
    const size_t P = h.dims().rows();
    const size_t Q = h.dims().cols();
    if (M == 0) M = 64;
    if (N == 0) N = 64;

    // freqspace-style row/col frequency grids: f[k] = -1 + 2(k-1)/N for
    // k = 1..N. (Even N: starts at -1 and stops at 1 - 2/N.)
    Value f1V = Value::matrix(M, 1, ValueType::DOUBLE, mr);
    Value f2V = Value::matrix(N, 1, ValueType::DOUBLE, mr);
    double *f1 = f1V.doubleDataMut();
    double *f2 = f2V.doubleDataMut();
    for (size_t i = 0; i < M; ++i) f1[i] = -1.0 + 2.0 * static_cast<double>(i) / static_cast<double>(M);
    for (size_t j = 0; j < N; ++j) f2[j] = -1.0 + 2.0 * static_cast<double>(j) / static_cast<double>(N);

    Value HV = Value::matrix(M, N, ValueType::COMPLEX, mr);
    if (P == 0 || Q == 0) return {std::move(HV), std::move(f1V), std::move(f2V)};
    Complex *Hd = HV.complexDataMut();

    // H[i, j] = Σ_p Σ_q h[p,q] · exp(+iπ·(f1[i]·(p - cp) + f2[j]·(q - cq)))
    // where (cp, cq) = (⌊(P-1)/2⌋, ⌊(Q-1)/2⌋) are the kernel centre
    // offsets. Matches MATLAB: real for symmetric h, complex otherwise.
    const double cp = static_cast<double>((P > 0 ? (P - 1) / 2 : 0));
    const double cq = static_cast<double>((Q > 0 ? (Q - 1) / 2 : 0));
    std::vector<Complex> phase_p_i(M * P), phase_q_j(N * Q);
    for (size_t i = 0; i < M; ++i)
        for (size_t p = 0; p < P; ++p) {
            const double a = M_PI * f1[i] * (static_cast<double>(p) - cp);
            phase_p_i[p + i * P] = {std::cos(a), std::sin(a)};
        }
    for (size_t j = 0; j < N; ++j)
        for (size_t q = 0; q < Q; ++q) {
            const double a = M_PI * f2[j] * (static_cast<double>(q) - cq);
            phase_q_j[q + j * Q] = {std::cos(a), std::sin(a)};
        }
    for (size_t j = 0; j < N; ++j) {
        for (size_t i = 0; i < M; ++i) {
            Complex sum{0.0, 0.0};
            for (size_t q = 0; q < Q; ++q) {
                const Complex pq = phase_q_j[q + j * Q];
                for (size_t p = 0; p < P; ++p) {
                    const double hpq = h.elemAsDouble(p + q * P);
                    sum += hpq * (phase_p_i[p + i * P] * pq);
                }
            }
            Hd[i + j * M] = sum;
        }
    }
    return {std::move(HV), std::move(f1V), std::move(f2V)};
}

// ════════════════════════════════════════════════════════════════════
// Engine adapters
// ════════════════════════════════════════════════════════════════════

namespace detail {

namespace {
PadMode parse_pad_mode(const Value &v, double &pad_value, bool &is_value) {
    is_value = false;
    pad_value = 0.0;
    if (v.isChar() || v.isString()) {
        auto s = v.toString();
        if (s == "replicate") return PadMode::Replicate;
        if (s == "symmetric") return PadMode::Symmetric;
        if (s == "circular")  return PadMode::Circular;
        // Unknown mode → treat as scalar 0.
    }
    if (v.numel() == 1) {
        is_value = true;
        pad_value = v.toScalar();
    }
    return PadMode::Constant;
}
} // anonymous

void padarray_reg(Span<const Value> args, size_t /*nargout*/,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("padarray: requires (A, padsize[, val|mode][, direction])",
                    0, 0, "padarray", "", "m:padarray:nargin");

    std::vector<int> padsize;
    {
        const Value &p = args[1];
        const size_t n = p.numel();
        padsize.resize(n);
        for (size_t i = 0; i < n; ++i) padsize[i] = int(p.elemAsDouble(i));
    }

    PadMode mode = PadMode::Constant;
    double pad_value = 0.0;
    std::string direction = "both";

    // Parse optional trailing args: pad_value-or-mode (one) and/or direction.
    for (size_t i = 2; i < args.size(); ++i) {
        const Value &a = args[i];
        if (a.isChar() || a.isString()) {
            auto s = a.toString();
            if (s == "pre" || s == "post" || s == "both") {
                direction = s;
            } else {
                bool dummy;
                mode = parse_pad_mode(a, pad_value, dummy);
            }
        } else {
            // Treat as scalar pad value.
            pad_value = a.toScalar();
            mode = PadMode::Constant;
        }
    }

    outs[0] = padarray(args[0], padsize, mode, pad_value, direction, ctx.engine->resource());
}

void fspecial_reg(Span<const Value> args, size_t /*nargout*/,
                  Span<Value> outs, CallContext &ctx)
{
    std::string type = "gaussian";
    if (!args.empty() && (args[0].isChar() || args[0].isString()))
        type = args[0].toString();

    std::vector<double> params;
    // Second positional arg can be either scalar (size) or 2-vector [rows cols].
    if (args.size() >= 2) {
        const Value &v = args[1];
        if (v.numel() == 1) {
            params.push_back(v.toScalar());
            params.push_back(v.toScalar());
        } else if (v.numel() == 2) {
            params.push_back(v.elemAsDouble(0));
            params.push_back(v.elemAsDouble(1));
        } else if (v.numel() > 0) {
            for (size_t i = 0; i < v.numel(); ++i)
                params.push_back(v.elemAsDouble(i));
        }
    }
    // Third positional arg = sigma / alpha / radius (scalar).
    if (args.size() >= 3 && args[2].numel() == 1)
        params.push_back(args[2].toScalar());

    outs[0] = fspecial(type, params, ctx.engine->resource());
}

void imfilter_reg(Span<const Value> args, size_t /*nargout*/,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("imfilter: requires (I, h[, options])",
                    0, 0, "imfilter", "", "m:imfilter:nargin");
    PadMode boundary = PadMode::Constant;
    double pad_value = 0.0;
    bool full = false;
    bool flip_kernel = false;  // 'corr' default
    for (size_t i = 2; i < args.size(); ++i) {
        const Value &a = args[i];
        if (a.isChar() || a.isString()) {
            auto s = a.toString();
            if      (s == "replicate") boundary = PadMode::Replicate;
            else if (s == "symmetric") boundary = PadMode::Symmetric;
            else if (s == "circular")  boundary = PadMode::Circular;
            else if (s == "full")      full = true;
            else if (s == "same")      full = false;
            else if (s == "conv")      flip_kernel = true;
            else if (s == "corr")      flip_kernel = false;
        } else {
            pad_value = a.toScalar();
            boundary = PadMode::Constant;
        }
    }
    outs[0] = imfilter(args[0], args[1], boundary, pad_value, full, flip_kernel, ctx.engine->resource());
}

void imgaussfilt_reg(Span<const Value> args, size_t /*nargout*/,
                     Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("imgaussfilt: requires (I[, sigma][, FilterSize])",
                    0, 0, "imgaussfilt", "", "m:imgaussfilt:nargin");
    double sigma = (args.size() >= 2 && !args[1].isEmpty()) ? args[1].toScalar() : 0.5;
    int fs = 0;  // auto
    // Look for 'FilterSize' name-value pair.
    for (size_t i = 2; i + 1 < args.size(); ++i) {
        if ((args[i].isChar() || args[i].isString())
            && args[i].toString() == "FilterSize") {
            fs = (int)args[i + 1].toScalar();
            break;
        }
    }
    outs[0] = imgaussfilt(args[0], sigma, fs, ctx.engine->resource());
}

void imboxfilt_reg(Span<const Value> args, size_t /*nargout*/,
                   Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("imboxfilt: requires (I[, FilterSize])",
                    0, 0, "imboxfilt", "", "m:imboxfilt:nargin");
    int fs = (args.size() >= 2 && !args[1].isEmpty()) ? (int)args[1].toScalar() : 3;
    outs[0] = imboxfilt(args[0], fs, ctx.engine->resource());
}

void medfilt3_reg(Span<const Value> args, size_t /*nargout*/,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("medfilt3: requires (V[, [M N P]])",
                    0, 0, "medfilt3", "", "m:medfilt3:nargin");
    int M = 3, N = 3, P = 3;
    if (args.size() >= 2 && !args[1].isEmpty()) {
        const Value &v = args[1];
        if (v.numel() == 1) {
            M = N = P = static_cast<int>(v.toScalar());
        } else if (v.numel() >= 3) {
            M = static_cast<int>(v.elemAsDouble(0));
            N = static_cast<int>(v.elemAsDouble(1));
            P = static_cast<int>(v.elemAsDouble(2));
        }
    }
    outs[0] = medfilt3(args[0], M, N, P, ctx.engine->resource());
}

void imgaussfilt3_reg(Span<const Value> args, size_t /*nargout*/,
                      Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("imgaussfilt3: requires (V[, sigma])",
                    0, 0, "imgaussfilt3", "", "m:imgaussfilt3:nargin");
    double sigH = 0.5, sigW = 0.5, sigP = 0.5;
    if (args.size() >= 2 && !args[1].isEmpty()) {
        const Value &v = args[1];
        if (v.numel() == 1) {
            sigH = sigW = sigP = v.toScalar();
        } else if (v.numel() >= 3) {
            sigH = v.elemAsDouble(0);
            sigW = v.elemAsDouble(1);
            sigP = v.elemAsDouble(2);
        }
    }
    outs[0] = imgaussfilt3(args[0], sigH, sigW, sigP, ctx.engine->resource());
}

void convmtx2_reg(Span<const Value> args, size_t /*nargout*/,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("convmtx2: requires (h, m, n) or (h, [m n])",
                    0, 0, "convmtx2", "", "m:convmtx2:nargin");
    int m = 0, n = 0;
    if (args.size() >= 3) {
        m = static_cast<int>(args[1].toScalar());
        n = static_cast<int>(args[2].toScalar());
    } else {
        const Value &v = args[1];
        if (v.numel() != 2)
            throw Error("convmtx2: 2nd arg must be a 2-element vector or pair (m, n)",
                        0, 0, "convmtx2", "", "m:convmtx2:size");
        m = static_cast<int>(v.elemAsDouble(0));
        n = static_cast<int>(v.elemAsDouble(1));
    }
    outs[0] = convmtx2(args[0], m, n, ctx.engine->resource());
}

void imboxfilt3_reg(Span<const Value> args, size_t /*nargout*/,
                    Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("imboxfilt3: requires (V[, FilterSize])",
                    0, 0, "imboxfilt3", "", "m:imboxfilt3:nargin");
    int fH = 3, fW = 3, fP = 3;
    if (args.size() >= 2 && !args[1].isEmpty()) {
        const Value &v = args[1];
        if (v.numel() == 1) {
            fH = fW = fP = (int)v.toScalar();
        } else if (v.numel() >= 3) {
            fH = (int)v.elemAsDouble(0);
            fW = (int)v.elemAsDouble(1);
            fP = (int)v.elemAsDouble(2);
        }
    }
    outs[0] = imboxfilt3(args[0], fH, fW, fP, ctx.engine->resource());
}

void freqz2_reg(Span<const Value> args, size_t nargout,
                Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("freqz2: requires (h [, M, N])",
                    0, 0, "freqz2", "", "m:freqz2:nargin");
    size_t M = 64, N = 64;
    if (args.size() >= 2 && !args[1].isEmpty()) {
        const Value &v = args[1];
        if (v.numel() == 1) {
            M = N = static_cast<size_t>(v.toScalar());
        } else if (v.numel() >= 2) {
            M = static_cast<size_t>(v.elemAsDouble(0));
            N = static_cast<size_t>(v.elemAsDouble(1));
        }
    }
    if (args.size() >= 3 && !args[2].isEmpty())
        N = static_cast<size_t>(args[2].toScalar());
    auto [H, f1, f2] = freqz2(args[0], M, N, ctx.engine->resource());
    outs[0] = std::move(H);
    if (nargout > 1) outs[1] = std::move(f1);
    if (nargout > 2) outs[2] = std::move(f2);
}

void medfilt2_reg(Span<const Value> args, size_t /*nargout*/,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("medfilt2: requires (I[, [m n]])",
                    0, 0, "medfilt2", "", "m:medfilt2:nargin");
    int rows = 3, cols = 3;
    if (args.size() >= 2 && !args[1].isEmpty()) {
        const Value &v = args[1];
        if (v.numel() == 1) {
            rows = cols = (int)v.toScalar();
        } else if (v.numel() >= 2) {
            rows = (int)v.elemAsDouble(0);
            cols = (int)v.elemAsDouble(1);
        }
    }
    outs[0] = medfilt2(args[0], rows, cols, ctx.engine->resource());
}

void im2col_reg(Span<const Value> args, size_t /*nargout*/,
                Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("im2col: requires (A, [m n] [, block_type])",
                    0, 0, "im2col", "", "m:im2col:nargin");
    int m = 0, n = 0;
    const Value &sz = args[1];
    if (sz.numel() == 1) {
        m = n = (int)sz.toScalar();
    } else if (sz.numel() >= 2) {
        m = (int)sz.elemAsDouble(0);
        n = (int)sz.elemAsDouble(1);
    } else {
        throw Error("im2col: block size must be scalar or 2-vector",
                    0, 0, "im2col", "", "m:im2col:size");
    }
    std::string mode = "sliding";
    if (args.size() >= 3 && (args[2].isChar() || args[2].isString()))
        mode = args[2].toString();
    outs[0] = im2col(args[0], m, n, mode, ctx.engine->resource());
}

void imbilatfilt_reg(Span<const Value> args, size_t /*nargout*/,
                     Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("imbilatfilt: requires (I[, degreeOfSmoothing, spatialSigma])",
                    0, 0, "imbilatfilt", "", "m:imbilatfilt:nargin");

    // Default DegreeOfSmoothing depends on input class range — MATLAB
    // uses 0.01 · diff(getrangefromclass(I)). For our purposes:
    //   double / single / logical → 0.01    (range = 1)
    //   uint8                     → 6.50    (= 0.01·255²·1e-3 ish; matches MATLAB)
    //   uint16                    → tiny but absolute; we use 0.01·65535²·1e-3
    // Actually MATLAB stores it as variance of the range Gaussian
    // expressed in the same units as I. For unit-range double the
    // canonical value is 0.01; for integer classes we scale by the
    // range so the *relative* sensitivity matches.
    const Value &I = args[0];
    double dos_default;
    switch (I.type()) {
        case ValueType::UINT8:
            dos_default = 0.01 * 255.0 * 255.0; break;
        case ValueType::UINT16:
            dos_default = 0.01 * 65535.0 * 65535.0; break;
        case ValueType::INT16:
            dos_default = 0.01 * 65535.0 * 65535.0; break;
        default:
            dos_default = 0.01; break;
    }
    double dos    = (args.size() >= 2 && !args[1].isEmpty())
                    ? args[1].toScalar() : dos_default;
    double sigma  = (args.size() >= 3 && !args[2].isEmpty())
                    ? args[2].toScalar() : 1.0;
    outs[0] = imbilatfilt(I, dos, sigma, ctx.engine->resource());
}

void col2im_reg(Span<const Value> args, size_t /*nargout*/,
                Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("col2im: requires (B, [m n], [mm nn] [, block_type])",
                    0, 0, "col2im", "", "m:col2im:nargin");
    int m, n, mm, nn;
    {
        const Value &b = args[1];
        if (b.numel() < 1)
            throw Error("col2im: empty block size",
                        0, 0, "col2im", "", "m:col2im:size");
        m = (int)b.elemAsDouble(0);
        n = (b.numel() >= 2) ? (int)b.elemAsDouble(1) : m;
    }
    {
        const Value &s = args[2];
        if (s.numel() < 1)
            throw Error("col2im: empty image size",
                        0, 0, "col2im", "", "m:col2im:size");
        mm = (int)s.elemAsDouble(0);
        nn = (s.numel() >= 2) ? (int)s.elemAsDouble(1) : mm;
    }
    std::string mode = "sliding";
    if (args.size() >= 4 && (args[3].isChar() || args[3].isString()))
        mode = args[3].toString();
    outs[0] = col2im(args[0], m, n, mm, nn, mode, ctx.engine->resource());
}

void imnoise_reg(Span<const Value> args, size_t /*nargout*/,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("imnoise: requires (I, mode [, p1, p2])",
                    0, 0, "imnoise", "", "m:imnoise:nargin");
    if (!(args[1].isChar() || args[1].isString()))
        throw Error("imnoise: mode must be a string",
                    0, 0, "imnoise", "", "m:imnoise:mode");
    const std::string mode = args[1].toString();
    Value p1, p2;
    if (args.size() >= 3) p1 = args[2];
    if (args.size() >= 4) p2 = args[3];
    outs[0] = imnoise(args[0], mode, p1, p2, ctx.engine->resource());
}

void imsharpen_reg(Span<const Value> args, size_t /*nargout*/,
                   Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("imsharpen: requires (I[, radius, amount, threshold])",
                    0, 0, "imsharpen", "", "m:imsharpen:nargin");
    double radius = 1.0, amount = 0.8, threshold = 0.0;
    // Accept either positional (I, radius, amount, threshold) or
    // name-value pairs ('Radius', r, 'Amount', a, 'Threshold', t).
    size_t i = 1;
    bool sawNV = false;
    while (i < args.size()) {
        if (args[i].isChar() || args[i].isString()) {
            sawNV = true;
            const std::string nm = args[i].toString();
            if (i + 1 >= args.size())
                throw Error("imsharpen: missing value for '" + nm + "'",
                            0, 0, "imsharpen", "", "m:imsharpen:nv");
            const double v = args[i + 1].toScalar();
            if (nm == "Radius" || nm == "radius") radius = v;
            else if (nm == "Amount" || nm == "amount") amount = v;
            else if (nm == "Threshold" || nm == "threshold") threshold = v;
            else throw Error("imsharpen: unknown option '" + nm + "'",
                             0, 0, "imsharpen", "", "m:imsharpen:opt");
            i += 2;
        } else {
            if (sawNV)
                throw Error("imsharpen: positional after name-value",
                            0, 0, "imsharpen", "", "m:imsharpen:syntax");
            const double v = args[i].toScalar();
            if      (i == 1) radius    = v;
            else if (i == 2) amount    = v;
            else if (i == 3) threshold = v;
            ++i;
        }
    }
    outs[0] = imsharpen(args[0], radius, amount, threshold, ctx.engine->resource());
}

void stdfilt_reg(Span<const Value> args, size_t /*nargout*/,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("stdfilt: requires (I [, domain])",
                    0, 0, "stdfilt", "", "m:stdfilt:nargin");
    Value dom;
    if (args.size() >= 2 && !args[1].isEmpty()) dom = args[1];
    outs[0] = stdfilt(args[0], dom, ctx.engine->resource());
}

void rangefilt_reg(Span<const Value> args, size_t /*nargout*/,
                   Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("rangefilt: requires (I [, domain])",
                    0, 0, "rangefilt", "", "m:rangefilt:nargin");
    Value dom;
    if (args.size() >= 2 && !args[1].isEmpty()) dom = args[1];
    outs[0] = rangefilt(args[0], dom, ctx.engine->resource());
}

void imsmooth_reg(Span<const Value> args, size_t /*nargout*/,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("imsmooth: requires (I [, name [, sigma]])",
                    0, 0, "imsmooth", "", "m:imsmooth:nargin");
    auto *mr = ctx.engine->resource();
    std::string name = "Gaussian";
    double sigma = 0.5;
    // imsmooth(I, scalar) — Octave shorthand: scalar is sigma, name=Gaussian.
    if (args.size() >= 2 && !args[1].isEmpty()) {
        if (args[1].isChar() || args[1].isString()) {
            name = args[1].toString();
            if (args.size() >= 3 && !args[2].isEmpty())
                sigma = args[2].toScalar();
        } else if (args[1].numel() == 1) {
            sigma = args[1].toScalar();
        }
    }
    outs[0] = imsmooth(args[0], name, sigma, mr);
}

void entropyfilt_reg(Span<const Value> args, size_t /*nargout*/,
                     Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("entropyfilt: requires (I [, domain])",
                    0, 0, "entropyfilt", "", "m:entropyfilt:nargin");
    Value dom;
    if (args.size() >= 2 && !args[1].isEmpty()) dom = args[1];
    outs[0] = entropyfilt(args[0], dom, ctx.engine->resource());
}

void ordfilt2_reg(Span<const Value> args, size_t /*nargout*/,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("ordfilt2: requires (A, nth, domain [, S] [, padding])",
                    0, 0, "ordfilt2", "", "m:ordfilt2:nargin");
    auto *mr = ctx.engine->resource();
    const int nth = static_cast<int>(args[1].toScalar());
    Value domain = args[2];
    if (domain.numel() == 1) {
        // Scalar domain → square true(N).
        const int n = static_cast<int>(domain.toScalar());
        domain = Value::matrix((size_t)n, (size_t)n, ValueType::LOGICAL, mr);
        std::uint8_t *dd = domain.logicalDataMut();
        for (size_t i = 0; i < (size_t)n * (size_t)n; ++i) dd[i] = 1;
    }
    Value S;
    PadMode pad = PadMode::Constant;
    double pad_value = 0.0;
    for (size_t i = 3; i < args.size(); ++i) {
        const Value &a = args[i];
        if (a.isEmpty()) continue;
        if (a.isChar() || a.isString()) {
            const std::string s = a.toString();
            if      (s == "replicate") pad = PadMode::Replicate;
            else if (s == "symmetric") pad = PadMode::Symmetric;
            else if (s == "circular")  pad = PadMode::Circular;
            else throw Error("ordfilt2: unknown padding mode",
                              0, 0, "ordfilt2", "", "m:ordfilt2:pad");
        } else if (a.numel() == 1) {
            pad = PadMode::Constant;
            pad_value = a.toScalar();
        } else if (a.dims().rows() == domain.dims().rows() &&
                   a.dims().cols() == domain.dims().cols()) {
            S = a;
        } else {
            throw Error("ordfilt2: unrecognized argument shape",
                        0, 0, "ordfilt2", "", "m:ordfilt2:arg");
        }
    }
    outs[0] = ordfilt2(args[0], nth, domain, S, pad, pad_value, mr);
}

void wiener2_reg(Span<const Value> args, size_t nargout,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("wiener2: requires (I [, nhood [, noise]])",
                    0, 0, "wiener2", "", "m:wiener2:nargin");
    size_t nh = 3, nw = 3;
    double noise = std::nan("");
    if (args.size() >= 2 && !args[1].isEmpty()) {
        if (args[1].numel() == 1) {
            // Single scalar = noise (Octave convention when no nhood).
            noise = args[1].toScalar();
        } else if (args[1].numel() >= 2) {
            nh = static_cast<size_t>(args[1].elemAsDouble(0));
            nw = static_cast<size_t>(args[1].elemAsDouble(1));
        }
    }
    if (args.size() >= 3 && !args[2].isEmpty())
        noise = args[2].toScalar();
    auto [denoised, n] =
        wiener2(args[0], nh, nw, noise, ctx.engine->resource());
    outs[0] = std::move(denoised);
    if (nargout > 1) outs[1] = std::move(n);
}

} // namespace detail

// ── nlfilter (general sliding-neighbourhood) ──────────────────────
//
// MATLAB R2025b nlfilter.m:
//   B = nlfilter(A, [m n], fun)
//   B = nlfilter(A, 'indexed', [m n], fun)
//
// For every pixel (i, j) ∈ A extract the m × n window
//   x = aa(i + 0..m-1, j + 0..n-1)
// from the padded image `aa` and call `fun(x)`. The output element
// is `b(i, j) = fun(x)`. Output class equals the class of the FIRST
// invocation of `fun` (matches MATLAB's `mkconstarray(class(...))`).
// Default `padval = 0`; `'indexed'` form uses `padval = 1` for
// `single` / `double` `A`, otherwise `padval = 0`.
//
// The dispatch goes through Engine::callFunctionHandle, matching the
// pattern adopted in libs/ode/ode45 (function_ref couldn't carry
// func-handle semantics through the round-trip).
Value nlfilter(numkit::Engine &eng, const Value &A,
               std::size_t m, std::size_t n, const Value &fun,
               bool indexed,
               std::pmr::memory_resource *mr)
{
    if (m < 1 || n < 1)
        throw Error("nlfilter: neighbourhood size must be positive",
                    0, 0, "nlfilter", "", "m:nlfilter:nhood");
    if (!fun.isFuncHandle())
        throw Error("nlfilter: 3rd argument must be a function handle",
                    0, 0, "nlfilter", "", "m:nlfilter:fun");

    const auto &dA = A.dims();
    if (dA.is3D())
        throw Error("nlfilter: A must be a 2-D image",
                    0, 0, "nlfilter", "", "m:nlfilter:rank");
    const std::size_t H = dA.rows();
    const std::size_t W = dA.cols();
    const ValueType inT = A.type();

    // Padding: 'indexed' uses 1.0 for single/double, else 0.
    double padval = 0.0;
    if (indexed) {
        padval = (inT == ValueType::DOUBLE || inT == ValueType::SINGLE)
               ? 1.0 : 0.0;
    }

    // Pad above by floor((m-1)/2) rows, below by ceil((m-1)/2);
    // left by floor((n-1)/2), right by ceil((n-1)/2). (MATLAB:
    // mkconstarray(class(a), padval, size(a)+nhood-1) — then drops
    // the original A into the offset block.)
    const std::size_t pad_top  = (m - 1) / 2;
    const std::size_t pad_left = (n - 1) / 2;
    const std::size_t Hpad     = H + m - 1;
    const std::size_t Wpad     = W + n - 1;

    // Build padded array in DOUBLE (we always read out via
    // elemAsDouble; this saves a class-specific dispatch).
    std::pmr::vector<double> aa(Hpad * Wpad, padval, mr);
    for (std::size_t j = 0; j < W; ++j)
        for (std::size_t i = 0; i < H; ++i)
            aa[(j + pad_left) * Hpad + (i + pad_top)] = A.elemAsDouble(j * H + i);

    // Allocate scratch window (DOUBLE — the class for `fun` is what
    // the kernel receives; MATLAB nlfilter forwards the same class
    // as `A`, but we choose DOUBLE here for simplicity. Tests use
    // class-agnostic kernels like `@(x) mean(x(:))`).
    Value window = Value::matrix(m, n, ValueType::DOUBLE, mr);
    double *wd = window.doubleDataMut();

    // First-call invocation determines output class.
    auto fill_window = [&](std::size_t i, std::size_t j) {
        for (std::size_t c = 0; c < n; ++c)
            for (std::size_t r = 0; r < m; ++r)
                wd[c * m + r] = aa[(j + c) * Hpad + (i + r)];
    };

    fill_window(0, 0);
    Value first = eng.callFunctionHandle(
        fun, Span<const Value>(&window, 1));
    if (first.numel() != 1)
        throw Error("nlfilter: fun must return a scalar",
                    0, 0, "nlfilter", "", "m:nlfilter:funScalar");
    const ValueType outT = first.type();
    Value B = Value::matrix(H, W, outT, mr);

    auto store = [&](std::size_t r, std::size_t c, const Value &v) {
        const std::size_t idx = c * H + r;
        const double d = v.toScalar();
        switch (outT) {
            case ValueType::DOUBLE:  B.doubleDataMut()[idx]  = d; break;
            case ValueType::SINGLE:  B.singleDataMut()[idx]  = static_cast<float>(d); break;
            case ValueType::UINT8:   B.uint8DataMut()[idx]   = static_cast<uint8_t>(d); break;
            case ValueType::UINT16:  B.uint16DataMut()[idx]  = static_cast<uint16_t>(d); break;
            case ValueType::UINT32:  B.uint32DataMut()[idx]  = static_cast<uint32_t>(d); break;
            case ValueType::UINT64:  B.uint64DataMut()[idx]  = static_cast<uint64_t>(d); break;
            case ValueType::INT8:    B.int8DataMut()[idx]    = static_cast<int8_t>(d); break;
            case ValueType::INT16:   B.int16DataMut()[idx]   = static_cast<int16_t>(d); break;
            case ValueType::INT32:   B.int32DataMut()[idx]   = static_cast<int32_t>(d); break;
            case ValueType::INT64:   B.int64DataMut()[idx]   = static_cast<int64_t>(d); break;
            case ValueType::LOGICAL: B.logicalDataMut()[idx] = d != 0.0 ? 1 : 0; break;
            default:
                throw Error("nlfilter: unsupported fun output class",
                            0, 0, "nlfilter", "", "m:nlfilter:outCls");
        }
    };

    store(0, 0, first);

    for (std::size_t i = 0; i < H; ++i) {
        for (std::size_t j = 0; j < W; ++j) {
            if (i == 0 && j == 0) continue;  // already filled
            fill_window(i, j);
            Value r = eng.callFunctionHandle(
                fun, Span<const Value>(&window, 1));
            if (r.numel() != 1)
                throw Error("nlfilter: fun must return a scalar at all (i,j)",
                            0, 0, "nlfilter", "", "m:nlfilter:funScalar");
            store(i, j, r);
        }
    }
    return B;
}

// ── colfilt (column-wise neighbourhood) ───────────────────────────
//
// MATLAB R2025b colfilt.m:
//   B = colfilt(A, [m n], block_type, fun)         (whole-matrix)
//   B = colfilt(A, [m n], [mblock nblock], block_type, fun)
//   B = colfilt(A, 'indexed', …)
//
// block_type ∈ {'sliding', 'distinct'} (case-insensitive, abbrev'd
// by leading char). Sliding mode:
//   1. Pad A by (m-1, n-1) with 0 (or 1 for 'indexed' double/single).
//   2. X is the matrix whose columns are the m*n elements of every
//      m × n window centred on (i, j); shape m*n × (H*W).
//   3. Call fun(X) — must return a row vector 1 × (H*W).
//   4. Reshape into H × W.
// Distinct mode:
//   1. Pad A to next multiple of [m, n].
//   2. X has one column per distinct m × n block.
//   3. fun(X) must return a same-size matrix; the columns are then
//      unpacked back into blocks (col2im 'distinct').
//   4. Crop the assembled image back to size(A).
//
// The optional [mblock nblock] arg is purely a memory optimisation
// (MATLAB explicitly notes: "does not change the result"). The
// engine adapter accepts and ignores it.
//
// Output class equals the class of fun()'s return value.
Value colfilt(numkit::Engine &eng, const Value &A,
              std::size_t m, std::size_t n,
              const std::string &block_type, const Value &fun,
              bool indexed,
              std::pmr::memory_resource *mr)
{
    if (m < 1 || n < 1)
        throw Error("colfilt: block size must be positive",
                    0, 0, "colfilt", "", "m:colfilt:nhood");
    if (!fun.isFuncHandle())
        throw Error("colfilt: fun must be a function handle",
                    0, 0, "colfilt", "", "m:colfilt:fun");

    const auto &dA = A.dims();
    if (dA.is3D())
        throw Error("colfilt: A must be a 2-D image",
                    0, 0, "colfilt", "", "m:colfilt:rank");
    const std::size_t H = dA.rows();
    const std::size_t W = dA.cols();
    const ValueType inT = A.type();

    std::string kind = block_type;
    for (auto &c : kind) c = static_cast<char>(
        std::tolower(static_cast<unsigned char>(c)));
    if (kind.empty())
        throw Error("colfilt: block_type must be 'sliding' or 'distinct'",
                    0, 0, "colfilt", "", "m:colfilt:blockType");
    const char first = kind[0];
    if (first != 's' && first != 'd')
        throw Error("colfilt: block_type must be 'sliding' or 'distinct'",
                    0, 0, "colfilt", "", "m:colfilt:blockType");

    // Common padval rule.
    double padval = 0.0;
    if (indexed && (inT == ValueType::DOUBLE || inT == ValueType::SINGLE))
        padval = 1.0;

    if (first == 's') {
        // ── Sliding ─────────────────────────────────────────────
        const std::size_t pad_top  = (m - 1) / 2;
        const std::size_t pad_left = (n - 1) / 2;
        const std::size_t Hpad = H + m - 1;
        const std::size_t Wpad = W + n - 1;
        const std::size_t Ncol = H * W;

        // Build X = m*n × Ncol in DOUBLE.
        Value X = Value::matrix(m * n, Ncol, ValueType::DOUBLE, mr);
        double *xd = X.doubleDataMut();

        // Pad row-by-col into a temporary scratch.
        std::pmr::vector<double> aa(Hpad * Wpad, padval, mr);
        for (std::size_t j = 0; j < W; ++j)
            for (std::size_t i = 0; i < H; ++i)
                aa[(j + pad_left) * Hpad + (i + pad_top)] =
                    A.elemAsDouble(j * H + i);

        // For each centre (i, j), gather m*n window values into
        // column k = j*H + i.
        for (std::size_t j = 0; j < W; ++j) {
            for (std::size_t i = 0; i < H; ++i) {
                const std::size_t k = j * H + i;
                std::size_t r = 0;
                // im2col 'sliding' iteration order: column-major
                // within each window, i.e. (col-major) flatten.
                for (std::size_t wc = 0; wc < n; ++wc)
                    for (std::size_t wr = 0; wr < m; ++wr)
                        xd[k * (m * n) + (r++)]
                            = aa[(j + wc) * Hpad + (i + wr)];
            }
        }

        Value result = eng.callFunctionHandle(
            fun, Span<const Value>(&X, 1));
        if (result.numel() != Ncol)
            throw Error("colfilt: sliding fun must return a 1 × N row "
                        "vector (one value per column)",
                        0, 0, "colfilt", "", "m:colfilt:funShape");

        // Reshape result (regardless of [1, Ncol] or [Ncol, 1]) into H × W.
        Value B = Value::matrix(H, W, result.type(), mr);
        const auto outT = result.type();
        for (std::size_t k = 0; k < Ncol; ++k) {
            const double v = result.elemAsDouble(k);
            const std::size_t idx = k;
            switch (outT) {
                case ValueType::DOUBLE: B.doubleDataMut()[idx]  = v; break;
                case ValueType::SINGLE: B.singleDataMut()[idx]  = static_cast<float>(v); break;
                case ValueType::UINT8:  B.uint8DataMut()[idx]   = static_cast<uint8_t>(v); break;
                case ValueType::UINT16: B.uint16DataMut()[idx]  = static_cast<uint16_t>(v); break;
                case ValueType::INT16:  B.int16DataMut()[idx]   = static_cast<int16_t>(v); break;
                case ValueType::INT32:  B.int32DataMut()[idx]   = static_cast<int32_t>(v); break;
                case ValueType::LOGICAL: B.logicalDataMut()[idx] = v != 0 ? 1 : 0; break;
                default:
                    throw Error("colfilt: unsupported fun output class",
                                0, 0, "colfilt", "", "m:colfilt:outCls");
            }
        }
        return B;
    }

    // ── Distinct ────────────────────────────────────────────────────
    const std::size_t mpad = (H % m) ? (m - H % m) : 0;
    const std::size_t npad = (W % n) ? (n - W % n) : 0;
    const std::size_t Hpad = H + mpad;
    const std::size_t Wpad = W + npad;
    const std::size_t mblocks = Hpad / m;
    const std::size_t nblocks = Wpad / n;
    const std::size_t Ncol    = mblocks * nblocks;

    // Pad to multiple of (m, n).
    std::pmr::vector<double> aa(Hpad * Wpad, padval, mr);
    for (std::size_t j = 0; j < W; ++j)
        for (std::size_t i = 0; i < H; ++i)
            aa[j * Hpad + i] = A.elemAsDouble(j * H + i);

    // Build X = m*n × Ncol; column index k = bj * mblocks + bi.
    Value X = Value::matrix(m * n, Ncol, ValueType::DOUBLE, mr);
    double *xd = X.doubleDataMut();
    for (std::size_t bj = 0; bj < nblocks; ++bj) {
        for (std::size_t bi = 0; bi < mblocks; ++bi) {
            const std::size_t k = bj * mblocks + bi;
            std::size_t r = 0;
            for (std::size_t wc = 0; wc < n; ++wc)
                for (std::size_t wr = 0; wr < m; ++wr)
                    xd[k * (m * n) + (r++)]
                        = aa[(bj * n + wc) * Hpad + (bi * m + wr)];
        }
    }

    Value result = eng.callFunctionHandle(
        fun, Span<const Value>(&X, 1));
    if (result.numel() != m * n * Ncol)
        throw Error("colfilt: distinct fun must return a matrix of the "
                    "same shape as its input (m*n × N)",
                    0, 0, "colfilt", "", "m:colfilt:funShape");

    // Reassemble padded image, then crop.
    const auto outT = result.type();
    std::pmr::vector<double> bb(Hpad * Wpad, 0.0, mr);
    for (std::size_t bj = 0; bj < nblocks; ++bj) {
        for (std::size_t bi = 0; bi < mblocks; ++bi) {
            const std::size_t k = bj * mblocks + bi;
            std::size_t r = 0;
            for (std::size_t wc = 0; wc < n; ++wc)
                for (std::size_t wr = 0; wr < m; ++wr) {
                    const double v = result.elemAsDouble(k * (m * n) + (r++));
                    bb[(bj * n + wc) * Hpad + (bi * m + wr)] = v;
                }
        }
    }

    Value B = Value::matrix(H, W, outT, mr);
    for (std::size_t j = 0; j < W; ++j) {
        for (std::size_t i = 0; i < H; ++i) {
            const double v = bb[j * Hpad + i];
            const std::size_t idx = j * H + i;
            switch (outT) {
                case ValueType::DOUBLE: B.doubleDataMut()[idx]  = v; break;
                case ValueType::SINGLE: B.singleDataMut()[idx]  = static_cast<float>(v); break;
                case ValueType::UINT8:  B.uint8DataMut()[idx]   = static_cast<uint8_t>(v); break;
                case ValueType::UINT16: B.uint16DataMut()[idx]  = static_cast<uint16_t>(v); break;
                case ValueType::INT16:  B.int16DataMut()[idx]   = static_cast<int16_t>(v); break;
                case ValueType::INT32:  B.int32DataMut()[idx]   = static_cast<int32_t>(v); break;
                case ValueType::LOGICAL: B.logicalDataMut()[idx] = v != 0 ? 1 : 0; break;
                default:
                    throw Error("colfilt: unsupported fun output class",
                                0, 0, "colfilt", "", "m:colfilt:outCls");
            }
        }
    }
    return B;
}

// ── imguidedfilter (Guided Image Filter, He/Sun/Tang 2013) ────────
//
// Algorithm 1 from K. He, J. Sun, X. Tang,
//   "Guided Image Filtering", IEEE TPAMI 35(6), 1397-1409, 2013.
//
//   meanI  = box(G)
//   meanP  = box(A)
//   corrI  = box(G·G)
//   corrIP = box(G·A)
//   varI  = corrI − meanI²
//   covIP = corrIP − meanI·meanP
//   a = covIP / (varI + ε)
//   b = meanP − a·meanI
//   meana = box(a)
//   meanb = box(b)
//   B = meana·G + meanb
//
// Grayscale guide only here. Default ε = 0.01 · range².
Value imguidedfilter(const Value &A, const Value &G_in, int nhood,
                     double eps_in,
                     std::pmr::memory_resource *mr)
{
    if (A.dims().is3D())
        throw Error("imguidedfilter: A must be 2-D",
                    0, 0, "imguidedfilter", "",
                    "m:imguidedfilter:dim");
    if (nhood < 1 || (nhood % 2) == 0)
        throw Error("imguidedfilter: NeighborhoodSize must be positive odd "
                    "integer",
                    0, 0, "imguidedfilter", "",
                    "m:imguidedfilter:nhood");
    const std::size_t H = A.dims().rows();
    const std::size_t W = A.dims().cols();
    if (H == 0 || W == 0) return A;

    Value G = G_in;
    if (G.numel() == 0) G = A;  // self-guidance
    if (G.dims().rows() != H || G.dims().cols() != W || G.dims().is3D())
        throw Error("imguidedfilter: G must be the same H × W as A "
                    "(RGB guidance not yet supported)",
                    0, 0, "imguidedfilter", "",
                    "m:imguidedfilter:gshape");

    // Resolve default ε per A's class.
    double eps = eps_in;
    if (eps < 0) {
        double range = 1.0;
        switch (A.type()) {
            case ValueType::UINT8:  range = 255.0;       break;
            case ValueType::UINT16: range = 65535.0;     break;
            case ValueType::UINT32: range = 4294967295.0;break;
            case ValueType::INT8:   range = 255.0;       break;  // [-128,127] diff=255
            case ValueType::INT16:  range = 65535.0;     break;
            case ValueType::INT32:  range = 4294967295.0;break;
            default:                range = 1.0;         break;  // double/single/logical
        }
        eps = 0.01 * range * range;
    }

    // Cast A, G → DOUBLE.
    const std::size_t N = H * W;
    Value Ad = Value::matrix(H, W, ValueType::DOUBLE, mr);
    Value Gd = Value::matrix(H, W, ValueType::DOUBLE, mr);
    for (std::size_t i = 0; i < N; ++i) Ad.doubleDataMut()[i] = A.elemAsDouble(i);
    for (std::size_t i = 0; i < N; ++i) Gd.doubleDataMut()[i] = G.elemAsDouble(i);

    // I.*I, I.*P (element-wise products).
    Value II = Value::matrix(H, W, ValueType::DOUBLE, mr);
    Value IP = Value::matrix(H, W, ValueType::DOUBLE, mr);
    for (std::size_t i = 0; i < N; ++i) {
        const double gi = Gd.doubleData()[i];
        const double ai = Ad.doubleData()[i];
        II.doubleDataMut()[i] = gi * gi;
        IP.doubleDataMut()[i] = gi * ai;
    }

    const Value meanI  = imboxfilt(Gd, nhood, mr);
    const Value meanP  = imboxfilt(Ad, nhood, mr);
    const Value corrI  = imboxfilt(II, nhood, mr);
    const Value corrIP = imboxfilt(IP, nhood, mr);

    // a, b.
    Value a_ = Value::matrix(H, W, ValueType::DOUBLE, mr);
    Value b_ = Value::matrix(H, W, ValueType::DOUBLE, mr);
    for (std::size_t i = 0; i < N; ++i) {
        const double mI = meanI.doubleData()[i];
        const double mP = meanP.doubleData()[i];
        const double varI  = corrI.doubleData()[i]  - mI * mI;
        const double covIP = corrIP.doubleData()[i] - mI * mP;
        const double a = covIP / (varI + eps);
        a_.doubleDataMut()[i] = a;
        b_.doubleDataMut()[i] = mP - a * mI;
    }

    const Value meana = imboxfilt(a_, nhood, mr);
    const Value meanb = imboxfilt(b_, nhood, mr);

    // B = meana·G + meanb.
    Value Bd = Value::matrix(H, W, ValueType::DOUBLE, mr);
    for (std::size_t i = 0; i < N; ++i)
        Bd.doubleDataMut()[i] = meana.doubleData()[i] * Gd.doubleData()[i]
                              + meanb.doubleData()[i];

    // Cast back to class(A).
    const ValueType outT = A.type();
    if (outT == ValueType::DOUBLE) return Bd;
    Value Bout = Value::matrix(H, W, outT, mr);
    auto saturate = [&](double v, double lo, double hi) {
        v = std::round(v);
        if (v < lo) v = lo;
        if (v > hi) v = hi;
        return v;
    };
    for (std::size_t i = 0; i < N; ++i) {
        const double v = Bd.doubleData()[i];
        switch (outT) {
            case ValueType::SINGLE:  Bout.singleDataMut()[i] = static_cast<float>(v); break;
            case ValueType::UINT8:   Bout.uint8DataMut()[i]  = static_cast<std::uint8_t>(saturate(v, 0.0, 255.0)); break;
            case ValueType::UINT16:  Bout.uint16DataMut()[i] = static_cast<std::uint16_t>(saturate(v, 0.0, 65535.0)); break;
            case ValueType::INT8:    Bout.int8DataMut()[i]   = static_cast<std::int8_t>(saturate(v, -128.0, 127.0)); break;
            case ValueType::INT16:   Bout.int16DataMut()[i]  = static_cast<std::int16_t>(saturate(v, -32768.0, 32767.0)); break;
            case ValueType::INT32:   Bout.int32DataMut()[i]  = static_cast<std::int32_t>(saturate(v, -2147483648.0, 2147483647.0)); break;
            case ValueType::UINT32:  Bout.uint32DataMut()[i] = static_cast<std::uint32_t>(saturate(v, 0.0, 4294967295.0)); break;
            case ValueType::LOGICAL: Bout.logicalDataMut()[i] = v >= 0.5 ? 1 : 0; break;
            default: Bout.doubleDataMut()[i] = v; break;
        }
    }
    return Bout;
}

namespace detail {

void nlfilter_reg(Span<const Value> args, std::size_t /*nargout*/,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("nlfilter: requires (A, [m n], fun) or "
                    "(A, 'indexed', [m n], fun)",
                    0, 0, "nlfilter", "", "m:nlfilter:nargin");
    auto *mr = ctx.engine->resource();

    bool indexed = false;
    std::size_t k = 1;
    if (args[1].isChar() || args[1].isString()) {
        std::string s = args[1].toString();
        for (auto &c : s) c = static_cast<char>(
            std::tolower(static_cast<unsigned char>(c)));
        if (s != "indexed")
            throw Error("nlfilter: unknown literal '" + args[1].toString()
                      + "' (expected 'indexed')",
                        0, 0, "nlfilter", "", "m:nlfilter:badLiteral");
        indexed = true;
        k = 2;
    }
    if (k + 1 >= args.size())
        throw Error("nlfilter: requires (A, [m n], fun) "
                    "or (A, 'indexed', [m n], fun)",
                    0, 0, "nlfilter", "", "m:nlfilter:nargin");
    const Value &nh = args[k];
    const Value &fn = args[k + 1];
    if (nh.numel() != 2)
        throw Error("nlfilter: neighbourhood must be a 2-element vector",
                    0, 0, "nlfilter", "", "m:nlfilter:nhood");
    const std::size_t m = static_cast<std::size_t>(nh.elemAsDouble(0));
    const std::size_t n = static_cast<std::size_t>(nh.elemAsDouble(1));
    outs[0] = nlfilter(*ctx.engine, args[0], m, n, fn, indexed, mr);
}

void colfilt_reg(Span<const Value> args, std::size_t /*nargout*/,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 4)
        throw Error("colfilt: requires (A, [m n], block_type, fun) "
                    "or (A, [m n], [mblock nblock], block_type, fun)",
                    0, 0, "colfilt", "", "m:colfilt:nargin");
    auto *mr = ctx.engine->resource();

    bool indexed = false;
    std::size_t k = 1;
    if (args[1].isChar() || args[1].isString()) {
        std::string s = args[1].toString();
        for (auto &c : s) c = static_cast<char>(
            std::tolower(static_cast<unsigned char>(c)));
        if (s != "indexed")
            throw Error("colfilt: unknown literal '" + args[1].toString()
                      + "' (expected 'indexed')",
                        0, 0, "colfilt", "", "m:colfilt:badLiteral");
        indexed = true;
        k = 2;
    }
    if (k + 2 >= args.size())
        throw Error("colfilt: requires (A, [m n], block_type, fun)",
                    0, 0, "colfilt", "", "m:colfilt:nargin");
    const Value &nh = args[k];
    if (nh.numel() != 2)
        throw Error("colfilt: neighbourhood must be a 2-element vector",
                    0, 0, "colfilt", "", "m:colfilt:nhood");
    const std::size_t m = static_cast<std::size_t>(nh.elemAsDouble(0));
    const std::size_t n = static_cast<std::size_t>(nh.elemAsDouble(1));

    // Detect optional [mblock nblock] — present iff arg[k+1] is a
    // 2-element numeric vector AND arg[k+2] is a string AND arg[k+3]
    // exists (the function handle).
    std::size_t bi = k + 1;
    if (!args[bi].isChar() && !args[bi].isString()
        && args[bi].numel() == 2
        && (bi + 2) < args.size()
        && (args[bi + 1].isChar() || args[bi + 1].isString())) {
        // mblock / nblock is purely a memory optimisation per MATLAB
        // docs — ignore it and proceed.
        bi = bi + 1;
    }
    if (bi + 1 >= args.size())
        throw Error("colfilt: missing block_type and/or fun argument",
                    0, 0, "colfilt", "", "m:colfilt:nargin");
    if (!args[bi].isChar() && !args[bi].isString())
        throw Error("colfilt: block_type must be 'sliding' or 'distinct'",
                    0, 0, "colfilt", "", "m:colfilt:blockType");
    const std::string kind = args[bi].toString();
    const Value &fn = args[bi + 1];
    outs[0] = colfilt(*ctx.engine, args[0], m, n, kind, fn, indexed, mr);
}

void imguidedfilter_reg(Span<const Value> args, std::size_t /*nargout*/,
                        Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("imguidedfilter: requires (A [, G] [, NV...])",
                    0, 0, "imguidedfilter", "",
                    "m:imguidedfilter:nargin");
    auto *mr = ctx.engine->resource();
    auto is_string = [](const Value &v) { return v.isChar() || v.isString(); };

    Value A = args[0];
    Value G;  // empty → self-guide
    int nhood = 5;
    double eps = -1.0;  // sentinel → class-based default

    std::size_t i = 1;
    if (i < args.size() && !is_string(args[i])) {
        G = args[i];
        ++i;
    }
    while (i + 1 < args.size()) {
        if (!is_string(args[i]))
            throw Error("imguidedfilter: expected NV-pair name",
                        0, 0, "imguidedfilter", "",
                        "m:imguidedfilter:badNv");
        std::string name = args[i].toString();
        std::string nlo;
        for (char ch : name)
            nlo += static_cast<char>(std::tolower(
                static_cast<unsigned char>(ch)));
        if (nlo == "neighborhoodsize") {
            const Value &v = args[i + 1];
            if (v.numel() == 1) {
                nhood = static_cast<int>(v.toScalar());
            } else if (v.numel() == 2) {
                const int n0 = static_cast<int>(v.elemAsDouble(0));
                const int n1 = static_cast<int>(v.elemAsDouble(1));
                if (n0 != n1)
                    throw Error("imguidedfilter: non-square NeighborhoodSize "
                                "not yet supported",
                                0, 0, "imguidedfilter", "",
                                "m:imguidedfilter:nhoodNonsq");
                nhood = n0;
            } else {
                throw Error("imguidedfilter: NeighborhoodSize must be a "
                            "scalar or 2-element vector",
                            0, 0, "imguidedfilter", "",
                            "m:imguidedfilter:nhoodSize");
            }
        } else if (nlo == "degreeofsmoothing") {
            eps = args[i + 1].toScalar();
            if (!(eps > 0) || !std::isfinite(eps))
                throw Error("imguidedfilter: DegreeOfSmoothing must be a "
                            "positive finite scalar",
                            0, 0, "imguidedfilter", "",
                            "m:imguidedfilter:dos");
        } else {
            throw Error("imguidedfilter: unknown option '" + name + "'",
                        0, 0, "imguidedfilter", "",
                        "m:imguidedfilter:unknownNv");
        }
        i += 2;
    }
    outs[0] = imguidedfilter(A, G, nhood, eps, mr);
}

} // namespace detail
} // namespace numkit::image
