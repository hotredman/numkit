// libs/linalg/src/page_ops.cpp
//
// Page-wise linalg ops on 3-D arrays.
// Migrated 2026-05-25 from libs/builtin/src/language/arrays/matrix.cpp.

#include <numkit/linalg/page_ops.hpp>

#include <numkit/builtin/internal/la_solve.hpp>   // detail::la_solve

#include <numkit/core/engine.hpp>
#include <numkit/core/scratch.hpp>
#include <numkit/core/span.hpp>
#include <numkit/core/types.hpp>

#include <algorithm>

namespace numkit::linalg {

namespace {

void fillIdentity(double *buf, std::size_t n)
{
    std::fill(buf, buf + n * n, 0.0);
    for (std::size_t i = 0; i < n; ++i)
        buf[i + i * n] = 1.0;
}

} // anonymous namespace

Value pageinv(const Value &A, std::pmr::memory_resource *mr)
{
    const auto &dims = A.dims();
    const int nd = dims.ndim();
    if (nd < 2 || nd > 3)
        throw Error("pageinv: input must be 2D or 3D",
                    0, 0, "pageinv", "", "m:pageinv:badDim");
    const std::size_t m = static_cast<std::size_t>(dims.dim(0));
    const std::size_t n = static_cast<std::size_t>(dims.dim(1));
    if (m != n)
        throw Error("pageinv: each page must be square",
                    0, 0, "pageinv", "", "m:pageinv:notSquare");
    const std::size_t pages = (nd == 2) ? 1 : static_cast<std::size_t>(dims.dim(2));
    const std::size_t pageStride = m * n;

    auto out = (nd == 2)
        ? Value::matrix(m, n, ValueType::DOUBLE, mr)
        : Value::matrix3d(m, n, pages, ValueType::DOUBLE, mr);

    ScratchArena scratch(mr);
    ScratchVec<double> A_buf(pageStride, &scratch);
    ScratchVec<double> I_buf(n * n, &scratch);
    const double *src = A.doubleData();
    double *dst = out.doubleDataMut();

    for (std::size_t p = 0; p < pages; ++p) {
        std::copy(src + p * pageStride,
                  src + (p + 1) * pageStride,
                  A_buf.begin());
        fillIdentity(I_buf.data(), n);
        if (!numkit::builtin::detail::la_solve(A_buf.data(), m, n,
                                                I_buf.data(), n,
                                                dst + p * pageStride, &scratch))
            throw Error("pageinv: page is singular",
                        0, 0, "pageinv", "", "m:pageinv:singular");
    }
    return out;
}

// ════════════════════════════════════════════════════════════════════════
// Engine adapter
// ════════════════════════════════════════════════════════════════════════

namespace detail {

void pageinv_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() != 1)
        throw Error("pageinv: requires exactly 1 argument",
                    0, 0, "pageinv", "", "m:pageinv:nargin");
    outs[0] = pageinv(args[0], ctx.engine->resource());
}

} // namespace detail

} // namespace numkit::linalg
