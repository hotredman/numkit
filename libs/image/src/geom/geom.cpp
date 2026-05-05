// libs/image/src/geom/geom.cpp
//
// Geometric transforms — resize / crop / rotate / translate. All
// transforms work on a generic (H, W, C) layout with column-major
// numkit storage. Sample types preserved: uint8 stays uint8 with
// values clipped to [0, 255], double / float pass through, etc.
//
// Interpolation:
//   * "nearest"  : round source coordinates to nearest integer
//   * "bilinear" : 2×2 weighted average of the four enclosing pixels
//   We don't expose bicubic yet — only marginal quality gain over
//   bilinear for the resampling factors that show up in
//   typical numkit pipelines.

#include <numkit/image/geom/geom.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace numkit::image {

namespace {

struct Shape { size_t H, W, C; };

// Pull (H, W, C) out of an image Value. Accepts both H×W (C=1) and
// H×W×C 3-D arrays.
Shape shapeOf(const Value &A) {
    const auto &d = A.dims();
    Shape s{};
    s.H = d.rows();
    s.W = d.cols();
    if (A.numel() == s.H * s.W)              s.C = 1;
    else if (A.numel() == s.H * s.W * 3)     s.C = 3;
    else if (A.numel() == s.H * s.W * 4)     s.C = 4;
    else
        throw Error("image geom: input must be H×W or H×W×{1,3,4}",
                    0, 0, "geom", "", "m:image:geom:shape");
    return s;
}

inline size_t idx3D(const Shape &s, size_t y, size_t x, size_t c) {
    return c * s.H * s.W + x * s.H + y;
}

inline double sample(const Value &A, const Shape &s,
                     int y, int x, size_t c) {
    if (y < 0 || x < 0 ||
        y >= static_cast<int>(s.H) ||
        x >= static_cast<int>(s.W))
        return 0.0;
    return A.elemAsDouble(idx3D(s, static_cast<size_t>(y),
                                static_cast<size_t>(x), c));
}

// Bilinear lookup with floating source coordinates (0-based).
// Out-of-bounds pixels evaluate to 0.
double bilinear(const Value &A, const Shape &s,
                double y, double x, size_t c) {
    const int y0 = static_cast<int>(std::floor(y));
    const int x0 = static_cast<int>(std::floor(x));
    const double dy = y - y0;
    const double dx = x - x0;
    const double v00 = sample(A, s, y0,     x0,     c);
    const double v01 = sample(A, s, y0,     x0 + 1, c);
    const double v10 = sample(A, s, y0 + 1, x0,     c);
    const double v11 = sample(A, s, y0 + 1, x0 + 1, c);
    return v00 * (1 - dy) * (1 - dx) +
           v01 * (1 - dy) *      dx  +
           v10 *      dy  * (1 - dx) +
           v11 *      dy  *      dx;
}

// Write a sampled value `v` (in native units — uint8 0..255,
// double passes straight through, etc.) into output element idx i.
// Clips integer types to their natural range.
void writeNative(Value &out, size_t i, double v, ValueType t) {
    switch (t) {
        case ValueType::DOUBLE:
            out.doubleDataMut()[i] = v; break;
        case ValueType::SINGLE:
            out.singleDataMut()[i] = static_cast<float>(v); break;
        case ValueType::UINT8: {
            if (v < 0.0) v = 0.0;
            if (v > 255.0) v = 255.0;
            out.uint8DataMut()[i] = static_cast<std::uint8_t>(std::lround(v));
            break;
        }
        case ValueType::UINT16: {
            if (v < 0.0) v = 0.0;
            if (v > 65535.0) v = 65535.0;
            out.uint16DataMut()[i] = static_cast<std::uint16_t>(std::lround(v));
            break;
        }
        case ValueType::INT16: {
            if (v < -32768.0) v = -32768.0;
            if (v > 32767.0)  v = 32767.0;
            out.int16DataMut()[i] = static_cast<std::int16_t>(std::lround(v));
            break;
        }
        case ValueType::LOGICAL:
            out.logicalDataMut()[i] = (v != 0.0) ? 1u : 0u; break;
        default:
            // Fall back: store as double (the buffer is large enough).
            out.doubleDataMut()[i] = v; break;
    }
}

void writePixel(Value &out, const Shape &s,
                size_t y, size_t x, size_t c,
                double v, ValueType srcType)
{
    writeNative(out, idx3D(s, y, x, c), v, srcType);
}

Value makeOut(std::pmr::memory_resource *mr,
              size_t H, size_t W, size_t C, ValueType t) {
    if (C == 1) return Value::matrix(H, W, t, mr);
    return Value::matrix3d(H, W, C, t, mr);
}

bool methodIsNearest(const std::string &m) {
    return m == "nearest" || m == "Nearest" || m == "NEAREST" ||
           m == "near"    || m == "n";
}

} // anonymous

