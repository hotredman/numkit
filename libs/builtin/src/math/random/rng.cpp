// libs/builtin/src/math/random/rng.cpp
// Shared RNG manager + integer random generators + rng() control.
// Routes rand/randn/randi/randperm through one process-static engine
// so MATLAB-style rng(seed) gives reproducible sequences across the
// whole RNG-using API surface.
// Engine: MATLAB-canonical MT19937 (Matsumoto-Nishimura reference,
// init_by_array seeding). For 53-bit double precision uniform values
// (rand()), genRes53 is used directly -- this gives bit-for-bit
// agreement with MATLAB R2025b's `rng(seed)` + `rand()` sequence.

#include <numkit/builtin/math/random/rng.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/core/types.hpp>

#include "helpers.hpp"
#include <numkit/builtin/math/random/matlab_mt19937.hpp>

#include <algorithm>
#include <cstdint>
#include <mutex>
#include <numeric>
#include <random>
#include <sstream>
#include <string>

namespace numkit::builtin {

// ────────────────────────────────────────────────────────────────────
// Process-static RNG state
// ────────────────────────────────────────────────────────────────────

std::mutex &rngMutex()
{
    static std::mutex m;
    return m;
}

detail::MatlabMT19937 &sharedEngine()
{
    // Default-constructed = init_by_array([0]) = MATLAB rng('default').
    static detail::MatlabMT19937 gen;
    return gen;
}

namespace {

// Serialise MatlabMT19937 state to / from a text blob.
// Format: "mt19937 <624 hex words> <index>"
std::string serializeEngine()
{
    uint32_t state[detail::MatlabMT19937::STATE_SIZE];
    int idx;
    sharedEngine().getState(state, idx);
    std::ostringstream os;
    os << "mt19937";
    for (std::size_t i = 0; i < detail::MatlabMT19937::STATE_SIZE; ++i)
        os << ' ' << state[i];
    os << ' ' << idx;
    return os.str();
}

void deserializeEngine(const std::string &blob)
{
    std::istringstream is(blob);
    std::string tag;
    is >> tag;
    if (tag != "mt19937")
        throw Error("rng: malformed state blob",
                     0, 0, "rng", "", "numkit:rng:badState");
    uint32_t state[detail::MatlabMT19937::STATE_SIZE];
    for (std::size_t i = 0; i < detail::MatlabMT19937::STATE_SIZE; ++i) {
        if (!(is >> state[i]))
            throw Error("rng: malformed state blob",
                         0, 0, "rng", "", "numkit:rng:badState");
    }
    int idx;
    if (!(is >> idx))
        throw Error("rng: malformed state blob",
                     0, 0, "rng", "", "numkit:rng:badState");
    sharedEngine().setState(state, idx);
}

} // namespace

// ────────────────────────────────────────────────────────────────────
// Seeding / state control
// ────────────────────────────────────────────────────────────────────

void rngSeed(uint64_t seed)
{
    std::lock_guard<std::mutex> lock(rngMutex());
    sharedEngine().seed(static_cast<uint32_t>(seed));
}

void rngShuffle()
{
    std::lock_guard<std::mutex> lock(rngMutex());
    std::random_device rd;
    sharedEngine().seed(rd());
}

Value rngState(std::pmr::memory_resource *mr)
{
    std::lock_guard<std::mutex> lock(rngMutex());
    auto blob = serializeEngine();
    auto s = Value::structure();
    s.field("Type")  = Value::fromString("twister", mr);
    s.field("State") = Value::fromString(blob, mr);
    return s;
}

void rngRestore(const Value &state)
{
    if (!state.isStruct())
        throw Error("rng: state must be a struct from rng()",
                     0, 0, "rng", "", "numkit:rng:notStruct");
    if (!state.hasField("State"))
        throw Error("rng: state struct missing .State field",
                     0, 0, "rng", "", "numkit:rng:noStateField");
    const auto &blob = state.field("State");
    if (!blob.isChar() && !blob.isString())
        throw Error("rng: .State must be a char array",
                     0, 0, "rng", "", "numkit:rng:badState");
    std::lock_guard<std::mutex> lock(rngMutex());
    deserializeEngine(blob.toString());
}

// ────────────────────────────────────────────────────────────────────
// Real-valued random (uniform / standard-normal). The previous
// static-RNG versions (replaced by this file) had no shared state with
// randi/randperm.
// ────────────────────────────────────────────────────────────────────

Value rand(detail::MatlabMT19937 &rng, size_t rows, size_t cols, size_t pages, std::pmr::memory_resource *mr)
{
    auto m = (pages > 0) ? Value::matrix3d(rows, cols, pages, ValueType::DOUBLE, mr)
                         : Value::matrix(rows, cols, ValueType::DOUBLE, mr);
    // genRes53 -- MATLAB-canonical 53-bit double in [0, 1).
    for (size_t i = 0; i < m.numel(); ++i)
        m.doubleDataMut()[i] = rng.genRes53();
    return m;
}

Value randn(detail::MatlabMT19937 &rng, size_t rows, size_t cols, size_t pages, std::pmr::memory_resource *mr)
{
    // NOTE: std::normal_distribution is NOT MATLAB-bit-identical (MATLAB
    // uses Marsaglia-Tsang Ziggurat with specific tables). Bit-identity
    // for randn() is a separate spec (Phase 0a-1b). Sequence is still
    // deterministic and seedable via rng().
    std::normal_distribution<double> dist(0.0, 1.0);
    auto m = (pages > 0) ? Value::matrix3d(rows, cols, pages, ValueType::DOUBLE, mr)
                         : Value::matrix(rows, cols, ValueType::DOUBLE, mr);
    for (size_t i = 0; i < m.numel(); ++i)
        m.doubleDataMut()[i] = dist(rng);
    return m;
}

Value randND(detail::MatlabMT19937 &rng, Span<const size_t> dims, std::pmr::memory_resource *mr)
{
    auto m = Value::matrixND(dims.data(), static_cast<int>(dims.size()), ValueType::DOUBLE, mr);
    for (size_t i = 0; i < m.numel(); ++i)
        m.doubleDataMut()[i] = rng.genRes53();
    return m;
}

Value randnND(detail::MatlabMT19937 &rng, Span<const size_t> dims, std::pmr::memory_resource *mr)
{
    auto m = Value::matrixND(dims.data(), static_cast<int>(dims.size()), ValueType::DOUBLE, mr);
    std::normal_distribution<double> dist(0.0, 1.0);
    for (size_t i = 0; i < m.numel(); ++i)
        m.doubleDataMut()[i] = dist(rng);
    return m;
}

// ────────────────────────────────────────────────────────────────────
// Integer random
// ────────────────────────────────────────────────────────────────────

namespace {

void fillUniformInt(double *dst, size_t n, int64_t lo, int64_t hi)
{
    if (lo > hi)
        throw Error("randi: low bound must be <= high bound",
                     0, 0, "randi", "", "numkit:randi:badRange");
    std::lock_guard<std::mutex> lock(rngMutex());
    std::uniform_int_distribution<int64_t> dist(lo, hi);
    for (size_t i = 0; i < n; ++i)
        dst[i] = static_cast<double>(dist(sharedEngine()));
}

Value makeIntMatrix(int64_t lo, int64_t hi, size_t rows, size_t cols, size_t pages, std::pmr::memory_resource *mr)
{
    auto m = (pages > 0) ? Value::matrix3d(rows, cols, pages, ValueType::DOUBLE, mr)
                         : Value::matrix(rows, cols, ValueType::DOUBLE, mr);
    fillUniformInt(m.doubleDataMut(), m.numel(), lo, hi);
    return m;
}

} // namespace

Value randi(int64_t imax, std::pmr::memory_resource *mr)
{
    std::lock_guard<std::mutex> lock(rngMutex());
    std::uniform_int_distribution<int64_t> dist(1, imax);
    return Value::scalar(static_cast<double>(dist(sharedEngine())), mr);
}

Value randi(int64_t imax, size_t rows, size_t cols, size_t pages, std::pmr::memory_resource *mr)
{
    return makeIntMatrix(1, imax, rows, cols, pages, mr);
}

Value randi(int64_t imin, int64_t imax, size_t rows, size_t cols, size_t pages, std::pmr::memory_resource *mr)
{
    return makeIntMatrix(imin, imax, rows, cols, pages, mr);
}

// ────────────────────────────────────────────────────────────────────
// Permutations
// ────────────────────────────────────────────────────────────────────
// randperm(n)    : Fisher-Yates shuffle of [1..n].
// randperm(n, k) : partial Fisher-Yates — k iterations are enough to
// produce k unique values without fully shuffling the rest.

Value randperm(size_t n, std::pmr::memory_resource *mr)
{
    return randperm(n, n, mr);
}

Value randperm(size_t n, size_t k, std::pmr::memory_resource *mr)
{
    if (k > n)
        throw Error("randperm: k must not exceed n",
                     0, 0, "randperm", "", "numkit:randperm:badK");
    auto r = Value::matrix(1, k, ValueType::DOUBLE, mr);
    if (k == 0) return r;

    // Fisher-Yates partial shuffle. We allocate a 1..n scratch buffer.
    // For tiny k vs huge n this is wasteful; an alternative is the
    // "selection sampling" algorithm (Knuth Vol 2, 3.4.2). Phase-4
    // scope is correctness; optimisation can come if benches care.
    ScratchArena scratch(mr);
    auto pool = ScratchVec<int64_t>(n, &scratch);
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

// Cast a DOUBLE Value buffer to single in-place (returns a new Value).
// Used by rand/randn when the user requests 'single'.
namespace { Value castDoubleToSingle(const Value &src, std::pmr::memory_resource *mr)
{
    Value dst;
    if (src.dims().is3D())
        dst = Value::matrix3d(src.dims().rows(), src.dims().cols(),
                              src.dims().pages(), ValueType::SINGLE, mr);
    else if (src.dims().ndim() > 3) {
        const auto &dimsRef = src.dims();
        size_t dimsBuf[Dims::kMaxRank];
        for (int i = 0; i < dimsRef.ndim(); ++i) dimsBuf[i] = dimsRef.dim(i);
        dst = Value::matrixND(dimsBuf, dimsRef.ndim(), ValueType::SINGLE, mr);
    } else
        dst = Value::matrix(src.dims().rows(), src.dims().cols(),
                            ValueType::SINGLE, mr);
    const size_t n = src.numel();
    const double *sp = src.doubleData();
    float *dp = dst.singleDataMut();
    for (size_t i = 0; i < n; ++i) dp[i] = static_cast<float>(sp[i]);
    return dst;
}}

// rand / randn supersede the earlier static-RNG versions. Same shape
// API (parseDimsArgs); the only change is they now share the engine
// that rng() controls.
void rand_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
              CallContext &ctx)
{
    auto *mr = ctx.engine->resource();
    ValueType t;
    auto dimArgs = extractTypeArg(args, t);
    if (t != ValueType::DOUBLE && t != ValueType::SINGLE)
        throw Error("rand: type must be 'double' or 'single'",
                    0, 0, "rand", "", "numkit:rand:badType");
    ScratchArena scratch(mr);
    auto dims = parseDimsArgsND(&scratch, dimArgs);
    stripTrailingOnes(dims);
    Value out;
    {
        std::lock_guard<std::mutex> lock(rngMutex());
        if (dims.size() <= 3) {
            const size_t r = dims.size() >= 1 ? dims[0] : 1;
            const size_t c = dims.size() >= 2 ? dims[1] : 1;
            const size_t p = dims.size() >= 3 ? dims[2] : 0;
            out = rand(sharedEngine(), r, c, p, mr);
        } else {
            out = randND(sharedEngine(), Span<const size_t>(dims.data(), dims.size()), mr);
        }
    }
    outs[0] = (t == ValueType::SINGLE) ? castDoubleToSingle(out, mr) : std::move(out);
}

void randn_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
               CallContext &ctx)
{
    auto *mr = ctx.engine->resource();
    ValueType t;
    auto dimArgs = extractTypeArg(args, t);
    if (t != ValueType::DOUBLE && t != ValueType::SINGLE)
        throw Error("randn: type must be 'double' or 'single'",
                    0, 0, "randn", "", "numkit:randn:badType");
    ScratchArena scratch(mr);
    auto dims = parseDimsArgsND(&scratch, dimArgs);
    stripTrailingOnes(dims);
    Value out;
    {
        std::lock_guard<std::mutex> lock(rngMutex());
        if (dims.size() <= 3) {
            const size_t r = dims.size() >= 1 ? dims[0] : 1;
            const size_t c = dims.size() >= 2 ? dims[1] : 1;
            const size_t p = dims.size() >= 3 ? dims[2] : 0;
            out = randn(sharedEngine(), r, c, p, mr);
        } else {
            out = randnND(sharedEngine(), Span<const size_t>(dims.data(), dims.size()), mr);
        }
    }
    outs[0] = (t == ValueType::SINGLE) ? castDoubleToSingle(out, mr) : std::move(out);
}

