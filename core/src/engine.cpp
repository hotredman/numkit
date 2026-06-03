// src/engine.cpp
#include <numkit/core/engine.hpp>
#include <numkit/core/value_stats.hpp>
#include <numkit/core/branding.hpp>
#include <numkit/core/compiler.hpp>
#include <numkit/core/lexer.hpp>
#include <numkit/core/parser.hpp>
#include <numkit/builtin/library.hpp>
#include <numkit/linalg/library.hpp>
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
#include <numkit/ode/library.hpp>
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
    LinalgLibrary::install(*this);
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
    OdeLibrary::install(*this);
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

void Engine::registerClass(BuiltinClass cls)
{
    if (classes_.count(cls.name))
        throw std::runtime_error("Engine::registerClass: duplicate class '" + cls.name + "'");
    std::string key = cls.name;
    classes_.emplace(std::move(key), std::move(cls));
}

const BuiltinClass *Engine::findClass(const std::string &name) const
{
    auto it = classes_.find(name);
    return it == classes_.end() ? nullptr : &it->second;
}

void Engine::registerCallbackBuiltin(const std::string &name, std::shared_ptr<CallbackBuiltin> cb)
{
    callbackBuiltins_[name] = std::move(cb);
}

CallbackBuiltin *Engine::callbackBuiltin(const std::string &name) const
{
    auto it = callbackBuiltins_.find(name);
    return it == callbackBuiltins_.end() ? nullptr : it->second.get();
}

bool Engine::isUserCodeHandle(const Value &handle) const
{
    const Value *bare = &handle;
    if (handle.isCell() && handle.numel() >= 1 && handle.cellAt(0).isFuncHandle())
        bare = &handle.cellAt(0);
    if (!bare->isFuncHandle())
        return false;
    return lookupUserFunctionLocal(bare->funcHandleName()) != nullptr;
}

std::string Engine::formatObjectDisplay(const std::string &name, const Value &obj) const
{
    const BuiltinClass *cls = findClass(obj.objectClassName());
    std::string body;
    const size_t n = obj.objectCount();
    if (n == 1 && cls && cls->dispText) {
        body = cls->dispText(obj); // scalar object → class-defined body
    } else if (n == 1) {
        body = "  " + obj.objectClassName() + "\n";
    } else {
        // Object array: "<rows>×<cols> <ClassName> array" + property list.
        const Dims &d = obj.dims();
        std::string hdr = "  " + std::to_string(d.rows()) + "\xC3\x97"
                          + std::to_string(d.cols()) + " " + obj.objectClassName()
                          + " array";
        if (cls && !cls->propNames.empty()) {
            hdr += " with properties:\n\n";
            for (const auto &p : cls->propNames)
                hdr += "    " + p + "\n";
        } else {
            hdr += "\n";
        }
        body = hdr;
    }
    if (name.empty())
        return body;
    return name + " =\n\n" + body + "\n";
}

void Engine::displayObject(const std::string &name, const Value &obj)
{
    const BuiltinClass *cls = findClass(obj.objectClassName());
    // Custom disp/display only for a scalar object; arrays use the default.
    if (cls && obj.objectCount() == 1) {
        auto call = [&](const ObjectMethod &m) {
            Value self = obj;
            Value outBuf[1];
            CallContext ctx{this, workspaceEnv_.get()};
            m(self, Span<const Value>(nullptr, 0), /*nargout=*/0, Span<Value>(outBuf, 1), ctx);
        };
        // `display` owns the entire output (including any variable-name line).
        if (auto it = cls->methods.find("display"); it != cls->methods.end()) {
            call(it->second);
            return;
        }
        // `disp` owns the body; the default `display` prints the `name =`
        // header around it (only when shown as a named variable).
        if (auto it = cls->methods.find("disp"); it != cls->methods.end()) {
            if (!name.empty())
                outputText(name + " =\n\n");
            call(it->second);
            if (!name.empty())
                outputText("\n");
            return;
        }
    }
    outputText(formatObjectDisplay(name, obj));
}

// MATLAB operator-overload methods — the single source for the source-token
// ⇆ method-name mapping. The same token can be both binary and unary
// (`-` → minus / uminus), so each row is arity-tagged and lookups
// disambiguate. operatorMethodName / unaryOperatorMethodName /
// isOperatorMethodName are thin views over this table.
struct OpMethod
{
    const char *token;
    const char *method;
    bool unary;
};
static constexpr OpMethod kOpMethods[] = {
    {"+", "plus", false},      {"-", "minus", false},     {"*", "mtimes", false},
    {".*", "times", false},    {"/", "mrdivide", false},  {"./", "rdivide", false},
    {"\\", "mldivide", false}, {".\\", "ldivide", false}, {"^", "mpower", false},
    {".^", "power", false},    {"==", "eq", false},       {"~=", "ne", false},
    {"<", "lt", false},        {"<=", "le", false},       {">", "gt", false},
    {">=", "ge", false},       {"&", "and", false},       {"|", "or", false},
    {"-", "uminus", true},     {"+", "uplus", true},      {"~", "not", true},
    {"'", "ctranspose", true}, {".'", "transpose", true},
};

static const char *operatorMethodName(const std::string &op)
{
    for (const auto &e : kOpMethods)
        if (!e.unary && op == e.token)
            return e.method;
    return nullptr;
}
static const char *unaryOperatorMethodName(const std::string &op)
{
    for (const auto &e : kOpMethods)
        if (e.unary && op == e.token)
            return e.method;
    return nullptr;
}
// True when a classdef method name is an operator-overload method (wired into
// BuiltinClass::ops so `a + b`, `-a`, `a == b`, … dispatch to it).
static bool isOperatorMethodName(const std::string &n)
{
    for (const auto &e : kOpMethods)
        if (n == e.method)
            return true;
    return false;
}

bool Engine::tryObjectBinaryOp(const std::string &op, const Value &lhs, const Value &rhs,
                               Environment *env, Value &out)
{
    if (!lhs.isObject() && !rhs.isObject())
        return false;
    // The dominant object decides the class (first object operand wins —
    // v1 dispatch fidelity, see OBJECT_MODEL.md §3).
    const Value &dom = lhs.isObject() ? lhs : rhs;
    const std::string &clsName = dom.objectClassName();
    const BuiltinClass *cls = findClass(clsName);
    if (const char *mname = operatorMethodName(op); cls && mname) {
        auto it = cls->ops.find(mname);
        if (it != cls->ops.end()) {
            Value self = dom;                 // class context for the hook
            Value operands[2] = {lhs, rhs};   // args in source order
            Value res[1];
            CallContext ctx{this, env};
            it->second(self, Span<const Value>(operands, 2), 1, Span<Value>(res, 1), ctx);
            out = std::move(res[0]);
            return true;
        }
    }
    throw std::runtime_error("Undefined operator '" + op
                             + "' for input arguments of type '" + clsName + "'.");
}

bool Engine::tryObjectUnaryOp(const std::string &op, const Value &operand,
                              Environment *env, Value &out)
{
    if (!operand.isObject())
        return false;
    const std::string &clsName = operand.objectClassName();
    const BuiltinClass *cls = findClass(clsName);
    if (const char *mname = unaryOperatorMethodName(op); cls && mname) {
        auto it = cls->ops.find(mname);
        if (it != cls->ops.end()) {
            Value self = operand;
            Value res[1];
            CallContext ctx{this, env};
            it->second(self, Span<const Value>(nullptr, 0), 1, Span<Value>(res, 1), ctx);
            out = std::move(res[0]);
            return true;
        }
    }
    // Transpose has a builtin array meaning for any object array (reorder
    // elements), so with no class override fall through to the registered
    // builtin transpose instead of erroring. Other unary operators
    // (uminus/uplus/not) are overload-only → undefined without one.
    if (op == "'" || op == ".'")
        return false;
    throw std::runtime_error("Undefined operator '" + op
                             + "' for input arguments of type '" + clsName + "'.");
}

