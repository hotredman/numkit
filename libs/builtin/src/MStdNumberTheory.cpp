// libs/builtin/src/MStdNumberTheory.cpp

#include <numkit/m/builtin/MStdNumberTheory.hpp>

#include <numkit/m/core/MEngine.hpp>
#include <numkit/m/core/MTypes.hpp>

#include "MStdHelpers.hpp"

#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

namespace numkit::m::builtin {

namespace {

bool isExactNonnegInt(double v, std::uint64_t &outU)
{
    if (!std::isfinite(v) || v < 0) return false;
    if (v != std::floor(v))         return false;
    if (v > static_cast<double>(std::numeric_limits<std::uint64_t>::max()))
        return false;
    outU = static_cast<std::uint64_t>(v);
    return true;
}

bool isPrimeU64(std::uint64_t n)
{
    if (n < 2)                return false;
    if (n == 2 || n == 3)     return true;
    if (n % 2 == 0)           return false;
    if (n % 3 == 0)           return false;
    // 6k ± 1 trial division.
    for (std::uint64_t i = 5; i * i <= n; i += 6) {
        if (n % i == 0)        return false;
        if (n % (i + 2) == 0)  return false;
    }
    return true;
}

bool isPrimeDouble(double v)
{
    std::uint64_t u;
    if (!isExactNonnegInt(v, u)) return false;
    return isPrimeU64(u);
}

} // namespace

MValue primes(Allocator &alloc, double n)
{
    if (!std::isfinite(n) || n < 2)
        return MValue::matrix(1, 0, MType::DOUBLE, &alloc);
    const std::uint64_t N = static_cast<std::uint64_t>(std::floor(n));
    // Sieve of Eratosthenes for [2..N].
    std::vector<bool> composite(N + 1, false);
    for (std::uint64_t i = 2; i * i <= N; ++i)
        if (!composite[i])
            for (std::uint64_t j = i * i; j <= N; j += i)
                composite[j] = true;

    std::vector<double> primesVec;
    primesVec.reserve(static_cast<size_t>(N / std::log(static_cast<double>(N) + 1.0)) + 1);
    for (std::uint64_t i = 2; i <= N; ++i)
        if (!composite[i])
            primesVec.push_back(static_cast<double>(i));

    auto out = MValue::matrix(1, primesVec.size(), MType::DOUBLE, &alloc);
    if (!primesVec.empty())
        std::memcpy(out.doubleDataMut(), primesVec.data(),
                    primesVec.size() * sizeof(double));
    return out;
}

MValue isprime(Allocator &alloc, const MValue &x)
{
    if (x.type() == MType::COMPLEX)
        throw MError("isprime: complex inputs are not supported",
                     0, 0, "isprime", "", "m:isprime:complex");
    auto out = createLike(x, MType::LOGICAL, &alloc);
    uint8_t *dst = out.logicalDataMut();
    const size_t N = x.numel();
    for (size_t i = 0; i < N; ++i) {
        const double v = x.elemAsDouble(i);
        dst[i] = isPrimeDouble(v) ? 1 : 0;
    }
    return out;
}

MValue factor(Allocator &alloc, double n)
{
    std::uint64_t u;
    if (!isExactNonnegInt(n, u))
        throw MError("factor: argument must be a non-negative integer scalar",
                     0, 0, "factor", "", "m:factor:badArg");
    // MATLAB special cases: factor(0) → [0], factor(1) → [1].
    if (u == 0 || u == 1) {
        auto r = MValue::matrix(1, 1, MType::DOUBLE, &alloc);
        r.doubleDataMut()[0] = static_cast<double>(u);
        return r;
    }
    std::vector<double> factors;
    std::uint64_t m = u;
    // Strip 2s.
    while (m % 2 == 0) { factors.push_back(2.0); m /= 2; }
    // Then odd trial division.
    for (std::uint64_t p = 3; p * p <= m; p += 2) {
        while (m % p == 0) {
            factors.push_back(static_cast<double>(p));
            m /= p;
        }
    }
    if (m > 1)
        factors.push_back(static_cast<double>(m));

    auto out = MValue::matrix(1, factors.size(), MType::DOUBLE, &alloc);
    if (!factors.empty())
        std::memcpy(out.doubleDataMut(), factors.data(),
                    factors.size() * sizeof(double));
    return out;
}

// ── Engine adapters ──────────────────────────────────────────────────
namespace detail {

void primes_reg(Span<const MValue> args, size_t /*nargout*/, Span<MValue> outs, CallContext &ctx)
{
    if (args.empty())
        throw MError("primes: requires 1 argument",
                     0, 0, "primes", "", "m:primes:nargin");
    outs[0] = primes(ctx.engine->allocator(), args[0].toScalar());
}

void isprime_reg(Span<const MValue> args, size_t /*nargout*/, Span<MValue> outs, CallContext &ctx)
{
    if (args.empty())
        throw MError("isprime: requires 1 argument",
                     0, 0, "isprime", "", "m:isprime:nargin");
    outs[0] = isprime(ctx.engine->allocator(), args[0]);
}

void factor_reg(Span<const MValue> args, size_t /*nargout*/, Span<MValue> outs, CallContext &ctx)
{
    if (args.empty())
        throw MError("factor: requires 1 argument",
                     0, 0, "factor", "", "m:factor:nargin");
    if (!args[0].isScalar())
        throw MError("factor: argument must be a scalar",
                     0, 0, "factor", "", "m:factor:notScalar");
    outs[0] = factor(ctx.engine->allocator(), args[0].toScalar());
}

} // namespace detail

} // namespace numkit::m::builtin
