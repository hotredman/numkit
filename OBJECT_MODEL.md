# Object model design (numkit core)

Status: **P1–P7 implemented** (type + registry + value/handle clone,
properties, constructors, methods, subsref/subsasgn indexing,
`dictionary` + `containers.Map`, object display, multi-output methods,
binary + unary operator overloading, **object arrays** — array-backed
storage + builtin `()` index read/write with grow + concatenation
`[a b]`/`[a;b]` + `[arr.prop]` CSL + array display) — both engines, on
`core-dev`. Owner: CORE. Remaining: 2-D/N-D object-array grids,
`image.*` objects, user `classdef` authoring.

## Object arrays

`HeapObject` stores a `std::vector<std::shared_ptr<ObjectState>>` whose
length is `numel()`; the shape is in `dims` (a scalar object is a 1×1
array of one state). `clone()` applies the value/handle pivot per
element. Two `Value` primitives drive the array mechanics:
- `objectSubArray(idxs)` — element read; one index → a 1×1 scalar
  object, several → a 1×N row. Value classes get independent deep copies
  per element (safe to mutate), handle classes alias.
- `objectAssignElement(idx, elem, fill)` — element store with 1-D grow,
  gap-filling new slots with independent copies of a default-constructed
  `fill`. An empty/unset receiver becomes a fresh object array.

Engine integration fires the builtin path **only when the class has no
custom `subsref`/`subsasgn`** (else the class controls indexing, MATLAB-
faithful): TreeWalker `execCall`/`execIndexedAssign`, VM `INDEX_GET`/
`INDEX_SET`/`execCallIndirect`. `arr(i)`, `arr([1 3])`, `arr(end)`,
`arr(i) = obj` (grow), `numel`/`size` all work on both engines.

Concatenation `[a b]` / `[a; b]` of same-class objects is handled in
`Value::horzcat`/`vertcat` (the single concat chokepoint both engines
route through) via `Value::concatObjects`, which flattens each operand's
states into a 1×N row / N×1 column applying the per-element value/handle
rule. Mixing classes or objects-with-non-objects throws.

**Comma-separated lists**: `[arr.prop]` expands a property over every
element into a row. Both engines run it through the existing struct-CSL
machinery (TreeWalker `execMatrixLiteral`, VM `HORZCAT_APPEND_CSL`),
extended with an object branch that calls the class `propGet` per element
(`Value::objectSubArray({i})` gives the scalar element). On the VM this
also fixed bracketed scalar `[obj.prop]`, which previously demanded a
struct and threw.

**Display**: `Engine::formatObjectDisplay` (shared by both engines) shows
a scalar via the class `dispText`, and an array as
`<rows>×<cols> <Class> array with properties:` + the `propNames` list.

v1 limits: indexed **assignment** takes a single linear subscript (read
supports vector/`end`/logical); object arrays are 1-D (a row or column) —
2-D/N-D grids are not yet wired.

## Goal

A first-class **object model in the engine** so that C++-implemented
toolbox functions can construct and return real MATLAB objects —
`containers.Map`, `table`, `datetime`, `digraph`, image-toolbox handles,
… — that behave like MATLAB objects: `class(obj)`, `isa`, property
access `obj.Prop` / `obj.Prop = v`, method calls `obj.m(...)` and
`m(obj, ...)`, custom indexing `obj(i)`, operator overloads, `disp`.

The mechanism is built **fully**, not as a one-off: the same registry +
dispatch must later host **user `classdef`** classes (methods sourced
from `.m` AST instead of C++). Builtin classes are the first population
path; classdef is a later one onto the identical machinery.

Today the codebase has **no** object type (`ValueType` ends at `STRING`)
and `class(x)` just maps the storage type to a name. Toolbox functions
that "should" return objects currently fake them as plain `struct`
values (`strel`, `bwconncomp`, …) — those become real objects under this
model.

First validation vertical: **`containers.Map`** — it exercises every
hard mechanism at once (handle/reference semantics, opaque native
payload, method dispatch, custom paren-indexing). Then **`image.*`**.

---

## 1. Object representation

Add `ValueType::OBJECT`. An object instance is a heap value carrying a
class identity plus shared instance state:

