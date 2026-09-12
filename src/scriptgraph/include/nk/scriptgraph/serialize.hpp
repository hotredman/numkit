// scriptgraph/include/nk/scriptgraph/serialize.hpp
//
// JSON serializer for NodeGraph. Hand-rolled (no nlohmann dep) to
// match the rest of the project's WASM bindings. Output is a single
// JSON object — see comment in serialize.cpp for the schema.

#pragma once

#include <nk/scriptgraph/node_graph.hpp>

#include <string>

namespace nk::scriptgraph {

std::string toJSON(const NodeGraph &g);

} // namespace nk::scriptgraph
