// include/MLabEnvironment.hpp
#pragma once

#include "MLabValue.hpp"
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace mlab {

class GlobalStore
{
public:
    void set(const std::string &name, MValue val);
    MValue *get(const std::string &name);
    const MValue *get(const std::string &name) const;
    void remove(const std::string &name);
    void clear();

private:
    std::unordered_map<std::string, MValue> vars_;
};

class Environment
{
public:
    // For normal execution: raw parent pointer (caller guarantees lifetime)
    explicit Environment(Environment *parent = nullptr, GlobalStore *globalStore = nullptr);

    // For snapshots/closures: owning parent
    explicit Environment(std::shared_ptr<Environment> owningParent, GlobalStore *globalStore);

    void set(const std::string &name, MValue val);
    MValue *get(const std::string &name);
    bool has(const std::string &name) const;

    void setLocal(const std::string &name, MValue val);
    MValue *getLocal(const std::string &name);

    // Fast path: get pointer for an existing variable, assign in-place.
    // Returns pointer to the MValue if found locally (single hash lookup).
    // Returns nullptr if not found locally.
    MValue *getLocalFast(const std::string &name);

    // Fast path: set an existing local variable without hash lookup for globals.
    // Caller must ensure the variable already exists locally.
    void setLocalFast(const std::string &name, MValue val);

    void declareGlobal(const std::string &name);
    bool isGlobal(const std::string &name) const;

    GlobalStore *globalStore() const { return globalStore_; }

    // Iterate over local variables only
    void forEachLocal(const std::function<void(const std::string &, const MValue &)> &fn) const;

    // Create a deep snapshot of this environment and its parent chain
    std::shared_ptr<Environment> snapshot(std::shared_ptr<Environment> newParent,
                                          GlobalStore *gs) const;

    void remove(const std::string &name);

    void clearAll();

    // Reset for reuse — clears all data and sets new parent/globalStore
    void reset(Environment *parent, GlobalStore *gs);

    std::vector<std::string> localNames() const;

private:
    // Small buffer: inline storage for first N variables (avoids unordered_map for small scopes)
    static constexpr size_t SBO_SLOTS = 8;
    struct Slot
    {
        std::string name;
        MValue value;
    };
    Slot sbo_[SBO_SLOTS];
    size_t sboCount_ = 0;

    // Overflow map for large scopes
    std::unordered_map<std::string, MValue> vars_;

    std::unordered_set<std::string> globals_;
    Environment *parent_ = nullptr;             // non-owning, for lookup
    std::shared_ptr<Environment> owningParent_; // owning, for snapshots only
    GlobalStore *globalStore_ = nullptr;
    bool hasGlobals_ = false;

    // Internal helpers
    MValue *sboFind(const std::string &name);
    const MValue *sboFind(const std::string &name) const;
    void sboSet(const std::string &name, MValue val);
};

} // namespace mlab