// libs/comm/include/numkit/comm/source/huffman.hpp
//
// Huffman entropy coding (huffmandict + huffmanenco / huffmandeco).

#pragma once

#include <memory_resource>
#include <numkit/value/value.hpp>
#include <utility>

namespace numkit::comm {

/// @brief Build a canonical Huffman dictionary
/// (`[dict, avglen] = huffmandict(symbols, probs)`).
///
/// Returns the dictionary as a K-by-2 cell:
/// - `dict{k, 1}` = symbol value (scalar)
/// - `dict{k, 2}` = bit code (1×L row vector of 0/1 doubles)
///
/// along with the average code length
/// `avglen = Σ p_i · length(code_i)`.
///
/// @param symbols  Length-K vector of symbol values (any doubles).
/// @param probs    Length-K probability vector that must sum to 1.
/// @param mr       Memory resource (nullptr → process default).
/// @return         Pair `(dict, avglen)`.
/// @throws Error   On length mismatch or non-normalised `probs`.
/// @see huffmanenco, huffmandeco
std::pair<Value, double>
huffmandict(const Value &symbols, const Value &probs,
            std::pmr::memory_resource *mr = nullptr);

/// @brief Encode a symbol sequence using a Huffman dictionary
/// (`enc = huffmanenco(sig, dict)`).
///
/// Concatenates the per-symbol codes from `dict` into a flat 0/1 bit
/// vector. Output preserves `sig`'s row/column orientation.
///
/// @param sig    Symbol sequence (values drawn from `dict{:,1}`).
/// @param dict   Dictionary produced by @ref huffmandict.
/// @param mr     Memory resource (nullptr → process default).
/// @return       Flat 0/1 bit vector.
/// @throws Error If `sig` contains symbols not present in `dict`.
/// @see huffmandict, huffmandeco
Value huffmanenco(const Value &sig, const Value &dict,
                  std::pmr::memory_resource *mr = nullptr);

/// @brief Decode a Huffman bit stream
/// (`dec = huffmandeco(bits, dict)`).
///
/// Walks `bits` through a prefix tree built from `dict`. Returns the
/// recovered symbol vector with `bits`' orientation.
///
/// @param bits   0/1 bit vector to decode.
/// @param dict   Same dictionary used at encode time.
/// @param mr     Memory resource (nullptr → process default).
/// @return       Decoded symbol vector.
/// @throws Error On invalid bit values, mid-stream patterns not in
///               `dict`, or trailing bits that do not complete a code.
/// @see huffmandict, huffmanenco
Value huffmandeco(const Value &bits, const Value &dict,
                  std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::comm
