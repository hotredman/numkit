// src/MLabEnvironment.cpp
#include "MLabEnvironment.hpp"

namespace mlab {

// ============================================================
// GlobalStore
// ============================================================
void GlobalStore::set(const std::string &name, MValue val)
{
    vars_[name] = std::move(val);
}

MValue *GlobalStore::get(const std::string &name)
{
    auto it = vars_.find(name);
    return (it != vars_.end()) ? &it->second : nullptr;
}

const MValue *GlobalStore::get(const std::string &name) const
{
    auto it = vars_.find(name);
    return (it != vars_.end()) ? &it->second : nullptr;
}

// ============================================================
// Environment — SBO helpers
// ============================================================
MValue *Environment::sboFind(const std::string &name)
{
    for (size_t i = 0; i < sboCount_; ++i)
        if (sbo_[i].used && sbo_[i].name == name)
            return &sbo_[i].value;
    return nullptr;
}

const MValue *Environment::sboFind(const std::string &name) const
{
    for (size_t i = 0; i < sboCount_; ++i)
        if (sbo_[i].used && sbo_[i].name == name)
            return &sbo_[i].value;
    return nullptr;
}

void Environment::sboSet(const std::string &name, MValue val)
{
    // Try to update existing slot
    for (size_t i = 0; i < sboCount_; ++i) {
        if (sbo_[i].used && sbo_[i].name == name) {
            sbo_[i].value = std::move(val);
            return;
        }
    }
    // Try to add new slot
    if (sboCount_ < SBO_SLOTS) {
        sbo_[sboCount_].name = name;
        sbo_[sboCount_].value = std::move(val);
        sbo_[sboCount_].used = true;
        sboCount_++;
        return;
    }
    // Overflow to map
    vars_[name] = std::move(val);
}

// ============================================================
// Environment
// ============================================================
Environment::Environment(std::shared_ptr<Environment> parent, GlobalStore *globalStore)
    : parent_(std::move(parent))
    , globalStore_(globalStore)
{}

void Environment::set(const std::string &name, MValue val)
{
    if (hasGlobals_ && globals_.count(name) && globalStore_) {
        globalStore_->set(name, std::move(val));
    } else {
        sboSet(name, std::move(val));
    }
}

MValue *Environment::get(const std::string &name)
{
    if (hasGlobals_ && globals_.count(name) && globalStore_)
        return globalStore_->get(name);
    MValue *v = sboFind(name);
    if (v)
        return v;
    auto it = vars_.find(name);
    if (it != vars_.end())
        return &it->second;
    if (parent_)
        return parent_->get(name);
    return nullptr;
}

bool Environment::has(const std::string &name) const
{
    if (hasGlobals_ && globals_.count(name) && globalStore_)
        return globalStore_->get(name) != nullptr;
    if (sboFind(name))
        return true;
    if (vars_.count(name))
        return true;
    if (parent_)
        return parent_->has(name);
    return false;
}

void Environment::setLocal(const std::string &name, MValue val)
{
    sboSet(name, std::move(val));
}

MValue *Environment::getLocal(const std::string &name)
{
    MValue *v = sboFind(name);
    if (v)
        return v;
    auto it = vars_.find(name);
    return (it != vars_.end()) ? &it->second : nullptr;
}

MValue *Environment::getLocalFast(const std::string &name)
{
    return getLocal(name);
}

void Environment::setLocalFast(const std::string &name, MValue val)
{
    sboSet(name, std::move(val));
}

void Environment::declareGlobal(const std::string &name)
{
    globals_.insert(name);
    hasGlobals_ = true;
}

bool Environment::isGlobal(const std::string &name) const
{
    return hasGlobals_ && globals_.count(name) > 0;
}

void Environment::forEachLocal(
    const std::function<void(const std::string &, const MValue &)> &fn) const
{
    for (size_t i = 0; i < sboCount_; ++i)
        if (sbo_[i].used)
            fn(sbo_[i].name, sbo_[i].value);
    for (auto &[k, v] : vars_)
        fn(k, v);
}

std::shared_ptr<Environment> Environment::snapshot(std::shared_ptr<Environment> newParent,
                                                   GlobalStore *gs) const
{
    std::shared_ptr<Environment> snappedParent;
    if (parent_ && parent_->parent_) {
        snappedParent = parent_->snapshot(newParent, gs);
    } else {
        snappedParent = newParent;
    }

    auto snap = std::make_shared<Environment>(std::move(snappedParent), gs);

    for (size_t i = 0; i < sboCount_; ++i) {
        if (sbo_[i].used) {
            snap->sbo_[snap->sboCount_].name = sbo_[i].name;
            snap->sbo_[snap->sboCount_].value = sbo_[i].value;
            snap->sbo_[snap->sboCount_].used = true;
            snap->sboCount_++;
        }
    }

    for (auto &[k, v] : vars_)
        snap->vars_[k] = v;

    snap->globals_ = globals_;
    snap->hasGlobals_ = hasGlobals_;

    return snap;
}

void Environment::remove(const std::string &name)
{
    for (size_t i = 0; i < sboCount_; ++i) {
        if (sbo_[i].used && sbo_[i].name == name) {
            sbo_[i].used = false;
            if (i < sboCount_ - 1)
                std::swap(sbo_[i], sbo_[sboCount_ - 1]);
            sboCount_--;
            globals_.erase(name);
            return;
        }
    }
    vars_.erase(name);
    globals_.erase(name);
}

void Environment::clearAll()
{
    for (size_t i = 0; i < sboCount_; ++i)
        sbo_[i].used = false;
    sboCount_ = 0;
    vars_.clear();
    globals_.clear();
    hasGlobals_ = false;
}

std::vector<std::string> Environment::localNames() const
{
    std::vector<std::string> names;
    names.reserve(sboCount_ + vars_.size());
    for (size_t i = 0; i < sboCount_; ++i)
        if (sbo_[i].used)
            names.push_back(sbo_[i].name);
    for (auto &[k, v] : vars_)
        names.push_back(k);
    return names;
}

} // namespace mlab