// Cast a DOUBLE Value to any integer or single class (for randi typed
// outputs). Saturating cast (out-of-range int64 values clamp).
namespace { Value castDoubleToType(const Value &src, ValueType t, std::pmr::memory_resource *mr)
{
    if (t == ValueType::DOUBLE) return src;
    Value dst;
    if (src.dims().is3D())
        dst = Value::matrix3d(src.dims().rows(), src.dims().cols(),
                              src.dims().pages(), t, mr);
    else if (src.dims().ndim() > 3) {
        const auto &dimsRef = src.dims();
        size_t dimsBuf[Dims::kMaxRank];
        for (int i = 0; i < dimsRef.ndim(); ++i) dimsBuf[i] = dimsRef.dim(i);
        dst = Value::matrixND(dimsBuf, dimsRef.ndim(), t, mr);
    } else
        dst = Value::matrix(src.dims().rows(), src.dims().cols(), t, mr);
    const size_t n = src.numel();
    const double *sp = src.doubleData();
    auto cast_loop = [&](auto *dp, auto /*tag*/) {
        using T = std::remove_pointer_t<decltype(dp)>;
        for (size_t i = 0; i < n; ++i) dp[i] = static_cast<T>(sp[i]);
    };
    switch (t) {
      case ValueType::SINGLE: cast_loop(dst.singleDataMut(),  float{});    break;
      case ValueType::INT8:   cast_loop(dst.int8DataMut(),    int8_t{});   break;
      case ValueType::INT16:  cast_loop(dst.int16DataMut(),   int16_t{});  break;
      case ValueType::INT32:  cast_loop(dst.int32DataMut(),   int32_t{});  break;
      case ValueType::INT64:  cast_loop(dst.int64DataMut(),   int64_t{});  break;
      case ValueType::UINT8:  cast_loop(dst.uint8DataMut(),   uint8_t{});  break;
      case ValueType::UINT16: cast_loop(dst.uint16DataMut(),  uint16_t{}); break;
      case ValueType::UINT32: cast_loop(dst.uint32DataMut(),  uint32_t{}); break;
      case ValueType::UINT64: cast_loop(dst.uint64DataMut(),  uint64_t{}); break;
      default: throw Error("randi: unsupported type for cast",
                           0, 0, "randi", "", "numkit:randi:badType");
    }
    return dst;
}}

