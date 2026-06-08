// toolboxes/image/src/geom/geom_reg.cpp
//
// Register half of the image geom builtins: the CallContext wrappers
// delegating to the engine-free compute in geom.cpp. library.cpp
// forward-declares + registers these by name.
//
// Phase 2b compute/register split — see project_layering_refactor memory.

#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#endif
#include <cmath>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include <numkit/image/geom/geom.hpp>

#include <numkit/core/engine.hpp>   // CallContext, Span, ctx.engine->resource()
#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>
#include "geom_detail.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace numkit::image {

namespace detail {

static std::string argString(const Value &v) {
    if (!v.isChar() && !v.isString())
        throw Error("image geom: expected a string argument",
                    0, 0, "geom", "", "numkit:image:geom:type");
    return v.toString();
}

void imresize_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
                  CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("imresize: requires (A, scale_or_size [, method])",
                    0, 0, "imresize", "", "numkit:imresize:nargin");
    auto *mr = ctx.engine->resource();
    std::string method = "bilinear";
    if (args.size() >= 3 && !args[2].isEmpty()) method = argString(args[2]);
    if (args[1].numel() == 1) {
        outs[0] = imresize(args[0], args[1].toScalar(), method, mr);
    } else if (args[1].numel() == 2) {
        const size_t outH = size_t(args[1].elemAsDouble(0));
        const size_t outW = size_t(args[1].elemAsDouble(1));
        outs[0] = imresize(args[0], outH, outW, method, mr);
    } else {
        throw Error("imresize: 2nd arg must be a scalar scale "
                    "or a 2-element [outH outW]",
                    0, 0, "imresize", "", "numkit:imresize:size");
    }
}

void imcrop_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
                CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("imcrop: requires (A, [xmin ymin width height])",
                    0, 0, "imcrop", "", "numkit:imcrop:nargin");
    if (args[1].numel() < 4)
        throw Error("imcrop: rect must have 4 elements",
                    0, 0, "imcrop", "", "numkit:imcrop:rect");
    outs[0] = imcrop(args[0], args[1].elemAsDouble(0), args[1].elemAsDouble(1), args[1].elemAsDouble(2), args[1].elemAsDouble(3), ctx.engine->resource());
}

void imcrop3_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
                 CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("imcrop3: requires (V, cuboid)",
                    0, 0, "imcrop3", "", "numkit:imcrop3:nargin");
    outs[0] = imcrop3(args[0], args[1], ctx.engine->resource());
}

void imrotate_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
                  CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("imrotate: requires (A, angle [, method [, bbox]])",
                    0, 0, "imrotate", "", "numkit:imrotate:nargin");
    // MATLAB R2025b default method is 'nearest' (not bilinear), and the
    // 3rd argument may be EITHER a method or a bbox keyword:
    //   imrotate(A, angle, method [, bbox])   or   imrotate(A, angle, bbox)
    std::string method = "nearest";
    std::string bbox   = "loose";
    auto isBboxKw = [](const std::string &s) {
        return s == "loose" || s == "Loose" || s == "crop" || s == "Crop";
    };
    if (args.size() >= 3 && !args[2].isEmpty()) {
        const std::string s3 = argString(args[2]);
        if (isBboxKw(s3)) {
            bbox = s3;                       // imrotate(A, angle, bbox)
        } else {
            method = s3;                     // imrotate(A, angle, method [, bbox])
            if (args.size() >= 4 && !args[3].isEmpty()) bbox = argString(args[3]);
        }
    }
    outs[0] = imrotate(args[0], args[1].toScalar(), method, bbox, ctx.engine->resource());
}

void imtranslate_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
                     CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("imtranslate: requires (A, [dx dy])",
                    0, 0, "imtranslate", "", "numkit:imtranslate:nargin");
    if (args[1].numel() < 2)
        throw Error("imtranslate: vector must have 2 elements",
                    0, 0, "imtranslate", "", "numkit:imtranslate:vec");
    outs[0] = imtranslate(args[0], args[1].elemAsDouble(0), args[1].elemAsDouble(1), ctx.engine->resource());
}

