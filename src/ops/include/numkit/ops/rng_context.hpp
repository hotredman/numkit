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
    void seed(std::uint64_t seed) { gen_.seed(static_cast<std::uint32_t>(seed)); }
    /// `rng('shuffle')` — seed from a nondeterministic source.
    void shuffle();
    /// `s = rng()` — capture the stream state as a struct (.Type / .State).
    Value state(std::pmr::memory_resource *mr = nullptr) const;
    /// `rng(s)` — restore a state struct captured by state().
    void restore(const Value &s);

    /// Escape hatch: the underlying MT19937 (e.g. for getState/setState).
    MatlabMT19937 &engine() noexcept { return gen_; }
    const MatlabMT19937 &engine() const noexcept { return gen_; }

private:
    MatlabMT19937 gen_;   // default-constructed = rng('default')
};

} // namespace numkit::ops
