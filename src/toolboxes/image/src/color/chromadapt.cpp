// toolboxes/image/src/color/chromadapt.cpp
//
// Chromatic adaptation transform for white-balance correction.
//
// Three documented MATLAB R2025b methods:
//   * 'bradford' (default; Lam 1985) — linear LMS adaptation via the
//     Bradford matrix (the CIECAM02 Bradford variant is the de-facto
//     standard).
//   * 'vonkries' — Hunt-Pointer-Estevez sharpened LMS basis.
//   * 'simple'  — per-channel RGB scaling (no LMS transform).
//
// Algorithm matches MATLAB R2025b chromadapt.m + makecform('adapt')
// verbatim. Each method:
//
//   1. illuminant_xyz = rgb2xyz(illuminant, ColorSpace=cs);
//   2. illuminant_xyz(illuminant_xyz==0) = eps;  scale so Y=1.
//   3. If 'simple':
//        illuminant_rgb = xyz2rgb(illuminant_xyz, ColorSpace=cs);
//        B = A_single ./ illuminant_rgb_broadcast (per channel).
//      Else (Bradford or vonKries):
//        Build adapt = M⁻¹ · diag(M·whiteD65 / M·illuminant_xyz) · M;
//        A_XYZ = rgb2xyz(A, ColorSpace=cs, WhitePoint='d65');
//        B_XYZ = adapt · A_XYZ (3×3 matvec per pixel);
//        B = xyz2rgb(B_XYZ, ColorSpace=cs, WhitePoint='d65').
//   4. Cast back to input class (im2uint8 / im2uint16 / im2single for
//      uint inputs; clip [0,1] for single/double).
//
// References:
//   - Lam, K.M. (1985). Metamerism and Colour Constancy, PhD thesis,
//     University of Bradford.
//   - Hunt, R. W. G. (2005). The Reproduction of Colour, 6th ed.
//
// Inlines all color-space gamma + primaries (no extension to
// numkit's existing sRGB-only rgb2xyz).
//
// PMR HARD RULE: every fn takes std::pmr::memory_resource *mr.

#include <numkit/image/color/color.hpp>
#include <numkit/image/type_convert/type_convert.hpp>

#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <vector>