Value imresize(std::pmr::memory_resource *mr,
               const Value &A, size_t outH, size_t outW,
               const std::string &method)
{
    const Shape s = shapeOf(A);
    const ValueType t = A.type();
    if (outH == 0 || outW == 0)
        return makeOut(mr, outH, outW, s.C, t);

    // Map output (yo, xo) → source coordinate via the centre-aligned
    // formula MATLAB / OpenCV use for resampling:
    //   x_src = (xo + 0.5) * (W / outW) − 0.5
    const double sx = double(s.W) / double(outW);
    const double sy = double(s.H) / double(outH);
    const bool nearest = methodIsNearest(method);

    Value B = makeOut(mr, outH, outW, s.C, t);
    Shape sd{outH, outW, s.C};
    for (size_t c = 0; c < s.C; ++c) {
        for (size_t yo = 0; yo < outH; ++yo) {
            const double ys = (yo + 0.5) * sy - 0.5;
            for (size_t xo = 0; xo < outW; ++xo) {
                const double xs = (xo + 0.5) * sx - 0.5;
                double v;
                if (nearest) {
                    int yi = int(std::floor(ys + 0.5));
                    int xi = int(std::floor(xs + 0.5));
                    if (yi < 0) yi = 0;
                    if (xi < 0) xi = 0;
                    if (yi >= int(s.H)) yi = int(s.H) - 1;
                    if (xi >= int(s.W)) xi = int(s.W) - 1;
                    v = sample(A, s, yi, xi, c);
                } else {
                    v = bilinear(A, s, ys, xs, c);
                }
                writePixel(B, sd, yo, xo, c, v, t);
            }
        }
    }
    return B;
}

Value imresize(std::pmr::memory_resource *mr,
               const Value &A, double scale,
               const std::string &method)
{
    const Shape s = shapeOf(A);
    if (!(scale > 0.0))
        throw Error("imresize: scale must be > 0",
                    0, 0, "imresize", "", "m:imresize:scale");
    const size_t outH = static_cast<size_t>(std::round(scale * double(s.H)));
    const size_t outW = static_cast<size_t>(std::round(scale * double(s.W)));
    return imresize(mr, A, outH, outW, method);
}

Value imcrop(std::pmr::memory_resource *mr,
             const Value &A, double xmin, double ymin,
             double width, double height)
{
    const Shape s = shapeOf(A);
    const ValueType t = A.type();

    // MATLAB rect = [xmin ymin width height], xmin/ymin are 1-based
    // *real* coordinates; rect is clipped to image bounds.
    int x0 = std::max(int(std::round(xmin)) - 1, 0);
    int y0 = std::max(int(std::round(ymin)) - 1, 0);
    int w  = std::max(int(std::round(width))  + 1, 1);
    int h  = std::max(int(std::round(height)) + 1, 1);
    if (x0 + w > int(s.W)) w = int(s.W) - x0;
    if (y0 + h > int(s.H)) h = int(s.H) - y0;
    if (w <= 0 || h <= 0)
        return makeOut(mr, 0, 0, s.C, t);

    Value B = makeOut(mr, size_t(h), size_t(w), s.C, t);
    Shape sd{size_t(h), size_t(w), s.C};
    for (size_t c = 0; c < s.C; ++c)
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x) {
                const double v = A.elemAsDouble(
                    idx3D(s, size_t(y0 + y), size_t(x0 + x), c));
                writePixel(B, sd, size_t(y), size_t(x), c, v, t);
            }
    return B;
}

