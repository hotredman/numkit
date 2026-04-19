// src/MLabEngine.cpp
#include "MLabEngine.hpp"
#include "MLabBranding.hpp"
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
    registerVirtualFS(std::make_unique<NativeFS>());
    StdLibrary::install(*this);
    DspLibrary::install(*this);
    PltLibrary::install(*this);
    FitLibrary::install(*this);
}

Engine::~Engine()
{
    // Flush any files the user left open — best-effort, swallow any
    // backend errors because we're already tearing down.
    closeAllFiles();
}

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
    return externalFuncs_.count(name) || hasUserFunction(name);
}

bool Engine::hasUserFunction(const std::string &name) const
{
    return scriptLocalUserFuncs_.count(name) > 0
           || userFuncs_.count(name) > 0;
}

const UserFunction *Engine::lookupUserFunction(const std::string &name) const
{
    auto it = scriptLocalUserFuncs_.find(name);
    if (it != scriptLocalUserFuncs_.end())
        return &it->second;
    auto it2 = userFuncs_.find(name);
    return it2 != userFuncs_.end() ? &it2->second : nullptr;
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
    // Only the workspace bucket — script-local functions live in
    // scriptLocalUserFuncs_/scriptLocalCompiledFuncs_ and are
    // managed by begin/endScript.
    userFuncs_.clear();
    if (compiler_)
        compiler_->clearCompiledFuncs();
}

void Engine::beginScript(const ASTNode *ast)
{
    savedScriptLocalUserFuncs_.push_back(std::move(scriptLocalUserFuncs_));
    scriptLocalUserFuncs_.clear();
    if (!compiler_)
        return;
    compiler_->beginScriptScope();
    if (!ast)
        return;
    // Pre-compile the script's top-level FUNCTION_DEFs into the
    // (now-active) script-local buckets. Forward references inside
    // the script resolve, and a single FUNCTION_DEF file (AST is
    // the function itself) still registers cleanly.
    auto registerNode = [&](const ASTNode *f) {
        if (f && f->type == NodeType::FUNCTION_DEF)
            compiler_->registerFunction(f);
    };
    if (ast->type == NodeType::BLOCK) {
        for (const auto &c : ast->children)
            registerNode(c.get());
    } else {
        registerNode(ast);
    }
}

void Engine::endScript()
{
    if (compiler_)
        compiler_->endScriptScope();
    if (savedScriptLocalUserFuncs_.empty()) {
        scriptLocalUserFuncs_.clear();
        return;
    }
    scriptLocalUserFuncs_ = std::move(savedScriptLocalUserFuncs_.back());
    savedScriptLocalUserFuncs_.pop_back();
}

