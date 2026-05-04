// libs/image/src/color/color.cpp
//
// Colour-space conversions. Portable scalar implementation; SIMD
// optimisation deferred to a later phase.

#include <numkit/image/color/color.hpp>
#include <numkit/image/type_convert/type_convert.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include <algorithm>
#include <array>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#include <cmath>
#include <cstdint>
#include <cstring>
#include <utility>
#include <vector>

namespace numkit::image {

namespace {

// Detect input layout: returns (H, W, P) where P is 3, and a triple of
// strides (s_pix, s_chan) so that x[pix_idx, chan] = data[pix_idx*s_pix
// + chan*s_chan]. Supports H×W×3 (s_pix=1, s_chan=H*W) and N×3 colormap
// (s_pix=1, s_chan=N).
struct Layout {
    size_t H, W, npix, s_pix, s_chan;
    bool is_3d;
};

Layout detect_layout(const Value &x, const char *fn) {
    const auto &d = x.dims();
    Layout lay{};
    if (d.is3D() && d.pages() == 3) {
        lay.H = d.rows(); lay.W = d.cols();
        lay.npix = lay.H * lay.W;
        lay.s_pix = 1;
        lay.s_chan = lay.npix;
        lay.is_3d = true;
    } else if (!d.is3D() && d.cols() == 3) {
        lay.H = d.rows(); lay.W = 1;
        lay.npix = d.rows();
        lay.s_pix = 1;
        lay.s_chan = lay.npix;
        lay.is_3d = false;
    } else {
        throw Error(std::string(fn) + ": input must be H×W×3 or N×3",
                    0, 0, fn, "", std::string("m:") + fn + ":size");
    }
    return lay;
}

// Read a [0,1] float from an integer image element with class scaling.
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

Value alloc_out_double(std::pmr::memory_resource *mr, const Layout &lay) {
    if (lay.is_3d) return Value::matrix3d(lay.H, lay.W, 3, ValueType::DOUBLE, mr);
    return Value::matrix(lay.H, 3, ValueType::DOUBLE, mr);
}

// Generic per-pixel transform.
template <typename Op>
Value pixel_transform(std::pmr::memory_resource *mr, const Value &x, const char *fn, Op op) {
    auto lay = detect_layout(x, fn);
    Value out = alloc_out_double(mr, lay);
    if (lay.npix == 0) return out;
    double *od = out.doubleDataMut();

    for (size_t p = 0; p < lay.npix; ++p) {
        const double a = element_to_unit(x, p + 0 * lay.s_chan);
        const double b = element_to_unit(x, p + 1 * lay.s_chan);
        const double c = element_to_unit(x, p + 2 * lay.s_chan);
        std::array<double, 3> r = op(a, b, c);
        od[p + 0 * lay.s_chan] = r[0];
        od[p + 1 * lay.s_chan] = r[1];
        od[p + 2 * lay.s_chan] = r[2];
    }
    return out;
}

// Same as pixel_transform but without the unit-scale conversion: input
// is read as raw doubles (used for HSV / YCbCr / Lab → RGB where the
// input is already in the appropriate domain).
template <typename Op>
Value pixel_transform_raw(std::pmr::memory_resource *mr, const Value &x, const char *fn, Op op) {
    auto lay = detect_layout(x, fn);
    Value out = alloc_out_double(mr, lay);
    if (lay.npix == 0) return out;
    double *od = out.doubleDataMut();

    for (size_t p = 0; p < lay.npix; ++p) {
        const double a = x.elemAsDouble(p + 0 * lay.s_chan);
        const double b = x.elemAsDouble(p + 1 * lay.s_chan);
        const double c = x.elemAsDouble(p + 2 * lay.s_chan);
        std::array<double, 3> r = op(a, b, c);
        od[p + 0 * lay.s_chan] = r[0];
        od[p + 1 * lay.s_chan] = r[1];
        od[p + 2 * lay.s_chan] = r[2];
    }
    return out;
}

} // anonymous

// ════════════════════════════════════════════════════════════════════
// RGB ↔ HSV  (MATLAB convention: all channels in [0, 1])
// ════════════════════════════════════════════════════════════════════

Value rgb2hsv(std::pmr::memory_resource *mr, const Value &x) {
    return pixel_transform(mr, x, "rgb2hsv", [](double r, double g, double b) {
        const double cmax = std::max({r, g, b});
        const double cmin = std::min({r, g, b});
        const double delta = cmax - cmin;
        double h = 0.0;
        if (delta > 0.0) {
            if (cmax == r)      h = std::fmod((g - b) / delta, 6.0) / 6.0;
            else if (cmax == g) h = ((b - r) / delta + 2.0) / 6.0;
            else                h = ((r - g) / delta + 4.0) / 6.0;
            if (h < 0.0) h += 1.0;
        }
        const double s = (cmax == 0.0) ? 0.0 : delta / cmax;
        return std::array<double, 3>{h, s, cmax};
    });
}

Value hsv2rgb(std::pmr::memory_resource *mr, const Value &x) {
    return pixel_transform_raw(mr, x, "hsv2rgb", [](double h, double s, double v) {
        // Wrap h into [0, 1).
        h = h - std::floor(h);
        const double H = h * 6.0;
        const int    I = static_cast<int>(std::floor(H));
        const double f = H - I;
        const double p = v * (1.0 - s);
        const double q = v * (1.0 - s * f);
        const double t = v * (1.0 - s * (1.0 - f));
        switch (I % 6) {
            case 0: return std::array<double, 3>{v, t, p};
            case 1: return std::array<double, 3>{q, v, p};
            case 2: return std::array<double, 3>{p, v, t};
            case 3: return std::array<double, 3>{p, q, v};
            case 4: return std::array<double, 3>{t, p, v};
            default:return std::array<double, 3>{v, p, q};
        }
    });
}

// ════════════════════════════════════════════════════════════════════
// RGB ↔ YCbCr (ITU-R BT.601, the MATLAB default)
// Input RGB in [0, 1]; output YCbCr scaled to [16/255 .. 235/255] for
// Y, [16/255 .. 240/255] for Cb/Cr (this matches MATLAB rgb2ycbcr's
// output in DOUBLE class).
// ════════════════════════════════════════════════════════════════════

// ════════════════════════════════════════════════════════════════════
// RGB ↔ NTSC (YIQ, 3-significant-figure matrix from Wikipedia/MATLAB)
// ════════════════════════════════════════════════════════════════════

Value rgb2ntsc(std::pmr::memory_resource *mr, const Value &x) {
    return pixel_transform(mr, x, "rgb2ntsc", [](double r, double g, double b) {
        const double y = 0.299 * r + 0.587 * g + 0.114 * b;
        const double i = 0.596 * r - 0.274 * g - 0.322 * b;
        const double q = 0.211 * r - 0.523 * g + 0.312 * b;
        return std::array<double, 3>{y, i, q};
    });
}

Value ntsc2rgb(std::pmr::memory_resource *mr, const Value &x) {
    // Octave-image's exact inverse (5 sig figs); built so that
    // rgb2ntsc/ntsc2rgb round-trip back to the input. Negatives are
    // clipped to 0 and per-pixel overshoot above 1 is scaled down by
    // the row max — Matlab compatibility tweak from ntsc2rgb.m.
    return pixel_transform_raw(mr, x, "ntsc2rgb", [](double y, double i, double q) {
        double r = y + 0.95617 * i + 0.62143 * q;
        double g = y - 0.27269 * i - 0.64681 * q;
        double b = y - 1.10374 * i + 1.70062 * q;
        if (r < 0.0) r = 0.0;
        if (g < 0.0) g = 0.0;
        if (b < 0.0) b = 0.0;
        const double m = std::max({r, g, b});
        if (m > 1.0) { r /= m; g /= m; b /= m; }
        return std::array<double, 3>{r, g, b};
    });
}

Value rgb2ycbcr(std::pmr::memory_resource *mr, const Value &x) {
    return pixel_transform(mr, x, "rgb2ycbcr", [](double r, double g, double b) {
        // BT.601 conversion (8-bit-style numbers, normalised by 255).
        const double y  = ( 65.481 * r + 128.553 * g +  24.966 * b +  16.0) / 255.0;
        const double cb = (-37.797 * r -  74.203 * g + 112.0   * b + 128.0) / 255.0;
        const double cr = (112.0   * r -  93.786 * g -  18.214 * b + 128.0) / 255.0;
        return std::array<double, 3>{y, cb, cr};
    });
}

Value ycbcr2rgb(std::pmr::memory_resource *mr, const Value &x) {
    return pixel_transform_raw(mr, x, "ycbcr2rgb", [](double y, double cb, double cr) {
        // Inverse BT.601 (matches MATLAB ycbcr2rgb on DOUBLE input).
        const double Y  = y  * 255.0;
        const double Cb = cb * 255.0;
        const double Cr = cr * 255.0;
        const double r = (   298.082 * Y +    0.0   * (Cb - 128.0) + 408.583 * (Cr - 128.0)) / 255.0 / 255.0 - 222.921 / 255.0;
        const double g = (   298.082 * Y -  100.291 * (Cb - 128.0) - 208.120 * (Cr - 128.0)) / 255.0 / 255.0 + 135.576 / 255.0;
        const double bo= (   298.082 * Y +  516.412 * (Cb - 128.0) +    0.0  * (Cr - 128.0)) / 255.0 / 255.0 - 276.836 / 255.0;
        // The above factoring isn't pretty — explicit inverse matrix:
        //   R = 1.164*(Y-16) + 1.596*(Cr-128)
        //   G = 1.164*(Y-16) - 0.392*(Cb-128) - 0.813*(Cr-128)
        //   B = 1.164*(Y-16) + 2.017*(Cb-128)
        // Use that directly for clarity / accuracy:
        const double Ys = 1.16438356 * (Y - 16.0);
        const double Cbs = Cb - 128.0;
        const double Crs = Cr - 128.0;
        double R = (Ys                + 1.59602715 * Crs) / 255.0;
        double G = (Ys - 0.39176229*Cbs - 0.81296765 * Crs) / 255.0;
        double B = (Ys + 2.01723214 * Cbs                 ) / 255.0;
        // Clip [0, 1].
        R = std::clamp(R, 0.0, 1.0);
        G = std::clamp(G, 0.0, 1.0);
        B = std::clamp(B, 0.0, 1.0);
        (void)r; (void)g; (void)bo;  // silence unused warnings
        return std::array<double, 3>{R, G, B};
    });
}

// ════════════════════════════════════════════════════════════════════
// RGB ↔ XYZ (sRGB → CIE XYZ, D65 white point)
// MATLAB convention applies sRGB gamma decode first.
// ════════════════════════════════════════════════════════════════════

namespace {
inline double srgb_decode(double c) {
    // sRGB gamma → linear.
    return (c <= 0.04045) ? (c / 12.92) : std::pow((c + 0.055) / 1.055, 2.4);
}
inline double srgb_encode(double c) {
    // Linear → sRGB gamma.
    return (c <= 0.0031308) ? (12.92 * c) : (1.055 * std::pow(c, 1.0 / 2.4) - 0.055);
}
} // anonymous

Value rgb2xyz(std::pmr::memory_resource *mr, const Value &x) {
    return pixel_transform(mr, x, "rgb2xyz", [](double r, double g, double b) {
        // sRGB → linear.
        const double Rl = srgb_decode(r);
        const double Gl = srgb_decode(g);
        const double Bl = srgb_decode(b);
        // sRGB / D65 matrix (CIE).
        const double X = 0.4124564 * Rl + 0.3575761 * Gl + 0.1804375 * Bl;
        const double Y = 0.2126729 * Rl + 0.7151522 * Gl + 0.0721750 * Bl;
        const double Z = 0.0193339 * Rl + 0.1191920 * Gl + 0.9503041 * Bl;
        return std::array<double, 3>{X, Y, Z};
    });
}

Value xyz2rgb(std::pmr::memory_resource *mr, const Value &x) {
    return pixel_transform_raw(mr, x, "xyz2rgb", [](double X, double Y, double Z) {
        // Inverse matrix (sRGB / D65).
        const double Rl =  3.2404542 * X - 1.5371385 * Y - 0.4985314 * Z;
        const double Gl = -0.9692660 * X + 1.8760108 * Y + 0.0415560 * Z;
        const double Bl =  0.0556434 * X - 0.2040259 * Y + 1.0572252 * Z;
        double R = srgb_encode(std::clamp(Rl, 0.0, 1.0));
        double G = srgb_encode(std::clamp(Gl, 0.0, 1.0));
        double B = srgb_encode(std::clamp(Bl, 0.0, 1.0));
        return std::array<double, 3>{R, G, B};
    });
}

// ════════════════════════════════════════════════════════════════════
// XYZ ↔ Lab (CIELAB, D65 reference white)
// ════════════════════════════════════════════════════════════════════

namespace {
constexpr double XYZ_Xn = 0.95047;  // D65
constexpr double XYZ_Yn = 1.00000;
constexpr double XYZ_Zn = 1.08883;
inline double f_lab(double t) {
    constexpr double delta = 6.0 / 29.0;
    constexpr double delta3 = delta * delta * delta;
    if (t > delta3) return std::cbrt(t);
    return t / (3.0 * delta * delta) + 4.0 / 29.0;
}
inline double finv_lab(double t) {
    constexpr double delta = 6.0 / 29.0;
    if (t > delta) return t * t * t;
    return 3.0 * delta * delta * (t - 4.0 / 29.0);
}
} // anonymous

Value xyz2lab(std::pmr::memory_resource *mr, const Value &x) {
    return pixel_transform_raw(mr, x, "xyz2lab", [](double X, double Y, double Z) {
        const double fx = f_lab(X / XYZ_Xn);
        const double fy = f_lab(Y / XYZ_Yn);
        const double fz = f_lab(Z / XYZ_Zn);
        const double L  = 116.0 * fy - 16.0;
        const double a  = 500.0 * (fx - fy);
        const double b  = 200.0 * (fy - fz);
        return std::array<double, 3>{L, a, b};
    });
}

Value lab2xyz(std::pmr::memory_resource *mr, const Value &x) {
    return pixel_transform_raw(mr, x, "lab2xyz", [](double L, double a, double b) {
        const double fy = (L + 16.0) / 116.0;
        const double fx = fy + a / 500.0;
        const double fz = fy - b / 200.0;
        const double X = XYZ_Xn * finv_lab(fx);
        const double Y = XYZ_Yn * finv_lab(fy);
        const double Z = XYZ_Zn * finv_lab(fz);
        return std::array<double, 3>{X, Y, Z};
    });
}

Value rgb2lab(std::pmr::memory_resource *mr, const Value &x) {
    Value xyz = rgb2xyz(mr, x);
    return xyz2lab(mr, xyz);
}

Value lab2rgb(std::pmr::memory_resource *mr, const Value &x) {
    Value xyz = lab2xyz(mr, x);
    return xyz2rgb(mr, xyz);
}

// ════════════════════════════════════════════════════════════════════
// imsplit — split H×W×P → P planes of H×W
// ════════════════════════════════════════════════════════════════════
//
// Numkit packs an H×W×P volume column-major across all three dims:
// element (r, c, p) sits at linear offset p·H·W + c·H + r. Splitting
// is therefore a contiguous H·W copy per channel — no transpose, no
// stride trickery.

namespace {

inline void copy_plane(const Value &src, size_t plane,
                       size_t H, size_t W, Value &dst)
{
    const size_t N = H * W;
    const size_t off = plane * N;
    const ValueType T = src.type();
    switch (T) {
        case ValueType::DOUBLE:
            std::memcpy(dst.doubleDataMut(),
                        src.doubleData() + off, N * sizeof(double));
            break;
        case ValueType::SINGLE:
            std::memcpy(dst.singleDataMut(),
                        src.singleData() + off, N * sizeof(float));
            break;
        case ValueType::UINT8:
            std::memcpy(dst.uint8DataMut(),
                        src.uint8Data() + off, N);
            break;
        case ValueType::UINT16:
            std::memcpy(dst.uint16DataMut(),
                        src.uint16Data() + off, N * sizeof(std::uint16_t));
            break;
        case ValueType::INT16:
            std::memcpy(dst.int16DataMut(),
                        src.int16Data() + off, N * sizeof(std::int16_t));
            break;
        case ValueType::LOGICAL:
            std::memcpy(dst.logicalDataMut(),
                        src.logicalData() + off, N);
            break;
        default:
            // Generic fallback — re-read elements.
            for (size_t i = 0; i < N; ++i) {
                const double v = src.elemAsDouble(off + i);
                dst.doubleDataMut()[i] = v;
            }
    }
}

} // anonymous

void imsplit(std::pmr::memory_resource *mr,
             const Value &I, std::vector<Value> &planes)
{
    const auto &d = I.dims();
    const size_t H = d.rows();
    const size_t W = d.cols();
    const size_t P = d.is3D() ? d.pages() : 1;
    planes.clear();
    planes.reserve(P);
    for (size_t p = 0; p < P; ++p) {
        Value plane = Value::matrix(H, W, I.type(), mr);
        copy_plane(I, p, H, W, plane);
        planes.push_back(std::move(plane));
    }
}

namespace {

// Normalise an RGB-vector argument to an N×3 row-of-triplets layout.
// Accepts 3-element vectors of any orientation (1×3, 3×1, or 3-D
// linear) and N×3 matrices. Returns the data flattened into a
// std::vector<double> of length 3*N (row-major: per-colour triple
// stored consecutively).
std::vector<double> rgb_rows(const Value &v, const char *name) {
    const auto &d = v.dims();
    const size_t H = d.rows();
    const size_t W = d.cols();
    if (v.numel() == 3) {
        std::vector<double> out(3);
        for (size_t i = 0; i < 3; ++i) out[i] = v.elemAsDouble(i);
        return out;
    }
    if (W != 3)
        throw Error(std::string(name) + " must be a 3-element or N-by-3 array",
                    0, 0, "colorangle", "", "m:colorangle:shape");
    std::vector<double> out(3 * H);
    for (size_t r = 0; r < H; ++r)
        for (size_t c = 0; c < 3; ++c)
            // col-major source: idx = c*H + r → row-major dest: r*3 + c.
            out[r * 3 + c] = v.elemAsDouble(c * H + r);
    return out;
}

} // anonymous

namespace {

// Detect LAB layout: Mx3 colormap or MxNx3 image. Returns (npix, plane)
// where plane is the per-channel stride (npix == plane). Throws on
// other shapes.
struct LabLayout {
    size_t H, W, npix;
    bool is_3d;
};

LabLayout detect_lab_layout(const Value &v, const char *fn) {
    const auto &d = v.dims();
    LabLayout L{};
    if (d.is3D() && d.pages() == 3) {
        L.H = d.rows(); L.W = d.cols();
        L.npix = L.H * L.W;
        L.is_3d = true;
    } else if (!d.is3D() && d.cols() == 3) {
        L.H = d.rows(); L.W = 1;
        L.npix = d.rows();
        L.is_3d = false;
    } else {
        throw Error(std::string(fn) + ": LAB must be Mx3 or MxNx3",
                    0, 0, fn, "", std::string("m:") + fn + ":size");
    }
    return L;
}

template <typename T>
Value lab_alloc_int(std::pmr::memory_resource *mr, const LabLayout &L,
                    ValueType cls)
{
    return L.is_3d ? Value::matrix3d(L.H, L.W, 3, cls, mr)
                   : Value::matrix(L.H, 3, cls, mr);
}

// Per-pixel channel reader / writer in the same layout as detect_lab_layout.
inline size_t lab_idx(const LabLayout &L, size_t pix, size_t ch) {
    return pix + ch * L.npix;
}

} // anonymous

Value lab2double(std::pmr::memory_resource *mr, const Value &lab)
{
    if (lab.type() == ValueType::DOUBLE) return lab;
    auto L = detect_lab_layout(lab, "lab2double");
    Value out = L.is_3d
        ? Value::matrix3d(L.H, L.W, 3, ValueType::DOUBLE, mr)
        : Value::matrix(L.H, 3, ValueType::DOUBLE, mr);
    double *od = out.doubleDataMut();

    for (size_t p = 0; p < L.npix; ++p) {
        const double l_ = lab.elemAsDouble(lab_idx(L, p, 0));
        const double a_ = lab.elemAsDouble(lab_idx(L, p, 1));
        const double b_ = lab.elemAsDouble(lab_idx(L, p, 2));
        double L_, a, b;
        switch (lab.type()) {
            case ValueType::UINT8:
                L_ = l_ * (100.0 / 255.0);
                a  = a_ - 128.0;
                b  = b_ - 128.0;
                break;
            case ValueType::UINT16:
                L_ = l_ * (100.0 / 65280.0);
                a  = a_ * (255.0 / 65280.0) - 128.0;
                b  = b_ * (255.0 / 65280.0) - 128.0;
                break;
            case ValueType::SINGLE:
                L_ = l_; a = a_; b = b_;
                break;
            default:
                throw Error("lab2double: unsupported LAB class",
                            0, 0, "lab2double", "", "m:lab2double:cls");
        }
        od[lab_idx(L, p, 0)] = L_;
        od[lab_idx(L, p, 1)] = a;
        od[lab_idx(L, p, 2)] = b;
    }
    return out;
}

Value lab2single(std::pmr::memory_resource *mr, const Value &lab)
{
    if (lab.type() == ValueType::SINGLE) return lab;
    Value asDouble = lab2double(mr, lab);
    auto L = detect_lab_layout(asDouble, "lab2single");
    Value out = L.is_3d
        ? Value::matrix3d(L.H, L.W, 3, ValueType::SINGLE, mr)
        : Value::matrix(L.H, 3, ValueType::SINGLE, mr);
    float *of = out.singleDataMut();
    const double *src = asDouble.doubleData();
    for (size_t i = 0; i < 3 * L.npix; ++i)
        of[i] = static_cast<float>(src[i]);
    return out;
}

Value lab2uint8(std::pmr::memory_resource *mr, const Value &lab)
{
    if (lab.type() == ValueType::UINT8) return lab;
    auto L = detect_lab_layout(lab, "lab2uint8");
    Value out = L.is_3d
        ? Value::matrix3d(L.H, L.W, 3, ValueType::UINT8, mr)
        : Value::matrix(L.H, 3, ValueType::UINT8, mr);
    std::uint8_t *od = out.uint8DataMut();

    for (size_t p = 0; p < L.npix; ++p) {
        for (size_t ch = 0; ch < 3; ++ch) {
            const double v = lab.elemAsDouble(lab_idx(L, p, ch));
            double w;
            switch (lab.type()) {
                case ValueType::SINGLE:
                case ValueType::DOUBLE: {
                    if (std::isnan(v)) { w = 255.0; break; }
                    w = (ch == 0) ? v * (255.0 / 100.0)
                                  : v + 128.0;
                    break;
                }
                case ValueType::UINT16:
                    w = v / 256.0;
                    break;
                default:
                    throw Error("lab2uint8: unsupported LAB class",
                                0, 0, "lab2uint8", "", "m:lab2uint8:cls");
            }
            if (w < 0)   w = 0;
            if (w > 255) w = 255;
            od[lab_idx(L, p, ch)] = static_cast<std::uint8_t>(std::lround(w));
        }
    }
    return out;
}

Value lab2uint16(std::pmr::memory_resource *mr, const Value &lab)
{
    if (lab.type() == ValueType::UINT16) return lab;
    auto L = detect_lab_layout(lab, "lab2uint16");
    Value out = L.is_3d
        ? Value::matrix3d(L.H, L.W, 3, ValueType::UINT16, mr)
        : Value::matrix(L.H, 3, ValueType::UINT16, mr);
    std::uint16_t *od = out.uint16DataMut();

    for (size_t p = 0; p < L.npix; ++p) {
        for (size_t ch = 0; ch < 3; ++ch) {
            const double v = lab.elemAsDouble(lab_idx(L, p, ch));
            double w;
            switch (lab.type()) {
                case ValueType::SINGLE:
                case ValueType::DOUBLE: {
                    if (std::isnan(v)) { w = 65535.0; break; }
                    w = (ch == 0) ? v * (65280.0 / 100.0)
                                  : (v + 128.0) * (65280.0 / 255.0);
                    break;
                }
                case ValueType::UINT8:
                    w = v * 256.0;
                    break;
                default:
                    throw Error("lab2uint16: unsupported LAB class",
                                0, 0, "lab2uint16", "", "m:lab2uint16:cls");
            }
            if (w < 0)     w = 0;
            if (w > 65535) w = 65535;
            od[lab_idx(L, p, ch)] = static_cast<std::uint16_t>(std::lround(w));
        }
    }
    return out;
}

namespace {

void wavelength_to_rgb(double lambda, double gamma,
                       double &r, double &g, double &b)
{
    r = g = b = 0.0;
    if (lambda < 380.0 || lambda > 780.0) return;   // out-of-band → black

    if (lambda < 440.0) {
        r = -(lambda - 440.0) / 60.0;
        b = 1.0;
    } else if (lambda < 490.0) {
        g = (lambda - 440.0) / 50.0;
        b = 1.0;
    } else if (lambda < 510.0) {
        g = 1.0;
        b = -(lambda - 510.0) / 20.0;
    } else if (lambda < 580.0) {
        r = (lambda - 510.0) / 70.0;
        g = 1.0;
    } else if (lambda < 645.0) {
        r = 1.0;
        g = -(lambda - 645.0) / 65.0;
    } else {
        r = 1.0;
    }

    double factor = 0.0;
    if (lambda >= 380.0 && lambda < 420.0)
        factor = 0.3 + 0.7 * (lambda - 380.0) / 40.0;
    else if (lambda <= 700.0)
        factor = 1.0;
    else if (lambda <= 780.0)
        factor = 0.3 + 0.7 * (780.0 - lambda) / 80.0;

    r = std::pow(r * factor, gamma);
    g = std::pow(g * factor, gamma);
    b = std::pow(b * factor, gamma);
}

} // anonymous

Value colorgradient(std::pmr::memory_resource *mr,
                    const Value &C, const Value &w, int n)
{
    if (C.dims().cols() != 3)
        throw Error("colorgradient: C must be K-by-3",
                    0, 0, "colorgradient", "", "m:colorgradient:C");
    const int K = static_cast<int>(C.dims().rows());
    if (K < 2)
        throw Error("colorgradient: C must have at least 2 rows",
                    0, 0, "colorgradient", "", "m:colorgradient:rows");
    if (n < 2)
        throw Error("colorgradient: n must be >= 2",
                    0, 0, "colorgradient", "", "m:colorgradient:n");

    std::vector<double> wv;
    wv.reserve(static_cast<size_t>(K - 1));
    if (w.numel() == 0) {
        wv.assign(static_cast<size_t>(K - 1), 1.0);
    } else {
        if (static_cast<int>(w.numel()) != K - 1)
            throw Error("colorgradient: must have one weight per interval",
                        0, 0, "colorgradient", "", "m:colorgradient:w");
        for (size_t i = 0; i < w.numel(); ++i)
            wv.push_back(w.elemAsDouble(i));
    }

    double total = 0.0;
    for (double x : wv) total += x;
    if (total == 0.0) total = 1.0;
    std::vector<int> wpos(static_cast<size_t>(K), 0);
    double acc = 0.0;
    wpos[0] = 1;
    for (int i = 0; i < K - 1; ++i) {
        acc += wv[(size_t)i];
        wpos[(size_t)(i + 1)] = static_cast<int>(
            1 + std::lround((n - 1) * acc / total));
    }

    Value map = Value::matrix(static_cast<size_t>(n), 3,
                              ValueType::DOUBLE, mr);
    double *md = map.doubleDataMut();

    auto Cval = [&](int r, int c) {
        return C.elemAsDouble(static_cast<size_t>(c) * (size_t)K + (size_t)r);
    };

    for (int i = 0; i < K - 1; ++i) {
        const int p0 = wpos[(size_t)i];
        const int p1 = wpos[(size_t)(i + 1)];
        if (p0 == p1) continue;
        const int len = p1 - p0 + 1;
        for (int ch = 0; ch < 3; ++ch) {
            const double a = Cval(i,     ch);
            const double b = Cval(i + 1, ch);
            for (int k = 0; k < len; ++k) {
                const double t = (len == 1) ? 0.0 :
                                 static_cast<double>(k) / (len - 1);
                md[(size_t)ch * (size_t)n + (size_t)(p0 - 1 + k)] =
                    a + t * (b - a);
            }
        }
    }
    return map;
}

Value wavelength2rgb(std::pmr::memory_resource *mr,
                     const Value &wavelength,
                     const std::string &out_class,
                     double gamma)
{
    if (!(gamma >= 0.0 && gamma <= 1.0))
        throw Error("wavelength2rgb: gamma must be in [0, 1]",
                    0, 0, "wavelength2rgb", "", "m:wavelength2rgb:gamma");

    const auto &d = wavelength.dims();
    const size_t N = wavelength.numel();
    if (N == 0)
        return Value::matrix(0, 0, ValueType::DOUBLE, mr);

    // Decide output shape:
    //   scalar λ            → 1×3 row.
    //   row vector 1×N       → 1×N×3.
    //   column vector N×1   → N×1×3.
    //   matrix M×N (>=2x2)  → M×N×3.
    Value out;
    size_t H, W, plane;
    if (N == 1) {
        out = Value::matrix(1, 3, ValueType::DOUBLE, mr);
        H = 1; W = 3; plane = 3;
    } else {
        H = d.rows(); W = d.cols();
        out = Value::matrix3d(H, W, 3, ValueType::DOUBLE, mr);
        plane = H * W;
    }
    double *od = out.doubleDataMut();

    if (N == 1) {
        double r, g, b;
        wavelength_to_rgb(wavelength.toScalar(), gamma, r, g, b);
        od[0] = r; od[1] = g; od[2] = b;
    } else {
        for (size_t i = 0; i < N; ++i) {
            double r, g, b;
            wavelength_to_rgb(wavelength.elemAsDouble(i), gamma, r, g, b);
            od[0 * plane + i] = r;
            od[1 * plane + i] = g;
            od[2 * plane + i] = b;
        }
    }

    // Cast to requested class.
    std::string lo;
    lo.reserve(out_class.size());
    for (char c : out_class) lo.push_back(static_cast<char>(std::tolower(c)));
    if (lo == "double" || lo.empty()) return out;
    if (lo == "single") return im2single(mr, out);
    if (lo == "uint8")  return im2uint8(mr, out);
    if (lo == "uint16") return im2uint16(mr, out);
    if (lo == "int16")  return im2int16(mr, out);
    throw Error("wavelength2rgb: unsupported class",
                0, 0, "wavelength2rgb", "", "m:wavelength2rgb:cls");
}

Value colorangle(std::pmr::memory_resource *mr,
                 const Value &rgb1, const Value &rgb2)
{
    const auto a = rgb_rows(rgb1, "RGB1");
    const auto b = rgb_rows(rgb2, "RGB2");
    const size_t Na = a.size() / 3;
    const size_t Nb = b.size() / 3;
    if (Na != Nb && Na != 1 && Nb != 1)
        throw Error("colorangle: RGB1 and RGB2 must broadcast (N or 1)",
                    0, 0, "colorangle", "", "m:colorangle:size");
    const size_t N = std::max(Na, Nb);

    Value out = Value::matrix(N, 1, ValueType::DOUBLE, mr);
    if (N == 0) return out;
    double *od = out.doubleDataMut();

    for (size_t i = 0; i < N; ++i) {
        const double *ai = &a[(Na == 1 ? 0 : i) * 3];
        const double *bi = &b[(Nb == 1 ? 0 : i) * 3];
        const double dot = ai[0]*bi[0] + ai[1]*bi[1] + ai[2]*bi[2];
        const double na  = std::sqrt(ai[0]*ai[0] + ai[1]*ai[1] + ai[2]*ai[2]);
        const double nb  = std::sqrt(bi[0]*bi[0] + bi[1]*bi[1] + bi[2]*bi[2]);
        double angle;
        if (na == 0.0 && nb == 0.0)      angle = 0.0;
        else if (na == 0.0 || nb == 0.0) angle = std::nan("");
        else {
            double c = dot / (na * nb);
            if (c >  1.0) c =  1.0;
            if (c < -1.0) c = -1.0;
            angle = std::acos(c) * 180.0 / M_PI;
        }
        od[i] = angle;
    }
    return out;
}

Value gray_cmap(std::pmr::memory_resource *mr, int n)
{
    if (n <= 0) return Value::matrix(0, 3, ValueType::DOUBLE, mr);
    Value out = Value::matrix(static_cast<size_t>(n), 3, ValueType::DOUBLE, mr);
    double *od = out.doubleDataMut();
    if (n == 1) {
        od[0] = od[1] = od[2] = 0.0;
        return out;
    }
    const double inv = 1.0 / static_cast<double>(n - 1);
    for (int i = 0; i < n; ++i) {
        const double v = static_cast<double>(i) * inv;
        // col-major: column 0 (R), column 1 (G), column 2 (B).
        od[0 * n + i] = v;
        od[1 * n + i] = v;
        od[2 * n + i] = v;
    }
    return out;
}

Value hot_cmap(std::pmr::memory_resource *mr, int n)
{
    if (n <= 0) return Value::matrix(0, 3, ValueType::DOUBLE, mr);
    Value out = Value::matrix(static_cast<size_t>(n), 3, ValueType::DOUBLE, mr);
    double *od = out.doubleDataMut();
    auto R = [&](int r) -> double & { return od[0 * n + r]; };
    auto G = [&](int r) -> double & { return od[1 * n + r]; };
    auto B = [&](int r) -> double & { return od[2 * n + r]; };
    if (n == 1) {
        R(0) = G(0) = B(0) = 1.0;
        return out;
    }
    if (n == 2) {
        R(0) = G(0) = 1.0; B(0) = 0.5;
        R(1) = G(1) = B(1) = 1.0;
        return out;
    }
    // n > 2
    const int idx = static_cast<int>(std::floor(3.0 / 8.0 * n));
    const int nel = idx;
    // R: r(1:idx) = i/nel; r(idx+1:end) = 1.
    for (int i = 0; i < n; ++i) R(i) = 1.0;
    for (int i = 0; i < nel; ++i) R(i) = static_cast<double>(i + 1) / nel;
    // G: g(1:idx) = 0; g(idx+1:2*idx) = (1..idx)/idx; g(2*idx+1:end) = 1.
    for (int i = 0; i < n; ++i) G(i) = 0.0;
    for (int i = 0; i < idx; ++i) G(idx + i) = static_cast<double>(i + 1) / idx;
    for (int i = 2 * idx; i < n; ++i) G(i) = 1.0;
    // B: b(idx2:end) = (1..nel2)/nel2 where idx2 = 2*idx, nel2 = n - 2*idx.
    for (int i = 0; i < n; ++i) B(i) = 0.0;
    const int idx2 = 2 * idx;
    const int nel2 = n - idx2;
    for (int i = 0; i < nel2; ++i) B(idx2 + i) = static_cast<double>(i + 1) / nel2;
    return out;
}

Value cool_cmap(std::pmr::memory_resource *mr, int n)
{
    if (n <= 0) return Value::matrix(0, 3, ValueType::DOUBLE, mr);
    Value out = Value::matrix(static_cast<size_t>(n), 3, ValueType::DOUBLE, mr);
    double *od = out.doubleDataMut();
    if (n == 1) {
        od[0]     = 0.0;  // R
        od[1]     = 1.0;  // G
        od[2]     = 1.0;  // B
        return out;
    }
    const double inv = 1.0 / static_cast<double>(n - 1);
    for (int i = 0; i < n; ++i) {
        const double r = static_cast<double>(i) * inv;
        od[0 * n + i] = r;
        od[1 * n + i] = 1.0 - r;
        od[2 * n + i] = 1.0;
    }
    return out;
}

Value spring_cmap(std::pmr::memory_resource *mr, int n)
{
    if (n <= 0) return Value::matrix(0, 3, ValueType::DOUBLE, mr);
    Value out = Value::matrix(static_cast<size_t>(n), 3, ValueType::DOUBLE, mr);
    double *od = out.doubleDataMut();
    if (n == 1) {
        od[0] = 1.0; od[1] = 0.0; od[2] = 1.0;
        return out;
    }
    const double inv = 1.0 / static_cast<double>(n - 1);
    for (int i = 0; i < n; ++i) {
        const double g = static_cast<double>(i) * inv;
        od[0 * n + i] = 1.0;
        od[1 * n + i] = g;
        od[2 * n + i] = 1.0 - g;
    }
    return out;
}

Value summer_cmap(std::pmr::memory_resource *mr, int n)
{
    if (n <= 0) return Value::matrix(0, 3, ValueType::DOUBLE, mr);
    Value out = Value::matrix(static_cast<size_t>(n), 3, ValueType::DOUBLE, mr);
    double *od = out.doubleDataMut();
    if (n == 1) {
        od[0] = 0.0; od[1] = 0.5; od[2] = 0.4;
        return out;
    }
    const double inv = 1.0 / static_cast<double>(n - 1);
    for (int i = 0; i < n; ++i) {
        const double r = static_cast<double>(i) * inv;
        od[0 * n + i] = r;
        od[1 * n + i] = 0.5 + 0.5 * r;
        od[2 * n + i] = 0.4;
    }
    return out;
}

Value autumn_cmap(std::pmr::memory_resource *mr, int n)
{
    if (n <= 0) return Value::matrix(0, 3, ValueType::DOUBLE, mr);
    Value out = Value::matrix(static_cast<size_t>(n), 3, ValueType::DOUBLE, mr);
    double *od = out.doubleDataMut();
    if (n == 1) {
        od[0] = 1.0; od[1] = 0.0; od[2] = 0.0;
        return out;
    }
    const double inv = 1.0 / static_cast<double>(n - 1);
    for (int i = 0; i < n; ++i) {
        od[0 * n + i] = 1.0;
        od[1 * n + i] = static_cast<double>(i) * inv;
        od[2 * n + i] = 0.0;
    }
    return out;
}

Value winter_cmap(std::pmr::memory_resource *mr, int n)
{
    if (n <= 0) return Value::matrix(0, 3, ValueType::DOUBLE, mr);
    Value out = Value::matrix(static_cast<size_t>(n), 3, ValueType::DOUBLE, mr);
    double *od = out.doubleDataMut();
    if (n == 1) {
        od[0] = 0.0; od[1] = 0.0; od[2] = 1.0;
        return out;
    }
    const double inv = 1.0 / static_cast<double>(n - 1);
    for (int i = 0; i < n; ++i) {
        const double g = static_cast<double>(i) * inv;
        od[0 * n + i] = 0.0;
        od[1 * n + i] = g;
        od[2 * n + i] = 1.0 - 0.5 * g;
    }
    return out;
}

Value copper_cmap(std::pmr::memory_resource *mr, int n)
{
    if (n <= 0) return Value::matrix(0, 3, ValueType::DOUBLE, mr);
    Value out = Value::matrix(static_cast<size_t>(n), 3, ValueType::DOUBLE, mr);
    double *od = out.doubleDataMut();
    if (n == 1) {
        od[0] = od[1] = od[2] = 0.0;
        return out;
    }
    const double inv = 1.0 / static_cast<double>(n - 1);
    for (int i = 0; i < n; ++i) {
        const double x = static_cast<double>(i) * inv;
        double r = 1.25 * x;
        if (r > 1.0) r = 1.0;
        od[0 * n + i] = r;
        od[1 * n + i] = 0.7812 * x;
        od[2 * n + i] = 0.4975 * x;
    }
    return out;
}

Value pink_cmap(std::pmr::memory_resource *mr, int n)
{
    if (n <= 0) return Value::matrix(0, 3, ValueType::DOUBLE, mr);
    Value out = Value::matrix(static_cast<size_t>(n), 3, ValueType::DOUBLE, mr);
    double *od = out.doubleDataMut();
    if (n == 1) {
        const double v = std::sqrt(1.0 / 3.0);
        od[0] = v; od[1] = v; od[2] = v;
        return out;
    }
    if (n == 2) {
        const double s = std::sqrt(1.0 / 3.0);
        const double s2 = std::sqrt(1.0 / 6.0);
        od[0 * n + 0] = s;  od[1 * n + 0] = s;  od[2 * n + 0] = s2;
        od[0 * n + 1] = 1;  od[1 * n + 1] = 1;  od[2 * n + 1] = 1;
        return out;
    }
    // n > 2.
    const int IDX = static_cast<int>(std::floor(3.0 / 8.0 * n));
    const double base = 1.0 / (3.0 * IDX);
    const double inv  = 1.0 / static_cast<double>(n - 1);
    std::vector<double> r(n), g(n), b(n);

    // R channel.
    const double r_end = (2.0 / 3.0) * (IDX - 1) * inv + 1.0 / 3.0;
    if (IDX == 1) {
        r[0] = base;
    } else {
        for (int i = 0; i < IDX; ++i)
            r[i] = base + (r_end - base) * i / (IDX - 1);
    }
    for (int i = IDX; i < n; ++i)
        r[i] = (2.0 / 3.0) * i * inv + 1.0 / 3.0;

    // G channel: 3-piece. Initial fill, then linspace overwrites overlap.
    for (int i = 0; i < IDX; ++i)
        g[i] = (2.0 / 3.0) * i * inv;
    {
        const double a = (2.0 / 3.0) * (IDX - 1) * inv;
        const double bb = (2.0 / 3.0) * (2 * IDX - 1) * inv + 1.0 / 3.0;
        const int len = IDX + 1;
        for (int j = 0; j < len; ++j) {
            const int pos = IDX - 1 + j;
            if (pos < 0 || pos >= n) continue;
            g[pos] = a + (bb - a) * j / (len - 1);
        }
    }
    for (int i = 2 * IDX; i < n; ++i)
        g[i] = (2.0 / 3.0) * i * inv + 1.0 / 3.0;

    // B channel: linear up to 2*IDX-1, then linspace to 1.
    const int upto = std::min(2 * IDX, n);
    for (int i = 0; i < upto; ++i)
        b[i] = (2.0 / 3.0) * i * inv;
    {
        const int b_start = 2 * IDX - 1;
        if (b_start >= 0 && b_start < n) {
            const int len = n - 2 * IDX + 1;
            const double a = (2.0 / 3.0) * b_start * inv;
            for (int j = 0; j < len; ++j) {
                const int pos = b_start + j;
                if (pos >= n) break;
                b[pos] = a + (1.0 - a) * j / (len - 1);
            }
        }
    }

    for (int i = 0; i < n; ++i) {
        od[0 * n + i] = std::sqrt(r[i]);
        od[1 * n + i] = std::sqrt(g[i]);
        od[2 * n + i] = std::sqrt(b[i]);
    }
    return out;
}

Value hsv_cmap(std::pmr::memory_resource *mr, int n)
{
    if (n <= 0) return Value::matrix(0, 3, ValueType::DOUBLE, mr);
    if (n == 1) {
        Value out = Value::matrix(1, 3, ValueType::DOUBLE, mr);
        double *od = out.doubleDataMut();
        od[0] = 1.0; od[1] = 0.0; od[2] = 0.0;
        return out;
    }
    // Build N×3 [hue, 1, 1] then dispatch through hsv2rgb.
    Value hsv_in = Value::matrix(static_cast<size_t>(n), 3, ValueType::DOUBLE, mr);
    double *id = hsv_in.doubleDataMut();
    const double inv = 1.0 / static_cast<double>(n);
    for (int i = 0; i < n; ++i) {
        id[0 * n + i] = static_cast<double>(i) * inv;  // hue
        id[1 * n + i] = 1.0;                            // saturation
        id[2 * n + i] = 1.0;                            // value
    }
    return hsv2rgb(mr, hsv_in);
}

Value flag_cmap(std::pmr::memory_resource *mr, int n)
{
    if (n <= 0) return Value::matrix(0, 3, ValueType::DOUBLE, mr);
    Value out = Value::matrix(static_cast<size_t>(n), 3, ValueType::DOUBLE, mr);
    double *od = out.doubleDataMut();
    static constexpr double kFlag[4][3] = {
        {1.0, 0.0, 0.0}, {1.0, 1.0, 1.0}, {0.0, 0.0, 1.0}, {0.0, 0.0, 0.0}
    };
    for (int i = 0; i < n; ++i) {
        const int k = i % 4;
        od[0 * n + i] = kFlag[k][0];
        od[1 * n + i] = kFlag[k][1];
        od[2 * n + i] = kFlag[k][2];
    }
    return out;
}

Value prism_cmap(std::pmr::memory_resource *mr, int n)
{
    if (n <= 0) return Value::matrix(0, 3, ValueType::DOUBLE, mr);
    Value out = Value::matrix(static_cast<size_t>(n), 3, ValueType::DOUBLE, mr);
    double *od = out.doubleDataMut();
    static constexpr double kPrism[6][3] = {
        {1.0, 0.0,        0.0},
        {1.0, 0.5,        0.0},
        {1.0, 1.0,        0.0},
        {0.0, 1.0,        0.0},
        {0.0, 0.0,        1.0},
        {2.0 / 3.0, 0.0,  1.0}
    };
    for (int i = 0; i < n; ++i) {
        const int k = i % 6;
        od[0 * n + i] = kPrism[k][0];
        od[1 * n + i] = kPrism[k][1];
        od[2 * n + i] = kPrism[k][2];
    }
    return out;
}

Value lines_cmap(std::pmr::memory_resource *mr, int n)
{
    if (n <= 0) return Value::matrix(0, 3, ValueType::DOUBLE, mr);
    Value out = Value::matrix(static_cast<size_t>(n), 3, ValueType::DOUBLE, mr);
    double *od = out.doubleDataMut();
    if (n == 1) {
        // MATLAB special-case: lines(1) → [0 0 1] (last fallback blue).
        od[0] = 0.0; od[1] = 0.0; od[2] = 1.0;
        return out;
    }
    // MATLAB R2025b factory axes colororder (7 rows).
    static constexpr double kLines[7][3] = {
        {0.066, 0.443, 0.745},
        {0.866, 0.329, 0.000},
        {0.929, 0.694, 0.125},
        {0.521, 0.086, 0.819},
        {0.231, 0.666, 0.196},
        {0.184, 0.745, 0.937},
        {0.819, 0.015, 0.545}
    };
    for (int i = 0; i < n; ++i) {
        const int k = i % 7;
        od[0 * n + i] = kLines[k][0];
        od[1 * n + i] = kLines[k][1];
        od[2 * n + i] = kLines[k][2];
    }
    return out;
}

Value bone_cmap(std::pmr::memory_resource *mr, int n)
{
    if (n <= 0) return Value::matrix(0, 3, ValueType::DOUBLE, mr);
    Value out = Value::matrix(static_cast<size_t>(n), 3, ValueType::DOUBLE, mr);
    double *od = out.doubleDataMut();
    if (n == 1) {
        od[0] = 0.125; od[1] = 0.125; od[2] = 0.125;
        return out;
    }
    if (n == 2) {
        od[0 * n + 0] = 1.0 / 16.0;
        od[1 * n + 0] = 0.125;
        od[2 * n + 0] = 0.125;
        od[0 * n + 1] = 1.0;
        od[1 * n + 1] = 1.0;
        od[2 * n + 1] = 1.0;
        return out;
    }
    // n > 2.
    const double inv = 1.0 / static_cast<double>(n - 1);
    auto x = [&](int i) { return static_cast<double>(i) * inv; };
    std::vector<double> r(n), g(n), b(n);

    // R channel.
    const int IDX_R = static_cast<int>(std::floor(0.75 * n));
    const int nel_R = n - IDX_R + 1;
    const int rem8  = n % 8;
    double base_R = 0.0;
    if (rem8 == 2 || rem8 == 4)        base_R = 1.0 / (16.0 + 2.0 * (n - rem8));
    else if (rem8 == 5 || rem8 == 7)   base_R = 1.0 / (24.0 + 2.0 * (n - rem8));
    for (int i = 0; i < IDX_R; ++i) r[i] = (7.0 / 8.0) * x(i);
    {
        const double a  = (7.0 / 8.0) * x(IDX_R - 1) + base_R;
        const double bb = 1.0;
        for (int j = 0; j < nel_R; ++j) {
            const int pos = IDX_R - 1 + j;
            if (pos < 0 || pos >= n) continue;
            r[pos] = a + (bb - a) * j / (nel_R - 1);
        }
    }

    // G channel.
    const int IDX_G = static_cast<int>(std::floor(0.375 * n));
    for (int i = 0; i < IDX_G; ++i) g[i] = (7.0 / 8.0) * x(i);
    {
        const int len = IDX_G + 1;  // 1-based length idx..2*idx
        const double a  = (7.0 / 8.0) * x(IDX_G - 1);
        const double bb = (7.0 / 8.0) * x(2 * IDX_G - 1) + 0.125;
        for (int j = 0; j < len; ++j) {
            const int pos = IDX_G - 1 + j;
            if (pos < 0 || pos >= n) continue;
            g[pos] = a + (bb - a) * j / (len - 1);
        }
    }
    for (int i = 2 * IDX_G; i < n; ++i)
        g[i] = (7.0 / 8.0) * x(i) + 0.125;

    // B channel.
    {
        const double base_B = 1.0 / (8.0 * IDX_G);
        const double a  = base_B;
        const double bb = (7.0 / 8.0) * x(IDX_G - 1) + 0.125;
        const int nel_B = IDX_G;
        for (int j = 0; j < nel_B; ++j) {
            const int pos = j;
            if (pos < 0 || pos >= n) continue;
            b[pos] = a + (bb - a) * j / (nel_B - 1);
        }
    }
    for (int i = IDX_G - 1; i < n; ++i)
        b[i] = (7.0 / 8.0) * x(i) + 0.125;

    for (int i = 0; i < n; ++i) {
        od[0 * n + i] = r[i];
        od[1 * n + i] = g[i];
        od[2 * n + i] = b[i];
    }
    return out;
}

Value rgb2lin(std::pmr::memory_resource *mr, const Value &A)
{
    // MATLAB R2025b convention: int classes float to single; else keep
    // input class. Range outside [0, 1] is allowed — the function uses
    // sign(x) * f(|x|) so negatives mirror through the gamma curve.
    Value in;
    if (A.type() == ValueType::DOUBLE) in = A;
    else if (A.type() == ValueType::SINGLE) in = A;
    else in = im2single(mr, A);

    auto sRGBtoLin = [](double x) -> double {
        const double s = (x < 0.0) ? -1.0 : 1.0;
        const double ax = std::abs(x);
        if (ax < 0.04045) return s * (ax / 12.92);
        return s * std::pow((ax + 0.055) / 1.055, 2.4);
    };

    const auto &d = in.dims();
    Value out = d.is3D()
        ? Value::matrix3d(d.rows(), d.cols(), d.pages(), in.type(), mr)
        : Value::matrix(d.rows(), d.cols(), in.type(), mr);
    const size_t N = in.numel();

    if (in.type() == ValueType::DOUBLE) {
        const double *pin = in.doubleData();
        double *pout = out.doubleDataMut();
        for (size_t i = 0; i < N; ++i) pout[i] = sRGBtoLin(pin[i]);
    } else {
        const float *pin = in.singleData();
        float *pout = out.singleDataMut();
        for (size_t i = 0; i < N; ++i)
            pout[i] = static_cast<float>(sRGBtoLin(static_cast<double>(pin[i])));
    }
    return out;
}

Value xyz2double(std::pmr::memory_resource *mr, const Value &xyz)
{
    const auto &d = xyz.dims();
    const bool ok_2d = !d.is3D() && d.cols() == 3;
    const bool ok_3d = d.is3D() && d.pages() == 3;
    if (!ok_2d && !ok_3d)
        throw Error("xyz2double: input must be M-by-3 or H-by-W-by-3",
                    0, 0, "xyz2double", "", "m:xyz2double:size");
    const ValueType t = xyz.type();
    if (t != ValueType::DOUBLE && t != ValueType::UINT16)
        throw Error("xyz2double: input must be uint16 or double",
                    0, 0, "xyz2double", "", "m:xyz2double:type");

    Value out = ok_3d
        ? Value::matrix3d(d.rows(), d.cols(), d.pages(), ValueType::DOUBLE, mr)
        : Value::matrix(d.rows(), d.cols(), ValueType::DOUBLE, mr);
    const size_t N = xyz.numel();
    double *od = out.doubleDataMut();
    if (t == ValueType::DOUBLE) {
        const double *id = xyz.doubleData();
        for (size_t i = 0; i < N; ++i) od[i] = id[i];
    } else {
        // ICC: uint16 32768 ↔ 1.0 → divide by 32768.
        const uint16_t *id = xyz.uint16Data();
        constexpr double inv32768 = 1.0 / 32768.0;
        for (size_t i = 0; i < N; ++i)
            od[i] = static_cast<double>(id[i]) * inv32768;
    }
    return out;
}

Value xyz2uint16(std::pmr::memory_resource *mr, const Value &xyz)
{
    const auto &d = xyz.dims();
    const bool ok_2d = !d.is3D() && d.cols() == 3;
    const bool ok_3d = d.is3D() && d.pages() == 3;
    if (!ok_2d && !ok_3d)
        throw Error("xyz2uint16: input must be M-by-3 or H-by-W-by-3",
                    0, 0, "xyz2uint16", "", "m:xyz2uint16:size");
    const ValueType t = xyz.type();
    if (t != ValueType::DOUBLE && t != ValueType::UINT16)
        throw Error("xyz2uint16: input must be uint16 or double",
                    0, 0, "xyz2uint16", "", "m:xyz2uint16:type");

    Value out = ok_3d
        ? Value::matrix3d(d.rows(), d.cols(), d.pages(), ValueType::UINT16, mr)
        : Value::matrix(d.rows(), d.cols(), ValueType::UINT16, mr);
    const size_t N = xyz.numel();
    uint16_t *od = out.uint16DataMut();
    if (t == ValueType::UINT16) {
        const uint16_t *id = xyz.uint16Data();
        for (size_t i = 0; i < N; ++i) od[i] = id[i];
    } else {
        const double *id = xyz.doubleData();
        for (size_t i = 0; i < N; ++i) {
            const double v = id[i] * 32768.0;
            long r = (v <= 0.0) ? 0L : std::lround(v);
            if (r < 0L) r = 0L;
            if (r > 65535L) r = 65535L;
            od[i] = static_cast<uint16_t>(r);
        }
    }
    return out;
}

Value deltaE(std::pmr::memory_resource *mr,
             const Value &I1, const Value &I2, bool isInputLab)
{
    const auto &d1 = I1.dims();
    const auto &d2 = I2.dims();

    // Determine output shape based on input dims.
    enum Shape { COLORMAP, IMAGE } shape;
    size_t out_h, out_w;
    if (!d1.is3D() && !d2.is3D()) {
        if (d1.cols() != 3 || d2.cols() != 3 ||
            d1.rows() != d2.rows())
            throw Error("deltaE: M-by-3 inputs must agree in row count",
                        0, 0, "deltaE", "", "m:deltaE:size");
        shape = COLORMAP;
        out_h = d1.rows();
        out_w = 1;
    } else if (d1.is3D() && d2.is3D()) {
        if (d1.pages() != 3 || d2.pages() != 3 ||
            d1.rows() != d2.rows() || d1.cols() != d2.cols())
            throw Error("deltaE: H-by-W-by-3 inputs must agree in size",
                        0, 0, "deltaE", "", "m:deltaE:size");
        shape = IMAGE;
        out_h = d1.rows();
        out_w = d1.cols();
    } else {
        throw Error("deltaE: I1 and I2 must both be M-by-3 or H-by-W-by-3",
                    0, 0, "deltaE", "", "m:deltaE:size");
    }

    // Class promotion: double if either is double, else single.
    const bool to_double = (I1.type() == ValueType::DOUBLE) ||
                           (I2.type() == ValueType::DOUBLE);
    Value A = to_double ? im2double(mr, I1) : im2single(mr, I1);
    Value B = to_double ? im2double(mr, I2) : im2single(mr, I2);
    if (!isInputLab) {
        A = rgb2lab(mr, A);
        B = rgb2lab(mr, B);
    }

    Value out = Value::matrix(out_h, out_w,
                              to_double ? ValueType::DOUBLE : ValueType::SINGLE,
                              mr);
    const size_t plane = out_h * out_w;
    for (size_t i = 0; i < plane; ++i) {
        const double l1 = A.elemAsDouble(0 * plane + i);
        const double a1 = A.elemAsDouble(1 * plane + i);
        const double b1 = A.elemAsDouble(2 * plane + i);
        const double l2 = B.elemAsDouble(0 * plane + i);
        const double a2 = B.elemAsDouble(1 * plane + i);
        const double b2 = B.elemAsDouble(2 * plane + i);
        const double dl = l1 - l2, da = a1 - a2, db = b1 - b2;
        const double v = std::sqrt(dl*dl + da*da + db*db);
        if (to_double) out.doubleDataMut()[i] = v;
        else           out.singleDataMut()[i] = static_cast<float>(v);
    }
    return out;
}

Value whitepoint(std::pmr::memory_resource *mr,
                 const std::string &illuminant)
{
    std::string lo;
    lo.reserve(illuminant.size());
    for (char c : illuminant) lo.push_back(static_cast<char>(std::tolower(c)));

    double X = 0.0, Y = 0.0, Z = 0.0;
    if (lo == "a")        { X = 1.0985; Y = 1.0; Z = 0.3558; }
    else if (lo == "c")   { X = 0.9807; Y = 1.0; Z = 1.1823; }
    else if (lo == "d50") { X = 0.96419865576090109; Y = 1.0;
                            Z = 0.82511648321920425; }
    else if (lo == "d55") { X = 0.9568; Y = 1.0; Z = 0.9214; }
    else if (lo == "d65") { X = 0.95047; Y = 1.0; Z = 1.08883; }
    else if (lo == "e")   { X = 1.0; Y = 1.0; Z = 1.0; }
    else if (lo.empty() || lo == "icc")
                          { X = 0.96420288085938; Y = 1.0;
                            Z = 0.82489013671875; }
    else
        throw Error("whitepoint: unsupported illuminant '" + illuminant +
                    "' (use a, c, d50, d55, d65, e, or icc)",
                    0, 0, "whitepoint", "", "m:whitepoint:illum");

    Value out = Value::matrix(1, 3, ValueType::DOUBLE, mr);
    double *od = out.doubleDataMut();
    od[0] = X; od[1] = Y; od[2] = Z;
    return out;
}

Value lin2rgb(std::pmr::memory_resource *mr, const Value &A)
{
    Value in;
    if (A.type() == ValueType::DOUBLE) in = A;
    else if (A.type() == ValueType::SINGLE) in = A;
    else in = im2single(mr, A);

    auto linTosRGB = [](double x) -> double {
        const double s = (x < 0.0) ? -1.0 : 1.0;
        const double ax = std::abs(x);
        if (ax <= 0.0031308) return s * (12.92 * ax);
        return s * (1.055 * std::pow(ax, 1.0 / 2.4) - 0.055);
    };

    const auto &d = in.dims();
    Value out = d.is3D()
        ? Value::matrix3d(d.rows(), d.cols(), d.pages(), in.type(), mr)
        : Value::matrix(d.rows(), d.cols(), in.type(), mr);
    const size_t N = in.numel();

    if (in.type() == ValueType::DOUBLE) {
        const double *pin = in.doubleData();
        double *pout = out.doubleDataMut();
        for (size_t i = 0; i < N; ++i) pout[i] = linTosRGB(pin[i]);
    } else {
        const float *pin = in.singleData();
        float *pout = out.singleDataMut();
        for (size_t i = 0; i < N; ++i)
            pout[i] = static_cast<float>(linTosRGB(static_cast<double>(pin[i])));
    }
    return out;
}

Value white_cmap(std::pmr::memory_resource *mr, int n)
{
    if (n <= 0) return Value::matrix(0, 3, ValueType::DOUBLE, mr);
    Value out = Value::matrix(static_cast<size_t>(n), 3, ValueType::DOUBLE, mr);
    double *od = out.doubleDataMut();
    const size_t total = static_cast<size_t>(n) * 3;
    for (size_t i = 0; i < total; ++i) od[i] = 1.0;
    return out;
}

Value cmap2gray(std::pmr::memory_resource *mr, const Value &cmap)
{
    const auto &d = cmap.dims();
    if (d.is3D() || d.cols() != 3 || d.rows() < 1)
        throw Error("cmap2gray: CMAP must be an N-by-3 colormap",
                    0, 0, "cmap2gray", "", "m:cmap2gray:shape");
    const size_t N = d.rows();
    Value out = Value::matrix(N, 3, ValueType::DOUBLE, mr);
    double *od = out.doubleDataMut();
    // MATLAB R2020b+ derives Y from inv(YIQ→RGB) — first row of inv
    // matrix [1 0.956 0.621; 1 -0.272 -0.647; 1 -1.106 1.703]. Full
    // double-precision (matches MATLAB's runtime `inv` to 1 ULP):
    // [0.29893602129377539, 0.58704307445112136, 0.11402090425510331].
    // Output is N×3 with the same grayscale value replicated across
    // R/G/B, clipped to [0, 1].
    constexpr double Cr = 0.29893602129377539;
    constexpr double Cg = 0.58704307445112136;
    constexpr double Cb = 0.11402090425510331;
    for (size_t i = 0; i < N; ++i) {
        const double r = cmap.elemAsDouble(0 * N + i);
        const double g = cmap.elemAsDouble(1 * N + i);
        const double b = cmap.elemAsDouble(2 * N + i);
        double y = Cr * r + Cg * g + Cb * b;
        if (y < 0.0) y = 0.0;
        if (y > 1.0) y = 1.0;
        od[0 * N + i] = y;
        od[1 * N + i] = y;
        od[2 * N + i] = y;
    }
    return out;
}

Value label2rgb(std::pmr::memory_resource *mr,
                const Value &L, const Value &cmap,
                const Value &background)
{
    const auto &dL = L.dims();
    const size_t H = dL.rows();
    const size_t W = dL.cols();
    if (dL.is3D() && dL.pages() != 1)
        throw Error("label2rgb: L must be a 2-D labelled image",
                    0, 0, "label2rgb", "", "m:label2rgb:dims");

    if (cmap.dims().cols() != 3)
        throw Error("label2rgb: CMAP must be N-by-3",
                    0, 0, "label2rgb", "", "m:label2rgb:cmap");
    const size_t N = cmap.dims().rows();

    double bg[3] = {1.0, 1.0, 1.0};
    if (background.numel() == 3) {
        bg[0] = background.elemAsDouble(0);
        bg[1] = background.elemAsDouble(1);
        bg[2] = background.elemAsDouble(2);
    } else if (background.numel() != 0)
        throw Error("label2rgb: background must be a 3-element RGB triplet",
                    0, 0, "label2rgb", "", "m:label2rgb:bg");

    Value out = Value::matrix3d(H, W, 3, ValueType::UINT8, mr);
    if (H == 0 || W == 0) return out;
    uint8_t *od = out.uint8DataMut();
    const size_t plane = H * W;
    const double *cd = cmap.doubleData();

    auto sat = [](double v) -> uint8_t {
        v = std::round(v * 255.0);
        if (v < 0)   v = 0;
        if (v > 255) v = 255;
        return static_cast<uint8_t>(v);
    };

    for (size_t i = 0; i < plane; ++i) {
        const double lv = L.elemAsDouble(i);
        const int64_t lab = static_cast<int64_t>(lv);
        if (lv < 0 || lab != static_cast<int64_t>(lv))
            throw Error("label2rgb: L must be non-negative integer-valued",
                        0, 0, "label2rgb", "", "m:label2rgb:value");
        if (lab == 0) {
            od[0 * plane + i] = sat(bg[0]);
            od[1 * plane + i] = sat(bg[1]);
            od[2 * plane + i] = sat(bg[2]);
        } else {
            const size_t row = static_cast<size_t>(lab) - 1;
            if (row >= N)
                throw Error("label2rgb: CMAP has fewer rows than max label",
                            0, 0, "label2rgb", "", "m:label2rgb:short");
            // cmap is N×3 column-major: idx[row, ch] = ch * N + row.
            od[0 * plane + i] = sat(cd[0 * N + row]);
            od[1 * plane + i] = sat(cd[1 * N + row]);
            od[2 * plane + i] = sat(cd[2 * N + row]);
        }
    }
    return out;
}

// ════════════════════════════════════════════════════════════════════
// Engine adapters
// ════════════════════════════════════════════════════════════════════

namespace detail {

#define NK_COLOR_REG(name)                                                        \
    void name##_reg(Span<const Value> args, size_t /*nargout*/,                   \
                    Span<Value> outs, CallContext &ctx)                           \
    {                                                                               \
        if (args.empty())                                                           \
            throw Error(#name ": requires X", 0, 0, #name, "",                     \
                        "m:" #name ":nargin");                                     \
        outs[0] = name(ctx.engine->resource(), args[0]);                           \
    }

void imsplit_reg(Span<const Value> args, size_t nargout,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("imsplit: requires (I)", 0, 0, "imsplit", "",
                    "m:imsplit:nargin");
    std::vector<Value> planes;
    imsplit(ctx.engine->resource(), args[0], planes);
    const size_t M = std::min((size_t)outs.size(),
                               std::max(nargout, (size_t)1));
    for (size_t i = 0; i < M && i < planes.size(); ++i)
        outs[i] = std::move(planes[i]);
}

NK_COLOR_REG(rgb2hsv)
NK_COLOR_REG(hsv2rgb)
NK_COLOR_REG(rgb2ycbcr)
NK_COLOR_REG(ycbcr2rgb)
NK_COLOR_REG(rgb2ntsc)
NK_COLOR_REG(ntsc2rgb)
NK_COLOR_REG(lab2double)
NK_COLOR_REG(lab2single)
NK_COLOR_REG(lab2uint8)
NK_COLOR_REG(lab2uint16)
NK_COLOR_REG(rgb2xyz)
NK_COLOR_REG(xyz2rgb)
NK_COLOR_REG(rgb2lab)
NK_COLOR_REG(lab2rgb)
NK_COLOR_REG(xyz2lab)
NK_COLOR_REG(lab2xyz)

#undef NK_COLOR_REG

void label2rgb_reg(Span<const Value> args, size_t /*nargout*/,
                   Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("label2rgb: requires (L, cmap [, background])",
                    0, 0, "label2rgb", "", "m:label2rgb:nargin");
    Value bg;
    if (args.size() >= 3 && !args[2].isEmpty()) bg = args[2];
    outs[0] = label2rgb(ctx.engine->resource(), args[0], args[1], bg);
}

void colorgradient_reg(Span<const Value> args, size_t /*nargout*/,
                       Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("colorgradient: requires (C [, w] [, n])",
                    0, 0, "colorgradient", "", "m:colorgradient:nargin");
    auto *mr = ctx.engine->resource();
    Value w;
    int n = 64;
    // Octave shorthand: colorgradient(C, w_or_n) — if 2nd arg is scalar,
    // it's n; if vector, it's w (and n defaults to 64).
    if (args.size() >= 2 && !args[1].isEmpty()) {
        if (args[1].numel() == 1) {
            n = static_cast<int>(args[1].toScalar());
        } else {
            w = args[1];
        }
    }
    if (args.size() >= 3 && !args[2].isEmpty())
        n = static_cast<int>(args[2].toScalar());
    outs[0] = colorgradient(mr, args[0], w, n);
}

void wavelength2rgb_reg(Span<const Value> args, size_t /*nargout*/,
                        Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("wavelength2rgb: requires (wavelength [, class [, gamma]])",
                    0, 0, "wavelength2rgb", "", "m:wavelength2rgb:nargin");
    auto *mr = ctx.engine->resource();
    std::string cls = "double";
    double gamma = 0.8;
    if (args.size() >= 2 && !args[1].isEmpty()) {
        if (!args[1].isChar() && !args[1].isString())
            throw Error("wavelength2rgb: class must be a string",
                        0, 0, "wavelength2rgb", "", "m:wavelength2rgb:cls");
        cls = args[1].toString();
    }
    if (args.size() >= 3 && !args[2].isEmpty()) gamma = args[2].toScalar();
    outs[0] = wavelength2rgb(mr, args[0], cls, gamma);
}

void colorangle_reg(Span<const Value> args, size_t /*nargout*/,
                    Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("colorangle: requires (rgb1, rgb2)",
                    0, 0, "colorangle", "", "m:colorangle:nargin");
    outs[0] = colorangle(ctx.engine->resource(), args[0], args[1]);
}

void cmap2gray_reg(Span<const Value> args, size_t /*nargout*/,
                   Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("cmap2gray: requires (cmap)",
                    0, 0, "cmap2gray", "", "m:cmap2gray:nargin");
    outs[0] = cmap2gray(ctx.engine->resource(), args[0]);
}

void gray_reg(Span<const Value> args, size_t /*nargout*/,
              Span<Value> outs, CallContext &ctx)
{
    int n = 256;
    if (args.size() >= 1 && !args[0].isEmpty()) {
        const Value &v = args[0];
        if (v.numel() != 1)
            throw Error("gray: N must be a scalar integer",
                        0, 0, "gray", "", "m:gray:n");
        const double d = v.toScalar();
        if (!std::isfinite(d) || d != std::floor(d))
            throw Error("gray: N must be a scalar integer",
                        0, 0, "gray", "", "m:gray:n");
        n = static_cast<int>(d);
    }
    outs[0] = gray_cmap(ctx.engine->resource(), n);
}

void hot_reg(Span<const Value> args, size_t /*nargout*/,
             Span<Value> outs, CallContext &ctx)
{
    int n = 256;
    if (args.size() >= 1 && !args[0].isEmpty()) {
        const Value &v = args[0];
        if (v.numel() != 1)
            throw Error("hot: N must be a scalar integer",
                        0, 0, "hot", "", "m:hot:n");
        const double d = v.toScalar();
        if (!std::isfinite(d) || d != std::floor(d))
            throw Error("hot: N must be a scalar integer",
                        0, 0, "hot", "", "m:hot:n");
        n = static_cast<int>(d);
    }
    outs[0] = hot_cmap(ctx.engine->resource(), n);
}

void cool_reg(Span<const Value> args, size_t /*nargout*/,
              Span<Value> outs, CallContext &ctx)
{
    int n = 256;
    if (args.size() >= 1 && !args[0].isEmpty()) {
        const Value &v = args[0];
        if (v.numel() != 1)
            throw Error("cool: N must be a scalar integer",
                        0, 0, "cool", "", "m:cool:n");
        const double d = v.toScalar();
        if (!std::isfinite(d) || d != std::floor(d))
            throw Error("cool: N must be a scalar integer",
                        0, 0, "cool", "", "m:cool:n");
        n = static_cast<int>(d);
    }
    outs[0] = cool_cmap(ctx.engine->resource(), n);
}

void spring_reg(Span<const Value> args, size_t /*nargout*/,
                Span<Value> outs, CallContext &ctx)
{
    int n = 256;
    if (args.size() >= 1 && !args[0].isEmpty()) {
        const Value &v = args[0];
        if (v.numel() != 1)
            throw Error("spring: N must be a scalar integer",
                        0, 0, "spring", "", "m:spring:n");
        const double d = v.toScalar();
        if (!std::isfinite(d) || d != std::floor(d))
            throw Error("spring: N must be a scalar integer",
                        0, 0, "spring", "", "m:spring:n");
        n = static_cast<int>(d);
    }
    outs[0] = spring_cmap(ctx.engine->resource(), n);
}

void summer_reg(Span<const Value> args, size_t /*nargout*/,
                Span<Value> outs, CallContext &ctx)
{
    int n = 256;
    if (args.size() >= 1 && !args[0].isEmpty()) {
        const Value &v = args[0];
        if (v.numel() != 1)
            throw Error("summer: N must be a scalar integer",
                        0, 0, "summer", "", "m:summer:n");
        const double d = v.toScalar();
        if (!std::isfinite(d) || d != std::floor(d))
            throw Error("summer: N must be a scalar integer",
                        0, 0, "summer", "", "m:summer:n");
        n = static_cast<int>(d);
    }
    outs[0] = summer_cmap(ctx.engine->resource(), n);
}

void autumn_reg(Span<const Value> args, size_t /*nargout*/,
                Span<Value> outs, CallContext &ctx)
{
    int n = 256;
    if (args.size() >= 1 && !args[0].isEmpty()) {
        const Value &v = args[0];
        if (v.numel() != 1)
            throw Error("autumn: N must be a scalar integer",
                        0, 0, "autumn", "", "m:autumn:n");
        const double d = v.toScalar();
        if (!std::isfinite(d) || d != std::floor(d))
            throw Error("autumn: N must be a scalar integer",
                        0, 0, "autumn", "", "m:autumn:n");
        n = static_cast<int>(d);
    }
    outs[0] = autumn_cmap(ctx.engine->resource(), n);
}

void winter_reg(Span<const Value> args, size_t /*nargout*/,
                Span<Value> outs, CallContext &ctx)
{
    int n = 256;
    if (args.size() >= 1 && !args[0].isEmpty()) {
        const Value &v = args[0];
        if (v.numel() != 1)
            throw Error("winter: N must be a scalar integer",
                        0, 0, "winter", "", "m:winter:n");
        const double d = v.toScalar();
        if (!std::isfinite(d) || d != std::floor(d))
            throw Error("winter: N must be a scalar integer",
                        0, 0, "winter", "", "m:winter:n");
        n = static_cast<int>(d);
    }
    outs[0] = winter_cmap(ctx.engine->resource(), n);
}

void copper_reg(Span<const Value> args, size_t /*nargout*/,
                Span<Value> outs, CallContext &ctx)
{
    int n = 256;
    if (args.size() >= 1 && !args[0].isEmpty()) {
        const Value &v = args[0];
        if (v.numel() != 1)
            throw Error("copper: N must be a scalar integer",
                        0, 0, "copper", "", "m:copper:n");
        const double d = v.toScalar();
        if (!std::isfinite(d) || d != std::floor(d))
            throw Error("copper: N must be a scalar integer",
                        0, 0, "copper", "", "m:copper:n");
        n = static_cast<int>(d);
    }
    outs[0] = copper_cmap(ctx.engine->resource(), n);
}

void pink_reg(Span<const Value> args, size_t /*nargout*/,
              Span<Value> outs, CallContext &ctx)
{
    int n = 256;
    if (args.size() >= 1 && !args[0].isEmpty()) {
        const Value &v = args[0];
        if (v.numel() != 1)
            throw Error("pink: N must be a scalar integer",
                        0, 0, "pink", "", "m:pink:n");
        const double d = v.toScalar();
        if (!std::isfinite(d) || d != std::floor(d))
            throw Error("pink: N must be a scalar integer",
                        0, 0, "pink", "", "m:pink:n");
        n = static_cast<int>(d);
    }
    outs[0] = pink_cmap(ctx.engine->resource(), n);
}

void hsv_cmap_reg(Span<const Value> args, size_t /*nargout*/,
                  Span<Value> outs, CallContext &ctx)
{
    int n = 256;
    if (args.size() >= 1 && !args[0].isEmpty()) {
        const Value &v = args[0];
        if (v.numel() != 1)
            throw Error("hsv: N must be a scalar integer",
                        0, 0, "hsv", "", "m:hsv:n");
        const double d = v.toScalar();
        if (!std::isfinite(d) || d != std::floor(d))
            throw Error("hsv: N must be a scalar integer",
                        0, 0, "hsv", "", "m:hsv:n");
        n = static_cast<int>(d);
    }
    outs[0] = hsv_cmap(ctx.engine->resource(), n);
}

void flag_reg(Span<const Value> args, size_t /*nargout*/,
              Span<Value> outs, CallContext &ctx)
{
    int n = 256;
    if (args.size() >= 1 && !args[0].isEmpty()) {
        const Value &v = args[0];
        if (v.numel() != 1)
            throw Error("flag: N must be a scalar integer",
                        0, 0, "flag", "", "m:flag:n");
        const double d = v.toScalar();
        if (!std::isfinite(d) || d != std::floor(d))
            throw Error("flag: N must be a scalar integer",
                        0, 0, "flag", "", "m:flag:n");
        n = static_cast<int>(d);
    }
    outs[0] = flag_cmap(ctx.engine->resource(), n);
}

void prism_reg(Span<const Value> args, size_t /*nargout*/,
               Span<Value> outs, CallContext &ctx)
{
    int n = 256;
    if (args.size() >= 1 && !args[0].isEmpty()) {
        const Value &v = args[0];
        if (v.numel() != 1)
            throw Error("prism: N must be a scalar integer",
                        0, 0, "prism", "", "m:prism:n");
        const double d = v.toScalar();
        if (!std::isfinite(d) || d != std::floor(d))
            throw Error("prism: N must be a scalar integer",
                        0, 0, "prism", "", "m:prism:n");
        n = static_cast<int>(d);
    }
    outs[0] = prism_cmap(ctx.engine->resource(), n);
}

void lines_reg(Span<const Value> args, size_t /*nargout*/,
               Span<Value> outs, CallContext &ctx)
{
    int n = 256;
    if (args.size() >= 1 && !args[0].isEmpty()) {
        const Value &v = args[0];
        if (v.numel() != 1)
            throw Error("lines: N must be a scalar integer",
                        0, 0, "lines", "", "m:lines:n");
        const double d = v.toScalar();
        if (!std::isfinite(d) || d != std::floor(d))
            throw Error("lines: N must be a scalar integer",
                        0, 0, "lines", "", "m:lines:n");
        n = static_cast<int>(d);
    }
    outs[0] = lines_cmap(ctx.engine->resource(), n);
}

void bone_reg(Span<const Value> args, size_t /*nargout*/,
              Span<Value> outs, CallContext &ctx)
{
    int n = 256;
    if (args.size() >= 1 && !args[0].isEmpty()) {
        const Value &v = args[0];
        if (v.numel() != 1)
            throw Error("bone: N must be a scalar integer",
                        0, 0, "bone", "", "m:bone:n");
        const double d = v.toScalar();
        if (!std::isfinite(d) || d != std::floor(d))
            throw Error("bone: N must be a scalar integer",
                        0, 0, "bone", "", "m:bone:n");
        n = static_cast<int>(d);
    }
    outs[0] = bone_cmap(ctx.engine->resource(), n);
}

void white_reg(Span<const Value> args, size_t /*nargout*/,
               Span<Value> outs, CallContext &ctx)
{
    int n = 256;
    if (args.size() >= 1 && !args[0].isEmpty()) {
        const Value &v = args[0];
        if (v.numel() != 1)
            throw Error("white: N must be a scalar integer",
                        0, 0, "white", "", "m:white:n");
        const double d = v.toScalar();
        if (!std::isfinite(d) || d != std::floor(d))
            throw Error("white: N must be a scalar integer",
                        0, 0, "white", "", "m:white:n");
        n = static_cast<int>(d);
    }
    outs[0] = white_cmap(ctx.engine->resource(), n);
}

void rgb2lin_reg(Span<const Value> args, size_t /*nargout*/,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("rgb2lin: requires (A)", 0, 0, "rgb2lin", "",
                    "m:rgb2lin:nargin");
    outs[0] = rgb2lin(ctx.engine->resource(), args[0]);
}

void lin2rgb_reg(Span<const Value> args, size_t /*nargout*/,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("lin2rgb: requires (A)", 0, 0, "lin2rgb", "",
                    "m:lin2rgb:nargin");
    outs[0] = lin2rgb(ctx.engine->resource(), args[0]);
}

void xyz2double_reg(Span<const Value> args, size_t /*nargout*/,
                    Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("xyz2double: requires (xyz)", 0, 0, "xyz2double", "",
                    "m:xyz2double:nargin");
    outs[0] = xyz2double(ctx.engine->resource(), args[0]);
}

void xyz2uint16_reg(Span<const Value> args, size_t /*nargout*/,
                    Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("xyz2uint16: requires (xyz)", 0, 0, "xyz2uint16", "",
                    "m:xyz2uint16:nargin");
    outs[0] = xyz2uint16(ctx.engine->resource(), args[0]);
}

void deltaE_reg(Span<const Value> args, size_t /*nargout*/,
                Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("deltaE: requires (I1, I2[, 'isInputLab', tf])",
                    0, 0, "deltaE", "", "m:deltaE:nargin");
    bool isInputLab = false;
    // Parse name-value pair: 'isInputLab', value.
    for (size_t i = 2; i + 1 < args.size(); i += 2) {
        if (!args[i].isChar() && !args[i].isString())
            throw Error("deltaE: name-value pairs require string keys",
                        0, 0, "deltaE", "", "m:deltaE:nv");
        std::string key = args[i].toString();
        std::string lo;
        for (char c : key) lo.push_back(static_cast<char>(std::tolower(c)));
        if (lo == "isinputlab")
            isInputLab = (args[i + 1].toScalar() != 0.0);
        else
            throw Error("deltaE: unknown name '" + key + "'",
                        0, 0, "deltaE", "", "m:deltaE:nv");
    }
    outs[0] = deltaE(ctx.engine->resource(), args[0], args[1], isInputLab);
}

void whitepoint_reg(Span<const Value> args, size_t /*nargout*/,
                    Span<Value> outs, CallContext &ctx)
{
    std::string illum = "icc";  // default
    if (args.size() >= 1 && !args[0].isEmpty()) {
        if (!args[0].isChar() && !args[0].isString())
            throw Error("whitepoint: illuminant must be a string",
                        0, 0, "whitepoint", "", "m:whitepoint:type");
        illum = args[0].toString();
    }
    outs[0] = whitepoint(ctx.engine->resource(), illum);
}

} // namespace detail
} // namespace numkit::image
