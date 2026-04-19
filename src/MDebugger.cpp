// src/MDebugger.cpp
#include "MDebugger.hpp"
#include "MBytecode.hpp"
#include "MEnvironment.hpp"

#include <algorithm>

namespace numkit {

// ============================================================
// StackFrame::variables
// ============================================================

std::vector<StackFrame::VarEntry> StackFrame::variables() const
{
    std::vector<VarEntry> result;

    if (env) {
        // TW frame — read from Environment
        for (auto &name : env->localNames()) {
            auto *val = env->getLocal(name);
            if (val && !val->isEmpty())
                result.push_back({name, val});
        }
    } else if (chunk && registers) {
        // VM frame — read from registers via varMap
        for (auto &[name, reg] : chunk->varMap) {
            if (reg < chunk->numRegisters) {
                const MValue &val = registers[reg];
                if (!val.isUnset() && !val.isDeleted())
                    result.push_back({name, &val});
            }
        }
    }

    return result;
}

// ============================================================
// DebugContext
// ============================================================

const MValue *DebugContext::getVariable(const std::string &name) const
{
    auto *frame = currentFrame();
    if (!frame)
        return nullptr;

    auto vars = frame->variables();
    for (auto &v : vars)
        if (v.name == name)
            return v.value;
    return nullptr;
}

std::vector<std::string> DebugContext::variableNames() const
{
    std::vector<std::string> result;
    auto *frame = currentFrame();
    if (!frame)
        return result;

    for (auto &v : frame->variables())
        result.push_back(v.name);

    std::sort(result.begin(), result.end());
    return result;
}

// ============================================================
// BreakpointManager
// ============================================================

int BreakpointManager::addBreakpoint(uint16_t line)
{
    int id = nextId_++;
    breakpoints_.push_back({id, line, true});
    indexDirty_ = true;
    return id;
}

void BreakpointManager::removeBreakpoint(int id)
{
    breakpoints_.erase(std::remove_if(breakpoints_.begin(),
                                      breakpoints_.end(),
                                      [id](const Breakpoint &bp) { return bp.id == id; }),
                       breakpoints_.end());
    indexDirty_ = true;
}

void BreakpointManager::enableBreakpoint(int id, bool enabled)
{
    for (auto &bp : breakpoints_) {
        if (bp.id == id) {
            bp.enabled = enabled;
            indexDirty_ = true;
            return;
        }
    }
}

void BreakpointManager::clearAll()
{
    breakpoints_.clear();
    lineIndex_.clear();
    indexDirty_ = false;
}

bool BreakpointManager::shouldBreak(uint16_t line) const
{
    if (breakpoints_.empty())
        return false;
    if (indexDirty_)
        rebuildIndex();
    auto it = lineIndex_.find(line);
    return it != lineIndex_.end();
}

void BreakpointManager::rebuildIndex() const
{
    lineIndex_.clear();
    for (size_t i = 0; i < breakpoints_.size(); ++i)
        if (breakpoints_[i].enabled)
            lineIndex_[breakpoints_[i].line] = i;
    indexDirty_ = false;
}

// ============================================================
// DebugController
// ============================================================

void DebugController::reset(DebugAction initialAction)
{
    callStack_.clear();
    stepAction_ = initialAction;
    stepDepth_ = 0;
    lastLine_ = 0;
}

bool DebugController::checkLine(uint16_t line, uint16_t col, int callDepth)
{
    if (line == 0 || line == lastLine_)
        return true;

    lastLine_ = line;

    // Update current frame's position
    if (auto *frame = currentFrame()) {
        frame->line = line;
        frame->col = col;
    }

    DebugContext ctx = makeContext(line, col);
    DebugAction action = DebugAction::Continue;

    // Breakpoints have priority
    if (bpm_ && bpm_->shouldBreak(line)) {
        action = observer_->onBreakpoint(ctx);
    } else {
        // Step mode
        switch (stepAction_) {
        case DebugAction::StepInto:
            action = observer_->onLine(ctx);
            break;
        case DebugAction::StepOver:
            if (callDepth <= stepDepth_)
                action = observer_->onLine(ctx);
            break;
        case DebugAction::StepOut:
            if (callDepth < stepDepth_)
                action = observer_->onLine(ctx);
            break;
        case DebugAction::Continue:
        case DebugAction::Stop:
            break;
        }
    }

    applyAction(action, callDepth);
    return action != DebugAction::Stop;
}

void DebugController::pushFrame(StackFrame frame)
{
    callStack_.push_back(std::move(frame));

    DebugContext ctx = makeContext();
    observer_->onFunctionEntry(ctx);
}

void DebugController::popFrame()
{
    if (callStack_.empty())
        return;

    DebugContext ctx = makeContext();
    observer_->onFunctionExit(ctx);

    callStack_.pop_back();

    // Reset line tracking — returning to caller may land on the same line
    // number that was last seen, and we need the hook to fire.
    lastLine_ = 0;
}

DebugContext DebugController::makeContext(uint16_t line, uint16_t col) const
{
    DebugContext ctx;
    ctx.line = line;
    ctx.col = col;
    ctx.callStack = &callStack_;
    if (!callStack_.empty()) {
        ctx.functionName = &callStack_.back().functionName;
        if (callStack_.back().chunk && callStack_.back().chunk->sourceCode)
            ctx.sourceCode = callStack_.back().chunk->sourceCode.get();
    }
    return ctx;
}

DebugContext DebugController::makeContext() const
{
    if (callStack_.empty())
        return makeContext(0, 0);
    auto &frame = callStack_.back();
    return makeContext(frame.line, frame.col);
}

void DebugController::setResumeAction(DebugAction action, int callDepth)
{
    applyAction(action, callDepth);
    // Don't reset lastLine_ — we want the current pause line to be skipped
    // so we don't immediately re-trigger on the same line.
}

void DebugController::applyAction(DebugAction action, int callDepth)
{
    switch (action) {
    case DebugAction::Continue:
        stepAction_ = DebugAction::Continue;
        break;
    case DebugAction::StepInto:
        stepAction_ = DebugAction::StepInto;
        break;
    case DebugAction::StepOver:
        stepAction_ = DebugAction::StepOver;
        stepDepth_ = callDepth;
        break;
    case DebugAction::StepOut:
        stepAction_ = DebugAction::StepOut;
        stepDepth_ = callDepth;
        break;
    case DebugAction::Stop:
        stepAction_ = DebugAction::Stop;
        break;
    }
}

} // namespace numkit