Value imrotate(std::pmr::memory_resource *mr,
               const Value &A, double angle,
               const std::string &method, const std::string &bbox)
{
    const Shape s = shapeOf(A);
    const ValueType t = A.type();
    const bool nearest = methodIsNearest(method);
    const double rad = angle * M_PI / 180.0;
    const double co = std::cos(rad);
    const double si = std::sin(rad);

    // Output dims.
    size_t outH = s.H, outW = s.W;
    if (bbox != "crop" && bbox != "Crop") {
        // Loose: bounding box of the rotated rectangle around its
        // centre. Snap near-integer results down before the ceil so a
        // sub-ULP overshoot from sin/cos (e.g. cos(90°) ≈ 6e-17)
        // doesn't bump 2.0 up to 3.
        const double Hd = double(s.H), Wd = double(s.W);
        const double rawH = std::abs(Hd * co) + std::abs(Wd * si);
        const double rawW = std::abs(Wd * co) + std::abs(Hd * si);
        auto snap = [](double v) {
            const double r = std::round(v);
            return (std::abs(v - r) < 1e-9) ? r : v;
        };
        outH = size_t(std::ceil(snap(rawH)));
        outW = size_t(std::ceil(snap(rawW)));
        if (outH < 1) outH = 1;
        if (outW < 1) outW = 1;
    }

    // Centre coordinates.
    const double cyOut = (double(outH) - 1.0) / 2.0;
    const double cxOut = (double(outW) - 1.0) / 2.0;
    const double cyIn  = (double(s.H)  - 1.0) / 2.0;
    const double cxIn  = (double(s.W)  - 1.0) / 2.0;

    // Inverse rotation in image coords. The y-axis runs downward, so
    // a "CCW rotation as you look at the image" (MATLAB convention) is
    // a CW rotation in math coords. The inverse mapping
    //   pᵢₙ = R(+θ) · (pₒᵤₜ − cₒᵤₜ) + cᵢₙ
    // — i.e. (cos, −sin; +sin, cos) — gives the canonical MATLAB
    // imrotate output (verified against Octave 11.1).
    Value B = makeOut(mr, outH, outW, s.C, t);
    Shape sd{outH, outW, s.C};
    for (size_t c = 0; c < s.C; ++c)
        for (size_t y = 0; y < outH; ++y) {
            const double dy = double(y) - cyOut;
            for (size_t x = 0; x < outW; ++x) {
                const double dx = double(x) - cxOut;
                const double xs = co * dx - si * dy + cxIn;
                const double ys = si * dx + co * dy + cyIn;
                double v;
                if (nearest) {
                    int yi = int(std::floor(ys + 0.5));
                    int xi = int(std::floor(xs + 0.5));
                    v = sample(A, s, yi, xi, c);
                } else {
                    v = bilinear(A, s, ys, xs, c);
                }
                writePixel(B, sd, y, x, c, v, t);
            }
        }
    return B;
}

Value imtranslate(std::pmr::memory_resource *mr,
                  const Value &A, double dx, double dy)
{
    const Shape s = shapeOf(A);
    const ValueType t = A.type();
    Value B = makeOut(mr, s.H, s.W, s.C, t);
    for (size_t c = 0; c < s.C; ++c)
        for (size_t y = 0; y < s.H; ++y)
            for (size_t x = 0; x < s.W; ++x) {
                const double xs = double(x) - dx;
                const double ys = double(y) - dy;
                const double v = bilinear(A, s, ys, xs, c);
                writePixel(B, s, y, x, c, v, t);
            }
    return B;
}

Value axes2pix(std::pmr::memory_resource *mr,
               double n, const Value &extent, const Value &axesCoord)
{
    if (extent.numel() < 1)
        throw Error("axes2pix: EXTENT must be a non-empty vector",
                    0, 0, "axes2pix", "", "m:axes2pix:extent");

    const double e0 = extent.elemAsDouble(0);
    const double e1 = extent.elemAsDouble(extent.numel() - 1);
    const bool degenerate = (n == 1.0) || (e0 == e1);
    const double pixelWidth = degenerate ? 1.0 : (e1 - e0) / (n - 1.0);

    const auto &d = axesCoord.dims();
    const size_t H = d.rows();
    const size_t W = d.cols();
    Value out;
    if (d.is3D())
        out = Value::matrix3d(H, W, d.pages(), ValueType::DOUBLE, mr);
    else
        out = Value::matrix(H, W, ValueType::DOUBLE, mr);
    if (axesCoord.numel() == 0) return out;

    double *od = out.doubleDataMut();
    const size_t N = axesCoord.numel();
    for (size_t i = 0; i < N; ++i) {
        const double a = axesCoord.elemAsDouble(i);
        od[i] = degenerate ? (a - e0 + 1.0)
                           : ((a - e0) / pixelWidth + 1.0);
    }
    return out;
}