const BytecodeChunk *Engine::resolveBinaryOpChunk(const std::string &op, const Value &lhs,
                                                  const Value &rhs, std::string &ownerClassOut)
{
    if (!lhs.isObject() && !rhs.isObject())
        return nullptr; // numeric fast path
    const Value &dom = lhs.isObject() ? lhs : rhs; // first object operand wins
    const std::string &cn = dom.objectClassName();
    const BuiltinClass *cls = findClass(cn);
    const char *mname = operatorMethodName(op);
    if (!cls || !mname)
        return nullptr;
    // methodFns holds user-defined operator methods (real UserFunctions);
    // synthetic enum eq/ne live only in `ops` (no UserFunction) → not found
    // here → caller's slow path handles them. Same membership as `ops` for
    // genuine overloads.
    auto it = cls->methodFns.find(mname);
    if (it == cls->methodFns.end())
        return nullptr;
    const BytecodeChunk *cc = ensureClassMethodChunk(*it->second);
    if (!cc)
        return nullptr; // uncompilable body → slow path (callReentrant / TW)
    enforceMethodAccess(cn, mname);
    ownerClassOut = cn;
    return cc;
}

const BytecodeChunk *Engine::resolveUnaryOpChunk(const std::string &op, const Value &operand,
                                                 std::string &ownerClassOut)
{
    if (!operand.isObject())
        return nullptr;
    const std::string &cn = operand.objectClassName();
    const BuiltinClass *cls = findClass(cn);
    const char *mname = unaryOperatorMethodName(op);
    if (!cls || !mname)
        return nullptr;
    auto it = cls->methodFns.find(mname);
    if (it == cls->methodFns.end())
        return nullptr; // no override → slow path (transpose→builtin, else throw)
    const BytecodeChunk *cc = ensureClassMethodChunk(*it->second);
    if (!cc)
        return nullptr;
    enforceMethodAccess(cn, mname);
    ownerClassOut = cn;
    return cc;
}

// Defined below; forward-declared so the subsref/subsasgn resolvers (which run
// before its definition) can marshal the substruct argument.
static Value buildSubsStruct(const char *type, Span<const Value> subscripts,
                             std::pmr::memory_resource *mr);

const BytecodeChunk *Engine::resolveSubsrefChunk(const Value &self, Span<const Value> idx,
                                                 std::string &ownerClassOut,
                                                 std::vector<Value> &argsOut)
{
    const BuiltinClass *cls = findClass(self.objectClassName());
    if (!cls || !cls->subsref)
        return nullptr;
    auto it = cls->methodFns.find("subsref");
    if (it == cls->methodFns.end())
        return nullptr; // builtin/native subsref (no UserFunction) → slow path
    const BytecodeChunk *cc = ensureClassMethodChunk(*it->second);
    if (!cc)
        return nullptr;
    Value s = buildSubsStruct("()", idx, resource());
    argsOut.clear();
    argsOut.reserve(2);
    argsOut.push_back(self);          // subsref(obj, S)
    argsOut.push_back(std::move(s));
    ownerClassOut = self.objectClassName();
    return cc;
}

const BytecodeChunk *Engine::resolveSubsasgnChunk(const Value &self, Span<const Value> idxAndVal,
                                                  std::string &ownerClassOut,
                                                  std::vector<Value> &argsOut)
{
    const BuiltinClass *cls = findClass(self.objectClassName());
    if (!cls || !cls->subsasgn)
        return nullptr;
    auto it = cls->methodFns.find("subsasgn");
    if (it == cls->methodFns.end())
        return nullptr;
    const BytecodeChunk *cc = ensureClassMethodChunk(*it->second);
    if (!cc)
        return nullptr;
    // Hook convention: idxAndVal = [subscripts…, value] (value last).
    const size_t nsub = idxAndVal.empty() ? 0 : idxAndVal.size() - 1;
    Value s = buildSubsStruct("()", Span<const Value>(idxAndVal.data(), nsub), resource());
    argsOut.clear();
    argsOut.reserve(3);
    argsOut.push_back(self);          // subsasgn(obj, S, val)
    argsOut.push_back(std::move(s));
    argsOut.push_back(nsub < idxAndVal.size() ? idxAndVal[nsub] : Value());
    ownerClassOut = self.objectClassName();
    return cc;
}

bool Engine::tryObjectSubsref(Value &self, Span<const Value> args, std::size_t nargout,
                              Value &out, Environment *env)
{
    const BuiltinClass *cls = findClass(self.objectClassName());
    if (!cls || !cls->subsref)
        return false;
    Value res[1];
    CallContext ctx{this, env};
    cls->subsref(self, args, nargout, Span<Value>(res, 1), ctx);
    out = std::move(res[0]);
    return true;
}

void Engine::objectStoreSlice(Value &dst, const std::vector<std::vector<size_t>> &perDim,
                              const Value &val, Environment *env)
{
    const BuiltinClass *cls = findClass(val.objectClassName());
    Value fill; // default element for grown gaps (class no-arg constructor)
    if (cls && cls->construct) {
        CallContext ctx{this, env};
        fill = cls->construct(Span<const Value>(nullptr, 0), ctx);
    }
    if (perDim.size() == 1)
        dst.objectAssignLinear(perDim[0], val, fill, resource());
    else
        dst.objectAssignND(perDim, val, fill, resource());
}

// ── classdef → BuiltinClass adapter ──────────────────────────
// Descriptor captured by the synthesised BuiltinClass hooks; kept alive by
// the std::functions in the class registry, and stored in classDefs_ so a
// subclass can merge its bases. (Namespace-scope to match the engine.hpp
// forward declaration used by Engine::classDefs_.)
// classdef member-access levels (private/protected enforcement). `Immutable`
// is a SetAccess-only level: the property is settable only inside its
// declaring class's constructor.
enum class Access
{
    Public,
    Protected,
    Private,
    Immutable
};

// A property: name, default value, and per-side access. `declClass` is the
// class that declared it (needed because protected access admits subclasses,
// and inherited properties keep their original declaring class).
struct PropInfo
{
    std::string name;
    Value def;
    Access getAccess = Access::Public;
    Access setAccess = Access::Public;
    std::string declClass;
    bool isConstant = false; // `Constant` → also exposed as ClassName.Prop
    bool isDependent = false; // `properties (Dependent)` → computed via get.Prop
};
// Method access recorded ONLY for non-public methods (absent == public →
// no check, no overhead). `declClass` as above.
struct MethodAccess
{
    Access level = Access::Public;
    std::string declClass;
};
// A Static method: callable as `ClassName.method(args)`. Stored so it can be
// inherited (re-registered under a subclass name) and access-checked.
struct StaticInfo
{
    std::shared_ptr<UserFunction> uf;
    Access access = Access::Public;
    std::string declClass;
};

