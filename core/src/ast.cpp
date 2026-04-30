// src/ast.cpp
#include <numkit/core/ast.hpp>

namespace numkit {

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
    dst->endLine = src->endLine;
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

std::string tryBuildQualifiedName(const ASTNode *node, const ASTNode **rootIdent)
{
    if (!node || node->type != NodeType::FIELD_ACCESS)
        return {};
    std::string out;
    const ASTNode *cur = node;
    while (cur && cur->type == NodeType::FIELD_ACCESS && cur->children.size() == 1) {
        if (!out.empty()) out.insert(0, ".");
        out.insert(0, cur->strValue);
        cur = cur->children[0].get();
    }
    if (!cur || cur->type != NodeType::IDENTIFIER)
        return {};
    out.insert(0, ".");
    out.insert(0, cur->strValue);
    if (rootIdent)
        *rootIdent = cur;
    return out;
}

} // namespace numkit
