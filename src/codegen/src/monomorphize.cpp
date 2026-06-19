// codegen/src/monomorphize.cpp — see monomorphize.hpp.

#include <numkit/codegen/monomorphize.hpp>

#include <numkit/codegen/classinfo.hpp>

#include <memory>
#include <unordered_set>

namespace numkit::codegen {

void FunctionTable::add(const ASTNode &funcDef)
{
    if (funcDef.type == NodeType::FUNCTION_DEF && !funcDef.strValue.empty())
        defs_.insert_or_assign(funcDef.strValue, &funcDef);
}

const ASTNode *FunctionTable::find(const std::string &name) const
{
    const auto it = defs_.find(name);
    return it == defs_.end() ? nullptr : it->second;
}

void collectFunctions(const ASTNode &root, FunctionTable &table)
{
    if (root.type == NodeType::FUNCTION_DEF) table.add(root);
    for (const auto &c : root.children)
        if (c) collectFunctions(*c, table);
    for (const auto &br : root.branches) {
        if (br.first)  collectFunctions(*br.first, table);
        if (br.second) collectFunctions(*br.second, table);
    }
    if (root.elseBranch) collectFunctions(*root.elseBranch, table);
}

InferredType inferFunctionReturn(const ASTNode &funcDef,
                                 const std::vector<ArgInfo> &args,
                                 const TransferRegistry &reg, const ClassRegistry *classes)
{
    if (funcDef.type != NodeType::FUNCTION_DEF || funcDef.children.empty())
        return InferredType::dynamic();
    if (funcDef.returnNames.size() != 1)            // MVP: single output
        return InferredType::dynamic();
    if (args.size() != funcDef.paramNames.size())   // MVP: exact arity
        return InferredType::dynamic();

    // Seed the entry env: each parameter takes the matching argument's type
    // and constant facet (the constant flows into shape-from-value).
    TypeEnv env;
    for (std::size_t i = 0; i < args.size(); ++i)
        env.set(funcDef.paramNames[i], {args[i].type, args[i].constant});

    inferStmt(*funcDef.children[0], env, reg, nullptr, classes);
    return env.get(funcDef.returnNames[0]).type;
}

void registerClassMethods(TransferRegistry &reg, const ClassRegistry &classes)
{
    auto inProgress = std::make_shared<std::unordered_set<std::string>>();
    for (std::size_t i = 0; i < classes.size(); ++i) {
        const ClassInfo *ci = classes.byId(static_cast<int>(i));
        if (!ci) continue;
        for (const auto &[mname, mdef] : ci->methods) {
            const std::string key = ci->name + "::" + mname;
            const ASTNode    *md  = mdef;
            reg.add(key, [md, key, &reg, &classes, inProgress](const std::vector<ArgInfo> &args) {
                if (!inProgress->insert(key).second)
                    return InferredType::dynamic();          // recursion -> sound break
                const InferredType r = inferFunctionReturn(*md, args, reg, &classes);
                inProgress->erase(key);
                return r;
            });
        }
    }
}

void registerUserFunctions(TransferRegistry &reg, const FunctionTable &table)
{
    // Shared across all user-function transfers: the set of functions
    // currently being inferred, so a recursive (re-entrant) call returns
    // Dynamic instead of looping forever.
    auto inProgress = std::make_shared<std::unordered_set<std::string>>();

    for (const auto &[name, def] : table.entries()) {
        const std::string key = name;   // own copy per transfer
        const ASTNode    *fn  = def;
        reg.add(key, [fn, key, &reg, inProgress](const std::vector<ArgInfo> &args) {
            if (!inProgress->insert(key).second)         // already on the stack
                return InferredType::dynamic();          // recursion -> sound break
            const InferredType r = inferFunctionReturn(*fn, args, reg);
            inProgress->erase(key);
            return r;
        });
    }
}

} // namespace numkit::codegen
