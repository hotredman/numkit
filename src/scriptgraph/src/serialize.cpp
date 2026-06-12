// scriptgraph/src/serialize.cpp
//
// NodeGraph → JSON. Hand-rolled via ostringstream to match the rest
// of the project's WASM bindings (no nlohmann dep).
//
// Output schema (top-level object):
//   {
//     "functionName": "<script>",
//     "functionInputs":  [...],
//     "functionOutputs": [...],
//     "nodes": [
//       {
//         "id": 0, "kind": "Assignment",
//         "sourceLine": 1, "sourceCol": 1, "endLine": 0,
//         "sourceText": "x = 1",
//         "inputs":  ["a", "b"],
//         "outputs": ["x"],
//         "leadingComment": "",
//         "parentRegionId": null,        // or integer
//         "childNodeIds":     [...],     // empty unless region
//         "branchPartitions": [...]      // empty unless branched region
//       }, ...
//     ],
//     "edges": [
//       { "source": {"nodeId": 0, "portIndex": 0, "name": "x"},
//         "target": {"nodeId": 1, "portIndex": 0, "name": "x"},
//         "kind":  "Data",
//         "varName": "x" },
//       ...
//     ]
//   }

#include <numkit/scriptgraph/serialize.hpp>

#include <sstream>

namespace numkit::scriptgraph {
namespace {

void escapeTo(std::ostringstream &os, const std::string &s)
{
    os << '"';
    for (char c : s) {
        switch (c) {
        case '"':  os << "\\\""; break;
        case '\\': os << "\\\\"; break;
        case '\n': os << "\\n";  break;
        case '\r': os << "\\r";  break;
        case '\t': os << "\\t";  break;
        case '\b': os << "\\b";  break;
        case '\f': os << "\\f";  break;
        default:
            if (static_cast<unsigned char>(c) < 0x20) {
                char buf[8];
                std::snprintf(buf, sizeof(buf), "\\u%04x",
                              static_cast<int>(static_cast<unsigned char>(c)));
                os << buf;
            } else {
                os << c;
            }
        }
    }
    os << '"';
}

const char *nodeKindName(NodeKind k)
{
    switch (k) {
    case NodeKind::Assignment:     return "Assignment";
    case NodeKind::ExprStmt:       return "ExprStmt";
    case NodeKind::IfRegion:       return "IfRegion";
    case NodeKind::ForRegion:      return "ForRegion";
    case NodeKind::WhileRegion:    return "WhileRegion";
    case NodeKind::SwitchRegion:   return "SwitchRegion";
    case NodeKind::TryRegion:      return "TryRegion";
    case NodeKind::JumpContinue:   return "JumpContinue";
    case NodeKind::JumpBreak:      return "JumpBreak";
    case NodeKind::JumpReturn:     return "JumpReturn";
    case NodeKind::Merge:          return "Merge";
    case NodeKind::GlobalDecl:     return "GlobalDecl";
    case NodeKind::PersistentDecl: return "PersistentDecl";
    case NodeKind::FunctionDef:    return "FunctionDef";
    }
    return "Unknown";
}

const char *edgeKindName(EdgeKind k)
{
    switch (k) {
    case EdgeKind::Data:      return "Data";
    case EdgeKind::Sequence:  return "Sequence";
    case EdgeKind::Jump:      return "Jump";
    case EdgeKind::Exception: return "Exception";
    }
    return "Unknown";
}

void writeStringArray(std::ostringstream &os, const std::vector<std::string> &v)
{
    os << '[';
    for (size_t i = 0; i < v.size(); ++i) {
        if (i) os << ',';
        escapeTo(os, v[i]);
    }
    os << ']';
}

void writeIntArray(std::ostringstream &os, const std::vector<int> &v)
{
    os << '[';
    for (size_t i = 0; i < v.size(); ++i) {
        if (i) os << ',';
        os << v[i];
    }
    os << ']';
}

void writePort(std::ostringstream &os, const Port &p)
{
    os << "{\"nodeId\":" << p.nodeId
       << ",\"portIndex\":" << p.portIndex
       << ",\"name\":";
    escapeTo(os, p.name);
    os << '}';
}

void writeNode(std::ostringstream &os, const Node &n)
{
    os << "{\"id\":" << n.id
       << ",\"kind\":\"" << nodeKindName(n.kind) << "\""
       << ",\"sourceLine\":" << n.sourceLine
       << ",\"sourceCol\":"  << n.sourceCol
       << ",\"endLine\":"    << n.endLine
       << ",\"sourceText\":";
    escapeTo(os, n.sourceText);
    os << ",\"inputs\":";
    writeStringArray(os, n.inputs);
    os << ",\"outputs\":";
    writeStringArray(os, n.outputs);
    os << ",\"leadingComment\":";
    escapeTo(os, n.leadingComment);
    os << ",\"parentRegionId\":";
    if (n.parentRegionId) os << *n.parentRegionId;
    else                  os << "null";
    os << ",\"childNodeIds\":";
    writeIntArray(os, n.childNodeIds);
    os << ",\"branchPartitions\":";
    writeIntArray(os, n.branchPartitions);
    os << '}';
}

void writeEdge(std::ostringstream &os, const Edge &e)
{
    os << "{\"source\":";
    writePort(os, e.source);
    os << ",\"target\":";
    writePort(os, e.target);
    os << ",\"kind\":\"" << edgeKindName(e.kind) << "\""
       << ",\"varName\":";
    escapeTo(os, e.varName);
    os << '}';
}

} // namespace

std::string toJSON(const NodeGraph &g)
{
    std::ostringstream os;
    os << "{\"functionName\":";
    escapeTo(os, g.functionName);
    os << ",\"functionInputs\":";
    writeStringArray(os, g.functionInputs);
    os << ",\"functionOutputs\":";
    writeStringArray(os, g.functionOutputs);
    os << ",\"nodes\":[";
    for (size_t i = 0; i < g.nodes.size(); ++i) {
        if (i) os << ',';
        writeNode(os, g.nodes[i]);
    }
    os << "],\"edges\":[";
    for (size_t i = 0; i < g.edges.size(); ++i) {
        if (i) os << ',';
        writeEdge(os, g.edges[i]);
    }
    os << "]}";
    return os.str();
}

} // namespace numkit::scriptgraph