Value impyramid(std::pmr::memory_resource *mr,
                const Value &A, const std::string &type)
{
    // Binomial 5-tap kernel [1 4 6 4 1]/16 (MATLAB / Octave default).
    static constexpr double kBurt[5] = {1.0 / 16, 4.0 / 16, 6.0 / 16,
                                        4.0 / 16, 1.0 / 16};
    const Shape s = shapeOf(A);
    const ValueType t = A.type();

    bool reduce;
    if      (type == "reduce" || type == "Reduce") reduce = true;
    else if (type == "expand" || type == "Expand") reduce = false;
    else throw Error("impyramid: type must be 'reduce' or 'expand'",
                     0, 0, "impyramid", "", "m:impyramid:type");

    size_t Hout, Wout;
    if (reduce) { Hout = (s.H + 1) / 2; Wout = (s.W + 1) / 2; }
    else        { Hout = s.H ? 2 * s.H - 1 : 0;
                  Wout = s.W ? 2 * s.W - 1 : 0; }

    Value B = makeOut(mr, Hout, Wout, s.C, t);
    if (s.H == 0 || s.W == 0 || Hout == 0 || Wout == 0) return B;
    Shape sd{Hout, Wout, s.C};

    auto clamp_i = [](int i, int N) {
        if (i < 0) return 0;
        if (i >= N) return N - 1;
        return i;
    };
    // MATLAB's impyramid 'reduce' pads with symmetric (mirror without
    // edge), NOT replicate: padded[-k] = original[k-1] for k>=1, and
    // padded[N+k-1] = original[N-k] for k>=1. Octave-image uses
    // replicate, so for reduce we follow MATLAB per source-of-truth.
    auto sym_i = [](int i, int N) {
        if (N <= 1) return 0;
        if (i < 0)  return -1 - i;
        if (i >= N) return 2 * N - 1 - i;
        return i;
    };
    (void)clamp_i;  // 'expand' branch below references sym_i; clamp_i
                    // kept around for potential future variants.

    if (reduce) {
        for (size_t c = 0; c < s.C; ++c) {
            // Pass 1: horizontal filter at every (r, cc).
            std::vector<double> tmp(s.H * s.W);
            for (size_t r = 0; r < s.H; ++r) {
                for (size_t cc = 0; cc < s.W; ++cc) {
                    double acc = 0;
                    for (int k = -2; k <= 2; ++k) {
                        const int cs = sym_i((int)cc + k, (int)s.W);
                        acc += kBurt[k + 2] *
                               A.elemAsDouble(idx3D(s, r, (size_t)cs, c));
                    }
                    tmp[cc * s.H + r] = acc;
                }
            }
            // Pass 2: vertical filter at sample positions only.
            for (size_t yo = 0; yo < Hout; ++yo) {
                const size_t ys = 2 * yo;
                for (size_t xo = 0; xo < Wout; ++xo) {
                    const size_t xs = 2 * xo;
                    double acc = 0;
                    for (int k = -2; k <= 2; ++k) {
                        const int rs = sym_i((int)ys + k, (int)s.H);
                        acc += kBurt[k + 2] * tmp[xs * s.H + (size_t)rs];
                    }
                    writePixel(B, sd, yo, xo, c, acc, t);
                }
            }
        }
    } else {
        // Octave-image (Burt-Adelson Laplacian-pyramid expand) uses
        // ZERO-pad boundary on the zero-stuffed grid, with separable
        // ×2 compensation per axis (×4 total).
        for (size_t c = 0; c < s.C; ++c) {
            // Zero-stuff into Hout × Wout double buffer.
            std::vector<double> stuffed(Hout * Wout, 0.0);
            for (size_t r = 0; r < s.H; ++r)
                for (size_t cc = 0; cc < s.W; ++cc)
                    stuffed[(2 * cc) * Hout + 2 * r] =
                        A.elemAsDouble(idx3D(s, r, cc, c));

            auto sample = [&](int r, int cc) -> double {
                if (r < 0 || cc < 0 ||
                    r >= (int)Hout || cc >= (int)Wout) return 0.0;
                return stuffed[(size_t)cc * Hout + (size_t)r];
            };

            // Horizontal filter (zero-pad boundary), ×2 compensation.
            std::vector<double> tmp(Hout * Wout);
            for (size_t r = 0; r < Hout; ++r) {
                for (size_t cc = 0; cc < Wout; ++cc) {
                    double acc = 0;
                    for (int k = -2; k <= 2; ++k)
                        acc += kBurt[k + 2] * sample((int)r, (int)cc + k);
                    tmp[cc * Hout + r] = 2.0 * acc;
                }
            }
            auto sample_t = [&](int r, int cc) -> double {
                if (r < 0 || cc < 0 ||
                    r >= (int)Hout || cc >= (int)Wout) return 0.0;
                return tmp[(size_t)cc * Hout + (size_t)r];
            };
            // Vertical filter (zero-pad boundary), ×2 compensation.
            for (size_t cc = 0; cc < Wout; ++cc) {
                for (size_t r = 0; r < Hout; ++r) {
                    double acc = 0;
                    for (int k = -2; k <= 2; ++k)
                        acc += kBurt[k + 2] * sample_t((int)r + k, (int)cc);
                    writePixel(B, sd, r, cc, c, 2.0 * acc, t);
                }
            }
        }
    }
    return B;
}