void Engine::promoteScriptLocalsToWorkspace()
{
    for (auto &entry : scriptLocalUserFuncs_)
        userFuncs_[entry.first] = std::move(entry.second);
    scriptLocalUserFuncs_.clear();
    if (compiler_)
        compiler_->promoteScriptLocalsToWorkspace();
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
    vm_->setCompiledFuncs(&compiler_->compiledFuncs(),
                          &compiler_->scriptLocalCompiledFuncs());

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

    // A "script" here = a BLOCK that mixes FUNCTION_DEFs with
    // executable statements (the shape of a .m file with local
    // helper functions after the script body). Those FUNCTION_DEFs
    // are file-local and vanish on return — MATLAB script
    // semantics. Any other shape — a pure-statement paste, a lone
    // `function ...`, or a batch of function defs at the REPL —
    // registers functions into the workspace bucket so subsequent
    // evals can see them.
    bool hasFunc = false, hasStmt = false;
    if (ast && ast->type == NodeType::BLOCK) {
        for (auto &c : ast->children) {
            if (!c) continue;
            if (c->type == NodeType::FUNCTION_DEF) hasFunc = true;
            else hasStmt = true;
        }
    }
    const bool isScript = hasFunc && hasStmt;
    if (isScript)
        beginScript(ast.get());
    // At eval exit, promote script-locals into the workspace
    // before tearing down the scope — matches the engine's
    // established REPL contract where defining a function and
    // calling it in the same paste keeps the function around for
    // later evals. DebugSession's own beginScript path doesn't
    // call this promotion, so .m file-local helpers stay file-local.
    struct ScriptEndGuard {
        Engine &e;
        bool armed;
        ~ScriptEndGuard() {
            if (armed) {
                e.promoteScriptLocalsToWorkspace();
                e.endScript();
            }
        }
    } _scriptGuard{*this, isScript};

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
    // An attached debug observer runs the whole eval as one chunk: the
    // observer expects step/line semantics to correspond to the source as
    // a unit, and a split would re-fire initial-stop events between every
    // top-level statement.
    const bool splittable = !debugObserver_ && ast
                            && ast->type == NodeType::BLOCK
                            && ast->children.size() > 1;
    if (splittable) {
        // Pre-compile FUNCTION_DEF children: the per-statement
        // loop below skips them (they're definitions, not stmts),
        // so if nothing else compiles them they'd be unreachable.
        // In script mode, beginScript has already routed them into
        // scriptLocalCompiledFuncs_. Outside script mode we register
        // into the workspace bucket so REPL-style forward references
        // keep working.
        if (!isScript) {
            for (auto &c : ast->children) {
                if (c && c->type == NodeType::FUNCTION_DEF)
                    compiler_->registerFunction(c.get());
            }
        }
        MValue result = MValue::empty();
        for (auto &c : ast->children) {
            if (!c || c->type == NodeType::FUNCTION_DEF)
                continue;
            result = runOneChunk(c.get(), src);
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

// ============================================================
// Virtual filesystem registry + path resolver
// ============================================================

void Engine::registerVirtualFS(std::unique_ptr<VirtualFS> fs)
{
    if (!fs)
        return;
    auto n = fs->name();
    virtualFs_[n] = std::move(fs);
}

VirtualFS *Engine::findVirtualFS(const std::string &name) const
{
    auto it = virtualFs_.find(name);
    return (it != virtualFs_.end()) ? it->second.get() : nullptr;
}

void Engine::pushScriptOrigin(const std::string &fsName)
{
    scriptOriginStack_.push_back(fsName);
}

void Engine::popScriptOrigin()
{
    if (!scriptOriginStack_.empty())
        scriptOriginStack_.pop_back();
}

const std::string *Engine::currentScriptOrigin() const
{
    return scriptOriginStack_.empty() ? nullptr : &scriptOriginStack_.back();
}

namespace {

// Split "prefix:rest" into {prefix, rest} if `prefix` is a known FS name,
// otherwise return {"", path}. Two guards against false positives on
// paths that happen to contain ':':
//   • colon must be at index >= 2, so Windows drive letters (C:/foo) and
//     empty prefixes (":foo") never look like a scheme. This forbids
//     single-character FS names by construction — acceptable because all
//     current FS names ('native', 'temporary', 'local') are longer.
//   • the prefix must match a registered FS. So a path like "http://..."
//     or "mailto:..." falls through to the default FS untouched.
std::pair<std::string, std::string> splitFsScheme(const std::string &path,
                                                  const std::unordered_map<std::string, std::unique_ptr<VirtualFS>> &fsMap)
{
    auto colon = path.find(':');
    if (colon == std::string::npos || colon < 2)
        return {"", path};
    std::string scheme = path.substr(0, colon);
    if (fsMap.find(scheme) == fsMap.end())
        return {"", path};
    return {scheme, path.substr(colon + 1)};
}

bool isAbsolutePath(const std::string &p)
{
    if (p.empty())
        return false;
    if (p[0] == '/' || p[0] == '\\')
        return true;
#ifdef _WIN32
    if (p.size() >= 2 && p[1] == ':' && ((p[0] >= 'A' && p[0] <= 'Z') || (p[0] >= 'a' && p[0] <= 'z')))
        return true;
#endif
    return false;
}

std::string joinPath(const std::string &base, const std::string &rel)
{
    if (base.empty())
        return rel;
    if (rel.empty())
        return base;
    char last = base.back();
    if (last == '/' || last == '\\')
        return base + rel;
    return base + "/" + rel;
}

} // namespace

Engine::ResolvedPath Engine::resolvePath(const std::string &userPath) const
{
    // 1. Explicit scheme in the path wins.
    auto [scheme, rest] = splitFsScheme(userPath, virtualFs_);
    if (!scheme.empty()) {
        auto *fs = findVirtualFS(scheme);
        if (!fs)
            throw MLabError("unknown filesystem '" + scheme + "' in path");
        return {fs, rest};
    }

    // 2. MLAB_FS env var selects the backend.
    std::string fsName = envGet(envVarName("FS").c_str());
    if (fsName == "auto")
        fsName.clear();

    // 3. Fall back to script origin, then to "native".
    if (fsName.empty()) {
        if (auto *o = currentScriptOrigin())
            fsName = *o;
    }
    if (fsName.empty())
        fsName = "native";

    VirtualFS *fs = findVirtualFS(fsName);
    if (!fs)
        throw MLabError("filesystem '" + fsName + "' is not available");

    // Normalize path: if relative, prepend MLAB_CWD.
    std::string path = userPath;
    if (!isAbsolutePath(path)) {
        std::string cwd = envGet(envVarName("CWD").c_str());
        if (!cwd.empty())
            path = joinPath(cwd, path);
    }

    return {fs, path};
}

// ============================================================
// File descriptor table — MATLAB fopen/fclose/fprintf plumbing
// ============================================================

int Engine::openFile(const std::string &userPath, const std::string &modeRaw)
{
    lastFopenError_.clear();

    // Strip Windows-style 't'/'b' suffix ("rt", "wb"). The underlying
    // buffer is bytes anyway; we don't do CRLF translation.
    std::string mode = modeRaw;
    while (!mode.empty() && (mode.back() == 't' || mode.back() == 'b'))
        mode.pop_back();
    if (mode != "r" && mode != "w" && mode != "a") {
        lastFopenError_ = "Invalid permission specified";
        return -1;
    }

    ResolvedPath r;
    try {
        r = resolvePath(userPath);
    } catch (const std::exception &e) {
        lastFopenError_ = e.what();
        return -1;
    }

    OpenFile f;
    f.path = r.path;
    f.mode = mode;
    f.fs = r.fs;
    f.forRead = (mode == "r");
    f.forWrite = (mode == "w" || mode == "a");

    if (mode == "r") {
        try {
            f.buffer = r.fs->readFile(r.path);
        } catch (const std::exception &e) {
            lastFopenError_ = e.what();
            return -1;
        }
    } else if (mode == "a") {
        // Seed the append buffer with existing content if any — fclose
        // will overwrite the whole file, so we need to preserve what was
        // there before the first fprintf.
        try {
            if (r.fs->exists(r.path))
                f.buffer = r.fs->readFile(r.path);
        } catch (const std::exception &) {
            // Treat read-failure-of-existing-file as "start from empty";
            // less surprising than returning -1 when the user asked to
            // append.
            f.buffer.clear();
        }
        f.cursor = f.buffer.size();
    }

    int fid = nextFid_++;
    openFiles_.emplace(fid, std::move(f));
    return fid;
}

bool Engine::closeFile(int fid)
{
    auto it = openFiles_.find(fid);
    if (it == openFiles_.end())
        return false;

    bool ok = true;
    // Always commit on close for write modes — MATLAB semantics require
    // fopen('w')+fclose to leave an empty file behind, and 'a' should
    // preserve existing content even when no fprintf happened.
    if (it->second.forWrite) {
        try {
            it->second.fs->writeFile(it->second.path, it->second.buffer);
        } catch (const std::exception &) {
            ok = false;
        }
    }
    openFiles_.erase(it);
    return ok;
}

void Engine::closeAllFiles()
{
    // Flush every user fid; swallow individual failures — the caller is
    // typically a destructor or a `fclose('all')` where partial success
    // shouldn't abort the rest.
    std::vector<int> fids;
    fids.reserve(openFiles_.size());
    for (auto &kv : openFiles_)
        fids.push_back(kv.first);
    for (int fid : fids)
        closeFile(fid);
}

Engine::OpenFile *Engine::findFile(int fid)
{
    auto it = openFiles_.find(fid);
    return (it == openFiles_.end()) ? nullptr : &it->second;
}

std::vector<int> Engine::openFileIds() const
{
    std::vector<int> ids;
    ids.reserve(openFiles_.size());
    for (auto &kv : openFiles_)
        ids.push_back(kv.first);
    std::sort(ids.begin(), ids.end());
    return ids;
}

} // namespace mlab