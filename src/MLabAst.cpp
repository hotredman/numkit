// src/MLabAst.cpp
#include "MLabAst.hpp"

namespace mlab {

// ============================================================
// Фабрики узлов
// ============================================================

ASTNodePtr makeNode(NodeType t)
{
    return std::make_unique<ASTNode>(t);
}

ASTNodePtr makeNode(NodeType t, int line, int col)
{
    auto node = std::make_unique<ASTNode>(t);
    node->line = line;
    node->col = col;
    return node;
}

ASTNodePtr cloneNode(const ASTNode *src)
{
    if (!src)
        return nullptr;

    auto dst = std::make_unique<ASTNode>(src->type);
    dst->line = src->line;
    dst->col = src->col;
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