void axes2pix_reg(Span<const Value> args, size_t /*nargout*/,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("axes2pix: requires (n, extent, axesCoord)",
                    0, 0, "axes2pix", "", "numkit:axes2pix:nargin");
    const double n = args[0].toScalar();
    outs[0] = axes2pix(n, args[1], args[2], ctx.engine->resource());
}

void impyramid_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
                   CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("impyramid: requires (A, type)", 0, 0, "impyramid", "",
                    "numkit:impyramid:nargin");
    const std::string type = argString(args[1]);
    outs[0] = impyramid(args[0], type, ctx.engine->resource());
}

// ── imrotate3 (3-D volume rotation, MATLAB R2025b imrotate3.m) ───────
//
// Rotate a 3-D numeric volume by `angle` degrees CCW (right-hand
// rule) around the axis vector W = [Wx Wy Wz] passing through the
// centre of the volume. Output size:
//
//   bbox = "loose"  (default)
//     outDim[j] = ceil( sum_k inDim[k] * |R[j][k]| )
//   bbox = "crop"
//     outDim = inDim
//
// Inverse warp:
//   (xs, ys, zs) = inCentre + R * ((xo, yo, zo) - outCentre)
//
// MATLAB's spatial X / Y / Z = col / row / page. Standard Rodrigues
// rotation matrix is used directly: a right-hand-rule CCW rotation
// around W of the centred output delta maps to the source delta.
//
// References:
//   * Rodrigues 1840 — rotation matrix from axis-angle.
//   * Keys 1981 — Catmull-Rom cubic (a = −0.5).
namespace {

struct Rot3 { double r[9]; };          // row-major 3 × 3

Rot3 rodrigues(double Wx, double Wy, double Wz, double theta)
{
    const double n = std::sqrt(Wx*Wx + Wy*Wy + Wz*Wz);
    if (n < 1e-15)
        throw Error("imrotate3: rotation axis must be non-zero",
                    0, 0, "imrotate3", "", "numkit:imrotate3:axis");
    const double ux = Wx / n, uy = Wy / n, uz = Wz / n;
    const double c = std::cos(theta), s = std::sin(theta), t = 1.0 - c;
    Rot3 R;
    R.r[0] = t*ux*ux + c;    R.r[1] = t*ux*uy - s*uz; R.r[2] = t*ux*uz + s*uy;
    R.r[3] = t*uy*ux + s*uz; R.r[4] = t*uy*uy + c;    R.r[5] = t*uy*uz - s*ux;
    R.r[6] = t*uz*ux - s*uy; R.r[7] = t*uz*uy + s*ux; R.r[8] = t*uz*uz + c;
    return R;
}

inline double cubic_keys(double x)
{
    const double a = std::fabs(x);
    if (a < 1.0) return ((1.5 * a - 2.5) * a * a + 1.0);
    if (a < 2.0) return (((-0.5 * a + 2.5) * a - 4.0) * a + 2.0);
    return 0.0;
}

inline double getV3(const Value &V, size_t H, size_t W, size_t D,
                    int xi, int yi, int zi, double fill)
{
    // 1-indexed spatial (xi, yi, zi). Out-of-bounds → fill.
    if (xi < 1 || xi > int(W) || yi < 1 || yi > int(H) ||
        zi < 1 || zi > int(D))
        return fill;
    return V.elemAsDouble(size_t(yi - 1)
                        + size_t(xi - 1) * H
                        + size_t(zi - 1) * H * W);
}

double samp_nearest3(const Value &V, size_t H, size_t W, size_t D,
                     double xs, double ys, double zs, double fill)
{
    const int xi = static_cast<int>(std::floor(xs + 0.5));
    const int yi = static_cast<int>(std::floor(ys + 0.5));
    const int zi = static_cast<int>(std::floor(zs + 0.5));
    return getV3(V, H, W, D, xi, yi, zi, fill);
}

double samp_trilinear(const Value &V, size_t H, size_t W, size_t D,
                      double xs, double ys, double zs, double fill)
{
    const int x0 = static_cast<int>(std::floor(xs));
    const int y0 = static_cast<int>(std::floor(ys));
    const int z0 = static_cast<int>(std::floor(zs));
    const double fx = xs - double(x0);
    const double fy = ys - double(y0);
    const double fz = zs - double(z0);
    const double c000 = getV3(V, H, W, D, x0,     y0,     z0,     fill);
    const double c100 = getV3(V, H, W, D, x0 + 1, y0,     z0,     fill);
    const double c010 = getV3(V, H, W, D, x0,     y0 + 1, z0,     fill);
    const double c110 = getV3(V, H, W, D, x0 + 1, y0 + 1, z0,     fill);
    const double c001 = getV3(V, H, W, D, x0,     y0,     z0 + 1, fill);
    const double c101 = getV3(V, H, W, D, x0 + 1, y0,     z0 + 1, fill);
    const double c011 = getV3(V, H, W, D, x0,     y0 + 1, z0 + 1, fill);
    const double c111 = getV3(V, H, W, D, x0 + 1, y0 + 1, z0 + 1, fill);
    const double c00 = c000 * (1 - fx) + c100 * fx;
    const double c10 = c010 * (1 - fx) + c110 * fx;
    const double c01 = c001 * (1 - fx) + c101 * fx;
    const double c11 = c011 * (1 - fx) + c111 * fx;
    const double c0  = c00  * (1 - fy) + c10  * fy;
    const double c1  = c01  * (1 - fy) + c11  * fy;
    return c0 * (1 - fz) + c1 * fz;
}

double samp_tricubic(const Value &V, size_t H, size_t W, size_t D,
                     double xs, double ys, double zs, double fill)
{
    const int x0 = static_cast<int>(std::floor(xs));
    const int y0 = static_cast<int>(std::floor(ys));
    const int z0 = static_cast<int>(std::floor(zs));
    const double fx = xs - double(x0);
    const double fy = ys - double(y0);
    const double fz = zs - double(z0);
    double wx[4], wy[4], wz[4];
    for (int k = -1; k <= 2; ++k) {
        wx[k + 1] = cubic_keys(double(k) - fx);
        wy[k + 1] = cubic_keys(double(k) - fy);
        wz[k + 1] = cubic_keys(double(k) - fz);
    }
    double sum = 0.0;
    for (int kz = -1; kz <= 2; ++kz)
        for (int kx = -1; kx <= 2; ++kx) {
            const double wxz = wx[kx + 1] * wz[kz + 1];
            for (int ky = -1; ky <= 2; ++ky)
                sum += wy[ky + 1] * wxz *
                       getV3(V, H, W, D, x0 + kx, y0 + ky, z0 + kz, fill);
        }
    return sum;
}

} // anonymous (imrotate3 helpers)

