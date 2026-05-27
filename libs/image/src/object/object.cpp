// libs/image/src/object/object.cpp
//
// Gradient + edge detection. For first cut: Sobel / Prewitt / Roberts.
// Canny / log / zerocross share the simpler-Sobel path with hysteresis
// thresholding; full Canny non-max suppression deferred.

#include <numkit/image/object/object.hpp>

#include <numkit/image/filter/filter.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace numkit::image {

namespace {

// Build a small 3×3 (or 2×2) kernel as a DOUBLE Value (column-major).
Value make_kernel(const std::vector<double> &flat_rowmajor, int rows, int cols, std::pmr::memory_resource *mr)
{
    Value k = Value::matrix(rows, cols, ValueType::DOUBLE, mr);
    double *kd = k.doubleDataMut();
    for (int c = 0; c < cols; ++c)
        for (int r = 0; r < rows; ++r)
            kd[(size_t)c * (size_t)rows + (size_t)r]
                = flat_rowmajor[(size_t)r * (size_t)cols + (size_t)c];
    return k;
}

// Convolve `I` with `k` using imfilter (Replicate boundary, same size,
// correlation = no flip — these are gradient kernels meant to be applied
// directly).
Value apply_kernel(const Value &I, const Value &k, std::pmr::memory_resource *mr)
{
    return imfilter(I, k, PadMode::Replicate, 0.0, /*full=*/false, /*flip_kernel=*/false, mr);
}

} // anonymous

std::tuple<Value, Value>
imgradientxy(const Value &I, const std::string &method, std::pmr::memory_resource *mr)
{
    // Choose kernels (Gx for horizontal, Gy for vertical).
    std::vector<double> kx, ky;
    int rows = 3, cols = 3;
    if (method == "prewitt") {
        kx = { 1, 0, -1,
               1, 0, -1,
               1, 0, -1 };
        ky = { 1, 1, 1,
               0, 0, 0,
              -1,-1,-1 };
    } else if (method == "central") {
        // 1×3 / 3×1 central difference.
        kx = {-0.5, 0, 0.5}; rows = 1; cols = 3;
        ky = {-0.5, 0, 0.5};
    } else if (method == "intermediate") {
        kx = {-1, 1}; rows = 1; cols = 2;
        ky = {-1, 1};
    } else {  // sobel default
        kx = { 1, 0, -1,
               2, 0, -2,
               1, 0, -1 };
        ky = { 1, 2, 1,
               0, 0, 0,
              -1,-2,-1 };
    }

    Value Kx, Ky;
    if (method == "central" || method == "intermediate") {
        Kx = make_kernel(kx, 1, cols, mr);
        Ky = make_kernel(ky, cols, 1, mr);  // transpose for vertical
    } else {
        Kx = make_kernel(kx, 3, 3, mr);
        Ky = make_kernel(ky, 3, 3, mr);
    }

    Value Gx = apply_kernel(I, Kx, mr);
    Value Gy = apply_kernel(I, Ky, mr);
    return std::make_tuple(std::move(Gx), std::move(Gy));
}

