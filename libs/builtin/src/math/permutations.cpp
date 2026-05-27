// libs/builtin/src/math/permutations.cpp
//
// Sparse-style permutation utilities for dense matrices.
//   * colperm — sort columns by ascending nonzero count.
//
// MATLAB's colperm is documented for sparse matrices but works on
// dense matrices too (any element != 0 counts as nonzero).

#include <numkit/builtin/library.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include <algorithm>
#include <cstdint>
#include <vector>

namespace numkit::builtin {
namespace detail {

// j = colperm(S) — permutation vector that orders the columns of S
// by ascending count of nonzero entries. Stable: ties broken by
// original column index. Returns a row vector of doubles in MATLAB
// (1-indexed).
void colperm_reg(Span<const Value> args, size_t /*nargout*/,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("colperm: requires 1 argument",
                     0, 0, "colperm", "", "m:colperm:nargin");
    auto *mr = ctx.engine->resource();
    const Value &S = args[0];
    const auto &d = S.dims();
    const std::size_t M = d.rows();
    const std::size_t N = (d.ndim() >= 2) ? d.cols() : 1;
    if (S.numel() != M * N)
        throw Error("colperm: input must be a 2-D matrix",
                     0, 0, "colperm", "", "m:colperm:shape");

    // Count nonzeros per column.
    std::vector<std::size_t> nnz(N, 0);
    for (std::size_t c = 0; c < N; ++c) {
        std::size_t k = 0;
        for (std::size_t r = 0; r < M; ++r)
            if (S.elemAsDouble(r + c * M) != 0.0) ++k;
        nnz[c] = k;
    }

    // Stable sort column indices by ascending nnz.
    std::vector<std::size_t> perm(N);
    for (std::size_t i = 0; i < N; ++i) perm[i] = i;
    std::stable_sort(perm.begin(), perm.end(),
        [&](std::size_t a, std::size_t b) { return nnz[a] < nnz[b]; });

    // Output: row vector of 1-indexed permutation indices, DOUBLE.
    auto out = Value::matrix(1, N, ValueType::DOUBLE, mr);
    double *od = out.doubleDataMut();
    for (std::size_t i = 0; i < N; ++i)
        od[i] = double(perm[i] + 1);
    outs[0] = std::move(out);
}

} // namespace detail
} // namespace numkit::builtin
