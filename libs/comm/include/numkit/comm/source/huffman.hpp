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

} // namespace numkit::comm
