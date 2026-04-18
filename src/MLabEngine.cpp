// src/MLabEngine.cpp
#include "MLabEngine.hpp"
#include "MLabCompiler.hpp"
#include "MLabLexer.hpp"
#include "MLabParser.hpp"
#include "MLabStdLibrary.hpp"
#include "MLabDspLibrary.hpp"
#include "MLabPltLibrary.hpp"
#include "MLabFitLibrary.hpp"
#include "MLabTreeWalker.hpp"
#include "MLabVM.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <sstream>
#include <unordered_set>

namespace mlab {

// ============================================================
// Reserved names — see MLabTypes.hpp for per-set semantics.
// ============================================================
const std::unordered_set<std::string> kBuiltinConstants = {
    "pi", "eps", "inf", "Inf", "nan", "NaN", "true", "false", "i", "j",
};

const std::unordered_set<std::string> kPseudoVars = {
    "ans", "nargin", "nargout", "end",
};

// Union kept as a named constant so existing filter sites keep reading
// naturally ("is this any reserved name?"). Initialised at static-init
// time — order within this TU doesn't matter since both operands above
// are defined first.
static std::unordered_set<std::string> makeBuiltinNamesUnion()
{
    std::unordered_set<std::string> u = kBuiltinConstants;
    u.insert(kPseudoVars.begin(), kPseudoVars.end());
    return u;
}
const std::unordered_set<std::string> kBuiltinNames = makeBuiltinNamesUnion();

// ============================================================
// Construction
// ============================================================
Engine::Engine()
{
    allocator_ = Allocator::defaultAllocator();
    globalsEnv_ = std::make_unique<Environment>();
    constantsEnv_ = std::make_unique<Environment>(nullptr, globalsEnv_.get());
    workspaceEnv_ = std::make_unique<Environment>(constantsEnv_.get(), globalsEnv_.get());
    treeWalker_ = std::make_unique<TreeWalker>(*this);
    compiler_ = std::make_unique<Compiler>(*this);
    vm_ = std::make_unique<VM>(*this);

    reinstallConstants();
    StdLibrary::install(*this);
    DspLibrary::install(*this);
    PltLibrary::install(*this);
    FitLibrary::install(*this);
}

Engine::~Engine() = default;

void Engine::reinstallConstants()
{
    constantsEnv_->set("pi", MValue::scalar(3.14159265358979323846, &allocator_));
    constantsEnv_->set("eps", MValue::scalar(2.2204460492503131e-16, &allocator_));
    constantsEnv_->set("inf", MValue::scalar(std::numeric_limits<double>::infinity(), &allocator_));
    constantsEnv_->set("Inf", MValue::scalar(std::numeric_limits<double>::infinity(), &allocator_));
    constantsEnv_->set("nan", MValue::scalar(std::numeric_limits<double>::quiet_NaN(), &allocator_));
    constantsEnv_->set("NaN", MValue::scalar(std::numeric_limits<double>::quiet_NaN(), &allocator_));
    constantsEnv_->set("true", MValue::logicalScalar(true, &allocator_));
    constantsEnv_->set("false", MValue::logicalScalar(false, &allocator_));
    constantsEnv_->set("i", MValue::complexScalar(0.0, 1.0, &allocator_));
    constantsEnv_->set("j", MValue::complexScalar(0.0, 1.0, &allocator_));

    // Re-install host-registered constants so they survive `clear all`.
    for (auto &[name, val] : userConstants_)
        constantsEnv_->set(name, val);
}

void Engine::registerConstant(const std::string &name, MValue val)
{
    userConstants_[name] = val;
    constantsEnv_->set(name, std::move(val));
}

bool Engine::isReservedName(const std::string &name) const
{
    return kBuiltinNames.count(name) > 0 || userConstants_.count(name) > 0;
}

// ============================================================
// Registration & accessors
// ============================================================
void Engine::setAllocator(Allocator alloc)
{
    allocator_ = std::move(alloc);
}
Allocator &Engine::allocator()
{
    return allocator_;
}

void Engine::registerBinaryOp(const std::string &op, BinaryOpFunc func)
{
    binaryOps_[op] = std::move(func);
}
void Engine::registerUnaryOp(const std::string &op, UnaryOpFunc func)
{
    unaryOps_[op] = std::move(func);
}
void Engine::registerFunction(const std::string &name, ExternalFunc func)
{
    externalFuncs_[name] = std::move(func);
}

void Engine::setVariable(const std::string &name, MValue val)
{
    workspaceEnv_->set(name, std::move(val));
}
MValue *Engine::getVariable(const std::string &name)
{
    // Check globalsEnv first (for global variables set by functions)
    MValue *gs = globalsEnv_->get(name);
    if (gs && !gs->isUnset()) {
        // Sync to workspaceEnv if different
        MValue *ge = workspaceEnv_->get(name);
        if (!ge || ge->isUnset() || ge != gs)
            workspaceEnv_->set(name, *gs);
        return workspaceEnv_->get(name);
    }
    return workspaceEnv_->get(name);
}

void Engine::setOutputFunc(OutputFunc f)
{
    outputFunc_ = f;
    figureManager_.setOutputFunc(std::move(f));
}

void Engine::setMaxRecursionDepth(int d)
{
    treeWalker_->setMaxRecursionDepth(d);
    vm_->setMaxRecursionDepth(d);
}

void Engine::outputText(const std::string &s)
{
    if (outputFunc_)
        outputFunc_(s);
    else
        std::cout << s;
}

bool Engine::hasFunction(const std::string &name) const
{
    return externalFuncs_.count(name) || userFuncs_.count(name);
}

bool Engine::hasUserFunction(const std::string &name) const
{
    return userFuncs_.count(name) > 0;
}

bool Engine::hasExternalFunction(const std::string &name) const
{
    return externalFuncs_.count(name) > 0;
}

bool Engine::isInsideFunctionCall() const
{
    if (vm_ && backend_ == Backend::VM)
        return vm_->callDepth() > 0;
    if (treeWalker_)
        return treeWalker_->callDepth() > 0;
    return false;
}

void Engine::clearUserFunctions()
{
    userFuncs_.clear();
    if (compiler_)
        compiler_->clearCompiledFuncs();
}

void Engine::setDebugObserver(std::shared_ptr<DebugObserver> observer)
{
    debugObserver_ = std::move(observer);
    if (debugObserver_)
        debugController_ = std::make_unique<DebugController>(debugObserver_.get(), &breakpointManager_);
    else
        debugController_.reset();
}

// ============================================================
// eval
// ============================================================

// Compile one AST subtree into a chunk and run it on the VM, syncing
// modified registers to workspaceEnv before returning. Used by eval() when
// executing a single statement (or a whole single-expression chunk).
MValue Engine::runOneChunk(const ASTNode *ast, std::shared_ptr<const std::string> src)
{
    clearAllCalled_ = false;
    vm_->clearLastVarMap();

    auto chunk = compiler_->compile(ast, src);
    vm_->setCompiledFuncs(&compiler_->compiledFuncs());

    // Remember any `global X` declarations from this chunk so the next
    // chunk's compile can see them (split-mode top-level globals).
    auto updateTopLevelGlobals = [&]() {
        for (auto &g : chunk.globalNames)
            topLevelGlobals_.insert(g);
    };

    try {
        MValue result = vm_->execute(chunk);
        syncVMToWorkspace();
        updateTopLevelGlobals();
        return result;
    } catch (const DebugStopException &) {
        syncVMToWorkspace();
        updateTopLevelGlobals();
        throw;
    } catch (...) {
        syncVMToWorkspace();
        updateTopLevelGlobals();
        throw;
    }
}

MValue Engine::eval(const std::string &code)
{
    Lexer lexer(code);
    auto tokens = lexer.tokenize();
    Parser parser(tokens);
    auto ast = parser.parse();
    auto src = std::make_shared<const std::string>(code);

    // TreeWalker already executes top-level BLOCK statements sequentially
    // against `workspaceEnv_`, so its behaviour matches MATLAB's script
    // semantics out of the box — no split needed here.
    if (backend_ != Backend::VM)
        return treeWalker_->execute(ast.get(), workspaceEnv_.get());

    // VM: a whole multi-statement script compiled as a single chunk keeps
    // its variables in chunk-local registers and only commits them to
    // workspaceEnv on completion. That leaves mid-script `whos` / `clear x`
    // blind to the running state. Match MATLAB by executing every top-level
    // statement as its own mini-chunk, with a sync in between.
    //
    // Function definitions are pre-registered across the whole BLOCK so
    // statements earlier in the file can call functions declared later.
    // They are also *re-*registered after every statement so that a
    // `clear all` in the middle of a script (which wipes
    // engine.userFuncs_) cannot make the script's own local functions
    // disappear from under the calls that follow — MATLAB's local
    // script functions are always in scope for the script that
    // defines them.
    //
    // An attached debug observer runs the whole eval as one chunk: the
    // observer expects step/line semantics to correspond to the source as
    // a unit, and a split would re-fire initial-stop events between every
    // top-level statement.
    const bool splittable = !debugObserver_ && ast
                            && ast->type == NodeType::BLOCK
                            && ast->children.size() > 1;
    if (splittable) {
        std::vector<const ASTNode *> funcDefs;
        for (auto &c : ast->children) {
            if (c && c->type == NodeType::FUNCTION_DEF)
                funcDefs.push_back(c.get());
        }
        auto ensureFunctions = [&]() {
            for (const ASTNode *f : funcDefs) {
                if (!hasUserFunction(f->strValue))
                    compiler_->registerFunction(f);
            }
        };
        ensureFunctions();
        MValue result = MValue::empty();
        for (auto &c : ast->children) {
            if (!c || c->type == NodeType::FUNCTION_DEF)
                continue;
            result = runOneChunk(c.get(), src);
            // `clear all` / `clear functions` in the previous statement
            // may have emptied userFuncs_ — restore script-local ones.
            ensureFunctions();
        }
        return result;
    }

    // Single-statement path: works for REPL lines, lone expressions, and
    // scripts consisting of just one top-level construct.
    return runOneChunk(ast.get(), src);
}

// ============================================================
// evalSafe
// ============================================================
Engine::EvalResult Engine::evalSafe(const std::string &code)
{
    EvalResult r;
    try {
        r.value = eval(code);
    } catch (const DebugStopException &) {
        r.ok = false;
        r.debugStop = true;
    } catch (const MLabError &e) {
        r.ok = false;
        r.errorMessage = e.what();
        r.errorLine = e.line();
        r.errorCol = e.col();
        r.errorFunc = e.funcName();
        r.errorContext = e.context();
    } catch (const std::exception &e) {
        r.ok = false;
        r.errorMessage = e.what();
    } catch (...) {
        r.ok = false;
        r.errorMessage = "Unknown exception";
    }
    return r;
}

// ============================================================
// VM → workspaceEnv sync
// ============================================================
ExecStatus Engine::debugResume(DebugAction action)
{
    if (!vm_ || !vm_->isPaused())
        return ExecStatus::Completed;

    // Set the resume action on the debug controller
    if (debugController_)
        debugController_->setResumeAction(action, vm_->callDepth());

    ExecStatus status = vm_->resumeExecution();

    // Sync variables on completion
    if (status == ExecStatus::Completed)
        syncVMToWorkspace();

    return status;
}

void Engine::syncVMToWorkspace()
{
    if (clearAllCalled_)
        workspaceEnv_->clearAll();
    for (auto &[name, val] : vm_->lastVarMap()) {
        if (val.isUnset() || val.isDeleted()) {
            workspaceEnv_->remove(name);
        } else {
            MValue *gsVal = globalsEnv_->get(name);
            workspaceEnv_->set(name, gsVal ? *gsVal : val);
        }
    }
}

// ============================================================
// REPL helpers
// ============================================================
std::vector<std::string> Engine::workspaceVarNames() const
{
    // `localNames()` only returns variables that were written into the base
    // workspace — built-in constants live in `constantsEnv_` (a parent env)
    // and don't appear here unless the user has explicitly shadowed them,
    // which matches MATLAB's `who`/`whos` behaviour.
    auto names = workspaceEnv_->localNames();
    std::sort(names.begin(), names.end());
    return names;
}

static std::string jsonEscape(const std::string &s)
{
    std::string out;
    out.reserve(s.size() + 4);
    for (char c : s) {
        if (c == '"')
            out += "\\\"";
        else if (c == '\\')
            out += "\\\\";
        else if (c == '\n')
            out += "\\n";
        else if (c == '\t')
            out += "\\t";
        else
            out += c;
    }
    return out;
}

std::string Engine::workspaceJSON() const
{
    auto names = workspaceVarNames();
    std::ostringstream os;
    os << "{";
    bool first = true;
    for (auto &name : names) {
        auto *val = workspaceEnv_->get(name);
        if (!val)
            continue;
        if (!first)
            os << ",";
        first = false;
        os << "\"" << jsonEscape(name) << "\":{";
        os << "\"type\":\"" << mtypeName(val->type()) << "\"";
        auto &d = val->dims();
        os << ",\"size\":\"" << d.rows() << "x" << d.cols();
        if (d.is3D())
            os << "x" << d.pages();
        os << "\"";
        os << ",\"bytes\":" << val->rawBytes();
        os << ",\"preview\":";
        if (val->type() == MType::DOUBLE && val->isScalar()) {
            double v = val->toScalar();
            if (std::isnan(v))
                os << "\"NaN\"";
            else if (std::isinf(v))
                os << (v > 0 ? "\"Inf\"" : "\"-Inf\"");
            else
                os << v;
        } else if (val->type() == MType::COMPLEX && val->isScalar()) {
            auto c = val->toComplex();
            os << "\"" << c.real();
            if (c.imag() >= 0)
                os << "+";
            os << c.imag() << "i\"";
        } else if (val->type() == MType::CHAR) {
            os << "\"" << jsonEscape(val->toString()) << "\"";
        } else if (val->type() == MType::LOGICAL && val->isScalar()) {
            os << (val->toBool() ? "true" : "false");
        } else if ((val->type() == MType::DOUBLE) && val->numel() <= 10) {
            os << "[";
            for (size_t i = 0; i < val->numel(); ++i) {
                if (i)
                    os << ",";
                os << val->doubleData()[i];
            }
            os << "]";
        } else {
            os << "null";
        }
        os << "}";
    }
    os << "}";
    return os.str();
}

} // namespace mlab