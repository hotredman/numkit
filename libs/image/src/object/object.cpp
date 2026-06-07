// libs/image/src/object/object.cpp
//
// Gradient + edge detection. For first cut: Sobel / Prewitt / Roberts.
// Canny / log / zerocross share the simpler-Sobel path with hysteresis
// thresholding; full Canny non-max suppression deferred.

#include <numkit/image/object/object.hpp>

#include <numkit/image/filter/filter.hpp>
#include <numkit/image/type_convert/type_convert.hpp>

#include <numkit/value/value.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/error.hpp>

#include <algorithm>
#include <tuple>
#include <array>
#include <cctype>
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
    // Kernel sign convention matches MATLAB imgradientxy: Gx is positive
    // where intensity increases left→right (negative on the left column,
    // positive on the right), Gy positive where intensity increases
    // top→bottom. (MATLAB uses hx = [-1 0 1; -2 0 2; -1 0 1] for sobel.)
    if (method == "prewitt") {
        kx = {-1, 0, 1,
              -1, 0, 1,
              -1, 0, 1 };
        ky = {-1,-1,-1,
               0, 0, 0,
               1, 1, 1 };
    } else if (method == "central") {
        // 1×3 / 3×1 central difference.
        kx = {-0.5, 0, 0.5}; rows = 1; cols = 3;
        ky = {-0.5, 0, 0.5};
    } else if (method == "intermediate") {
        kx = {-1, 1}; rows = 1; cols = 2;
        ky = {-1, 1};
    } else {  // sobel default
        kx = {-1, 0, 1,
              -2, 0, 2,
              -1, 0, 1 };
        ky = {-1,-2,-1,
               0, 0, 0,
               1, 2, 1 };
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
                    0, 0, "imgradientxyz", "", "numkit:imgradientxyz:rank");
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
                    0, 0, "imgradientxyz", "", "numkit:imgradientxyz:method");
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
                    0, 0, "imgradient3", "", "numkit:imgradient3:size");
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

