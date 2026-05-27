// libs/image/src/filter/locallapfilt.cpp
//
// Fast Local Laplacian Filtering (Aubry-Paris-Hasinoff-Kautz-Durand
// 2014, "Fast Local Laplacian Filters: Theory and Applications", ACM
// TOG 33(5), §3-4). Extends the original Local Laplacian filter of
// Paris-Hasinoff-Kautz 2011 (SIGGRAPH 2011, §3-4) with intensity
// sampling: instead of remapping the image around every pyramid
// coefficient, we sample a small set of reference intensities {g_k},
// build the Laplacian pyramid of the remapped image around each, and
// interpolate per-pixel based on the *original* Gaussian-pyramid
// value at that location. The interpolation kernel is a triangular
// hat of width `delta = (max-min)/(N-1)`. With N ≈ 16-50 intensity
// samples the result is visually indistinguishable from the dense
// per-pixel formulation at a fraction of the cost.
//
// Pyramid filter: separable 5-tap binomial `[1 4 6 4 1] / 16`
// (Burt-Adelson 1983, "The Laplacian Pyramid as a Compact Image
// Code", IEEE Trans. Communications, 31(4)). Replicate boundary.
// Upsample = zero-insert + same-kernel·4 convolution.
//
// Remap (Paris-Hasinoff-Kautz Eq. 4):
//
//   d = I - g
//   if |d| <= sigma:
//       I_new = g + sign(d) · sigma · (|d|/sigma)^alpha          (detail)
//   else:
//       I_new = g + sign(d) · (sigma + beta · (|d| - sigma))     (edge)
//
// alpha > 1 → smoothing; alpha < 1 → enhancement; beta scales the
// edge curve only.
//
// Internal pipeline (matches MATLAB R2025b locallapfilt.m + private
// builtins llf.remap / llf.pyrdownsample / llf.pyrupsample /
// llf.upSampleSubAddContribution):
//
//   1. Promote input to single (im2single semantics).
//   2. If RGB and ColorMode == luminance, compute Y = 0.298936·R +
//      0.587043·G + 0.114021·B, filter Y, then scale RGB by Y'/Y.
//   3. Else filter each plane independently.
//   4. Convert back to input class.
//
// llfCore:
//   - Build Gaussian pyramid of input.
//   - outL[end] := G[end]; outL[i<end] := zeros.
//   - For k = 0..N-1: refVal = min + k·delta; build remapped Gauss
//     pyramid; for each level: outL[i] += w(G[i], refVal) · (rG[i]
//     - upsample(rG[i+1])).
//   - Collapse outL: outL[i] += upsample(outL[i+1]).
//
// PMR HARD RULE: every fn takes std::pmr::memory_resource *mr.

