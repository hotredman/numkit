// src/engine.cpp
#include <numkit/core/engine.hpp>
#include <numkit/core/branding.hpp>
#include <numkit/core/compiler.hpp>
#include <numkit/core/lexer.hpp>
#include <numkit/core/parser.hpp>
#include <numkit/builtin/library.hpp>
#include <numkit/signal/library.hpp>
#include <numkit/stats/library.hpp>
#include <numkit/image/library.hpp>
#include <numkit/comm/library.hpp>
#include <numkit/wavelet/library.hpp>
#include <numkit/control/library.hpp>
#include <numkit/graphics/library.hpp>
#include <numkit/io/library.hpp>
#include <numkit/optim/library.hpp>
#include <numkit/audio/library.hpp>
#include <numkit/core/tree_walker.hpp>
#include <numkit/core/vm.hpp>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <sstream>
#include <unordered_set>

namespace numkit {

// ============================================================
// Reserved names — see types.hpp for per-set semantics.
// ============================================================
const std::unordered_set<std::string> kBuiltinConstants = {
    // `true`/`false`/`nan`/`NaN`/`inf`/`Inf` are MATLAB built-in
    // functions, not constants — they support shape forms
    // `nan(M, N, 'single')`, `Inf(N)` etc. See library.cpp /
    // matrix.cpp:nan_reg/inf_reg/true_reg/false_reg and BUGS.md #30.
    "pi", "eps", "i", "j",
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
Engine::Engine() : Engine(std::pmr::get_default_resource()) {}

Engine::Engine(std::pmr::memory_resource *mr)
    : mr_(mr ? mr : std::pmr::get_default_resource())
{
    globalsEnv_ = std::make_unique<Environment>();
    constantsEnv_ = std::make_unique<Environment>(nullptr, globalsEnv_.get());
    workspaceEnv_ = std::make_unique<Environment>(constantsEnv_.get(), globalsEnv_.get());
    treeWalker_ = std::make_unique<TreeWalker>(*this);
    compiler_ = std::make_unique<Compiler>(*this);
    vm_ = std::make_unique<VM>(*this);

    reinstallConstants();
    registerVirtualFS(std::make_unique<NativeFS>());
    BuiltinLibrary::install(*this);
    SignalLibrary::install(*this);
    StatsLibrary::install(*this);
    ImageLibrary::install(*this);
    CommLibrary::install(*this);
    WaveletLibrary::install(*this);
    ControlLibrary::install(*this);
    GraphicsLibrary::install(*this);
    IoLibrary::install(*this);
    OptimLibrary::install(*this);
    AudioLibrary::install(*this);
}

Engine::~Engine()
{
    // Flush any files the user left open — best-effort, swallow any
    // backend errors because we're already tearing down.
    closeAllFiles();
}

void Engine::reinstallConstants()
{
    constantsEnv_->set("pi", Value::scalar(3.14159265358979323846, mr_));
    constantsEnv_->set("eps", Value::scalar(2.2204460492503131e-16, mr_));
    // `nan` / `NaN` / `inf` / `Inf` are MATLAB built-in functions, not
    // constants. Bare `nan` calls nan() → scalar NaN; `nan(M, N)` calls
    // nan(M, N) → MxN matrix of NaN; `nan(M, N, 'single')` returns
    // single-precision. Registration lives in libs/builtin/src/library.cpp
    // via nan_reg / inf_reg. Same pattern as true/false (BUGS.md #30).
    // `true` and `false` are MATLAB built-in functions, not constants.
    // Bare `true` calls true() → scalar logical 1; `true(M, N)` calls
    // true(M, N) → MxN logical array. Registration lives in
    // libs/builtin/src/library.cpp via true_reg / false_reg. See
    // BUGS.md #30.
    constantsEnv_->set("i", Value::complexScalar(0.0, 1.0, mr_));
    constantsEnv_->set("j", Value::complexScalar(0.0, 1.0, mr_));

    // Re-install host-registered constants so they survive `clear all`.
    for (auto &[name, val] : userConstants_)
        constantsEnv_->set(name, val);
}

void Engine::registerConstant(const std::string &name, Value val)
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
void Engine::registerBinaryOp(const std::string &op, BinaryOpFunc func)
{
    binaryOps_[op] = std::move(func);
}
void Engine::registerUnaryOp(const std::string &op, UnaryOpFunc func)
{
    unaryOps_[op] = std::move(func);
}
// Internal helper — implements the actual registration logic shared
// by both registerFunction overloads. Maintains the auxiliary indices
// (shortNameIndex_, namespaceOrder_) and enforces uniqueness of the
// full name in the primary externalFuncs_ map.
void Engine::registerFunctionImpl_(const std::string &fullName,
                                   const std::string &leafName,
                                   ExternalFunc func)
{
    if (externalFuncs_.find(fullName) != externalFuncs_.end()) {
        // Common cause for `compat.<name>` collisions: a stale `noop`
        // placeholder was left in a library's install() while the real
        // implementation was registered elsewhere. Each `reg(<sub>,
        // <name>, …)` helper auto-registers `compat.<name>` once, so
        // two such calls with the same `<name>` (even from different
        // sub-namespaces) crash here. Hint the user where to look.
        std::string hint;
        if (fullName.rfind("compat.", 0) == 0) {
            hint = " (look for a stale `reg(<sub>, \"" + leafName
                 + "\", noop)` or two real implementations sharing this"
                   " name across sub-namespaces)";
        }
        throw std::runtime_error("duplicate function registration: "
                                 + fullName + hint);
    }
    externalFuncs_.emplace(fullName, std::move(func));
    shortNameIndex_.emplace(leafName, fullName);

    // Track top-level namespace (everything before the first '.').
    // Core registrations (no '.') do not introduce a namespace.
    auto dot = fullName.find('.');
    if (dot != std::string::npos) {
        std::string topNs = fullName.substr(0, dot);
        if (namespaceSet_.insert(topNs).second) {
            namespaceOrder_.push_back(topNs);
        }
    }
}

void Engine::registerFunction(const std::string &name, ExternalFunc func)
{
    // 1-arg form: equivalent to namespace = "" (core). full name == leaf name.
    registerFunctionImpl_(name, name, std::move(func));
}

void Engine::registerFunction(const std::string &ns,
                              const std::string &name,
                              ExternalFunc func)
{
    if (ns.empty()) {
        registerFunctionImpl_(name, name, std::move(func));
    } else {
        registerFunctionImpl_(ns + "." + name, name, std::move(func));
    }
}

// Internal helper: walk active imports across env→parent chain plus the
// engine's workspace fallback, calling `tryQualified(qualified)` for each
// candidate. Returns true the first time the callback returns true.
//
// Resolution rules (mirrors NAMESPACE_DESIGN.md §4):
//   * `import a.b.*`     → tries "a.b.<name>" first, then "deep" candidates
//                          via shortNameIndex_: any registered fullname that
//                          starts with "a.b." and ends with ".<name>". This
//                          lets `import signal.*` find `signal.transforms.fft`
//                          when calling bare `fft()` — sub-namespaces are
//                          transparent for wildcard imports.
//   * `import a.b.<name>` → tries "a.b.<name>" (when last path segment matches)
//   * `import a.b as x`   → when `name` starts with "x.", rewrites to
//                           "a.b.<rest>" and tries that; otherwise skipped.
template <class ShortNameIndex, class Fn>
static bool walkImportCandidates_(const std::string &name,
                                   const Environment *env,
                                   const Environment *workspaceEnv,
                                   const ShortNameIndex &shortNameIndex,
                                   Fn &&tryQualified)
{
    auto buildPrefix = [](const std::vector<std::string> &path) {
        std::string s;
        s.reserve(64);
        for (const auto &p : path) {
            if (!s.empty()) s.push_back('.');
            s.append(p);
        }
        return s;
    };

    auto runOne = [&](const Environment *cur) -> bool {
        // Walk imports newest-first: in `import a.*; import b.*;` the
        // second import shadows the first when both contain the same
        // leaf. activeImports() pushes append-only, so iterate in
        // reverse order.
        const auto &imps = cur->activeImports();
        for (auto rit = imps.rbegin(); rit != imps.rend(); ++rit) {
            const auto &imp = *rit;
            if (imp.path.empty()) continue;
            if (imp.wildcard) {
                std::string prefix = buildPrefix(imp.path);
                std::string direct = prefix + "." + name;
                if (tryQualified(direct))
                    return true;
                // Deep scan: any registered "a.b.SUB.<name>" (or deeper)
                // also matches `import a.b.*`.
                std::string dottedPrefix = prefix + ".";
                std::string dottedSuffix = "." + name;
                auto range = shortNameIndex.equal_range(name);
                for (auto it = range.first; it != range.second; ++it) {
                    const std::string &full = it->second;
                    if (full == direct) continue;            // already tried
                    if (full.size() <= dottedPrefix.size())  continue;
                    if (full.compare(0, dottedPrefix.size(),
                                     dottedPrefix) != 0)     continue;
                    if (full.size() < dottedSuffix.size()) continue;
                    if (full.compare(full.size() - dottedSuffix.size(),
                                     dottedSuffix.size(),
                                     dottedSuffix) != 0) continue;
                    if (tryQualified(full))
                        return true;
                }
            } else if (!imp.alias.empty()) {
                // Alias import (`import a.b as alias`): rewrite a name
                // that begins with "alias." to "a.b.<rest>" and try the
                // registered qualified form. Bare names (no dot in
                // `name`) and names whose prefix doesn't match the
                // alias fall through.
                const std::string aliasDot = imp.alias + ".";
                if (name.size() > aliasDot.size()
                    && name.compare(0, aliasDot.size(), aliasDot) == 0) {
                    std::string candidate = buildPrefix(imp.path);
                    candidate.push_back('.');
                    candidate.append(name, aliasDot.size(),
                                     std::string::npos);
                    if (tryQualified(candidate))
                        return true;
                }
            } else if (imp.path.back() == name) {
                if (tryQualified(buildPrefix(imp.path)))
                    return true;
            }
        }
        return false;
    };

    for (const Environment *cur = env; cur != nullptr;
         cur = cur->parentForImports()) {
        if (runOne(cur)) return true;
    }
    // Top-level imports always cascade into function calls. TW user
    // functions parent localEnv at constantsEnv (not workspaceEnv), so
    // the parent-walk above doesn't reach workspaceEnv — this fallback
    // is load-bearing for TW. VM frame.env parents at workspaceEnv
    // directly, so this re-visits an env we already walked (harmless).
    if (workspaceEnv && env != workspaceEnv) {
        if (runOne(workspaceEnv)) return true;
    }
    return false;
}

const ExternalFunc *Engine::findExternal(const std::string &name,
                                         const Environment *env) const
{
    // 1. Direct hit — covers core, promotions, and already-qualified names.
    auto it = externalFuncs_.find(name);
    if (it != externalFuncs_.end()) {
        return &it->second;
    }

    // 2. Walk active imports across env→parent → workspaceEnv fallback.
    //    The workspace fallback is a pragmatic relaxation so that REPL /
    //    test code that does `import compat.*` at the top can flatten
    //    those names inside nested function bodies too. MATLAB-strict
    //    mode would scope imports to the declaring function only.
    const ExternalFunc *hit = nullptr;
    walkImportCandidates_(name, env, workspaceEnv_.get(), shortNameIndex_,
        [&](const std::string &qualified) {
            auto qit = externalFuncs_.find(qualified);
            if (qit != externalFuncs_.end()) {
                hit = &qit->second;
                return true;
            }
            return false;
        });
    return hit;
}

void Engine::setVariable(const std::string &name, Value val)
{
    workspaceEnv_->set(name, std::move(val));
}
Value *Engine::getVariable(const std::string &name)
{
    // Check globalsEnv first (for global variables set by functions)
    Value *gs = globalsEnv_->get(name);
    if (gs && !gs->isUnset()) {
        // Sync to workspaceEnv if different
        Value *ge = workspaceEnv_->get(name);
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

const UserFunction *Engine::lookupUserFunctionLocal(const std::string &name) const
{
    auto it = scriptLocalUserFuncs_.find(name);
    if (it != scriptLocalUserFuncs_.end())
        return &it->second;
    auto it2 = userFuncs_.find(name);
    return it2 != userFuncs_.end() ? &it2->second : nullptr;
}

const UserFunction *Engine::lookupUserFunction(const std::string &name,
                                                const Environment *env)
{
    if (auto *f = lookupUserFunctionLocal(name))
        return f;
    // Direct path: parse-and-load <name>.m (or +pkg/.../<leaf>.m for a
    // dotted name) from the search path.
    if (auto *f = resolveMFile_(name))
        return f;

    // No scope to walk imports from — nothing more to try.
    if (!env)
        return nullptr;
    // Bare names walk wildcard / single-symbol / alias imports below.
    // Dotted names only benefit from alias rewriting (`x.foo` → `a.b.foo`
    // when `import a.b as x` is active); the wildcard / single-symbol
    // branches in walkImportCandidates_ no-op for dotted names.

    // Walk imports across env→parent → workspaceEnv fallback. Each
    // successful resolveMFile_ caches the entry under its full
    // qualified key, so subsequent calls hit userFuncs_ directly.
    const UserFunction *hit = nullptr;
    walkImportCandidates_(name, env, workspaceEnv_.get(), shortNameIndex_,
        [&](const std::string &qualified) {
            if (auto *f = lookupUserFunctionLocal(qualified)) {
                hit = f;
                return true;
            }
            if (auto *f = resolveMFile_(qualified)) {
                hit = f;
                return true;
            }
            return false;
        });
    return hit;
}

void Engine::addPath(const std::string &dir)
{
    // De-dup: ignore if already present.
    for (const auto &p : mPath_) {
        if (p == dir) return;
    }
    mPath_.push_back(dir);
}

void Engine::rmPath(const std::string &dir)
{
    auto it = std::find(mPath_.begin(), mPath_.end(), dir);
    if (it != mPath_.end()) mPath_.erase(it);
}

void Engine::adoptUserFunction(const std::string &name,
                                UserFunction uf,
                                bool scriptScope)
{
    if (scriptScope)
        scriptLocalUserFuncs_[name] = std::move(uf);
    else
        userFuncs_[name] = std::move(uf);
}

void Engine::rehashMFiles()
{
    // Drop cache entries AND the user-function/compiled mirrors created
    // by resolveMFile_. Functions registered by execFunctionDef from
    // top-level scripts are kept — only m-file-loaded ones should go.
    // The compiler stores chunks under the same key resolveMFile_ used
    // (qualified for +pkg/foo.m, bare for plain foo.m), so erasing by
    // mFileCache_ key alone is sufficient.
    for (const auto &[name, _] : mFileCache_) {
        userFuncs_.erase(name);
        if (compiler_) {
            compiler_->clearCompiledFuncs();
        }
    }
    mFileCache_.clear();
}

const UserFunction *Engine::resolveMFile_(const std::string &name)
{
    // Build search-path list: script-dir first (if any), then mPath_.
    // The implicit script-dir entry is what makes sibling lookup work
    // without addpath — `caller.m` calling `helper(x)` resolves against
    // the directory the running script came from. Routes through the
    // script's FS via `resolvePath` (which also falls back to the
    // origin's fsName when the relative path has no scheme).
    std::vector<std::string> searchDirs;
    if (auto *dir = currentScriptDir(); dir && !dir->empty())
        searchDirs.push_back(*dir);
    searchDirs.insert(searchDirs.end(), mPath_.begin(), mPath_.end());

    // Decompose dotted name. "pkg.sub.foo" → +pkg/+sub/foo.m. The leaf
    // (last segment) is the function name and what we cache under; the
    // earlier segments become +<seg>/ directory components per
    // MATLAB's package-folder convention.
    std::string leafName = name;
    std::string nsPrefix;            // "+pkg/+sub/" form, empty for unqualified
    {
        size_t dot = name.find('.');
        if (dot != std::string::npos) {
            std::string tail = name;
            while ((dot = tail.find('.')) != std::string::npos) {
                nsPrefix += '+';
                nsPrefix.append(tail, 0, dot);
                nsPrefix += '/';
                tail.erase(0, dot + 1);
            }
            leafName = std::move(tail);
        }
    }

    for (const auto &dir : searchDirs) {
        std::string userPath = dir;
        if (!userPath.empty() && userPath.back() != '/' && userPath.back() != '\\')
            userPath += '/';
        userPath += nsPrefix;
        userPath += leafName + ".m";

        ResolvedPath rp;
        try {
            rp = resolvePath(userPath);
        } catch (const std::exception &) {
            continue;
        }
        if (!rp.fs || !rp.fs->exists(rp.path))
            continue;

        // Cache hit? Validate via mtime; re-parse if stale.
        auto cit = mFileCache_.find(name);
        if (cit != mFileCache_.end() && cit->second.fullPath == userPath) {
            auto st = rp.fs->stat(rp.path);
            int64_t curMtime = st ? st->mtime : 0;
            if (curMtime != 0 && curMtime == cit->second.mtime) {
                if (auto *uf = lookupUserFunctionLocal(name))
                    return uf;
            }
            // Stale or no mtime — drop and re-parse.
            mFileCache_.erase(cit);
            userFuncs_.erase(name);
        }

        // Read + parse + extract FUNCTION_DEF.
        std::string content;
        try {
            content = rp.fs->readFile(rp.path);
        } catch (const std::exception &) {
            continue;
        }

        Lexer lexer(content);
        std::vector<Token> tokens;
        try {
            tokens = lexer.tokenize();
        } catch (const std::exception &) {
            continue;
        }
        Parser parser(tokens);
        ASTNodePtr ast;
        try {
            ast = parser.parse();
        } catch (const std::exception &) {
            continue;
        }
        if (!ast) continue;

        // First top-level FUNCTION_DEF whose name matches the leaf. The
        // function-def name inside a +pkg/foo.m file is "foo" — the
        // package qualification lives in the path, not the source.
        const ASTNode *funcDef = nullptr;
        if (ast->type == NodeType::BLOCK) {
            for (const auto &c : ast->children) {
                if (c && c->type == NodeType::FUNCTION_DEF && c->strValue == leafName) {
                    funcDef = c.get();
                    break;
                }
            }
        } else if (ast->type == NodeType::FUNCTION_DEF && ast->strValue == leafName) {
            funcDef = ast.get();
        }
        if (!funcDef) continue;

        // Build UserFunction (mirrors TreeWalker::execFunctionDef). We
        // store under the QUALIFIED key (`name`) so multiple packages
        // can host functions with the same leaf without colliding.
        UserFunction func;
        func.name = name;
        func.params = funcDef->paramNames;
        func.returns = funcDef->returnNames;
        func.body = std::shared_ptr<const ASTNode>(cloneNode(funcDef->children[0].get()));
        func.closureEnv = nullptr;

        // Register for VM dispatch (mirrors what execFunctionDef +
        // beginScript pre-compile pass do for in-script function defs).
        // Bind under the QUALIFIED name so two packages with the same
        // leaf (`+a/foo.m` and `+b/foo.m`) don't collide in
        // compiledFuncs_. registerFunctionAs also writes
        // engine_.userFuncs_[qualified] — but we still set it
        // explicitly below so non-VM-backed calls work even when the
        // compiler is unavailable or rejects the chunk.
        if (compiler_) {
            try {
                compiler_->registerFunctionAs(name, funcDef);
            } catch (const std::exception &) {
                // Compiler errors are non-fatal here — TW will still
                // dispatch through userFuncs_; only VM-mode invocations
                // will fall through to the generic CALL → external
                // path, which fails cleanly.
            }
        }
        userFuncs_[name] = std::move(func);

        // Cache stat metadata for mtime-based invalidation.
        MFileCacheEntry e;
        e.fullPath = userPath;
        if (auto st = rp.fs->stat(rp.path))
            e.mtime = st->mtime;
        e.sourceCode = std::make_shared<const std::string>(std::move(content));
        mFileCache_[name] = std::move(e);

        return &userFuncs_[name];
    }
    return nullptr;
}

bool Engine::hasExternalFunction(const std::string &name) const
{
    return externalFuncs_.count(name) > 0;
}

Value Engine::callFunctionHandle(const Value &handle,
                                  Span<const Value> args,
                                  Environment *env)
{
    auto results = callFunctionHandleMulti(handle, args, 1, env);
    return results.empty() ? Value::empty() : results[0];
}

std::vector<Value> Engine::callFunctionHandleMulti(const Value &handle,
                                                    Span<const Value> args,
                                                    size_t nout,
                                                    Environment *env)
{
    // Closure form: VM packages `@(x) x + capture` as a cell whose
    // first element is the bare funcHandle and the rest are captured
    // values to append to the user-supplied args.
    const Value *bareHandle = &handle;
    std::vector<Value> withCaptures;
    if (handle.isCell() && handle.numel() >= 1
        && handle.cellAt(0).isFuncHandle()) {
        bareHandle = &handle.cellAt(0);
        withCaptures.reserve(args.size() + handle.numel() - 1);
        for (const auto &a : args) withCaptures.push_back(a);
        for (std::size_t i = 1; i < handle.numel(); ++i)
            withCaptures.push_back(handle.cellAt(i));
        args = Span<const Value>(withCaptures.data(), withCaptures.size());
    }
    if (!bareHandle->isFuncHandle())
        throw std::runtime_error("callFunctionHandleMulti: argument is not a function handle");
    Environment *e = env ? env : workspaceEnv_.get();
    const std::string name = bareHandle->funcHandleName();

    // 1) Built-in (registered external) — works regardless of backend.
    {
        auto it = externalFuncs_.find(name);
        if (it != externalFuncs_.end()) {
            std::vector<Value> out(nout);
            CallContext ctx{this, e};
            it->second(args, nout, Span<Value>(out), ctx);
            return out;
        }
    }

    // 2) TW user-function path. Works for any named user function and
    // for anonymous handles regardless of which backend created them:
    // VM-compiled anon-funcs are mirror-registered into
    // engine.userFuncs_ by Compiler::compileAnonFunc, so TW finds them
    // here even when the VM was the active backend at handle creation
    // time. Captures travel as appended args (the closure-cell unwrap
    // above) — both backends use the same `[user_params, captures]`
    // parameter layout. Pass the BARE handle (not the closure cell) so
    // TW resolves the funcHandleName correctly.
    if (treeWalker_)
        return treeWalker_->callHandleMultiPublic(*bareHandle, args, e, nout);

    throw std::runtime_error("callFunctionHandle: undefined function in handle '@"
                             + name + "'");
}

bool Engine::isInsideFunctionCall() const
{
    if (vm_ && backend_ == Backend::VM)
        return vm_->callDepth() > 0;
    if (treeWalker_)
        return treeWalker_->callDepth() > 0;
    return false;
}

// ── Frame stack introspection ───────────────────────────────────

int Engine::callerDepth() const
{
    if (vm_ && backend_ == Backend::VM)
        return vm_->callDepth();
    if (treeWalker_)
        return static_cast<int>(treeWalker_->activeFrames().size());
    return 0;
}

Environment *Engine::callerEnv(int n)
{
    if (n < 0) n = 0;
    if (vm_ && backend_ == Backend::VM)
        return vm_->callerEnvAtDepth(n);
    if (treeWalker_) {
        const auto &frames = treeWalker_->activeFrames();
        if (n < static_cast<int>(frames.size()))
            return frames[frames.size() - 1 - n].env;
        return workspaceEnv_.get();
    }
    return workspaceEnv_.get();
}

void Engine::assignToCaller(int n, const std::string &name, Value val)
{
    Environment *env = callerEnv(n);
    if (!env) env = workspaceEnv_.get();
    // Write-through to register if VM caller frame has the name in its
    // varMap (so static reads pick up the new value).
    if (vm_ && backend_ == Backend::VM)
        vm_->assignInCallerFrame(n, name, val);
    env->set(name, std::move(val));
}

std::string Engine::inputName(int k)
{
    if (k < 1)
        throw std::runtime_error("inputname: argument index must be >= 1");
    if (callerDepth() < 1)
        throw std::runtime_error(
            "inputname: must be called from within a function");

    if (vm_ && backend_ == Backend::VM) {
        const auto &names = vm_->currentFrameCallerArgNames();
        if (k > static_cast<int>(names.size()))
            return {};  // beyond known names: arg wasn't recorded
        return names[k - 1];
    }
    if (treeWalker_) {
        const auto &frames = treeWalker_->activeFrames();
        if (frames.empty()) return {};
        const auto &names = frames.back().callerArgNames;
        if (k > static_cast<int>(names.size()))
            return {};
        return names[k - 1];
    }
    return {};
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
Value Engine::runOneChunk(const ASTNode *ast, std::shared_ptr<const std::string> src)
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
        Value result = vm_->execute(chunk);
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

// When the eval-builtin's caller captures the result (`r = eval(...)`),
// MATLAB suppresses any "ans = ..." or lhs-display the inner code would
// otherwise emit. We honour that by flipping `suppressOutput=true` on
// each top-level statement before TW/VM gets to it — both backends
// already gate their DISPLAY emission on that flag, so this single hook
// covers ASSIGN, EXPR_STMT, FIELD_ASSIGN, CELL_ASSIGN, etc. Side-effect
// prints inside called functions (disp, fprintf, ...) are unaffected:
// those originate inside CALL nodes whose own statement-level suppress
// flag we don't touch.
static void markTopLevelSuppressed(ASTNode *ast)
{
    if (!ast) return;
    if (ast->type == NodeType::BLOCK) {
        for (auto &c : ast->children)
            if (c) c->suppressOutput = true;
    } else {
        ast->suppressOutput = true;
    }
}

Value Engine::eval(const std::string &code, bool suppressTopLevelDisplay)
{
    Lexer lexer(code);
    auto tokens = lexer.tokenize();
    Parser parser(tokens);
    auto ast = parser.parse();
    auto src = std::make_shared<const std::string>(code);
    if (suppressTopLevelDisplay)
        markTopLevelSuppressed(ast.get());

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
        Value result = Value::empty();
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
    } catch (const Error &e) {
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
            Value *gsVal = globalsEnv_->get(name);
            workspaceEnv_->set(name, gsVal ? *gsVal : val);
        }
    }
}

void Engine::syncVMToScope(Environment *scope)
{
    if (!scope || scope == workspaceEnv_.get()) {
        syncVMToWorkspace();
        return;
    }
    for (auto &[name, val] : vm_->lastVarMap()) {
        if (val.isUnset() || val.isDeleted()) {
            scope->remove(name);
            continue;
        }
        scope->set(name, val);
        // VM mode: write-through to the scope-owning frame's static
        // register slot if any, so subsequent register-based reads in
        // the caller pick up the value.
        if (vm_ && backend_ == Backend::VM)
            vm_->writeToFrameMatchingEnv(scope, name, val);
    }
}

Value Engine::eval(const std::string &code, Environment *scope,
                   bool suppressTopLevelDisplay)
{
    if (!scope || scope == workspaceEnv_.get())
        return eval(code, suppressTopLevelDisplay);

    // Scoped re-entrant eval: inner script's imports go to scope, and
    // its top-level variable assignments are pushed back into scope on
    // exit (with VM register write-through where applicable). Used by
    // evalin, and by eval/run when called from inside a user function
    // (so script-defined vars stay scoped to the caller's frame).
    Lexer lexer(code);
    auto tokens = lexer.tokenize();
    Parser parser(tokens);
    auto ast = parser.parse();
    auto src = std::make_shared<const std::string>(code);
    if (suppressTopLevelDisplay)
        markTopLevelSuppressed(ast.get());

    if (backend_ != Backend::VM) {
        // TW already executes statements against the env passed in;
        // imports and assignments naturally land in `scope`.
        return treeWalker_->execute(ast.get(), scope);
    }

    // VM path: route inner top-level's ctx.env via inheritedScope_,
    // sync registers to scope after exec.
    Environment *prevInherited = vm_->inheritedScope_;
    vm_->inheritedScope_ = scope;
    bool prevClearAll = clearAllCalled_;
    clearAllCalled_ = false;
    vm_->clearLastVarMap();

    // Pre-populate inner top-level's dynVars with caller's variables
    // (registers + existing overlay + env-resident vars) so
    // ASSERT_DEF fallback resolves bare identifiers that live in the
    // caller's scope. Map lives on this C++ stack frame — must
    // outlive vm_->execute(chunk).
    auto callerSnapshot = vm_->snapshotFrameVars(scope);
    // Also include vars set in scope.env directly (e.g. by assignin
    // when the name wasn't in the caller's static varMap). Env values
    // lose to register values when both exist (registers are more
    // up-to-date for static caller writes).
    scope->forEachLocal([&](const std::string &n, const Value &v) {
        if (v.isUnset() || v.isDeleted()) return;
        callerSnapshot.try_emplace(n, v);
    });
    vm_->setNextFrameDynVars(callerSnapshot.empty() ? nullptr : &callerSnapshot);

    auto chunk = compiler_->compile(ast.get(), src);
    vm_->setCompiledFuncs(&compiler_->compiledFuncs(),
                          &compiler_->scriptLocalCompiledFuncs());

    Value result;
    try {
        result = vm_->execute(chunk);
        syncVMToScope(scope);
    } catch (...) {
        syncVMToScope(scope);
        vm_->inheritedScope_ = prevInherited;
        clearAllCalled_ = prevClearAll;
        throw;
    }

    vm_->inheritedScope_ = prevInherited;
    clearAllCalled_ = prevClearAll;
    return result;
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
        if (val->type() == ValueType::DOUBLE && val->isScalar()) {
            double v = val->toScalar();
            if (std::isnan(v))
                os << "\"NaN\"";
            else if (std::isinf(v))
                os << (v > 0 ? "\"Inf\"" : "\"-Inf\"");
            else
                os << v;
        } else if (val->type() == ValueType::COMPLEX && val->isScalar()) {
            auto c = val->toComplex();
            os << "\"" << c.real();
            if (c.imag() >= 0)
                os << "+";
            os << c.imag() << "i\"";
        } else if (val->type() == ValueType::CHAR) {
            os << "\"" << jsonEscape(val->toString()) << "\"";
        } else if (val->type() == ValueType::LOGICAL && val->isScalar()) {
            os << (val->toBool() ? "true" : "false");
        } else if ((val->type() == ValueType::DOUBLE) && val->numel() <= 10) {
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
    scriptOriginStack_.push_back({fsName, std::string{}});
}

void Engine::pushScriptOrigin(const std::string &fsName, const std::string &scriptDir)
{
    scriptOriginStack_.push_back({fsName, scriptDir});
}

void Engine::popScriptOrigin()
{
    if (!scriptOriginStack_.empty())
        scriptOriginStack_.pop_back();
}

const std::string *Engine::currentScriptOrigin() const
{
    return scriptOriginStack_.empty() ? nullptr : &scriptOriginStack_.back().fsName;
}

const std::string *Engine::currentScriptDir() const
{
    return scriptOriginStack_.empty() ? nullptr : &scriptOriginStack_.back().scriptDir;
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
            throw Error("unknown filesystem '" + scheme + "' in path");
        return {fs, rest};
    }

    // 2. NUMKIT_FS env var selects the backend.
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
        throw Error("filesystem '" + fsName + "' is not available");

    // Normalize path: if relative, prepend the engine's cwd. Precedence:
    //   1. Engine::cwd_ when set (`cd`/`setCwd` write here — canonical).
    //   2. NUMKIT_CWD env var (host-runtime override; only consulted
    //      when the engine hasn't been told a cwd of its own).
    // No "two sources diverge" risk: cwd_ wins whenever it's non-empty.
    // The env fallback exists so hosts can `setenv NUMKIT_CWD` after
    // engine construction without needing to call setCwd explicitly.
    std::string path = userPath;
    if (!isAbsolutePath(path)) {
        std::string cwd = !cwd_.empty() ? cwd_
                                        : envGet(envVarName("CWD").c_str());
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

    // Accept the six MATLAB modes. 'r+'/'w+'/'a+' grant both read and
    // write permission; the base letter still governs seed/truncate/
    // append behaviour.
    bool canRead = false, canWrite = false, appendOnly = false, truncate = false, seedBuffer = false;
    if      (mode == "r")  { canRead = true;  seedBuffer = true; }
    else if (mode == "w")  { canWrite = true; truncate = true; }
    else if (mode == "a")  { canWrite = true; appendOnly = true; seedBuffer = true; }
    else if (mode == "r+") { canRead = true;  canWrite = true; seedBuffer = true; }
    else if (mode == "w+") { canRead = true;  canWrite = true; truncate = true; }
    else if (mode == "a+") { canRead = true;  canWrite = true; appendOnly = true; seedBuffer = true; }
    else {
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
    f.forRead = canRead;
    f.forWrite = canWrite;
    f.appendOnly = appendOnly;

    if (seedBuffer) {
        // Plain 'r' and 'r+' demand the file exist (MATLAB: "File must
        // exist"). 'a' / 'a+' tolerate a missing target and start from
        // an empty buffer.
        const bool requireExisting = (mode == "r" || mode == "r+");
        try {
            if (r.fs->exists(r.path))
                f.buffer = r.fs->readFile(r.path);
            else if (requireExisting) {
                lastFopenError_ = "No such file or directory";
                return -1;
            }
        } catch (const std::exception &e) {
            if (requireExisting) {
                lastFopenError_ = e.what();
                return -1;
            }
            f.buffer.clear();
        }
    }
    if (truncate)
        f.buffer.clear();
    if (appendOnly)
        f.cursor = f.buffer.size();

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

} // namespace numkit