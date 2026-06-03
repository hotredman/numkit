// include/ast.hpp
#pragma once
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace numkit {

enum class NodeType {
    NUMBER_LITERAL,
    IMAG_LITERAL,
    STRING_LITERAL,
    BOOL_LITERAL,
    IDENTIFIER,
    BINARY_OP,
    UNARY_OP,
    ASSIGN,
    MULTI_ASSIGN,
    INDEX,
    CELL_INDEX,
    FIELD_ACCESS,
    DYNAMIC_FIELD_ACCESS, // s.(expr) — field name from expression
    MATRIX_LITERAL,
    CELL_LITERAL,
    CALL,
    COLON_EXPR,
    IF_STMT,
    FOR_STMT,
    WHILE_STMT,
    BREAK_STMT,
    CONTINUE_STMT,
    RETURN_STMT,
    SWITCH_STMT,
    FUNCTION_DEF,
    // classdef. CLASSDEF_DEF: strValue = class name; paramNames =
    // superclass names (`< Base & ...`); children = a mix of
    // CLASSDEF_PROPERTY and FUNCTION_DEF (methods incl. the constructor).
    // CLASSDEF_PROPERTY: strValue = property name; children[0] = default
    // expression (absent → default []). See OBJECT_MODEL.md.
    CLASSDEF_DEF,
    CLASSDEF_PROPERTY,
    // Superclass-qualified reference `lhs@Base`: children[0] = lhs (the
    // object var in a constructor, or a method-name identifier in a method),
    // strValue = Base class name. Wrapped in a CALL for `lhs@Base(args)`.
    SUPERCLASS_REF,
    BLOCK,
    EXPR_STMT,
    END_VAL,
    ANON_FUNC,
    TRY_STMT,
    GLOBAL_STMT,
    PERSISTENT_STMT,
    DELETE_ASSIGN,
    COMMAND_CALL,
    DQSTRING_LITERAL,
};

struct ASTNode;
using ASTNodePtr = std::unique_ptr<ASTNode>;

struct ASTNode
{
    NodeType type;
    int line = 0;
    int col = 0;
    int endLine = 0; // line of closing 'end' keyword (for loops, if, switch, etc.)
    // Position one past the last character of the statement's TEXTUAL
    // form (so e.g. for `a = 1;` endCol points to col after `;`).
    // Populated by the parser for simple statements via
    // consumeStmtTerminator() right when the trailing ';' / ',' token
    // is consumed. Stays 0 when no terminator was present — callers
    // (graph view) treat 0 as "slice to end of line".
    int endCol  = 0;

    std::string strValue;
    double numValue = 0;
    bool boolValue = false;
    bool suppressOutput = false;

    std::vector<ASTNodePtr> children;
    std::vector<std::string> paramNames;
    std::vector<std::string> returnNames;
    // classdef only: attributes of the enclosing properties/methods block,
    // stored as flat tokens (e.g. {"Static"}, {"Constant"},
    // {"Access","private"}) on each CLASSDEF_PROPERTY / method FUNCTION_DEF.
    std::vector<std::string> classAttrs;

    // MULTI_ASSIGN only: one lvalue expression per output target when at
    // least one target is a complex lvalue (`s.f`, `a(i)`, `c{i}`, ...).
    // Empty when every target is a bare identifier or `~` (the common
    // case stays on the fast returnNames-only path). A nullptr entry
    // denotes an ignored `~` output. When non-empty, the size matches
    // returnNames and this is the authoritative target list.
    std::vector<ASTNodePtr> lhsTargets;

    std::vector<std::pair<ASTNodePtr, ASTNodePtr>> branches;
    ASTNodePtr elseBranch;

    // Cached operator function pointer (set on first eval, avoids hash lookup)
    mutable const void *cachedOp = nullptr;
    // Cached builtin ID for inline scalar evaluation (0 = not resolved, -1 = not a builtin)
    mutable int8_t cachedBuiltinId = 0;
    // Cached pointer to UserFunction (for user-defined function calls)
    mutable const void *cachedUserFunc = nullptr;

    ASTNode()
        : type(NodeType::NUMBER_LITERAL)
    {}
    explicit ASTNode(NodeType t)
        : type(t)
    {}
};

ASTNodePtr makeNode(NodeType t);
ASTNodePtr makeNode(NodeType t, int line, int col);
ASTNodePtr cloneNode(const ASTNode *src);

// Walk a chain of FIELD_ACCESS nodes rooted at an IDENTIFIER and produce
// the dotted name (`pkg.sub.foo` for `pkg.sub.foo` or its enclosing CALL
// node's funcNode). Returns the empty string when the chain doesn't fit
// the qualified-name shape — caller falls through to other paths.
//
// On success, *rootIdent is set to the IDENTIFIER node at the chain root
// (callers need it for variable-shadow checks).
std::string tryBuildQualifiedName(const ASTNode *node,
                                   const ASTNode **rootIdent = nullptr);

} // namespace numkit