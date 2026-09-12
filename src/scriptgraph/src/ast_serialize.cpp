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

#include <nk/scriptgraph/ast_serialize.hpp>

#include <cmath>
#include <cstdio>
#include <sstream>

namespace nk::scriptgraph {
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

const char *nodeTypeName(nk::NodeType t)
{
    switch (t) {
    case nk::NodeType::NUMBER_LITERAL:        return "NUMBER_LITERAL";
    case nk::NodeType::IMAG_LITERAL:          return "IMAG_LITERAL";
    case nk::NodeType::STRING_LITERAL:        return "STRING_LITERAL";
    case nk::NodeType::DQSTRING_LITERAL:      return "DQSTRING_LITERAL";
    case nk::NodeType::BOOL_LITERAL:          return "BOOL_LITERAL";
    case nk::NodeType::IDENTIFIER:            return "IDENTIFIER";
    case nk::NodeType::BINARY_OP:             return "BINARY_OP";
    case nk::NodeType::UNARY_OP:              return "UNARY_OP";
    case nk::NodeType::ASSIGN:                return "ASSIGN";
    case nk::NodeType::MULTI_ASSIGN:          return "MULTI_ASSIGN";
    case nk::NodeType::INDEX:                 return "INDEX";
    case nk::NodeType::CELL_INDEX:            return "CELL_INDEX";
    case nk::NodeType::FIELD_ACCESS:          return "FIELD_ACCESS";
    case nk::NodeType::DYNAMIC_FIELD_ACCESS:  return "DYNAMIC_FIELD_ACCESS";
    case nk::NodeType::MATRIX_LITERAL:        return "MATRIX_LITERAL";
    case nk::NodeType::CELL_LITERAL:          return "CELL_LITERAL";
    case nk::NodeType::CALL:                  return "CALL";
    case nk::NodeType::COLON_EXPR:            return "COLON_EXPR";
    case nk::NodeType::IF_STMT:               return "IF_STMT";
    case nk::NodeType::FOR_STMT:              return "FOR_STMT";
    case nk::NodeType::WHILE_STMT:            return "WHILE_STMT";
    case nk::NodeType::BREAK_STMT:            return "BREAK_STMT";
    case nk::NodeType::CONTINUE_STMT:         return "CONTINUE_STMT";
    case nk::NodeType::RETURN_STMT:           return "RETURN_STMT";
    case nk::NodeType::SWITCH_STMT:           return "SWITCH_STMT";
    case nk::NodeType::FUNCTION_DEF:          return "FUNCTION_DEF";
    case nk::NodeType::BLOCK:                 return "BLOCK";
    case nk::NodeType::EXPR_STMT:             return "EXPR_STMT";
    case nk::NodeType::END_VAL:               return "END_VAL";
    case nk::NodeType::ANON_FUNC:             return "ANON_FUNC";
    case nk::NodeType::TRY_STMT:              return "TRY_STMT";
    case nk::NodeType::GLOBAL_STMT:           return "GLOBAL_STMT";
    case nk::NodeType::PERSISTENT_STMT:       return "PERSISTENT_STMT";
    case nk::NodeType::DELETE_ASSIGN:         return "DELETE_ASSIGN";
    case nk::NodeType::COMMAND_CALL:          return "COMMAND_CALL";
    }
    return "UNKNOWN";
}

/** True if this NodeType uses `numValue` semantically (so we should
 *  emit it). For non-numeric types numValue is undefined garbage. */
bool typeUsesNumValue(nk::NodeType t)
{
    return t == nk::NodeType::NUMBER_LITERAL
        || t == nk::NodeType::IMAG_LITERAL;
}

bool typeUsesBoolValue(nk::NodeType t)
{
    return t == nk::NodeType::BOOL_LITERAL;
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

void writeNode(std::ostringstream &os, const nk::ASTNode *n)
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

std::string toASTJSON(const nk::ASTNode &root)
{
    std::ostringstream os;
    writeNode(os, &root);
    return os.str();
}

} // namespace nk::scriptgraph
