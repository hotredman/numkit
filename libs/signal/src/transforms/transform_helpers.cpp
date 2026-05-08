// libs/signal/src/transforms/transform_helpers.cpp
//
// nextpow2 / fftshift / ifftshift. Split from library.cpp.

#include <numkit/signal/transforms/transform_helpers.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include "../dsp_helpers.hpp"  // Complex typedef
#include "helpers.hpp"         // createLike

#include <cmath>
#include <complex>
#include <cstring>
#include <vector>

namespace numkit::signal {

namespace {

// Cyclic shift along a single dim (1=rows, 2=cols, 3=pages). Element-major
// flat layout is column-major; we walk dst by destination index and pull
// from src using a per-axis offset.
//
// `mode = +1` -> fftshift (shift = ceil(extent/2))
// `mode = -1` -> ifftshift (shift = floor(extent/2))
template <typename T>
void shiftAlongDim(const T *src, T *dst, size_t R, size_t C, size_t P,
                   int dim, int mode)
{
    auto pickShift = [mode](size_t extent) -> size_t {
        if (extent <= 1) return 0;
        return (mode > 0) ? (extent + 1) / 2 : extent / 2;  // ceil vs floor
    };
    const size_t sR = (dim == 1) ? pickShift(R) : 0;
    const size_t sC = (dim == 2) ? pickShift(C) : 0;
    const size_t sP = (dim == 3) ? pickShift(P) : 0;
    for (size_t p = 0; p < P; ++p) {
        const size_t ps = (sP ? (p + sP) % P : p);
        for (size_t c = 0; c < C; ++c) {
            const size_t cs = (sC ? (c + sC) % C : c);
            for (size_t r = 0; r < R; ++r) {
                const size_t rs = (sR ? (r + sR) % R : r);
                dst[r + R * (c + C * p)]   = src[rs + R * (cs + C * ps)];
            }
        }
    }
}

// Apply fftshift/ifftshift across all non-singleton dims (mode=+1/-1).
Value cyclicShiftAll(const Value &x, int mode, std::pmr::memory_resource *mr)
{
    const size_t R = x.dims().rows();
    const size_t C = x.dims().cols();
    const size_t P = x.dims().is3D() ? x.dims().pages() : 1;
    const bool cplx = x.isComplex();
    auto r = createLike(x, cplx ? ValueType::COMPLEX : ValueType::DOUBLE, mr);
    if (x.numel() == 0) return r;
    // Determine which dims to shift: every non-singleton dim. (For a row
    // vector R=1, only cols shift; for a column vector C=1, only rows.)
    const bool dR = R > 1, dC = C > 1, dP = P > 1;
    if (cplx) {
        const Complex *src = x.complexData();
        Complex *dst = r.complexDataMut();
        // Shift one dim at a time using two scratch buffers.
        std::pmr::vector<Complex> buf(R * C * P, mr);
        std::memcpy(buf.data(), src, sizeof(Complex) * R * C * P);
        Complex *cur = buf.data(), *nxt = dst;
        if (dR) { shiftAlongDim<Complex>(cur, nxt, R, C, P, 1, mode); std::swap(cur, nxt); }
        if (dC) { shiftAlongDim<Complex>(cur, nxt, R, C, P, 2, mode); std::swap(cur, nxt); }
        if (dP) { shiftAlongDim<Complex>(cur, nxt, R, C, P, 3, mode); std::swap(cur, nxt); }
        if (cur != dst) std::memcpy(dst, cur, sizeof(Complex) * R * C * P);
    } else {
        const double *src = x.doubleData();
        double *dst = r.doubleDataMut();
        std::pmr::vector<double> buf(R * C * P, mr);
        std::memcpy(buf.data(), src, sizeof(double) * R * C * P);
        double *cur = buf.data(), *nxt = dst;
        if (dR) { shiftAlongDim<double>(cur, nxt, R, C, P, 1, mode); std::swap(cur, nxt); }
        if (dC) { shiftAlongDim<double>(cur, nxt, R, C, P, 2, mode); std::swap(cur, nxt); }
        if (dP) { shiftAlongDim<double>(cur, nxt, R, C, P, 3, mode); std::swap(cur, nxt); }
        if (cur != dst) std::memcpy(dst, cur, sizeof(double) * R * C * P);
    }
    return r;
}

// Single-dim form (when user passes explicit dim argument).
Value cyclicShiftOneDim(const Value &x, int dim, int mode,
                        std::pmr::memory_resource *mr)
{
    const size_t R = x.dims().rows();
    const size_t C = x.dims().cols();
    const size_t P = x.dims().is3D() ? x.dims().pages() : 1;
    const bool cplx = x.isComplex();
    auto r = createLike(x, cplx ? ValueType::COMPLEX : ValueType::DOUBLE, mr);
    if (x.numel() == 0) return r;
    if (cplx)
        shiftAlongDim<Complex>(x.complexData(), r.complexDataMut(), R, C, P, dim, mode);
    else
        shiftAlongDim<double>(x.doubleData(), r.doubleDataMut(), R, C, P, dim, mode);
    return r;
}

} // anonymous namespace

Value nextpow2(std::pmr::memory_resource *mr, double n)
{
    if (n <= 0)
        return Value::scalar(0.0, mr);
    return Value::scalar(std::ceil(std::log2(n)), mr);
}

// Vectorized form: smallest integer p such that 2^p >= |x_i| for each
// element. MATLAB documents elementwise behavior; the absolute value
// rule mirrors MATLAB's handling of negatives. See BUGS.md #10.
Value nextpow2(std::pmr::memory_resource *mr, const Value &x)
{
    if (x.isEmpty()) return x;
    if (x.isScalar()) return nextpow2(mr, std::abs(x.toScalar()));
    auto out = createLike(x, ValueType::DOUBLE, mr);
    double *dst = out.doubleDataMut();
    const size_t n = x.numel();
    for (size_t i = 0; i < n; ++i) {
        const double v = std::abs(x.elemAsDouble(i));
        dst[i] = (v <= 0.0) ? 0.0 : std::ceil(std::log2(v));
    }
    return out;
}

Value fftshift(std::pmr::memory_resource *mr, const Value &x)
{
    return cyclicShiftAll(x, +1, mr);
}

Value ifftshift(std::pmr::memory_resource *mr, const Value &x)
{
    return cyclicShiftAll(x, -1, mr);
}

Value fftshift(std::pmr::memory_resource *mr, const Value &x, int dim)
{
    return cyclicShiftOneDim(x, dim, +1, mr);
}

Value ifftshift(std::pmr::memory_resource *mr, const Value &x, int dim)
{
    return cyclicShiftOneDim(x, dim, -1, mr);
}

namespace detail {

void nextpow2_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("nextpow2: requires 1 argument",
                     0, 0, "nextpow2", "", "m:nextpow2:nargin");
    outs[0] = nextpow2(ctx.engine->resource(), args[0]);
}

void fftshift_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("fftshift: requires 1 argument",
                     0, 0, "fftshift", "", "m:fftshift:nargin");
    if (args.size() >= 2) {
        const int dim = static_cast<int>(args[1].toScalar());
        if (dim < 1 || dim > 3)
            throw Error("fftshift: dim must be 1, 2, or 3",
                         0, 0, "fftshift", "", "m:fftshift:dim");
        outs[0] = fftshift(ctx.engine->resource(), args[0], dim);
    } else {
        outs[0] = fftshift(ctx.engine->resource(), args[0]);
    }
}

void ifftshift_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("ifftshift: requires 1 argument",
                     0, 0, "ifftshift", "", "m:ifftshift:nargin");
    if (args.size() >= 2) {
        const int dim = static_cast<int>(args[1].toScalar());
        if (dim < 1 || dim > 3)
            throw Error("ifftshift: dim must be 1, 2, or 3",
                         0, 0, "ifftshift", "", "m:ifftshift:dim");
        outs[0] = ifftshift(ctx.engine->resource(), args[0], dim);
    } else {
        outs[0] = ifftshift(ctx.engine->resource(), args[0]);
    }
}

} // namespace detail

} // namespace numkit::signal
