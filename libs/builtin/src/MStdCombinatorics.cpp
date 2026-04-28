// libs/builtin/src/MStdCombinatorics.cpp

#include <numkit/m/builtin/MStdCombinatorics.hpp>

#include <numkit/m/core/MEngine.hpp>
#include <numkit/m/core/MTypes.hpp>

#include "MStdHelpers.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <vector>

namespace numkit::m::builtin {

namespace {

// 11! = 39 916 800 rows of doubles ≈ 3.5 GB if we let n=12 through.
// Capping at n=11 is exactly MATLAB's documented hard limit on perms.
constexpr int kPermMaxN = 11;

std::uint64_t factorial(int n)
{
    std::uint64_t f = 1;
    for (int i = 2; i <= n; ++i) f *= static_cast<std::uint64_t>(i);
    return f;
}

} // namespace

MValue perms(Allocator &alloc, const MValue &v)
{
    if (v.type() == MType::COMPLEX)
        throw MError("perms: complex inputs are not supported",
                     0, 0, "perms", "", "m:perms:complex");
    if (v.isEmpty()) {
        // perms([]) returns 1×0 — the single permutation of zero elements.
        return MValue::matrix(1, 0, MType::DOUBLE, &alloc);
    }
    if (!v.dims().isVector())
        throw MError("perms: argument must be a vector",
                     0, 0, "perms", "", "m:perms:notVector");

    const size_t n = v.numel();
    if (n > static_cast<size_t>(kPermMaxN))
        throw MError("perms: numel(v) > 11 is not supported (n! is too large)",
                     0, 0, "perms", "", "m:perms:tooLarge");

    // Read v as DOUBLE (promote integer/logical/SINGLE).
    std::vector<double> vals(n);
    for (size_t i = 0; i < n; ++i)
        vals[i] = v.elemAsDouble(i);

    // MATLAB orders rows in reverse-lexicographic order. Walk via
    // prev_permutation starting from the descending-sorted state.
    std::vector<double> cur(vals);
    std::sort(cur.begin(), cur.end(), std::greater<double>());

    const size_t totalRows = static_cast<size_t>(factorial(static_cast<int>(n)));
    auto out = MValue::matrix(totalRows, n, MType::DOUBLE, &alloc);
    double *dst = out.doubleDataMut();

    size_t row = 0;
    do {
        // Column-major store: dst[col*totalRows + row] = cur[col].
        for (size_t c = 0; c < n; ++c)
            dst[c * totalRows + row] = cur[c];
        ++row;
    } while (std::prev_permutation(cur.begin(), cur.end()));

    return out;
}

// ── Engine adapter ───────────────────────────────────────────────────
namespace detail {

void perms_reg(Span<const MValue> args, size_t /*nargout*/, Span<MValue> outs, CallContext &ctx)
{
    if (args.empty())
        throw MError("perms: requires 1 argument",
                     0, 0, "perms", "", "m:perms:nargin");
    outs[0] = perms(ctx.engine->allocator(), args[0]);
}

} // namespace detail

} // namespace numkit::m::builtin