struct ClassDefDesc
{
    std::string name;
    bool isHandle = false;
    std::vector<PropInfo> props;                                     // name+default+access
    bool anyNonPublicProp = false;                                   // fast-path gate
    std::shared_ptr<UserFunction> ctor;                              // null if none
    std::unordered_map<std::string, std::shared_ptr<UserFunction>> methods;
    std::unordered_map<std::string, std::shared_ptr<UserFunction>> getters; // get.Prop
    std::unordered_map<std::string, std::shared_ptr<UserFunction>> setters; // set.Prop
    std::unordered_map<std::string, StaticInfo> statics;             // Static methods
    std::vector<std::string> superclasses;                           // transitive, for isa
    // Non-public methods only (public == absent).
    std::unordered_map<std::string, MethodAccess> methodAccess;
    // Constructor access (this class's OWN ctor). Public unless the ctor sits
    // in a `methods (Access = private|protected)` block.
    Access ctorAccess = Access::Public;
    std::string ctorDeclClass;
    bool isEnum = false;                          // has an `enumeration` block
    std::vector<std::string> abstractMethods;     // declared Abstract (own + inherited)
    bool isAbstract = false;                      // an abstract method is unimplemented
    bool isSealed = false;                        // `Sealed` — cannot be subclassed
    std::vector<std::string> hiddenMembers;       // `Hidden` props/methods (own + inherited)
};

// Translate an attribute keyword to an Access level. `Immutable` is only
// meaningful for SetAccess. Friend-class access (`Access = ?Foo` / a `{?A,?B}`
// list) is NOT a silent downgrade here — the `?` is rejected at lex time, so
// such a class fails to load loudly and never reaches this function. The
// default below therefore only covers `public` and any access keyword we don't
// yet model, which fall back to Public (permissive, forward-compatible).
static Access parseAccessLevel(const std::string &v)
{
    if (v == "private")
        return Access::Private;
    if (v == "protected")
        return Access::Protected;
    if (v == "immutable")
        return Access::Immutable;
    return Access::Public; // "public" + any not-yet-modeled keyword
}

// Inverse of parseAccessLevel — the MATLAB attribute string for reflection
// (metaclass / meta.property / meta.method GetAccess/SetAccess/Access).
static const char *accessName(Access a)
{
    switch (a) {
    case Access::Private:   return "private";
    case Access::Protected: return "protected";
    case Access::Immutable: return "immutable";
    default:                return "public";
    }
}

// Resolve a properties/methods block's flattened attribute tokens
// (e.g. {"Access","private","SetAccess","immutable"}) into get/set levels.
// `Access` sets the default for both sides; `GetAccess` / `SetAccess`
// override their side. Returns false in `any` when every side stays public.
struct BlockAccess
{
    Access get = Access::Public;
    Access set = Access::Public;
    bool any = false;
};
static BlockAccess parseBlockAccess(const std::vector<std::string> &attrs)
{
    BlockAccess ba;
    auto valueOf = [&](const char *key) -> const std::string * {
        for (size_t i = 0; i + 1 < attrs.size(); ++i)
            if (attrs[i] == key)
                return &attrs[i + 1];
        return nullptr;
    };
    // `Access` is the default for both sides; the specific `GetAccess` /
    // `SetAccess` always override it (MATLAB is order-independent here, so
    // resolve the general first, then the specific).
    if (const std::string *a = valueOf("Access")) {
        Access lvl = parseAccessLevel(*a);
        ba.get = (lvl == Access::Immutable) ? Access::Public : lvl; // immutable is set-only
        ba.set = lvl;
    }
    if (const std::string *g = valueOf("GetAccess")) {
        Access lvl = parseAccessLevel(*g);
        ba.get = (lvl == Access::Immutable) ? Access::Public : lvl;
    }
    if (const std::string *s = valueOf("SetAccess"))
        ba.set = parseAccessLevel(*s);
    ba.any = ba.get != Access::Public || ba.set != Access::Public;
    return ba;
}

static const char *accessWord(Access a)
{
    switch (a) {
    case Access::Private:
        return "private";
    case Access::Protected:
        return "protected";
    case Access::Immutable:
        return "immutable";
    default:
        return "public";
    }
}

// Throw a MATLAB-style error when the current execution context may not
// touch a member with the given access level. `kind` is "property" or
// "method". Public members and allowed contexts return silently.
static void enforceAccess(Engine *engine, Access level, const std::string &declClass,
                          const char *kind, const std::string &name)
{
    if (level == Access::Public)
        return;
    if (level == Access::Immutable) {
        // Settable only inside the declaring class's own constructor.
        if (engine->classCtxInCtorOf(declClass))
            return;
        throw std::runtime_error("Cannot set property '" + name
                                 + "': it has immutable SetAccess (settable only in the "
                                 + declClass + " constructor)");
    }
    if (engine->classCtxAllows(declClass, /*privateOnly=*/level == Access::Private))
        return;
    throw std::runtime_error(std::string("Cannot access ") + kind + " '" + name
                             + "' of class '" + declClass + "': it has " + accessWord(level)
                             + " access");
}

// Find a property by name in a class descriptor (linear; property counts are
// small, and access checks only reach here when the class has a non-public
// property). Returns nullptr if absent.
static const PropInfo *findProp(const ClassDefDesc &d, const std::string &name)
{
    for (const auto &p : d.props)
        if (p.name == name)
            return &p;
    return nullptr;
}

// Build the MATLAB subscript struct `S` passed to a user subsref/subsasgn
// method: a 1×1 struct with `.type` (e.g. "()") and `.subs` (a 1×N cell of
// the subscripts). Our object index hooks only fire for paren-indexing, so
// the type is always "()".
static Value buildSubsStruct(const char *type, Span<const Value> subscripts,
                             std::pmr::memory_resource *mr)
{
    Value subs = Value::cell(1, subscripts.size(), mr);
    for (size_t i = 0; i < subscripts.size(); ++i)
        subs.cellAt(i) = subscripts[i];
    Value s = Value::structure(mr);
    s.setField(0, "type", Value::fromString(type, mr));
    s.setField(0, "subs", subs);
    return s;
}