// randi MATLAB forms:
//   randi(imax)                    scalar
//   randi(imax, n)                 n×n
//   randi(imax, m, n[, p])         shape
//   randi(imax, [m n p])           shape via vector
//   randi([imin imax], …)          range form (first arg is 2-vector)
//   randi(..., 'type')             typed output (any int / 'double' / 'single')
void randi_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
               CallContext &ctx)
{
    if (args.empty())
        throw Error("randi: requires at least 1 argument",
                     0, 0, "randi", "", "numkit:randi:nargin");

    int64_t imin = 1, imax = 0;
    const Value &first = args[0];
    if (!first.isScalar() && first.numel() == 2) {
        imin = static_cast<int64_t>(first.doubleData()[0]);
        imax = static_cast<int64_t>(first.doubleData()[1]);
    } else {
        imax = static_cast<int64_t>(first.toScalar());
    }

    Span<const Value> dimArgs = (args.size() > 1) ? args.subspan(1) : Span<const Value>{};
    auto *mr = ctx.engine->resource();

    // Strip trailing class-name from dim args.
    ValueType t;
    dimArgs = extractTypeArg(dimArgs, t);

    Value dbl_out;
    if (dimArgs.empty()) {
        // Scalar form.
        dbl_out = randi(imin, imax, 1, 1, 0, mr);
    } else {
        ScratchArena scratch(mr);
        auto dims = parseDimsArgsND(&scratch, dimArgs);
        stripTrailingOnes(dims);
        if (dims.size() <= 3) {
            const size_t r = dims.size() >= 1 ? dims[0] : 1;
            const size_t c = dims.size() >= 2 ? dims[1] : 1;
            const size_t p = dims.size() >= 3 ? dims[2] : 0;
            dbl_out = randi(imin, imax, r, c, p, mr);
        } else {
            // ND form: allocate matrixND and fill via the same uniform-int pass.
            auto m = Value::matrixND(dims.data(), static_cast<int>(dims.size()),
                                      ValueType::DOUBLE, mr);
            std::lock_guard<std::mutex> lock(rngMutex());
            std::uniform_int_distribution<int64_t> dist(imin, imax);
            for (size_t i = 0; i < m.numel(); ++i)
                m.doubleDataMut()[i] = static_cast<double>(dist(sharedEngine()));
            dbl_out = std::move(m);
        }
    }
    outs[0] = (t == ValueType::DOUBLE) ? std::move(dbl_out)
                                       : castDoubleToType(dbl_out, t, mr);
}

