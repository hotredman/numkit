// libs/builtin/src/math/random/matlab_mt19937.cpp
//
// MATLAB-canonical MT19937 RNG. See header for context.

#include <numkit/builtin/math/random/matlab_mt19937.hpp>

namespace numkit::builtin::detail {

// ── Single-uint32 seeding (Matsumoto-Nishimura init_genrand) ──────────
//
// state[0] = seed
// for i in [1, N): state[i] = 1812433253 * (state[i-1] xor (state[i-1] >> 30)) + i
//
// (This is the "Knuth" line; LCG warm-up.)
void MatlabMT19937::initGenrand(uint32_t s)
{
    mt_[0] = s;
    for (std::size_t i = 1; i < N; ++i) {
        mt_[i] = (1812433253UL * (mt_[i - 1] ^ (mt_[i - 1] >> 30)) + static_cast<uint32_t>(i));
    }
    mti_ = static_cast<int>(N);   // force generateNextBatch on first draw
}

// ── init_by_array seeding (canonical for MATLAB's rng(seed)) ──────────
//
// MATLAB's rng(seed) for non-negative integer seed seeds via
// init_by_array on a length-1 array {seed}. This is materially
// different from initGenrand(seed): it does an additional mixing pass
// driven by the key array.
//
// Reference (Matsumoto-Nishimura mt19937ar.c):
//   init_genrand(19650218);
//   i=1; j=0; k = (N>key_length ? N : key_length);
//   for (; k; k--) {
//       mt[i] = (mt[i] ^ ((mt[i-1] ^ (mt[i-1] >> 30)) * 1664525UL))
//             + key[j] + j;
//       i++; j++;
//       if (i>=N) { mt[0] = mt[N-1]; i=1; }
//       if (j>=key_length) j=0;
//   }
//   for (k=N-1; k; k--) {
//       mt[i] = (mt[i] ^ ((mt[i-1] ^ (mt[i-1] >> 30)) * 1566083941UL))
//             - i;
//       i++;
//       if (i>=N) { mt[0] = mt[N-1]; i=1; }
//   }
//   mt[0] = 0x80000000UL;   // MSB is 1; assuring non-zero initial array
void MatlabMT19937::initByArray_single(uint32_t key)
{
    initGenrand(19650218UL);

    // Length-1 key array: only key[0] = `key`, key[j] = `key` for any j (j always 0).
    constexpr std::size_t key_length = 1;
    std::size_t i = 1, j = 0;
    std::size_t k = (N > key_length ? N : key_length);
    for (; k; --k) {
        mt_[i] = (mt_[i] ^ ((mt_[i - 1] ^ (mt_[i - 1] >> 30)) * 1664525UL))
               + key + static_cast<uint32_t>(j);
        ++i;
        ++j;
        if (i >= N) { mt_[0] = mt_[N - 1]; i = 1; }
        if (j >= key_length) j = 0;
    }
    for (k = N - 1; k; --k) {
        mt_[i] = (mt_[i] ^ ((mt_[i - 1] ^ (mt_[i - 1] >> 30)) * 1566083941UL))
               - static_cast<uint32_t>(i);
        ++i;
        if (i >= N) { mt_[0] = mt_[N - 1]; i = 1; }
    }
    mt_[0] = 0x80000000UL;
    mti_ = static_cast<int>(N);
}

// ── Generate next batch of N values (MT twist) ────────────────────────
void MatlabMT19937::generateNextBatch()
{
    static constexpr uint32_t mag01[2] = { 0x0UL, MATRIX_A };
    uint32_t y;
    std::size_t kk;
    for (kk = 0; kk < N - M; ++kk) {
        y = (mt_[kk] & UPPER_MASK) | (mt_[kk + 1] & LOWER_MASK);
        mt_[kk] = mt_[kk + M] ^ (y >> 1) ^ mag01[y & 0x1UL];
    }
    for (; kk < N - 1; ++kk) {
        y = (mt_[kk] & UPPER_MASK) | (mt_[kk + 1] & LOWER_MASK);
        mt_[kk] = mt_[kk + (M - N)] ^ (y >> 1) ^ mag01[y & 0x1UL];
    }
    y = (mt_[N - 1] & UPPER_MASK) | (mt_[0] & LOWER_MASK);
    mt_[N - 1] = mt_[M - 1] ^ (y >> 1) ^ mag01[y & 0x1UL];
    mti_ = 0;
}

uint32_t MatlabMT19937::genUint32()
{
    if (mti_ >= static_cast<int>(N))
        generateNextBatch();

    uint32_t y = mt_[mti_++];
    // Tempering.
    y ^= (y >> 11);
    y ^= (y << 7)  & 0x9d2c5680UL;
    y ^= (y << 15) & 0xefc60000UL;
    y ^= (y >> 18);
    return y;
}

// 53-bit precision uniform double in [0, 1). Two uint32 draws combined:
//   a = first  >> 5  (27 bits)
//   b = second >> 6  (26 bits)
//   res = (a * 2^26 + b) / 2^53
//
// MATLAB's rand() under 'twister' emits exactly this.
double MatlabMT19937::genRes53()
{
    const uint32_t a = genUint32() >> 5;
    const uint32_t b = genUint32() >> 6;
    return (static_cast<double>(a) * 67108864.0 + static_cast<double>(b))
         * (1.0 / 9007199254740992.0);
}

void MatlabMT19937::getState(uint32_t out[STATE_SIZE], int &outIndex) const
{
    for (std::size_t i = 0; i < N; ++i) out[i] = mt_[i];
    outIndex = mti_;
}

void MatlabMT19937::setState(const uint32_t in[STATE_SIZE], int inIndex)
{
    for (std::size_t i = 0; i < N; ++i) mt_[i] = in[i];
    mti_ = inIndex;
}

} // namespace numkit::builtin::detail
