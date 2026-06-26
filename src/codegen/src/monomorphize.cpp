// codegen/src/monomorphize.cpp — see monomorphize.hpp.

#include <numkit/codegen/monomorphize.hpp>

#include <numkit/codegen/classinfo.hpp>

#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace numkit::codegen {

namespace {

// Does `n` (a function body) contain a direct call to `name`? Used to detect
// DIRECT self-recursion so the return-type fixpoint runs only where it is needed
// (a non-recursive function keeps the single-pass inference -- no 2x cost, and no
// exponential blow-up down a deep non-recursive call chain). Mutual recursion is
// NOT detected here (each fn sees no direct self-call) -> it takes the sound
// Dynamic break, as before.
bool nodeCallsName(const ASTNode &n, const std::string &name)
{
    if (n.type == NodeType::CALL && !n.children.empty() && n.children[0]
        && n.children[0]->type == NodeType::IDENTIFIER && n.children[0]->strValue == name)
        return true;
    for (const auto &c : n.children)
        if (c && nodeCallsName(*c, name)) return true;
    for (const auto &br : n.branches) {
        if (br.first && nodeCallsName(*br.first, name)) return true;
        if (br.second && nodeCallsName(*br.second, name)) return true;
    }
    if (n.elseBranch && nodeCallsName(*n.elseBranch, name)) return true;
    return false;
}

// The fixpoint key: function name + each argument's TYPE signature (str() is a
// precise encoding for the eligible types -- numeric scalars/arrays/complex). A
// same-signature self-call shares the estimate; a different-signature self-call
// (polymorphic recursion) does not, and falls to the Dynamic break.
std::string fixpointKey(const std::string &name, const std::vector<ArgInfo> &args)
{
    std::string s = name;
    for (const ArgInfo &a : args) {
        s += '#';
        s += a.type.str();
    }
    return s;
}

// An argument is fixpoint-eligible when its type has a PRECISE str() signature.
// STRUCT (str = field names only, not field types) and OBJECT are excluded -- a
// signature collision there could share an estimate unsoundly; such recursion is
// rare, so it takes the sound Dynamic break instead.
bool fixpointEligible(const std::vector<ArgInfo> &args)
{
    for (const ArgInfo &a : args)
        if (a.type.isStruct() || a.type.isObject()) return false;
    return true;
}

constexpr int kFixpointCap = 8;  // lattice height is tiny; 8 is far above need

}  // namespace

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
    // A classdef's FUNCTION_DEF children are METHODS, not free functions —
    // they are handled by registerClassMethods. Do not descend into a
    // classdef, or methods would leak into the free-function table.
    if (root.type == NodeType::CLASSDEF_DEF) return;
    if (root.type == NodeType::FUNCTION_DEF) table.add(root);
    for (const auto &c : root.children)
        if (c) collectFunctions(*c, table);
    for (const auto &br : root.branches) {
        if (br.first)  collectFunctions(*br.first, table);
        if (br.second) collectFunctions(*br.second, table);
    }
    if (root.elseBranch) collectFunctions(*root.elseBranch, table);
}

std::vector<InferredType> inferFunctionReturns(const ASTNode &funcDef,
                                               const std::vector<ArgInfo> &args,
                                               const TransferRegistry &reg,
                                               const ClassRegistry *classes)
{
    if (funcDef.type != NodeType::FUNCTION_DEF || funcDef.children.empty())
        return {};
    if (args.size() != funcDef.paramNames.size())   // exact arity (MVP)
        return {};

    // Seed the entry env: each parameter takes the matching argument's type
    // and constant facet (the constant flows into shape-from-value).
    TypeEnv env;
    for (std::size_t i = 0; i < args.size(); ++i) {
        env.set(funcDef.paramNames[i], {args[i].type, args[i].constant});
        // A STRUCT param also seeds its field-locals (_nk_fld_<p>_<f>) so the body's `s.f`
        // reads them -- mirrors emitOneFunction's struct-param explosion (G2.3). Without
        // this the body's field reads infer Dynamic and the return is mis-typed.
        if (args[i].type.isStruct() && args[i].type.structLayout)
            for (const auto &f : args[i].type.structLayout->fields)
                env.set("_nk_fld_" + funcDef.paramNames[i] + "_" + f.first,
                        {f.second, ConstVal::unknown()});
    }

    inferStmt(*funcDef.children[0], env, reg, nullptr, classes);

    std::vector<InferredType> outs;
    outs.reserve(funcDef.returnNames.size());
    for (const auto &rn : funcDef.returnNames)
        outs.push_back(rn.empty() ? InferredType::dynamic() : env.get(rn).type);
    return outs;
}