// Magnitude-only variant — skips the per-pixel atan2 direction, which costs
// nothing when the caller wants only Gmag (the common `m = imgradient(I)`).
// Gmag is bit-identical to imgradient()'s first output.
Value imgradient_mag(const Value &I, const std::string &method, std::pmr::memory_resource *mr)
{
    auto [Gx, Gy] = imgradientxy(I, method, mr);
    const size_t N = Gx.numel();
    Value Gmag = Value::matrix(Gx.dims().rows(), Gx.dims().cols(), ValueType::DOUBLE, mr);
    double *gm = Gmag.doubleDataMut();
    for (size_t i = 0; i < N; ++i) {
        const double gxv = Gx.elemAsDouble(i), gyv = Gy.elemAsDouble(i);
        gm[i] = std::sqrt(gxv * gxv + gyv * gyv);
    }
    return Gmag;
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

// Proper Canny edge detector: Gaussian smoothing → Sobel gradient →
// non-maximum suppression (thins ridges to 1 px) → double-threshold
// hysteresis. `userHigh` is the MATLAB-style HIGH threshold as a fraction
// of the peak gradient magnitude (low = 0.4·high); NaN → an auto fraction.
Value canny_edge(const Value &Iin, double userHigh, std::pmr::memory_resource *mr)
{
    const int H = static_cast<int>(Iin.dims().rows());
    const int W = static_cast<int>(Iin.dims().cols());
    const size_t N = static_cast<size_t>(H) * static_cast<size_t>(W);

    Value out = Value::matrix(H, W, ValueType::LOGICAL, mr);
    uint8_t *od = out.logicalDataMut();
    std::fill(od, od + N, uint8_t{0});
    if (N == 0) return out;

    // 1–2. Derivative-of-Gaussian gradient (MATLAB Canny approach, σ = √2):
    // smooth along one axis with a Gaussian g and differentiate along the
    // other with its derivative dg. Separable, so two 1-D passes per axis.
    // This avoids the extra smoothing that a Gaussian-blur-then-Sobel chain
    // would add (which weakens weak edges below threshold).
    const double sigma = std::sqrt(2.0);
    const int hw = std::max(1, static_cast<int>(std::ceil(3.0 * sigma)));
    const int L = 2 * hw + 1;
    std::vector<double> g(L), dg(L);
    double gsum = 0.0;
    for (int i = 0; i < L; ++i) {
        const double x = i - hw;
        g[i] = std::exp(-(x * x) / (2.0 * sigma * sigma));
        gsum += g[i];
    }
    for (int i = 0; i < L; ++i) g[i] /= gsum;
    for (int i = 0; i < L; ++i) {
        const double x = i - hw;
        dg[i] = -x * std::exp(-(x * x) / (2.0 * sigma * sigma));  // scale irrelevant (relative threshold)
    }
    // Filter in DOUBLE. imfilter on an integer image clamps each pass to the
    // integer range — which would zero every negative derivative response
    // (i.e. every falling edge), losing ~half the edges. MATLAB likewise
    // converts to floating point before the Canny gradient.
    Value Id = Value::matrix(H, W, ValueType::DOUBLE, mr);
    {
        double *dd = Id.doubleDataMut();
        for (size_t i = 0; i < N; ++i) dd[i] = Iin.elemAsDouble(i);
    }

    Value gRow  = make_kernel(g,  1, L, mr);
    Value gCol  = make_kernel(g,  L, 1, mr);
    Value dgRow = make_kernel(dg, 1, L, mr);
    Value dgCol = make_kernel(dg, L, 1, mr);
    Value GxV = apply_kernel(apply_kernel(Id, gCol, mr), dgRow, mr);
    Value GyV = apply_kernel(apply_kernel(Id, gRow, mr), dgCol, mr);

    ScratchArena arena(mr);
    ScratchVec<double> Gx(N, &arena), Gy(N, &arena), mag(N, &arena);
    double maxMag = 0.0;
    for (size_t i = 0; i < N; ++i) {
        const double a = GxV.elemAsDouble(i), b = GyV.elemAsDouble(i);
        Gx[i] = a; Gy[i] = b;
        const double m = std::sqrt(a * a + b * b);
        mag[i] = m;
        if (m > maxMag) maxMag = m;
    }
    if (maxMag <= 0.0) return out;

    auto at = [&](int r, int c) -> double {
        return mag[static_cast<size_t>(c) * static_cast<size_t>(H) + static_cast<size_t>(r)];
    };

    // 3. Non-maximum suppression with sub-pixel interpolation along the
    // gradient direction (as MATLAB does). Interpolating the magnitude
    // between the two straddling neighbours yields better-connected 1-px
    // ridges than 4-way quantisation, which matters for closed-contour use
    // (imfill). Border pixels can't be interpolated → left as non-edges.
    ScratchVec<double> nms(N, &arena);
    std::fill(nms.begin(), nms.end(), 0.0);
    for (int c = 1; c < W - 1; ++c)
        for (int r = 1; r < H - 1; ++r) {
            const size_t i = static_cast<size_t>(c) * static_cast<size_t>(H) + static_cast<size_t>(r);
            const double m = mag[i];
            if (m <= 0.0) continue;
            const double gx = Gx[i], gy = Gy[i];
            const double agx = std::fabs(gx), agy = std::fabs(gy);
            double mf, mb, w;
            if (agx >= agy) {                        // gradient more horizontal
                w = (agx > 0.0) ? agy / agx : 0.0;
                if (gx * gy >= 0.0) {                // ↘ / ↖
                    mf = (1.0 - w) * at(r, c + 1) + w * at(r + 1, c + 1);
                    mb = (1.0 - w) * at(r, c - 1) + w * at(r - 1, c - 1);
                } else {                             // ↗ / ↙
                    mf = (1.0 - w) * at(r, c + 1) + w * at(r - 1, c + 1);
                    mb = (1.0 - w) * at(r, c - 1) + w * at(r + 1, c - 1);
                }
            } else {                                 // gradient more vertical
                w = (agy > 0.0) ? agx / agy : 0.0;
                if (gx * gy >= 0.0) {
                    mf = (1.0 - w) * at(r + 1, c) + w * at(r + 1, c + 1);
                    mb = (1.0 - w) * at(r - 1, c) + w * at(r - 1, c - 1);
                } else {
                    mf = (1.0 - w) * at(r + 1, c) + w * at(r + 1, c - 1);
                    mb = (1.0 - w) * at(r - 1, c) + w * at(r - 1, c + 1);
                }
            }
            if (m >= mf && m >= mb) nms[i] = m;
        }

    // 4. Thresholds, relative to the peak gradient magnitude.
    const double highFrac = (std::isnan(userHigh) || userHigh <= 0.0) ? 0.2 : userHigh;
    const double highT = highFrac * maxMag;
    const double lowT  = 0.4 * highT;

    // 5. Hysteresis: strong pixels (≥ highT) seed a flood that keeps any
    // weak pixel (≥ lowT) reachable through 8-connectivity.
    ScratchVec<size_t> stack(0, &arena);
    stack.reserve(N / 8 + 1);
    for (size_t i = 0; i < N; ++i)
        if (nms[i] >= highT) { od[i] = 1; stack.push_back(i); }
    static constexpr int dr8[8] = { -1, -1, -1, 0, 0, 1, 1, 1 };
    static constexpr int dc8[8] = { -1, 0, 1, -1, 1, -1, 0, 1 };
    while (!stack.empty()) {
        const size_t p = stack.back(); stack.pop_back();
        const int r = static_cast<int>(p % static_cast<size_t>(H));
        const int c = static_cast<int>(p / static_cast<size_t>(H));
        for (int k = 0; k < 8; ++k) {
            const int nr = r + dr8[k], nc = c + dc8[k];
            if (nr < 0 || nr >= H || nc < 0 || nc >= W) continue;
            const size_t q = static_cast<size_t>(nc) * static_cast<size_t>(H) + static_cast<size_t>(nr);
            if (!od[q] && nms[q] >= lowT) { od[q] = 1; stack.push_back(q); }
        }
    }
    return out;
}

} // anonymous

