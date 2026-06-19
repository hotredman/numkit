// codegen/src/classinfo.cpp — see classinfo.hpp.

#include <numkit/codegen/classinfo.hpp>

#include <numkit/codegen/inference.hpp>

#include <stdexcept>
#include <utility>

namespace numkit::codegen {

const ClassField *ClassInfo::field(const std::string &n) const
{
    for (const auto &f : fields)
        if (f.name == n) return &f;
    return nullptr;
}

const ASTNode *ClassInfo::method(const std::string &n) const
{
    const auto it = methods.find(n);
    return it == methods.end() ? nullptr : it->second;
}

int ClassRegistry::add(ClassInfo info)
{
    info.id = static_cast<int>(classes_.size());
    byName_.insert_or_assign(info.name, info.id);
    classes_.push_back(std::move(info));
    return classes_.back().id;
}

const ClassInfo *ClassRegistry::byId(int id) const
{
    if (id < 0 || id >= static_cast<int>(classes_.size())) return nullptr;
    return &classes_[static_cast<std::size_t>(id)];
}

int ClassRegistry::idOf(const std::string &name) const
{
    const auto it = byName_.find(name);
    return it == byName_.end() ? -1 : it->second;
}

const ClassInfo *ClassRegistry::byName(const std::string &name) const
{
    return byId(idOf(name));
}

std::string classRefusalReason(const ASTNode &classDef)
{
    if (classDef.type != NodeType::CLASSDEF_DEF) return "not a classdef";

    // Inheritance beyond the `handle` mixin is out of the v1 subset.
    for (const auto &super : classDef.paramNames)
        if (super != "handle")
            return "inheritance from '" + super + "' not supported (v1)";

    for (const auto &c : classDef.children) {
        if (!c) continue;
        if (c->type == NodeType::FUNCTION_DEF
            && c->strValue.find('.') != std::string::npos)
            return "property accessor '" + c->strValue + "' not supported (v1)";
        if (c->type == NodeType::CLASSDEF_PROPERTY)
            for (const auto &a : c->classAttrs)
                if (a == "Dependent" || a == "Constant" || a == "Abstract")
                    return "property attribute '" + a + "' not supported (v1)";
        if (c->type == NodeType::CLASSDEF_ENUM_MEMBER)
            return "enumeration classes not supported (v1)";
    }
    return {};
}

ClassInfo buildClassInfo(const ASTNode &classDef, int id, const TransferRegistry &reg)
{
    if (const std::string why = classRefusalReason(classDef); !why.empty())
        throw std::runtime_error("class '" + classDef.strValue + "': " + why);

    ClassInfo ci;
    ci.name = classDef.strValue;
    ci.id   = id;
    ci.def  = &classDef;
    for (const auto &super : classDef.paramNames)
        if (super == "handle") ci.isHandle = true;

    for (const auto &c : classDef.children) {
        if (!c) continue;
        if (c->type == NodeType::CLASSDEF_PROPERTY) {
            if (c->children.empty() || !c->children[0])
                throw std::runtime_error("class '" + ci.name + "': property '" + c->strValue
                                         + "' needs a concrete-typed default (v1)");
            TypeEnv            empty;
            const InferredType ft = inferExpr(*c->children[0], empty, reg).type;
            if (!ft.isConcrete())
                throw std::runtime_error("class '" + ci.name + "': property '" + c->strValue
                                         + "' default is not concretely typed (" + ft.str() + ")");
            ci.fields.push_back({c->strValue, ft, c->children[0].get()});
        } else if (c->type == NodeType::FUNCTION_DEF) {
            ci.methods.insert_or_assign(c->strValue, c.get());
        }
    }
    return ci;
}

std::size_t collectClasses(const ASTNode &root, ClassRegistry &reg,
                           const TransferRegistry &reg2)
{
    std::size_t added = 0;
    if (root.type == NodeType::CLASSDEF_DEF) {
        reg.add(buildClassInfo(root, static_cast<int>(reg.size()), reg2));
        ++added;
    }
    for (const auto &c : root.children)
        if (c) added += collectClasses(*c, reg, reg2);
    return added;
}

} // namespace numkit::codegen