Value imrotate3(const Value &V, double angle_deg,
                double Wx, double Wy, double Wz,
                const std::string &method, const std::string &bbox,
                double fill,
                std::pmr::memory_resource *mr)
{
    const auto &dims = V.dims();
    if (dims.ndims() < 2)
        throw Error("imrotate3: V must be a 3-D volume",
                    0, 0, "imrotate3", "", "numkit:imrotate3:rank");
    const size_t H = dims.dim(0);
    const size_t W = dims.dim(1);
    const size_t D = (dims.ndims() >= 3) ? dims.dim(2) : 1;
    if (V.numel() != H * W * D)
        throw Error("imrotate3: V must be a 3-D numeric volume",
                    0, 0, "imrotate3", "", "numkit:imrotate3:shape");

    const ValueType T = V.type();
    const double theta = angle_deg * M_PI / 180.0;
    const Rot3 R = rodrigues(Wx, Wy, Wz, theta);

    auto snap = [](double v) {
        const double r = std::round(v);
        return (std::fabs(v - r) < 1e-9) ? r : v;
    };

    size_t outH, outW, outD;
    if (bbox == "crop" || bbox == "Crop") {
        outH = H; outW = W; outD = D;
    } else if (bbox == "loose" || bbox == "Loose" || bbox.empty()) {
        const double Wd = double(W), Hd = double(H), Dd = double(D);
        // outDim[axis] = sum_k inDim[k] * |R[axis][k]|
        const double xe = Wd*std::fabs(R.r[0]) + Hd*std::fabs(R.r[1]) + Dd*std::fabs(R.r[2]);
        const double ye = Wd*std::fabs(R.r[3]) + Hd*std::fabs(R.r[4]) + Dd*std::fabs(R.r[5]);
        const double ze = Wd*std::fabs(R.r[6]) + Hd*std::fabs(R.r[7]) + Dd*std::fabs(R.r[8]);
        outW = std::max(size_t(1), size_t(std::ceil(snap(xe))));
        outH = std::max(size_t(1), size_t(std::ceil(snap(ye))));
        outD = std::max(size_t(1), size_t(std::ceil(snap(ze))));
    } else {
        throw Error("imrotate3: bbox must be 'loose' or 'crop'",
                    0, 0, "imrotate3", "", "numkit:imrotate3:bbox");
    }

    Value B = (outD == 1) ? Value::matrix(outH, outW, T, mr)
                          : Value::matrix3d(outH, outW, outD, T, mr);

    const double cxOut = (double(outW) + 1.0) * 0.5;
    const double cyOut = (double(outH) + 1.0) * 0.5;
    const double czOut = (double(outD) + 1.0) * 0.5;
    const double cxIn  = (double(W) + 1.0) * 0.5;
    const double cyIn  = (double(H) + 1.0) * 0.5;
    const double czIn  = (double(D) + 1.0) * 0.5;

    int code = 1;  // 0 = nearest, 1 = linear, 2 = cubic
    if      (method == "nearest" || method == "Nearest") code = 0;
    else if (method == "linear"  || method == "Linear" ||
             method == "bilinear")                       code = 1;
    else if (method == "cubic"   || method == "Cubic")   code = 2;
    else
        throw Error("imrotate3: method must be 'nearest', 'linear', or 'cubic'",
                    0, 0, "imrotate3", "", "numkit:imrotate3:method");

    for (size_t p = 1; p <= outD; ++p) {
        const double dzo = double(p) - czOut;
        for (size_t c = 1; c <= outW; ++c) {
            const double dxo = double(c) - cxOut;
            for (size_t r = 1; r <= outH; ++r) {
                const double dyo = double(r) - cyOut;
                const double dxi = R.r[0]*dxo + R.r[1]*dyo + R.r[2]*dzo;
                const double dyi = R.r[3]*dxo + R.r[4]*dyo + R.r[5]*dzo;
                const double dzi = R.r[6]*dxo + R.r[7]*dyo + R.r[8]*dzo;
                const double xs = cxIn + dxi;
                const double ys = cyIn + dyi;
                const double zs = czIn + dzi;
                double v;
                switch (code) {
                    case 0: v = samp_nearest3 (V, H, W, D, xs, ys, zs, fill); break;
                    case 1: v = samp_trilinear(V, H, W, D, xs, ys, zs, fill); break;
                    default:v = samp_tricubic (V, H, W, D, xs, ys, zs, fill); break;
                }
                writeNative(B, (r - 1) + (c - 1) * outH + (p - 1) * outH * outW, v, T);
            }
        }
    }
    return B;
}