Value edge(const Value &I, const std::string &methodRaw, double thresh_lo, double /*thresh_hi*/, std::pmr::memory_resource *mr)
{
    // Method names are case-insensitive (MATLAB-compatible).
    std::string method = methodRaw;
    std::transform(method.begin(), method.end(), method.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });

    // Sobel / Prewitt / Roberts threshold the gradient magnitude; Canny does
    // full non-max suppression + hysteresis; log / zerocross mark zero
    // crossings.
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
        // Full Canny: Gaussian smooth → Sobel gradient → non-max
        // suppression → double-threshold hysteresis. `thresh_lo` carries
        // the user's scalar threshold, which MATLAB treats as the HIGH
        // threshold (a fraction of the peak gradient magnitude); NaN → auto.
        return canny_edge(I, thresh_lo, mr);
    }
    // Default sobel / prewitt path: grad-magnitude threshold.
    const std::string mth = (method == "prewitt") ? "prewitt" : "sobel";
    auto [Gmag, _] = imgradient(I, mth, mr);
    if (std::isnan(thresh_lo)) thresh_lo = auto_threshold(Gmag, 0.4);
    return threshold_to_logical(Gmag, thresh_lo, mr);
}

// ── cornermetric (Harris / Shi-Tomasi) ────────────────────────────
//
// MATLAB R2025b cornermetric.m algorithm (transliterated):
//
//   Convert I → double via im2double.
//   Dx = imfilter(I, [-1 0 1],  'replicate', 'conv')
//   Dy = imfilter(I, [-1 0 1]', 'replicate', 'conv')
//   Trim 1px border on Dx, Dy (i.e. take rows 2..end-1, cols 2..end-1).
//   A = Dx², B = Dy², C = Dx·Dy.
//   W = filter_coef * filter_coef'      (outer-product 2-D kernel)
//   A,B,C ← imfilter(., W, 'replicate', 'full', 'conv').
//   removed = (len(filter_coef) - 1) / 2 - 1; crop back to image size.
//   Harris:           C* = A·B - C² - k·(A+B)²
//   MinimumEigenvalue: C* = ((A+B) - sqrt((A-B)² + 4·C²)) / 2.
//
// References:
//   Harris & Stephens, "A Combined Corner and Edge Detector", 1988.
//   Shi & Tomasi,     "Good Features to Track",                1994.
Value cornermetric(const Value &I, const std::string &method,
                   double sensitivity_factor,
                   const Value &filter_coef,
                   std::pmr::memory_resource *mr)
{
    if (I.dims().is3D())
        throw Error("cornermetric: I must be 2-D",
                    0, 0, "cornermetric", "", "numkit:cornermetric:dim");
    if (method != "Harris" && method != "MinimumEigenvalue")
        throw Error("cornermetric: METHOD must be 'Harris' or "
                    "'MinimumEigenvalue'",
                    0, 0, "cornermetric", "", "numkit:cornermetric:method");
    if (method == "Harris"
        && (!(sensitivity_factor > 0.0) || !(sensitivity_factor < 0.25)))
        throw Error("cornermetric: SensitivityFactor must be in (0, 0.25)",
                    0, 0, "cornermetric", "", "numkit:cornermetric:k");

    // Resolve filter coef vector.
    std::pmr::vector<double> fcoef(mr);
    if (filter_coef.numel() == 0) {
        // Default fspecial('gaussian', [5 1], 1.5).
        // Hardcoded exact values: g = exp(-x²/(2σ²)), normalised.
        const double raw[5] = {
            0.12007838770739, 0.23388075658030,
            0.29208171142462,
            0.23388075658030, 0.12007838770739};
        fcoef.assign(raw, raw + 5);
    } else {
        if (filter_coef.numel() < 3 || (filter_coef.numel() % 2) == 0)
            throw Error("cornermetric: FilterCoefficients length must "
                        "be odd and ≥ 3",
                        0, 0, "cornermetric", "",
                        "numkit:cornermetric:filter");
        fcoef.reserve(filter_coef.numel());
        for (std::size_t i = 0; i < filter_coef.numel(); ++i)
            fcoef.push_back(filter_coef.elemAsDouble(i));
    }
    const std::size_t Lc = fcoef.size();
    const std::size_t halfLc = Lc / 2;       // (Lc-1)/2
    if (halfLc < 1)
        throw Error("cornermetric: filter coef too short",
                    0, 0, "cornermetric", "", "numkit:cornermetric:filter");
    const std::size_t removed = halfLc - 1;  // matches MATLAB

    // Promote I → DOUBLE via im2double.
    Value Id = im2double(I, mr);
    const std::size_t H = Id.dims().rows();
    const std::size_t W = Id.dims().cols();
    if (H < 3 || W < 3)
        throw Error("cornermetric: image too small (need ≥ 3x3)",
                    0, 0, "cornermetric", "", "numkit:cornermetric:tooSmall");

    // Build [-1 0 1] row + column kernels.
    Value hx = Value::matrix(1, 3, ValueType::DOUBLE, mr);
    hx.doubleDataMut()[0] = -1; hx.doubleDataMut()[1] = 0; hx.doubleDataMut()[2] = 1;
    Value hy = Value::matrix(3, 1, ValueType::DOUBLE, mr);
    hy.doubleDataMut()[0] = -1; hy.doubleDataMut()[1] = 0; hy.doubleDataMut()[2] = 1;

    Value Dx = imfilter(Id, hx, PadMode::Replicate, 0.0,
                        /*full=*/false, /*flip_kernel=*/true, mr);
    Value Dy = imfilter(Id, hy, PadMode::Replicate, 0.0,
                        /*full=*/false, /*flip_kernel=*/true, mr);

    // Trim 1px border: take rows 1..H-2, cols 1..W-2 (0-indexed).
    const std::size_t Ht = H - 2;
    const std::size_t Wt = W - 2;
    if (Ht == 0 || Wt == 0)
        throw Error("cornermetric: image too small after gradient trim",
                    0, 0, "cornermetric", "", "numkit:cornermetric:tooSmall");

    // Build A = Dx², B = Dy², C = Dx·Dy at trimmed shape.
    Value A = Value::matrix(Ht, Wt, ValueType::DOUBLE, mr);
    Value B = Value::matrix(Ht, Wt, ValueType::DOUBLE, mr);
    Value Cmat = Value::matrix(Ht, Wt, ValueType::DOUBLE, mr);
    for (std::size_t c = 0; c < Wt; ++c) {
        for (std::size_t r = 0; r < Ht; ++r) {
            const std::size_t src = (c + 1) * H + (r + 1);
            const double dx = Dx.doubleData()[src];
            const double dy = Dy.doubleData()[src];
            const std::size_t dst = c * Ht + r;
            A.doubleDataMut()[dst] = dx * dx;
            B.doubleDataMut()[dst] = dy * dy;
            Cmat.doubleDataMut()[dst] = dx * dy;
        }
    }

    // W = filter_coef * filter_coef' (Lc x Lc outer-product kernel).
    Value Wk = Value::matrix(Lc, Lc, ValueType::DOUBLE, mr);
    for (std::size_t c = 0; c < Lc; ++c)
        for (std::size_t r = 0; r < Lc; ++r)
            Wk.doubleDataMut()[c * Lc + r] = fcoef[r] * fcoef[c];

    // Filter A, B, C with 'full' shape under Replicate boundary —
    // MATLAB's `imfilter(.,W,'replicate','full','conv')` pads the
    // input by (Lc-1)/2 replicate on each side and then runs the
    // convolution at the larger size. numkit's `imfilter(full=true)`
    // uses zero-padding regardless of PadMode, so we pre-pad by
    // (Lc-1)/2 with `padarray` and run a 'same' convolution.
    const std::vector<int> pad{static_cast<int>(halfLc),
                               static_cast<int>(halfLc)};
    auto full_replicate = [&](Value X) {
        X = padarray(X, pad, PadMode::Replicate, 0.0, "both", mr);
        return imfilter(X, Wk, PadMode::Replicate, 0.0,
                        /*full=*/false, /*flip_kernel=*/true, mr);
    };
    A = full_replicate(A);
    B = full_replicate(B);
    Cmat = full_replicate(Cmat);

    // Crop back to (H, W) image size: rows [removed+1, end-removed].
    const std::size_t Ha = A.dims().rows();
    const std::size_t Wa = A.dims().cols();
    if (Ha < H || Wa < W)
        throw Error("cornermetric: internal shape error after filter",
                    0, 0, "cornermetric", "", "numkit:cornermetric:shape");
    auto crop_value = [&](const Value &X) {
        Value Y = Value::matrix(H, W, ValueType::DOUBLE, mr);
        for (std::size_t c = 0; c < W; ++c) {
            for (std::size_t r = 0; r < H; ++r) {
                const std::size_t sr = r + removed;
                const std::size_t sc = c + removed;
                Y.doubleDataMut()[c * H + r]
                    = X.doubleData()[sc * Ha + sr];
            }
        }
        return Y;
    };
    A = crop_value(A);
    B = crop_value(B);
    Cmat = crop_value(Cmat);

    // Compute cornerness.
    Value out = Value::matrix(H, W, ValueType::DOUBLE, mr);
    const double *ap = A.doubleData();
    const double *bp = B.doubleData();
    const double *cp = Cmat.doubleData();
    double *op = out.doubleDataMut();
    const std::size_t N = H * W;
    if (method == "Harris") {
        const double k = sensitivity_factor;
        for (std::size_t i = 0; i < N; ++i) {
            const double sum = ap[i] + bp[i];
            op[i] = ap[i] * bp[i] - cp[i] * cp[i] - k * sum * sum;
        }
    } else { // MinimumEigenvalue
        for (std::size_t i = 0; i < N; ++i) {
            const double sum = ap[i] + bp[i];
            const double diff = ap[i] - bp[i];
            op[i] = (sum - std::sqrt(diff * diff + 4.0 * cp[i] * cp[i])) * 0.5;
        }
    }
    return out;
}

