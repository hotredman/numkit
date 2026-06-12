// scriptgraph/include/numkit/scriptgraph/ast_serialize.hpp
//
// AST → JSON serializer. The IDE's AST view consumes this to render
// a literal parse-tree visualization side-by-side with the data-flow
// graph view. Schema and field semantics are documented at the top
// of ast_serialize.cpp.
//
// Implementation is hand-rolled (no nlohmann dep) to match the rest
// of the project's WASM bindings.

#pragma once

#include <numkit/core/ast.hpp>

#include <string>

namespace numkit::scriptgraph {

/** Serialize a single AST subtree (typically the BLOCK returned by
 *  Parser::parse()) to a JSON string. The output is a nested object
 *  where every node has a `type` (NodeType name), source-position
 *  fields, and a `children` array — plus per-node extras like
 *  `strValue`, `numValue`, `paramNames`, `branches`, `elseBranch`
 *  emitted only when the node kind uses them. */
std::string toASTJSON(const numkit::ASTNode &root);

} // namespace numkit::scriptgraph
