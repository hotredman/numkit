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

inline Value alloc_like(std::pmr::memory_resource *mr, const Value &x, ValueType t) {
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
Value to_float_impl(std::pmr::memory_resource *mr, const Value &x, ValueType target) {
    const size_t n = x.numel();
    Value out;
    if (x.isScalar()) {
        out = Value::matrix(1, 1, target, mr);
    } else {
        out = alloc_like(mr, x, target);
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
Value to_int_impl(std::pmr::memory_resource *mr, const Value &x, ValueType target,
                  double scale, double bias_after) {
    const size_t n = x.numel();
    Value out;
    if (x.isScalar()) out = Value::matrix(1, 1, target, mr);
    else              out = alloc_like(mr, x, target);
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
                        "m:im2int:badtype");
    }
    return out;
}

} // anonymous

Value im2double(std::pmr::memory_resource *mr, const Value &x) {
    if (x.type() == ValueType::DOUBLE) return x;
    return to_float_impl<double>(mr, x, ValueType::DOUBLE);
}

Value im2single(std::pmr::memory_resource *mr, const Value &x) {
    if (x.type() == ValueType::SINGLE) return x;
    return to_float_impl<float>(mr, x, ValueType::SINGLE);
}

Value im2uint8(std::pmr::memory_resource *mr, const Value &x) {
    if (x.type() == ValueType::UINT8) return x;
    return to_int_impl<uint8_t>(mr, x, ValueType::UINT8, 255.0, 0.0);
}

Value im2uint16(std::pmr::memory_resource *mr, const Value &x) {
    if (x.type() == ValueType::UINT16) return x;
    return to_int_impl<uint16_t>(mr, x, ValueType::UINT16, 65535.0, 0.0);
}

Value im2int16(std::pmr::memory_resource *mr, const Value &x) {
    if (x.type() == ValueType::INT16) return x;
    // [0, 1] float → [-32768, 32767]: round(x*65535) - 32768.
    return to_int_impl<int16_t>(mr, x, ValueType::INT16, 65535.0, -32768.0);
}

Value mat2gray(std::pmr::memory_resource *mr, const Value &x, double lo, double hi)
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
    Value out = alloc_like(mr, x, ValueType::DOUBLE);
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

Value rgb2gray(std::pmr::memory_resource *mr, const Value &x)
{
    const auto &d = x.dims();
    if (!d.is3D() || d.pages() != 3)
        throw Error("rgb2gray: input must be H×W×3", 0, 0, "rgb2gray", "",
                    "m:rgb2gray:size");
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
                            "m:rgb2gray:badtype");
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

Value im2gray(std::pmr::memory_resource *mr, const Value &x) {
    const auto &d = x.dims();
    if (d.is3D() && d.pages() == 3) return rgb2gray(mr, x);
    return x;  // already grayscale (or unsupported shape — pass through)
}

Value intlut(std::pmr::memory_resource *mr, const Value &A, const Value &LUT)
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
                        0, 0, "intlut", "", "m:intlut:atype");
    }
    if (ltype != ValueType::UINT8 && ltype != ValueType::UINT16 &&
        ltype != ValueType::INT16)
        throw Error("intlut: LUT must be uint8, uint16, or int16",
                    0, 0, "intlut", "", "m:intlut:luttype");
    if (LUT.numel() != expectedLen)
        throw Error("intlut: LUT length does not match input class range",
                    0, 0, "intlut", "", "m:intlut:lutsize");

    Value out = alloc_like(mr, A, ltype);
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
                        "m:" #name ":nargin");                                  \
        outs[0] = name(ctx.engine->resource(), args[0]);                        \
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
                    "m:mat2gray:nargin");
    double lo = std::numeric_limits<double>::quiet_NaN();
    double hi = std::numeric_limits<double>::quiet_NaN();
    if (args.size() >= 2 && !args[1].isEmpty()) {
        if (args[1].numel() != 2)
            throw Error("mat2gray: range must be a 2-element vector",
                        0, 0, "mat2gray", "", "m:mat2gray:size");
        lo = args[1].elemAsDouble(0);
        hi = args[1].elemAsDouble(1);
    }
    outs[0] = mat2gray(ctx.engine->resource(), args[0], lo, hi);
}

void intlut_reg(Span<const Value> args, size_t /*nargout*/,
                Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("intlut: requires (A, LUT)", 0, 0, "intlut", "",
                    "m:intlut:nargin");
    outs[0] = intlut(ctx.engine->resource(), args[0], args[1]);
}

} // namespace detail
} // namespace numkit::image