// imresize3 — MATLAB:
//   B = imresize3(V, scale)
//   B = imresize3(V, [r c d])
//   B = imresize3(___, method)
//   B = imresize3(___, Name=Value)         Method / Antialiasing / Scale / OutputSize
void imresize3_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
                   CallContext &ctx)
{
    if (args.size() < 1)
        throw Error("imresize3: requires (V, scale_or_size [, method] [, NV])",
                    0, 0, "imresize3", "", "numkit:imresize3:nargin");
    auto *mr = ctx.engine->resource();
    const Value &V = args[0];

    // Defaults
    std::string method = "cubic";
    bool antialiasSet  = false;
    bool antialias     = true;        // initial — recomputed once scale known
    bool haveScaleNum  = false;       // scalar Scale OR per-axis Scale
    double scaleScalar = 1.0;
    std::vector<double> scaleVec3;    // length 3 if 3-vec scale provided
    bool haveSize3     = false;
    size_t outRows = 0, outCols = 0, outDeps = 0;

    // Positional: scale/size, then optional method (string).
    size_t i = 1;
    if (args.size() >= 2 && !args[1].isEmpty()
        && !args[1].isChar() && !args[1].isString()) {
        const Value &s = args[1];
        if (s.numel() == 1) {
            haveScaleNum = true; scaleScalar = s.toScalar();
        } else if (s.numel() == 3) {
            haveSize3 = true;
            outRows = static_cast<size_t>(s.elemAsDouble(0));
            outCols = static_cast<size_t>(s.elemAsDouble(1));
            outDeps = static_cast<size_t>(s.elemAsDouble(2));
        } else {
            throw Error("imresize3: 2nd arg must be scalar scale or "
                        "[numrows numcols numplanes]",
                        0, 0, "imresize3", "", "numkit:imresize3:size");
        }
        i = 2;
    }
    if (i < args.size() && (args[i].isChar() || args[i].isString())) {
        const std::string s = argString(args[i]);
        // First trailing string is the method positional. Subsequent
        // strings are NV-pair names handled in the loop below.
        // Heuristic: only treat as positional method if it's NOT a
        // known NV-pair name.
        const bool isNV = (s == "Antialiasing" || s == "antialiasing" ||
                           s == "Method"       || s == "method" ||
                           s == "OutputSize"   || s == "outputsize" ||
                           s == "Scale"        || s == "scale");
        if (!isNV) { method = s; ++i; }
    }

    // NV pairs.
    while (i + 1 < args.size()) {
        const std::string name = argString(args[i]);
        const Value &val = args[i + 1];
        if (name == "Antialiasing" || name == "antialiasing") {
            antialiasSet = true;
            antialias = (val.toScalar() != 0.0);
        } else if (name == "Method" || name == "method") {
            method = argString(val);
        } else if (name == "OutputSize" || name == "outputsize") {
            if (val.numel() != 3)
                throw Error("imresize3: OutputSize must be a 3-element vector",
                            0, 0, "imresize3", "", "numkit:imresize3:size");
            haveSize3 = true;
            outRows = static_cast<size_t>(val.elemAsDouble(0));
            outCols = static_cast<size_t>(val.elemAsDouble(1));
            outDeps = static_cast<size_t>(val.elemAsDouble(2));
        } else if (name == "Scale" || name == "scale") {
            if (val.numel() == 1) {
                haveScaleNum = true; scaleScalar = val.toScalar();
            } else if (val.numel() == 3) {
                scaleVec3.assign({val.elemAsDouble(0),
                                  val.elemAsDouble(1),
                                  val.elemAsDouble(2)});
            } else {
                throw Error("imresize3: Scale must be a positive number "
                            "or a 3-element vector",
                            0, 0, "imresize3", "", "numkit:imresize3:scaleNV");
            }
        } else {
            throw Error("imresize3: unknown name-value parameter '"
                        + name + "'",
                        0, 0, "imresize3", "", "numkit:imresize3:nv");
        }
        i += 2;
    }

    // Resolve output dims.
    const auto &d = V.dims();
    const size_t inR = d.dim(0);
    const size_t inC = (d.ndims() >= 2) ? d.dim(1) : 1;
    const size_t inD = (d.ndims() >= 3) ? d.dim(2) : 1;
    if (!haveSize3) {
        double sR, sC, sD;
        if (!scaleVec3.empty()) {
            sR = scaleVec3[0]; sC = scaleVec3[1]; sD = scaleVec3[2];
        } else if (haveScaleNum) {
            sR = sC = sD = scaleScalar;
        } else {
            throw Error("imresize3: must specify scale, size, or "
                        "Scale/OutputSize NV",
                        0, 0, "imresize3", "", "numkit:imresize3:noSize");
        }
        if (!(sR > 0 && sC > 0 && sD > 0))
            throw Error("imresize3: scale factors must be > 0",
                        0, 0, "imresize3", "", "numkit:imresize3:scale");
        outRows = static_cast<size_t>(std::round(sR * double(inR)));
        outCols = static_cast<size_t>(std::round(sC * double(inC)));
        outDeps = static_cast<size_t>(std::round(sD * double(inD)));
    }

    // Per-axis scale that drives the u formula. MATLAB uses the
    // user-supplied scale when given (scalar or 3-vec), and out/in
    // when only a size vector is given — these produce DIFFERENT
    // values even when round(scale * inLen) == outLen.
    double scY, scX, scZ;
    if (haveSize3 && scaleVec3.empty() && !haveScaleNum) {
        scY = double(outRows) / double(inR ? inR : 1);
        scX = double(outCols) / double(inC ? inC : 1);
        scZ = double(outDeps) / double(inD ? inD : 1);
    } else if (!scaleVec3.empty()) {
        scY = scaleVec3[0]; scX = scaleVec3[1]; scZ = scaleVec3[2];
    } else {  // scalar scale (positional or 'Scale' NV)
        scY = scX = scZ = scaleScalar;
    }

    // Resolve antialiasing default = true if any output dim < input dim
    if (!antialiasSet) {
        antialias = (outRows < inR) || (outCols < inC) || (outDeps < inD);
    }

    outs[0] = imresize3_core(V, outRows, outCols, outDeps,
                             scY, scX, scZ, method, antialias, mr);
}

