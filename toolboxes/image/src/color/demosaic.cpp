// toolboxes/image/src/color/demosaic.cpp
//
// demosaic — Bayer mosaic → RGB via Malvar-He-Cutler 2004 linear
// 5×5 filtering. Input is a 2-D uint8 / uint16 / uint32 mosaic;
// output is an H×W×3 truecolor RGB image of the same class.
//
// Algorithm: Malvar-He-Cutler "High-Quality Linear Interpolation for
// Demosaicing of Bayer-Patterned Color Images", IEEE ICASSP 2004.
// Five 5×5 integer kernels (×8) cover the four pixel-type configurations:
//
//   * G at R or B locations           — kernel G_at_RB
//   * R at G in R-row, B at G in B-row — kernel C_at_G_sameRow
//   * R at G in B-row, B at G in R-row — kernel C_at_G_diffRow
//   * R at B or B at R                — kernel C_at_C2
//
// At the sensor positions themselves (e.g. R-channel at R-pixel) we
// simply return the raw mosaic value. Boundary handling reflects
// through the FIRST pixel (k = -1 → orig(1), k = N → orig(N-2)) so
// the mirrored neighbourhood preserves the Bayer pattern — this is
// what MATLAB R2025b does, NOT imfilter's standard `symmetric`.

#include <numkit/image/color/color.hpp>

#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>