#include <numkit/image/filter/filter.hpp>
#include <numkit/image/type_convert/type_convert.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace numkit::image {
namespace {

// 5-tap binomial kernel weights / 16.
constexpr float kK0 = 1.0f / 16.0f;
constexpr float kK1 = 4.0f / 16.0f;
constexpr float kK2 = 6.0f / 16.0f;
constexpr float kK3 = 4.0f / 16.0f;
constexpr float kK4 = 1.0f / 16.0f;

// 5-tap binomial for upsample: each 1-D pass scaled by 2 (so the
// separable 2-D filter sees a total factor of 4, compensating for the
// 1-in-4 nonzero density after zero-insert upsampling).
constexpr float kU0 = 2.0f / 16.0f;
constexpr float kU1 = 8.0f / 16.0f;
constexpr float kU2 = 12.0f / 16.0f;
constexpr float kU3 = 8.0f / 16.0f;
constexpr float kU4 = 2.0f / 16.0f;

// A 2-D plane buffer (single precision, column-major to match Value).
struct Plane {
    std::size_t H = 0, W = 0;
    std::vector<float> v;
    Plane() = default;
    Plane(std::size_t h, std::size_t w) : H(h), W(w), v(h * w, 0.0f) {}
    float &at(std::size_t r, std::size_t c) { return v[c * H + r]; }
    float at(std::size_t r, std::size_t c) const { return v[c * H + r]; }
};

// Read a single plane (page `p`) from a Value into a Plane buffer.
// Performs im2single semantics (int8 → (x + 128) / 255; uint8 → x/255;
// uint16 → x/65535; int16 → (x + 32768) / 65535; single/double pass
// through). Other types use elemAsDouble().
Plane to_single_plane(const Value &I, std::size_t page)
{
    const std::size_t H = I.dims().rows();
    const std::size_t W = I.dims().cols();
    Plane P(H, W);
    const std::size_t off = page * H * W;
    const ValueType t = I.type();
    for (std::size_t c = 0; c < W; ++c)
        for (std::size_t r = 0; r < H; ++r) {
            const double raw = I.elemAsDouble(off + c * H + r);
            float v;
            switch (t) {
                case ValueType::UINT8:  v = float(raw) / 255.0f;       break;
                case ValueType::UINT16: v = float(raw) / 65535.0f;     break;
                case ValueType::INT8:   v = (float(raw) + 128.0f) / 255.0f; break;
                case ValueType::INT16:  v = (float(raw) + 32768.0f) / 65535.0f; break;
                case ValueType::LOGICAL: v = raw == 0.0 ? 0.0f : 1.0f; break;
                default:                v = static_cast<float>(raw);   break;
            }
            P.at(r, c) = v;
        }
    return P;
}

// Symmetric (reflect-from-pixel) boundary, matching MATLAB's
// `impyramid('reduce', I)` and `imfilter(I, k, 'symmetric')`:
//   …, P[2], P[1], P[0] | P[0], P[1], P[2], … (the first row is
// repeated, then mirrored). Index -k → P[k-1].
inline std::size_t clamp_idx(long i, std::size_t n)
{
    if (n == 0) return 0;
    long N = static_cast<long>(n);
    // Make symmetric extension. The pattern over period 2N is
    //   P[0..N-1, N-1..0, ...].  Mapping: i → reflect(i, N).
    // We use: shift into [0, 2N), then if >= N reflect.
    long period = 2 * N;
    long m = i;
    if (m < 0) m = ((-m - 1) % period);          // i=-1 → 0, i=-2 → 1
    else m = m % period;
    if (m >= N) m = period - 1 - m;
    return static_cast<std::size_t>(m);
}

// ── Pyramid downsample ────────────────────────────────────────────
// In MATLAB, pyrdownsample(I) returns ceil(size(I)/2) (start from row
// 1, take every other). Implemented separably: row-wise convolve with
// [1 4 6 4 1]/16, col-wise convolve, then decimate by 2.
Plane pyrdownsample(const Plane &P)
{
    const std::size_t H = P.H;
    const std::size_t W = P.W;
    if (H == 0 || W == 0) return Plane(0, 0);

    // 1) Vertical convolution (rows).
    Plane tmp(H, W);
    for (std::size_t c = 0; c < W; ++c)
        for (std::size_t r = 0; r < H; ++r) {
            const float v = kK0 * P.at(clamp_idx((long)r - 2, H), c)
                          + kK1 * P.at(clamp_idx((long)r - 1, H), c)
                          + kK2 * P.at(r,                            c)
                          + kK3 * P.at(clamp_idx((long)r + 1, H), c)
                          + kK4 * P.at(clamp_idx((long)r + 2, H), c);
            tmp.at(r, c) = v;
        }
    // 2) Horizontal convolution (cols).
    Plane sm(H, W);
    for (std::size_t c = 0; c < W; ++c)
        for (std::size_t r = 0; r < H; ++r) {
            const float v = kK0 * tmp.at(r, clamp_idx((long)c - 2, W))
                          + kK1 * tmp.at(r, clamp_idx((long)c - 1, W))
                          + kK2 * tmp.at(r, c)
                          + kK3 * tmp.at(r, clamp_idx((long)c + 1, W))
                          + kK4 * tmp.at(r, clamp_idx((long)c + 2, W));
            sm.at(r, c) = v;
        }
    // 3) Decimate by 2 (start from row 1 / col 1 in 1-based = 0 in 0-based).
    const std::size_t H2 = (H + 1) / 2;
    const std::size_t W2 = (W + 1) / 2;
    Plane out(H2, W2);
    for (std::size_t c = 0; c < W2; ++c)
        for (std::size_t r = 0; r < H2; ++r)
            out.at(r, c) = sm.at(r * 2, c * 2);
    return out;
}

// ── Pyramid upsample to exact target H × W ─────────────────────────
// Standard Burt-Adelson expand: insert zero rows/cols between samples
// (the upsampled grid is target H × W, even index = original, odd = 0),
// then convolve with the 5-tap binomial scaled by 4. Replicate boundary
// applied to the zero-inserted intermediate, which is itself zero-padded
// on the right/bottom if target dims are odd.
Plane pyrupsample(const Plane &P, std::size_t Htarget, std::size_t Wtarget)
{
    const std::size_t H = P.H;
    const std::size_t W = P.W;
    if (Htarget == 0 || Wtarget == 0) return Plane(0, 0);
    // 1) Zero-insert: build (Htarget, Wtarget) plane with source at
    //    even indices.
    Plane up(Htarget, Wtarget);
    for (std::size_t c = 0; c < W; ++c) {
        const std::size_t cu = 2 * c;
        if (cu >= Wtarget) break;
        for (std::size_t r = 0; r < H; ++r) {
            const std::size_t ru = 2 * r;
            if (ru >= Htarget) break;
            up.at(ru, cu) = P.at(r, c);
        }
    }
    // 2) Vertical convolution with [4 16 24 16 4]/16. Replicate boundary
    //    on the up grid (zero-inserted).
    Plane tmp(Htarget, Wtarget);
    for (std::size_t c = 0; c < Wtarget; ++c)
        for (std::size_t r = 0; r < Htarget; ++r) {
            const float v = kU0 * up.at(clamp_idx((long)r - 2, Htarget), c)
                          + kU1 * up.at(clamp_idx((long)r - 1, Htarget), c)
                          + kU2 * up.at(r,                                  c)
                          + kU3 * up.at(clamp_idx((long)r + 1, Htarget), c)
                          + kU4 * up.at(clamp_idx((long)r + 2, Htarget), c);
            tmp.at(r, c) = v;
        }
    // 3) Horizontal convolution with [4 16 24 16 4]/16.
    Plane out(Htarget, Wtarget);
    for (std::size_t c = 0; c < Wtarget; ++c)
        for (std::size_t r = 0; r < Htarget; ++r) {
            const float v = kU0 * tmp.at(r, clamp_idx((long)c - 2, Wtarget))
                          + kU1 * tmp.at(r, clamp_idx((long)c - 1, Wtarget))
                          + kU2 * tmp.at(r, c)
                          + kU3 * tmp.at(r, clamp_idx((long)c + 1, Wtarget))
                          + kU4 * tmp.at(r, clamp_idx((long)c + 2, Wtarget));
            out.at(r, c) = v;
        }
    return out;
}

// ── Remap (Paris-Hasinoff Eq. 4) ─────────────────────────────────
Plane remap_image(const Plane &I, float g, float sigma, float alpha, float beta)
{
    Plane out(I.H, I.W);
    const float inv_sigma = sigma > 0.0f ? 1.0f / sigma : 0.0f;
    for (std::size_t i = 0; i < I.H * I.W; ++i) {
        const float v = I.v[i];
        const float d = v - g;
        const float ad = std::fabs(d);
        const float sgn = (d > 0.0f) - (d < 0.0f);
        float nd;
        if (sigma == 0.0f) {
            // d/sigma is ill-defined; the detail branch collapses to
            // g (since 0^alpha → 0 for alpha > 0). All pixels go
            // through the edge curve.
            nd = sgn * beta * ad;
        } else if (ad <= sigma) {
            // Detail curve.
            const float t = ad * inv_sigma;
            nd = sgn * sigma * std::pow(t, alpha);
        } else {
            // Edge curve.
            nd = sgn * (sigma + beta * (ad - sigma));
        }
        out.v[i] = g + nd;
    }
    return out;
}

// ── llfCore (Aubry-Paris fast variant) ────────────────────────────
Plane llf_core(const Plane &input, float sigma, float alpha, float beta,
               int numIntensityLevels, int numPyramidLevels)
{
    // Find global min / max.
    float minVal = input.v[0], maxVal = input.v[0];
    for (float x : input.v) {
        if (x < minVal) minVal = x;
        if (x > maxVal) maxVal = x;
    }

    // ── Special case 1: 1 sample ────────────────────────────────
    if (numIntensityLevels == 1) {
        const float refVal = 0.5f * (minVal + maxVal);
        return remap_image(input, refVal, sigma, alpha, beta);
    }

    // ── Special case 2: flat image ──────────────────────────────
    if (minVal == maxVal) {
        Plane out = input;
        return out;
    }

    // ── Gaussian pyramid of input ───────────────────────────────
    std::vector<Plane> inG(numPyramidLevels);
    inG[0] = input;
    for (int i = 1; i < numPyramidLevels; ++i)
        inG[i] = pyrdownsample(inG[i - 1]);

    // ── Output Laplacian pyramid; top level = top Gaussian ──────
    std::vector<Plane> outL(numPyramidLevels);
    outL[numPyramidLevels - 1] = inG[numPyramidLevels - 1];
    for (int i = 0; i < numPyramidLevels - 1; ++i)
        outL[i] = Plane(inG[i].H, inG[i].W);   // zeroed

    // ── Sequentially accumulate over N intensity samples ───────
    const float delta = (maxVal - minVal) / float(numIntensityLevels - 1);
    std::vector<Plane> rG(numPyramidLevels);
    for (int k = 0; k < numIntensityLevels; ++k) {
        const float refVal = minVal + float(k) * delta;
        rG[0] = remap_image(input, refVal, sigma, alpha, beta);
        for (int i = 1; i < numPyramidLevels; ++i)
            rG[i] = pyrdownsample(rG[i - 1]);
        // For each finer level: outL[i] += w · (rG[i] - up(rG[i+1])).
        for (int i = numPyramidLevels - 2; i >= 0; --i) {
            Plane up = pyrupsample(rG[i + 1], rG[i].H, rG[i].W);
            for (std::size_t j = 0; j < outL[i].v.size(); ++j) {
                const float rL = rG[i].v[j] - up.v[j];
                const float w = 1.0f
                    - std::fabs(inG[i].v[j] - refVal) / delta;
                const float ww = w > 0.0f ? w : 0.0f;
                outL[i].v[j] += ww * rL;
            }
        }
    }

    // ── Collapse the output Laplacian pyramid ──────────────────
    for (int i = numPyramidLevels - 2; i >= 0; --i) {
        Plane up = pyrupsample(outL[i + 1], outL[i].H, outL[i].W);
        for (std::size_t j = 0; j < outL[i].v.size(); ++j)
            outL[i].v[j] += up.v[j];
    }
    return outL[0];
}

// ── Convert RGB → luminance with the MATLAB-specific weights ─────
struct RGBSplit {
    Plane gray;
    Plane r, g, b;            // original [0,1] planes
    Plane ratio_r, ratio_g, ratio_b;  // RGB / (Y + eps_single)
};
RGBSplit rgb_to_gray_with_ratios(const Plane &R, const Plane &G, const Plane &B)
{
    constexpr float kRC = 0.298936021293776f;
    constexpr float kGC = 0.587043074451121f;
    constexpr float kBC = 0.114020904255103f;
    const float eps_s = std::numeric_limits<float>::epsilon();
    RGBSplit s;
    s.gray = Plane(R.H, R.W);
    s.r = R; s.g = G; s.b = B;
    s.ratio_r = Plane(R.H, R.W);
    s.ratio_g = Plane(R.H, R.W);
    s.ratio_b = Plane(R.H, R.W);
    for (std::size_t i = 0; i < R.v.size(); ++i) {
        const float y = kRC * R.v[i] + kGC * G.v[i] + kBC * B.v[i];
        s.gray.v[i] = y;
        const float yp = y + eps_s;
        s.ratio_r.v[i] = R.v[i] / yp;
        s.ratio_g.v[i] = G.v[i] / yp;
        s.ratio_b.v[i] = B.v[i] / yp;
    }
    return s;
}

// ── Convert filtered Plane back into a Value (output type cast) ──
Value plane_to_value(const Plane &P, ValueType target,
                     std::pmr::memory_resource *mr)
{
    Value out;
    const std::size_t H = P.H, W = P.W;
    auto sat_uint8 = [](float v) -> uint8_t {
        v = std::round(v * 255.0f);
        if (v < 0.0f)   v = 0.0f;
        if (v > 255.0f) v = 255.0f;
        return static_cast<uint8_t>(v);
    };
    auto sat_uint16 = [](float v) -> uint16_t {
        v = std::round(v * 65535.0f);
        if (v < 0.0f)     v = 0.0f;
        if (v > 65535.0f) v = 65535.0f;
        return static_cast<uint16_t>(v);
    };
    auto sat_int8 = [](float v) -> int8_t {
        v = std::round(v * 255.0f - 128.0f);
        if (v < -128.0f) v = -128.0f;
        if (v > 127.0f)  v = 127.0f;
        return static_cast<int8_t>(v);
    };
    auto sat_int16 = [](float v) -> int16_t {
        // im2int16 of single in [0,1]: round(x*65535-32768).
        v = std::round(v * 65535.0f - 32768.0f);
        if (v < -32768.0f) v = -32768.0f;
        if (v >  32767.0f) v =  32767.0f;
        return static_cast<int16_t>(v);
    };
    switch (target) {
        case ValueType::UINT8: {
            out = Value::matrix(H, W, ValueType::UINT8, mr);
            uint8_t *od = out.uint8DataMut();
            for (std::size_t i = 0; i < H * W; ++i) od[i] = sat_uint8(P.v[i]);
            break;
        }
        case ValueType::UINT16: {
            out = Value::matrix(H, W, ValueType::UINT16, mr);
            uint16_t *od = out.uint16DataMut();
            for (std::size_t i = 0; i < H * W; ++i) od[i] = sat_uint16(P.v[i]);
            break;
        }
        case ValueType::INT8: {
            out = Value::matrix(H, W, ValueType::INT8, mr);
            int8_t *od = out.int8DataMut();
            for (std::size_t i = 0; i < H * W; ++i) od[i] = sat_int8(P.v[i]);
            break;
        }
        case ValueType::INT16: {
            out = Value::matrix(H, W, ValueType::INT16, mr);
            int16_t *od = out.int16DataMut();
            for (std::size_t i = 0; i < H * W; ++i) od[i] = sat_int16(P.v[i]);
            break;
        }
        case ValueType::SINGLE: {
            out = Value::matrix(H, W, ValueType::SINGLE, mr);
            float *od = out.singleDataMut();
            std::memcpy(od, P.v.data(), H * W * sizeof(float));
            break;
        }
        default: {
            // Fallback: double (e.g. for double inputs that MATLAB
            // doesn't accept but a caller might pass anyway).
            out = Value::matrix(H, W, ValueType::DOUBLE, mr);
            double *od = out.doubleDataMut();
            for (std::size_t i = 0; i < H * W; ++i) od[i] = static_cast<double>(P.v[i]);
            break;
        }
    }
    return out;
}

// 3-channel version: stack three filtered planes into H × W × 3.
Value rgb_planes_to_value(const Plane &R, const Plane &G, const Plane &B,
                          ValueType target,
                          std::pmr::memory_resource *mr)
{
    const std::size_t H = R.H, W = R.W;
    Value out = Value::matrix3d(H, W, 3, target, mr);
    auto write = [&](int page, const Plane &P) {
        const std::size_t off = static_cast<std::size_t>(page) * H * W;
        switch (target) {
            case ValueType::UINT8: {
                uint8_t *od = out.uint8DataMut() + off;
                for (std::size_t i = 0; i < H * W; ++i) {
                    float v = std::round(P.v[i] * 255.0f);
                    if (v < 0)    v = 0;
                    if (v > 255)  v = 255;
                    od[i] = static_cast<uint8_t>(v);
                }
                break;
            }
            case ValueType::UINT16: {
                uint16_t *od = out.uint16DataMut() + off;
                for (std::size_t i = 0; i < H * W; ++i) {
                    float v = std::round(P.v[i] * 65535.0f);
                    if (v < 0)     v = 0;
                    if (v > 65535) v = 65535;
                    od[i] = static_cast<uint16_t>(v);
                }
                break;
            }
            case ValueType::INT8: {
                int8_t *od = out.int8DataMut() + off;
                for (std::size_t i = 0; i < H * W; ++i) {
                    float v = std::round(P.v[i] * 255.0f - 128.0f);
                    if (v < -128) v = -128;
                    if (v >  127) v =  127;
                    od[i] = static_cast<int8_t>(v);
                }
                break;
            }
            case ValueType::INT16: {
                int16_t *od = out.int16DataMut() + off;
                for (std::size_t i = 0; i < H * W; ++i) {
                    float v = std::round(P.v[i] * 65535.0f - 32768.0f);
                    if (v < -32768) v = -32768;
                    if (v >  32767) v =  32767;
                    od[i] = static_cast<int16_t>(v);
                }
                break;
            }
            case ValueType::SINGLE: {
                float *od = out.singleDataMut() + off;
                std::memcpy(od, P.v.data(), H * W * sizeof(float));
                break;
            }
            default: {
                double *od = out.doubleDataMut() + off;
                for (std::size_t i = 0; i < H * W; ++i)
                    od[i] = static_cast<double>(P.v[i]);
                break;
            }
        }
    };
    write(0, R);
    write(1, G);
    write(2, B);
    return out;
}

int auto_intensity_levels(double alpha)
{
    if (alpha < 0.1) return 50;
    if (alpha < 0.9) {
        // round(((50*0.9 - 16*0.1) - (50-16)*alpha) / 0.8)
        const double num = (50.0 * 0.9 - 16.0 * 0.1) - (50.0 - 16.0) * alpha;
        return static_cast<int>(std::round(num / 0.8));
    }
    return 16;
}

} // namespace

