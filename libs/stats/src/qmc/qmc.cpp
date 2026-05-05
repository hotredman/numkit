// libs/stats/src/qmc/qmc.cpp

#include <numkit/stats/qmc/qmc.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include <vector>

namespace numkit::stats {

namespace {

// First d primes via simple sieve / trial division.
std::vector<int> first_primes(size_t d)
{
    std::vector<int> ps;
    ps.reserve(d);
    int x = 2;
    while (ps.size() < d) {
        bool p = true;
        for (int q : ps) {
            if (q * q > x) break;
            if (x % q == 0) { p = false; break; }
        }
        if (p) ps.push_back(x);
        ++x;
    }
    return ps;
}

// Radical inverse φ_p(i) = sum_k digit_k(i) · p^(-k-1).
double radical_inverse(long long i, int p)
{
    double r = 0.0;
    double f = 1.0 / static_cast<double>(p);
    while (i > 0) {
        r += f * static_cast<double>(i % p);
        i /= p;
        f /= static_cast<double>(p);
    }
    return r;
}

} // anonymous

Value haltonset(std::pmr::memory_resource *mr, int d, long long skip, long long leap)
{
    if (d < 1)
        throw Error("haltonset: dim must be ≥ 1",
                    0, 0, "haltonset", "", "m:haltonset:dim");
    Value s = Value::structure(mr);
    s.field("kind") = Value::fromString("halton", mr);
    s.field("dim")  = Value::scalar(static_cast<double>(d), mr);
    s.field("skip") = Value::scalar(static_cast<double>(skip), mr);
    s.field("leap") = Value::scalar(static_cast<double>(leap), mr);
    return s;
}

Value net(std::pmr::memory_resource *mr, const Value &stream, long long n)
{
    if (!stream.isStruct() || !stream.hasField("kind"))
        throw Error("net: input must be a quasi-random stream struct",
                    0, 0, "net", "", "m:net:struct");
    const std::string kind = stream.field("kind").toString();
    if (kind != "halton")
        throw Error("net: only halton streams supported in this release",
                    0, 0, "net", "", "m:net:kind");
    if (n < 0)
        throw Error("net: n must be ≥ 0",
                    0, 0, "net", "", "m:net:n");
    const size_t d    = static_cast<size_t>(stream.field("dim").toScalar());
    const long long skip = static_cast<long long>(stream.field("skip").toScalar());
    const long long leap = static_cast<long long>(stream.field("leap").toScalar());
    const std::vector<int> primes = first_primes(d);
    const long long step = leap + 1;
    Value X = Value::matrix(static_cast<size_t>(n), d, ValueType::DOUBLE, mr);
    if (n == 0) return X;
    double *xd = X.doubleDataMut();
    // MATLAB: default Skip=1 (handled in haltonset() ctor); row r maps
    // to index = skip + r·step (so 'Skip', 0 yields the trivial origin).
    for (long long row = 0; row < n; ++row) {
        const long long idx = skip + row * step;
        for (size_t k = 0; k < d; ++k)
            xd[static_cast<size_t>(row) + k * static_cast<size_t>(n)]
                = radical_inverse(idx, primes[k]);
    }
    return X;
}

// ════════════════════════════════════════════════════════════════════
// Engine adapters
// ════════════════════════════════════════════════════════════════════

namespace detail {

void haltonset_reg(Span<const Value> args, size_t /*nargout*/,
                   Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("haltonset: requires (dim[, 'Skip', s, 'Leap', l])",
                    0, 0, "haltonset", "", "m:haltonset:nargin");
    const int d = static_cast<int>(args[0].toScalar());
    // MATLAB's default Skip is 0 — the trivial origin [0…0] is the first
    // returned point. 'Skip', 1 starts at index 1 = [0.5, 1/3, …].
    long long skip = 0, leap = 0;
    for (size_t i = 1; i + 1 < args.size(); i += 2) {
        if (!args[i].isChar() && !args[i].isString()) break;
        std::string name = args[i].toString();
        for (auto &c : name) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        const Value &v = args[i + 1];
        if      (name == "skip") skip = static_cast<long long>(v.toScalar());
        else if (name == "leap") leap = static_cast<long long>(v.toScalar());
    }
    outs[0] = haltonset(ctx.engine->resource(), d, skip, leap);
}

void net_reg(Span<const Value> args, size_t /*nargout*/,
             Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("net: requires (stream, n)",
                    0, 0, "net", "", "m:net:nargin");
    const long long n = static_cast<long long>(args[1].toScalar());
    outs[0] = net(ctx.engine->resource(), args[0], n);
}

} // namespace detail
} // namespace numkit::stats
