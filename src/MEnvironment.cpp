// src/MEnvironment.cpp
#include "MEnvironment.hpp"

namespace numkit::m::m {

// ============================================================
// Environment — SBO helpers
// ============================================================
MValue *Environment::sboFind(const std::string &name)
{
    for (size_t i = 0; i < sboCount_; ++i)
        if (sbo_[i].name == name)
            return &sbo_[i].value;
    return nullptr;
}

const MValue *Environment::sboFind(const std::string &name) const
{
    for (size_t i = 0; i < sboCount_; ++i)
        if (sbo_[i].name == name)
            return &sbo_[i].value;
    return nullptr;
}

void Environment::sboSet(const std::string &name, MValue val)
{
    // If already overflowed, use map directly
    if (!vars_.empty()) {
        vars_[name] = std::move(val);
        return;
    }
    // Try to update existing slot
    for (size_t i = 0; i < sboCount_; ++i) {
        if (sbo_[i].name == name) {
            sbo_[i].value = std::move(val);
            return;
        }
    }
    // Try to add new slot
    if (sboCount_ < SBO_SLOTS) {
        sbo_[sboCount_].name = name;
        sbo_[sboCount_].value = std::move(val);
        sboCount_++;
        return;
    }
    // Overflow: migrate all SBO slots to map, then add new entry
    for (size_t i = 0; i < sboCount_; ++i) {
        vars_[std::move(sbo_[i].name)] = std::move(sbo_[i].value);
    }
    sboCount_ = 0;
    vars_[name] = std::move(val);
}

// ============================================================
// Environment
// ============================================================
Environment::Environment(Environment *parent, Environment *globalsEnv)
    : parent_(parent)
    , globalsEnv_(globalsEnv)
{}

Environment::Environment(std::shared_ptr<Environment> owningParent, Environment *globalsEnv)
    : parent_(owningParent.get())
    , owningParent_(std::move(owningParent))
    , globalsEnv_(globalsEnv)
{}

void Environment::set(const std::string &name, MValue val)
{
    if (hasGlobals_ && globals_.count(name) && globalsEnv_) {
        globalsEnv_->set(name, std::move(val));
    } else {
        sboSet(name, std::move(val));
    }
}

MValue *Environment::get(const std::string &name)
{
    if (hasGlobals_ && globals_.count(name) && globalsEnv_)
        return globalsEnv_->get(name);
    if (!vars_.empty()) {
        // Overflowed — use map only
        auto it = vars_.find(name);
        if (it != vars_.end())
            return &it->second;
    } else if (sboCount_ > 0) {
        MValue *v = sboFind(name);
        if (v)
            return v;
    }
    if (parent_)
        return parent_->get(name);
    return nullptr;
}

bool Environment::has(const std::string &name) const
{
    if (hasGlobals_ && globals_.count(name) && globalsEnv_)
        return globalsEnv_->get(name) != nullptr;
    if (!vars_.empty()) {
        if (vars_.count(name))
            return true;
    } else if (sboFind(name)) {
        return true;
    }
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
    if (!vars_.empty()) {
        auto it = vars_.find(name);
        return (it != vars_.end()) ? &it->second : nullptr;
    }
    return sboFind(name);
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
        fn(sbo_[i].name, sbo_[i].value);
    for (auto &[k, v] : vars_)
        fn(k, v);
}

std::shared_ptr<Environment> Environment::snapshot(std::shared_ptr<Environment> newParent,
                                                   Environment *gs) const
{
    std::shared_ptr<Environment> snappedParent;
    if (parent_ && parent_->parent_) {
        snappedParent = parent_->snapshot(newParent, gs);
    } else {
        snappedParent = newParent;
    }

    auto snap = std::make_shared<Environment>(std::move(snappedParent), gs);

    for (size_t i = 0; i < sboCount_; ++i) {
        snap->sbo_[snap->sboCount_].name = sbo_[i].name;
        snap->sbo_[snap->sboCount_].value = sbo_[i].value;
        snap->sboCount_++;
    }

    for (auto &[k, v] : vars_)
        snap->vars_[k] = v;

    snap->globals_ = globals_;
    snap->hasGlobals_ = hasGlobals_;

    return snap;
}

void Environment::remove(const std::string &name)
{
    // If this is a global variable, also remove from globalsEnv
    if (hasGlobals_ && globals_.count(name) && globalsEnv_) {
        globalsEnv_->remove(name);
    }

    for (size_t i = 0; i < sboCount_; ++i) {
        if (sbo_[i].name == name) {
            if (i < sboCount_ - 1)
                std::swap(sbo_[i], sbo_[sboCount_ - 1]);
            sboCount_--;
            globals_.erase(name);
            if (globals_.empty())
                hasGlobals_ = false;
            return;
        }
    }
    vars_.erase(name);
    globals_.erase(name);
    if (globals_.empty())
        hasGlobals_ = false;
}

void Environment::clearAll()
{
    sboCount_ = 0;
    vars_.clear();
    globals_.clear();
    hasGlobals_ = false;
}

void Environment::reset(Environment *parent, Environment *gs)
{
    for (size_t i = 0; i < sboCount_; ++i) {
        sbo_[i].name.clear();
        sbo_[i].value = MValue();
    }
    sboCount_ = 0;
    vars_.clear();
    globals_.clear();
    hasGlobals_ = false;
    parent_ = parent;
    owningParent_.reset();
    globalsEnv_ = gs;
}

std::vector<std::string> Environment::localNames() const
{
    std::vector<std::string> names;
    names.reserve(sboCount_ + vars_.size());
    for (size_t i = 0; i < sboCount_; ++i)
        names.push_back(sbo_[i].name);
    for (auto &[k, v] : vars_)
        names.push_back(k);
    return names;
}

} // namespace numkit::m::m