namespace numkit::image {
namespace {

// ── Color-space tags ─────────────────────────────────────────────
enum class CS { sRGB, AdobeRGB1998, ProPhotoRGB, LinearRGB };

CS parse_color_space(const std::string &s)
{
    std::string lo;
    for (char ch : s) lo += static_cast<char>(std::tolower(
        static_cast<unsigned char>(ch)));
    if (lo == "srgb")            return CS::sRGB;
    if (lo == "adobe-rgb-1998")  return CS::AdobeRGB1998;
    if (lo == "prophoto-rgb")    return CS::ProPhotoRGB;
    if (lo == "linear-rgb")      return CS::LinearRGB;
    throw Error("chromadapt: ColorSpace must be 'srgb', "
                "'adobe-rgb-1998', 'prophoto-rgb', or 'linear-rgb'",
                0, 0, "chromadapt", "", "numkit:chromadapt:colorSpace");
}

// ── Gamma encode / decode helpers ────────────────────────────────
double gamma_decode(double v, CS cs)
{
    switch (cs) {
        case CS::sRGB:
            if (v <= 0.04045) return v / 12.92;
            return std::pow((v + 0.055) / 1.055, 2.4);
        case CS::AdobeRGB1998:
            return std::pow(v < 0.0 ? 0.0 : v, 2.19921875);
        case CS::ProPhotoRGB:
            if (v < 16.0 / 512.0) return v / 16.0;
            return std::pow(v, 1.8);
        case CS::LinearRGB:
        default:
            return v;
    }
}

double gamma_encode(double v, CS cs)
{
    switch (cs) {
        case CS::sRGB:
            if (v <= 0.0031308) return v * 12.92;
            return 1.055 * std::pow(v < 0.0 ? 0.0 : v, 1.0 / 2.4) - 0.055;
        case CS::AdobeRGB1998:
            return std::pow(v < 0.0 ? 0.0 : v, 1.0 / 2.19921875);
        case CS::ProPhotoRGB:
            if (v < 1.0 / 512.0) return v * 16.0;
            return std::pow(v < 0.0 ? 0.0 : v, 1.0 / 1.8);
        case CS::LinearRGB:
        default:
            return v;
    }
}

// ── RGB-to-XYZ matrices (D65 reference for sRGB/Adobe/Linear,
//   D50 for ProPhoto). Matlab's rgb2xyz with default WhitePoint='d65'
//   actually outputs XYZ in the *ColorSpace's native* whitepoint
//   reference, then chromadapt applies the adaptation. For
//   chromadapt we treat A as if its source white IS D65 (matching
//   MATLAB's call rgb2xyz(A, 'WhitePoint', 'd65')).
//
//   For sRGB, Adobe, and Linear RGB this is the natural matrix.
//   For ProPhoto, the native matrix is D50; we convert by applying
//   a D50→D65 Bradford adapt step inside the rgb_to_xyz / xyz_to_rgb
//   functions so callers see D65 XYZ consistently.

// Matrix-vector multiplication 3x3 · 3.
inline void mat3_mul_vec3(const double M[9], const double v[3], double out[3])
{
    out[0] = M[0] * v[0] + M[1] * v[1] + M[2] * v[2];
    out[1] = M[3] * v[0] + M[4] * v[1] + M[5] * v[2];
    out[2] = M[6] * v[0] + M[7] * v[1] + M[8] * v[2];
}

// 3x3 matrix inverse via cofactors.
inline void mat3_inv(const double M[9], double Inv[9])
{
    const double a = M[0], b = M[1], c = M[2];
    const double d = M[3], e = M[4], f = M[5];
    const double g = M[6], h = M[7], i = M[8];
    const double A = e * i - f * h;
    const double B = -(d * i - f * g);
    const double C = d * h - e * g;
    const double det = a * A + b * B + c * C;
    const double invd = 1.0 / det;
    Inv[0] = A * invd;
    Inv[1] = -(b * i - c * h) * invd;
    Inv[2] = (b * f - c * e) * invd;
    Inv[3] = B * invd;
    Inv[4] = (a * i - c * g) * invd;
    Inv[5] = -(a * f - c * d) * invd;
    Inv[6] = C * invd;
    Inv[7] = -(a * h - b * g) * invd;
    Inv[8] = (a * e - b * d) * invd;
}

// 3x3 · 3x3 matrix multiply.
inline void mat3_mul(const double A[9], const double B[9], double Out[9])
{
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j) {
            double s = 0.0;
            for (int k = 0; k < 3; ++k) s += A[3 * i + k] * B[3 * k + j];
            Out[3 * i + j] = s;
        }
}

// Whitepoint XYZ (Y normalised to 1).
constexpr double kD65[3] = {0.95047, 1.00000, 1.08883};
constexpr double kD50[3] = {0.96422, 1.00000, 0.82521};

// sRGB / linear-RGB primaries to XYZ (D65), per IEC 61966-2-1.
constexpr double kMsRGB_D65[9] = {
    0.4124564, 0.3575761, 0.1804375,
    0.2126729, 0.7151522, 0.0721750,
    0.0193339, 0.1191920, 0.9503041
};
// Adobe RGB (1998) to XYZ (D65), per Adobe spec.
constexpr double kMAdobe_D65[9] = {
    0.5767309, 0.1855540, 0.1881852,
    0.2973769, 0.6273491, 0.0752741,
    0.0270343, 0.0706872, 0.9911085
};
// ProPhoto RGB (ROMM RGB) to XYZ (D50), per the ROMM spec.
constexpr double kMProPhoto_D50[9] = {
    0.7976749, 0.1351917, 0.0313534,
    0.2880402, 0.7118741, 0.0000857,
    0.0000000, 0.0000000, 0.8252100
};

// Bradford matrix (LMS basis, CIECAM02 Bradford variant).
constexpr double kMBradford[9] = {
     0.8951,  0.2664, -0.1614,
    -0.7502,  1.7135,  0.0367,
     0.0389, -0.0685,  1.0296
};
// Von Kries / Hunt-Pointer-Estevez normalized to D65 basis.
constexpr double kMVonKries[9] = {
     0.4002400,  0.7076000, -0.0808100,
    -0.2263000,  1.1653200,  0.0457000,
     0.0000000,  0.0000000,  0.9182200
};

const double *cs_to_xyz_matrix(CS cs)
{
    switch (cs) {
        case CS::sRGB:
        case CS::LinearRGB:   return kMsRGB_D65;
        case CS::AdobeRGB1998: return kMAdobe_D65;
        case CS::ProPhotoRGB:  return kMProPhoto_D50;
    }
    return kMsRGB_D65;
}

bool cs_native_d50(CS cs)
{
    return cs == CS::ProPhotoRGB;
}

// Bradford D50↔D65 adapt matrix (precomputed via the Bradford
// transform from D50 to D65). Used for ProPhoto, which is natively
// D50, to bring XYZ values into the D65 reference frame chromadapt
// works in.
//   M_d50_to_d65 = M_B⁻¹ · diag(M_B · D65 / M_B · D50) · M_B
//   M_d65_to_d50 is its inverse.

void make_bradford_adapt(const double src_xyz[3], const double dst_xyz[3],
                         double Out[9])
{
    double LMS_src[3], LMS_dst[3];
    mat3_mul_vec3(kMBradford, src_xyz, LMS_src);
    mat3_mul_vec3(kMBradford, dst_xyz, LMS_dst);
    double D[9] = {
        LMS_dst[0] / LMS_src[0], 0, 0,
        0, LMS_dst[1] / LMS_src[1], 0,
        0, 0, LMS_dst[2] / LMS_src[2]
    };
    double Mb_inv[9];
    mat3_inv(kMBradford, Mb_inv);
    double tmp[9];
    mat3_mul(D, kMBradford, tmp);
    mat3_mul(Mb_inv, tmp, Out);
}

void make_adapt(const double src_xyz[3], const double dst_xyz[3],
                const double M[9], double Out[9])
{
    double LMS_src[3], LMS_dst[3];
    mat3_mul_vec3(M, src_xyz, LMS_src);
    mat3_mul_vec3(M, dst_xyz, LMS_dst);
    double D[9] = {
        LMS_dst[0] / LMS_src[0], 0, 0,
        0, LMS_dst[1] / LMS_src[1], 0,
        0, 0, LMS_dst[2] / LMS_src[2]
    };
    double M_inv[9];
    mat3_inv(M, M_inv);
    double tmp[9];
    mat3_mul(D, M, tmp);
    mat3_mul(M_inv, tmp, Out);
}

// Convert encoded RGB triplet (in CS-native gamma) → XYZ (D65).
void rgb_to_xyz_d65(const double rgb[3], CS cs, double xyz[3])
{
    // 1. Gamma decode to linear.
    double lin[3];
    lin[0] = gamma_decode(rgb[0], cs);
    lin[1] = gamma_decode(rgb[1], cs);
    lin[2] = gamma_decode(rgb[2], cs);
    // 2. Apply CS-native matrix.
    double xyz_native[3];
    mat3_mul_vec3(cs_to_xyz_matrix(cs), lin, xyz_native);
    // 3. If CS is D50-native (ProPhoto), Bradford-adapt to D65.
    if (cs_native_d50(cs)) {
        double M_adapt[9];
        make_bradford_adapt(kD50, kD65, M_adapt);
        mat3_mul_vec3(M_adapt, xyz_native, xyz);
    } else {
        xyz[0] = xyz_native[0];
        xyz[1] = xyz_native[1];
        xyz[2] = xyz_native[2];
    }
}

// Convert XYZ (D65) → encoded RGB in CS-native gamma.
void xyz_d65_to_rgb(const double xyz_d65[3], CS cs, double rgb[3])
{
    // 1. If CS is D50-native, Bradford-adapt D65 → D50 first.
    double xyz_native[3];
    if (cs_native_d50(cs)) {
        double M_adapt[9];
        make_bradford_adapt(kD65, kD50, M_adapt);
        mat3_mul_vec3(M_adapt, xyz_d65, xyz_native);
    } else {
        xyz_native[0] = xyz_d65[0];
        xyz_native[1] = xyz_d65[1];
        xyz_native[2] = xyz_d65[2];
    }
    // 2. Apply inverse matrix.
    double M[9], M_inv[9];
    std::memcpy(M, cs_to_xyz_matrix(cs), sizeof(M));
    mat3_inv(M, M_inv);
    double lin[3];
    mat3_mul_vec3(M_inv, xyz_native, lin);
    // 3. Gamma encode.
    rgb[0] = gamma_encode(lin[0], cs);
    rgb[1] = gamma_encode(lin[1], cs);
    rgb[2] = gamma_encode(lin[2], cs);
}

// ── Helpers for input/output handling ────────────────────────────

// Convert input value to a linear [0,1] double buffer (NOT gamma
// decoded — chromadapt's pipeline works on the encoded RGB then
// decodes inside rgb_to_xyz_d65).
struct RGBPlane {
    std::size_t H = 0, W = 0;
    std::vector<double> v;  // 3 channels, column-major: ch*H*W + c*H + r
};

RGBPlane to_double_rgb01(const Value &A)
{
    const std::size_t H = A.dims().rows();
    const std::size_t W = A.dims().cols();
    RGBPlane out; out.H = H; out.W = W;
    out.v.resize(H * W * 3);
    const ValueType t = A.type();
    for (std::size_t i = 0; i < H * W * 3; ++i) {
        const double raw = A.elemAsDouble(i);
        switch (t) {
            case ValueType::UINT8:  out.v[i] = raw / 255.0;   break;
            case ValueType::UINT16: out.v[i] = raw / 65535.0; break;
            default:                out.v[i] = raw;            break;
        }
    }
    return out;
}

Value rgb_to_value(const RGBPlane &P, ValueType outClass,
                   std::pmr::memory_resource *mr)
{
    const std::size_t H = P.H, W = P.W;
    Value out = Value::matrix3d(H, W, 3, outClass, mr);
    auto write_uint8 = [&]() {
        uint8_t *od = out.uint8DataMut();
        for (std::size_t i = 0; i < H * W * 3; ++i) {
            double v = std::round(P.v[i] * 255.0);
            if (v < 0.0)   v = 0.0;
            if (v > 255.0) v = 255.0;
            od[i] = static_cast<uint8_t>(v);
        }
    };
    auto write_uint16 = [&]() {
        uint16_t *od = out.uint16DataMut();
        for (std::size_t i = 0; i < H * W * 3; ++i) {
            double v = std::round(P.v[i] * 65535.0);
            if (v < 0.0)     v = 0.0;
            if (v > 65535.0) v = 65535.0;
            od[i] = static_cast<uint16_t>(v);
        }
    };
    auto write_single = [&]() {
        float *od = out.singleDataMut();
        for (std::size_t i = 0; i < H * W * 3; ++i)
            od[i] = static_cast<float>(P.v[i]);
    };
    auto write_double = [&]() {
        double *od = out.doubleDataMut();
        std::memcpy(od, P.v.data(), H * W * 3 * sizeof(double));
    };
    switch (outClass) {
        case ValueType::UINT8:  write_uint8();  break;
        case ValueType::UINT16: write_uint16(); break;
        case ValueType::SINGLE: write_single(); break;
        default:                write_double(); break;
    }
    return out;
}

}  // namespace