```cpp
// New: instance state behind a shared_ptr (the handle/value pivot).
struct NativePayload {                 // opaque C++ state (Map hash, table cols, …)
    virtual ~NativePayload() = default;
    virtual std::shared_ptr<NativePayload> clone() const = 0;  // value-class deep copy
};

struct ObjectState {
    std::pmr::map<std::string, Value> props;        // MATLAB-visible properties
    std::shared_ptr<NativePayload>    native;       // optional opaque payload
};

// HeapObject (ValueType::OBJECT) gains:
//   std::string                  *objClass;   // class name (registry key)
//   std::shared_ptr<ObjectState>  objState;   // instance state
//   bool                          objIsHandle; // cached from the class
```

Scalar object = one `ObjectState`. Object **arrays** reuse the existing
struct-array AoS storage pattern (a vector of states) — deferred until a
class needs non-scalar arrays; `containers.Map` is always scalar.

### Value vs handle — the single contained special case

COW is the existing whole-engine mechanism: mutating accessors call
`detach()` which clones the `HeapObject`. The object model rides on it
with **one** override, in `HeapObject::clone()`:

- **value class**: clone deep-copies `ObjectState` (`props` copied,
  `native = native ? native->clone() : nullptr`). Each owner gets its
  own state → value semantics, like `struct`.
- **handle class** (`objIsHandle`): clone keeps the **same**
  `objState` shared_ptr. All copies of the value share one state →
  reference semantics. `b = a; a.x = 5;` is visible through `b`.

Everything else in the COW machinery is untouched. No new "reference
mode" plumbing across the engine — just the clone rule.

**Load-bearing invariant:** *every* object mutation (property set,
method that writes state, `subsasgn`) must go through `detach()` before
touching `objState`. `detach()` clones the `HeapObject` only when it is
shared; `clone()` then applies the value/handle split. With the
invariant: a value object's writer always operates on a uniquely-owned
state; a handle object's writer operates on the shared state (clone kept
the same `objState` pointer). Skipping `detach()` on any mutation path
breaks value semantics — so object writers funnel through one helper,
not ad-hoc field pokes.

---

## 2. Class registry (engine-level)

```cpp
using ObjectMethod = std::function<void(
    Value &self, Span<const Value> args, size_t nargout,
    Span<Value> out, CallContext &ctx)>;

struct BuiltinClass {
    std::string  name;                 // registry key, e.g. "containers.Map"
    bool         isHandle = false;
    std::vector<std::string> superclasses;     // for isa()

    // Constructor: ClassName(args) -> object Value. No self yet.
    std::function<Value(Span<const Value> args, CallContext &)> construct;

    std::unordered_map<std::string, ObjectMethod> methods;   // obj.m / m(obj)
    std::vector<std::string> propNames;                      // properties(), disp

    // Property access via per-class hooks. v1 ships ONLY the hooks +
    // their dispatch — a class supplies propGet/propSet (Map's Count etc.
    // are computed there). The convenience "default backing reads/writes
    // ObjectState.props" is NOT built until a class actually needs it
    // (classdef) — no unexercised code in v1.
    std::function<bool(const Value &self, const std::string &, Value &out, CallContext &)> propGet;
    std::function<bool(Value &self, const std::string &, const Value &val, CallContext &)>  propSet;

    // Optional overloads (nullptr = use default / error).
    ObjectMethod subsref;     // obj(i) / obj{i} read
    ObjectMethod subsasgn;    // obj(i) = v write
    std::function<std::string(const Value &self)> dispText;   // disp/display
    // Operator overloads keyed by op name ("plus","eq",…) — phase later.
    std::unordered_map<std::string, ObjectMethod> ops;
};

// Engine API:
void Engine::registerClass(BuiltinClass cls);
const BuiltinClass *Engine::findClass(std::string_view name) const;
```

Instances store only `objClass` + `objState`; the `BuiltinClass`
(methods, prop list, attributes) lives once in the registry.

---

## 3. Dispatch integration points

