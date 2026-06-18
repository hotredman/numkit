// codegen/include/numkit/codegen/emitter.hpp
//
// The C++ emitter — turns a typed AST into C++ source text. Built in
// sub-bricks; this first one is the unboxed-scalar expression core:
// literals, identifiers, and scalar arithmetic / comparison / logical /
// power operators emit a plain C++ expression (double / bool / complex),
// no Value involved. Array access, runtime calls, statements and control
// flow come in later sub-bricks.

#pragma once

#include <numkit/codegen/type_lattice.hpp>

#include <numkit/core/ast.hpp>

#include <string>

namespace numkit::codegen {

// The C++ type name for an unboxable scalar dtype (double / float /
// std::complex<double> / bool / intN_t). Throws std::runtime_error for a
// dtype with no scalar C++ mapping (CELL/STRUCT/STRING/…).
std::string cppScalarType(ValueType dtype);

// A round-trip C++ double literal for `v` (shortest form; integers get a
// trailing .0; Inf/NaN map to std::numeric_limits forms).
std::string formatDoubleLiteral(double v);

// Emit a C++ expression string for an unboxed-scalar AST expression
// (NUMBER/IMAG/BOOL literal, IDENTIFIER, scalar BINARY_OP / UNARY_OP).
// `^`/`.^` lower to std::pow. Throws std::runtime_error on a node kind
// outside this sub-brick's scope (calls, indexing, matrices — later).
std::string emitScalarExpr(const ASTNode &expr);

} // namespace numkit::codegen