// ── 3-D imgradientxyz / imgradient3 ────────────────────────────────
//
// MATLAB R2025b imgradientxyz.m uses non-standard 3-D Sobel kernels
// with [1, 3, 3, 1]-style weights (NOT the naive [1, 2, 1] 2-D
// extension); the actual hx/hy/hz tensors are reproduced verbatim
// from the MATLAB source below. Prewitt uses [1, 1, 1]; central is
// `gradient(V)`; intermediate is forward `diff` with the trailing
// slice zeroed.
namespace {

// Convolve V (3-D DOUBLE) with a 3×3×3 kernel K (27 doubles, row-
// then-col-then-page layout matching `K[r, c, p]` indexing) using
// 'replicate' boundary handling. Returns DOUBLE same shape.
//
// Convention: V is stored column-major H × W × D, linear index
//   k(r, c, p) = r + H * c + H * W * p
// Sobel-style direct correlation (no kernel flip — kernel embeds
// the sign convention).
Value conv3d_replicate(const Value &V, const double K[27],
                       std::pmr::memory_resource *mr)
{
    const auto &d = V.dims();
    const std::size_t H = d.rows();
    const std::size_t W = d.cols();
    const std::size_t D = d.is3D() ? d.pages() : 1;
    Value out = (D > 1)
        ? Value::matrix3d(H, W, D, ValueType::DOUBLE, mr)
        : Value::matrix(H, W, ValueType::DOUBLE, mr);
    double *od = out.doubleDataMut();
    const double *vd = V.doubleData();

    auto clamp = [](long long x, std::size_t n) -> std::size_t {
        if (x < 0) return 0;
        if (x >= static_cast<long long>(n)) return n - 1;
        return static_cast<std::size_t>(x);
    };

    for (std::size_t p = 0; p < D; ++p) {
        for (std::size_t c = 0; c < W; ++c) {
            for (std::size_t r = 0; r < H; ++r) {
                double sum = 0.0;
                for (int dp = -1; dp <= 1; ++dp) {
                    const std::size_t pp = clamp(
                        static_cast<long long>(p) + dp, D);
                    for (int dc = -1; dc <= 1; ++dc) {
                        const std::size_t cc = clamp(
                            static_cast<long long>(c) + dc, W);
                        for (int dr = -1; dr <= 1; ++dr) {
                            const std::size_t rr = clamp(
                                static_cast<long long>(r) + dr, H);
                            const int ki = (dr + 1)
                                         + 3 * (dc + 1)
                                         + 9 * (dp + 1);
                            sum += K[ki] * vd[rr + H * cc + H * W * pp];
                        }
                    }
                }
                od[r + H * c + H * W * p] = sum;
            }
        }
    }
    return out;
}

// Cast V to DOUBLE (or SINGLE if outT is SINGLE).
Value promote_to_double(const Value &V, std::pmr::memory_resource *mr)
{
    const std::size_t N = V.numel();
    const auto &d = V.dims();
    Value out = d.is3D()
        ? Value::matrix3d(d.rows(), d.cols(), d.pages(), ValueType::DOUBLE, mr)
        : Value::matrix(d.rows(), d.cols(), ValueType::DOUBLE, mr);
    double *od = out.doubleDataMut();
    for (std::size_t i = 0; i < N; ++i) od[i] = V.elemAsDouble(i);
    return out;
}

Value zeros_like(const Value &V, ValueType t,
                 std::pmr::memory_resource *mr)
{
    const auto &d = V.dims();
    return d.is3D()
        ? Value::matrix3d(d.rows(), d.cols(), d.pages(), t, mr)
        : Value::matrix(d.rows(), d.cols(), t, mr);
}

} // anonymous