| Surface | TreeWalker | VM | Behaviour for OBJECT |
|---|---|---|---|
| `obj.p` read | `execFieldAccess` | `FIELD_GET` | `propGet`; if `p` is a no-arg method, call it |
| `obj.p = v` | `resolveObjectSlot` / `execFieldAssign` | `FIELD_SET` | `propSet` |
| `obj.m(a)` | `execCall` (funcNode = FIELD_ACCESS) | `CALL`/`CALL_MULTI` | method `m` with self prepended |
| `m(obj,a)` | `execCall` function-form | `CALL`/`CALL_MULTI` | if 1st arg is OBJECT whose class has `m` → method |
| `ClassName(a)` | `execCall` | `CALL` | if name is a registered class → `construct` |
| `obj(i)` / `obj(i)=v` | `execIndexAccess` / `execIndexedAssign` | `INDEX_GET`/`INDEX_SET` | `subsref` / `subsasgn` overload |
| `a OP b` | `execBinaryOp` | op opcodes | if operand is OBJECT with `ops[OP]` → dispatch (later phase) |
| `class/isa/isobject/disp/properties/methods` | builtins (libs/builtin) | — | consult registry |

Method dispatch precedence (MATLAB): a class method on the first object
argument beats a same-named global/builtin function. Constructor for a
**package-qualified** name (`containers.Map`) resolves through the
existing qualified-name path (`tryBuildQualifiedName`) → registry.

**Multi-output methods** (`[a,b] = obj.m()` / `[a,b] = m(obj)`): the
`ObjectMethod` signature already carries `nargout` + an `outs` span, so
methods returning several values work on both engines. Dispatch sites
size the output buffer to `nargout`:
- TreeWalker — `execCallMulti` gained an object branch for both the
  dotted (`obj.m(...)`) and function (`m(obj,...)`) forms.
- VM — function-form rides `CALL_MULTI` (object check on the first arg);
  the dotted form uses a dedicated `CALL_METHOD_MULTI` opcode
  (`a=outBase, b=objReg, c=argBase, d=nameIdx, e=(nargs<<4)|nout`; each
  nibble ≤15). The compiler's `compileMultiAssign` mirrors `compileCall`'s
  gating — a non-variable root (`pkg.fn(x)`) routes the **qualified** name
  through `CALL_MULTI` (this also fixed a latent bug where the leaf name
  was used); a variable root (`obj.m(x)`) emits `CALL_METHOD_MULTI`.
- Not yet: multi-output `subsref` (`[a,b] = obj(i)`) — needs object
  arrays first; struct-field-func-handle multi (`[a,b] = s.fh(x)`) still
  routes through the object path and errors if the receiver isn't an
  object (was already unsupported).

**Binary operator overloading** (`a + b`, `a == b`, `a .* b`, …): the
`BuiltinClass::ops` map keys MATLAB operator-method names
(`plus`/`minus`/`mtimes`/`times`/`rdivide`/`eq`/`lt`/`and`/…). Both
engines funnel non-scalar binary ops through one slow path
(`TreeWalker::execBinaryOp` / `VM::binarySlowPath`); each now calls
`Engine::tryObjectBinaryOp` **before** the numeric/cached builtin path
when either operand is an object. The dominant (first) object operand's
class decides dispatch (v1 fidelity); the hook receives `self` = that
object and `args = {lhs, rhs}` in source order. A missing overload
raises the MATLAB `Undefined operator '<op>' for input arguments of type
'<class>'` error rather than silently falling through.

**Unary operator overloading** (`-a`, `~a`, `a'`, `a.'`) mirrors the
binary path: `Engine::tryObjectUnaryOp` (called from
`TreeWalker::execUnaryOp` / `VM::unarySlowPath`) maps the token to
`uminus`/`not`/`ctranspose`/`transpose` and dispatches to the operand's
class `ops` with self = the operand and no args. (`+a` stays the default
identity copy — `uplus` is not intercepted.)

**Decided cut (v1 dispatch fidelity):** dispatch keys on the **first
object argument's class only**. Full MATLAB argument-dominance
(`Inferiorto`/`Superiorto`, dominant-among-several-objects, double-vs-
object precedence tables) is out of scope for v1 — covers the common
case; revisit if a real class needs multi-object dispatch.

---

## 4. `containers.Map` vertical (first class)