namespace detail {

static std::string argString(const Value &v) {
    if (!v.isChar() && !v.isString())
        throw Error("image geom: expected a string argument",
                    0, 0, "geom", "", "m:image:geom:type");
    return v.toString();
}

void imresize_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
                  CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("imresize: requires (A, scale_or_size [, method])",
                    0, 0, "imresize", "", "m:imresize:nargin");
    auto *mr = ctx.engine->resource();
    std::string method = "bilinear";
    if (args.size() >= 3 && !args[2].isEmpty()) method = argString(args[2]);
    if (args[1].numel() == 1) {
        outs[0] = imresize(mr, args[0], args[1].toScalar(), method);
    } else if (args[1].numel() == 2) {
        const size_t outH = size_t(args[1].elemAsDouble(0));
        const size_t outW = size_t(args[1].elemAsDouble(1));
        outs[0] = imresize(mr, args[0], outH, outW, method);
    } else {
        throw Error("imresize: 2nd arg must be a scalar scale "
                    "or a 2-element [outH outW]",
                    0, 0, "imresize", "", "m:imresize:size");
    }
}

void imcrop_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
                CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("imcrop: requires (A, [xmin ymin width height])",
                    0, 0, "imcrop", "", "m:imcrop:nargin");
    if (args[1].numel() < 4)
        throw Error("imcrop: rect must have 4 elements",
                    0, 0, "imcrop", "", "m:imcrop:rect");
    outs[0] = imcrop(ctx.engine->resource(), args[0],
                     args[1].elemAsDouble(0), args[1].elemAsDouble(1),
                     args[1].elemAsDouble(2), args[1].elemAsDouble(3));
}

void imrotate_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
                  CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("imrotate: requires (A, angle [, method [, bbox]])",
                    0, 0, "imrotate", "", "m:imrotate:nargin");
    std::string method = "bilinear";
    std::string bbox   = "loose";
    if (args.size() >= 3 && !args[2].isEmpty()) method = argString(args[2]);
    if (args.size() >= 4 && !args[3].isEmpty()) bbox   = argString(args[3]);
    outs[0] = imrotate(ctx.engine->resource(), args[0],
                       args[1].toScalar(), method, bbox);
}

void imtranslate_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
                     CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("imtranslate: requires (A, [dx dy])",
                    0, 0, "imtranslate", "", "m:imtranslate:nargin");
    if (args[1].numel() < 2)
        throw Error("imtranslate: vector must have 2 elements",
                    0, 0, "imtranslate", "", "m:imtranslate:vec");
    outs[0] = imtranslate(ctx.engine->resource(), args[0],
                          args[1].elemAsDouble(0),
                          args[1].elemAsDouble(1));
}

void axes2pix_reg(Span<const Value> args, size_t /*nargout*/,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("axes2pix: requires (n, extent, axesCoord)",
                    0, 0, "axes2pix", "", "m:axes2pix:nargin");
    const double n = args[0].toScalar();
    outs[0] = axes2pix(ctx.engine->resource(), n, args[1], args[2]);
}

void impyramid_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
                   CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("impyramid: requires (A, type)", 0, 0, "impyramid", "",
                    "m:impyramid:nargin");
    const std::string type = argString(args[1]);
    outs[0] = impyramid(ctx.engine->resource(), args[0], type);
}

} // namespace detail

} // namespace numkit::image
