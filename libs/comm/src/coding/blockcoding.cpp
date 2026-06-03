// libs/comm/src/coding/blockcoding.cpp
//
// Block linear coding (Error Correction Codes): gen2par, hammgen,
// cyclpoly, cyclgen, encode, decode. All GF(2) (binary) arithmetic on
// plain double matrices.
//
// MATLAB R2025b semantics (verified via probe + toolbox-doc algorithm):
//   hammgen(m): H(:,i) = coeffs of x^i mod p(x), p = default primitive
//     polynomial of degree m, ascending power. First m columns = I_m, so
//     the code is systematic and g = gen2par(h).
//   gen2par(g): systematic generator<->parity converter (involution).
//   cyclpoly(n,k): first degree-(n-k) poly 1+...+x^(n-k) dividing x^n-1.
//   cyclgen(n,p): cyclic parity/generator matrices (system / nonsystem).
//   encode/decode: reshape into words, generator-multiply / syndrome-decode.

#include <numkit/comm/coding/blockcoding.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/scratch.hpp>
#include <numkit/core/types.hpp>
#include <numkit/core/value.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace numkit::comm {

namespace {

// ── GF(2) primitives ───────────────────────────────────────────────────

// Default primitive polynomial of degree m (m = 2..16) as an integer bit
// mask: bit j set ⇔ coefficient of x^j is 1. Matches MATLAB gfprimdf.
// Bit 0 (constant) and bit m (leading) are always set.
std::uint64_t defaultPrimMask(long long m, const char *who)
{
    switch (m) {
    case 2:  return 0x7ULL;        // 1+x+x^2
    case 3:  return 0xBULL;        // 1+x+x^3
    case 4:  return 0x13ULL;       // 1+x+x^4
    case 5:  return 0x25ULL;       // 1+x^2+x^5
    case 6:  return 0x43ULL;       // 1+x+x^6
    case 7:  return 0x89ULL;       // 1+x^3+x^7
    case 8:  return 0x11DULL;      // 1+x^2+x^3+x^4+x^8
    case 9:  return 0x211ULL;      // 1+x^4+x^9
    case 10: return 0x409ULL;      // 1+x^3+x^10
    case 11: return 0x805ULL;      // 1+x^2+x^11
    case 12: return 0x1053ULL;     // 1+x+x^4+x^6+x^12
    case 13: return 0x201BULL;     // 1+x+x^3+x^4+x^13
    case 14: return 0x4443ULL;     // 1+x+x^6+x^10+x^14
    case 15: return 0x8003ULL;     // 1+x+x^15
    case 16: return 0x1100BULL;    // 1+x+x^3+x^12+x^16
    default:
        throw Error(std::string(who) + ": M out of supported range [2,16]",
                    0, 0, who, "", std::string("numkit:") + who + ":range");
    }
}

// Read element (i,j) of a numeric Value (column-major) as 0/1.
inline int matBit(const Value &v, std::size_t i, std::size_t j)
{
    const std::size_t rows = v.dims().rows();
    return v.elemAsDouble(j * rows + i) != 0.0 ? 1 : 0;
}

// ── gen2par core (shared by gen2par + hammgen) ─────────────────────────

// Convert an r×c binary matrix (r < c) between [P|I_r]/[I_r|P] systematic
// form and its complement. Returns a fresh (c-r)×c DOUBLE Value.
Value gen2parImpl(const Value &mat, const char *who,
                  std::pmr::memory_resource *mr)
{
    const std::size_t r = mat.dims().rows();
    const std::size_t c = mat.dims().cols();
    if (r == 0 || c == 0 || r >= c)
        throw Error(std::string(who) + ": input must be r×c with r < c",
                    0, 0, who, "", std::string("numkit:") + who + ":shape");
    const std::size_t pcols = c - r;  // parity columns / output rows

    // Branch A: last r columns form I_r  ⇒ mat = [P | I_r]
    bool lastIsI = true;
    for (std::size_t i = 0; i < r && lastIsI; ++i)
        for (std::size_t t = 0; t < r; ++t)
            if (matBit(mat, i, pcols + t) != (i == t ? 1 : 0)) { lastIsI = false; break; }

    // Branch B: first r columns form I_r ⇒ mat = [I_r | P]
    bool firstIsI = true;
    for (std::size_t i = 0; i < r && firstIsI; ++i)
        for (std::size_t t = 0; t < r; ++t)
            if (matBit(mat, i, t) != (i == t ? 1 : 0)) { firstIsI = false; break; }

    Value out = Value::matrix(pcols, c, ValueType::DOUBLE, mr);
    double *od = out.doubleDataMut();
    auto put = [&](std::size_t i, std::size_t j, int b) { od[j * pcols + i] = b; };

    if (lastIsI) {
        // out = [I_(c-r) | Pᵀ], P = mat(:, 0..pcols-1)
        for (std::size_t i = 0; i < pcols; ++i) {
            for (std::size_t j = 0; j < pcols; ++j) put(i, j, i == j ? 1 : 0);
            for (std::size_t t = 0; t < r; ++t) put(i, pcols + t, matBit(mat, t, i));
        }
    } else if (firstIsI) {
        // out = [Pᵀ | I_(c-r)], P = mat(:, r..c-1)
        for (std::size_t i = 0; i < pcols; ++i) {
            for (std::size_t t = 0; t < r; ++t) put(i, t, matBit(mat, t, r + i));
            for (std::size_t j = 0; j < pcols; ++j) put(i, r + j, i == j ? 1 : 0);
        }
    } else {
        throw Error(std::string(who) + ": neither the first nor the last r "
                    "columns form an identity matrix (input not systematic)",
                    0, 0, who, "", std::string("numkit:") + who + ":notsystematic");
    }
    return out;
}

} // namespace

