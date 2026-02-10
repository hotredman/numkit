// src/MLabAst.cpp
#include "MLabAst.hpp"

namespace mlab {

ASTNodePtr makeNode(NodeType t)
{
    return std::make_unique<ASTNode>(t);
}

ASTNodePtr cloneNode(const ASTNode *src)
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
