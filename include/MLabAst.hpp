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
};

struct ASTNode;
using ASTNodePtr = std::unique_ptr<ASTNode>;

struct ASTNode
{
    NodeType type;
    std::string strValue;
    double numValue = 0;
    bool boolValue = false;
    bool suppressOutput = false;

    std::vector<ASTNodePtr> children;
    std::vector<std::string> paramNames;
    std::vector<std::string> returnNames;

    std::vector<std::pair<ASTNodePtr, ASTNodePtr>> branches;
    ASTNodePtr elseBranch;

    ASTNode()
        : type(NodeType::NUMBER_LITERAL)
    {}
    explicit ASTNode(NodeType t)
        : type(t)
    {}
};

inline ASTNodePtr makeNode(NodeType t)
{
    return std::make_unique<ASTNode>(t);
}

inline ASTNodePtr cloneNode(const ASTNode *src)
{
    if (!src)
        return nullptr;

    auto dst = std::make_unique<ASTNode>(src->type);
    dst->strValue = src->strValue;
    dst->numValue = src->numValue;
    dst->boolValue = src->boolValue;
    dst->suppressOutput = src->suppressOutput;
    dst->paramNames = src->paramNames;
    dst->returnNames = src->returnNames;

    for (auto &child : src->children)
        dst->children.push_back(cloneNode(child.get()));

    for (auto &[cond, body] : src->branches)
        dst->branches.push_back({cloneNode(cond.get()), cloneNode(body.get())});

    dst->elseBranch = cloneNode(src->elseBranch.get());

    return dst;
}

} // namespace mlab