InferredType inferFunctionReturn(const ASTNode &funcDef,
                                 const std::vector<ArgInfo> &args,
                                 const TransferRegistry &reg, const ClassRegistry *classes)
{
    const std::vector<InferredType> outs = inferFunctionReturns(funcDef, args, reg, classes);
    return outs.size() == 1 ? outs[0] : InferredType::dynamic();
}

void registerClassConstructors(TransferRegistry &reg, const ClassRegistry &classes)
{
    for (std::size_t i = 0; i < classes.size(); ++i) {
        const ClassInfo *ci = classes.byId(static_cast<int>(i));
        if (!ci) continue;
        const int id = ci->id;
        reg.add(ci->name,
                [id](const std::vector<ArgInfo> &) { return InferredType::object(id); });
    }
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
            reg.addMulti(key, [md, key, &reg, &classes, inProgress](const std::vector<ArgInfo> &args)
                                  -> std::vector<InferredType> {
                if (!inProgress->insert(key).second) return {};
                std::vector<InferredType> r = inferFunctionReturns(*md, args, reg, &classes);
                inProgress->erase(key);
                return r;
            });
        }
    }
}

void registerUserFunctions(TransferRegistry &reg, const FunctionTable &table)
{
    // Expose the table so the inference can tell a user fn from a builtin (e.g.
    // the bare `c = f(x)` multi-output first-output projection). Borrowed -- the
    // table must outlive the registry (it already backs the transfers below).
    reg.setUserFunctions(&table);

    // Shared across all user-function transfers: the set of functions
    // currently being inferred, so a recursive (re-entrant) call returns
    // Dynamic instead of looping forever.
    auto inProgress = std::make_shared<std::unordered_set<std::string>>();
    // The recursion return-type FIXPOINT state (Rec.2), keyed by (fn + arg-type
    // signature). A same-signature self-call reads the current estimate (seeded
    // Bottom); the outer entry iterates the body inference to a fixpoint.
    auto estimates = std::make_shared<std::unordered_map<std::string, InferredType>>();

    for (const auto &[name, def] : table.entries()) {
        const std::string key = name;   // own copy per transfer
        const ASTNode    *fn  = def;
        // DIRECT self-recursion, computed once: only such a function runs the
        // fixpoint (others keep the cheap single pass).
        const bool selfRec =
            !fn->children.empty() && fn->children[0] && nodeCallsName(*fn->children[0], key);
        reg.add(key,
                [fn, key, selfRec, &reg, inProgress, estimates](const std::vector<ArgInfo> &args) {
            const bool        eligible = selfRec && fixpointEligible(args);
            const std::string ek       = eligible ? fixpointKey(key, args) : std::string();
            if (eligible) {
                const auto it = estimates->find(ek);
                if (it != estimates->end())
                    return it->second;  // same-signature self-call: the current fixpoint estimate
            }
            if (!inProgress->insert(key).second)         // mutual / polymorphic / ineligible re-entry
                return InferredType::dynamic();          // -> sound break (no infinite monomorphisation)
            InferredType result;
            if (eligible) {
                // Seed Bottom; iterate the body inference (the self-call reads the
                // estimate) until the return type stops changing. The sequence is
                // monotone-ascending (Bottom -> ... -> at most Dynamic), so it
                // converges within the lattice height; the cap -> Dynamic is a sound
                // over-approximation if it somehow does not.
                (*estimates)[ek] = InferredType::bottom();
                result           = InferredType::dynamic();  // default if not converged
                for (int iter = 0; iter < kFixpointCap; ++iter) {
                    const InferredType next = inferFunctionReturn(*fn, args, reg);
                    if (next == (*estimates)[ek]) { result = next; break; }
                    (*estimates)[ek] = next;
                }
                estimates->erase(ek);
            } else {
                result = inferFunctionReturn(*fn, args, reg);
            }
            inProgress->erase(key);
            return result;
        });
        reg.addMulti(key, [fn, key, &reg, inProgress](const std::vector<ArgInfo> &args)
                              -> std::vector<InferredType> {
            if (!inProgress->insert(key).second) return {};  // recursion -> empty (targets Dynamic)
            std::vector<InferredType> r = inferFunctionReturns(*fn, args, reg);
            inProgress->erase(key);
            return r;
        });
    }
}

} // namespace numkit::codegen
