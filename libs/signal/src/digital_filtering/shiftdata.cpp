// libs/signal/src/digital_filtering/shiftdata.cpp
//
// shiftdata + unshiftdata (Phase 4.4 of audio sweep). Bit-equal MATLAB
// R2025b shiftdata.m / unshiftdata.m.
//
// shiftdata(x):           shifted = shiftdim(x); perm=[]; nshifts=k
// shiftdata(x, dim):      perm = [dim, 1..dim-1, dim+1..ndims]
//                          shifted = permute(x, perm); nshifts=[]
// unshiftdata(x, perm=[], nshifts=k): y = shiftdim(x, -k)
// unshiftdata(x, perm, nshifts=[]):    y = ipermute(x, perm)

#include <numkit/signal/digital_filtering/shiftdata.hpp>

#include <numkit/builtin/language/arrays/nd_manip.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/scratch.hpp>
#include <numkit/core/types.hpp>

#include <vector>

namespace numkit::signal {

namespace {

// Compute MATLAB ndims for a Value. MATLAB: ndims(x) = max(2, ndims_actual).
size_t mtlNdims(const Value &x)
{
    if (x.dims().is3D()) return 3;
    return 2;
}

} // anon

std::tuple<Value, Value, Value>
shiftdata(const Value &x, int dim, std::pmr::memory_resource *mr)
{
    if (dim == 0) {
        // Auto path: shiftdim(x) drops leading singletons.
        auto res = builtin::shiftdimAuto(x, mr);
        // perm = []  (return as 1×0 double matrix)
        Value emptyPerm = Value::matrix(0, 0, ValueType::DOUBLE, mr);
        Value nsh = Value::scalar(static_cast<double>(res.dropped), mr);
        return {std::move(res.v), emptyPerm, nsh};
    }
    if (dim < 1)
        throw Error("shiftdata: DIM must be >= 1",
                    0, 0, "shiftdata", "", "numkit:shiftdata:BadDim");

    // perm = [dim, 1..dim-1, dim+1..N]
    const size_t N = std::max<size_t>(static_cast<size_t>(dim), mtlNdims(x));
    std::vector<int> perm(N);
    perm[0] = dim;
    size_t k = 1;
    for (int i = 1; i < dim; ++i) perm[k++] = i;
    for (size_t i = dim + 1; i <= N; ++i) perm[k++] = static_cast<int>(i);

    Value shifted = builtin::permute(x, Span<const int>(perm.data(), N), mr);

    // Build perm Value (1 × N row).
    Value permV = Value::matrix(1, N, ValueType::DOUBLE, mr);
    {
        double *pd = permV.doubleDataMut();
        for (size_t i = 0; i < N; ++i) pd[i] = static_cast<double>(perm[i]);
    }
    Value emptyNsh = Value::matrix(0, 0, ValueType::DOUBLE, mr);
    return {std::move(shifted), std::move(permV), std::move(emptyNsh)};
}

Value unshiftdata(const Value &x, const Value &perm, const Value &nshifts, std::pmr::memory_resource *mr)
{
    if (perm.isEmpty()) {
        const int n = nshifts.isEmpty() ? 0 : static_cast<int>(nshifts.toScalar());
        return builtin::shiftdim(x, -n, mr);
    }
    // ipermute(x, perm)
    const size_t N = perm.numel();
    std::vector<int> permVec(N);
    for (size_t i = 0; i < N; ++i)
        permVec[i] = static_cast<int>(perm.elemAsDouble(i));
    return builtin::ipermute(x, Span<const int>(permVec.data(), N), mr);
}

namespace detail {

void shiftdata_reg(Span<const Value> args, size_t nargout,
                    Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("shiftdata: requires (x [, dim])",
                    0, 0, "shiftdata", "", "numkit:shiftdata:nargin");
    int dim = 0;
    if (args.size() >= 2 && !args[1].isEmpty())
        dim = static_cast<int>(args[1].toScalar());
    auto [shifted, permV, nshV] = shiftdata(args[0], dim, ctx.engine->resource());
    outs[0] = shifted;
    if (nargout >= 2 && outs.size() >= 2) outs[1] = permV;
    if (nargout >= 3 && outs.size() >= 3) outs[2] = nshV;
}

void unshiftdata_reg(Span<const Value> args, size_t /*nargout*/,
                      Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("unshiftdata: requires (x, perm, nshifts)",
                    0, 0, "unshiftdata", "", "numkit:unshiftdata:nargin");
    outs[0] = unshiftdata(args[0], args[1], args[2], ctx.engine->resource());
}

} // namespace detail

} // namespace numkit::signal
