// scriptgraph/include/numkit/scriptgraph/serialize.hpp
//
// JSON serializer for NodeGraph. Hand-rolled (no nlohmann dep) to
// match the rest of the project's WASM bindings. Output is a single
// JSON object — see comment in serialize.cpp for the schema.

#pragma once

#include <numkit/scriptgraph/node_graph.hpp>

#include <string>

namespace numkit::scriptgraph {

std::string toJSON(const NodeGraph &g);

} // namespace numkit::scriptgraph
