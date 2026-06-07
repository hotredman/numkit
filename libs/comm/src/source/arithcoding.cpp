// libs/comm/src/source/arithcoding.cpp
//
// Arithmetic coding encoder + decoder.
//
//   code = arithenco(seq, counts)
//   seq  = arithdeco(code, counts, len)
//
// Algorithm: classic adaptive-precision arithmetic coding from
// Sayood, "Introduction to Data Compression" (Morgan Kaufmann),
// the same reference MATLAB R2025b's arithenco.m / arithdeco.m cite.
//
// Word length N = ceil(log2(sum(counts))) + 2. The lower / upper
// bounds are kept in N+1-bit unsigned integers; E1/E2/E3 rescaling
// emits / shifts bits as the bounds tighten.

#include <numkit/comm/source/arithcoding.hpp>

#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace numkit::comm {

namespace {

bool isRow(const Value &v)
{
    return v.dims().rows() == 1 && v.dims().cols() >= 1;
}

void readVecU(const Value &v, std::vector<uint64_t> &out)
{
    const size_t n = v.numel();
    out.resize(n);
    for (size_t i = 0; i < n; ++i) {
        const double d = v.elemAsDouble(i);
        if (!(d > 0.0) || std::floor(d) != d)
            throw Error("arithenco: counts must be positive integers",
                        0, 0, "arithenco", "", "numkit:arithenco:Counts");
        out[i] = static_cast<uint64_t>(d);
    }
}

// Bit at position p (1-based from LSB), matching MATLAB's bitget(x, p).
inline uint64_t bitget(uint64_t x, int p)
{
    return (x >> (p - 1)) & 1ULL;
}

// Mask x to N low bits (clear bits N+1 and above).
inline uint64_t maskN(uint64_t x, int N)
{
    return x & ((N == 64) ? ~0ULL : ((1ULL << N) - 1ULL));
}

} // namespace

Value arithenco(const Value &seq, const Value &counts,
                std::pmr::memory_resource *mr)
{
    std::vector<uint64_t> cnt;
    readVecU(counts, cnt);
    const size_t K = cnt.size();
    if (K == 0)
        throw Error("arithenco: counts must be non-empty",
                    0, 0, "arithenco", "", "numkit:arithenco:Counts");

    // Cumulative counts: cum[0]=0, cum[i]=sum(cnt[0..i-1])
    std::vector<uint64_t> cum(K + 1, 0);
    for (size_t i = 0; i < K; ++i) cum[i + 1] = cum[i] + cnt[i];
    const uint64_t total = cum[K];

    const int N = static_cast<int>(std::ceil(std::log2(static_cast<double>(total)))) + 2;
    const uint64_t topMask = 1ULL << (N - 1);

    uint64_t dec_low = 0;
    uint64_t dec_up  = (1ULL << N) - 1;
    uint64_t E3_count = 0;

    std::vector<uint8_t> bits;
    bits.reserve(seq.numel() * (static_cast<size_t>(std::ceil(std::log2(K)) + 2)) + N);

    const size_t Nseq = seq.numel();
    for (size_t k = 0; k < Nseq; ++k) {
        const double sd = seq.elemAsDouble(k);
        if (!(sd > 0.0) || std::floor(sd) != sd
            || static_cast<size_t>(sd) > K)
            throw Error("arithenco: symbol out of range [1, length(counts)]",
                        0, 0, "arithenco", "", "numkit:arithenco:Sym");
        const size_t s = static_cast<size_t>(sd);

        const uint64_t span = dec_up - dec_low + 1;
        const uint64_t low_new = dec_low + (span * cum[s - 1]) / total;
        dec_up  = dec_low + (span * cum[s]) / total - 1;
        dec_low = low_new;

        // E1/E2/E3 rescaling.
        for (;;) {
            if (bitget(dec_low, N) == bitget(dec_up, N)) {
                const uint64_t b = bitget(dec_low, N);
                bits.push_back(static_cast<uint8_t>(b));
                dec_low = (dec_low << 1) + 0;
                dec_up  = (dec_up  << 1) + 1;
                // Flush queued E3 bits (complement of b).
                if (E3_count > 0) {
                    for (uint64_t i = 0; i < E3_count; ++i)
                        bits.push_back(static_cast<uint8_t>(b ^ 1ULL));
                    E3_count = 0;
                }
                dec_low = maskN(dec_low, N);
                dec_up  = maskN(dec_up,  N);
            } else if (bitget(dec_low, N - 1) == 1
                    && bitget(dec_up,  N - 1) == 0) {
                dec_low = (dec_low << 1) + 0;
                dec_up  = (dec_up  << 1) + 1;
                dec_low = maskN(dec_low, N);
                dec_up  = maskN(dec_up,  N);
                dec_low ^= topMask;
                dec_up  ^= topMask;
                ++E3_count;
            } else {
                break;
            }
        }
    }

    // Termination: emit final bits of dec_low (with E3 handling).
    // bin_low MSB-first = N bits of dec_low.
    std::vector<uint8_t> bin_low(N);
    for (int i = 0; i < N; ++i)
        bin_low[i] = static_cast<uint8_t>((dec_low >> (N - 1 - i)) & 1ULL);
    if (E3_count == 0) {
        for (int i = 0; i < N; ++i) bits.push_back(bin_low[i]);
    } else {
        const uint8_t b = bin_low[0];
        bits.push_back(b);
        for (uint64_t i = 0; i < E3_count; ++i)
            bits.push_back(static_cast<uint8_t>(b ^ 1));
        for (int i = 1; i < N; ++i) bits.push_back(bin_low[i]);
    }

    // Output preserves seq orientation.
    const bool row = isRow(seq);
    const size_t L = bits.size();
    Value out = Value::matrix(row ? 1 : L, row ? L : 1,
                              ValueType::DOUBLE, mr);
    double *o = out.doubleDataMut();
    for (size_t i = 0; i < L; ++i) o[i] = static_cast<double>(bits[i]);
    return out;
}

