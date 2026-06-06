// libs/comm/include/numkit/comm/eq/compand.hpp
//
// μ-law / A-law signal compander.

#pragma once

#include <memory_resource>
#include <numkit/value/value.hpp>
#include <string>

namespace numkit::comm {

/// @brief μ-law / A-law signal compander
/// (`y = compand(x, param, V, method)`).
///
/// Formulas:
/// - μ-law compress: `y = sign(x) · V · log(1 + μ|x|/V) / log(1 + μ)`
/// - μ-law expand:   `x = sign(y) · (V/μ) · (exp(|y|/V · log(1+μ)) - 1)`
/// - A-law compress, `|x|/V ≤ 1/A`: `y = sign(x) · A·|x| / (1 + log A)`
/// - A-law compress, else:          `y = sign(x) · V · (1 + log(A·|x|/V)) / (1 + log A)`
///
/// Output preserves input shape.
///
/// @param x       Input signal (any shape).
/// @param param   `μ` for μ-law (typical 255) or `A` for A-law
///                (typical 87.6).
/// @param V       Peak magnitude.
/// @param method  `"mu/compressor"`, `"mu/expander"`, `"A/compressor"`,
///                or `"A/expander"`.
/// @param mr      Memory resource (nullptr → process default).
/// @return        Companded signal of the same shape as `x`.
/// @throws Error  Unknown `method` string.
Value compand(const Value &x, double param, double V,
              const std::string &method,
              std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::comm