namespace numkit::image {

namespace {

// MHC kernels from Malvar-He-Cutler 2004 Table 1. Each kernel is the
// "displayed" form (sum = 8); divide the convolution sum by 8 to get
// the reconstructed pixel value. We use double precision so the 0.5
// and 1.5 entries fall out naturally.

// G at R or B locations.
constexpr double K_G_at_RB[25] = {
    0, 0, -1, 0, 0,
    0, 0,  2, 0, 0,
   -1, 2,  4, 2,-1,
    0, 0,  2, 0, 0,
    0, 0, -1, 0, 0
};

// R at G in R-row (or B at G in B-row).
constexpr double K_C_at_G_sameRow[25] = {
    0,  0,  0.5, 0,  0,
    0, -1,    0,-1,  0,
   -1,  4,    5, 4, -1,
    0, -1,    0,-1,  0,
    0,  0,  0.5, 0,  0
};

// R at G in B-row (or B at G in R-row).
constexpr double K_C_at_G_diffRow[25] = {
    0,    0, -1, 0,    0,
    0,   -1,  4,-1,    0,
    0.5,  0,  5, 0,    0.5,
    0,   -1,  4,-1,    0,
    0,    0, -1, 0,    0
};

// R at B (or B at R).
constexpr double K_C_at_C2[25] = {
    0,    0, -1.5, 0,    0,
    0,    2,    0, 2,    0,
   -1.5,  0,    6, 0,   -1.5,
    0,    2,    0, 2,    0,
    0,    0, -1.5, 0,    0
};

// Mirror through the first pixel (Bayer-preserving). For 0-indexed
// index k and length N:  k<0 → -k;  k>=N → 2N-2-k.
inline int mirror1(int k, int N) {
    if (k < 0)    return -k;
    if (k >= N)   return 2*N - 2 - k;
    return k;
}

inline double sampleI(const Value &I, int r, int c, size_t M, size_t N) {
    const int rp = mirror1(r, int(M));
    const int cp = mirror1(c, int(N));
    return I.elemAsDouble(size_t(rp) + size_t(cp) * M);
}

inline double conv5(const Value &I, int r, int c, size_t M, size_t N,
                    const double *K)
{
    double acc = 0.0;
    for (int dr = -2; dr <= 2; ++dr)
        for (int dc = -2; dc <= 2; ++dc) {
            const double w = K[(dr + 2) * 5 + (dc + 2)];
            if (w != 0.0)
                acc += w * sampleI(I, r + dr, c + dc, M, N);
        }
    return acc / 8.0;
}

inline void writeNativeInt(Value &out, size_t idx, double v, ValueType T)
{
    if (v < 0.0) v = 0.0;
    switch (T) {
        case ValueType::UINT8:
            if (v > 255.0) v = 255.0;
            out.uint8DataMut()[idx] = static_cast<std::uint8_t>(std::lround(v));
            break;
        case ValueType::UINT16:
            if (v > 65535.0) v = 65535.0;
            out.uint16DataMut()[idx] = static_cast<std::uint16_t>(std::lround(v));
            break;
        case ValueType::UINT32:
            if (v > 4294967295.0) v = 4294967295.0;
            out.uint32DataMut()[idx] = static_cast<std::uint32_t>(std::llround(v));
            break;
        default:
            // Unreachable — adapter validates type.
            break;
    }
}

} // anonymous

Value demosaic(const Value &I, const std::string &sensorAlignment,
               int bitsPerSample, std::pmr::memory_resource *mr)
{
    const auto &dims = I.dims();
    if (dims.ndims() < 2)
        throw Error("demosaic: I must be a 2-D matrix",
                    0, 0, "demosaic", "", "numkit:demosaic:rank");
    const size_t M = dims.dim(0);
    const size_t N = dims.dim(1);
    if (M < 2 || N < 2 || (M % 2) != 0 || (N % 2) != 0)
        throw Error("demosaic: image dims must both be even and ≥ 2",
                    0, 0, "demosaic", "", "numkit:demosaic:invalidImageSize");
    if (I.numel() != M * N)
        throw Error("demosaic: I must be 2-D",
                    0, 0, "demosaic", "", "numkit:demosaic:shape");

    const ValueType T = I.type();
    if (T != ValueType::UINT8 && T != ValueType::UINT16 &&
        T != ValueType::UINT32)
        throw Error("demosaic: image must be uint8, uint16, or uint32",
                    0, 0, "demosaic", "", "numkit:demosaic:type");

    // Sensor alignment → parity (r_par, c_par) of R pixel (1-indexed).
    // The four MATLAB patterns:
    //   rggb: R at (odd,odd),  B at (even,even)
    //   bggr: R at (even,even), B at (odd,odd)
    //   grbg: R at (odd,even),  B at (even,odd)
    //   gbrg: R at (even,odd),  B at (odd,even)
    int r_par_r, r_par_c, b_par_r, b_par_c;
    if      (sensorAlignment == "rggb") { r_par_r = 1; r_par_c = 1; b_par_r = 0; b_par_c = 0; }
    else if (sensorAlignment == "bggr") { r_par_r = 0; r_par_c = 0; b_par_r = 1; b_par_c = 1; }
    else if (sensorAlignment == "grbg") { r_par_r = 1; r_par_c = 0; b_par_r = 0; b_par_c = 1; }
    else if (sensorAlignment == "gbrg") { r_par_r = 0; r_par_c = 1; b_par_r = 1; b_par_c = 0; }
    else
        throw Error("demosaic: sensorAlignment must be 'rggb', 'bggr', "
                    "'grbg', or 'gbrg'",
                    0, 0, "demosaic", "", "numkit:demosaic:alignment");

    // Output clamp limit = class max. Empirically (probed against
    // MATLAB R2025b), BitsPerSample does NOT clamp — values above
    // 2^bps - 1 pass through unchanged. We accept-and-ignore the
    // parameter for API compatibility.
    (void)bitsPerSample;
    const double maxVal = (T == ValueType::UINT8)   ? 255.0
                        : (T == ValueType::UINT16)  ? 65535.0
                                                    : 4294967295.0;

    Value RGB = Value::matrix3d(M, N, 3, T, mr);

    // Per-pixel processing. r, c are 0-indexed; pattern parity uses 1-indexed parity.
    for (int c = 0; c < int(N); ++c) {
        const int c1 = c + 1;                // 1-indexed
        for (int r = 0; r < int(M); ++r) {
            const int r1 = r + 1;
            const bool isR = ((r1 & 1) == r_par_r) && ((c1 & 1) == r_par_c);
            const bool isB = ((r1 & 1) == b_par_r) && ((c1 & 1) == b_par_c);

            double R_v, G_v, B_v;
            const double m = sampleI(I, r, c, M, N);
            if (isR) {
                R_v = m;
                G_v = conv5(I, r, c, M, N, K_G_at_RB);
                B_v = conv5(I, r, c, M, N, K_C_at_C2);
            } else if (isB) {
                B_v = m;
                G_v = conv5(I, r, c, M, N, K_G_at_RB);
                R_v = conv5(I, r, c, M, N, K_C_at_C2);
            } else {
                // G pixel.
                G_v = m;
                // "Same-row" kernel = R-at-G in R's row (or B-at-G in B's row).
                const bool gInRrow = ((r1 & 1) == r_par_r);
                if (gInRrow) {
                    R_v = conv5(I, r, c, M, N, K_C_at_G_sameRow);
                    B_v = conv5(I, r, c, M, N, K_C_at_G_diffRow);
                } else {
                    R_v = conv5(I, r, c, M, N, K_C_at_G_diffRow);
                    B_v = conv5(I, r, c, M, N, K_C_at_G_sameRow);
                }
            }

            auto clamp = [&](double v) {
                if (v < 0.0)      v = 0.0;
                if (v > maxVal)   v = maxVal;
                return v;
            };
            const size_t base = size_t(r) + size_t(c) * M;
            writeNativeInt(RGB, base + 0 * M * N, clamp(R_v), T);
            writeNativeInt(RGB, base + 1 * M * N, clamp(G_v), T);
            writeNativeInt(RGB, base + 2 * M * N, clamp(B_v), T);
        }
    }
    return RGB;
}

} // namespace numkit::image
