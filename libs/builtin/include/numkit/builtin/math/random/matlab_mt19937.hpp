// libs/builtin/src/math/random/matlab_mt19937.hpp
//
// MATLAB-canonical MT19937 RNG.
//
// Reference: Matsumoto & Nishimura, "Mersenne Twister: A 623-Dimensionally
// Equidistributed Uniform Pseudo-Random Number Generator", 1998.
// http://www.math.sci.hiroshima-u.ac.jp/~m-mat/MT/MT2002/CODES/mt19937ar.c
//
// MATLAB's `rng(seed, 'twister')` (the default since R2011a) seeds via
// `init_by_array(seed_as_length_1_array)` and generates uniform doubles
// via `genrand_res53` (53-bit precision: combine two uint32 draws).
//
// Why not std::mt19937?
//   - std::mt19937 is the same algorithm but the seed-to-state mapping
//     differs from MATLAB's init_by_array (std uses a single-uint32
//     seed expansion that is implementation-defined between libstdc++
//     / libc++ / MSVC).
//   - std::uniform_real_distribution's uint32 -> double conversion is
//     implementation-defined too.
//
// This implementation IS bit-identical with MATLAB R2025b's rng() +
// rand() chain on probed seeds (0, 42, 12345).

#pragma once

#include <cstddef>
#include <cstdint>

namespace numkit::builtin::detail {

class MatlabMT19937 {
public:
    // ── UniformRandomBitGenerator concept ─────────────────────────
    // Lets std::distributions (uniform_int_distribution, etc.) drive
    // this engine directly without a wrapper. Note: std distributions
    // are still implementation-defined in their uint32 -> output map;
    // for MATLAB-bit-identical doubles, use genRes53() instead.
    using result_type = uint32_t;
    static constexpr result_type min() { return 0; }
    static constexpr result_type max() { return 0xFFFFFFFFUL; }
    result_type operator()() { return genUint32(); }

    // Default constructor seeds with MATLAB's rng('default').
    // MATLAB silently maps seed=0 to MT default seed=5489 (the
    // historic Matsumoto-Nishimura "magic" default), so the actual
    // state[0] is 5489 even though state.Seed reports 0.
    MatlabMT19937() { seed(0); }

    // Seed via single uint32. Bit-identical with MATLAB R2025b's
    // `rng(seed, 'twister')`:
    //   rng(0)    -> initGenrand(5489)  [MATLAB's seed=0 special case]
    //   rng(N>0)  -> initGenrand(N)     [direct]
    void seed(uint32_t s) {
        // MATLAB's quirk: seed 0 maps to MT default 5489.
        // (Verified empirically: rng(0); rand() == 0.8147236863931789
        //  which matches initGenrand(5489), not initGenrand(0).)
        initGenrand(s == 0 ? 5489u : s);
    }

    // Generate next uint32 from the MT stream.
    uint32_t genUint32();

    // Generate next 53-bit double in [0, 1) -- two uint32 draws combined.
    // This is what MATLAB's rand() returns under 'twister'.
    double genRes53();

    // State serialisation -- 624 uint32 state words + the index pointer.
    // Used by rng() to snapshot/restore.
    static constexpr std::size_t STATE_SIZE = 624;
    void getState(uint32_t out[STATE_SIZE], int &outIndex) const;
    void setState(const uint32_t in[STATE_SIZE], int inIndex);

private:
    // Reference algorithm constants.
    static constexpr std::size_t N = 624;
    static constexpr std::size_t M = 397;
    static constexpr uint32_t MATRIX_A   = 0x9908b0dfUL;
    static constexpr uint32_t UPPER_MASK = 0x80000000UL;
    static constexpr uint32_t LOWER_MASK = 0x7fffffffUL;

    uint32_t mt_[N];
    int mti_;  // index; mti_ == N means "regenerate the whole array"

    // Reference seeders.
    void initGenrand(uint32_t s);
    void initByArray_single(uint32_t key);   // kept for reference / future use

    void generateNextBatch();
};

} // namespace numkit::builtin::detail
