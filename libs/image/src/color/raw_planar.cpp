// libs/image/src/color/raw_planar.cpp
//
// raw2planar / planar2raw — deinterleave / re-interleave a Bayer CFA
// mosaic into its four colour-filter sensor planes. Pure parity-based
// rearrangement; no interpolation. Class-preserving.
//
// raw2planar(cfa)   M×N  → (M/2)×(N/2)×4
//   plane 1 = cfa(1:2:M, 1:2:N)     (odd row, odd col)
//   plane 2 = cfa(1:2:M, 2:2:N)     (odd row, even col)
//   plane 3 = cfa(2:2:M, 1:2:N)     (even row, odd col)
//   plane 4 = cfa(2:2:M, 2:2:N)     (even row, even col)
//
// planar2raw is the exact inverse. Both round-trip with equality on
// any class accepted by Value (numeric and logical).

#include <numkit/image/color/color.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include <cstdint>
#include <cstring>
#include <string>

namespace numkit::image {

namespace {

// Copy one element from src[si] to dst[di] preserving class.
inline void copyElem(Value &dst, size_t di, const Value &src, size_t si)
{
    switch (src.type()) {
        case ValueType::DOUBLE:  dst.doubleDataMut()[di]  = src.doubleData()[si];  break;
        case ValueType::SINGLE:  dst.singleDataMut()[di]  = src.singleData()[si];  break;
        case ValueType::UINT8:   dst.uint8DataMut()[di]   = src.uint8Data()[si];   break;
        case ValueType::UINT16:  dst.uint16DataMut()[di]  = src.uint16Data()[si];  break;
        case ValueType::UINT32:  dst.uint32DataMut()[di]  = src.uint32Data()[si];  break;
        case ValueType::UINT64:  dst.uint64DataMut()[di]  = src.uint64Data()[si];  break;
        case ValueType::INT8:    dst.int8DataMut()[di]    = src.int8Data()[si];    break;
        case ValueType::INT16:   dst.int16DataMut()[di]   = src.int16Data()[si];   break;
        case ValueType::INT32:   dst.int32DataMut()[di]   = src.int32Data()[si];   break;
        case ValueType::INT64:   dst.int64DataMut()[di]   = src.int64Data()[si];   break;
        case ValueType::LOGICAL: dst.logicalDataMut()[di] = src.logicalData()[si]; break;
        default:
            throw Error("raw_planar: unsupported element type",
                        0, 0, "raw_planar", "", "m:raw_planar:type");
    }
}

} // anonymous

Value raw2planar(const Value &cfa, std::pmr::memory_resource *mr)
{
    const auto &d = cfa.dims();
    if (d.ndims() < 2)
        throw Error("raw2planar: cfa must be 2-D",
                    0, 0, "raw2planar", "", "m:raw2planar:rank");
    const size_t M = d.dim(0);
    const size_t N = d.dim(1);
    if (cfa.numel() != M * N)
        throw Error("raw2planar: cfa must be a 2-D matrix",
                    0, 0, "raw2planar", "", "m:raw2planar:shape");
    if ((M % 2) != 0 || (N % 2) != 0)
        throw Error("raw2planar: cfa dimensions must both be even",
                    0, 0, "raw2planar", "", "m:raw2planar:DimsMustBeEven");

    const size_t Mp = M / 2;
    const size_t Np = N / 2;
    const ValueType T = cfa.type();
    Value P = Value::matrixND(std::array<size_t,3>{Mp, Np, 4}.data(), 3, T, mr);

    // Column-major index: cfa(r,c) = r + c*M  (0-indexed).
    //                     P(r,c,k) = r + c*Mp + k*Mp*Np.
    for (size_t cp = 0; cp < Np; ++cp) {
        for (size_t rp = 0; rp < Mp; ++rp) {
            const size_t r2 = 2 * rp;
            const size_t c2 = 2 * cp;
            const size_t baseP = rp + cp * Mp;
            // plane 1: (odd row, odd col)  — 0-indexed (r2, c2)
            copyElem(P, baseP + 0 * Mp * Np, cfa, r2     + c2     * M);
            // plane 2: (odd row, even col) — (r2, c2+1)
            copyElem(P, baseP + 1 * Mp * Np, cfa, r2     + (c2+1) * M);
            // plane 3: (even row, odd col) — (r2+1, c2)
            copyElem(P, baseP + 2 * Mp * Np, cfa, (r2+1) + c2     * M);
            // plane 4: (even row, even col)— (r2+1, c2+1)
            copyElem(P, baseP + 3 * Mp * Np, cfa, (r2+1) + (c2+1) * M);
        }
    }
    return P;
}

Value planar2raw(const Value &I, std::pmr::memory_resource *mr)
{
    const auto &d = I.dims();
    if (d.ndims() < 3 || d.dim(2) != 4)
        throw Error("planar2raw: I must be (M)×(N)×4",
                    0, 0, "planar2raw", "", "m:planar2raw:shape");
    const size_t Mp = d.dim(0);
    const size_t Np = d.dim(1);
    if (I.numel() != Mp * Np * 4)
        throw Error("planar2raw: I must be (M)×(N)×4",
                    0, 0, "planar2raw", "", "m:planar2raw:shape");

    const size_t M = 2 * Mp;
    const size_t N = 2 * Np;
    const ValueType T = I.type();
    Value cfa = Value::matrix(M, N, T, mr);

    for (size_t cp = 0; cp < Np; ++cp) {
        for (size_t rp = 0; rp < Mp; ++rp) {
            const size_t r2 = 2 * rp;
            const size_t c2 = 2 * cp;
            const size_t baseP = rp + cp * Mp;
            copyElem(cfa, r2     + c2     * M, I, baseP + 0 * Mp * Np);
            copyElem(cfa, r2     + (c2+1) * M, I, baseP + 1 * Mp * Np);
            copyElem(cfa, (r2+1) + c2     * M, I, baseP + 2 * Mp * Np);
            copyElem(cfa, (r2+1) + (c2+1) * M, I, baseP + 3 * Mp * Np);
        }
    }
    return cfa;
}

namespace detail {

void raw2planar_reg(Span<const Value> args, size_t /*nargout*/,
                    Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 1)
        throw Error("raw2planar: requires (cfa)",
                    0, 0, "raw2planar", "", "m:raw2planar:nargin");
    outs[0] = raw2planar(args[0], ctx.engine->resource());
}

void planar2raw_reg(Span<const Value> args, size_t /*nargout*/,
                    Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 1)
        throw Error("planar2raw: requires (I)",
                    0, 0, "planar2raw", "", "m:planar2raw:nargin");
    outs[0] = planar2raw(args[0], ctx.engine->resource());
}

} // namespace detail

} // namespace numkit::image
