// libs/comm/include/numkit/comm/source/huffman.hpp
//
// Huffman entropy coding (huffmandict; enco/deco planned).

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>
#include <utility>

namespace numkit::comm {

/// `[dict, avglen] = huffmandict(symbols, probs)` — build a
/// canonical Huffman dictionary.
///
///   symbols : length-K vector of symbol values (any doubles).
///   probs   : length-K vector of probabilities, summing to 1.
///
/// Returns the dict as a K-by-2 cell:
///   dict{k, 1} = symbol value (scalar)
///   dict{k, 2} = bit code (1×L row vector of 0/1 doubles)
/// and the average code length avglen = Σ p_i · length(code_i).
std::pair<Value, double>
huffmandict(std::pmr::memory_resource *mr,
            const Value &symbols, const Value &probs);

/// `enc = huffmanenco(sig, dict)` — encode `sig` (vector of symbols
/// drawn from `dict{:,1}`) into a flat 0/1 bit vector by
/// concatenating the per-symbol codes from `dict`. Output preserves
/// `sig`'s row/column orientation.
Value huffmanenco(std::pmr::memory_resource *mr,
                  const Value &sig, const Value &dict);

/// `dec = huffmandeco(bits, dict)` — invert huffmanenco by walking
/// `bits` through a prefix tree built from `dict`. Returns the
/// recovered symbol vector with `bits`' orientation. Throws on
/// invalid bit values, mid-stream patterns not in `dict`, or
/// trailing bits that do not complete a code.
Value huffmandeco(std::pmr::memory_resource *mr,
                  const Value &bits, const Value &dict);

} // namespace numkit::comm
