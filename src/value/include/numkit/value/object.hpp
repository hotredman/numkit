// include/numkit/value/object.hpp
//
// Engine object model — see OBJECT_MODEL.md. A class instance is a
// ValueType::OBJECT heap value carrying a class name + shared
// ObjectState. The class itself (methods, property hooks, attributes)
// is registered once with the Engine as a BuiltinClass.
#pragma once

#include <numkit/value/span.hpp>
#include <numkit/value/value.hpp>

#include <functional>
#include <map>
#include <memory>
#include <memory_resource>
#include <string>
#include <unordered_map>
#include <vector>

namespace numkit {

struct CallContext;   // defined in types.hpp; used only by-reference here
struct UserFunction;  // defined in types.hpp; held by shared_ptr in BuiltinClass

// ============================================================
// NativePayload — opaque C++ instance state for builtin classes
// (a hash for containers.Map, columns for table, …). Subclassed per
// class. `clone()` is used only for VALUE classes (deep copy on COW);
// handle classes share the payload and never clone it.
// ============================================================
struct NativePayload
{
    virtual ~NativePayload() = default;
    virtual std::shared_ptr<NativePayload> clone() const = 0;
};

// ============================================================
// ObjectState — instance state, shared via shared_ptr. The handle/value
// pivot lives in HeapObject::clone(): a handle object shares this state
// across copies; a value object deep-copies it. See OBJECT_MODEL.md §1.
// ============================================================
struct ObjectState
{
    std::pmr::map<std::string, Value> props;     // MATLAB-visible properties
    std::shared_ptr<NativePayload>    native;     // optional opaque payload
    std::string enumName;                         // enumeration member name ("" if not an enum)

    explicit ObjectState(std::pmr::memory_resource *mr) : props(mr) {}

    // Deep copy for value-class COW. props copied by value; native via
    // its own clone().
    std::shared_ptr<ObjectState> deepCopy(std::pmr::memory_resource *mr) const
    {
        auto s = std::make_shared<ObjectState>(mr);
        for (const auto &[k, v] : props)
            s->props.emplace(k, v);
        s->native = native ? native->clone() : nullptr;
        s->enumName = enumName;
        return s;
    }
};

// A class method / constructor body. `self` is the receiver (the object
// the method dispatches on); constructors take no self and return the
// new object via the BuiltinClass::construct functor instead.
using ObjectMethod = std::function<void(Value &self, Span<const Value> args,
                                        size_t nargout, Span<Value> outs,
                                        CallContext &ctx)>;

// Per-member reflection metadata, mirrored from the classdef into the runtime
// class so introspection (metaclass/meta.property/meta.method) can report it
// without reaching back into the compiler-side ClassDefDesc.
struct PropMeta
{
    std::string name;
    std::string getAccess = "public";   // public | private | protected
    std::string setAccess = "public";   // (+ immutable on the set side)
    bool        isConstant = false;
    bool        isDependent = false;
};
struct MethodMeta
{
    std::string name;
    bool        isStatic = false;
    std::string access = "public";
    bool        isAbstract = false;
};

// ============================================================
// BuiltinClass — one registry entry. Instances reference it by name.
// ============================================================
struct BuiltinClass
{
    std::string name;                       // registry key, e.g. "containers.Map"
    bool        isHandle = false;
    bool        isSealed = false;           // classdef (Sealed) — reflection
    bool        isAbstract = false;         // has an unimplemented Abstract member
    std::vector<std::string> superclasses;  // for isa()

    // Constructor: ClassName(args) -> object Value.
    std::function<Value(Span<const Value> args, CallContext &ctx)> construct;

    // Methods: obj.m(args) / m(obj, args).
    std::unordered_map<std::string, ObjectMethod> methods;

    // Property names (for properties(), disp). Access goes through the
    // hooks below — v1 ships hooks only (no default ObjectState.props
    // backing until a class needs it; see OBJECT_MODEL.md §2).
    std::vector<std::string> propNames;
    std::function<bool(const Value &self, const std::string &name,
                       Value &out, CallContext &ctx)> propGet;
    std::function<bool(Value &self, const std::string &name,
                       const Value &val, CallContext &ctx)> propSet;

    // Optional overloads (empty = default / error).
    // subsref:  obj(i…) read  — args = the subscripts; result in out[0].
    // subsasgn: obj(i…) = v   — args = [subscripts…, value] (value last);
    //           mutates `self` in place via objectStateMut() (so the
    //           value/handle COW rule applies); out is unused.
    ObjectMethod subsref;
    ObjectMethod subsasgn;
    std::function<std::string(const Value &self)> dispText;          // disp/display
    std::unordered_map<std::string, ObjectMethod> ops;               // "plus","eq",…
    // Members declared `Hidden` — still usable, but omitted from
    // properties() / methods() introspection listings.
    std::vector<std::string> hidden;
    // Reflection metadata for metaclass()/meta.property/meta.method. Populated
    // for classdef-defined classes; left empty for native builtin classes
    // (containers.Map, …), whose introspection falls back to propNames/methods.
    std::vector<PropMeta>   propMeta;
    std::vector<MethodMeta> methodMeta;
    // classdef instance methods as UserFunctions (name → uf), so the VM can
    // compile + run their bodies as native frames instead of the C++ hook.
    // Absent for native builtin-class methods (containers.Map, …), which keep
    // the hook. `uf->name` is the declaring-class-qualified "Class>method".
    std::unordered_map<std::string, std::shared_ptr<const UserFunction>> methodFns;
};

} // namespace numkit
