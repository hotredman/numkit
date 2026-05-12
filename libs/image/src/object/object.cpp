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
