// ops/src/rng.cpp
// Value-producing random generators + RngContext rng() control. Every generator
// draws from a caller-provided RngContext (the Engine owns one; engine.rng()) —
// no process-global stream, no mutex. A session shares ONE reproducible stream
// (MATLAB `rng(seed)`); two Engines are independent. Bit-identical with MATLAB
// R2025b's rng()+rand() (53-bit genRes53). Engine-free (Value + RngContext).

#include <numkit/ops/rng.hpp>

#include <numkit/value/error.hpp>
#include <numkit/value/scratch.hpp>

#include <algorithm>
#include <cstdint>
#include <numeric>
#include <random>
#include <sstream>
#include <string>

namespace numkit::ops {

// ────────────────────────────────────────────────────────────────────
// RngContext state control (seed lives inline in the header)
// ────────────────────────────────────────────────────────────────────

namespace {

// Serialise / restore MT19937 state as a text blob: "mt19937 <624 words> <idx>".
std::string serializeState(const MatlabMT19937 &gen)
{
    uint32_t state[MatlabMT19937::STATE_SIZE];
    int idx;
    gen.getState(state, idx);
    std::ostringstream os;
    os << "mt19937";
    for (std::size_t i = 0; i < MatlabMT19937::STATE_SIZE; ++i)
        os << ' ' << state[i];
    os << ' ' << idx;
    return os.str();
}

void deserializeState(MatlabMT19937 &gen, const std::string &blob)
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
    gen.setState(state, idx);
}

} // namespace

void RngContext::shuffle()
{
    std::random_device rd;
    gen_.seed(rd());
}

Value RngContext::state(std::pmr::memory_resource *mr) const
{
    auto blob = serializeState(gen_);
    auto s = Value::structure();
    s.field("Type")  = Value::fromString("twister", mr);
    s.field("State") = Value::fromString(blob, mr);
    return s;
}

void RngContext::restore(const Value &state)
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
    deserializeState(gen_, blob.toString());
}

// ────────────────────────────────────────────────────────────────────
// Real-valued random (uniform / standard-normal)
// ────────────────────────────────────────────────────────────────────

// Normal draw: MATLAB v4 polar method in legacy mode (bit-identical,
// see RngContext::v4Normal); std::normal_distribution otherwise (NOT
// MATLAB-bit-identical in the modern mode — documented).
namespace {
double legacyNormal(RngContext &rng)
{
    if (rng.legacyV4())
        return rng.v4Normal();
    std::normal_distribution<double> dist(0.0, 1.0);
    return dist(rng);
}
} // namespace

Value rand(RngContext &rng, size_t rows, size_t cols, size_t pages, std::pmr::memory_resource *mr)
{
    auto m = (pages > 0) ? Value::matrix3d(rows, cols, pages, ValueType::DOUBLE, mr)
                         : Value::matrix(rows, cols, ValueType::DOUBLE, mr);
    // genRes53 -- MATLAB-canonical 53-bit double in [0, 1); the legacy
    // v4 mode ('seed') draws from the Park-Miller stream instead.
    for (size_t i = 0; i < m.numel(); ++i)
        m.doubleDataMut()[i] = rng.legacyV4() ? rng.v4Uniform() : rng.genRes53();
    return m;
}

Value randn(RngContext &rng, size_t rows, size_t cols, size_t pages, std::pmr::memory_resource *mr)
{
    // NOTE: std::normal_distribution is NOT MATLAB-bit-identical (MATLAB
    // uses Marsaglia-Tsang Ziggurat with specific tables). Sequence is still
    // deterministic and seedable via the session RngContext.
    auto m = (pages > 0) ? Value::matrix3d(rows, cols, pages, ValueType::DOUBLE, mr)
                         : Value::matrix(rows, cols, ValueType::DOUBLE, mr);
    for (size_t i = 0; i < m.numel(); ++i)
        m.doubleDataMut()[i] = legacyNormal(rng);
    return m;
}

Value randND(RngContext &rng, Span<const size_t> dims, std::pmr::memory_resource *mr)
{
    auto m = Value::matrixND(dims.data(), static_cast<int>(dims.size()), ValueType::DOUBLE, mr);
    for (size_t i = 0; i < m.numel(); ++i)
        m.doubleDataMut()[i] = rng.legacyV4() ? rng.v4Uniform() : rng.genRes53();
    return m;
}

Value randnND(RngContext &rng, Span<const size_t> dims, std::pmr::memory_resource *mr)
{
    auto m = Value::matrixND(dims.data(), static_cast<int>(dims.size()), ValueType::DOUBLE, mr);
    for (size_t i = 0; i < m.numel(); ++i)
        m.doubleDataMut()[i] = legacyNormal(rng);
    return m;
}

// ────────────────────────────────────────────────────────────────────
// Integer random
// ────────────────────────────────────────────────────────────────────

namespace {

void fillUniformInt(RngContext &rng, double *dst, size_t n, int64_t lo, int64_t hi)
{
    if (lo > hi)
        throw Error("randi: low bound must be <= high bound",
                     0, 0, "randi", "", "numkit:randi:badRange");
    std::uniform_int_distribution<int64_t> dist(lo, hi);
    for (size_t i = 0; i < n; ++i)
        dst[i] = static_cast<double>(dist(rng));
}

Value makeIntMatrix(RngContext &rng, int64_t lo, int64_t hi, size_t rows, size_t cols,
                    size_t pages, std::pmr::memory_resource *mr)
{
    auto m = (pages > 0) ? Value::matrix3d(rows, cols, pages, ValueType::DOUBLE, mr)
                         : Value::matrix(rows, cols, ValueType::DOUBLE, mr);
    fillUniformInt(rng, m.doubleDataMut(), m.numel(), lo, hi);
    return m;
}

} // namespace

Value randi(RngContext &rng, int64_t imax, std::pmr::memory_resource *mr)
{
    std::uniform_int_distribution<int64_t> dist(1, imax);
    return Value::scalar(static_cast<double>(dist(rng)), mr);
}

Value randi(RngContext &rng, int64_t imax, size_t rows, size_t cols, size_t pages, std::pmr::memory_resource *mr)
{
    return makeIntMatrix(rng, 1, imax, rows, cols, pages, mr);
}

Value randi(RngContext &rng, int64_t imin, int64_t imax, size_t rows, size_t cols, size_t pages, std::pmr::memory_resource *mr)
{
    return makeIntMatrix(rng, imin, imax, rows, cols, pages, mr);
}

// ────────────────────────────────────────────────────────────────────
// Permutations
// ────────────────────────────────────────────────────────────────────
// randperm(n)    : Fisher-Yates shuffle of [1..n].
// randperm(n, k) : partial Fisher-Yates — k iterations produce k unique
// values without fully shuffling the rest.

Value randperm(RngContext &rng, size_t n, std::pmr::memory_resource *mr)
{
    return randperm(rng, n, n, mr);
}

Value randperm(RngContext &rng, size_t n, size_t k, std::pmr::memory_resource *mr)
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

    double *dst = r.doubleDataMut();
    for (size_t i = 0; i < k; ++i) {
        std::uniform_int_distribution<size_t> dist(i, n - 1);
        const size_t j = dist(rng);
        std::swap(pool[i], pool[j]);
        dst[i] = static_cast<double>(pool[i]);
    }
    return r;
}

} // namespace numkit::ops
