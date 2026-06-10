// toolboxes/signal/src/language/arrays/manip_reg.cpp
//
// CallContext register half of language/arrays/manip.cpp (Phase 2b compute/register split).
// Engine-coupled glue: marshals CallContext args/outs into the engine-free
// compute API declared in the headers below. See project_layering_refactor.
#include <numkit/core/engine.hpp>
#include <numkit/builtin/language/arrays/manip.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/shape_ops.hpp>
#include <numkit/value/value.hpp>
#include "helpers.hpp"
#include "arrays/manip_detail.hpp"
#include <numkit/value/error.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/span.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <complex>
#include <cstddef>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace numkit::builtin {
using namespace numkit::lang;  // C4c localized (umbrella removed)

namespace detail {

void repmat_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
                CallContext &ctx)
{
    if (args.empty())
        throw Error("repmat: requires at least 2 arguments",
                     0, 0, "repmat", "", "numkit:repmat:nargin");
    auto *mr = ctx.engine->resource();

    // Forms:
    //   repmat(A, n)            → m=n=arg
    //   repmat(A, [m n])        → vector
    //   repmat(A, [m n p ...])  → vector (any length ≥ 1)
    //   repmat(A, m, n)         → two scalars
    //   repmat(A, m, n, p, ...) → ≥ 2 scalars
    ScratchArena scratch(mr);
    auto tiles = ScratchVec<size_t>(&scratch);
    if (args.size() == 2) {
        const Value &v = args[1];
        const size_t k = v.numel();
        if (k == 0) {
            throw Error("repmat: tile vector must not be empty",
                         0, 0, "repmat", "", "numkit:repmat:badTileVec");
        }
        if (k == 1) {
            const size_t s = static_cast<size_t>(v.toScalar());
            tiles.assign({s, s});
        } else {
            tiles.reserve(k);
            for (size_t i = 0; i < k; ++i)
                tiles.push_back(static_cast<size_t>(v.doubleData()[i]));
        }
    } else {
        tiles.reserve(args.size() - 1);
        for (size_t i = 1; i < args.size(); ++i)
            tiles.push_back(static_cast<size_t>(args[i].toScalar()));
    }

    // Fast path: rank ≤ 3 + tile vector ≤ 3 + DOUBLE → existing 2D/3D
    // kernel. Anything else (higher rank, longer tile vector, or non-
    // DOUBLE type) goes through repmatND.
    const auto &inDims = args[0].dims();
    const int outNdim = std::max(inDims.ndim(), static_cast<int>(tiles.size()));
    if (outNdim <= 3 && tiles.size() <= 3 && args[0].type() == ValueType::DOUBLE) {
        const size_t m = tiles[0];
        const size_t n = tiles.size() >= 2 ? tiles[1] : 1;
        const size_t p = tiles.size() >= 3 ? tiles[2] : 1;
        outs[0] = repmat(args[0], m, n, p, mr);
    } else {
        outs[0] = repmatND(args[0], Span<const size_t>(tiles.data(), tiles.size()), mr);
    }
}

#define NK_FLIP_REG(name)                                                      \
    void name##_reg(Span<const Value> args, size_t /*nargout*/,               \
                    Span<Value> outs, CallContext &ctx)                       \
    {                                                                          \
        if (args.empty())                                                      \
            throw Error(#name ": requires 1 argument",                        \
                         0, 0, #name, "", "numkit:" #name ":nargin");               \
        outs[0] = name(args[0], ctx.engine->resource());                      \
    }

NK_FLIP_REG(fliplr)
NK_FLIP_REG(flipud)

#undef NK_FLIP_REG

void rot90_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
               CallContext &ctx)
{
    if (args.empty())
        throw Error("rot90: requires at least 1 argument",
                     0, 0, "rot90", "", "numkit:rot90:nargin");
    int k = (args.size() >= 2 && !args[1].isEmpty())
                ? static_cast<int>(args[1].toScalar())
                : 1;
    outs[0] = rot90(args[0], k, ctx.engine->resource());
}

