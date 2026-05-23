// libs/graph/include/numkit/graph/lowering.hpp
//
// Public API: lower an AST root to a NodeGraph IR.

#pragma once

#include <numkit/core/ast.hpp>
#include <numkit/core/lexer.hpp>
#include <numkit/graph/node_graph.hpp>

#include <string>
#include <vector>

namespace numkit::graph {

/** Lower a parsed script (top-level BLOCK or single statement) to a
 *  NodeGraph. `sourceText` is the original .m text used for slicing
 *  per-node sourceText fields by line/col. Pass empty string to skip
 *  source slicing — node.sourceText falls back to a synthesized
 *  short form.
 *
 *  `tokens` is the raw lexer output (including COMMENT tokens) used
 *  to trim trailing `% comments` from per-node sourceText. Pass an
 *  empty vector to skip the trim — sourceText keeps the whole line
 *  (warts and all). Re-using already-lexed tokens avoids a second
 *  pass; the WASM binding does this naturally.
 *
 *  MVP scope: Assignment + ExprStmt only. Control-flow nodes
 *  (IF_STMT, FOR_STMT, etc.) are stubbed as opaque region roots
 *  without recursion; full support lands in Phase 2. */
NodeGraph lowerScript(const ASTNode &root,
                      const std::string &sourceText = "",
                      const std::vector<Token> &tokens = {});

} // namespace numkit::graph
