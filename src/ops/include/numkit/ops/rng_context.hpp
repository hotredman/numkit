// ops/include/numkit/ops/rng_context.hpp
//
// RngContext — the seedable random-number session state. Owns one MATLAB-style
// MT19937 stream and the rng() control surface (seed / shuffle / state save +
// restore). The Engine owns one RngContext (engine.rng()); every RNG-using
// function (rand/randn/randi/randperm + the toolbox *rnd samplers, comm noise,
// …) takes `RngContext &` and draws from it — so a single session shares ONE
// reproducible stream (MATLAB `rng(seed)` semantics) WITHOUT a process-global
// singleton or a mutex. Two Engines have independent streams.
//
// It models UniformRandomBitGenerator (forwards operator()/min/max/result_type
// to the owned MT19937), so the std distributions draw from it directly:
//   std::normal_distribution<double> d; double x = d(rng);
// An L0.5 ops type (Value + MatlabMT19937 only, no Engine).

#pragma once

#include <numkit/ops/matlab_mt19937.hpp>
#include <numkit/value/value.hpp>

#include <cmath>    // v4Normal: std::sqrt / std::log
#include <cstdint>
#include <memory_resource>
#include <random>   // RngContext models a URBG — callers draw via std::*_distribution(rng)

namespace numkit::ops {

class RngContext
{
public:
    // ── UniformRandomBitGenerator surface (forwarded to the owned stream) ──
    // Lets std::*_distribution and the value generators draw via `dist(rng)`.
    using result_type = MatlabMT19937::result_type;
    static constexpr result_type min() { return MatlabMT19937::min(); }
    static constexpr result_type max() { return MatlabMT19937::max(); }
    result_type operator()() { return gen_(); }

    /// MATLAB-canonical 53-bit double in [0, 1) — what `rand` uses.
    double genRes53() { return gen_.genRes53(); }

    // ── MATLAB rng() control ──────────────────────────────────────────
    /// Seed the stream. `seed == 0` ≡ `rng('default')`.
    void seed(std::uint64_t seed)
    {
        legacyV4_ = false;
        gen_.seed(static_cast<std::uint32_t>(seed));
    }
    /// `rng('shuffle')` — seed from a nondeterministic source.
    void shuffle();
    /// `s = rng()` — capture the stream state as a struct (.Type / .State).
    Value state(std::pmr::memory_resource *mr = nullptr) const;
    /// `rng(s)` — restore a state struct captured by state().
    void restore(const Value &s);

    // ── Legacy v4 generator (rand/randn('seed', S)) ───────────────────
    // MATLAB's pre-v5 generator, bit-identical (derived + verified vs
    // R2025b, see bugs/closed/stats/randn-legacy-seed-syntax.md):
    //   x <- 16807*x mod (2^31-1);  u = x/(2^31-1)
    //   seed S -> state S*2^16 (S==0 -> the constant 1144108930)
    //   randn: Marsaglia polar on that stream, FIRST of each accepted
    //   pair emitted (the second is discarded — probed; MATLAB's v4
    //   randn wastes half the pairs). ONE shared stream for rand+randn.
    void setLegacyV4(std::uint64_t seed)
    {
        legacyV4_ = true;
        v4state_ = seed ? seed * 65536ull : 1144108930ull;
    }
    bool legacyV4() const noexcept { return legacyV4_; }
    double v4Uniform()
    {
        v4state_ = 16807ull * v4state_ % 2147483647ull;
        return static_cast<double>(v4state_) / 2147483647.0;
    }
    double v4Normal()
    {
        for (;;) {
            const double v1 = 2.0 * v4Uniform() - 1.0;
            const double v2 = 2.0 * v4Uniform() - 1.0;
            const double s = v1 * v1 + v2 * v2;
            if (s > 0.0 && s < 1.0)
                return v1 * std::sqrt(-2.0 * std::log(s) / s);
        }
    }

    /// Escape hatch: the underlying MT19937 (e.g. for getState/setState).
    MatlabMT19937 &engine() noexcept { return gen_; }
    const MatlabMT19937 &engine() const noexcept { return gen_; }

private:
    MatlabMT19937 gen_;   // default-constructed = rng('default')
    bool legacyV4_ = false;
    std::uint64_t v4state_ = 1;
};

} // namespace numkit::ops
