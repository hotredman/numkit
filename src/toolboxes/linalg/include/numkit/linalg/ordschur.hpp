// toolboxes/linalg/include/numkit/linalg/ordschur.hpp
//
// Reorder Schur decomposition: [US, TS] = ordschur(U, T, select)

#pragma once

#include <memory_resource>
#include <string>
#include <tuple>
#include <vector>
#include <numkit/value/value.hpp>

namespace numkit::linalg {

/// @brief Reorder Schur decomposition A = U * T * U^H using a boolean select vector.
/// Returns std::tuple<Value, Value>{U_out, T_out}.
std::tuple<Value, Value> ordschur(const Value &U, const Value &T,
                                  const Value &select,
                                  std::pmr::memory_resource *mr = nullptr);

/// @brief Reorder Schur decomposition A = U * T * U^H using domain keyword
/// ('lhp', 'rhp', 'uip', 'uop').
std::tuple<Value, Value> ordschur(const Value &U, const Value &T,
                                  const std::string &domain,
                                  std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::linalg