Value arithdeco(const Value &code, const Value &counts, size_t len,
                std::pmr::memory_resource *mr)
{
    std::vector<uint64_t> cnt;
    readVecU(counts, cnt);
    const size_t K = cnt.size();
    std::vector<uint64_t> cum(K + 1, 0);
    for (size_t i = 0; i < K; ++i) cum[i + 1] = cum[i] + cnt[i];
    const uint64_t total = cum[K];

    const int N = static_cast<int>(std::ceil(std::log2(static_cast<double>(total)))) + 2;
    const uint64_t topMask = 1ULL << (N - 1);

    // Read code bits.
    const size_t Lcode = code.numel();
    auto readBit = [&](size_t i) -> uint64_t {
        const double d = code.elemAsDouble(i);
        if (d != 0.0 && d != 1.0)
            throw Error("arithdeco: code bits must be 0 or 1",
                        0, 0, "arithdeco", "", "numkit:arithdeco:Bit");
        return static_cast<uint64_t>(d);
    };
    if (Lcode < static_cast<size_t>(N))
        throw Error("arithdeco: code shorter than required word length",
                    0, 0, "arithdeco", "", "numkit:arithdeco:Short");

    uint64_t dec_low = 0;
    uint64_t dec_up  = (1ULL << N) - 1;
    uint64_t dec_tag = 0;
    for (int i = 0; i < N; ++i)
        dec_tag = (dec_tag << 1) | readBit(static_cast<size_t>(i));

    // Find bin index `ptr` (1..K) such that cum[ptr-1] <= value < cum[ptr].
    auto pick = [&](uint64_t value) -> size_t {
        if (value >= cum[K]) return K;
        // Linear scan from end (matches MATLAB's order; K is small).
        for (size_t i = K; i >= 1; --i) {
            if (cum[i - 1] <= value && value < cum[i]) return i;
        }
        return 1;
    };

    std::vector<double> dseq(len);
    size_t k = static_cast<size_t>(N);   // next code-bit index (0-based)
    for (size_t i = 0; i < len; ++i) {
        const uint64_t span = dec_up - dec_low + 1;
        const uint64_t dec_tag_new = ((dec_tag - dec_low + 1) * total - 1) / span;
        const size_t ptr = pick(dec_tag_new);
        dseq[i] = static_cast<double>(ptr);

        const uint64_t low_new = dec_low + (span * cum[ptr - 1]) / total;
        dec_up  = dec_low + (span * cum[ptr]) / total - 1;
        dec_low = low_new;

        for (;;) {
            const bool e1e2 = (bitget(dec_low, N) == bitget(dec_up, N));
            const bool e3   = (bitget(dec_low, N - 1) == 1
                            && bitget(dec_up,  N - 1) == 0);
            if (!e1e2 && !e3) break;
            if (k >= Lcode) break;
            const uint64_t cb = readBit(k);
            ++k;
            if (e1e2) {
                dec_low = (dec_low << 1);
                dec_up  = (dec_up  << 1) + 1;
                dec_tag = (dec_tag << 1) + cb;
                dec_low = maskN(dec_low, N);
                dec_up  = maskN(dec_up,  N);
                dec_tag = maskN(dec_tag, N);
            } else { // e3
                dec_low = (dec_low << 1);
                dec_up  = (dec_up  << 1) + 1;
                dec_tag = (dec_tag << 1) + cb;
                dec_low = maskN(dec_low, N);
                dec_up  = maskN(dec_up,  N);
                dec_tag = maskN(dec_tag, N);
                dec_low ^= topMask;
                dec_up  ^= topMask;
                dec_tag ^= topMask;
            }
        }
    }

    // Output preserves code orientation.
    const bool row = isRow(code);
    Value out = Value::matrix(row ? 1 : len, row ? len : 1,
                              ValueType::DOUBLE, mr);
    if (len > 0) std::copy(dseq.begin(), dseq.end(), out.doubleDataMut());
    return out;
}

} // namespace numkit::comm