void Engine::registerClassDef(const ASTNode *cd)
{
    if (!cd || cd->type != NodeType::CLASSDEF_DEF || cd->strValue.empty())
        return;
    if (findClass(cd->strValue))
        return; // idempotent — already registered this session

    auto desc = std::make_shared<ClassDefDesc>();
    desc->name = cd->strValue;
    for (const auto &super : cd->paramNames)
        if (super == "handle")
            desc->isHandle = true;

    auto hasAttr = [](const ASTNode *n, const char *a) {
        for (const auto &s : n->classAttrs)
            if (s == a)
                return true;
        return false;
    };
    desc->isSealed = hasAttr(cd, "Sealed"); // class-level attribute
    for (const auto &childPtr : cd->children) {
        const ASTNode *child = childPtr.get();
        if (child->type == NodeType::CLASSDEF_ENUM_MEMBER) {
            desc->isEnum = true; // members are instantiated after the class is built
            continue;
        }
        if (child->type == NodeType::CLASSDEF_PROPERTY) {
            Value def = Value::Empty;
            if (!child->children.empty() && treeWalker_)
                def = treeWalker_->evalExpressionPublic(child->children[0].get(),
                                                        &constantsEnv());
            // Record non-public get/set access (this class is the declarer).
            // A `Constant` property is also exposed as `ClassName.Prop`; the
            // qualified external is registered after the merge (so inherited
            // constants resolve under a subclass too).
            BlockAccess ba = parseBlockAccess(child->classAttrs);
            desc->props.push_back(
                {child->strValue, def, ba.get, ba.set, desc->name,
                 hasAttr(child, "Constant"), hasAttr(child, "Dependent")});
            desc->anyNonPublicProp = desc->anyNonPublicProp || ba.any;
            if (hasAttr(child, "Hidden"))
                desc->hiddenMembers.push_back(child->strValue);
        } else if (child->type == NodeType::FUNCTION_DEF) {
            // Abstract method: a signature with no body (no children[0]).
            // Record the name; a concrete subclass implements it. The class
            // stays abstract (uninstantiable) until every such name is
            // implemented.
            if (hasAttr(child, "Abstract")) {
                desc->abstractMethods.push_back(child->strValue);
                continue;
            }
            auto uf = std::make_shared<UserFunction>();
            uf->name = desc->name + ">" + child->strValue;
            uf->params = child->paramNames;
            uf->returns = child->returnNames;
            uf->body = std::shared_ptr<const ASTNode>(cloneNode(child->children[0].get()));
            uf->closureEnv = nullptr;
            uf->ownerClass = desc->name; // member-access execution context
            const std::string &mn = child->strValue;
            if (mn.rfind("get.", 0) == 0) {
                desc->getters[mn.substr(4)] = uf; // property get accessor
            } else if (mn.rfind("set.", 0) == 0) {
                desc->setters[mn.substr(4)] = uf; // property set accessor
            } else if (hasAttr(child, "Static")) {
                // Static method: callable as `ClassName.method(args)` (no
                // self). Stored now; the qualified external is registered
                // after the merge (so a subclass inherits it). `immutable` is
                // meaningless for a method.
                BlockAccess ba = parseBlockAccess(child->classAttrs);
                Access slvl = (ba.set == Access::Immutable) ? Access::Public : ba.set;
                desc->statics[child->strValue] = {uf, slvl, desc->name};
            } else if (child->strValue == desc->name) {
                desc->ctor = uf; // constructor: method named like the class
                BlockAccess ba = parseBlockAccess(child->classAttrs);
                Access clvl = (ba.set == Access::Immutable) ? Access::Public : ba.set;
                if (clvl != Access::Public) {
                    desc->ctorAccess = clvl;
                    desc->ctorDeclClass = desc->name;
                }
            } else {
                desc->methods[child->strValue] = uf;
                // Record non-public method access (Access sets the level;
                // immutable is meaningless for a method → treat as public).
                BlockAccess ba = parseBlockAccess(child->classAttrs);
                Access lvl = (ba.set == Access::Immutable) ? Access::Public : ba.set;
                if (lvl != Access::Public)
                    desc->methodAccess[child->strValue] = {lvl, desc->name};
                if (hasAttr(child, "Hidden"))
                    desc->hiddenMembers.push_back(child->strValue);
            }
        }
    }

    // Method names declared by THIS class, captured before the merge mutates
    // the descriptor — so an inherited method-access entry never overrides a
    // method the derived class declares itself (a public override of a
    // protected base method stays public). Properties need no such capture:
    // their access travels inside PropInfo, and a derived property fully
    // replaces the inherited entry in the merge below.
    std::vector<std::string> ownMethodNames;
    ownMethodNames.reserve(desc->methods.size());
    for (const auto &[mn, uf] : desc->methods)
        ownMethodNames.push_back(mn);

    // Make inheritance order-independent: if a superclass is a file-based
    // class not yet loaded, pull it in from the path now (it registers as a
    // side effect). Without this, `Derived(...)` referenced before `Base`
    // would register Derived with the base members missing. Inline classes
    // that aren't on the path simply stay unregistered (recorded for isa).
    for (const auto &superName : cd->paramNames)
        if (superName != "handle" && !classDefs_.count(superName))
            resolveMFile_(superName);

    // ── Inheritance: merge registered superclasses (base members first,
    // derived overrides). `handle` is the semantics marker, not a classdef.
    for (const auto &superName : cd->paramNames) {
        if (superName == "handle")
            continue;
        auto bit = classDefs_.find(superName);
        if (bit == classDefs_.end())
            continue; // unknown / non-classdef base — skip (still recorded below)
        const auto &base = bit->second;
        if (base->isSealed)
            throw std::runtime_error("Cannot subclass sealed class '" + superName + "'");
        desc->isHandle = desc->isHandle || base->isHandle;
        // Properties: base first; a derived property of the same name fully
        // replaces the inherited entry (default + access + declaring class),
        // so a public override of a protected base property stays public.
        std::vector<PropInfo> merged = base->props;
        for (const auto &dp : desc->props) {
            auto it = std::find_if(merged.begin(), merged.end(),
                                   [&](const PropInfo &m) { return m.name == dp.name; });
            if (it != merged.end())
                *it = dp;
            else
                merged.push_back(dp);
        }
        desc->props = std::move(merged);
        desc->anyNonPublicProp = desc->anyNonPublicProp || base->anyNonPublicProp;
        // Methods + accessors: inherit those the derived class doesn't override.
        for (const auto &[mn, uf] : base->methods)
            if (!desc->methods.count(mn))
                desc->methods[mn] = uf;
        for (const auto &[pn, uf] : base->getters)
            if (!desc->getters.count(pn))
                desc->getters[pn] = uf;
        for (const auto &[pn, uf] : base->setters)
            if (!desc->setters.count(pn))
                desc->setters[pn] = uf;
        // Static methods: inherit those not overridden (keeping the base's
        // declaring class for the access check).
        for (const auto &[sn, si] : base->statics)
            if (!desc->statics.count(sn))
                desc->statics[sn] = si;
        // Abstract method obligations propagate to the subclass (satisfied
        // when the subclass provides a concrete method of the same name).
        for (const auto &am : base->abstractMethods)
            if (std::find(desc->abstractMethods.begin(), desc->abstractMethods.end(), am)
                == desc->abstractMethods.end())
                desc->abstractMethods.push_back(am);
        // Hidden members stay hidden in the subclass.
        for (const auto &hm : base->hiddenMembers)
            if (std::find(desc->hiddenMembers.begin(), desc->hiddenMembers.end(), hm)
                == desc->hiddenMembers.end())
                desc->hiddenMembers.push_back(hm);
        // Method access: inherit base entries (keeping the base's declaring
        // class so `protected` still admits this subclass) unless the derived
        // class declared the method itself (its own access — or public by
        // omission — then wins).
        for (const auto &[mn, ma] : base->methodAccess)
            if (std::find(ownMethodNames.begin(), ownMethodNames.end(), mn)
                == ownMethodNames.end())
                desc->methodAccess.emplace(mn, ma);
        if (!desc->ctor) {
            // Inherit the base constructor — and its access, so a subclass
            // without its own ctor honours a private/protected base ctor
            // (the declaring class stays the base, for protected/subclass).
            desc->ctor = base->ctor;
            desc->ctorAccess = base->ctorAccess;
            desc->ctorDeclClass = base->ctorDeclClass;
        }
    }
    // Ancestry (transitive) for isa(): direct supers + their ancestors.
    for (const auto &superName : cd->paramNames) {
        if (superName == "handle")
            continue;
        desc->superclasses.push_back(superName);
        auto bit = classDefs_.find(superName);
        if (bit != classDefs_.end())
            for (const auto &a : bit->second->superclasses)
                if (std::find(desc->superclasses.begin(), desc->superclasses.end(), a)
                    == desc->superclasses.end())
                    desc->superclasses.push_back(a);
    }
    // A class is abstract — uninstantiable — while any declared abstract
    // method has no concrete implementation (own or inherited).
    for (const auto &am : desc->abstractMethods)
        if (!desc->methods.count(am)) {
            desc->isAbstract = true;
            break;
        }
    classDefs_[desc->name] = desc;

    // Register `ClassName.member` qualified externals for every Static method
    // and Constant property — own AND inherited (the merged maps above hold
    // both) — so e.g. `Subclass.baseStatic()` / `Subclass.BASE_CONST` resolve.
    for (const auto &[sname, si] : desc->statics) {
        auto uf = si.uf;
        Access acc = si.access;
        std::string decl = si.declClass, mn = sname;
        registerFunction(desc->name, sname,
                         [uf, acc, decl, mn](Span<const Value> args, size_t nargout,
                                             Span<Value> outs, CallContext &ctx) {
                             if (acc != Access::Public)
                                 enforceAccess(ctx.engine, acc, decl, "method", mn);
                             const size_t nout = std::max<size_t>(nargout, 1);
                             auto results = ctx.engine->invokeClassMethod(*uf, args, nout);
                             const size_t writeN = std::min(nout, results.size());
                             for (size_t i = 0; i < writeN && i < outs.size(); ++i)
                                 outs[i] = std::move(results[i]);
                         });
    }
    for (const auto &p : desc->props) {
        if (!p.isConstant)
            continue;
        Value cval = p.def;
        Access getLvl = p.getAccess;
        std::string decl = p.declClass, pn = p.name;
        registerFunction(desc->name, p.name,
                         [cval, getLvl, decl, pn](Span<const Value>, size_t, Span<Value> outs,
                                                  CallContext &ctx) {
                             if (getLvl != Access::Public)
                                 enforceAccess(ctx.engine, getLvl, decl, "property", pn);
                             outs[0] = cval;
                         });
    }

    BuiltinClass cls;
    cls.name = desc->name;
    cls.isHandle = desc->isHandle;
    cls.hidden = desc->hiddenMembers;
    cls.propNames.reserve(desc->props.size());
    for (const auto &p : desc->props)
        if (std::find(cls.hidden.begin(), cls.hidden.end(), p.name) == cls.hidden.end())
            cls.propNames.push_back(p.name); // Hidden props omitted from properties()/disp
    cls.superclasses = desc->superclasses;
    cls.isSealed = desc->isSealed;
    cls.isAbstract = desc->isAbstract;
    // ── Reflection metadata (metaclass / meta.property / meta.method) ──
    // Mirrors the merged classdef member attributes into the runtime class so
    // introspection needn't reach back into ClassDefDesc. Hidden members are
    // omitted, matching properties()/methods().
    {
        auto isHidden = [&](const std::string &n) {
            return std::find(desc->hiddenMembers.begin(), desc->hiddenMembers.end(), n)
                   != desc->hiddenMembers.end();
        };
        for (const auto &p : desc->props)
            if (!isHidden(p.name))
                cls.propMeta.push_back({p.name, accessName(p.getAccess),
                                        accessName(p.setAccess), p.isConstant, p.isDependent});
        std::map<std::string, MethodMeta> mm; // dedup + deterministic order
        for (const auto &[name, uf] : desc->methods) {
            MethodMeta m;
            m.name = name;
            if (auto it = desc->methodAccess.find(name); it != desc->methodAccess.end())
                m.access = accessName(it->second.level);
            mm[name] = m;
        }
        for (const auto &[name, si] : desc->statics) {
            MethodMeta &m = mm[name];
            m.name = name;
            m.isStatic = true;
            m.access = accessName(si.access);
        }
        for (const auto &name : desc->abstractMethods) {
            MethodMeta &m = mm[name];
            m.name = name;
            m.isAbstract = true;
        }
        for (auto &[name, m] : mm)
            if (!isHidden(name))
                cls.methodMeta.push_back(m);
    }
    cls.propGet = [desc](const Value &self, const std::string &name, Value &out,
                         CallContext &ctx) -> bool {
        // Enforce GetAccess for restricted properties (a class with only
        // public properties skips the lookup entirely).
        if (desc->anyNonPublicProp)
            if (const PropInfo *pi = findProp(*desc, name);
                pi && pi->getAccess != Access::Public)
                enforceAccess(ctx.engine, pi->getAccess, pi->declClass, "property", name);
        // A `get.Prop` accessor overrides the stored value.
        auto git = desc->getters.find(name);
        if (git != desc->getters.end()) {
            auto r = ctx.engine->invokeClassMethod(*git->second,
                                                   Span<const Value>(&self, 1), 1);
            out = r.empty() ? Value() : std::move(r[0]);
            return true;
        }
        const auto *st = self.objectStateConst();
        if (!st)
            return false;
        auto it = st->props.find(name);
        if (it == st->props.end())
            return false;
        out = it->second;
        return true;
    };
    cls.propSet = [desc](Value &self, const std::string &name, const Value &val,
                         CallContext &ctx) -> bool {
        // Enforce SetAccess (incl. immutable) for restricted properties.
        if (desc->anyNonPublicProp)
            if (const PropInfo *pi = findProp(*desc, name);
                pi && pi->setAccess != Access::Public)
                enforceAccess(ctx.engine, pi->setAccess, pi->declClass, "property", name);
        // A `set.Prop` accessor overrides the store. A value-class accessor
        // returns the modified object (`function obj = set.Prop(obj, v)`),
        // which we write back; a handle accessor mutates in place.
        auto sit = desc->setters.find(name);
        if (sit != desc->setters.end()) {
            Value callArgs[2] = {self, val};
            auto r = ctx.engine->invokeClassMethod(*sit->second,
                                                   Span<const Value>(callArgs, 2), 1);
            if (!r.empty() && r[0].isObject())
                self = std::move(r[0]);
            return true;
        }
        self.objectStateMut()->props[name] = val;
        return true;
    };
    cls.construct = [desc](Span<const Value> args, CallContext &ctx) -> Value {
        Value obj = ctx.engine->makeDefaultInstance(desc->name);
        if (desc->ctor)
            obj = ctx.engine->invokeClassCtor(*desc->ctor, obj, args);
        return obj;
    };
    for (const auto &[mname, uf] : desc->methods) {
        auto ufCopy = uf; // shared_ptr keeps the body alive
        cls.methodFns[mname] = uf; // classdef method → VM can run it as a frame
        std::string nameCopy = mname;
        Access mlevel = Access::Public;
        std::string mdecl;
        if (auto mit = desc->methodAccess.find(mname); mit != desc->methodAccess.end()) {
            mlevel = mit->second.level;
            mdecl = mit->second.declClass;
        }
        cls.methods[mname] = [ufCopy, nameCopy, mlevel, mdecl](
                                 Value &self, Span<const Value> args, size_t nargout,
                                 Span<Value> outs, CallContext &ctx) {
            if (mlevel != Access::Public)
                enforceAccess(ctx.engine, mlevel, mdecl, "method", nameCopy);
            std::vector<Value> callArgs;
            callArgs.reserve(args.size() + 1);
            callArgs.push_back(self); // MATLAB: obj is the first parameter
            for (const auto &a : args)
                callArgs.push_back(a);
            const size_t nout = std::max<size_t>(nargout, 1);
            auto results = ctx.engine->invokeClassMethod(
                *ufCopy, Span<const Value>(callArgs.data(), callArgs.size()), nout);
            // Always produce at least one output (the `ans`/expression value
            // when called with nargout==0), bounded by the outs span.
            const size_t writeN = std::min(nout, results.size());
            for (size_t i = 0; i < writeN && i < outs.size(); ++i)
                outs[i] = std::move(results[i]);
        };
        // Operator-overload method (plus/minus/eq/uminus/…): also wire it into
        // `ops` so `a + b`, `-a`, `a == b`, … dispatch here. Unlike a regular
        // method, an operator method's parameters ARE its operands (no `self`
        // is prepended): binary ops arrive as `args` = [lhs, rhs]; a unary op
        // arrives as the receiver `self` with empty `args`.
        if (isOperatorMethodName(mname)) {
            auto ufOp = uf;
            cls.ops[mname] = [ufOp, nameCopy, mlevel, mdecl](
                                 Value &self, Span<const Value> args, size_t nargout,
                                 Span<Value> outs, CallContext &ctx) {
                if (mlevel != Access::Public)
                    enforceAccess(ctx.engine, mlevel, mdecl, "method", nameCopy);
                std::vector<Value> callArgs;
                if (args.empty())
                    callArgs.push_back(self); // unary: operand came as self
                else
                    callArgs.assign(args.begin(), args.end()); // binary: [lhs, rhs]
                const size_t nout = std::max<size_t>(nargout, 1);
                auto results = ctx.engine->invokeClassMethod(
                    *ufOp, Span<const Value>(callArgs.data(), callArgs.size()), nout);
                const size_t writeN = std::min(nout, results.size());
                for (size_t i = 0; i < writeN && i < outs.size(); ++i)
                    outs[i] = std::move(results[i]);
            };
        }
    }
    // Custom indexing: a `subsref` / `subsasgn` method overrides `obj(...)`
    // read / assignment. The user method takes MATLAB's substruct form
    // (subsref(obj, S) / subsasgn(obj, S, val)); our paren-index hooks pass
    // flat subscripts, which we wrap into S here. (`.`/`{}` keep their default
    // property / cell semantics — only `()` routes through these.)
    if (auto it = desc->methods.find("subsref"); it != desc->methods.end()) {
        auto uf = it->second;
        cls.subsref = [uf](Value &self, Span<const Value> args, size_t nargout, Span<Value> outs,
                           CallContext &ctx) {
            Value s = buildSubsStruct("()", args, ctx.engine->resource());
            Value callArgs[2] = {self, s};
            const size_t nout = std::max<size_t>(nargout, 1);
            auto results = ctx.engine->invokeClassMethod(*uf, Span<const Value>(callArgs, 2), nout);
            const size_t writeN = std::min(nout, results.size());
            for (size_t i = 0; i < writeN && i < outs.size(); ++i)
                outs[i] = std::move(results[i]);
        };
    }
    if (auto it = desc->methods.find("subsasgn"); it != desc->methods.end()) {
        auto uf = it->second;
        cls.subsasgn = [uf](Value &self, Span<const Value> args, size_t, Span<Value>,
                            CallContext &ctx) {
            // Hook convention: args = [subscripts…, value] (value last).
            const size_t nsub = args.empty() ? 0 : args.size() - 1;
            Value s = buildSubsStruct("()", Span<const Value>(args.data(), nsub),
                                      ctx.engine->resource());
            Value callArgs[3] = {self, s, nsub < args.size() ? args[nsub] : Value()};
            auto results = ctx.engine->invokeClassMethod(*uf, Span<const Value>(callArgs, 3), 1);
            // Value class: subsasgn returns the modified object → write back.
            // Handle class: it mutates shared state in place (write-back of the
            // returned handle is harmless).
            if (!results.empty() && results[0].isObject())
                self = std::move(results[0]);
        };
    }
    cls.dispText = [desc](const Value &self) -> std::string {
        std::string body = "  " + desc->name + " with properties:\n\n";
        const auto *st = self.objectStateConst();
        for (const auto &p : desc->props) {
            body += "    " + p.name;
            if (st) {
                auto it = st->props.find(p.name);
                if (it != st->props.end() && it->second.isScalar()
                    && it->second.type() == ValueType::DOUBLE)
                    body += ": " + std::to_string(it->second.toScalar());
            }
            body += "\n";
        }
        return body;
    };
    // Enumeration defaults: members compare by their member name (so
    // `Color.Red == Color.Red`), and display as that name — unless the class
    // defines its own eq / disp.
    if (desc->isEnum) {
        auto enumEq = [](Value &, Span<const Value> args, size_t, Span<Value> outs,
                         CallContext &ctx) {
            bool eq = false;
            if (args.size() == 2 && args[0].isObject() && args[1].isObject()
                && args[0].objectClassName() == args[1].objectClassName()) {
                const auto *a = args[0].objectStateConst();
                const auto *b = args[1].objectStateConst();
                eq = a && b && !a->enumName.empty() && a->enumName == b->enumName;
            }
            outs[0] = Value::logicalScalar(eq, ctx.engine->resource());
        };
        if (!cls.ops.count("eq"))
            cls.ops["eq"] = enumEq;
        if (!cls.ops.count("ne"))
            cls.ops["ne"] = [enumEq](Value &self, Span<const Value> args, size_t no,
                                     Span<Value> outs, CallContext &ctx) {
                enumEq(self, args, no, outs, ctx);
                if (!outs.empty())
                    outs[0] = Value::logicalScalar(!outs[0].toBool(), ctx.engine->resource());
            };
        if (!desc->methods.count("disp") && !desc->methods.count("display"))
            cls.dispText = [](const Value &self) -> std::string {
                const auto *st = self.objectStateConst();
                return (st && !st->enumName.empty()) ? ("    " + st->enumName + "\n") : "";
            };
    }
    registerClass(std::move(cls));

    // Instantiate enumeration members: each is a constant instance exposed as
    // `ClassName.Member`, built by the constructor with the member's args (so
    // an underlying value is set) and tagged with its member name.
    if (desc->isEnum) {
        const BuiltinClass *ecls = findClass(desc->name);
        for (const auto &childPtr : cd->children) {
            const ASTNode *child = childPtr.get();
            if (child->type != NodeType::CLASSDEF_ENUM_MEMBER)
                continue;
            std::vector<Value> args;
            if (treeWalker_)
                for (const auto &a : child->children)
                    args.push_back(
                        treeWalker_->evalExpressionPublic(a.get(), &constantsEnv()));
            CallContext ctx{this, workspaceEnv_.get()};
            Value inst = ecls->construct(Span<const Value>(args.data(), args.size()), ctx);
            if (inst.isObject())
                inst.objectStateMut()->enumName = child->strValue;
            registerFunction(desc->name, child->strValue,
                             [inst](Span<const Value>, size_t, Span<Value> outs,
                                    CallContext &) { outs[0] = inst; });
        }
    }
}

