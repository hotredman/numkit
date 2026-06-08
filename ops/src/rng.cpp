// ops/src/rng.cpp
// Shared RNG compute: the process-static MT19937 stream + value-producing
// generators (rand/randn/randi/randperm) + rng() state control. Routes all
// RNG through one engine so MATLAB-style rng(seed) reproduces sequences across
// the whole RNG-using surface. Bit-identical with MATLAB R2025b's rng()+rand()
// (53-bit genRes53). Engine-free (Value + MatlabMT19937 only) — the builtins
// that parse shape/type args live in toolboxes/builtin and call these.

#include <numkit/ops/rng.hpp>

#include <numkit/value/error.hpp>
#include <numkit/value/scratch.hpp>

#include <algorithm>
#include <cstdint>
#include <mutex>
#include <numeric>
#include <random>
#include <sstream>
#include <string>

namespace numkit::ops {

// ────────────────────────────────────────────────────────────────────
// Process-static RNG state
// ────────────────────────────────────────────────────────────────────

std::mutex &rngMutex()
{
    static std::mutex m;
    return m;
}

MatlabMT19937 &sharedEngine()
{
    // Default-constructed = init_by_array([0]) = MATLAB rng('default').
    static MatlabMT19937 gen;
    return gen;
}

namespace {

// Serialise MatlabMT19937 state to / from a text blob.
// Format: "mt19937 <624 hex words> <index>"
std::string serializeEngine()
{
    uint32_t state[MatlabMT19937::STATE_SIZE];
    int idx;
    sharedEngine().getState(state, idx);
    std::ostringstream os;
    os << "mt19937";
    for (std::size_t i = 0; i < MatlabMT19937::STATE_SIZE; ++i)
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
    uint32_t state[MatlabMT19937::STATE_SIZE];
    for (std::size_t i = 0; i < MatlabMT19937::STATE_SIZE; ++i) {
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
// Real-valued random (uniform / standard-normal)
// ────────────────────────────────────────────────────────────────────

Value rand(MatlabMT19937 &rng, size_t rows, size_t cols, size_t pages, std::pmr::memory_resource *mr)
{
    auto m = (pages > 0) ? Value::matrix3d(rows, cols, pages, ValueType::DOUBLE, mr)
                         : Value::matrix(rows, cols, ValueType::DOUBLE, mr);
    // genRes53 -- MATLAB-canonical 53-bit double in [0, 1).
    for (size_t i = 0; i < m.numel(); ++i)
        m.doubleDataMut()[i] = rng.genRes53();
    return m;
}

Value randn(MatlabMT19937 &rng, size_t rows, size_t cols, size_t pages, std::pmr::memory_resource *mr)
{
    // NOTE: std::normal_distribution is NOT MATLAB-bit-identical (MATLAB
    // uses Marsaglia-Tsang Ziggurat with specific tables). Bit-identity
    // for randn() is a separate spec. Sequence is still deterministic and
    // seedable via rng().
    std::normal_distribution<double> dist(0.0, 1.0);
    auto m = (pages > 0) ? Value::matrix3d(rows, cols, pages, ValueType::DOUBLE, mr)
                         : Value::matrix(rows, cols, ValueType::DOUBLE, mr);
    for (size_t i = 0; i < m.numel(); ++i)
        m.doubleDataMut()[i] = dist(rng);
    return m;
}

Value randND(MatlabMT19937 &rng, Span<const size_t> dims, std::pmr::memory_resource *mr)
{
    auto m = Value::matrixND(dims.data(), static_cast<int>(dims.size()), ValueType::DOUBLE, mr);
    for (size_t i = 0; i < m.numel(); ++i)
        m.doubleDataMut()[i] = rng.genRes53();
    return m;
}

Value randnND(MatlabMT19937 &rng, Span<const size_t> dims, std::pmr::memory_resource *mr)
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
// randperm(n, k) : partial Fisher-Yates — k iterations produce k unique
// values without fully shuffling the rest.

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

    // Fisher-Yates partial shuffle over a 1..n scratch buffer.
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

} // namespace numkit::ops
