// libs/image/src/arithmetic/arithmetic.cpp
//
// Image arithmetic. Saturating element-wise operations for integer
// classes; pass-through for floating types. Output class = X's class.

#include <numkit/image/arithmetic/arithmetic.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

namespace numkit::image {

namespace {

// Integer saturation helper. Cast `v` (double) to T with clipping to
// T's representable range, rounding to nearest.
template <typename T>
inline T satCast(double v) {
    if (std::isnan(v)) return T{};
    constexpr double lo = static_cast<double>(std::numeric_limits<T>::lowest());
    constexpr double hi = static_cast<double>(std::numeric_limits<T>::max());
    if (v <= lo) return std::numeric_limits<T>::lowest();
    if (v >= hi) return std::numeric_limits<T>::max();
    return static_cast<T>(std::lround(v));
}
template <> inline double satCast<double>(double v) { return v; }
template <> inline float  satCast<float>(double v)  { return static_cast<float>(v); }

// Allocate an output Value matching X's shape and class.
inline Value makeOutLike(const Value &x, std::pmr::memory_resource *mr) {
    const auto &d = x.dims();
    if (x.isScalar()) return Value::scalar(0.0, mr);  // overwritten by caller
    if (d.is3D()) return Value::matrix3d(d.rows(), d.cols(), d.pages(), x.type(), mr);
    return Value::matrix(d.rows(), d.cols(), x.type(), mr);
}

// Dispatch a binary op on element pairs (xi, yi) producing xi.class.
template <typename Op>
Value binop(const Value &x, const Value &y, Op op, std::pmr::memory_resource *mr) {
    const size_t n = std::max(x.numel(), y.numel());
    if (n == 0) return makeOutLike(x, mr);
    Value out;
    if (x.isScalar() && !y.isScalar()) {
        // Output shape matches y, but class matches x.
        const auto &d = y.dims();
        if (d.is3D()) out = Value::matrix3d(d.rows(), d.cols(), d.pages(), x.type(), mr);
        else          out = Value::matrix(d.rows(), d.cols(), x.type(), mr);
    } else {
        out = makeOutLike(x, mr);
    }

    auto getX = [&](size_t i){ return x.elemAsDouble(x.isScalar() ? 0 : i); };
    auto getY = [&](size_t i){ return y.elemAsDouble(y.isScalar() ? 0 : i); };

    switch (x.type()) {
        case ValueType::DOUBLE: {
            double *od = out.doubleDataMut();
            for (size_t i = 0; i < n; ++i) od[i] = satCast<double>(op(getX(i), getY(i)));
            break;
        }
        case ValueType::SINGLE: {
            float *od = out.singleDataMut();
            for (size_t i = 0; i < n; ++i) od[i] = satCast<float>(op(getX(i), getY(i)));
            break;
        }
        case ValueType::UINT8: {
            uint8_t *od = out.uint8DataMut();
            for (size_t i = 0; i < n; ++i) od[i] = satCast<uint8_t>(op(getX(i), getY(i)));
            break;
        }
        case ValueType::UINT16: {
            uint16_t *od = out.uint16DataMut();
            for (size_t i = 0; i < n; ++i) od[i] = satCast<uint16_t>(op(getX(i), getY(i)));
            break;
        }
        case ValueType::INT16: {
            int16_t *od = out.int16DataMut();
            for (size_t i = 0; i < n; ++i) od[i] = satCast<int16_t>(op(getX(i), getY(i)));
            break;
        }
        case ValueType::LOGICAL: {
            uint8_t *od = out.logicalDataMut();
            for (size_t i = 0; i < n; ++i) od[i] = satCast<uint8_t>(op(getX(i), getY(i))) ? 1 : 0;
            break;
        }
        default:
            throw Error("imarith: unsupported image class", 0, 0, "imarith", "",
                        "m:imarith:badtype");
    }
    return out;
}

// Unary variant.
template <typename Op>
Value unop(const Value &x, Op op, std::pmr::memory_resource *mr) {
    const size_t n = x.numel();
    Value out = makeOutLike(x, mr);
    if (n == 0) return out;
    auto getX = [&](size_t i){ return x.elemAsDouble(i); };
    switch (x.type()) {
        case ValueType::DOUBLE: {
            double *od = out.doubleDataMut();
            for (size_t i = 0; i < n; ++i) od[i] = satCast<double>(op(getX(i)));
            break;
        }
        case ValueType::SINGLE: {
            float *od = out.singleDataMut();
            for (size_t i = 0; i < n; ++i) od[i] = satCast<float>(op(getX(i)));
            break;
        }
        case ValueType::UINT8: {
            uint8_t *od = out.uint8DataMut();
            for (size_t i = 0; i < n; ++i) od[i] = satCast<uint8_t>(op(getX(i)));
            break;
        }
        case ValueType::UINT16: {
            uint16_t *od = out.uint16DataMut();
            for (size_t i = 0; i < n; ++i) od[i] = satCast<uint16_t>(op(getX(i)));
            break;
        }
        case ValueType::INT16: {
            int16_t *od = out.int16DataMut();
            for (size_t i = 0; i < n; ++i) od[i] = satCast<int16_t>(op(getX(i)));
            break;
        }
        case ValueType::LOGICAL: {
            uint8_t *od = out.logicalDataMut();
            for (size_t i = 0; i < n; ++i) od[i] = satCast<uint8_t>(op(getX(i))) ? 1 : 0;
            break;
        }
        default:
            throw Error("imarith: unsupported image class", 0, 0, "imarith", "",
                        "m:imarith:badtype");
    }
    return out;
}

// Saturated max/min for X's class.
inline double classMax(ValueType t) {
    switch (t) {
        case ValueType::UINT8:  return 255.0;
        case ValueType::UINT16: return 65535.0;
        case ValueType::INT16:  return 32767.0;
        case ValueType::LOGICAL: return 1.0;
        case ValueType::SINGLE:
        case ValueType::DOUBLE: return 1.0;  // floating-point images are conventionally [0, 1]
        default: return 1.0;
    }
}

} // anonymous

Value imadd(const Value &x, const Value &y, std::pmr::memory_resource *mr) {
    return binop(x, y, [](double a, double b){ return a + b; }, mr);
}

Value imsubtract(const Value &x, const Value &y, std::pmr::memory_resource *mr) {
    return binop(x, y, [](double a, double b){ return a - b; }, mr);
}

Value immultiply(const Value &x, const Value &y, std::pmr::memory_resource *mr) {
    return binop(x, y, [](double a, double b){ return a * b; }, mr);
}

Value imdivide(const Value &x, const Value &y, std::pmr::memory_resource *mr) {
    return binop(x, y, [](double a, double b){
        if (b == 0.0) {
            if (a > 0.0) return std::numeric_limits<double>::infinity();
            if (a < 0.0) return -std::numeric_limits<double>::infinity();
            return std::numeric_limits<double>::quiet_NaN();
        }
        return a / b;
    }, mr);
}

Value imabsdiff(const Value &x, const Value &y, std::pmr::memory_resource *mr) {
    return binop(x, y, [](double a, double b){ return std::fabs(a - b); }, mr);
}

Value imcomplement(const Value &x, std::pmr::memory_resource *mr) {
    const double cmax = classMax(x.type());
    return unop(x, [=](double v){ return cmax - v; }, mr);
}

Value imlincomb(const std::vector<double> &coefs, const std::vector<Value> &images, ValueType output_class, std::pmr::memory_resource *mr)
{
    if (coefs.size() != images.size() && coefs.size() != images.size() + 1)
        throw Error("imlincomb: number of coefficients must equal number of images "
                    "(optional one extra for the additive constant)",
                    0, 0, "imlincomb", "", "m:imlincomb:nargin");
    if (images.empty()) return Value::scalar(0.0, mr);

    const Value &x0 = images[0];
    const auto &d = x0.dims();
    const size_t n = x0.numel();
    Value out;
    if (d.is3D()) out = Value::matrix3d(d.rows(), d.cols(), d.pages(), output_class, mr);
    else          out = Value::matrix(d.rows(), d.cols(), output_class, mr);

    // Accumulate in double, then cast at the end.
    std::vector<double> acc(n, 0.0);
    const double bias = (coefs.size() == images.size() + 1) ? coefs.back() : 0.0;
    for (size_t k = 0; k < images.size(); ++k) {
        const Value &xk = images[k];
        const double ck = coefs[k];
        if (xk.numel() != n)
            throw Error("imlincomb: all images must have the same size",
                        0, 0, "imlincomb", "", "m:imlincomb:size");
        for (size_t i = 0; i < n; ++i) acc[i] += ck * xk.elemAsDouble(i);
    }
    for (size_t i = 0; i < n; ++i) acc[i] += bias;

    // Cast acc into output_class with saturation.
    switch (output_class) {
        case ValueType::DOUBLE:
            std::copy(acc.begin(), acc.end(), out.doubleDataMut()); break;
        case ValueType::SINGLE: {
            float *od = out.singleDataMut();
            for (size_t i = 0; i < n; ++i) od[i] = satCast<float>(acc[i]);
            break;
        }
        case ValueType::UINT8: {
            uint8_t *od = out.uint8DataMut();
            for (size_t i = 0; i < n; ++i) od[i] = satCast<uint8_t>(acc[i]);
            break;
        }
        case ValueType::UINT16: {
            uint16_t *od = out.uint16DataMut();
            for (size_t i = 0; i < n; ++i) od[i] = satCast<uint16_t>(acc[i]);
            break;
        }
        case ValueType::INT16: {
            int16_t *od = out.int16DataMut();
            for (size_t i = 0; i < n; ++i) od[i] = satCast<int16_t>(acc[i]);
            break;
        }
        default:
            throw Error("imlincomb: unsupported output class",
                        0, 0, "imlincomb", "", "m:imlincomb:badtype");
    }
    return out;
}

Value imapplymatrix(const Value &M, const Value &x, ValueType output_class, std::pmr::memory_resource *mr)
{
    const auto &dx = x.dims();
    const size_t pages = dx.is3D() ? dx.pages() : 1;
    const size_t Mrows = M.dims().rows();
    const size_t Mcols = M.dims().cols();
    if (Mcols != pages)
        throw Error("imapplymatrix: matrix columns must match the third dimension of X",
                    0, 0, "imapplymatrix", "", "m:imapplymatrix:size");

    const size_t H = dx.rows();
    const size_t W = dx.cols();
    const size_t plane = H * W;
    Value out;
    if (Mrows == 1) out = Value::matrix(H, W, output_class, mr);
    else            out = Value::matrix3d(H, W, Mrows, output_class, mr);

    // Accumulate per output pixel in double.
    auto Md = [&](size_t r, size_t c){ return M.elemAsDouble(c * Mrows + r); };
    auto Xd = [&](size_t i, size_t p){ return x.elemAsDouble(p * plane + i); };

    auto store = [&](size_t outIdx, double v) {
        switch (output_class) {
            case ValueType::DOUBLE:  out.doubleDataMut()[outIdx]  = satCast<double>(v); break;
            case ValueType::SINGLE:  out.singleDataMut()[outIdx]  = satCast<float>(v); break;
            case ValueType::UINT8:   out.uint8DataMut()[outIdx]   = satCast<uint8_t>(v); break;
            case ValueType::UINT16:  out.uint16DataMut()[outIdx]  = satCast<uint16_t>(v); break;
            case ValueType::INT16:   out.int16DataMut()[outIdx]   = satCast<int16_t>(v); break;
            default:
                throw Error("imapplymatrix: unsupported output class",
                            0, 0, "imapplymatrix", "", "m:imapplymatrix:badtype");
        }
    };

    for (size_t r = 0; r < Mrows; ++r) {
        for (size_t i = 0; i < plane; ++i) {
            double acc = 0.0;
            for (size_t c = 0; c < pages; ++c) acc += Md(r, c) * Xd(i, c);
            store(r * plane + i, acc);
        }
    }
    return out;
}

// ════════════════════════════════════════════════════════════════════
// Engine adapters
// ════════════════════════════════════════════════════════════════════

namespace detail {

namespace {
ValueType classFromString(const Value &v) {
    if (!v.isChar() && !v.isString())
        throw Error("imlincomb: output_class must be a string",
                    0, 0, "imlincomb", "", "m:imlincomb:badclass");
    auto s = v.toString();
    if (s == "double") return ValueType::DOUBLE;
    if (s == "single") return ValueType::SINGLE;
    if (s == "uint8")  return ValueType::UINT8;
    if (s == "uint16") return ValueType::UINT16;
    if (s == "int16")  return ValueType::INT16;
    if (s == "logical")return ValueType::LOGICAL;
    throw Error("imlincomb: unrecognised output_class", 0, 0, "imlincomb", "",
                "m:imlincomb:badclass");
}
} // anonymous

#define NK_BIN_REG(name, fn)                                                      \
    void name##_reg(Span<const Value> args, size_t /*nargout*/,                  \
                    Span<Value> outs, CallContext &ctx)                          \
    {                                                                              \
        if (args.size() < 2)                                                       \
            throw Error(#name ": requires (X, Y)",                                \
                         0, 0, #name, "", "m:" #name ":nargin");                  \
        outs[0] = fn(args[0], args[1], ctx.engine->resource());                   \
    }

NK_BIN_REG(imadd,      imadd)
NK_BIN_REG(imsubtract, imsubtract)
NK_BIN_REG(immultiply, immultiply)
NK_BIN_REG(imdivide,   imdivide)
NK_BIN_REG(imabsdiff,  imabsdiff)

#undef NK_BIN_REG

void imcomplement_reg(Span<const Value> args, size_t /*nargout*/,
                      Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("imcomplement: requires X", 0, 0, "imcomplement", "",
                    "m:imcomplement:nargin");
    outs[0] = imcomplement(args[0], ctx.engine->resource());
}

void imlincomb_reg(Span<const Value> args, size_t /*nargout*/,
                   Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("imlincomb: requires (k1, A1, k2, A2, ..., [output_class])",
                    0, 0, "imlincomb", "", "m:imlincomb:nargin");

    // Trailing string argument (optional) is the output class.
    size_t end = args.size();
    ValueType out_class = ValueType::DOUBLE;
    const Value &last = args[args.size() - 1];
    if ((last.isChar() || last.isString())) {
        out_class = classFromString(last);
        end -= 1;
    } else {
        // Default class = first image's class.
        if (args.size() >= 2) out_class = args[1].type();
    }
    if ((end & 1) == 1 && end < 3) // need at least k1, A1
        throw Error("imlincomb: arguments must come in (coef, image) pairs",
                    0, 0, "imlincomb", "", "m:imlincomb:nargin");

    std::vector<double> coefs;
    std::vector<Value>  images;
    for (size_t i = 0; i + 1 < end; i += 2) {
        coefs.push_back(args[i].toScalar());
        images.push_back(args[i + 1]);
    }
    if ((end & 1) == 1) coefs.push_back(args[end - 1].toScalar()); // trailing additive

    outs[0] = imlincomb(coefs, images, out_class, ctx.engine->resource());
}

void imapplymatrix_reg(Span<const Value> args, size_t /*nargout*/,
                       Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("imapplymatrix: requires (M, X[, output_class])",
                    0, 0, "imapplymatrix", "", "m:imapplymatrix:nargin");
    ValueType out_class = (args.size() >= 3 && (args[2].isChar() || args[2].isString()))
        ? classFromString(args[2])
        : args[1].type();
    outs[0] = imapplymatrix(args[0], args[1], out_class, ctx.engine->resource());
}

} // namespace detail
} // namespace numkit::image