namespace {
// Push the running class onto the access-context stack for the duration of a
// method/constructor body; pops on scope exit (incl. exceptions). The class
// is `uf.ownerClass`, set when the classdef body was registered.
struct ClassCtxGuard
{
    Engine *engine;
    ClassCtxGuard(Engine *e, const std::string &className, bool isCtor)
        : engine(e)
    {
        engine->pushClassCtx(className, isCtor);
    }
    ~ClassCtxGuard() { engine->popClassCtx(); }
    ClassCtxGuard(const ClassCtxGuard &) = delete;
    ClassCtxGuard &operator=(const ClassCtxGuard &) = delete;
};
} // namespace

std::vector<Value> Engine::invokeClassMethod(const UserFunction &uf, Span<const Value> args,
                                             size_t nout)
{
    // Under the VM backend, run the body on the VM (P4) so C++-initiated
    // classdef callbacks — operator methods, subsref/subsasgn, custom
    // disp/display, super-method targets, and method calls made from a builtin —
    // execute on the same engine as the rest of the program. callReentrant
    // carries the class context on the frame (ownerClass), so no ClassCtxGuard
    // is needed. Falls back to the TreeWalker if the body can't VM-compile.
    if (vm_ && backend_ == Backend::VM)
        if (const BytecodeChunk *cc = ensureClassMethodChunk(uf))
            return vm_->callReentrant(*cc, args, std::max<size_t>(nout, 1), uf.ownerClass,
                                      /*isCtor=*/false);
    if (treeWalker_) {
        ClassCtxGuard ctx(this, uf.ownerClass, /*isCtor=*/false);
        return treeWalker_->runClassMethod(uf, args, nout);
    }
    throw std::runtime_error("classdef methods require the interpreter backend");
}

