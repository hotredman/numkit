// libs/image/src/type_convert/type_convert.cpp

#include <numkit/image/type_convert/type_convert.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

namespace numkit::image {

namespace {

template <typename T>
inline T satCast(double v) {
    if (std::isnan(v)) return T{};
    constexpr double lo = static_cast<double>(std::numeric_limits<T>::lowest());
    constexpr double hi = static_cast<double>(std::numeric_limits<T>::max());
    if (v <= lo) return std::numeric_limits<T>::lowest();
    if (v >= hi) return std::numeric_limits<T>::max();
    return static_cast<T>(std::lround(v));
}

inline Value alloc_like(const Value &x, ValueType t, std::pmr::memory_resource *mr) {
    const auto &d = x.dims();
    if (x.isScalar()) return Value::scalar(0.0, mr);
    if (d.is3D()) return Value::matrix3d(d.rows(), d.cols(), d.pages(), t, mr);
    return Value::matrix(d.rows(), d.cols(), t, mr);
}

// Convert one element of source type to a value in [0, 1] (double).
inline double element_to_unit(const Value &x, size_t i) {
    const double v = x.elemAsDouble(i);
    switch (x.type()) {
        case ValueType::DOUBLE:
        case ValueType::SINGLE:  return v;                       // already [0,1]
        case ValueType::UINT8:   return v / 255.0;
        case ValueType::UINT16:  return v / 65535.0;
        case ValueType::INT16:   return (v + 32768.0) / 65535.0;
        case ValueType::LOGICAL: return v != 0.0 ? 1.0 : 0.0;
        default:                 return v;
    }
}

// Generic float-output helper.
template <typename Float>
Value to_float_impl(const Value &x, ValueType target, std::pmr::memory_resource *mr) {
    const size_t n = x.numel();
    Value out;
    if (x.isScalar()) {
        out = Value::matrix(1, 1, target, mr);
    } else {
        out = alloc_like(x, target, mr);
    }
    if (n == 0) return out;

    Float *od = nullptr;
    if constexpr (std::is_same_v<Float, double>) od = out.doubleDataMut();
    else                                          od = out.singleDataMut();
    for (size_t i = 0; i < n; ++i)
        od[i] = static_cast<Float>(element_to_unit(x, i));
    return out;
}

// Saturating int-output. Float source: round(unit · scale) + bias_after_round
// (MATLAB rounds the un-shifted product first, then subtracts the bias). Using
// the shifted intermediate would corrupt half-integer cases (e.g. 0.5*65535
// → 32767.5; rounded gives 32768, then -32768 → 0; the alternative
// rounds (32767.5 - 32768) = -0.5 → -1, which is wrong).
template <typename Int>
Value to_int_impl(const Value &x, ValueType target, double scale, double bias_after, std::pmr::memory_resource *mr) {
    const size_t n = x.numel();
    Value out;
    if (x.isScalar()) out = Value::matrix(1, 1, target, mr);
    else              out = alloc_like(x, target, mr);
    if (n == 0) return out;

    Int *od = nullptr;
    if      constexpr (std::is_same_v<Int, uint8_t>)  od = out.uint8DataMut();
    else if constexpr (std::is_same_v<Int, uint16_t>) od = out.uint16DataMut();
    else if constexpr (std::is_same_v<Int, int16_t>)  od = out.int16DataMut();
    else                                              od = nullptr;

    // Float → int: scale unit value by `scale` then round, plus optional bias.
    // Int → int: use proportional rescale (handled per-source below for accuracy).
    switch (x.type()) {
        case ValueType::DOUBLE:
        case ValueType::SINGLE:
        case ValueType::LOGICAL:
            for (size_t i = 0; i < n; ++i) {
                double u = element_to_unit(x, i);  // already in [0,1]
                if (std::isnan(u)) { od[i] = Int{}; continue; }
                if (u < 0.0) u = 0.0;
                if (u > 1.0) u = 1.0;
                // Round before applying the integer-class shift.
                const double rounded = std::lround(u * scale);
                od[i] = satCast<Int>(rounded + bias_after);
            }
            break;
        case ValueType::UINT8: {
            const uint8_t *src = x.uint8Data();
            if constexpr (std::is_same_v<Int, uint8_t>) {
                for (size_t i = 0; i < n; ++i) od[i] = src[i];
            } else if constexpr (std::is_same_v<Int, uint16_t>) {
                // 0xAB → 0xABAB (bit-replication).
                for (size_t i = 0; i < n; ++i) od[i] = (uint16_t)((src[i] << 8) | src[i]);
            } else { // int16
                for (size_t i = 0; i < n; ++i) {
                    const uint16_t u = (uint16_t)((src[i] << 8) | src[i]);
                    od[i] = satCast<int16_t>((double)u - 32768.0);
                }
            }
            break;
        }
        case ValueType::UINT16: {
            const uint16_t *src = x.uint16Data();
            if constexpr (std::is_same_v<Int, uint16_t>) {
                for (size_t i = 0; i < n; ++i) od[i] = src[i];
            } else if constexpr (std::is_same_v<Int, uint8_t>) {
                // Round high byte: round(src/257).
                for (size_t i = 0; i < n; ++i)
                    od[i] = satCast<uint8_t>((double)src[i] / 257.0);
            } else { // int16
                for (size_t i = 0; i < n; ++i)
                    od[i] = satCast<int16_t>((double)src[i] - 32768.0);
            }
            break;
        }
        case ValueType::INT16: {
            const int16_t *src = x.int16Data();
            if constexpr (std::is_same_v<Int, int16_t>) {
                for (size_t i = 0; i < n; ++i) od[i] = src[i];
            } else if constexpr (std::is_same_v<Int, uint16_t>) {
                for (size_t i = 0; i < n; ++i)
                    od[i] = satCast<uint16_t>((double)src[i] + 32768.0);
            } else { // uint8
                for (size_t i = 0; i < n; ++i) {
                    const double u16 = (double)src[i] + 32768.0;
                    od[i] = satCast<uint8_t>(u16 / 257.0);
                }
            }
            break;
        }
        default:
            throw Error("im2int: unsupported source class", 0, 0, "im2int", "",
                        "numkit:im2int:badtype");
    }
    return out;
}

} // anonymous

Value im2double(const Value &x, std::pmr::memory_resource *mr) {
    if (x.type() == ValueType::DOUBLE) return x;
    return to_float_impl<double>(x, ValueType::DOUBLE, mr);
}

Value im2single(const Value &x, std::pmr::memory_resource *mr) {
    if (x.type() == ValueType::SINGLE) return x;
    return to_float_impl<float>(x, ValueType::SINGLE, mr);
}

Value im2uint8(const Value &x, std::pmr::memory_resource *mr) {
    if (x.type() == ValueType::UINT8) return x;
    return to_int_impl<uint8_t>(x, ValueType::UINT8, 255.0, 0.0, mr);
}

Value im2uint16(const Value &x, std::pmr::memory_resource *mr) {
    if (x.type() == ValueType::UINT16) return x;
    return to_int_impl<uint16_t>(x, ValueType::UINT16, 65535.0, 0.0, mr);
}

Value im2int16(const Value &x, std::pmr::memory_resource *mr) {
    if (x.type() == ValueType::INT16) return x;
    // [0, 1] float → [-32768, 32767]: round(x*65535) - 32768.
    return to_int_impl<int16_t>(x, ValueType::INT16, 65535.0, -32768.0, mr);
}

Value mat2gray(const Value &x, double lo, double hi, std::pmr::memory_resource *mr)
{
    const size_t n = x.numel();
    if (std::isnan(lo) || std::isnan(hi)) {
        // Auto-detect range, ignoring NaNs.
        lo =  std::numeric_limits<double>::infinity();
        hi = -std::numeric_limits<double>::infinity();
        for (size_t i = 0; i < n; ++i) {
            const double v = x.elemAsDouble(i);
            if (std::isnan(v)) continue;
            if (v < lo) lo = v;
            if (v > hi) hi = v;
        }
        if (!std::isfinite(lo) || !std::isfinite(hi)) {
            lo = 0.0; hi = 0.0;
        }
    }
    Value out = alloc_like(x, ValueType::DOUBLE, mr);
    if (x.isScalar()) out = Value::matrix(1, 1, ValueType::DOUBLE, mr);
    if (n == 0) return out;
    double *od = out.doubleDataMut();
    if (lo == hi) {
        // MATLAB sets all entries ≤ lo to 0 and others to 1; for uniform
        // input that's all-0 unless any element exceeds hi (no element does).
        for (size_t i = 0; i < n; ++i) od[i] = 0.0;
        return out;
    }
    const double inv = 1.0 / (hi - lo);
    for (size_t i = 0; i < n; ++i) {
        double v = (x.elemAsDouble(i) - lo) * inv;
        if (v < 0.0) v = 0.0;
        if (v > 1.0) v = 1.0;
        od[i] = v;
    }
    return out;
}

Value rgb2gray(const Value &x, std::pmr::memory_resource *mr)
{
    const auto &d = x.dims();
    if (!d.is3D() || d.pages() != 3)
        throw Error("rgb2gray: input must be H×W×3", 0, 0, "rgb2gray", "",
                    "numkit:rgb2gray:size");
    const size_t H = d.rows(), W = d.cols();
    const size_t plane = H * W;
    Value out = Value::matrix(H, W, x.type(), mr);

    auto pix = [&](size_t i, size_t p){ return x.elemAsDouble(p * plane + i); };
    auto store = [&](size_t i, double v) {
        switch (x.type()) {
            case ValueType::DOUBLE: out.doubleDataMut()[i] = v; break;
            case ValueType::SINGLE: out.singleDataMut()[i] = (float)v; break;
            case ValueType::UINT8:  out.uint8DataMut()[i]  = satCast<uint8_t>(v); break;
            case ValueType::UINT16: out.uint16DataMut()[i] = satCast<uint16_t>(v); break;
            case ValueType::INT16:  out.int16DataMut()[i]  = satCast<int16_t>(v); break;
            default:
                throw Error("rgb2gray: unsupported class", 0, 0, "rgb2gray", "",
                            "numkit:rgb2gray:badtype");
        }
    };

    // Rec. 601 coefficients (MATLAB convention).
    constexpr double Cr = 0.2989, Cg = 0.5870, Cb = 0.1140;
    for (size_t i = 0; i < plane; ++i) {
        const double y = Cr * pix(i, 0) + Cg * pix(i, 1) + Cb * pix(i, 2);
        store(i, y);
    }
    return out;
}

Value im2gray(const Value &x, std::pmr::memory_resource *mr) {
    const auto &d = x.dims();
    if (d.is3D() && d.pages() == 3) return rgb2gray(x, mr);
    return x;  // already grayscale (or unsupported shape — pass through)
}

namespace {

bool spatial_pages_eq(const Value &x, size_t want) {
    const auto &d = x.dims();
    if (d.is3D()) return d.pages() == want;
    return want == 1;
}

bool is_int_image_class(ValueType t) {
    return t == ValueType::UINT8 || t == ValueType::UINT16 ||
           t == ValueType::INT16;
}

bool is_float_unit_image(const Value &x) {
    if (x.type() != ValueType::DOUBLE && x.type() != ValueType::SINGLE)
        return false;
    const size_t N = x.numel();
    bool any_real = false;
    for (size_t i = 0; i < N; ++i) {
        const double v = x.elemAsDouble(i);
        if (std::isnan(v)) continue;
        if (v < 0.0 || v > 1.0) return false;
        any_real = true;
    }
    return any_real;
}

bool is_pos_int_float(const Value &x) {
    if (x.type() != ValueType::DOUBLE && x.type() != ValueType::SINGLE)
        return false;
    const size_t N = x.numel();
    if (N == 0) return false;
    for (size_t i = 0; i < N; ++i) {
        const double v = x.elemAsDouble(i);
        if (!std::isfinite(v)) return false;
        if (v < 1.0 || v != std::floor(v)) return false;
    }
    return true;
}

bool all_zero_or_one(const Value &x) {
    const size_t N = x.numel();
    for (size_t i = 0; i < N; ++i) {
        const double v = x.elemAsDouble(i);
        if (std::isnan(v)) return false;
        if (v != 0.0 && v != 1.0) return false;
    }
    return true;
}

inline Value bool_scalar(bool b, std::pmr::memory_resource *mr) {
    Value out = Value::matrix(1, 1, ValueType::LOGICAL, mr);
    out.logicalDataMut()[0] = b ? 1u : 0u;
    return out;
}

} // anonymous

Value iptnum2ordinal(double n, std::pmr::memory_resource *mr)
{
    if (!std::isfinite(n) || n <= 0.0 || n != std::floor(n))
        throw Error("iptnum2ordinal: num must be a real positive integer",
                    0, 0, "iptnum2ordinal", "", "numkit:iptnum2ordinal:n");

    static const char *kWords[] = {
        nullptr,    "first",      "second",   "third",
        "fourth",   "fifth",      "sixth",    "seventh",
        "eighth",   "ninth",      "tenth",    "eleventh",
        "twelfth",  "thirteenth", "fourteenth","fifteenth",
        "sixteenth","seventeenth","eighteenth","nineteenth",
        "twentieth"
    };

    const long long ll = static_cast<long long>(n);
    std::string s;
    if (ll >= 1 && ll <= 20) {
        s = kWords[ll];
    } else {
        s = std::to_string(ll);
        const char last = s.back();
        const char *suffix = "th";
        if      (last == '1') suffix = "st";
        else if (last == '2') suffix = "nd";
        else if (last == '3') suffix = "rd";
        s += suffix;
    }
    return Value::fromString(s, mr);
}

Value imcast(const Value &I, const std::string &type, std::pmr::memory_resource *mr)
{
    std::string lo;
    lo.reserve(type.size());
    for (char c : type) lo.push_back(static_cast<char>(std::tolower(c)));

    auto cls_of = [](ValueType t) -> const char * {
        switch (t) {
            case ValueType::DOUBLE:  return "double";
            case ValueType::SINGLE:  return "single";
            case ValueType::UINT8:   return "uint8";
            case ValueType::UINT16:  return "uint16";
            case ValueType::INT16:   return "int16";
            case ValueType::LOGICAL: return "logical";
            default:                 return "?";
        }
    };

    if (lo == cls_of(I.type())) return I;

    if (lo == "double")  return im2double(I, mr);
    if (lo == "single")  return im2single(I, mr);
    if (lo == "uint8")   return im2uint8(I, mr);
    if (lo == "uint16")  return im2uint16(I, mr);
    if (lo == "int16")   return im2int16(I, mr);
    if (lo == "logical") {
        const auto &d = I.dims();
        Value out = d.is3D()
            ? Value::matrix3d(d.rows(), d.cols(), d.pages(),
                              ValueType::LOGICAL, mr)
            : Value::matrix(d.rows(), d.cols(), ValueType::LOGICAL, mr);
        const size_t N = I.numel();
        std::uint8_t *od = out.logicalDataMut();
        for (size_t i = 0; i < N; ++i) {
            const double v = I.elemAsDouble(i);
            od[i] = (v != 0.0 && !std::isnan(v)) ? 1u : 0u;
        }
        return out;
    }
    throw Error("imcast: unsupported TYPE", 0, 0, "imcast", "",
                "numkit:imcast:type");
}

namespace {

Value gray_colormap(int n, std::pmr::memory_resource *mr)
{
    Value map = Value::matrix(static_cast<size_t>(n), 3,
                              ValueType::DOUBLE, mr);
    double *md = map.doubleDataMut();
    if (n <= 0) return map;
    if (n == 1) {
        md[0] = md[1] = md[2] = 0.0;
        return map;
    }
    for (int i = 0; i < n; ++i) {
        const double v = static_cast<double>(i) / static_cast<double>(n - 1);
        md[0 * n + i] = v;
        md[1 * n + i] = v;
        md[2 * n + i] = v;
    }
    return map;
}

} // anonymous

std::tuple<Value, Value>
gray2ind(const Value &I, int n, std::pmr::memory_resource *mr)
{
    if (n < 1 || n > 65536)
        throw Error("gray2ind: N must be in [1, 65536]",
                    0, 0, "gray2ind", "", "numkit:gray2ind:n");

    const ValueType cls = I.type();
    const auto &d = I.dims();
    const size_t H = d.rows();
    const size_t W = d.cols();
    const size_t N = I.numel();
    const ValueType outT = (n <= 256) ? ValueType::UINT8
                                       : ValueType::UINT16;
    Value out = d.is3D()
        ? Value::matrix3d(H, W, d.pages(), outT, mr)
        : Value::matrix(H, W, outT, mr);
    Value map = gray_colormap(n, mr);
    if (N == 0) return {std::move(out), std::move(map)};

    double low = 0.0, scale = 1.0;
    const bool isFloat = (cls == ValueType::DOUBLE || cls == ValueType::SINGLE);
    switch (cls) {
        case ValueType::UINT8:  scale = 255.0;                       break;
        case ValueType::UINT16: scale = 65535.0;                     break;
        case ValueType::INT16:  low = -32768.0; scale = 65535.0;     break;
        case ValueType::DOUBLE:
        case ValueType::SINGLE:
        case ValueType::LOGICAL: scale = 1.0;                        break;
        default:
            throw Error("gray2ind: unsupported class",
                        0, 0, "gray2ind", "", "numkit:gray2ind:cls");
    }
    if (isFloat) {
        for (size_t i = 0; i < N; ++i) {
            const double v = I.elemAsDouble(i);
            if (v < 0.0 || v > 1.0)
                throw Error("gray2ind: float values must be in [0, 1]",
                            0, 0, "gray2ind", "", "numkit:gray2ind:range");
        }
    }
    const double k = static_cast<double>(n - 1) / scale;
    // MATLAB / Octave's integer cast rounds half-away-from-zero, not
    // truncates toward zero.
    if (outT == ValueType::UINT8) {
        std::uint8_t *od = out.uint8DataMut();
        for (size_t i = 0; i < N; ++i) {
            double v = std::lround((I.elemAsDouble(i) - low) * k);
            if (v < 0)   v = 0;
            if (v > 255) v = 255;
            od[i] = static_cast<std::uint8_t>(v);
        }
    } else {
        std::uint16_t *od = out.uint16DataMut();
        for (size_t i = 0; i < N; ++i) {
            double v = std::lround((I.elemAsDouble(i) - low) * k);
            if (v < 0)     v = 0;
            if (v > 65535) v = 65535;
            od[i] = static_cast<std::uint16_t>(v);
        }
    }
    return {std::move(out), std::move(map)};
}

Value ind2gray(const Value &idx, const Value &map, std::pmr::memory_resource *mr)
{
    // MATLAB ind2gray:
    //   1. graycm = rgb2gray(MAP)         — per-row YIQ luma, double in [0,1]
    //   2. graycm = graycm(:,1)            — N×1 grey vector
    //   3. Class-preserving lookup:
    //        - double / single X: clamp(X, 1, N) then I = graycm(X)
    //        - uint8/uint16  X: build LUT in target class (changeClass via
    //          `uint8/uint16(round(graycm·SCALE))`), padded with last
    //          entry to vs = 256 / 65536, then intlut.
    //   4. Output class equals input class.
    //
    // Coefficients match MATLAB rgb2gray (Rec. BT.601 YIQ luma):
    //   Y = 0.298936021293775·R + 0.587043074451121·G + 0.114020904255103·B.
    if (map.numel() == 0)
        throw Error("ind2gray: requires (X, MAP) with non-empty MAP",
                    0, 0, "ind2gray", "", "numkit:ind2gray:nargin");
    if (map.dims().cols() != 3 || map.dims().is3D())
        throw Error("ind2gray: MAP must be N-by-3",
                    0, 0, "ind2gray", "", "numkit:ind2gray:map");

    const int M = static_cast<int>(map.dims().rows());
    if (M < 1)
        throw Error("ind2gray: MAP must have at least one row",
                    0, 0, "ind2gray", "", "numkit:ind2gray:emptyMap");

    // Build the grey colormap (length M, DOUBLE in [0, 1]).
    constexpr double Cr = 0.298936021293775;
    constexpr double Cg = 0.587043074451121;
    constexpr double Cb = 0.114020904255103;
    std::pmr::vector<double> graycm(M, mr);
    for (int k = 0; k < M; ++k) {
        // map is column-major M×3: map[r, c] = data[c*M + r].
        const double r = map.elemAsDouble(0 * M + k);
        const double g = map.elemAsDouble(1 * M + k);
        const double b = map.elemAsDouble(2 * M + k);
        graycm[k] = Cr * r + Cg * g + Cb * b;
    }

    const auto &d = idx.dims();
    const size_t H = d.rows();
    const size_t W = d.cols();
    const size_t N = idx.numel();
    const ValueType outT = idx.type();   // class-preserving

    Value out = d.is3D()
        ? Value::matrix3d(H, W, d.pages(), outT, mr)
        : Value::matrix(H, W, outT, mr);
    if (N == 0) return out;

    // Index → grey conversion. MATLAB index conventions:
    //   * float X is 1-based, clamped to [1, M].
    //   * integer X is 0-based, LUT padded to vs = 256 / 65536 with the
    //     last grey value beyond `M - 1`.
    auto float_lookup = [&](double v) -> double {
        long long k = static_cast<long long>(v);
        if (k < 1)  k = 1;
        if (k > M)  k = M;
        return graycm[static_cast<std::size_t>(k - 1)];
    };
    auto int_lookup_u8 = [&](unsigned v) -> uint8_t {
        std::size_t k = v;
        if (k >= static_cast<std::size_t>(M))
            k = static_cast<std::size_t>(M - 1);
        double scaled = graycm[k] * 255.0;
        if (scaled < 0.0)   scaled = 0.0;
        if (scaled > 255.0) scaled = 255.0;
        return static_cast<uint8_t>(std::lround(scaled));
    };
    auto int_lookup_u16 = [&](unsigned v) -> uint16_t {
        std::size_t k = v;
        if (k >= static_cast<std::size_t>(M))
            k = static_cast<std::size_t>(M - 1);
        double scaled = graycm[k] * 65535.0;
        if (scaled < 0.0)     scaled = 0.0;
        if (scaled > 65535.0) scaled = 65535.0;
        return static_cast<uint16_t>(std::lround(scaled));
    };

    switch (outT) {
        case ValueType::DOUBLE: {
            double *od = out.doubleDataMut();
            for (size_t i = 0; i < N; ++i)
                od[i] = float_lookup(idx.elemAsDouble(i));
            break;
        }
        case ValueType::SINGLE: {
            float *od = out.singleDataMut();
            for (size_t i = 0; i < N; ++i)
                od[i] = static_cast<float>(float_lookup(idx.elemAsDouble(i)));
            break;
        }
        case ValueType::UINT8: {
            uint8_t *od = out.uint8DataMut();
            const uint8_t *src = idx.uint8Data();
            for (size_t i = 0; i < N; ++i)
                od[i] = int_lookup_u8(src[i]);
            break;
        }
        case ValueType::UINT16: {
            uint16_t *od = out.uint16DataMut();
            const uint16_t *src = idx.uint16Data();
            for (size_t i = 0; i < N; ++i)
                od[i] = int_lookup_u16(src[i]);
            break;
        }
        case ValueType::LOGICAL: {
            // MATLAB historical compat: logical → uint8-like 0/1 lookup
            // (the MATLAB source uses intlut with vs = 256 here too).
            uint8_t *od = out.uint8DataMut();
            const uint8_t *src = idx.logicalData();
            for (size_t i = 0; i < N; ++i)
                od[i] = int_lookup_u8(src[i] ? 1u : 0u);
            break;
        }
        default:
            throw Error("ind2gray: X must be double, single, uint8, uint16, "
                        "or logical",
                        0, 0, "ind2gray", "", "numkit:ind2gray:cls");
    }
    return out;
}

Value ind2rgb(const Value &idx, const Value &map, std::pmr::memory_resource *mr)
{
    if (map.dims().cols() != 3)
        throw Error("ind2rgb: map must be N-by-3",
                    0, 0, "ind2rgb", "", "numkit:ind2rgb:map");
    const auto &d = idx.dims();
    const size_t H = d.rows();
    const size_t W = d.cols();
    const size_t N = idx.numel();
    const int M = static_cast<int>(map.dims().rows());

    Value out = Value::matrix3d(H, W, 3, ValueType::DOUBLE, mr);
    if (N == 0) return out;
    double *od = out.doubleDataMut();
    const size_t plane = H * W;

    const bool isFloatIdx = (idx.type() == ValueType::DOUBLE ||
                             idx.type() == ValueType::SINGLE);
    for (size_t i = 0; i < N; ++i) {
        long long k;
        const double v = idx.elemAsDouble(i);
        if (std::isnan(v) || !std::isfinite(v)) {
            // NaN / Inf clip to last colormap row.
            k = M - 1;
        } else {
            k = static_cast<long long>(v);
            if (isFloatIdx) k -= 1;
        }
        if (k < 0)  k = 0;
        if (k >= M) k = M - 1;
        // map is M×3, col-major: map[r, c] = data[c*M + r].
        od[0 * plane + i] = map.elemAsDouble(0 * M + k);
        od[1 * plane + i] = map.elemAsDouble(1 * M + k);
        od[2 * plane + i] = map.elemAsDouble(2 * M + k);
    }
    return out;
}

Value getrangefromclass(const Value &I, std::pmr::memory_resource *mr)
{
    Value r = Value::matrix(1, 2, ValueType::DOUBLE, mr);
    double *rd = r.doubleDataMut();
    switch (I.type()) {
        case ValueType::DOUBLE:
        case ValueType::SINGLE:
        case ValueType::LOGICAL: rd[0] = 0.0;       rd[1] = 1.0;     break;
        case ValueType::UINT8:   rd[0] = 0.0;       rd[1] = 255.0;   break;
        case ValueType::UINT16:  rd[0] = 0.0;       rd[1] = 65535.0; break;
        case ValueType::INT16:   rd[0] = -32768.0;  rd[1] = 32767.0; break;
        default:
            throw Error("getrangefromclass: unrecognized image class",
                        0, 0, "getrangefromclass", "",
                        "numkit:getrangefromclass:cls");
    }
    return r;
}

Value isbw(const Value &BW, const std::string &mode, std::pmr::memory_resource *mr)
{
    if (!spatial_pages_eq(BW, 1)) return bool_scalar(false, mr);
    if (BW.numel() == 0)          return bool_scalar(false, mr);
    if (mode == "logical")
        return bool_scalar(BW.type() == ValueType::LOGICAL, mr);
    if (mode != "non-logical")
        throw Error("isbw: MODE must be 'logical' or 'non-logical'",
                    0, 0, "isbw", "", "numkit:isbw:mode");
    if (BW.type() == ValueType::LOGICAL) return bool_scalar(true, mr);
    if (is_int_image_class(BW.type()) ||
        BW.type() == ValueType::DOUBLE || BW.type() == ValueType::SINGLE)
        return bool_scalar(all_zero_or_one(BW), mr);
    return bool_scalar(false, mr);
}

Value isgray(const Value &I, std::pmr::memory_resource *mr)
{
    if (!spatial_pages_eq(I, 1)) return bool_scalar(false, mr);
    if (I.numel() == 0)          return bool_scalar(false, mr);
    if (is_int_image_class(I.type())) return bool_scalar(true, mr);
    return bool_scalar(is_float_unit_image(I), mr);
}

Value isind(const Value &I, std::pmr::memory_resource *mr)
{
    if (!spatial_pages_eq(I, 1)) return bool_scalar(false, mr);
    if (I.numel() == 0)          return bool_scalar(false, mr);
    if (I.type() == ValueType::UINT8 || I.type() == ValueType::UINT16)
        return bool_scalar(true, mr);
    return bool_scalar(is_pos_int_float(I), mr);
}

Value isrgb(const Value &I, std::pmr::memory_resource *mr)
{
    if (!spatial_pages_eq(I, 3)) return bool_scalar(false, mr);
    if (I.numel() == 0)          return bool_scalar(false, mr);
    if (is_int_image_class(I.type())) return bool_scalar(true, mr);
    return bool_scalar(is_float_unit_image(I), mr);
}

Value iscolormap(const Value &cmap, std::pmr::memory_resource *mr)
{
    const auto &d = cmap.dims();
    if (d.is3D())                return bool_scalar(false, mr);
    if (d.cols() != 3)           return bool_scalar(false, mr);
    if (cmap.numel() == 0)       return bool_scalar(false, mr);
    const ValueType t = cmap.type();
    if (t != ValueType::DOUBLE && t != ValueType::SINGLE)
        return bool_scalar(false, mr);
    if (cmap.isComplex())        return bool_scalar(false, mr);
    return bool_scalar(true, mr);
}

Value intlut(const Value &A, const Value &LUT, std::pmr::memory_resource *mr)
{
    const ValueType atype = A.type();
    const ValueType ltype = LUT.type();

    size_t expectedLen;
    switch (atype) {
        case ValueType::UINT8:  expectedLen = 256;   break;
        case ValueType::UINT16:
        case ValueType::INT16:  expectedLen = 65536; break;
        default:
            throw Error("intlut: A must be uint8, uint16, or int16",
                        0, 0, "intlut", "", "numkit:intlut:atype");
    }
    if (ltype != ValueType::UINT8 && ltype != ValueType::UINT16 &&
        ltype != ValueType::INT16)
        throw Error("intlut: LUT must be uint8, uint16, or int16",
                    0, 0, "intlut", "", "numkit:intlut:luttype");
    if (LUT.numel() != expectedLen)
        throw Error("intlut: LUT length does not match input class range",
                    0, 0, "intlut", "", "numkit:intlut:lutsize");

    Value out = alloc_like(A, ltype, mr);
    const size_t N = A.numel();
    if (N == 0) return out;

    const int shift = (atype == ValueType::INT16) ? 32768 : 0;

    auto idx_at = [&](size_t i) -> size_t {
        return static_cast<size_t>(static_cast<int64_t>(A.elemAsDouble(i))
                                   + shift);
    };

    switch (ltype) {
        case ValueType::UINT8: {
            const uint8_t *lut = LUT.uint8Data();
            uint8_t *od = out.uint8DataMut();
            for (size_t i = 0; i < N; ++i) od[i] = lut[idx_at(i)];
            break;
        }
        case ValueType::UINT16: {
            const uint16_t *lut = LUT.uint16Data();
            uint16_t *od = out.uint16DataMut();
            for (size_t i = 0; i < N; ++i) od[i] = lut[idx_at(i)];
            break;
        }
        case ValueType::INT16: {
            const int16_t *lut = LUT.int16Data();
            int16_t *od = out.int16DataMut();
            for (size_t i = 0; i < N; ++i) od[i] = lut[idx_at(i)];
            break;
        }
        default: break;
    }
    return out;
}

// ════════════════════════════════════════════════════════════════════
// Engine adapters
// ════════════════════════════════════════════════════════════════════

namespace detail {

#define NK_IM_UNARY_REG(name)                                                   \
    void name##_reg(Span<const Value> args, size_t /*nargout*/,                \
                    Span<Value> outs, CallContext &ctx)                        \
    {                                                                            \
        if (args.empty())                                                        \
            throw Error(#name ": requires X", 0, 0, #name, "",                  \
                        "numkit:" #name ":nargin");                                  \
        outs[0] = name(args[0], ctx.engine->resource());                        \
    }

NK_IM_UNARY_REG(im2double)
NK_IM_UNARY_REG(im2single)
NK_IM_UNARY_REG(im2uint8)
NK_IM_UNARY_REG(im2uint16)
NK_IM_UNARY_REG(im2int16)
NK_IM_UNARY_REG(im2gray)
NK_IM_UNARY_REG(rgb2gray)

#undef NK_IM_UNARY_REG

void mat2gray_reg(Span<const Value> args, size_t /*nargout*/,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("mat2gray: requires X[, [lo hi]]", 0, 0, "mat2gray", "",
                    "numkit:mat2gray:nargin");
    double lo = std::numeric_limits<double>::quiet_NaN();
    double hi = std::numeric_limits<double>::quiet_NaN();
    if (args.size() >= 2 && !args[1].isEmpty()) {
        if (args[1].numel() != 2)
            throw Error("mat2gray: range must be a 2-element vector",
                        0, 0, "mat2gray", "", "numkit:mat2gray:size");
        lo = args[1].elemAsDouble(0);
        hi = args[1].elemAsDouble(1);
    }
    outs[0] = mat2gray(args[0], lo, hi, ctx.engine->resource());
}

void iptnum2ordinal_reg(Span<const Value> args, size_t /*nargout*/,
                        Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("iptnum2ordinal: requires (n)",
                    0, 0, "iptnum2ordinal", "", "numkit:iptnum2ordinal:nargin");
    outs[0] = iptnum2ordinal(args[0].toScalar(), ctx.engine->resource());
}

void imcast_reg(Span<const Value> args, size_t /*nargout*/,
                Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("imcast: requires (I, type)",
                    0, 0, "imcast", "", "numkit:imcast:nargin");
    if (!args[1].isChar() && !args[1].isString())
        throw Error("imcast: TYPE must be a string",
                    0, 0, "imcast", "", "numkit:imcast:type");
    outs[0] = imcast(args[0], args[1].toString(), ctx.engine->resource());
}

void gray2ind_reg(Span<const Value> args, size_t nargout,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("gray2ind: requires (I [, n])",
                    0, 0, "gray2ind", "", "numkit:gray2ind:nargin");
    int n = (args[0].type() == ValueType::LOGICAL) ? 2 : 64;
    if (args.size() >= 2 && !args[1].isEmpty())
        n = static_cast<int>(args[1].toScalar());
    auto [ind, map] = gray2ind(args[0], n, ctx.engine->resource());
    outs[0] = std::move(ind);
    if (nargout > 1) outs[1] = std::move(map);
}

void ind2gray_reg(Span<const Value> args, size_t /*nargout*/,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("ind2gray: requires (idx [, map])",
                    0, 0, "ind2gray", "", "numkit:ind2gray:nargin");
    Value mp;
    if (args.size() >= 2 && !args[1].isEmpty()) mp = args[1];
    outs[0] = ind2gray(args[0], mp, ctx.engine->resource());
}

void ind2rgb_reg(Span<const Value> args, size_t /*nargout*/,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("ind2rgb: requires (idx, map)",
                    0, 0, "ind2rgb", "", "numkit:ind2rgb:nargin");
    outs[0] = ind2rgb(args[0], args[1], ctx.engine->resource());
}

void getrangefromclass_reg(Span<const Value> args, size_t /*nargout*/,
                           Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("getrangefromclass: requires (I)",
                    0, 0, "getrangefromclass", "",
                    "numkit:getrangefromclass:nargin");
    outs[0] = getrangefromclass(args[0], ctx.engine->resource());
}

void isbw_reg(Span<const Value> args, size_t /*nargout*/,
              Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("isbw: requires (BW [, mode])",
                    0, 0, "isbw", "", "numkit:isbw:nargin");
    std::string mode = "logical";
    if (args.size() >= 2 && !args[1].isEmpty()) {
        if (!args[1].isChar() && !args[1].isString())
            throw Error("isbw: MODE must be a string",
                        0, 0, "isbw", "", "numkit:isbw:mode");
        mode = args[1].toString();
    }
    outs[0] = isbw(args[0], mode, ctx.engine->resource());
}

void isgray_reg(Span<const Value> args, size_t /*nargout*/,
                Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("isgray: requires (I)", 0, 0, "isgray", "",
                    "numkit:isgray:nargin");
    outs[0] = isgray(args[0], ctx.engine->resource());
}

void isind_reg(Span<const Value> args, size_t /*nargout*/,
               Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("isind: requires (I)", 0, 0, "isind", "",
                    "numkit:isind:nargin");
    outs[0] = isind(args[0], ctx.engine->resource());
}

void isrgb_reg(Span<const Value> args, size_t /*nargout*/,
               Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("isrgb: requires (I)", 0, 0, "isrgb", "",
                    "numkit:isrgb:nargin");
    outs[0] = isrgb(args[0], ctx.engine->resource());
}

void iscolormap_reg(Span<const Value> args, size_t /*nargout*/,
                    Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("iscolormap: requires (cmap)", 0, 0, "iscolormap", "",
                    "numkit:iscolormap:nargin");
    outs[0] = iscolormap(args[0], ctx.engine->resource());
}

void intlut_reg(Span<const Value> args, size_t /*nargout*/,
                Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("intlut: requires (A, LUT)", 0, 0, "intlut", "",
                    "numkit:intlut:nargin");
    outs[0] = intlut(args[0], args[1], ctx.engine->resource());
}

} // namespace detail
} // namespace numkit::image
