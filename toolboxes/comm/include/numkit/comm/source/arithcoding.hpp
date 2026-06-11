// toolboxes/comm/include/numkit/comm/source/arithcoding.hpp
//
// Arithmetic coding encoder + decoder.

#pragma once

#include <cstddef>
#include <memory_resource>
#include <numkit/value/value.hpp>

namespace numkit::comm {

/// @brief Arithmetic-encode a symbol sequence
/// (`code = arithenco(seq, counts)`).
///
/// Uses the cumulative-probability intervals derived from `counts`.
/// Output is a 0/1 bit vector whose orientation matches `seq`.
///
/// @param seq     Symbol sequence (positive integer values in
///                `1..length(counts)`).
/// @param counts  Positive integer count vector (length K).
/// @param mr      Memory resource (nullptr → process default).
/// @return        0/1 bit vector encoding `seq`.
/// @throws Error  On bad `seq` symbols or non-positive counts.
/// @see arithdeco
Value arithenco(const Value &seq, const Value &counts,
                std::pmr::memory_resource *mr = nullptr);

/// @brief Invert @ref arithenco (`seq = arithdeco(code, counts, len)`).
///
/// The contract requires the decoded length `len` explicitly;
/// the encoder emits no stop sentinel.
///
/// @param code    0/1 bit vector produced by @ref arithenco.
/// @param counts  Same count vector used during encoding.
/// @param len     Expected decoded length.
/// @param mr      Memory resource (nullptr → process default).
/// @return        Decoded symbol sequence of length `len`.
/// @throws Error  On bad inputs or truncated bit stream.
/// @see arithenco
Value arithdeco(const Value &code, const Value &counts, std::size_t len,
                std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::comm