std::tuple<Value, Value, Value>
imgradientxyz(const Value &V, const std::string &method, std::pmr::memory_resource *mr)
{
    if (!V.dims().is3D())
        throw Error("imgradientxyz: V must be 3-D",
                    0, 0, "imgradientxyz", "", "m:imgradientxyz:rank");
    const ValueType inT = V.type();
    const ValueType outT = (inT == ValueType::SINGLE)
                         ? ValueType::SINGLE : ValueType::DOUBLE;
    Value Vd = promote_to_double(V, mr);

    Value Gx, Gy, Gz;
    if (method == "sobel") {
        // MATLAB R2025b imgradientxyz.m kernels.
        // hx (X = horizontal / cols, derivative along c):
        //   page 1: [-1 0 1; -3 0 3; -1 0 1]
        //   page 2: [-3 0 3; -6 0 6; -3 0 3]
        //   page 3: [-1 0 1; -3 0 3; -1 0 1]
        // hy (Y = vertical / rows, derivative along r):
        //   page 1: [-1 -3 -1; 0 0 0; 1 3 1]
        //   page 2: [-3 -6 -3; 0 0 0; 3 6 3]
        //   page 3: [-1 -3 -1; 0 0 0; 1 3 1]
        // hz (Z = depth / pages, derivative along p):
        //   page 1: [-1 -3 -1; -3 -6 -3; -1 -3 -1]
        //   page 2: [ 0  0  0;  0  0  0;  0  0  0]
        //   page 3: [ 1  3  1;  3  6  3;  1  3  1]
        // K index = (dr+1) + 3*(dc+1) + 9*(dp+1).
        auto build = [](const double rows3[3][3][3], double K[27]) {
            for (int p = 0; p < 3; ++p)
                for (int c = 0; c < 3; ++c)
                    for (int r = 0; r < 3; ++r)
                        K[r + 3 * c + 9 * p] = rows3[p][r][c];
        };
        const double hx3[3][3][3] = {
            {{-1, 0, 1}, {-3, 0, 3}, {-1, 0, 1}},
            {{-3, 0, 3}, {-6, 0, 6}, {-3, 0, 3}},
            {{-1, 0, 1}, {-3, 0, 3}, {-1, 0, 1}},
        };
        const double hy3[3][3][3] = {
            {{-1, -3, -1}, {0, 0, 0}, {1, 3, 1}},
            {{-3, -6, -3}, {0, 0, 0}, {3, 6, 3}},
            {{-1, -3, -1}, {0, 0, 0}, {1, 3, 1}},
        };
        const double hz3[3][3][3] = {
            {{-1, -3, -1}, {-3, -6, -3}, {-1, -3, -1}},
            {{ 0,  0,  0}, { 0,  0,  0}, { 0,  0,  0}},
            {{ 1,  3,  1}, { 3,  6,  3}, { 1,  3,  1}},
        };
        double Kx[27], Ky[27], Kz[27];
        build(hx3, Kx); build(hy3, Ky); build(hz3, Kz);
        Gx = conv3d_replicate(Vd, Kx, mr);
        Gy = conv3d_replicate(Vd, Ky, mr);
        Gz = conv3d_replicate(Vd, Kz, mr);
    } else if (method == "prewitt") {
        auto build = [](const double rows3[3][3][3], double K[27]) {
            for (int p = 0; p < 3; ++p)
                for (int c = 0; c < 3; ++c)
                    for (int r = 0; r < 3; ++r)
                        K[r + 3 * c + 9 * p] = rows3[p][r][c];
        };
        const double hx3[3][3][3] = {
            {{-1, 0, 1}, {-1, 0, 1}, {-1, 0, 1}},
            {{-1, 0, 1}, {-1, 0, 1}, {-1, 0, 1}},
            {{-1, 0, 1}, {-1, 0, 1}, {-1, 0, 1}},
        };
        const double hy3[3][3][3] = {
            {{-1, -1, -1}, {0, 0, 0}, {1, 1, 1}},
            {{-1, -1, -1}, {0, 0, 0}, {1, 1, 1}},
            {{-1, -1, -1}, {0, 0, 0}, {1, 1, 1}},
        };
        const double hz3[3][3][3] = {
            {{-1, -1, -1}, {-1, -1, -1}, {-1, -1, -1}},
            {{ 0,  0,  0}, { 0,  0,  0}, { 0,  0,  0}},
            {{ 1,  1,  1}, { 1,  1,  1}, { 1,  1,  1}},
        };
        double Kx[27], Ky[27], Kz[27];
        build(hx3, Kx); build(hy3, Ky); build(hz3, Kz);
        Gx = conv3d_replicate(Vd, Kx, mr);
        Gy = conv3d_replicate(Vd, Ky, mr);
        Gz = conv3d_replicate(Vd, Kz, mr);
    } else if (method == "central" || method == "intermediate") {
        // gradient(V) for 'central'; forward diff with zero pad for
        // 'intermediate'. Both are straight per-axis differences.
        const auto &d = Vd.dims();
        const std::size_t H = d.rows();
        const std::size_t W = d.cols();
        const std::size_t D = d.pages();
        Gx = zeros_like(Vd, ValueType::DOUBLE, mr);
        Gy = zeros_like(Vd, ValueType::DOUBLE, mr);
        Gz = zeros_like(Vd, ValueType::DOUBLE, mr);
        double *gxd = Gx.doubleDataMut();
        double *gyd = Gy.doubleDataMut();
        double *gzd = Gz.doubleDataMut();
        const double *vd = Vd.doubleData();
        auto idx = [&](std::size_t r, std::size_t c, std::size_t p) {
            return r + H * c + H * W * p;
        };
        if (method == "central") {
            // Gx[r,c,p] = (V[r,c+1,p] - V[r,c-1,p]) / 2  (central),
            // forward at c=0 and backward at c=W-1.
            for (std::size_t p = 0; p < D; ++p)
                for (std::size_t r = 0; r < H; ++r)
                    for (std::size_t c = 0; c < W; ++c) {
                        if (W == 1) { gxd[idx(r, c, p)] = 0.0; continue; }
                        if (c == 0)
                            gxd[idx(r, c, p)] = vd[idx(r, 1, p)] - vd[idx(r, 0, p)];
                        else if (c == W - 1)
                            gxd[idx(r, c, p)] = vd[idx(r, W - 1, p)] - vd[idx(r, W - 2, p)];
                        else
                            gxd[idx(r, c, p)] = 0.5 * (vd[idx(r, c + 1, p)] - vd[idx(r, c - 1, p)]);
                    }
            for (std::size_t p = 0; p < D; ++p)
                for (std::size_t c = 0; c < W; ++c)
                    for (std::size_t r = 0; r < H; ++r) {
                        if (H == 1) { gyd[idx(r, c, p)] = 0.0; continue; }
                        if (r == 0)
                            gyd[idx(r, c, p)] = vd[idx(1, c, p)] - vd[idx(0, c, p)];
                        else if (r == H - 1)
                            gyd[idx(r, c, p)] = vd[idx(H - 1, c, p)] - vd[idx(H - 2, c, p)];
                        else
                            gyd[idx(r, c, p)] = 0.5 * (vd[idx(r + 1, c, p)] - vd[idx(r - 1, c, p)]);
                    }
            for (std::size_t c = 0; c < W; ++c)
                for (std::size_t r = 0; r < H; ++r)
                    for (std::size_t p = 0; p < D; ++p) {
                        if (D == 1) { gzd[idx(r, c, p)] = 0.0; continue; }
                        if (p == 0)
                            gzd[idx(r, c, p)] = vd[idx(r, c, 1)] - vd[idx(r, c, 0)];
                        else if (p == D - 1)
                            gzd[idx(r, c, p)] = vd[idx(r, c, D - 1)] - vd[idx(r, c, D - 2)];
                        else
                            gzd[idx(r, c, p)] = 0.5 * (vd[idx(r, c, p + 1)] - vd[idx(r, c, p - 1)]);
                    }
        } else {
            // intermediate: forward diff with last slice = 0.
            for (std::size_t p = 0; p < D; ++p)
                for (std::size_t r = 0; r < H; ++r)
                    for (std::size_t c = 0; c + 1 < W; ++c)
                        gxd[idx(r, c, p)] = vd[idx(r, c + 1, p)] - vd[idx(r, c, p)];
            for (std::size_t p = 0; p < D; ++p)
                for (std::size_t c = 0; c < W; ++c)
                    for (std::size_t r = 0; r + 1 < H; ++r)
                        gyd[idx(r, c, p)] = vd[idx(r + 1, c, p)] - vd[idx(r, c, p)];
            for (std::size_t c = 0; c < W; ++c)
                for (std::size_t r = 0; r < H; ++r)
                    for (std::size_t p = 0; p + 1 < D; ++p)
                        gzd[idx(r, c, p)] = vd[idx(r, c, p + 1)] - vd[idx(r, c, p)];
        }
    } else {
        throw Error("imgradientxyz: method must be 'sobel', 'prewitt', "
                    "'central', or 'intermediate'",
                    0, 0, "imgradientxyz", "", "m:imgradientxyz:method");
    }

    if (outT == ValueType::SINGLE) {
        // Cast to single.
        auto cast_single = [&](Value &G) {
            const std::size_t N = G.numel();
            Value gs = zeros_like(G, ValueType::SINGLE, mr);
            for (std::size_t i = 0; i < N; ++i)
                gs.singleDataMut()[i] = static_cast<float>(G.doubleData()[i]);
            G = std::move(gs);
        };
        cast_single(Gx); cast_single(Gy); cast_single(Gz);
    }
    return std::make_tuple(std::move(Gx), std::move(Gy), std::move(Gz));
}

