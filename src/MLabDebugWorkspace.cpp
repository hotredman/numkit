// src/MLabDebugWorkspace.cpp
#include "MLabDebugWorkspace.hpp"
#include "MLabBytecode.hpp"
#include "MLabTypes.hpp"
#include "MLabVM.hpp"

namespace mlab {

// Names that don't qualify as user variables by default. `kBuiltinNames` is
// the union of numeric constants (pi, eps, …) and runtime pseudo-vars
// (nargin, nargout, ans, end). They all end up in the chunk's varMap, but
// the debug snapshot only lists them once the user has actually shadowed
// them — matching MATLAB's `whos` at K>>.
static bool isHiddenName(const std::string &name)
{
    return kBuiltinNames.count(name) > 0;
}

void DebugWorkspace::bindVMFrame(VM &vm)
{
    framePtrs_.clear();
    assignedInFrame_.clear();

    auto frame = vm.currentFrameView();
    if (!frame.chunk || !frame.registers)
        return;

    // Bind all names in varMap — including pseudo-vars (nargin, nargout) and
    // constants (pi, eps, …). They must be reachable by debug-console eval
    // even if we don't show them in the snapshot.
    for (auto &[name, reg] : frame.chunk->varMap) {
        if (reg < frame.chunk->numRegisters)
            framePtrs_[name] = &frame.registers[reg];
    }

    // Copy the chunk's "assigned" set — the compiler records every name it
    // emits a write for. Built-ins gain visibility only when this set (or
    // the runtime shadow set) contains them.
    assignedInFrame_ = frame.chunk->assignedVars;
}

void DebugWorkspace::unbindFrame()
{
    framePtrs_.clear();
    assignedInFrame_.clear();
}

void DebugWorkspace::reset()
{
    framePtrs_.clear();
    assignedInFrame_.clear();
    overlay_.clear();
    shadowedBuiltins_.clear();
}

const MValue *DebugWorkspace::get(const std::string &name) const
{
    auto it = framePtrs_.find(name);
    if (it != framePtrs_.end()) {
        const MValue *p = it->second;
        if (p && !p->isUnset() && !p->isDeleted())
            return p;
        // Frame slot is cleared — fall through to overlay in case the
        // console later recreated it. (Usually overlay won't hold it, but
        // this keeps the API symmetric.)
    }
    auto ov = overlay_.find(name);
    if (ov != overlay_.end() && !ov->second.isUnset() && !ov->second.isDeleted())
        return &ov->second;
    return nullptr;
}

void DebugWorkspace::set(const std::string &name, const MValue &val)
{
    // Console write to a built-in name is an explicit user-level shadow —
    // MATLAB shows it in `whos` after this point.
    if (isHiddenName(name))
        shadowedBuiltins_.insert(name);

    auto it = framePtrs_.find(name);
    if (it != framePtrs_.end()) {
        *it->second = val;
        // Writing through a frame slot shadows any same-named overlay entry.
        overlay_.erase(name);
        return;
    }
    overlay_[name] = val;
}

void DebugWorkspace::remove(const std::string &name)
{
    auto it = framePtrs_.find(name);
    if (it != framePtrs_.end())
        *it->second = MValue::deleted();
    overlay_.erase(name);
    // Symmetry with set(): removing a built-in via the console un-shadows it.
    shadowedBuiltins_.erase(name);
}

void DebugWorkspace::clearAll()
{
    for (auto &[name, ptr] : framePtrs_)
        *ptr = MValue::deleted();
    overlay_.clear();
}

std::vector<std::string> DebugWorkspace::names() const
{
    std::vector<std::string> result;
    result.reserve(framePtrs_.size() + overlay_.size());

    // MATLAB-matched visibility rule:
    //   - non-builtin names are shown whenever they hold a live value;
    //   - built-in names (pi, nargin, …) are shown only if they have been
    //     shadowed by either the script (tracked via assignedInFrame_) or
    //     by the debug console (tracked via shadowedBuiltins_).
    auto builtinVisible = [&](const std::string &name) {
        return assignedInFrame_.count(name) > 0 || shadowedBuiltins_.count(name) > 0;
    };

    for (auto &[name, ptr] : framePtrs_) {
        if (!ptr || ptr->isUnset() || ptr->isDeleted())
            continue;
        if (isHiddenName(name) && !builtinVisible(name))
            continue;
        result.push_back(name);
    }

    for (auto &[name, val] : overlay_) {
        if (val.isUnset() || val.isDeleted() || framePtrs_.count(name))
            continue;
        if (isHiddenName(name) && !builtinVisible(name))
            continue;
        result.push_back(name);
    }

    return result;
}

std::vector<std::string> DebugWorkspace::allNames() const
{
    std::vector<std::string> result;
    result.reserve(framePtrs_.size() + overlay_.size());

    for (auto &[name, ptr] : framePtrs_)
        if (ptr && !ptr->isUnset() && !ptr->isDeleted())
            result.push_back(name);

    for (auto &[name, val] : overlay_)
        if (!val.isUnset() && !val.isDeleted() && !framePtrs_.count(name))
            result.push_back(name);

    return result;
}

} // namespace mlab