void randperm_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
                  CallContext &ctx)
{
    if (args.empty())
        throw Error("randperm: requires at least 1 argument",
                     0, 0, "randperm", "", "numkit:randperm:nargin");
    const size_t n = static_cast<size_t>(args[0].toScalar());
    if (args.size() == 1) {
        outs[0] = randperm(n, ctx.engine->resource());
    } else {
        const size_t k = static_cast<size_t>(args[1].toScalar());
        outs[0] = randperm(n, k, ctx.engine->resource());
    }
}

// rng MATLAB forms:
//   rng()              return current state struct (read-only snapshot)
//   rng(seed)          seed with integer
//   rng('default')     rng(0)
//   rng('shuffle')     seed from random_device
//   rng(state_struct)  restore previously-snapshotted state
// nargout > 0 : return the current state BEFORE seeding/restoring.
// (Matches MATLAB: `prev = rng(123)` snapshots the old state and seeds.)
void rng_reg(Span<const Value> args, size_t nargout, Span<Value> outs,
             CallContext &ctx)
{
    auto *mr = ctx.engine->resource();

    // Always snapshot current state first if caller asked for it.
    Value prev;
    if (nargout > 0)
        prev = rngState(mr);

    if (args.empty()) {
        // rng() with no return value is a no-op; with a return it
        // gives the snapshot.
        if (nargout > 0) outs[0] = std::move(prev);
        return;
    }

    const Value &a = args[0];
    if (a.isStruct()) {
        rngRestore(a);
    } else if (a.isChar() || a.isString()) {
        const auto s = a.toString();
        if (s == "default") rngSeed(0);
        else if (s == "shuffle") rngShuffle();
        else
            throw Error("rng: string argument must be 'default' or 'shuffle'",
                         0, 0, "rng", "", "numkit:rng:badStringArg");
    } else if (a.isScalar() || a.numel() == 1) {
        const double sd = a.toScalar();
        if (sd < 0.0)
            throw Error("rng: seed must be a non-negative integer",
                         0, 0, "rng", "", "numkit:rng:badSeed");
        rngSeed(static_cast<uint64_t>(sd));
    } else {
        throw Error("rng: argument must be a non-negative integer, "
                     "a struct from rng(), 'default', or 'shuffle'",
                     0, 0, "rng", "", "numkit:rng:badArg");
    }

    if (nargout > 0) outs[0] = std::move(prev);
}

} // namespace detail

} // namespace numkit::builtin
