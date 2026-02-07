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

private:
    std::unordered_map<std::string, MValue> vars_;
};

class Environment
{
public:
    explicit Environment(std::shared_ptr<Environment> parent = nullptr,
                         GlobalStore *globalStore = nullptr);

    void set(const std::string &name, MValue val);
    MValue *get(const std::string &name);
    bool has(const std::string &name) const;

    void setLocal(const std::string &name, MValue val);
    MValue *getLocal(const std::string &name);

    void declareGlobal(const std::string &name);
    bool isGlobal(const std::string &name) const;

    GlobalStore *globalStore() const { return globalStore_; }

    // Iterate over local variables only
    void forEachLocal(const std::function<void(const std::string &, const MValue &)> &fn) const;

    // Create a deep snapshot of this environment and its parent chain
    // (up to but not including the globalEnv root).
    // The snapshot captures all variable values at this moment.
    std::shared_ptr<Environment> snapshot(std::shared_ptr<Environment> newParent,
                                          GlobalStore *gs) const;

    void remove(const std::string &name);

    void clearAll();

    std::vector<std::string> localNames() const;

private:
    std::unordered_map<std::string, MValue> vars_;
    std::unordered_set<std::string> globals_;
    std::shared_ptr<Environment> parent_;
    GlobalStore *globalStore_ = nullptr;
};

} // namespace mlab
