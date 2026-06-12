// scriptgraph/src/ast_serialize.cpp
//
// AST → JSON for the IDE's AST inspector view. Distinct from
// serialize.cpp (which emits the lowered NodeGraph IR) — this is
// the LITERAL parse-tree, every operator and literal visible.
//
// Schema (top-level matches recursive shape, every node has):
//   {
//     "type":     "<NodeType name>",   // always
//     "line":     <int>,               // 1-indexed, always
//     "col":      <int>,               // 1-indexed, always
//     "endLine":  <int>,               // optional (omitted when 0)
//     "endCol":   <int>,               // optional (omitted when 0)
//     "suppressOutput": true,          // optional (omitted when false)
//     "strValue": "...",               // optional (omitted when empty)
//     "numValue": 3.14,                // optional (only for NUMBER /
//                                      //  IMAG literals where it matters)
//     "boolValue": true,               // optional (only for BOOL literal)
//     "paramNames":  [...],            // optional (omitted when empty)
//     "returnNames": [...],            // optional (omitted when empty)
//     "children":    [ <node>, ... ],  // optional (omitted when empty)
//     "branches":    [                 // optional (only IF / SWITCH)
//       {"cond": <node|null>, "body": <node|null>}, ...
//     ],
//     "elseBranch":  <node>            // optional (only IF / SWITCH /
//                                      //  TRY when present)
//   }
//
// Fields are emitted ONLY when they carry information for the
// renderer — keeps JSON small and avoids ambiguity ("did I forget
// to emit, or is the value just default-empty?").

#include <numkit/scriptgraph/ast_serialize.hpp>

#include <cmath>
#include <cstdio>
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

const char *nodeTypeName(numkit::NodeType t)
{
    switch (t) {
    case numkit::NodeType::NUMBER_LITERAL:        return "NUMBER_LITERAL";
    case numkit::NodeType::IMAG_LITERAL:          return "IMAG_LITERAL";
    case numkit::NodeType::STRING_LITERAL:        return "STRING_LITERAL";
    case numkit::NodeType::DQSTRING_LITERAL:      return "DQSTRING_LITERAL";
    case numkit::NodeType::BOOL_LITERAL:          return "BOOL_LITERAL";
    case numkit::NodeType::IDENTIFIER:            return "IDENTIFIER";
    case numkit::NodeType::BINARY_OP:             return "BINARY_OP";
    case numkit::NodeType::UNARY_OP:              return "UNARY_OP";
    case numkit::NodeType::ASSIGN:                return "ASSIGN";
    case numkit::NodeType::MULTI_ASSIGN:          return "MULTI_ASSIGN";
    case numkit::NodeType::INDEX:                 return "INDEX";
    case numkit::NodeType::CELL_INDEX:            return "CELL_INDEX";
    case numkit::NodeType::FIELD_ACCESS:          return "FIELD_ACCESS";
    case numkit::NodeType::DYNAMIC_FIELD_ACCESS:  return "DYNAMIC_FIELD_ACCESS";
    case numkit::NodeType::MATRIX_LITERAL:        return "MATRIX_LITERAL";
    case numkit::NodeType::CELL_LITERAL:          return "CELL_LITERAL";
    case numkit::NodeType::CALL:                  return "CALL";
    case numkit::NodeType::COLON_EXPR:            return "COLON_EXPR";
    case numkit::NodeType::IF_STMT:               return "IF_STMT";
    case numkit::NodeType::FOR_STMT:              return "FOR_STMT";
    case numkit::NodeType::WHILE_STMT:            return "WHILE_STMT";
    case numkit::NodeType::BREAK_STMT:            return "BREAK_STMT";
    case numkit::NodeType::CONTINUE_STMT:         return "CONTINUE_STMT";
    case numkit::NodeType::RETURN_STMT:           return "RETURN_STMT";
    case numkit::NodeType::SWITCH_STMT:           return "SWITCH_STMT";
    case numkit::NodeType::FUNCTION_DEF:          return "FUNCTION_DEF";
    case numkit::NodeType::BLOCK:                 return "BLOCK";
    case numkit::NodeType::EXPR_STMT:             return "EXPR_STMT";
    case numkit::NodeType::END_VAL:               return "END_VAL";
    case numkit::NodeType::ANON_FUNC:             return "ANON_FUNC";
    case numkit::NodeType::TRY_STMT:              return "TRY_STMT";
    case numkit::NodeType::GLOBAL_STMT:           return "GLOBAL_STMT";
    case numkit::NodeType::PERSISTENT_STMT:       return "PERSISTENT_STMT";
    case numkit::NodeType::DELETE_ASSIGN:         return "DELETE_ASSIGN";
    case numkit::NodeType::COMMAND_CALL:          return "COMMAND_CALL";
    }
    return "UNKNOWN";
}

