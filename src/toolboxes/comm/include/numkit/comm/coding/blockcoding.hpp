/// @file blockcoding.hpp
/// @ingroup group_comm
// toolboxes/comm/include/numkit/comm/coding/blockcoding.hpp
//
// Block linear coding — Error Correction Codes section of the
// Communications Toolbox. Generator / parity-check matrix construction
// (hammgen, gen2par, cyclgen) and cyclic generator polynomials
// (cyclpoly), plus the block encoder / decoder (encode / decode).
//
// Everything here is GF(2) (binary) arithmetic on plain double matrices —
// no Galois-field object type is required. BCH / Reed-Solomon (which need
// a GF(2^m) `gf` array type) are a documented gap.

#pragma once

#include <memory_resource>
#include <string>
#include <numkit/value/value.hpp>

namespace numkit::comm {

/// @addtogroup group_comm
/// @{


/// @brief Parity-check matrix from a generator matrix, or vice versa
/// (`h = gen2par(g)`).
///
/// Converts between the two systematic descriptions of a linear block
/// code. The input `mat` is an `r×c` binary matrix with `r < c`:
///   - if its last `r` columns form `I_r` (i.e. `mat = [P | I_r]`),
///     the result is `[I_(c-r) | Pᵀ]`;
///   - if its first `r` columns form `I_r` (i.e. `mat = [I_r | P]`),
///     the result is `[Pᵀ | I_(c-r)]`.
/// The map is an involution: `gen2par(gen2par(mat)) == mat`. Applying it
/// to a generator matrix yields the parity-check matrix and the reverse.
///
/// @param mat  Binary generator or parity-check matrix (`r×c`, `r < c`).
/// @param mr   Memory resource (nullptr → process default).
/// @return     The complementary `(c-r)×c` binary matrix.
/// @throws Error if `r >= c` or neither the first nor the last `r` columns
///         of `mat` form an identity matrix.
Value gen2par(const Value &mat, std::pmr::memory_resource *mr = nullptr);

/// @brief Result of @ref hammgen: parity-check / generator matrices and
/// the code dimensions.
struct HammgenResult {
    Value     h;  ///< Parity-check matrix, `m × n`.
    Value     g;  ///< Generator matrix, `k × n`.
    long long n;  ///< Codeword length `2^m - 1`.
    long long k;  ///< Message length `n - m`.
};

/// @brief Parity-check and generator matrices for a Hamming code
/// (`[h, g, n, k] = hammgen(m)`).
///
/// Builds the `(n, k)` Hamming code with `n = 2^m - 1`, `k = n - m`. The
/// parity-check matrix `h` is `m × n`; its `i`-th column (1-based `i`,
/// `i = 0…n-1`) holds the GF(2) coefficients of `x^i mod p(x)`, ascending
/// in the power of `x`, where `p` is the degree-`m` primitive polynomial.
/// The first `m` columns therefore form `I_m`, so the code is systematic
/// and `g = gen2par(h)`.
///
/// @param m         Number of parity bits (`>= 2`).
/// @param primPoly  Optional primitive polynomial as an ascending binary
///                  coefficient row of length `m+1` (empty → the default
///                  primitive polynomial for degree `m`).
/// @param mr        Memory resource (nullptr → process default).
/// @return          @ref HammgenResult (h, g, n, k).
/// @throws Error if `m < 2`, or `primPoly` (when supplied) is not a binary
///         row of length `m+1`.
HammgenResult hammgen(long long m, const Value &primPoly = Value::Empty,
                      std::pmr::memory_resource *mr = nullptr);

/// @brief Generator polynomial(s) for an `(n, k)` cyclic code
/// (`p = cyclpoly(n, k)` / `cyclpoly(n, k, opt)`).
///
/// Searches degree-`(n-k)` binary polynomials of the form `1 + … + x^(n-k)`
/// (constant and leading terms set) for those that divide `x^n - 1` over
/// GF(2) — the defining property of a cyclic generator polynomial. The
/// `opt` flag selects which result(s) to return:
///   - `""`     (default) — the first generator polynomial found;
///   - `"min"`  — the generator polynomial with the fewest terms;
///   - `"max"`  — the generator polynomial with the most terms;
///   - `"all"`  — every generator polynomial, one per row.
/// Each polynomial is a binary row in ascending powers of `x`.
///
/// @param n    Codeword length (`>= 1`).
/// @param k    Message length (`1 <= k <= n`).
/// @param opt  Selection flag (`""`, `"min"`, `"max"`, `"all"`).
/// @param mr   Memory resource (nullptr → process default).
/// @return     Generator polynomial row (or rows for `"all"`); empty if
///             no valid generator polynomial exists.
/// @throws Error on non-integer / out-of-range `n` or `k`, or an unknown
///         `opt`.
Value cyclpoly(long long n, long long k, const std::string &opt = "",
               std::pmr::memory_resource *mr = nullptr);

/// @brief Result of @ref cyclgen: parity-check / generator matrices and
/// the message length.
struct CyclgenResult {
    Value     h;  ///< Parity-check matrix, `(n-k) × n`.
    Value     g;  ///< Generator matrix, `k × n`.
    long long k;  ///< Message length `n - (deg genpoly)`.
};

/// @brief Parity-check and generator matrices for a cyclic code
/// (`[h, g, k] = cyclgen(n, genpoly)` / `cyclgen(n, genpoly, opt)`).
///
/// Builds the `(n, k)` cyclic code defined by generator polynomial
/// `genpoly` (a binary row in ascending powers; `k = n - deg(genpoly)`).
/// `genpoly` must divide `x^n - 1` over GF(2).
///   - `opt = "system"` (default) — systematic matrices `g = [I_k | P]`,
///     `h = [Pᵀ | I_(n-k)]` formed from the parity remainders
///     `x^(n-k+i) mod genpoly`.
///   - `opt = "nonsystem"` (or `"no…"`) — the classic cyclic-shift
///     matrices: rows are successive single-position rotations of the
///     reversed parity / generator polynomials.
///
/// @param n        Codeword length.
/// @param genpoly  Generator polynomial (ascending binary row).
/// @param opt      `"system"` (default) or `"nonsystem"`.
/// @param mr       Memory resource (nullptr → process default).
/// @return         @ref CyclgenResult (h, g, k).
/// @throws Error if `genpoly` does not divide `x^n - 1`, or `opt` is
///         unknown.
CyclgenResult cyclgen(long long n, const Value &genpoly,
                      const std::string &opt = "system",
                      std::pmr::memory_resource *mr = nullptr);

/// @brief Result of @ref encode: the codeword stream and any zero-padding
/// added.
struct EncodeResult {
    Value     code;   ///< Encoded codewords (same orientation as `msg`).
    long long added;  ///< Number of zero message bits padded before coding.
};

/// @brief Encode a message with a linear block code
/// (`code = encode(msg, n, k, method, opt)`).
///
/// Reshapes the message into `k`-bit words and multiplies (mod 2) by the
/// code's generator matrix. Supported `method` strings:
///   - `"hamming/binary"` (default) — Hamming `(n, k)`; `opt` may give an
///     alternative primitive polynomial.
///   - `"linear/binary"` — generic linear code; `opt` is the `k × n`
///     generator matrix (required).
///   - `"cyclic/binary"` — cyclic code; `opt` is the generator polynomial
///     (defaults to `cyclpoly(n, k)`).
/// A `…/decimal` suffix takes / returns one integer per word instead of a
/// bit stream. BCH / RS methods (which need a `gf` type) are a gap.
///
/// @param msg     Message bits (or integers for the `decimal` format).
/// @param n       Codeword length.
/// @param k       Message length.
/// @param method  Coding method (see above; default `"hamming/binary"`).
/// @param opt     Method option (primitive poly / generator matrix /
///                generator polynomial), or empty.
/// @param mr      Memory resource (nullptr → process default).
/// @return        @ref EncodeResult (code, added).
/// @throws Error on a bad `n`/`k`, an unknown method, or a missing /
///         malformed `opt` where the method requires one.
/// @see decode, hammgen, cyclgen
EncodeResult encode(const Value &msg, long long n, long long k,
                    const std::string &method = "hamming/binary",
                    const Value &opt = Value::Empty,
                    std::pmr::memory_resource *mr = nullptr);

/// @brief Result of @ref decode: the recovered message and error report.
struct DecodeResult {
    Value msg;   ///< Decoded message words (same orientation as `code`).
    Value err;   ///< Per-word number of errors corrected (`-1` if a word
                 ///< could not be decoded), as a column/row to match `msg`.
};

/// @brief Decode a linear block code
/// (`[msg, err] = decode(code, n, k, method, opt)`).
///
/// Syndrome decoder: reshapes `code` into `n`-bit words, computes each
/// syndrome against the parity-check matrix, looks up the minimum-weight
/// coset leader, corrects, and extracts the `k` message bits. Methods and
/// `opt` mirror @ref encode (`"hamming/binary"` default, `"linear/binary"`
/// with a generator matrix, `"cyclic/binary"` with a generator
/// polynomial). The Hamming method corrects any single-bit error directly
/// from the syndrome.
///
/// @param code    Received codeword bits (or integers for `decimal`).
/// @param n       Codeword length.
/// @param k       Message length.
/// @param method  Coding method (default `"hamming/binary"`).
/// @param opt     Method option (as in @ref encode), or empty.
/// @param mr      Memory resource (nullptr → process default).
/// @return        @ref DecodeResult (msg, err).
/// @throws Error on a bad `n`/`k`, an unknown method, or a missing /
///         malformed `opt`.
/// @see encode, hammgen, cyclgen
DecodeResult decode(const Value &code, long long n, long long k,
                    const std::string &method = "hamming/binary",
                    const Value &opt = Value::Empty,
                    std::pmr::memory_resource *mr = nullptr);

/// @brief Syndrome decoding table (coset-leader lookup) from a parity-check
/// matrix (`t = syndtable(H)`).
///
/// For an `(n-k) × n` parity-check matrix `H`, returns the `2^(n-k) × n`
/// table whose row `s+1` holds the **minimum-weight** error pattern (coset
/// leader) with syndrome `s = bi2de(mod(H·eᵀ, 2), 'left-msb')` (row 1 of
/// `H` is the MSB). Error patterns are enumerated by ascending Hamming
/// weight, and within a weight by lexicographic bit position; the first
/// pattern reaching each syndrome wins (so among equal-weight leaders the
/// lowest-position one is chosen).
///
/// @param H   Parity-check matrix, `(n-k) × n`, binary.
/// @param mr  Memory resource (nullptr → process default).
/// @return    `2^(n-k) × n` coset-leader table.
/// @throws    Error if `H` is empty or `n-k` is impractically large.
/// @see hammgen, gen2par, decode
Value syndtable(const Value &H, std::pmr::memory_resource *mr = nullptr);


/// @}
} // namespace numkit::comm