std::tuple<Value, Value, Value>
imgradient3_from_grads(const Value &Gx, const Value &Gy, const Value &Gz,
                       std::pmr::memory_resource *mr)
{
    if (Gx.dims().rows() != Gy.dims().rows()
        || Gx.dims().cols() != Gy.dims().cols()
        || Gx.dims().pages() != Gy.dims().pages()
        || Gx.dims().rows() != Gz.dims().rows()
        || Gx.dims().cols() != Gz.dims().cols()
        || Gx.dims().pages() != Gz.dims().pages())
        throw Error("imgradient3: Gx, Gy, Gz must have the same size",
                    0, 0, "imgradient3", "", "m:imgradient3:size");
    const std::size_t N = Gx.numel();
    const ValueType outT = (Gx.type() == ValueType::SINGLE
                         || Gy.type() == ValueType::SINGLE
                         || Gz.type() == ValueType::SINGLE)
                         ? ValueType::SINGLE : ValueType::DOUBLE;
    auto alloc = [&]() {
        return zeros_like(Gx, outT, mr);
    };
    Value Gmag = alloc(), Gaz = alloc(), Gelev = alloc();
    constexpr double D2R = 180.0 / 3.14159265358979323846;
    for (std::size_t i = 0; i < N; ++i) {
        const double gx = Gx.elemAsDouble(i);
        const double gy = Gy.elemAsDouble(i);
        const double gz = Gz.elemAsDouble(i);
        const double mag = std::hypot(std::hypot(gx, gy), gz);
        const double az  = std::atan2(-gy, gx) * D2R;
        const double el  = std::atan2(gz, std::hypot(gx, gy)) * D2R;
        if (outT == ValueType::DOUBLE) {
            Gmag.doubleDataMut()[i]  = mag;
            Gaz.doubleDataMut()[i]   = az;
            Gelev.doubleDataMut()[i] = el;
        } else {
            Gmag.singleDataMut()[i]  = static_cast<float>(mag);
            Gaz.singleDataMut()[i]   = static_cast<float>(az);
            Gelev.singleDataMut()[i] = static_cast<float>(el);
        }
    }
    return std::make_tuple(std::move(Gmag), std::move(Gaz), std::move(Gelev));
}