Value Engine::invokeClassCtor(const UserFunction &ctor, const Value &seed,
                              Span<const Value> args)
{
    // VM backend: run the constructor body on the VM (P4), seeding the output
    // variable with the default instance. Reached for C++-initiated
    // construction (object-array growth, constructChecked from a builtin) and
    // super-constructor targets; in-bytecode `ClassName(args)` pushes its ctor
    // frame directly (P2). Falls back to the TreeWalker if it can't VM-compile.
    if (vm_ && backend_ == Backend::VM)
        if (const BytecodeChunk *cc = ensureClassMethodChunk(ctor)) {
            auto r = vm_->callReentrant(*cc, args, 1, ctor.ownerClass, /*isCtor=*/true, &seed);
            return r.empty() ? seed : std::move(r[0]);
        }
    if (treeWalker_) {
        ClassCtxGuard ctx(this, ctor.ownerClass, /*isCtor=*/true);
        return treeWalker_->runClassCtor(ctor, seed, args);
    }
    throw std::runtime_error("classdef constructor requires the interpreter backend");
}

const BytecodeChunk *Engine::ensureClassMethodChunk(const UserFunction &uf)
{
    return compiler_ ? compiler_->ensureClassMethodCompiled(uf) : nullptr;
}

void Engine::enforceMethodAccess(const std::string &className, const std::string &method)
{
    auto it = classDefs_.find(className);
    if (it == classDefs_.end())
        return;
    auto mit = it->second->methodAccess.find(method);
    if (mit != it->second->methodAccess.end())
        enforceAccess(this, mit->second.level, mit->second.declClass, "method", method);
}