// imrotate3 — MATLAB:
//   B = imrotate3(V, angle, W)
//   B = imrotate3(V, angle, W, method)
//   B = imrotate3(V, angle, W, method, bbox)
//   B = imrotate3(___, "FillValues", v)
void imrotate3_reg(Span<const Value> args, size_t /*nargout*/,
                   Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("imrotate3: requires (V, angle, W [, method [, bbox]])",
                    0, 0, "imrotate3", "", "numkit:imrotate3:nargin");
    auto *mr = ctx.engine->resource();
    const double angle = args[1].toScalar();
    if (args[2].numel() != 3)
        throw Error("imrotate3: axis W must be a 3-element vector",
                    0, 0, "imrotate3", "", "numkit:imrotate3:axis");
    const double Wx = args[2].elemAsDouble(0);
    const double Wy = args[2].elemAsDouble(1);
    const double Wz = args[2].elemAsDouble(2);

    std::string method = "linear";
    std::string bbox   = "loose";
    double fill        = 0.0;
    size_t i = 3;
    if (i < args.size() && (args[i].isChar() || args[i].isString())) {
        const std::string s = argString(args[i]);
        if (s != "FillValues" && s != "fillvalues") { method = s; ++i; }
    }
    if (i < args.size() && (args[i].isChar() || args[i].isString())) {
        const std::string s = argString(args[i]);
        if (s != "FillValues" && s != "fillvalues") { bbox = s; ++i; }
    }
    while (i + 1 < args.size()) {
        const std::string name = argString(args[i]);
        const Value &val = args[i + 1];
        if (name == "FillValues" || name == "fillvalues") {
            fill = val.toScalar();
        } else {
            throw Error("imrotate3: unknown name-value parameter '" + name + "'",
                        0, 0, "imrotate3", "", "numkit:imrotate3:nv");
        }
        i += 2;
    }
    outs[0] = imrotate3(args[0], angle, Wx, Wy, Wz, method, bbox, fill, mr);
}

} // namespace detail

} // namespace numkit::image