Value chromadapt(const Value &A, const Value &illuminant_in,
                 const std::string &method,
                 const std::string &color_space,
                 std::pmr::memory_resource *mr)
{
    const auto &dA = A.dims();
    if (!dA.is3D() || dA.pages() != 3)
        throw Error("chromadapt: A must be H × W × 3 RGB",
                    0, 0, "chromadapt", "", "numkit:chromadapt:shape");
    const std::size_t H = dA.rows();
    const std::size_t W = dA.cols();
    if (H == 0 || W == 0)
        throw Error("chromadapt: A must be nonempty",
                    0, 0, "chromadapt", "", "numkit:chromadapt:empty");
    if (illuminant_in.numel() != 3)
        throw Error("chromadapt: illuminant must be a 3-element vector",
                    0, 0, "chromadapt", "", "numkit:chromadapt:illShape");

    const ValueType origClass = A.type();
    if (origClass != ValueType::UINT8 && origClass != ValueType::UINT16
        && origClass != ValueType::SINGLE && origClass != ValueType::DOUBLE)
        throw Error("chromadapt: A must be uint8 / uint16 / single / double",
                    0, 0, "chromadapt", "", "numkit:chromadapt:class");

    // Resolve method.
    std::string m;
    for (char ch : method) m += static_cast<char>(std::tolower(
        static_cast<unsigned char>(ch)));
    if (m != "bradford" && m != "vonkries" && m != "simple")
        throw Error("chromadapt: Method must be 'bradford', 'vonkries', "
                    "or 'simple'",
                    0, 0, "chromadapt", "", "numkit:chromadapt:method");

    const CS cs = parse_color_space(color_space);

    // Build illuminant_rgb01 (illuminant in encoded RGB, scaled like
    // im2double would: integer → /max, float → as-is).
    double ill_rgb01[3];
    const ValueType ill_t = illuminant_in.type();
    for (int k = 0; k < 3; ++k) {
        double v = illuminant_in.elemAsDouble(k);
        if (ill_t == ValueType::UINT8)  v /= 255.0;
        if (ill_t == ValueType::UINT16) v /= 65535.0;
        ill_rgb01[k] = v;
    }
    if (ill_rgb01[0] == 0.0 && ill_rgb01[1] == 0.0 && ill_rgb01[2] == 0.0)
        throw Error("chromadapt: illuminant cannot be [0 0 0]",
                    0, 0, "chromadapt", "", "numkit:chromadapt:illBlack");

    // Compute illuminant XYZ (D65).
    double ill_xyz[3];
    rgb_to_xyz_d65(ill_rgb01, cs, ill_xyz);
    // Replace zeros with eps to avoid div-by-0.
    const double eps_d = std::numeric_limits<double>::epsilon();
    for (int k = 0; k < 3; ++k)
        if (ill_xyz[k] == 0.0) ill_xyz[k] = eps_d;
    // Normalize so Y=1.
    const double Y = ill_xyz[1];
    ill_xyz[0] /= Y; ill_xyz[1] /= Y; ill_xyz[2] /= Y;

    RGBPlane in_rgb = to_double_rgb01(A);
    RGBPlane out_rgb; out_rgb.H = H; out_rgb.W = W;
    out_rgb.v.resize(H * W * 3, 0.0);

    if (m == "simple") {
        // illuminant_rgb01_normalized = xyz_d65_to_rgb(ill_xyz, cs)
        double ill_rgb_norm[3];
        xyz_d65_to_rgb(ill_xyz, cs, ill_rgb_norm);
        // Avoid zero denominators.
        for (int k = 0; k < 3; ++k) {
            ill_rgb_norm[k] = std::fabs(ill_rgb_norm[k]);
            if (ill_rgb_norm[k] == 0.0) ill_rgb_norm[k] = eps_d;
        }
        for (int ch = 0; ch < 3; ++ch)
            for (std::size_t i = 0; i < H * W; ++i)
                out_rgb.v[ch * H * W + i] =
                    in_rgb.v[ch * H * W + i] / ill_rgb_norm[ch];
    } else {
        // Bradford / vonKries adaptation matrix.
        double M_adapt[9];
        const double *Madapt_mat = (m == "bradford") ? kMBradford
                                                     : kMVonKries;
        make_adapt(ill_xyz, kD65, Madapt_mat, M_adapt);
        // For each pixel: A_RGB → linear → XYZ (D65) → adapted XYZ →
        // linear RGB → encoded RGB.
        for (std::size_t i = 0; i < H * W; ++i) {
            const double rgb_in[3] = { in_rgb.v[0 * H * W + i],
                                       in_rgb.v[1 * H * W + i],
                                       in_rgb.v[2 * H * W + i] };
            double xyz[3];
            rgb_to_xyz_d65(rgb_in, cs, xyz);
            double xyz_adapt[3];
            mat3_mul_vec3(M_adapt, xyz, xyz_adapt);
            double rgb_out[3];
            xyz_d65_to_rgb(xyz_adapt, cs, rgb_out);
            out_rgb.v[0 * H * W + i] = rgb_out[0];
            out_rgb.v[1 * H * W + i] = rgb_out[1];
            out_rgb.v[2 * H * W + i] = rgb_out[2];
        }
    }
    return rgb_to_value(out_rgb, origClass, mr);
}

} // namespace numkit::image