- **Handle** class, opaque payload:
  ```cpp
  struct MapPayload : NativePayload {
      std::map<std::string,Value> sdata;   // 'char' KeyType
      std::map<double,Value>      ddata;    // numeric KeyType
      std::string keyType = "char", valueType = "any";
      std::shared_ptr<NativePayload> clone() const override; // unused (handle)
  };
  ```
- **Construct**: `containers.Map`, `containers.Map(keys,values)`,
  `containers.Map('KeyType',kt,'ValueType',vt)`,
  `containers.Map(k,v,'UniformValues',false)`.
- **Properties**: `Count`, `KeyType`, `ValueType` (computed → `propGet`).
- **Methods**: `keys`, `values`, `isKey`, `remove`, `length`. Dotted
  (`m.keys`) and function-form (`keys(m)`).
- **Indexing** (the reason Map drives the design): `m(key)` →
  `subsref`; `m(key) = v` → `subsasgn`. Custom paren-index on an object.
- **Handle check**: `m2 = m; m2('x') = 1; m('x')` → `1`.
- Artefacts (per repo rule): C++ impl, parity spec, gtest, smoke.

---

## 5. How user `classdef` layers on (no rework)

A `classdef` file is parsed to a `CLASSDEF` AST node and registered as a
`BuiltinClass` where:
- `construct` / `methods` are thin wrappers that invoke the interpreter
  on the method's `UserFunction` body (self + args).
- properties use the default `ObjectState.props` get/set with declared
  defaults; attributes (Access/Dependent/Constant/Static) gate the
  default accessors.
- `superclasses` from `< Base`; method/prop resolution walks the chain.
- handle-ness from `< handle`.

So phases here build the OBJECT type, registry, dispatch, value/handle,
indexing and overloads **once**; classdef is a separate later epic that
only adds a parser + a registration adapter.

---

## 6. Phase / commit plan (this epic)

Each phase: dual-engine (TreeWalker first, VM in the same phase), full
gtest suite green, commit. WASM check before any push that adds opcodes
or libc++-touching code.

1. **P1 — type + registry skeleton.** `ValueType::OBJECT`, HeapObject
   fields, `ObjectState`/`NativePayload`, `Engine::registerClass/findClass`,
   COW clone rule (value vs handle). `class/isa/isobject` recognise
   objects. A throwaway registered test class proves construct + identity.
2. **P2 — property get/set** dispatch on both engines (`propGet`/`propSet`,
   default → `ObjectState.props`).
3. **P3 — method dispatch**: dotted `obj.m()` + function-form `m(obj)` +
   constructor `ClassName(...)` incl. package-qualified.
4. **P4 — handle semantics** end-to-end (shared state, aliasing tests).
5. **P5 — custom indexing**: `subsref`/`subsasgn` for `obj(i)`/`obj(i)=v`.
6. **P6 — `containers.Map`** implemented on the above + 4 artefacts.
7. **P7 — `disp`/`display`** formatting for objects.

Later (separate epics): operator-overload dispatch; `image.*` objects;
object arrays; `classdef` authoring; events/listeners; enumerations.

---

## 7. Open questions / risks

- **Package-qualified constructor** (`containers.Map`): confirm the
  qualified-name resolver can be pointed at the class registry before
  falling through to m-file / namespace lookup.
- **`obj.method` vs `obj.prop` ambiguity**: a bare `obj.foo` must decide
  property-read vs no-arg-method-call. Resolution: property names win;
  if not a property but a method, call with nargout from context.
- **VM opcode impact (hypothesis, confirm in P2/P3)**: property and
  index get/set ride existing `FIELD_GET/SET`, `INDEX_GET/SET` with a
  runtime type check — solid, no new opcodes. **Method calls are the
  uncertain case**: `obj.m(a)` today likely compiles as "`obj.m` → value,
  then `CALL_INDIRECT`", but a method is not a func-handle. P3 either
  routes this through `CALL_INDIRECT` made object-aware, or adds one
  `CALL_METHOD` opcode / compiler tweak. Treat "zero new opcodes" as a
  goal, not a guarantee.
- **`end` inside `m(key)`**: `end` semantics for custom-indexed objects
  follow MATLAB (`end` → the class's `end`/`numel` overload); default
  for Map is undefined — document the cut.
- **Display registration collision**: a class named like an existing
  builtin must not shadow it (check `findExternal` precedence).