const UserFunction *Engine::classGetter(const std::string &className,
                                        const std::string &prop) const
{
    auto it = classDefs_.find(className);
    if (it == classDefs_.end())
        return nullptr;
    auto git = it->second->getters.find(prop);
    return git != it->second->getters.end() ? git->second.get() : nullptr;
}

const UserFunction *Engine::classSetter(const std::string &className,
                                        const std::string &prop) const
{
    auto it = classDefs_.find(className);
    if (it == classDefs_.end())
        return nullptr;
    auto sit = it->second->setters.find(prop);
    return sit != it->second->setters.end() ? sit->second.get() : nullptr;
}

void Engine::enforcePropGetAccess(const std::string &className, const std::string &prop)
{
    auto it = classDefs_.find(className);
    if (it == classDefs_.end() || !it->second->anyNonPublicProp)
        return;
    if (const PropInfo *pi = findProp(*it->second, prop); pi && pi->getAccess != Access::Public)
        enforceAccess(this, pi->getAccess, pi->declClass, "property", prop);
}

void Engine::enforcePropSetAccess(const std::string &className, const std::string &prop)
{
    auto it = classDefs_.find(className);
    if (it == classDefs_.end() || !it->second->anyNonPublicProp)
        return;
    if (const PropInfo *pi = findProp(*it->second, prop); pi && pi->setAccess != Access::Public)
        enforceAccess(this, pi->setAccess, pi->declClass, "property", prop);
}

Value Engine::superConstruct(const std::string &base, const Value &seed,
                             Span<const Value> args)
{
    auto it = classDefs_.find(base);
    if (it == classDefs_.end())
        throw std::runtime_error("superclass '" + base + "' is not a classdef");
    const auto &desc = it->second;
    // A base with no explicit constructor contributes only its default
    // property values, which are already present on `seed` — nothing to run.
    if (!desc->ctor)
        return seed;
    return invokeClassCtor(*desc->ctor, seed, args);
}

std::vector<Value> Engine::superMethod(const std::string &base, const std::string &method,
                                       Span<const Value> args, size_t nout)
{
    auto it = classDefs_.find(base);
    if (it == classDefs_.end())
        throw std::runtime_error("superclass '" + base + "' is not a classdef");
    const auto &desc = it->second;
    auto mit = desc->methods.find(method);
    if (mit == desc->methods.end())
        throw std::runtime_error("superclass '" + base + "' has no method '" + method + "'");
    // Enforce the base method's access from the calling context (the current
    // top frame is the subclass method making the super-call): a `protected`
    // base method is reachable from a subclass, a `private` one is not.
    if (auto ait = desc->methodAccess.find(method); ait != desc->methodAccess.end())
        enforceAccess(this, ait->second.level, ait->second.declClass, "method", method);
    return invokeClassMethod(*mit->second, args, nout);
}

void Engine::pushClassCtx(std::string className, bool isCtor)
{
    classCtx_.push_back({std::move(className), isCtor});
}

void Engine::popClassCtx()
{
    if (!classCtx_.empty())
        classCtx_.pop_back();
}

// The class whose method/constructor is currently executing. A TW callback's
// ClassCtxGuard (classCtx_) is innermost when present; otherwise, under the VM
// backend, the running class is the VM's top method frame. Empty == script
// scope. (No separate VM stack — read on demand, so it stays exception-safe.)
std::string Engine::currentClassCtx_() const
{
    if (!classCtx_.empty())
        return classCtx_.back().className;
    if (vm_ && backend_ == Backend::VM)
        return vm_->currentMethodClass();
    return std::string();
}

bool Engine::classCtxAllows(const std::string &declClass, bool privateOnly) const
{
    const std::string ctx = currentClassCtx_();
    if (ctx.empty())
        return false; // script scope — only public members are reachable
    if (ctx == declClass)
        return true; // the declaring class itself (covers private + protected)
    if (privateOnly)
        return false;
    // protected: the running class must be a subclass of the declaring class.
    auto it = classDefs_.find(ctx);
    if (it == classDefs_.end())
        return false;
    const auto &sc = it->second->superclasses;
    return std::find(sc.begin(), sc.end(), declClass) != sc.end();
}

bool Engine::classCtxInCtorOf(const std::string &declClass) const
{
    if (!classCtx_.empty())
        return classCtx_.back().isCtor && classCtx_.back().className == declClass;
    if (vm_ && backend_ == Backend::VM)
        return vm_->currentMethodIsCtor() && vm_->currentMethodClass() == declClass;
    return false;
}

void Engine::enforceCtorAccess(const std::string &className)
{
    auto it = classDefs_.find(className);
    if (it != classDefs_.end() && it->second->ctorAccess != Access::Public) {
        const Access lvl = it->second->ctorAccess;
        if (!classCtxAllows(it->second->ctorDeclClass, /*privateOnly=*/lvl == Access::Private))
            throw std::runtime_error(std::string("Cannot call the ") + accessWord(lvl)
                                     + " constructor of '" + className
                                     + "' from outside the class");
    }
}

Value Engine::constructChecked(const BuiltinClass *cls, Span<const Value> args, CallContext &ctx)
{
    enforceCtorAccess(cls->name);
    return cls->construct(args, ctx);
}

Value Engine::makeDefaultInstance(const std::string &className)
{
    auto it = classDefs_.find(className);
    if (it == classDefs_.end())
        throw std::runtime_error("'" + className + "' is not a classdef");
    const auto &desc = it->second;
    if (desc->isAbstract)
        throw std::runtime_error("Cannot instantiate abstract class '" + className
                                 + "' (unimplemented abstract method)");
    auto st = std::make_shared<ObjectState>(mr_);
    for (const auto &p : desc->props)
        st->props.emplace(p.name, p.def);
    return Value::object(desc->name, st, desc->isHandle, mr_);
}