// ── hough (Standard Hough Transform) ───────────────────────────────
//
// MATLAB R2025b hough.m algorithm:
//   M, N = size(BW); D = sqrt((M-1)² + (N-1)²)
//   q = ceil(D / rhoRes); nrho = 2*q + 1
//   rho = linspace(-q*rhoRes, q*rhoRes, nrho)
//   theta default = -90 : 1 : 89  (180 bins, [-90, 90))
//   H = zeros(nrho, ntheta)
//   for each true pixel (r, c)  // 1-indexed
//     x = c - 1, y = r - 1       // 0-indexed image coords
//     for each theta_k:
//       rho_val = x*cos(theta_k) + y*sin(theta_k)
//       bin = round(rho_val / rhoRes) + q + 1  // 1-indexed
//       H[bin, k] += 1
//
// Reference: Gonzalez, Woods & Eddins, "Digital Image Processing
// Using MATLAB", 2nd ed., Gatesmark, 2009.
void hough(const Value &BW, double rho_res,
           const Value &theta_deg,
           Value &H_out, Value &T_out, Value &R_out,
           std::pmr::memory_resource *mr)
{
    if (BW.dims().is3D())
        throw Error("hough: BW must be 2-D",
                    0, 0, "hough", "", "numkit:hough:dim");
    if (!(rho_res > 0.0) || !std::isfinite(rho_res))
        throw Error("hough: RhoResolution must be a positive scalar",
                    0, 0, "hough", "", "numkit:hough:rho");

    const std::size_t M = BW.dims().rows();
    const std::size_t N = BW.dims().cols();

    // Build theta grid (degrees → radians).
    std::pmr::vector<double> theta(mr);
    if (theta_deg.numel() == 0) {
        theta.reserve(180);
        for (int k = -90; k <= 89; ++k)
            theta.push_back(static_cast<double>(k));
    } else {
        theta.reserve(theta_deg.numel());
        for (std::size_t i = 0; i < theta_deg.numel(); ++i) {
            const double v = theta_deg.elemAsDouble(i);
            if (v < -90.0 || v >= 90.0)
                throw Error("hough: Theta values must lie in [-90, 90)",
                            0, 0, "hough", "", "numkit:hough:theta");
            theta.push_back(v);
        }
    }
    const std::size_t ntheta = theta.size();

    // ρ grid.
    const double D = std::sqrt(
        static_cast<double>(M - 1) * (M - 1)
      + static_cast<double>(N - 1) * (N - 1));
    const std::size_t q = (D == 0.0)
        ? 0
        : static_cast<std::size_t>(std::ceil(D / rho_res));
    const std::size_t nrho = 2 * q + 1;
    Value R = Value::matrix(1, nrho, ValueType::DOUBLE, mr);
    if (nrho == 1) {
        R.doubleDataMut()[0] = 0.0;
    } else {
        const double lo = -static_cast<double>(q) * rho_res;
        const double step = rho_res;
        for (std::size_t i = 0; i < nrho; ++i)
            R.doubleDataMut()[i] = lo + step * static_cast<double>(i);
    }

    // T echo.
    Value T = Value::matrix(1, ntheta, ValueType::DOUBLE, mr);
    for (std::size_t i = 0; i < ntheta; ++i)
        T.doubleDataMut()[i] = theta[i];

    // Pre-compute cos/sin of theta (in radians).
    constexpr double DEG2RAD = M_PI / 180.0;
    std::pmr::vector<double> cosT(ntheta, 0.0, mr);
    std::pmr::vector<double> sinT(ntheta, 0.0, mr);
    for (std::size_t k = 0; k < ntheta; ++k) {
        cosT[k] = std::cos(theta[k] * DEG2RAD);
        sinT[k] = std::sin(theta[k] * DEG2RAD);
    }

    // Accumulator (col-major: H(r, c) → c*nrho + r).
    Value H = Value::matrix(nrho, ntheta, ValueType::DOUBLE, mr);
    double *hp = H.doubleDataMut();

    // Iterate true pixels of BW (logical or numeric "non-zero").
    const bool islog = BW.isLogical();
    for (std::size_t c = 0; c < N; ++c) {
        for (std::size_t r = 0; r < M; ++r) {
            const std::size_t k = c * M + r;
            const bool on = islog
                ? (BW.logicalData()[k] != 0)
                : (BW.elemAsDouble(k) != 0.0);
            if (!on) continue;
            const double x = static_cast<double>(c);  // 0-indexed
            const double y = static_cast<double>(r);
            for (std::size_t tk = 0; tk < ntheta; ++tk) {
                const double rho_val = x * cosT[tk] + y * sinT[tk];
                // bin = round(rho_val / rho_res) + q + 1 (1-indexed)
                long bin = static_cast<long>(std::lround(rho_val / rho_res))
                         + static_cast<long>(q);  // 0-indexed
                if (bin < 0 || static_cast<std::size_t>(bin) >= nrho) continue;
                hp[tk * nrho + bin] += 1.0;
            }
        }
    }

    H_out = std::move(H);
    T_out = std::move(T);
    R_out = std::move(R);
}

