// include/environment.hpp
#pragma once

#include <numkit/value/value.hpp>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace numkit {

// A single `import` declaration active in the current scope. See
// NAMESPACE_DESIGN.md Sections 3-4 for resolution semantics.
struct Import
{
    std::vector<std::string> path;  // ["signal", "transforms"] for `import signal.transforms.*`
    bool wildcard = false;          // true for `.*` form
    std::string alias;              // for `import x.y as alias`; "" otherwise
    // For non-wildcard, non-alias single-symbol form (`import a.b.c`),
    // path = [a, b, c] and the imported leaf name is path.back().
};

class Environment
{
public:
    // For normal execution: raw parent pointer (caller guarantees lifetime)
    explicit Environment(Environment *parent = nullptr, Environment *globalsEnv = nullptr);

    // For snapshots/closures: owning parent
    explicit Environment(std::shared_ptr<Environment> owningParent, Environment *globalsEnv);

    void set(const std::string &name, Value val);
    Value *get(const std::string &name);
    bool has(const std::string &name) const;

    void setLocal(const std::string &name, Value val);
    Value *getLocal(const std::string &name);

    // Fast path: get pointer for an existing variable, assign in-place.
    // Returns pointer to the Value if found locally (single hash lookup).
    // Returns nullptr if not found locally.
    Value *getLocalFast(const std::string &name);

    // Fast path: set an existing local variable without hash lookup for globals.
    // Caller must ensure the variable already exists locally.
    void setLocalFast(const std::string &name, Value val);

    void declareGlobal(const std::string &name);
    bool isGlobal(const std::string &name) const;

    // Names this environment has declared `global`. The VALUE of each lives in
    // globalsEnv_ (get/set delegate there), never in this env's local storage —
    // so localNames() and globalNames() are disjoint. This set IS the base
    // workspace's global membership when called on the engine's workspaceEnv.
    const std::unordered_set<std::string> &globalNames() const { return globals_; }

    Environment *globalsEnv() const { return globalsEnv_; }

    // Iterate over local variables only
    void forEachLocal(const std::function<void(const std::string &, const Value &)> &fn) const;

    // Create a deep snapshot of this environment and its parent chain
    std::shared_ptr<Environment> snapshot(std::shared_ptr<Environment> newParent,
                                          Environment *gs) const;

    void remove(const std::string &name);

    void clearAll(bool keepImports = false);

    // Reset for reuse — clears all data and sets new parent/globalsEnv
    void reset(Environment *parent, Environment *gs);

    std::vector<std::string> localNames() const;

    // ── Active imports (scope-local) ──────────────────────────
    // Imports added via `import` statements at this scope. Lookups
    // check this scope's imports first, then walk parent_ chain.
    void pushImport(Import imp) { activeImports_.push_back(std::move(imp)); }
    const std::vector<Import> &activeImports() const { return activeImports_; }
    void clearImports() { activeImports_.clear(); }
    Environment *parentForImports() const { return parent_; }

private:
    // Small buffer: inline storage for first N variables (avoids unordered_map for small scopes)
    static constexpr size_t SBO_SLOTS = 8;
    struct Slot
    {
        std::string name;
        Value value;
    };
    Slot sbo_[SBO_SLOTS];
    size_t sboCount_ = 0;

    // Overflow map for large scopes
    std::unordered_map<std::string, Value> vars_;

    std::unordered_set<std::string> globals_;
    std::vector<Import> activeImports_;
    Environment *parent_ = nullptr;             // non-owning, for lookup
    std::shared_ptr<Environment> owningParent_; // owning, for snapshots only
    Environment *globalsEnv_ = nullptr;
    bool hasGlobals_ = false;

    // Internal helpers
    Value *sboFind(const std::string &name);
    const Value *sboFind(const std::string &name) const;
    void sboSet(const std::string &name, Value val);
};

} // namespace numkit