Value locallapfilt(const Value &I, double sigma, double alpha, double beta,
                   int num_intensity_levels, bool process_luminance,
                   std::pmr::memory_resource *mr)
{
    if (!std::isfinite(sigma) || sigma < 0.0)
        throw Error("locallapfilt: sigma must be a non-negative finite scalar",
                    0, 0, "locallapfilt", "", "numkit:locallapfilt:sigma");
    if (!std::isfinite(alpha) || alpha <= 0.0)
        throw Error("locallapfilt: alpha must be a positive finite scalar",
                    0, 0, "locallapfilt", "", "numkit:locallapfilt:alpha");
    if (!std::isfinite(beta) || beta < 0.0)
        throw Error("locallapfilt: beta must be a non-negative finite scalar",
                    0, 0, "locallapfilt", "", "numkit:locallapfilt:beta");

    const auto &dI = I.dims();
    if (dI.rows() == 0 || dI.cols() == 0)
        throw Error("locallapfilt: I must be non-empty",
                    0, 0, "locallapfilt", "", "numkit:locallapfilt:empty");
    const bool isRGB = dI.is3D() && dI.pages() == 3;
    const bool isGray = !dI.is3D() || (dI.is3D() && dI.pages() == 1);
    if (!isRGB && !isGray)
        throw Error("locallapfilt: I must be H×W (grayscale) or H×W×3 (RGB)",
                    0, 0, "locallapfilt", "", "numkit:locallapfilt:shape");

    // ── Special-case passthroughs (must short-circuit before pyramid) ──
    if ((alpha == 1.0 && beta == 1.0) || (sigma == 0.0 && beta == 1.0)) {
        return I;
    }

    const ValueType origClass = I.type();
    const std::size_t H = dI.rows();
    const std::size_t W = dI.cols();

    // ── Resolve num_intensity_levels ──────────────────────────────
    int N = num_intensity_levels;
    if (N <= 0) N = auto_intensity_levels(alpha);

    // Pyramid depth: floor(log2(min(H, W))) + 1.
    const std::size_t shortDim = std::min(H, W);
    int numPyrLevels = 0;
    {
        std::size_t s = shortDim;
        while (s >= 1) { ++numPyrLevels; s >>= 1; }
    }
    if (numPyrLevels < 1) numPyrLevels = 1;

    const float fsigma = static_cast<float>(sigma);
    const float falpha = static_cast<float>(alpha);
    const float fbeta  = static_cast<float>(beta);

    if (isGray) {
        Plane P = to_single_plane(I, 0);
        Plane out = llf_core(P, fsigma, falpha, fbeta, N, numPyrLevels);
        return plane_to_value(out, origClass, mr);
    }

    // ── RGB ──
    Plane R = to_single_plane(I, 0);
    Plane G = to_single_plane(I, 1);
    Plane B = to_single_plane(I, 2);
    if (process_luminance) {
        RGBSplit s = rgb_to_gray_with_ratios(R, G, B);
        Plane filt = llf_core(s.gray, fsigma, falpha, fbeta, N, numPyrLevels);
        Plane outR(H, W), outG(H, W), outB(H, W);
        for (std::size_t i = 0; i < H * W; ++i) {
            outR.v[i] = filt.v[i] * s.ratio_r.v[i];
            outG.v[i] = filt.v[i] * s.ratio_g.v[i];
            outB.v[i] = filt.v[i] * s.ratio_b.v[i];
        }
        return rgb_planes_to_value(outR, outG, outB, origClass, mr);
    }
    // Separate per-channel.
    Plane fR = llf_core(R, fsigma, falpha, fbeta, N, numPyrLevels);
    Plane fG = llf_core(G, fsigma, falpha, fbeta, N, numPyrLevels);
    Plane fB = llf_core(B, fsigma, falpha, fbeta, N, numPyrLevels);
    return rgb_planes_to_value(fR, fG, fB, origClass, mr);
}

