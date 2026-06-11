// toolboxes/image/src/geom/geom.cpp
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

#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>
#include "geom_detail.hpp"

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
                    0, 0, "geom", "", "numkit:image:geom:shape");
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

void writePixel(Value &out, const Shape &s,
                size_t y, size_t x, size_t c,
                double v, ValueType srcType)
{
    writeNative(out, idx3D(s, y, x, c), v, srcType);
}

Value makeOut(size_t H, size_t W, size_t C, ValueType t, std::pmr::memory_resource *mr) {
    if (C == 1) return Value::matrix(H, W, t, mr);
    return Value::matrix3d(H, W, C, t, mr);
}

bool methodIsNearest(const std::string &m) {
    return m == "nearest" || m == "Nearest" || m == "NEAREST" ||
           m == "near"    || m == "n";
}

} // anonymous

Value imresize(const Value &A, size_t outH, size_t outW, const std::string &method, std::pmr::memory_resource *mr)
{
    const Shape s = shapeOf(A);
    const ValueType t = A.type();
    if (outH == 0 || outW == 0)
        return makeOut(outH, outW, s.C, t, mr);

    // Map output (yo, xo) → source coordinate via the centre-aligned
    // formula MATLAB / OpenCV use for resampling:
    //   x_src = (xo + 0.5) * (W / outW) − 0.5
    const double sx = double(s.W) / double(outW);
    const double sy = double(s.H) / double(outH);
    const bool nearest = methodIsNearest(method);

    Value B = makeOut(outH, outW, s.C, t, mr);
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

Value imresize(const Value &A, double scale, const std::string &method, std::pmr::memory_resource *mr)
{
    const Shape s = shapeOf(A);
    if (!(scale > 0.0))
        throw Error("imresize: scale must be > 0",
                    0, 0, "imresize", "", "numkit:imresize:scale");
    const size_t outH = static_cast<size_t>(std::round(scale * double(s.H)));
    const size_t outW = static_cast<size_t>(std::round(scale * double(s.W)));
    return imresize(A, outH, outW, method, mr);
}

Value imcrop(const Value &A, double xmin, double ymin, double width, double height, std::pmr::memory_resource *mr)
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
        return makeOut(0, 0, s.C, t, mr);

    Value B = makeOut(size_t(h), size_t(w), s.C, t, mr);
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

// ── imcrop3 (3-D / 4-D volume cropping, MATLAB R2025b imcrop3.m) ──
//
// CUBOID = [XMIN YMIN ZMIN WIDTH HEIGHT DEPTH] (MATLAB's spatial
// X/Y/Z = col/row/page convention). xLimits = [round(XMIN),
// round(XMIN+WIDTH)] and similarly for y, z (1-based, inclusive).
// Output extracts V(yLim, xLim, zLim, :) so the 4th dimension
// (channels / time) passes through unchanged. Class-preserving.
// Out-of-bounds cuboid throws (matches MATLAB's error message
// `images:imcrop3:cropCuboidOutofBounds`).
Value imcrop3(const Value &V, const Value &cuboid, std::pmr::memory_resource *mr)
{
    if (cuboid.numel() != 6)
        throw Error("imcrop3: CUBOID must be a 6-element vector "
                    "[XMIN YMIN ZMIN WIDTH HEIGHT DEPTH]",
                    0, 0, "imcrop3", "", "numkit:imcrop3:cuboid");

    // Resolve input shape: H × W × D × T  (T defaults to 1 for 3-D inputs).
    const auto &dV = V.dims();
    const int nd = dV.ndims();
    if (nd < 3)
        throw Error("imcrop3: V must be at least 3-D",
                    0, 0, "imcrop3", "", "numkit:imcrop3:rank");
    const std::size_t H = dV.dim(0);
    const std::size_t W = dV.dim(1);
    const std::size_t D = dV.dim(2);
    const std::size_t T = (nd >= 4) ? dV.dim(3) : 1;

    // Round limits per MATLAB source.
    auto rd = [&](std::size_t k) {
        return static_cast<long long>(std::lround(cuboid.elemAsDouble(k)));
    };
    const long long xmin = rd(0);
    const long long ymin = rd(1);
    const long long zmin = rd(2);
    const long long xmax = static_cast<long long>(std::lround(
        cuboid.elemAsDouble(0) + cuboid.elemAsDouble(3)));
    const long long ymax = static_cast<long long>(std::lround(
        cuboid.elemAsDouble(1) + cuboid.elemAsDouble(4)));
    const long long zmax = static_cast<long long>(std::lround(
        cuboid.elemAsDouble(2) + cuboid.elemAsDouble(5)));

    auto in_range = [](long long lo, long long hi, std::size_t N) {
        return lo >= 1 && lo <= static_cast<long long>(N)
            && hi >= 1 && hi <= static_cast<long long>(N) && lo <= hi;
    };
    if (!in_range(xmin, xmax, W)
        || !in_range(ymin, ymax, H)
        || !in_range(zmin, zmax, D))
        throw Error("imcrop3: cuboid is out of bounds of the input "
                    "volume — lower bound must be >= 1 and upper "
                    "bound must not exceed the image size",
                    0, 0, "imcrop3", "", "numkit:imcrop3:cropCuboidOutofBounds");

    const std::size_t outH = static_cast<std::size_t>(ymax - ymin + 1);
    const std::size_t outW = static_cast<std::size_t>(xmax - xmin + 1);
    const std::size_t outD = static_cast<std::size_t>(zmax - zmin + 1);

    // Allocate the output with class preserved. For 3-D inputs we
    // return a 3-D Value; for 4-D inputs we keep the 4th dim.
    const ValueType t = V.type();
    Value B;
    if (T == 1) {
        B = (outD == 1) ? Value::matrix(outH, outW, t, mr)
                         : Value::matrix3d(outH, outW, outD, t, mr);
    } else {
        const std::size_t dims4[4] = { outH, outW, outD, T };
        B = Value::matrixND(dims4, 4, t, mr);
    }

    // Column-major linear index for the input (H, W, D, T):
    //   idx = r + H*c + H*W*p + H*W*D*tt
    auto src_lin = [&](std::size_t r, std::size_t c, std::size_t p,
                       std::size_t tt) {
        return r + H * c + H * W * p + H * W * D * tt;
    };
    auto dst_lin = [&](std::size_t r, std::size_t c, std::size_t p,
                       std::size_t tt) {
        return r + outH * c + outH * outW * p + outH * outW * outD * tt;
    };

    // Per-class byte-copy avoids any quantisation. Use elemAs* / write
    // helpers to dispatch by element type.
    auto write_one = [&](std::size_t di, std::size_t si) {
        switch (t) {
            case ValueType::DOUBLE:  B.doubleDataMut()[di]  = V.doubleData()[si];  break;
            case ValueType::SINGLE:  B.singleDataMut()[di]  = V.singleData()[si];  break;
            case ValueType::UINT8:   B.uint8DataMut()[di]   = V.uint8Data()[si];   break;
            case ValueType::UINT16:  B.uint16DataMut()[di]  = V.uint16Data()[si];  break;
            case ValueType::UINT32:  B.uint32DataMut()[di]  = V.uint32Data()[si];  break;
            case ValueType::UINT64:  B.uint64DataMut()[di]  = V.uint64Data()[si];  break;
            case ValueType::INT8:    B.int8DataMut()[di]    = V.int8Data()[si];    break;
            case ValueType::INT16:   B.int16DataMut()[di]   = V.int16Data()[si];   break;
            case ValueType::INT32:   B.int32DataMut()[di]   = V.int32Data()[si];   break;
            case ValueType::INT64:   B.int64DataMut()[di]   = V.int64Data()[si];   break;
            case ValueType::LOGICAL: B.logicalDataMut()[di] = V.logicalData()[si]; break;
            default:
                throw Error("imcrop3: unsupported class for V",
                            0, 0, "imcrop3", "", "numkit:imcrop3:cls");
        }
    };

    const std::size_t y0 = static_cast<std::size_t>(ymin - 1);
    const std::size_t x0 = static_cast<std::size_t>(xmin - 1);
    const std::size_t z0 = static_cast<std::size_t>(zmin - 1);

    for (std::size_t tt = 0; tt < T; ++tt)
        for (std::size_t p = 0; p < outD; ++p)
            for (std::size_t c = 0; c < outW; ++c)
                for (std::size_t r = 0; r < outH; ++r)
                    write_one(dst_lin(r, c, p, tt),
                              src_lin(y0 + r, x0 + c, z0 + p, tt));
    return B;
}

Value imrotate(const Value &A, double angle, const std::string &method, const std::string &bbox, std::pmr::memory_resource *mr)
{
    const Shape s = shapeOf(A);
    const ValueType t = A.type();
    const bool nearest = methodIsNearest(method);
    const bool isCrop = (bbox == "crop" || bbox == "Crop");

    // Exact 90-degree-multiple path: pure index permutation (rot90), no
    // interpolation or rounding — matches MATLAB bit-exactly and avoids the
    // half-pixel .5-tie ambiguity that an interpolated rotation hits at
    // these angles. 'crop' then centre-extracts the input-sized window.
    {
        double a = std::fmod(angle, 360.0);
        if (a < 0.0) a += 360.0;
        const long kk = std::lround(a / 90.0);
        if (std::abs(a - double(kk) * 90.0) < 1e-9) {
            const int k = static_cast<int>(((kk % 4) + 4) % 4);
            const size_t Hl = (k % 2 == 0) ? s.H : s.W;  // loose dims
            const size_t Wl = (k % 2 == 0) ? s.W : s.H;
            const size_t oH = isCrop ? s.H : Hl;
            const size_t oW = isCrop ? s.W : Wl;
            long rowOff = 0, colOff = 0;
            if (isCrop) {
                rowOff = static_cast<long>(std::floor((double(Hl) - double(s.H)) / 2.0));
                colOff = static_cast<long>(std::floor((double(Wl) - double(s.W)) / 2.0));
                // For the 270-degree quarter turn (k==3) the leftover half
                // pixel of an odd size difference falls on the opposite side
                // vs 90 degrees (k==1) — MATLAB centres the crop accordingly.
                if (k == 3 && ((static_cast<long>(Hl) - static_cast<long>(s.H)) & 1L))
                    rowOff += 1;
            }
            Value B = makeOut(oH, oW, s.C, t, mr);
            Shape sd{oH, oW, s.C};
            const long H = static_cast<long>(s.H), W = static_cast<long>(s.W);
            for (size_t c = 0; c < s.C; ++c)
                for (size_t i = 0; i < oH; ++i)
                    for (size_t j = 0; j < oW; ++j) {
                        const long li = static_cast<long>(i) + rowOff;  // loose coords
                        const long lj = static_cast<long>(j) + colOff;
                        double v = 0.0;
                        if (li >= 0 && li < static_cast<long>(Hl) &&
                            lj >= 0 && lj < static_cast<long>(Wl)) {
                            long sr = 0, sc = 0;  // map loose -> source (inverse rot90^k)
                            switch (k) {
                                case 0: sr = li;        sc = lj;        break;
                                case 1: sr = lj;        sc = W - 1 - li; break;
                                case 2: sr = H - 1 - li; sc = W - 1 - lj; break;
                                default:sr = H - 1 - lj; sc = li;        break;  // k==3
                            }
                            v = sample(A, s, static_cast<int>(sr), static_cast<int>(sc), c);
                        }
                        writePixel(B, sd, i, j, c, v, t);
                    }
            return B;
        }
    }

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
    Value B = makeOut(outH, outW, s.C, t, mr);
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
                    // MATLAB's imrotate nearest rounds the source coordinate
                    // half-to-even (std::nearbyint, the default FP rounding):
                    // an exact .5 tie goes to the even integer (e.g. 0.5->0),
                    // unlike floor(x+0.5) (half-up) or lround (half-away).
                    int yi = static_cast<int>(std::nearbyint(ys));
                    int xi = static_cast<int>(std::nearbyint(xs));
                    v = sample(A, s, yi, xi, c);
                } else {
                    v = bilinear(A, s, ys, xs, c);
                }
                writePixel(B, sd, y, x, c, v, t);
            }
        }
    return B;
}

