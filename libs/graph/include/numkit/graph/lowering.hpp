// libs/graph/include/numkit/graph/lowering.hpp
//
// Public API: lower an AST root to a NodeGraph IR.

#pragma once

#include <numkit/core/ast.hpp>
#include <numkit/graph/node_graph.hpp>

#include <string>

namespace numkit::graph {

/** Lower a parsed script (top-level BLOCK or single statement) to a
 *  NodeGraph. `sourceText` is the original .m text used for slicing
 *  per-node sourceText fields by line/col. Pass empty string to skip
 *  source slicing — node.sourceText falls back to a synthesized
 *  short form.
 *
 *  MVP scope: Assignment + ExprStmt only. Control-flow nodes
 *  (IF_STMT, FOR_STMT, etc.) are stubbed as opaque region roots
 *  without recursion; full support lands in Phase 2. */
NodeGraph lowerScript(const ASTNode &root, const std::string &sourceText = "");

} // namespace numkit::graph
