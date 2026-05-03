// libs/image/src/filter/filter.cpp

#include <numkit/image/filter/filter.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include <algorithm>
#include <cmath>
#include <cstring>
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

Value padarray(std::pmr::memory_resource *mr, const Value &x,
               const std::vector<int> &padsize,
               PadMode mode, double pad_value,
               const std::string &direction)
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

Value mat_double(std::pmr::memory_resource *mr,
                 const std::vector<double> &v, int rows, int cols) {
    Value out = Value::matrix(rows, cols, ValueType::DOUBLE, mr);
    if (!v.empty()) std::memcpy(out.doubleDataMut(), v.data(), v.size() * sizeof(double));
    return out;
}

Value fspecial_average(std::pmr::memory_resource *mr, int rows, int cols) {
    const double w = 1.0 / (double(rows) * double(cols));
    std::vector<double> data(size_t(rows) * size_t(cols), w);
    return mat_double(mr, data, rows, cols);
}

Value fspecial_gaussian(std::pmr::memory_resource *mr, int rows, int cols, double sigma) {
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
    return mat_double(mr, k, rows, cols);
}

Value fspecial_laplacian(std::pmr::memory_resource *mr, double alpha) {
    // 3×3 Laplacian, controlled by alpha ∈ [0, 1] (default 0.2 in MATLAB).
    if (alpha < 0.0) alpha = 0.0; if (alpha > 1.0) alpha = 1.0;
    const double a = alpha;
    const double s = 1.0 / (a + 1.0);
    std::vector<double> k = {
        s * a / 4.0,         s * (1.0 - a) / 4.0, s * a / 4.0,         // col 0
        s * (1.0 - a) / 4.0, s * (-1.0),          s * (1.0 - a) / 4.0, // col 1
        s * a / 4.0,         s * (1.0 - a) / 4.0, s * a / 4.0,         // col 2
    };
    return mat_double(mr, k, 3, 3);
}

Value fspecial_log(std::pmr::memory_resource *mr, int rows, int cols, double sigma) {
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
    return mat_double(mr, log_k, rows, cols);
}

Value fspecial_sobel(std::pmr::memory_resource *mr) {
    // MATLAB convention: [1 2 1; 0 0 0; -1 -2 -1]. Column-major storage:
    // col0 = [1, 0, -1], col1 = [2, 0, -2], col2 = [1, 0, -1].
    std::vector<double> k = {1, 0, -1,   2, 0, -2,   1, 0, -1};
    return mat_double(mr, k, 3, 3);
}

Value fspecial_prewitt(std::pmr::memory_resource *mr) {
    // [1 1 1; 0 0 0; -1 -1 -1] column-major.
    std::vector<double> k = {1, 0, -1,   1, 0, -1,   1, 0, -1};
    return mat_double(mr, k, 3, 3);
}

Value fspecial_disk(std::pmr::memory_resource *mr, double radius) {
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
    return mat_double(mr, k, side, side);
}

} // anonymous

Value fspecial(std::pmr::memory_resource *mr,
               const std::string &type,
               const std::vector<double> &params)
{
    if (type == "average") {
        int rows = params.size() >= 1 ? int(params[0]) : 3;
        int cols = params.size() >= 2 ? int(params[1]) : rows;
        return fspecial_average(mr, rows, cols);
    }
    if (type == "gaussian") {
        int rows = params.size() >= 1 ? int(params[0]) : 3;
        int cols = params.size() >= 2 ? int(params[1]) : rows;
        double sigma = params.size() >= 3 ? params[2] : 0.5;
        return fspecial_gaussian(mr, rows, cols, sigma);
    }
    if (type == "laplacian") {
        double alpha = params.size() >= 1 ? params[0] : 0.2;
        return fspecial_laplacian(mr, alpha);
    }
    if (type == "log") {
        int rows = params.size() >= 1 ? int(params[0]) : 5;
        int cols = params.size() >= 2 ? int(params[1]) : rows;
        double sigma = params.size() >= 3 ? params[2] : 0.5;
        return fspecial_log(mr, rows, cols, sigma);
    }
    if (type == "sobel")   return fspecial_sobel(mr);
    if (type == "prewitt") return fspecial_prewitt(mr);
    if (type == "disk") {
        double radius = params.size() >= 1 ? params[0] : 5.0;
        return fspecial_disk(mr, radius);
    }
    throw Error("fspecial: unknown filter type '" + type + "'",
                0, 0, "fspecial", "", "m:fspecial:badtype");
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

    outs[0] = padarray(ctx.engine->resource(), args[0], padsize, mode,
                       pad_value, direction);
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

    outs[0] = fspecial(ctx.engine->resource(), type, params);
}

} // namespace detail
} // namespace numkit::image