Value imtranslate(const Value &A, double dx, double dy, std::pmr::memory_resource *mr)
{
    const Shape s = shapeOf(A);
    const ValueType t = A.type();
    Value B = makeOut(s.H, s.W, s.C, t, mr);
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

Value axes2pix(double n, const Value &extent, const Value &axesCoord, std::pmr::memory_resource *mr)
{
    if (extent.numel() < 1)
        throw Error("axes2pix: EXTENT must be a non-empty vector",
                    0, 0, "axes2pix", "", "numkit:axes2pix:extent");

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

Value impyramid(const Value &A, const std::string &type, std::pmr::memory_resource *mr)
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
                     0, 0, "impyramid", "", "numkit:impyramid:type");

    size_t Hout, Wout;
    if (reduce) { Hout = (s.H + 1) / 2; Wout = (s.W + 1) / 2; }
    else        { Hout = s.H ? 2 * s.H - 1 : 0;
                  Wout = s.W ? 2 * s.W - 1 : 0; }

    Value B = makeOut(Hout, Wout, s.C, t, mr);
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

// ── imresize3 (3-D volume resampling, MATLAB R2025b imresize3.m) ────
//
// Separable 1-D resampling along H / W / D with the same kernel and
// boundary convention as MATLAB's imresize:
//
//   * 1-indexed source coordinate     u = o/scale + 0.5(1 − 1/scale)
//   * mirror boundary on the source   aux = [1..N, N..1], cycle 2N
//   * P = ceil(kernel_width) + 2      taps per output sample
//   * antialiasing for shrink         h(x) = scale·k(scale·x)
//                                      kw   = kw_base / scale
//   * weights normalized to sum 1
//
// References:
//   * Keys, "Cubic convolution interpolation for digital image
//     processing", IEEE TASSP, 1981 — Catmull-Rom cubic (a = −0.5)
//     used here as MATLAB's default 'cubic'.
//   * Duchon, "Lanczos filtering in 1- and 2-dimensional kernel
//     interpolation", J. Appl. Meteor., 1979 — Lanczos windowed sinc.
//   * Smith, "A pixel is not a little square…", Microsoft Tech Memo
//     6, 1995 — pixel-center coordinate convention used by MATLAB.
//
// Kernel half-widths (kernel_width here is total support, MATLAB
// convention):
//   nearest    -- handled directly (no kernel)
//   box        kw = 1
//   triangle / linear kw = 2
//   cubic      kw = 4
//   lanczos2   kw = 4
//   lanczos3   kw = 6
//
namespace {

inline double k_box(double x) {
    return (x >= -0.5 && x < 0.5) ? 1.0 : 0.0;
}
inline double k_triangle(double x) {
    x = std::fabs(x);
    return (x < 1.0) ? (1.0 - x) : 0.0;
}
inline double k_cubic(double x) {
    const double a = std::fabs(x);
    if (a < 1.0) return ((1.5 * a - 2.5) * a * a + 1.0);
    if (a < 2.0) return (((-0.5 * a + 2.5) * a - 4.0) * a + 2.0);
    return 0.0;
}
inline double sinc_pi(double x) {
    if (std::fabs(x) < 1e-12) return 1.0;
    const double px = M_PI * x;
    return std::sin(px) / px;
}
inline double k_lanczos2(double x) {
    return (std::fabs(x) < 2.0) ? sinc_pi(x) * sinc_pi(x / 2.0) : 0.0;
}
inline double k_lanczos3(double x) {
    return (std::fabs(x) < 3.0) ? sinc_pi(x) * sinc_pi(x / 3.0) : 0.0;
}

struct R3Kernel { double (*fn)(double); double width; bool isNearest; };

R3Kernel parseR3Kernel(const std::string &m) {
    auto eq = [&](const char *s) { return m == s; };
    if (eq("nearest") || eq("Nearest"))   return {nullptr,      0.0, true};
    if (eq("box") || eq("Box"))           return {k_box,        1.0, false};
    if (eq("triangle") || eq("Triangle") ||
        eq("linear")   || eq("Linear")  ||
        eq("bilinear") || eq("Bilinear"))
                                          return {k_triangle,   2.0, false};
    if (eq("cubic") || eq("Cubic") ||
        eq("bicubic") || eq("Bicubic"))   return {k_cubic,      4.0, false};
    if (eq("lanczos2") || eq("Lanczos2")) return {k_lanczos2,   4.0, false};
    if (eq("lanczos3") || eq("Lanczos3")) return {k_lanczos3,   6.0, false};
    throw Error("imresize3: unknown method '" + m + "'",
                0, 0, "imresize3", "", "numkit:imresize3:method");
}

struct ContribRow { int firstIdx; std::vector<double> w; };  // firstIdx 0-indexed

std::vector<ContribRow>
buildContrib(size_t inLen, size_t outLen, double scale,
             const R3Kernel &k, bool aa)
{
    // `scale` is the user-supplied scale (NOT outLen/inLen) — MATLAB
    // distinguishes the two even when round(scale*inLen)==outLen.
    // For the size-vector overload the caller passes outLen/inLen.
    double kw  = k.width;
    double scl = 1.0;
    if (aa && scale < 1.0) { scl = scale; kw = k.width / scale; }
    const int P = static_cast<int>(std::ceil(kw)) + 2;

    std::vector<ContribRow> rows(outLen);
    for (size_t o = 0; o < outLen; ++o) {
        // MATLAB 1-indexed input center
        const double u = (double(o) + 1.0) / scale + 0.5 * (1.0 - 1.0 / scale);
        const int left = static_cast<int>(std::floor(u - kw / 2.0));  // 1-indexed
        ContribRow &row = rows[o];
        row.firstIdx = left - 1;            // convert to 0-indexed
        row.w.assign(P, 0.0);
        double sum = 0.0;
        for (int j = 0; j < P; ++j) {
            const double off = u - double(left + j);
            const double w   = scl * k.fn(scl * off);
            row.w[j] = w;
            sum += w;
        }
        if (sum != 0.0)
            for (auto &w : row.w) w /= sum;
    }
    return rows;
}

inline int mirrorIdx(int i, int N) {
    if (N <= 1) return 0;
    const int cycle = 2 * N;
    int m = ((i % cycle) + cycle) % cycle;
    if (m >= N) m = cycle - 1 - m;
    return m;
}

} // anonymous

// Lower-level entry: scale is the user-supplied (per-axis) scale.
// Pass scale=outLen/inLen for the size-vector branch.
Value imresize3_core(const Value &V,
                     size_t outR, size_t outC, size_t outD,
                     double scaleY, double scaleX, double scaleZ,
                     const std::string &method, bool antialias,
                     std::pmr::memory_resource *mr)
{
    const auto &dims = V.dims();
    if (dims.ndims() < 2)
        throw Error("imresize3: V must be a 3-D volume",
                    0, 0, "imresize3", "", "numkit:imresize3:rank");
    const size_t inR = dims.dim(0);
    const size_t inC = (dims.ndims() >= 2) ? dims.dim(1) : 1;
    const size_t inD = (dims.ndims() >= 3) ? dims.dim(2) : 1;
    if (V.numel() != inR * inC * inD)
        throw Error("imresize3: V must be a 3-D numeric volume",
                    0, 0, "imresize3", "", "numkit:imresize3:shape");

    const ValueType T = V.type();
    if (outR == 0 || outC == 0 || outD == 0)
        return Value::matrix3d(outR, outC, outD, T, mr);

    const R3Kernel k = parseR3Kernel(method);

    // Nearest-neighbour fast path. MATLAB's nearest uses the centered-
    // coord mapping (same u as the kernel path) then rounds.
    if (k.isNearest) {
        Value B = Value::matrix3d(outR, outC, outD, T, mr);
        auto uCoord = [](double o1, double sc) {
            return (o1) / sc + 0.5 * (1.0 - 1.0 / sc);
        };
        for (size_t z = 0; z < outD; ++z) {
            const double uz = uCoord(double(z) + 1.0, scaleZ);
            int zi = static_cast<int>(std::floor(uz + 0.5)) - 1;
            zi = mirrorIdx(zi, static_cast<int>(inD));
            for (size_t x = 0; x < outC; ++x) {
                const double ux = uCoord(double(x) + 1.0, scaleX);
                int xi = static_cast<int>(std::floor(ux + 0.5)) - 1;
                xi = mirrorIdx(xi, static_cast<int>(inC));
                for (size_t y = 0; y < outR; ++y) {
                    const double uy = uCoord(double(y) + 1.0, scaleY);
                    int yi = static_cast<int>(std::floor(uy + 0.5)) - 1;
                    yi = mirrorIdx(yi, static_cast<int>(inR));
                    const double v = V.elemAsDouble(
                        size_t(yi) + size_t(xi) * inR + size_t(zi) * inR * inC);
                    writeNative(B, y + x * outR + z * outR * outC, v, T);
                }
            }
        }
        return B;
    }

    auto cy = buildContrib(inR, outR, scaleY, k, antialias);
    auto cx = buildContrib(inC, outC, scaleX, k, antialias);
    auto cz = buildContrib(inD, outD, scaleZ, k, antialias);

    // Pass 1: along rows.   tmp1[outR × inC × inD]
    std::vector<double> tmp1(outR * inC * inD);
    for (size_t z = 0; z < inD; ++z) {
        for (size_t x = 0; x < inC; ++x) {
            for (size_t y = 0; y < outR; ++y) {
                const ContribRow &r = cy[y];
                double s = 0.0;
                for (size_t j = 0; j < r.w.size(); ++j) {
                    const int yi = mirrorIdx(r.firstIdx + int(j), int(inR));
                    s += r.w[j] *
                         V.elemAsDouble(size_t(yi) + x * inR + z * inR * inC);
                }
                tmp1[y + x * outR + z * outR * inC] = s;
            }
        }
    }

    // Pass 2: along cols.   tmp2[outR × outC × inD]
    std::vector<double> tmp2(outR * outC * inD);
    for (size_t z = 0; z < inD; ++z) {
        for (size_t x = 0; x < outC; ++x) {
            const ContribRow &r = cx[x];
            for (size_t y = 0; y < outR; ++y) {
                double s = 0.0;
                for (size_t j = 0; j < r.w.size(); ++j) {
                    const int xi = mirrorIdx(r.firstIdx + int(j), int(inC));
                    s += r.w[j] * tmp1[y + size_t(xi) * outR + z * outR * inC];
                }
                tmp2[y + x * outR + z * outR * outC] = s;
            }
        }
    }

    // Pass 3: along planes.  B [outR × outC × outD], with class clip.
    Value B = Value::matrix3d(outR, outC, outD, T, mr);
    for (size_t z = 0; z < outD; ++z) {
        const ContribRow &r = cz[z];
        for (size_t x = 0; x < outC; ++x) {
            for (size_t y = 0; y < outR; ++y) {
                double s = 0.0;
                for (size_t j = 0; j < r.w.size(); ++j) {
                    const int zi = mirrorIdx(r.firstIdx + int(j), int(inD));
                    s += r.w[j] *
                         tmp2[y + x * outR + size_t(zi) * outR * outC];
                }
                writeNative(B, y + x * outR + z * outR * outC, s, T);
            }
        }
    }
    return B;
}

Value imresize3(const Value &V,
                size_t outR, size_t outC, size_t outD,
                const std::string &method, bool antialias,
                std::pmr::memory_resource *mr)
{
    // Size-vector entry: use outLen/inLen as the per-axis scale.
    const auto &dims = V.dims();
    const size_t inR = dims.dim(0);
    const size_t inC = (dims.ndims() >= 2) ? dims.dim(1) : 1;
    const size_t inD = (dims.ndims() >= 3) ? dims.dim(2) : 1;
    return imresize3_core(V, outR, outC, outD,
        double(outR) / double(inR ? inR : 1),
        double(outC) / double(inC ? inC : 1),
        double(outD) / double(inD ? inD : 1),
        method, antialias, mr);
}

Value imresize3(const Value &V, double scale,
                const std::string &method, bool antialias,
                std::pmr::memory_resource *mr)
{
    if (!(scale > 0.0))
        throw Error("imresize3: scale must be > 0",
                    0, 0, "imresize3", "", "numkit:imresize3:scale");
    const auto &dims = V.dims();
    const size_t inR = dims.dim(0);
    const size_t inC = (dims.ndims() >= 2) ? dims.dim(1) : 1;
    const size_t inD = (dims.ndims() >= 3) ? dims.dim(2) : 1;
    return imresize3_core(V,
        static_cast<size_t>(std::round(scale * double(inR))),
        static_cast<size_t>(std::round(scale * double(inC))),
        static_cast<size_t>(std::round(scale * double(inD))),
        scale, scale, scale,
        method, antialias, mr);
}

} // namespace numkit::image