void circshift_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
                   CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("circshift: requires (X, k) or (X, shiftVec)",
                     0, 0, "circshift", "", "numkit:circshift:nargin");
    const Value &k = args[1];
    auto *mr = ctx.engine->resource();
    const size_t nk = k.numel();
    if (nk == 0)
        throw Error("circshift: shift vector must not be empty",
                     0, 0, "circshift", "", "numkit:circshift:badShift");

    if (nk == 1) {
        const int64_t kk = static_cast<int64_t>(k.toScalar());
        // circshift(X, K, dim): shift by K ONLY along dimension `dim`. The
        // previous code ignored args[2] and always shifted dim 1.
        if (args.size() >= 3 && !args[2].isEmpty()) {
            const int dim = static_cast<int>(args[2].toScalar());
            if (dim < 1)
                throw Error("circshift: dim must be a positive integer",
                             0, 0, "circshift", "", "numkit:circshift:badDim");
            ScratchArena scratch(mr);
            auto shifts = ScratchVec<int64_t>(static_cast<size_t>(dim), &scratch);
            for (int i = 0; i < dim; ++i) shifts[i] = 0;
            shifts[dim - 1] = kk;
            outs[0] = circshiftND(args[0],
                                  Span<const int64_t>(shifts.data(), dim), mr);
            return;
        }
        outs[0] = circshift(args[0], kk, mr);
        return;
    }
    if (nk == 2 && args[0].dims().ndim() <= 3) {
        outs[0] = circshift(args[0], static_cast<int64_t>(k.doubleData()[0]), static_cast<int64_t>(k.doubleData()[1]), mr);
        return;
    }
    // ND path: shift vector ≥ 3 entries OR input rank ≥ 4.
    ScratchArena scratch(mr);
    auto shifts = ScratchVec<int64_t>(nk, &scratch);
    for (size_t i = 0; i < nk; ++i)
        shifts[i] = static_cast<int64_t>(k.doubleData()[i]);
    outs[0] = circshiftND(args[0], Span<const int64_t>(shifts.data(), nk), mr);
}

#define NK_TRI_REG(name)                                                       \
    void name##_reg(Span<const Value> args, size_t /*nargout*/,               \
                    Span<Value> outs, CallContext &ctx)                       \
    {                                                                          \
        if (args.empty())                                                      \
            throw Error(#name ": requires at least 1 argument",               \
                         0, 0, #name, "", "numkit:" #name ":nargin");               \
        int k = (args.size() >= 2 && !args[1].isEmpty())                       \
                    ? static_cast<int>(args[1].toScalar())                     \
                    : 0;                                                        \
        outs[0] = name(args[0], k, ctx.engine->resource());                   \
    }

NK_TRI_REG(tril)
NK_TRI_REG(triu)

#undef NK_TRI_REG

void flip_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
              CallContext &ctx)
{
    if (args.empty())
        throw Error("flip: requires at least 1 argument",
                     0, 0, "flip", "", "numkit:flip:nargin");
    int dim = (args.size() >= 2 && !args[1].isEmpty())
                  ? static_cast<int>(args[1].toScalar())
                  : 0;
    outs[0] = flip(args[0], dim, ctx.engine->resource());
}

void repelem_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
                 CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("repelem: requires at least 2 arguments",
                     0, 0, "repelem", "", "numkit:repelem:nargin");
    auto *mr = ctx.engine->resource();
    if (args.size() == 2) {
        // counts may be a scalar or a per-element vector — the Value
        // overload dispatches internally.
        outs[0] = repelem(args[0], args[1], mr);
        return;
    }
    // r / c may each be a scalar or a per-row / per-column vector.
    outs[0] = repelem(args[0], args[1], args[2], mr);
}

// sub2ind(siz, i1, i2, ...) → linear index. Column-major, 1-based.
void sub2ind_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
                 CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("sub2ind: requires siz and at least 1 subscript",
                     0, 0, "sub2ind", "", "numkit:sub2ind:nargin");
    outs[0] = sub2ind(args[0], args.subspan(1), ctx.engine->resource());
}

// paddata / trimdata / resize adapters.
#define NK_RESIZE_REG(FN)                                                       \
    void FN##_reg(Span<const Value> args, size_t /*nargout*/,                  \
                  Span<Value> outs, CallContext &ctx)                           \
    {                                                                            \
        if (args.size() < 2)                                                     \
            throw Error(#FN " requires (v, n)",                                  \
                         0, 0, #FN, "", "numkit:" #FN ":nargin");                     \
        const size_t n = static_cast<size_t>(args[1].toScalar());                \
        outs[0] = FN(args[0], n, ctx.engine->resource());                       \
    }

NK_RESIZE_REG(paddata)
NK_RESIZE_REG(trimdata)
NK_RESIZE_REG(resize)

#undef NK_RESIZE_REG

// ind2sub(siz, ind) → multiple outputs (one per dim of siz). When the
// caller requests fewer outputs than siz has dims, the last output
// absorbs trailing dims (column-major linear index of the remainder).
void ind2sub_reg(Span<const Value> args, size_t nargout, Span<Value> outs,
                 CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("ind2sub: requires siz and ind",
                     0, 0, "ind2sub", "", "numkit:ind2sub:nargin");
    std::vector<Value> rs = ind2sub(args[0], args[1],
                                    std::max<size_t>(nargout, 1),
                                    ctx.engine->resource());
    for (size_t i = 0; i < rs.size() && i < outs.size(); ++i)
        outs[i] = std::move(rs[i]);
}

} // namespace detail

} // namespace numkit::builtin