// ── houghpeaks (peak extraction from Hough accumulator) ───────────
Value houghpeaks(const Value &H, std::size_t numpeaks,
                 double threshold,
                 std::size_t nhoodRho, std::size_t nhoodTheta,
                 const Value &theta_deg,
                 std::pmr::memory_resource *mr)
{
    if (H.dims().is3D())
        throw Error("houghpeaks: H must be 2-D",
                    0, 0, "houghpeaks", "", "numkit:houghpeaks:dim");
    const std::size_t nrho = H.dims().rows();
    const std::size_t ntheta = H.dims().cols();
    const std::size_t N = nrho * ntheta;

    // Default neighbourhood: ceil(size(H)/50), bumped up to next odd, min 1.
    auto next_odd = [](std::size_t v) -> std::size_t {
        if (v < 1) v = 1;
        if ((v % 2) == 0) v += 1;
        return v;
    };
    if (nhoodRho == 0)   nhoodRho   = next_odd((nrho + 49) / 50);
    if (nhoodTheta == 0) nhoodTheta = next_odd((ntheta + 49) / 50);
    if ((nhoodRho   % 2) == 0) ++nhoodRho;
    if ((nhoodTheta % 2) == 0) ++nhoodTheta;

    // Default threshold: 0.5 * max(H(:)).
    double mxH = 0.0;
    for (std::size_t i = 0; i < N; ++i) {
        const double v = H.elemAsDouble(i);
        if (v > mxH) mxH = v;
    }
    if (threshold < 0.0) threshold = 0.5 * mxH;

    // Detect antisymmetric theta range.
    bool isThetaAntisym = false;
    if (theta_deg.numel() >= 2) {
        const std::size_t nT = theta_deg.numel();
        double minT = theta_deg.elemAsDouble(0);
        double maxT = theta_deg.elemAsDouble(0);
        for (std::size_t i = 1; i < nT; ++i) {
            const double v = theta_deg.elemAsDouble(i);
            if (v < minT) minT = v;
            if (v > maxT) maxT = v;
        }
        const double thetaRes
            = std::fabs(maxT - minT) / static_cast<double>(nT - 1);
        isThetaAntisym
            = std::fabs(minT + thetaRes * nhoodTheta) <= maxT;
    } else if (theta_deg.numel() == 0) {
        // Default -90:1:89: minT=-90, maxT=89, thetaRes=1.
        isThetaAntisym = std::fabs(-90.0 + 1.0 * nhoodTheta) <= 89.0;
    }

    // Working copy of H (DOUBLE).
    std::pmr::vector<double> Hwork(N, 0.0, mr);
    for (std::size_t i = 0; i < N; ++i) Hwork[i] = H.elemAsDouble(i);

    const std::size_t halfR = nhoodRho / 2;
    const std::size_t halfT = nhoodTheta / 2;

    std::pmr::vector<std::size_t> peak_r(mr), peak_c(mr);
    peak_r.reserve(numpeaks);
    peak_c.reserve(numpeaks);

    while (peak_r.size() < numpeaks) {
        // Find global max.
        std::size_t maxIdx = 0;
        double maxV = Hwork[0];
        for (std::size_t i = 1; i < N; ++i) {
            if (Hwork[i] > maxV) { maxV = Hwork[i]; maxIdx = i; }
        }
        if (maxV < threshold) break;
        const std::size_t p = maxIdx % nrho;  // 0-indexed rho
        const std::size_t q = maxIdx / nrho;  // 0-indexed theta
        peak_r.push_back(p);
        peak_c.push_back(q);

        // Suppress nhood.
        const long p1 = static_cast<long>(p) - static_cast<long>(halfR);
        const long p2 = static_cast<long>(p) + static_cast<long>(halfR);
        const long q1 = static_cast<long>(q) - static_cast<long>(halfT);
        const long q2 = static_cast<long>(q) + static_cast<long>(halfT);
        for (long qq = q1; qq <= q2; ++qq) {
            for (long pp = p1; pp <= p2; ++pp) {
                // Out-of-bounds in rho: drop.
                if (pp < 0 || pp >= static_cast<long>(nrho)) continue;
                long qWrap = qq;
                long pWrap = pp;
                if (qWrap < 0 || qWrap >= static_cast<long>(ntheta)) {
                    if (isThetaAntisym) {
                        if (qWrap < 0) {
                            qWrap += static_cast<long>(ntheta);
                            pWrap = static_cast<long>(nrho) - 1 - pp;
                        } else {
                            qWrap -= static_cast<long>(ntheta);
                            pWrap = static_cast<long>(nrho) - 1 - pp;
                        }
                        if (pWrap < 0 || pWrap >= static_cast<long>(nrho))
                            continue;
                    } else {
                        continue;
                    }
                }
                Hwork[static_cast<std::size_t>(qWrap) * nrho
                    + static_cast<std::size_t>(pWrap)] = 0.0;
            }
        }
    }

    // Pack output (P × 2): each row [rho_idx, theta_idx], 1-indexed.
    const std::size_t nP = peak_r.size();
    Value P = Value::matrix(nP, 2, ValueType::DOUBLE, mr);
    for (std::size_t i = 0; i < nP; ++i) {
        P.doubleDataMut()[i]      = static_cast<double>(peak_r[i] + 1);
        P.doubleDataMut()[nP + i] = static_cast<double>(peak_c[i] + 1);
    }
    return P;
}

