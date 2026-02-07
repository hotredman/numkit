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
// Environment
// ============================================================
Environment::Environment(std::shared_ptr<Environment> parent, GlobalStore *globalStore)
    : parent_(std::move(parent))
    , globalStore_(globalStore)
{}

void Environment::set(const std::string &name, MValue val)
{
    if (globals_.count(name) && globalStore_) {
        globalStore_->set(name, std::move(val));
    } else {
        vars_[name] = std::move(val);
    }
}

MValue *Environment::get(const std::string &name)
{
    if (globals_.count(name) && globalStore_)
        return globalStore_->get(name);
    auto it = vars_.find(name);
    if (it != vars_.end())
        return &it->second;
    if (parent_)
        return parent_->get(name);
    return nullptr;
}

bool Environment::has(const std::string &name) const
{
    if (globals_.count(name) && globalStore_)
        return globalStore_->get(name) != nullptr;
    if (vars_.count(name))
        return true;
    if (parent_)
        return parent_->has(name);
    return false;
}

void Environment::setLocal(const std::string &name, MValue val)
{
    vars_[name] = std::move(val);
}

MValue *Environment::getLocal(const std::string &name)
{
    auto it = vars_.find(name);
    return (it != vars_.end()) ? &it->second : nullptr;
}

void Environment::declareGlobal(const std::string &name)
{
    globals_.insert(name);
}

bool Environment::isGlobal(const std::string &name) const
{
    return globals_.count(name) > 0;
}

void Environment::forEachLocal(
    const std::function<void(const std::string &, const MValue &)> &fn) const
{
    for (auto &[k, v] : vars_)
        fn(k, v);
}

std::shared_ptr<Environment> Environment::snapshot(std::shared_ptr<Environment> newParent,
                                                   GlobalStore *gs) const
{
    // Рекурсивно снимаем snapshot parent chain
    // Останавливаемся когда parent_ == nullptr (дошли до корня)
    // или parent_ не имеет своего parent (это globalEnv)
    std::shared_ptr<Environment> snappedParent;
    if (parent_ && parent_->parent_) {
        // Промежуточный scope — рекурсивно снимаем snapshot
        snappedParent = parent_->snapshot(newParent, gs);
    } else {
        // parent_ это globalEnv или nullptr — используем переданный newParent
        snappedParent = newParent;
    }

    auto snap = std::make_shared<Environment>(std::move(snappedParent), gs);

    // Копируем все локальные переменные (deep copy через MValue copy ctor)
    for (auto &[k, v] : vars_)
        snap->vars_[k] = v;

    // Копируем объявления global
    snap->globals_ = globals_;

    return snap;
}

void Environment::remove(const std::string &name)
{
    vars_.erase(name);
    globals_.erase(name);
}

void Environment::clearAll()
{
    vars_.clear();
    globals_.clear();
}

std::vector<std::string> Environment::localNames() const
{
    std::vector<std::string> names;
    names.reserve(vars_.size());
    for (auto &[k, v] : vars_)
        names.push_back(k);
    return names;
}

} // namespace mlab