// ── gen2par ─────────────────────────────────────────────────────────────

Value gen2par(const Value &mat, std::pmr::memory_resource *mr)
{
    return gen2parImpl(mat, "gen2par", mr);
}

// ── hammgen ─────────────────────────────────────────────────────────────

HammgenResult hammgen(long long m, const Value &primPoly,
                      std::pmr::memory_resource *mr)
{
    if (m < 2)
        throw Error("hammgen: M must be an integer >= 2",
                    0, 0, "hammgen", "", "numkit:hammgen:M");

    // Primitive polynomial as an integer mask (bit j = coeff of x^j).
    std::uint64_t pmask;
    if (primPoly.isEmpty()) {
        pmask = defaultPrimMask(m, "hammgen");
    } else {
        const std::size_t len = primPoly.numel();
        if (static_cast<long long>(len) != m + 1)
            throw Error("hammgen: P must be a binary row of length M+1",
                        0, 0, "hammgen", "", "numkit:hammgen:P");
        pmask = 0;
        for (std::size_t j = 0; j < len; ++j)
            if (primPoly.elemAsDouble(j) != 0.0) pmask |= (1ULL << j);
        if ((pmask & 1ULL) == 0 || (pmask & (1ULL << m)) == 0)
            throw Error("hammgen: P must have nonzero constant and degree-M terms",
                        0, 0, "hammgen", "", "numkit:hammgen:P");
    }

    const long long n = (1LL << m) - 1;
    const long long k = n - m;

    // H(j,i) = bit j of (x^i mod p), i = 0..n-1, j = 0..m-1.
    Value h = Value::matrix(static_cast<std::size_t>(m),
                            static_cast<std::size_t>(n), ValueType::DOUBLE, mr);
    double *hd = h.doubleDataMut();
    const std::uint64_t topBit = 1ULL << m;
    std::uint64_t r = 1;  // x^0
    for (long long i = 0; i < n; ++i) {
        for (long long j = 0; j < m; ++j)
            hd[i * m + j] = (r >> j) & 1ULL;     // column-major, row j
        r <<= 1;                                  // multiply by x
        if (r & topBit) r ^= pmask;               // reduce mod p
    }

    HammgenResult res;
    res.h = std::move(h);
    res.g = gen2parImpl(res.h, "hammgen", mr);    // first m cols are I_m
    res.n = n;
    res.k = k;
    return res;
}

// ── registry adapters ───────────────────────────────────────────────────

namespace detail {

void gen2par_reg(Span<const Value> args, size_t /*nargout*/,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("gen2par: requires a generator or parity-check matrix",
                    0, 0, "gen2par", "", "numkit:gen2par:nargin");
    outs[0] = gen2par(args[0], ctx.engine->resource());
}

void hammgen_reg(Span<const Value> args, size_t nargout,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("hammgen: requires M (number of parity bits)",
                    0, 0, "hammgen", "", "numkit:hammgen:nargin");
    const long long m = static_cast<long long>(args[0].toScalar());
    const Value &prim = (args.size() >= 2) ? args[1] : Value::Empty;
    HammgenResult res = hammgen(m, prim, ctx.engine->resource());
    outs[0] = std::move(res.h);
    if (nargout >= 2) outs[1] = std::move(res.g);
    if (nargout >= 3) outs[2] = Value::scalar(static_cast<double>(res.n),
                                              ctx.engine->resource());
    if (nargout >= 4) outs[3] = Value::scalar(static_cast<double>(res.k),
                                              ctx.engine->resource());
}

} // namespace detail

} // namespace numkit::comm
