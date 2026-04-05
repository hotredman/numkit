// include/MLabAst.hpp
#pragma once
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace mlab {

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

    std::string strValue;
    double numValue = 0;
    bool boolValue = false;
    bool suppressOutput = false;

    std::vector<ASTNodePtr> children;
    std::vector<std::string> paramNames;
    std::vector<std::string> returnNames;

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

} // namespace mlab