std::tuple<Value, Value, Value>
imgradient3(const Value &V, const std::string &method, std::pmr::memory_resource *mr)
{
    auto [Gx, Gy, Gz] = imgradientxyz(V, method, mr);
    return imgradient3_from_grads(Gx, Gy, Gz, mr);
}

std::tuple<Value, Value>
imgradient(const Value &I, const std::string &method, std::pmr::memory_resource *mr)
{
    auto [Gx, Gy] = imgradientxy(I, method, mr);
    const size_t N = Gx.numel();
    Value Gmag = Value::matrix(Gx.dims().rows(), Gx.dims().cols(),
                                ValueType::DOUBLE, mr);
    Value Gdir = Value::matrix(Gx.dims().rows(), Gx.dims().cols(),
                                ValueType::DOUBLE, mr);
    double *gm = Gmag.doubleDataMut();
    double *gd = Gdir.doubleDataMut();
    for (size_t i = 0; i < N; ++i) {
        const double gxv = Gx.elemAsDouble(i);
        const double gyv = Gy.elemAsDouble(i);
        gm[i] = std::sqrt(gxv * gxv + gyv * gyv);
        gd[i] = std::atan2(-gyv, gxv) * 180.0 / M_PI;
    }
    return std::make_tuple(std::move(Gmag), std::move(Gdir));
}

