/// @file qz.hpp
/// @ingroup group_matfun
// toolboxes/linalg/include/numkit/linalg/qz.hpp
//
// Generalized Schur decomposition: [AA, BB, Q, Z] = qz(A, B)

#pragma once

#include <memory_resource>
#include <tuple>
#include <numkit/value/value.hpp>

namespace numkit::linalg {

/// @brief Generalized Schur decomposition of matrix pair (A, B).
/// Returns std::tuple<Value, Value, Value, Value>{AA, BB, Q, Z}
/// such that Q * A * Z = AA and Q * B * Z = BB, where AA is upper quasi-triangular,
/// BB is upper triangular, and Q, Z are unitary.
std::tuple<Value, Value, Value, Value> qz(const Value &A, const Value &B,
                                           std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::linalg