/** True if this NodeType uses `numValue` semantically (so we should
 *  emit it). For non-numeric types numValue is undefined garbage. */
bool typeUsesNumValue(numkit::NodeType t)
{
    return t == numkit::NodeType::NUMBER_LITERAL
        || t == numkit::NodeType::IMAG_LITERAL;
}

bool typeUsesBoolValue(numkit::NodeType t)
{
    return t == numkit::NodeType::BOOL_LITERAL;
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

void writeNumber(std::ostringstream &os, double v)
{
    // Emit doubles such that re-parsing round-trips. Special-case
    // NaN / +-Inf since JSON has no native representation — fall
    // back to a string the renderer recognizes.
    if (std::isnan(v))      { os << "\"NaN\"";   return; }
    if (std::isinf(v))      { os << (v < 0 ? "\"-Infinity\"" : "\"Infinity\""); return; }
    // Use ostringstream's default which produces enough precision
    // for round-trip on typical values; specify higher precision
    // explicitly to avoid surprises.
    std::ostringstream tmp;
    tmp.precision(17);
    tmp << v;
    os << tmp.str();
}

void writeNode(std::ostringstream &os, const numkit::ASTNode *n)
{
    if (!n) {
        os << "null";
        return;
    }
    os << '{';
    os << "\"type\":\"" << nodeTypeName(n->type) << "\"";
    os << ",\"line\":" << n->line;
    os << ",\"col\":"  << n->col;
    if (n->endLine > 0) os << ",\"endLine\":" << n->endLine;
    if (n->endCol  > 0) os << ",\"endCol\":"  << n->endCol;
    if (n->suppressOutput) os << ",\"suppressOutput\":true";
    if (!n->strValue.empty()) {
        os << ",\"strValue\":";
        escapeTo(os, n->strValue);
    }
    if (typeUsesNumValue(n->type)) {
        os << ",\"numValue\":";
        writeNumber(os, n->numValue);
    }
    if (typeUsesBoolValue(n->type)) {
        os << ",\"boolValue\":" << (n->boolValue ? "true" : "false");
    }
    if (!n->paramNames.empty()) {
        os << ",\"paramNames\":";
        writeStringArray(os, n->paramNames);
    }
    if (!n->returnNames.empty()) {
        os << ",\"returnNames\":";
        writeStringArray(os, n->returnNames);
    }
    if (!n->children.empty()) {
        os << ",\"children\":[";
        for (size_t i = 0; i < n->children.size(); ++i) {
            if (i) os << ',';
            writeNode(os, n->children[i].get());
        }
        os << ']';
    }
    if (!n->branches.empty()) {
        os << ",\"branches\":[";
        for (size_t i = 0; i < n->branches.size(); ++i) {
            if (i) os << ',';
            os << "{\"cond\":";
            writeNode(os, n->branches[i].first.get());
            os << ",\"body\":";
            writeNode(os, n->branches[i].second.get());
            os << '}';
        }
        os << ']';
    }
    if (n->elseBranch) {
        os << ",\"elseBranch\":";
        writeNode(os, n->elseBranch.get());
    }
    os << '}';
}

} // namespace

std::string toASTJSON(const numkit::ASTNode &root)
{
    std::ostringstream os;
    writeNode(os, &root);
    return os.str();
}

} // namespace numkit::scriptgraph
