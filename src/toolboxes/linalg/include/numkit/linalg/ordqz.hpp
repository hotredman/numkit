// toolboxes/linalg/include/numkit/linalg/ordqz.hpp
//
// Reorder Generalized Schur decomposition: [AAS, BBS, QS, ZS] = ordqz(AA, BB, Q, Z, select/domain)

#pragma once

#include <memory_resource>
#include <string>
#include <tuple>
#include <numkit/value/value.hpp>

namespace numkit::linalg {

/// @brief Reorder Generalized Schur decomposition using a boolean select vector.
std::tuple<Value, Value, Value, Value> ordqz(const Value &AA, const Value &BB,
                                              const Value &Q, const Value &Z,
                                              const Value &select,
                                              std::pmr::memory_resource *mr = nullptr);

/// @brief Reorder Generalized Schur decomposition using domain keyword ('lhp', 'rhp', 'uip', 'uop').
std::tuple<Value, Value, Value, Value> ordqz(const Value &AA, const Value &BB,
                                              const Value &Q, const Value &Z,
                                              const std::string &domain,
                                              std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::linalg
