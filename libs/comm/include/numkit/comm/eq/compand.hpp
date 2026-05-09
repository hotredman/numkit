// libs/comm/include/numkit/comm/eq/compand.hpp
//
// μ-law / A-law signal compander.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>
#include <string>

namespace numkit::comm {

/// `y = compand(x, param, V, method)` — apply μ-law / A-law
/// compression or expansion to a signal.
///
///   x       : input signal (any shape)
///   param   : μ for μ-law (typical 255), A for A-law (typical 87.6)
///   V       : peak magnitude
///   method  : "mu/compressor" | "mu/expander" |
///             "A/compressor"  | "A/expander"
///
/// μ-law compress:  y = sign(x) · V · log(1 + μ|x|/V) / log(1 + μ)
/// μ-law expand:    x = sign(y) · (V/μ) · (exp(|y|/V · log(1+μ)) − 1)
///
/// A-law compress, |x|/V ≤ 1/A: y = sign(x) · A·|x| / (1 + log A)
///                       else : y = sign(x) · V · (1 + log(A·|x|/V)) / (1 + log A)
///
/// Output preserves input shape; bit-equal with MATLAB R2025b.
Value compand(std::pmr::memory_resource *mr, const Value &x,
              double param, double V, const std::string &method);

} // namespace numkit::comm
