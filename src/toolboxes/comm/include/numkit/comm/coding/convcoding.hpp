/// @file convcoding.hpp
/// @ingroup group_comm
// toolboxes/comm/include/numkit/comm/coding/convcoding.hpp
//
// Convolutional coding — Error Correction Codes section of the
// Communications Toolbox: poly2trellis (and, as they land, istrellis /
// convenc / vitdec).

#pragma once

#include <memory_resource>
#include <numkit/value/value.hpp>

namespace numkit::comm {

/// @addtogroup group_comm
/// @{


/// @brief Convolutional-code trellis from polynomial generators
/// (`trellis = poly2trellis(ConstraintLength, CodeGenerator)`).
///
/// Builds the MATLAB trellis struct describing a rate-1/n feed-forward
/// convolutional encoder. Returns a 1×1 struct with fields (in MATLAB's
/// order):
///   - `numInputSymbols`  = `2`            (one input bit, k = 1)
///   - `numOutputSymbols` = `2^n`          (n = numel(CodeGenerator))
///   - `numStates`        = `2^(K-1)`      (K = ConstraintLength)
///   - `nextStates`       = `numStates × 2` next-state index per (state, input)
///   - `outputs`          = `numStates × 2` output word (decimal of the
///                          n output bits, first generator = MSB)
///
/// `CodeGenerator` entries are octal numbers (e.g. `[6 7]`, `[171 133]`).
/// The shift-register state holds the `K-1` previous input bits; for each
/// `(state, input)` the K-bit register `reg = (input << (K-1)) | state`
/// drives `output_i = parity(reg & g_i)` and `nextState = reg >> 1`.
///
/// v1 scope: rate 1/n (scalar `ConstraintLength`, length-n `CodeGenerator`),
/// feed-forward only. Rate k/n (vector `ConstraintLength`, k×n generator
/// matrix) and feedback connections are a documented gap.
///
/// @param constraintLength  Scalar constraint length `K` (`>= 1`).
/// @param codeGenerator      Length-n vector of octal generator polynomials.
/// @param mr                 Memory resource (nullptr → process default).
/// @return                   1×1 trellis struct.
/// @throws Error on a non-scalar `constraintLength` (rate k/n), `K < 1`, an
///         empty `codeGenerator`, or an octal digit `> 7`.
Value poly2trellis(const Value &constraintLength, const Value &codeGenerator,
                   std::pmr::memory_resource *mr = nullptr);

/// @brief Convolutionally encode a bit stream (`code = convenc(msg, trellis)`).
///
/// Runs the rate-1/n encoder described by `trellis` (from @ref poly2trellis)
/// over the input bits `msg`, starting from the all-zeros state. Each input
/// bit emits `n = log2(numOutputSymbols)` output bits (the trellis output
/// word, MSB first); the encoder then advances to `nextStates(state, bit)`.
/// The output has `n * numel(msg)` bits and takes `msg`'s orientation
/// (row → row, column → column). No tail-biting / termination flush
/// (truncated mode) and no puncturing in v1.
///
/// @param msg      Input bit vector (0/1).
/// @param trellis  Trellis struct from poly2trellis (k = 1 / rate 1/n).
/// @param mr       Memory resource (nullptr → process default).
/// @return         Encoded bit vector (length `n * numel(msg)`).
/// @throws Error if `trellis` is not a valid 1×1 trellis struct.
/// @see poly2trellis, vitdec
Value convenc(const Value &msg, const Value &trellis,
              std::pmr::memory_resource *mr = nullptr);

/// @brief Viterbi-decode a convolutional code
/// (`msg = vitdec(code, trellis, tblen, opmode, dectype)`).
///
/// Hard-decision maximum-likelihood (Viterbi) decoder for the rate-1/n
/// `trellis`. Runs the add-compare-select forward recursion over the
/// `numel(code)/n` received symbols (branch metric = Hamming distance to
/// each trellis output word) and traces the survivor path back to recover
/// the message bits.
///
/// `opmode`:
///   - `"trunc"` (default) — start at state 0, trace back from the
///     minimum-metric final state.
///   - `"term"` — the encoder was terminated, so trace back from state 0.
/// `dectype` must be `"hard"` (soft / unquantised decisions are a v1 gap).
/// `tblen` (traceback depth) is accepted for MATLAB compatibility; this v1
/// always performs a full traceback (`"cont"` continuous mode is deferred).
///
/// @param code     Received bit vector (length a multiple of n).
/// @param trellis  Trellis struct from poly2trellis.
/// @param tblen    Traceback depth (advisory in trunc/term).
/// @param opmode   `"trunc"` (default) or `"term"`.
/// @param dectype  `"hard"` (default; only hard-decision supported).
/// @param mr       Memory resource (nullptr → process default).
/// @return         Decoded message bits (numel(code)/n).
/// @throws Error on an invalid trellis, `numel(code)` not a multiple of n,
///         or an unsupported opmode/dectype.
/// @see convenc, poly2trellis
Value vitdec(const Value &code, const Value &trellis, long long tblen,
             const std::string &opmode = "trunc",
             const std::string &dectype = "hard",
             std::pmr::memory_resource *mr = nullptr);

/// @brief Validate a trellis structure (`tf = istrellis(S)`).
///
/// Returns logical `true` iff `S` is a well-formed trellis struct: a 1×1
/// struct carrying `numInputSymbols`, `numOutputSymbols`, `numStates`
/// (positive powers of two), and `nextStates` / `outputs` matrices of size
/// `numStates × numInputSymbols` whose entries lie in `[0, numStates)` and
/// `[0, numOutputSymbols)` respectively. Any other value → `false` (never
/// throws). MATLAB's 2nd `msg` output is a v1 gap.
///
/// @param S   Value to test.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Logical scalar.
/// @see poly2trellis
Value istrellis(const Value &S, std::pmr::memory_resource *mr = nullptr);


/// @}
} // namespace numkit::comm