// ── houghlines (line segment extraction from Hough peaks) ────────
//
// MATLAB R2025b houghlines.m algorithm:
//   nonzeropix = [x y]; with x=col-1, y=row-1 from find(BW).
//   For each peak (rbin, cbin):
//     theta_c = theta(cbin) * pi/180
//     rho_xy  = x*cos(theta_c) + y*sin(theta_c)
//     slope = (nrho - 1) / (rho(end) - rho(1))
//     rho_bin_index = round(slope * (rho_xy - rho(1)) + 1)
//     idx = where rho_bin_index == rbin
//     r = y(idx) + 1, c = x(idx) + 1  (back to 1-based pixel coords)
//     resort pixels along the dominant axis (row vs col range)
//     xy = [c r]
//     diff_sq = sum((xy(i+1,:) - xy(i,:))^2, 2)
//     fillgap_idx = where diff_sq > fillgap^2
//     split xy at gaps; for each segment [p1, p2]:
//       if ||p2 - p1||^2 >= minlength^2:
//         add line struct {point1=p1, point2=p2, theta, rho}
//
// Reference: Gonzalez/Woods/Eddins, *Digital Image Processing
// Using MATLAB*, Prentice Hall 2003.
Value houghlines(const Value &BW, const Value &theta_deg,
                 const Value &rho, const Value &peaks,
                 double fillgap, double minlength,
                 std::pmr::memory_resource *mr)
{
    if (BW.dims().is3D())
        throw Error("houghlines: BW must be 2-D",
                    0, 0, "houghlines", "", "numkit:houghlines:dim");
    if (!(fillgap > 0.0) || !std::isfinite(fillgap))
        throw Error("houghlines: FillGap must be positive",
                    0, 0, "houghlines", "", "numkit:houghlines:fillgap");
    if (!(minlength > 0.0) || !std::isfinite(minlength))
        throw Error("houghlines: MinLength must be positive",
                    0, 0, "houghlines", "", "numkit:houghlines:minlength");

    const std::size_t M = BW.dims().rows();
    const std::size_t N = BW.dims().cols();
    const std::size_t nrho = rho.numel();
    const std::size_t ntheta = theta_deg.numel();
    if (nrho < 2 || ntheta == 0)
        throw Error("houghlines: rho/theta vectors too small",
                    0, 0, "houghlines", "", "numkit:houghlines:vec");
    if (peaks.dims().cols() != 2 || peaks.dims().is3D())
        throw Error("houghlines: PEAKS must be P × 2",
                    0, 0, "houghlines", "", "numkit:houghlines:peaks");
    const std::size_t nPeaks = peaks.dims().rows();

    // Gather nonzero pixel (x, y) = (col-1, row-1).
    const bool islog = BW.isLogical();
    std::pmr::vector<double> px(mr), py(mr);
    px.reserve(M * N / 16);
    py.reserve(M * N / 16);
    // Walk column-major to match MATLAB's find() order.
    for (std::size_t c = 0; c < N; ++c) {
        for (std::size_t r = 0; r < M; ++r) {
            const std::size_t k = c * M + r;
            const bool on = islog
                ? (BW.logicalData()[k] != 0)
                : (BW.elemAsDouble(k) != 0.0);
            if (!on) continue;
            px.push_back(static_cast<double>(c));
            py.push_back(static_cast<double>(r));
        }
    }
    const std::size_t nP = px.size();

    constexpr double DEG2RAD = M_PI / 180.0;
    const double rho_first = rho.elemAsDouble(0);
    const double rho_last  = rho.elemAsDouble(nrho - 1);
    const double slope     = static_cast<double>(nrho - 1)
                           / (rho_last - rho_first);
    const double fillgap_sq   = fillgap   * fillgap;
    const double minlength_sq = minlength * minlength;

    // Collect line segments.
    struct Seg {
        double p1x, p1y, p2x, p2y;
        double theta, rho;
    };
    std::pmr::vector<Seg> segs(mr);

    std::pmr::vector<std::size_t> idx(mr);
    std::pmr::vector<std::size_t> rseg(mr), cseg(mr);
    idx.reserve(nP / 4);
    rseg.reserve(nP / 4);
    cseg.reserve(nP / 4);

    for (std::size_t k = 0; k < nPeaks; ++k) {
        const long rbin = static_cast<long>(peaks.elemAsDouble(k));
        const long cbin = static_cast<long>(peaks.elemAsDouble(nPeaks + k));
        if (cbin < 1 || cbin > static_cast<long>(ntheta)) continue;
        const double theta_c = theta_deg.elemAsDouble(cbin - 1) * DEG2RAD;
        const double cosT = std::cos(theta_c);
        const double sinT = std::sin(theta_c);

        // Pick pixels whose rho-bin matches.
        idx.clear();
        for (std::size_t i = 0; i < nP; ++i) {
            const double rho_xy = px[i] * cosT + py[i] * sinT;
            const long bin = static_cast<long>(std::lround(
                slope * (rho_xy - rho_first) + 1.0));
            if (bin == rbin) idx.push_back(i);
        }
        if (idx.empty()) continue;

        // Convert back to 1-based (r, c).
        rseg.clear(); cseg.clear();
        for (std::size_t i : idx) {
            rseg.push_back(static_cast<std::size_t>(py[i]) + 1);
            cseg.push_back(static_cast<std::size_t>(px[i]) + 1);
        }

        // Determine sort key: r_range vs c_range.
        std::size_t rmin = rseg[0], rmax = rseg[0];
        std::size_t cmin = cseg[0], cmax = cseg[0];
        for (std::size_t i = 1; i < rseg.size(); ++i) {
            if (rseg[i] < rmin) rmin = rseg[i];
            if (rseg[i] > rmax) rmax = rseg[i];
            if (cseg[i] < cmin) cmin = cseg[i];
            if (cseg[i] > cmax) cmax = cseg[i];
        }
        const std::size_t r_range = rmax - rmin;
        const std::size_t c_range = cmax - cmin;
        // Sort rows by (key1, key2): if r_range > c_range, key1=r, key2=c;
        // else key1=c, key2=r.
        std::pmr::vector<std::size_t> order(mr);
        order.reserve(rseg.size());
        for (std::size_t i = 0; i < rseg.size(); ++i) order.push_back(i);
        if (r_range > c_range) {
            std::sort(order.begin(), order.end(),
                      [&](std::size_t a, std::size_t b) {
                          if (rseg[a] != rseg[b]) return rseg[a] < rseg[b];
                          return cseg[a] < cseg[b];
                      });
        } else {
            std::sort(order.begin(), order.end(),
                      [&](std::size_t a, std::size_t b) {
                          if (cseg[a] != cseg[b]) return cseg[a] < cseg[b];
                          return rseg[a] < rseg[b];
                      });
        }

        // Build sorted (xy = [c r]).
        std::pmr::vector<long> xs(mr), ys(mr);
        xs.reserve(rseg.size());
        ys.reserve(rseg.size());
        for (std::size_t i : order) {
            xs.push_back(static_cast<long>(cseg[i]));
            ys.push_back(static_cast<long>(rseg[i]));
        }

        // diff_sq = (xs[i+1]-xs[i])^2 + (ys[i+1]-ys[i])^2.
        // Find gaps where diff_sq > fillgap^2.
        std::pmr::vector<std::size_t> gap_at(mr);
        for (std::size_t i = 0; i + 1 < xs.size(); ++i) {
            const long dx = xs[i + 1] - xs[i];
            const long dy = ys[i + 1] - ys[i];
            const double d2 = static_cast<double>(dx) * dx
                            + static_cast<double>(dy) * dy;
            if (d2 > fillgap_sq) gap_at.push_back(i + 1);  // 1-based start
        }
        // Segments split by gap_at indices: idx = [0; gap_at; size].
        // MATLAB: for p=1:length(idx)-1: p1=xy(idx(p)+1,:); p2=xy(idx(p+1),:);
        std::pmr::vector<std::size_t> splits(mr);
        splits.push_back(0);
        for (auto g : gap_at) splits.push_back(g);
        splits.push_back(xs.size());
        for (std::size_t p = 0; p + 1 < splits.size(); ++p) {
            const std::size_t lo = splits[p];      // exclusive lower (gap start)
            const std::size_t hi = splits[p + 1];  // inclusive upper
            if (hi <= lo) continue;
            // p1 = xy(lo+1) in MATLAB 1-idx; here 0-indexed → xs[lo].
            // p2 = xy(hi) in MATLAB → xs[hi-1].
            const long p1x = xs[lo];
            const long p1y = ys[lo];
            const long p2x = xs[hi - 1];
            const long p2y = ys[hi - 1];
            const double linelen_sq
                = static_cast<double>(p2x - p1x) * (p2x - p1x)
                + static_cast<double>(p2y - p1y) * (p2y - p1y);
            if (linelen_sq >= minlength_sq) {
                Seg s;
                s.p1x = static_cast<double>(p1x);
                s.p1y = static_cast<double>(p1y);
                s.p2x = static_cast<double>(p2x);
                s.p2y = static_cast<double>(p2y);
                s.theta = theta_deg.elemAsDouble(cbin - 1);
                s.rho   = rho.elemAsDouble(rbin - 1);
                segs.push_back(s);
            }
        }
    }

    // Build 1 × N struct array.
    const std::size_t nL = segs.size();
    if (nL == 0) {
        // Empty struct with the four fields (MATLAB: 1 × 0 struct).
        Value out = Value::structArray(1, 0, mr);
        return out;
    }
    Value out = Value::structArray(1, nL, mr);
    for (std::size_t i = 0; i < nL; ++i) {
        Value p1 = Value::matrix(1, 2, ValueType::DOUBLE, mr);
        p1.doubleDataMut()[0] = segs[i].p1x;
        p1.doubleDataMut()[1] = segs[i].p1y;
        Value p2 = Value::matrix(1, 2, ValueType::DOUBLE, mr);
        p2.doubleDataMut()[0] = segs[i].p2x;
        p2.doubleDataMut()[1] = segs[i].p2y;
        out.setField(i, "point1", p1);
        out.setField(i, "point2", p2);
        out.setField(i, "theta", Value::scalar(segs[i].theta, mr));
        out.setField(i, "rho",   Value::scalar(segs[i].rho,   mr));
    }
    return out;
}
} // namespace numkit::image
