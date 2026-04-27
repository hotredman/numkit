// libs/builtin/src/MStdRng.cpp
//
// Shared RNG manager + integer random generators + rng() control
// function. Routes rand/randn/randi/randperm through one process-
// static std::mt19937 so MATLAB-style rng(seed) gives reproducible
// sequences across the whole RNG-using API surface.

#include <numkit/m/builtin/MStdRng.hpp>

#include <numkit/m/builtin/MStdMath.hpp>   // rand / randn (real-valued)
#include <numkit/m/core/MEngine.hpp>
#include <numkit/m/core/MTypes.hpp>

#include "MStdHelpers.hpp"

#include <algorithm>
#include <cstdint>
#include <mutex>
#include <numeric>
#include <random>
#include <sstream>
#include <string>

namespace numkit::m::builtin {

// ────────────────────────────────────────────────────────────────────
// Process-static RNG state
// ────────────────────────────────────────────────────────────────────
namespace {

std::mutex &rngMutex()
{
    static std::mutex m;
    return m;
}

std::mt19937 &sharedEngine()
{
    // Default-constructed mt19937 = seed 5489 (Mersenne Twister default).
    // MATLAB's rng('default') seeds with 0; we honour the MATLAB default
    // by seeding to 0 on first use rather than relying on mt19937's.
    static std::mt19937 gen(0u);
    return gen;
}

// Serialise mt19937 state to / from a text blob. mt19937's stream
// operators emit 624 + 1 numbers separated by whitespace — robust
// across compilers/standard libraries (the format is mandated by
// the standard).
std::string serializeEngine()
{
    std::ostringstream os;
    os << sharedEngine();
    return os.str();
}

void deserializeEngine(const std::string &blob)
{
    std::istringstream is(blob);
    is >> sharedEngine();
    if (!is)
        throw MError("rng: malformed state blob",
                     0, 0, "rng", "", "m:rng:badState");
}

} // namespace

// ────────────────────────────────────────────────────────────────────
// Seeding / state control
// ────────────────────────────────────────────────────────────────────

void rngSeed(uint64_t seed)
{
    std::lock_guard<std::mutex> lock(rngMutex());
    sharedEngine().seed(static_cast<std::mt19937::result_type>(seed));
}

void rngShuffle()
{
    std::lock_guard<std::mutex> lock(rngMutex());
    std::random_device rd;
    sharedEngine().seed(rd());
}

MValue rngState(Allocator &alloc)
{
    std::lock_guard<std::mutex> lock(rngMutex());
    auto blob = serializeEngine();
    auto s = MValue::structure();
    s.field("Type")  = MValue::fromString("twister", &alloc);
    s.field("State") = MValue::fromString(blob, &alloc);
    return s;
}

void rngRestore(const MValue &state)
{
    if (!state.isStruct())
        throw MError("rng: state must be a struct from rng()",
                     0, 0, "rng", "", "m:rng:notStruct");
    if (!state.hasField("State"))
        throw MError("rng: state struct missing .State field",
                     0, 0, "rng", "", "m:rng:noStateField");
    const auto &blob = state.field("State");
    if (!blob.isChar() && !blob.isString())
        throw MError("rng: .State must be a char array",
                     0, 0, "rng", "", "m:rng:badState");
    std::lock_guard<std::mutex> lock(rngMutex());
    deserializeEngine(blob.toString());
}

// ────────────────────────────────────────────────────────────────────
// Integer random
// ────────────────────────────────────────────────────────────────────

namespace {

void fillUniformInt(double *dst, size_t n, int64_t lo, int64_t hi)
{
    if (lo > hi)
        throw MError("randi: low bound must be <= high bound",
                     0, 0, "randi", "", "m:randi:badRange");
    std::lock_guard<std::mutex> lock(rngMutex());
    std::uniform_int_distribution<int64_t> dist(lo, hi);
    for (size_t i = 0; i < n; ++i)
        dst[i] = static_cast<double>(dist(sharedEngine()));
}

MValue makeIntMatrix(Allocator &alloc, int64_t lo, int64_t hi,
                     size_t rows, size_t cols, size_t pages)
{
    auto m = (pages > 0) ? MValue::matrix3d(rows, cols, pages, MType::DOUBLE, &alloc)
                         : MValue::matrix(rows, cols, MType::DOUBLE, &alloc);
    fillUniformInt(m.doubleDataMut(), m.numel(), lo, hi);
    return m;
}

} // namespace

MValue randi(Allocator &alloc, int64_t imax)
{
    std::lock_guard<std::mutex> lock(rngMutex());
    std::uniform_int_distribution<int64_t> dist(1, imax);
    return MValue::scalar(static_cast<double>(dist(sharedEngine())), &alloc);
}

MValue randi(Allocator &alloc, int64_t imax,
             size_t rows, size_t cols, size_t pages)
{
    return makeIntMatrix(alloc, 1, imax, rows, cols, pages);
}

MValue randi(Allocator &alloc, int64_t imin, int64_t imax,
             size_t rows, size_t cols, size_t pages)
{
    return makeIntMatrix(alloc, imin, imax, rows, cols, pages);
}

// ────────────────────────────────────────────────────────────────────
// Permutations
// ────────────────────────────────────────────────────────────────────
//
// randperm(n)    : Fisher-Yates shuffle of [1..n].
// randperm(n, k) : partial Fisher-Yates — k iterations are enough to
// produce k unique values without fully shuffling the rest.

MValue randperm(Allocator &alloc, size_t n)
{
    return randperm(alloc, n, n);
}

MValue randperm(Allocator &alloc, size_t n, size_t k)
{
    if (k > n)
        throw MError("randperm: k must not exceed n",
                     0, 0, "randperm", "", "m:randperm:badK");
    auto r = MValue::matrix(1, k, MType::DOUBLE, &alloc);
    if (k == 0) return r;

    // Fisher-Yates partial shuffle. We allocate a 1..n scratch buffer.
    // For tiny k vs huge n this is wasteful; an alternative is the
    // "selection sampling" algorithm (Knuth Vol 2, 3.4.2). Phase-4
    // scope is correctness; optimisation can come if benches care.
    std::vector<int64_t> pool(n);
    std::iota(pool.begin(), pool.end(), int64_t{1});

    std::lock_guard<std::mutex> lock(rngMutex());
    auto &gen = sharedEngine();
    double *dst = r.doubleDataMut();
    for (size_t i = 0; i < k; ++i) {
        std::uniform_int_distribution<size_t> dist(i, n - 1);
        const size_t j = dist(gen);
        std::swap(pool[i], pool[j]);
        dst[i] = static_cast<double>(pool[i]);
    }
    return r;
}

// ════════════════════════════════════════════════════════════════════
// Engine adapters
// ════════════════════════════════════════════════════════════════════
namespace detail {

// rand / randn supersede the previous static-RNG versions in
// MStdMath.cpp. Same shape API (parseDimsArgs); the only change is
// they now share the engine that rng() controls.
void rand_reg(Span<const MValue> args, size_t /*nargout*/, Span<MValue> outs,
              CallContext &ctx)
{
    auto dims = parseDimsArgsND(args);
    stripTrailingOnes(dims);
    std::lock_guard<std::mutex> lock(rngMutex());
    auto &alloc = ctx.engine->allocator();
    if (dims.size() <= 3) {
        const size_t r = dims.size() >= 1 ? dims[0] : 1;
        const size_t c = dims.size() >= 2 ? dims[1] : 1;
        const size_t p = dims.size() >= 3 ? dims[2] : 0;
        outs[0] = rand(alloc, sharedEngine(), r, c, p);
    } else {
        outs[0] = randND(alloc, sharedEngine(),
                         dims.data(), static_cast<int>(dims.size()));
    }
}

void randn_reg(Span<const MValue> args, size_t /*nargout*/, Span<MValue> outs,
               CallContext &ctx)
{
    auto dims = parseDimsArgsND(args);
    stripTrailingOnes(dims);
    std::lock_guard<std::mutex> lock(rngMutex());
    auto &alloc = ctx.engine->allocator();
    if (dims.size() <= 3) {
        const size_t r = dims.size() >= 1 ? dims[0] : 1;
        const size_t c = dims.size() >= 2 ? dims[1] : 1;
        const size_t p = dims.size() >= 3 ? dims[2] : 0;
        outs[0] = randn(alloc, sharedEngine(), r, c, p);
    } else {
        outs[0] = randnND(alloc, sharedEngine(),
                          dims.data(), static_cast<int>(dims.size()));
    }
}

// randi MATLAB forms:
//   randi(imax)                    scalar
//   randi(imax, n)                 n×n
//   randi(imax, m, n[, p])         shape
//   randi(imax, [m n p])           shape via vector
//   randi([imin imax], …)          range form (first arg is 2-vector)
void randi_reg(Span<const MValue> args, size_t /*nargout*/, Span<MValue> outs,
               CallContext &ctx)
{
    if (args.empty())
        throw MError("randi: requires at least 1 argument",
                     0, 0, "randi", "", "m:randi:nargin");

    int64_t imin = 1, imax = 0;
    const MValue &first = args[0];
    if (!first.isScalar() && first.numel() == 2) {
        imin = static_cast<int64_t>(first.doubleData()[0]);
        imax = static_cast<int64_t>(first.doubleData()[1]);
    } else {
        imax = static_cast<int64_t>(first.toScalar());
    }

    Span<const MValue> dimArgs = (args.size() > 1) ? args.subspan(1) : Span<const MValue>{};
    auto &alloc = ctx.engine->allocator();

    if (dimArgs.empty()) {
        // Scalar form.
        outs[0] = randi(alloc, imin, imax, 1, 1, 0);
        return;
    }
    auto dims = parseDimsArgsND(dimArgs);
    stripTrailingOnes(dims);
    if (dims.size() <= 3) {
        const size_t r = dims.size() >= 1 ? dims[0] : 1;
        const size_t c = dims.size() >= 2 ? dims[1] : 1;
        const size_t p = dims.size() >= 3 ? dims[2] : 0;
        outs[0] = randi(alloc, imin, imax, r, c, p);
    } else {
        // ND form: allocate matrixND and fill via the same uniform-int pass.
        auto m = MValue::matrixND(dims.data(), static_cast<int>(dims.size()),
                                  MType::DOUBLE, &alloc);
        std::lock_guard<std::mutex> lock(rngMutex());
        std::uniform_int_distribution<int64_t> dist(imin, imax);
        for (size_t i = 0; i < m.numel(); ++i)
            m.doubleDataMut()[i] = static_cast<double>(dist(sharedEngine()));
        outs[0] = std::move(m);
    }
}

void randperm_reg(Span<const MValue> args, size_t /*nargout*/, Span<MValue> outs,
                  CallContext &ctx)
{
    if (args.empty())
        throw MError("randperm: requires at least 1 argument",
                     0, 0, "randperm", "", "m:randperm:nargin");
    const size_t n = static_cast<size_t>(args[0].toScalar());
    if (args.size() == 1) {
        outs[0] = randperm(ctx.engine->allocator(), n);
    } else {
        const size_t k = static_cast<size_t>(args[1].toScalar());
        outs[0] = randperm(ctx.engine->allocator(), n, k);
    }
}

// rng MATLAB forms:
//   rng()              return current state struct (read-only snapshot)
//   rng(seed)          seed with integer
//   rng('default')     rng(0)
//   rng('shuffle')     seed from random_device
//   rng(state_struct)  restore previously-snapshotted state
//
// nargout > 0 : return the current state BEFORE seeding/restoring.
// (Matches MATLAB: `prev = rng(123)` snapshots the old state and seeds.)
void rng_reg(Span<const MValue> args, size_t nargout, Span<MValue> outs,
             CallContext &ctx)
{
    auto &alloc = ctx.engine->allocator();

    // Always snapshot current state first if caller asked for it.
    MValue prev;
    if (nargout > 0)
        prev = rngState(alloc);

    if (args.empty()) {
        // rng() with no return value is a no-op; with a return it
        // gives the snapshot.
        if (nargout > 0) outs[0] = std::move(prev);
        return;
    }

    const MValue &a = args[0];
    if (a.isStruct()) {
        rngRestore(a);
    } else if (a.isChar() || a.isString()) {
        const auto s = a.toString();
        if (s == "default") rngSeed(0);
        else if (s == "shuffle") rngShuffle();
        else
            throw MError("rng: string argument must be 'default' or 'shuffle'",
                         0, 0, "rng", "", "m:rng:badStringArg");
    } else if (a.isScalar() || a.numel() == 1) {
        const double sd = a.toScalar();
        if (sd < 0.0)
            throw MError("rng: seed must be a non-negative integer",
                         0, 0, "rng", "", "m:rng:badSeed");
        rngSeed(static_cast<uint64_t>(sd));
    } else {
        throw MError("rng: argument must be a non-negative integer, "
                     "a struct from rng(), 'default', or 'shuffle'",
                     0, 0, "rng", "", "m:rng:badArg");
    }

    if (nargout > 0) outs[0] = std::move(prev);
}

} // namespace detail

} // namespace numkit::m::builtin