namespace detail {

void locallapfilt_reg(Span<const Value> args, std::size_t /*nargout*/,
                      Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("locallapfilt: requires (I, sigma, alpha [, beta] [, NV...])",
                    0, 0, "locallapfilt", "",
                    "numkit:locallapfilt:nargin");
    auto *mr = ctx.engine->resource();

    const Value &I = args[0];
    const double sigma = args[1].toScalar();
    const double alpha = args[2].toScalar();
    auto is_string = [](const Value &v) { return v.isChar() || v.isString(); };

    double beta = 1.0;
    int nlevels = -1;          // -1 = auto
    bool processLuminance = true;

    std::size_t i = 3;
    // Optional positional beta (if 4th arg is numeric scalar and not a string).
    if (i < args.size() && !is_string(args[i])) {
        if (args[i].numel() != 1)
            throw Error("locallapfilt: beta must be a scalar",
                        0, 0, "locallapfilt", "",
                        "numkit:locallapfilt:betaShape");
        beta = args[i].toScalar();
        ++i;
    }

    while (i + 1 < args.size()) {
        if (!is_string(args[i]))
            throw Error("locallapfilt: expected NV-pair name string",
                        0, 0, "locallapfilt", "",
                        "numkit:locallapfilt:badNv");
        std::string name = args[i].toString();
        std::string nlo;
        for (char ch : name)
            nlo += static_cast<char>(std::tolower(
                static_cast<unsigned char>(ch)));
        if (nlo == "numintensitylevels") {
            const Value &v = args[i + 1];
            if (is_string(v)) {
                std::string s = v.toString();
                std::string slo;
                for (char ch : s)
                    slo += static_cast<char>(std::tolower(
                        static_cast<unsigned char>(ch)));
                if (slo != "auto")
                    throw Error("locallapfilt: NumIntensityLevels string "
                                "must be 'auto'",
                                0, 0, "locallapfilt", "",
                                "numkit:locallapfilt:nIntStr");
                nlevels = -1;
            } else {
                nlevels = static_cast<int>(v.toScalar());
                if (nlevels < 1)
                    throw Error("locallapfilt: NumIntensityLevels must be >= 1",
                                0, 0, "locallapfilt", "",
                                "numkit:locallapfilt:nIntRange");
            }
        } else if (nlo == "colormode") {
            std::string s = args[i + 1].toString();
            std::string slo;
            for (char ch : s)
                slo += static_cast<char>(std::tolower(
                    static_cast<unsigned char>(ch)));
            if (slo == "luminance")     processLuminance = true;
            else if (slo == "separate") processLuminance = false;
            else throw Error("locallapfilt: ColorMode must be 'luminance' or "
                             "'separate'",
                             0, 0, "locallapfilt", "",
                             "numkit:locallapfilt:colorMode");
        } else {
            throw Error("locallapfilt: unknown option '" + name + "'",
                        0, 0, "locallapfilt", "",
                        "numkit:locallapfilt:unknownNv");
        }
        i += 2;
    }

    outs[0] = locallapfilt(I, sigma, alpha, beta, nlevels,
                           processLuminance, mr);
}

} // namespace detail

} // namespace numkit::image
