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

// ── factorial ────────────────────────────────────────────────────────
namespace {

double factorialDouble(double v, const char *fn)
{
    if (!std::isfinite(v) || v < 0 || v != std::floor(v))
        throw MError(std::string(fn)
                     + ": entries must be non-negative integers",
                     0, 0, fn, "", std::string("m:") + fn + ":badArg");
    if (v > 170.0)
        return std::numeric_limits<double>::infinity();
    double r = 1.0;
    const int n = static_cast<int>(v);
    for (int i = 2; i <= n; ++i) r *= static_cast<double>(i);
    return r;
}

} // namespace

MValue factorial(Allocator &alloc, const MValue &n)
{
    if (n.type() == MType::COMPLEX)
        throw MError("factorial: complex inputs are not supported",
                     0, 0, "factorial", "", "m:factorial:complex");
    auto out = createLike(n, MType::DOUBLE, &alloc);
    double *dst = out.doubleDataMut();
    const size_t N = n.numel();
    for (size_t i = 0; i < N; ++i)
        dst[i] = factorialDouble(n.elemAsDouble(i), "factorial");
    return out;
}

// ── nchoosek (scalar form) ───────────────────────────────────────────
MValue nchoosek(Allocator &alloc, double n, double k)
{
    if (!std::isfinite(n) || !std::isfinite(k))
        throw MError("nchoosek: arguments must be finite",
                     0, 0, "nchoosek", "", "m:nchoosek:badArg");
    if (n < 0 || k < 0 || n != std::floor(n) || k != std::floor(k))
        throw MError("nchoosek: arguments must be non-negative integers",
                     0, 0, "nchoosek", "", "m:nchoosek:badArg");
    if (k > n)
        throw MError("nchoosek: k must satisfy 0 ≤ k ≤ n",
                     0, 0, "nchoosek", "", "m:nchoosek:kTooLarge");

    // Exploit symmetry: C(n, k) = C(n, n-k); pick the smaller k.
    double kk = (k > n - k) ? n - k : k;
    if (kk == 0.0)
        return MValue::scalar(1.0, &alloc);

    double r = 1.0;
    const int kInt = static_cast<int>(kk);
    for (int i = 0; i < kInt; ++i) {
        r = r * (n - static_cast<double>(i)) / static_cast<double>(i + 1);
    }
    // For moderate n the result is an exact integer up to round-off.
    return MValue::scalar(std::round(r), &alloc);
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

void factorial_reg(Span<const MValue> args, size_t /*nargout*/, Span<MValue> outs, CallContext &ctx)
{
    if (args.empty())
        throw MError("factorial: requires 1 argument",
                     0, 0, "factorial", "", "m:factorial:nargin");
    outs[0] = factorial(ctx.engine->allocator(), args[0]);
}

void nchoosek_reg(Span<const MValue> args, size_t /*nargout*/, Span<MValue> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw MError("nchoosek: requires 2 arguments (n, k)",
                     0, 0, "nchoosek", "", "m:nchoosek:nargin");
    if (!args[0].isScalar() || !args[1].isScalar())
        throw MError("nchoosek: vector input form is not yet supported "
                     "(nchoosek(v, k) for k-combinations of v)",
                     0, 0, "nchoosek", "", "m:nchoosek:vectorForm");
    outs[0] = nchoosek(ctx.engine->allocator(),
                       args[0].toScalar(), args[1].toScalar());
}

} // namespace detail

} // namespace numkit::m::builtin