const UserFunction *Engine::classCtor(const std::string &className) const
{
    auto it = classDefs_.find(className);
    if (it == classDefs_.end() || !it->second->ctor)
        return nullptr;
    return it->second->ctor.get();
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
    // NOTE (deep audit): this consults globalsEnv_ unconditionally, which leaks
    // a function's `global G; G=5` into the base workspace (who / IDE variable
    // viewer). The correct gate is `workspaceEnv_->isGlobal(name)`, BUT that
    // alone breaks the legitimate case (`global gv` then read gv) because a
    // VM top-level `global X` does not currently propagate its global-membership
    // back into workspaceEnv_->globals_ (only globalsEnv_ gets the value). Fix
    // requires that membership sync first; deferred to avoid regressing real
    // global use. See FrameIntrospectionEdgesTest.GlobalInsideEvalinBase.
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

void Engine::registerBuiltinMSource(const std::string &src)
{
    Lexer lexer(src);
    Parser parser(lexer.tokenize());
    auto ast = parser.parse();
    // Persistently register one parsed `function` def (mirrors the m-file
    // loader): VM compiled chunk via registerFunctionAs + TW via userFuncs_.
    auto adopt = [this](const ASTNode *fd) {
        UserFunction func;
        func.name = fd->strValue;
        func.params = fd->paramNames;
        func.returns = fd->returnNames;
        func.body = std::shared_ptr<const ASTNode>(cloneNode(fd->children[0].get()));
        func.closureEnv = nullptr;
        if (compiler_) {
            try {
                compiler_->registerFunctionAs(func.name, fd);
            } catch (const RegisterExhaustionError &) {
                // Our own embedded wrappers MUST run on the VM (that is the
                // whole point — pausability). A too-large chunk would silently
                // drop to TW-only and surface later as a misleading "undefined
                // function" on the VM. Fail loudly at registration instead.
                throw Error("registerBuiltinMSource: embedded function '" + func.name
                    + "' needs more than the 255-register VM limit in one chunk — "
                    "split it into helper functions (see docs/CALLBACK_PAUSABILITY.md)",
                    0, 0, "registerBuiltinMSource", "",
                    "numkit:compiler:registerExhaustion");
            } catch (const std::exception &) {
                // Other VM-compile failures stay non-fatal — TW dispatches via
                // userFuncs_.
            }
        }
        userFuncs_[func.name] = std::move(func);
    };
    if (ast->type == NodeType::FUNCTION_DEF) {
        adopt(ast.get());
    } else {
        for (const auto &c : ast->children)
            if (c && c->type == NodeType::FUNCTION_DEF)
                adopt(c.get());
    }
}

void Engine::rehashMFiles()
{
    // Drop cache entries AND the user-function/compiled mirrors created
    // by resolveMFile_. Functions registered by execFunctionDef from
    // top-level scripts are kept — only m-file-loaded ones should go.
    // The compiler stores chunks under the same key resolveMFile_ used
    // (qualified for +pkg/foo.m, bare for plain foo.m), so erase that exact
    // key — NOT clearCompiledFuncs(), which would also nuke script-defined
    // compiled functions (contradicting the "kept" promise above).
    for (const auto &[name, _] : mFileCache_) {
        userFuncs_.erase(name);
        if (compiler_)
            compiler_->eraseCompiledFunc(name);
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

        // Read + parse + extract FUNCTION_DEF. The file EXISTS (checked above),
        // so a read/lex/parse failure is a real error in the matched m-file —
        // surface it (with the path) instead of silently skipping to the next
        // search dir, which would mask a syntax error as "undefined function".
        // MATLAB likewise reports the first path-matched file's error.
        auto loadError = [&](const std::exception &e) -> Error {
            return Error("error loading '" + userPath + "': " + e.what(),
                         0, 0, "", "", "numkit:mfile:loadError");
        };
        std::string content;
        try {
            content = rp.fs->readFile(rp.path);
        } catch (const std::exception &e) {
            throw loadError(e);
        }

        Lexer lexer(content);
        std::vector<Token> tokens;
        try {
            tokens = lexer.tokenize();
        } catch (const std::exception &e) {
            throw loadError(e);
        }
        Parser parser(tokens);
        ASTNodePtr ast;
        try {
            ast = parser.parse();
        } catch (const std::exception &e) {
            throw loadError(e);
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

        // classdef file `Name.m`: register the class and a constructor
        // external under `name` so this very call resolves (subsequent calls
        // hit findClass directly). Mirrors the FUNCTION_DEF path's caching.
        if (!funcDef) {
            const ASTNode *classDef = nullptr;
            if (ast->type == NodeType::BLOCK) {
                for (const auto &c : ast->children)
                    if (c && c->type == NodeType::CLASSDEF_DEF && c->strValue == leafName) {
                        classDef = c.get();
                        break;
                    }
            } else if (ast->type == NodeType::CLASSDEF_DEF && ast->strValue == leafName) {
                classDef = ast.get();
            }
            if (classDef) {
                registerClassDef(classDef);
                const std::string cn = leafName;
                registerFunction(name, [cn](Span<const Value> args, size_t, Span<Value> outs,
                                            CallContext &ctx) {
                    const BuiltinClass *c = ctx.engine->findClass(cn);
                    if (!c || !c->construct)
                        throw std::runtime_error("classdef '" + cn
                                                 + "' has no constructor");
                    outs[0] = ctx.engine->constructChecked(c, args, ctx);
                });
                MFileCacheEntry ce;
                ce.fullPath = userPath;
                if (auto st = rp.fs->stat(rp.path))
                    ce.mtime = st->mtime;
                ce.sourceCode = std::make_shared<const std::string>(std::move(content));
                mFileCache_[name] = std::move(ce);
                return nullptr; // class + ctor-external resolve the call
            }
            continue;
        }

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
            } catch (const RegisterExhaustionError &) {
                // Too large for the bytecode VM. Surface it instead of silently
                // dropping VM compilation — otherwise a VM-mode call would fall
                // through to the external path and throw a misleading "undefined
                // function" for a function that is plainly defined.
                throw Error("function '" + name + "' needs more than the 255-register "
                    "VM limit in one function — split it into smaller functions so it "
                    "can run on the bytecode VM (and be debuggable)",
                    0, 0, name, "", "numkit:compiler:registerExhaustion");
            } catch (const std::exception &) {
                // Other compiler errors are non-fatal here — TW will still
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
    return results.empty() ? Value() : results[0];
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

    // 2) VM user-function path (active backend == VM). Run the handle's body
    // as a re-entrant VM frame so it executes on the same engine as the rest
    // of the program (debuggable; consistent with classdef methods/ctors which
    // are VM-native — VM_CALLBACKS_PLAN.md P3). The handle's UserFunction is in
    // userFuncs_ (named funcs, and VM/TW anon-funcs mirror-registered by
    // Compiler::compileAnonFunc) with params == [user_params…, captures…];
    // captures already arrive as appended args from the closure-cell unwrap
    // above, so the arg layout matches the compiled chunk. Falls through to the
    // TreeWalker when the name is not a free user function or cannot compile.
    if (vm_ && backend_ == Backend::VM) {
        if (const UserFunction *uf = lookupUserFunctionLocal(name))
            if (const BytecodeChunk *cc = ensureClassMethodChunk(*uf))
                return vm_->callReentrant(*cc, args, nout);
    }

    // 3) TW user-function path. Works for any named user function and
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
        Value result = Value();
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
        // Optional display stats (min/max/mean/median/mode/var/std) for the
        // unified Variable / struct viewer's column chooser. Full-precision
        // so the JS side renders exact values; omitted for non-numeric.
        ValueStats st;
        if (computeValueStats(*val, st)) {
            std::ostringstream so;
            so.precision(17);
            so << ",\"stats\":{\"min\":" << st.min << ",\"max\":" << st.max
               << ",\"mean\":" << st.mean << ",\"median\":" << st.median
               << ",\"mode\":" << st.mode << ",\"var\":" << st.var
               << ",\"std\":" << st.std << "}";
            os << so.str();
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