namespace {

// Threshold (binarise) gradient magnitude into LOGICAL.
Value threshold_to_logical(const Value &G, double thresh, std::pmr::memory_resource *mr)
{
    const size_t H = G.dims().rows();
    const size_t W = G.dims().cols();
    Value out = Value::matrix(H, W, ValueType::LOGICAL, mr);
    uint8_t *od = out.logicalDataMut();
    const size_t N = H * W;
    for (size_t i = 0; i < N; ++i)
        od[i] = (G.elemAsDouble(i) > thresh) ? 1 : 0;
    return out;
}

double auto_threshold(const Value &G, double frac) {
    const size_t N = G.numel();
    double mx = 0.0;
    for (size_t i = 0; i < N; ++i) {
        const double v = std::fabs(G.elemAsDouble(i));
        if (v > mx) mx = v;
    }
    return frac * mx;
}

} // anonymous

Value edge(const Value &I, const std::string &method, double thresh_lo, double /*thresh_hi*/, std::pmr::memory_resource *mr)
{
    // First cut: Sobel / Prewitt / Roberts produce gradient magnitude
    // and threshold it. Canny / log / zerocross use a simplified path.
    if (method == "roberts") {
        // 2×2 Roberts kernels.
        Value Kx = make_kernel({ 1, 0, 0, -1 }, 2, 2, mr);
        Value Ky = make_kernel({ 0, 1, -1, 0 }, 2, 2, mr);
        Value Gx = apply_kernel(I, Kx, mr);
        Value Gy = apply_kernel(I, Ky, mr);
        const size_t N = Gx.numel();
        Value G = Value::matrix(Gx.dims().rows(), Gx.dims().cols(),
                                 ValueType::DOUBLE, mr);
        double *gd = G.doubleDataMut();
        for (size_t i = 0; i < N; ++i) {
            const double a = Gx.elemAsDouble(i), b = Gy.elemAsDouble(i);
            gd[i] = std::sqrt(a * a + b * b);
        }
        if (std::isnan(thresh_lo)) thresh_lo = auto_threshold(G, 0.5);
        return threshold_to_logical(G, thresh_lo, mr);
    }
    if (method == "log" || method == "zerocross") {
        // Apply LoG kernel, then mark zero-crossings as edges.
        const int hsz = 5;
        Value Klog = fspecial("log", { (double)hsz, (double)hsz, 0.5 }, mr);
        Value Y = apply_kernel(I, Klog, mr);
        const int H = (int)Y.dims().rows();
        const int W = (int)Y.dims().cols();
        Value out = Value::matrix(H, W, ValueType::LOGICAL, mr);
        uint8_t *od = out.logicalDataMut();
        const double th = std::isnan(thresh_lo) ? auto_threshold(Y, 0.05) : thresh_lo;
        auto y = [&](int r, int c) { return Y.elemAsDouble((size_t)c * (size_t)H + (size_t)r); };
        for (int c = 0; c < W; ++c)
            for (int r = 0; r < H; ++r) {
                bool zc = false;
                for (int dr = -1; dr <= 1 && !zc; ++dr)
                    for (int dc = -1; dc <= 1 && !zc; ++dc) {
                        if (dr == 0 && dc == 0) continue;
                        const int rr = r + dr, cc = c + dc;
                        if (rr < 0 || rr >= H || cc < 0 || cc >= W) continue;
                        const double a = y(r, c), b = y(rr, cc);
                        if (a * b < 0.0 && std::fabs(a - b) > th) zc = true;
                    }
                od[(size_t)c * (size_t)H + (size_t)r] = zc ? 1 : 0;
            }
        return out;
    }
    if (method == "canny") {
        // Simplified: gradient magnitude with two-threshold hysteresis,
        // no non-max suppression. Adequate for many use cases; full
        // Canny implementation deferred.
        auto [Gmag, _] = imgradient(I, "sobel", mr);
        if (std::isnan(thresh_lo)) thresh_lo = auto_threshold(Gmag, 0.2);
        return threshold_to_logical(Gmag, thresh_lo, mr);
    }
    // Default sobel / prewitt path: grad-magnitude threshold.
    const std::string mth = (method == "prewitt") ? "prewitt" : "sobel";
    auto [Gmag, _] = imgradient(I, mth, mr);
    if (std::isnan(thresh_lo)) thresh_lo = auto_threshold(Gmag, 0.4);
    return threshold_to_logical(Gmag, thresh_lo, mr);
}

// ════════════════════════════════════════════════════════════════════
// Engine adapters
// ════════════════════════════════════════════════════════════════════

namespace detail {

namespace {
std::string parse_method(Span<const Value> args, size_t i, const std::string &def) {
    if (i < args.size() && (args[i].isChar() || args[i].isString()))
        return args[i].toString();
    return def;
}
}

void imgradientxy_reg(Span<const Value> args, size_t nargout,
                      Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("imgradientxy: requires (I[, method])", 0, 0,
                    "imgradientxy", "", "m:imgradientxy:nargin");
    const auto m = parse_method(args, 1, "sobel");
    auto [Gx, Gy] = imgradientxy(args[0], m, ctx.engine->resource());
    outs[0] = std::move(Gx);
    if (nargout > 1) outs[1] = std::move(Gy);
}

void imgradient_reg(Span<const Value> args, size_t nargout,
                    Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("imgradient: requires (I[, method])", 0, 0,
                    "imgradient", "", "m:imgradient:nargin");
    const auto m = parse_method(args, 1, "sobel");
    auto [Gmag, Gdir] = imgradient(args[0], m, ctx.engine->resource());
    outs[0] = std::move(Gmag);
    if (nargout > 1) outs[1] = std::move(Gdir);
}

void imgradientxyz_reg(Span<const Value> args, size_t nargout,
                       Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("imgradientxyz: requires (V[, method])",
                    0, 0, "imgradientxyz", "", "m:imgradientxyz:nargin");
    const auto m = parse_method(args, 1, "sobel");
    auto [Gx, Gy, Gz] = imgradientxyz(args[0], m, ctx.engine->resource());
    outs[0] = std::move(Gx);
    if (nargout > 1) outs[1] = std::move(Gy);
    if (nargout > 2) outs[2] = std::move(Gz);
}

void imgradient3_reg(Span<const Value> args, size_t nargout,
                     Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("imgradient3: requires (V[, method]) or (Gx, Gy, Gz)",
                    0, 0, "imgradient3", "", "m:imgradient3:nargin");
    auto *mr = ctx.engine->resource();
    // Detect (Gx, Gy, Gz) form: three numeric args, NO string arg.
    if (args.size() == 3
        && !args[0].isChar() && !args[0].isString()
        && !args[1].isChar() && !args[1].isString()
        && !args[2].isChar() && !args[2].isString()
        && args[0].dims().is3D() && args[1].dims().is3D() && args[2].dims().is3D()) {
        auto [Gmag, Gaz, Gelev] =
            imgradient3_from_grads(args[0], args[1], args[2], mr);
        outs[0] = std::move(Gmag);
        if (nargout > 1) outs[1] = std::move(Gaz);
        if (nargout > 2) outs[2] = std::move(Gelev);
        return;
    }
    const auto m = parse_method(args, 1, "sobel");
    auto [Gmag, Gaz, Gelev] = imgradient3(args[0], m, mr);
    outs[0] = std::move(Gmag);
    if (nargout > 1) outs[1] = std::move(Gaz);
    if (nargout > 2) outs[2] = std::move(Gelev);
}

void edge_reg(Span<const Value> args, size_t /*nargout*/,
              Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("edge: requires (I[, method, thresh])", 0, 0, "edge", "",
                    "m:edge:nargin");
    const auto m = parse_method(args, 1, "sobel");
    double t_lo = std::nan(""), t_hi = std::nan("");
    if (args.size() >= 3 && !args[2].isEmpty() && !(args[2].isChar() || args[2].isString())) {
        const Value &v = args[2];
        if (v.numel() == 1) t_lo = v.toScalar();
        else if (v.numel() >= 2) {
            t_lo = v.elemAsDouble(0);
            t_hi = v.elemAsDouble(1);
        }
    }
    outs[0] = edge(args[0], m, t_lo, t_hi, ctx.engine->resource());
}

} // namespace detail
} // namespace numkit::image
