// toolboxes/builtin/src/language/datatypes/containers.cpp
//
// Key–value container classes on the engine object model
// (object_model.md): the modern value-semantics `dictionary` (R2022b+)
// and the legacy handle-semantics `containers.Map`. Both share one
// opaque KVPayload; the value/handle difference is entirely the registry
// isHandle flag + the engine's COW clone rule.
//
// The public, ENGINE-FREE C++ API (numkit::containers::map/dictionary/
// set/get/...) is the single source of truth; the interpreter's registry
// hooks (construct/subsref/subsasgn/methods) are thin adapters over it.
#include <numkit/runtime/containers.hpp>
#include <numkit/core/engine.hpp>
#include <numkit/value/object.hpp>

#include <algorithm>
#include <memory>
#include <numeric>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace numkit::runtime {
namespace {

// ── Shared opaque payload ────────────────────────────────────
struct KVPayload : NativePayload
{
    enum KeyKind { None, Str, Num } keyKind = None;
    std::vector<std::string> skeys; // when keyKind == Str
    std::vector<double>      nkeys;  // when keyKind == Num
    std::vector<Value>       vals;   // parallel to the active key vector
    bool        configured = false;
    std::string keyType = "char";   // MATLAB KeyType label (containers.Map)
    std::string valType = "any";    // MATLAB ValueType label

    std::shared_ptr<NativePayload> clone() const override
    {
        return std::make_shared<KVPayload>(*this); // deep copy → value semantics
    }

    size_t count() const { return vals.size(); }

    int find(const Value &key) const
    {
        if (keyKind == Str) {
            std::string s = key.toString();
            for (size_t i = 0; i < skeys.size(); ++i)
                if (skeys[i] == s)
                    return static_cast<int>(i);
        } else if (keyKind == Num) {
            double d = key.toScalar();
            for (size_t i = 0; i < nkeys.size(); ++i)
                if (nkeys[i] == d)
                    return static_cast<int>(i);
        }
        return -1;
    }

    void put(const Value &key, const Value &val)
    {
        if (keyKind == None) {
            keyKind = key.isNumeric() ? Num : Str;
            keyType = (keyKind == Num) ? "double" : "char";
        }
        int idx = find(key);
        if (idx >= 0) {
            vals[idx] = val;
            return;
        }
        if (keyKind == Str)
            skeys.push_back(key.toString());
        else
            nkeys.push_back(key.toScalar());
        vals.push_back(val);
        configured = true;
        valType = mtypeName(val.type());
    }
};

const KVPayload *cst(const Value &m)
{
    const auto *st = m.objectStateConst();
    return st ? static_cast<const KVPayload *>(st->native.get()) : nullptr;
}
KVPayload *mut(Value &m)
{
    auto *st = m.objectStateMut(); // detach (COW) → value/handle clone rule
    return st ? static_cast<KVPayload *>(st->native.get()) : nullptr;
}
const KVPayload *require(const Value &m)
{
    const KVPayload *p = cst(m);
    if (!p)
        throw std::runtime_error("expected a containers.Map or dictionary");
    return p;
}

void extractKeys(const Value &k, std::vector<Value> &out, std::pmr::memory_resource *mr)
{
    if (k.isCell())
        for (size_t i = 0; i < k.numel(); ++i)
            out.push_back(k.cellAt(i));
    else if (k.isString())
        for (size_t i = 0; i < k.numel(); ++i)
            out.push_back(Value::stringScalar(k.stringElem(i), mr));
    else if (k.isChar())
        out.push_back(k);
    else
        for (size_t i = 0; i < k.numel(); ++i)
            out.push_back(k.elemAt(i, mr));
}
void extractVals(const Value &v, std::vector<Value> &out, std::pmr::memory_resource *mr)
{
    if (v.isCell())
        for (size_t i = 0; i < v.numel(); ++i)
            out.push_back(v.cellAt(i));
    else
        for (size_t i = 0; i < v.numel(); ++i)
            out.push_back(v.elemAt(i, mr));
}

// Output order for keys()/values(). MATLAB containers.Map iterates its
// keys SORTED; dictionary preserves insertion order. We store insertion
// order in the payload and apply the Map sort only at output.
std::vector<size_t> outputOrder(const Value &m, const KVPayload *p)
{
    std::vector<size_t> idx(p->count());
    std::iota(idx.begin(), idx.end(), size_t{0});
    if (m.objectClassName() == "containers.Map") {
        if (p->keyKind == KVPayload::Num)
            std::sort(idx.begin(), idx.end(),
                      [&](size_t a, size_t b) { return p->nkeys[a] < p->nkeys[b]; });
        else
            std::sort(idx.begin(), idx.end(),
                      [&](size_t a, size_t b) { return p->skeys[a] < p->skeys[b]; });
    }
    return idx;
}

Value makeContainer(const std::string &cls, bool isHandle, std::pmr::memory_resource *mr)
{
    if (!mr)
        mr = std::pmr::get_default_resource();
    auto st = std::make_shared<ObjectState>(mr);
    st->native = std::make_shared<KVPayload>();
    return Value::object(cls, st, isHandle, mr);
}

} // namespace

// ============================================================
// Public, engine-free C++ API.
// ============================================================
namespace containers {

Value map(std::pmr::memory_resource *mr)
{
    return makeContainer("containers.Map", /*isHandle=*/true, mr);
}
Value dictionary(std::pmr::memory_resource *mr)
{
    return makeContainer("dictionary", /*isHandle=*/false, mr);
}

void set(Value &m, const Value &key, const Value &val)
{
    KVPayload *p = mut(m);
    if (!p)
        throw std::runtime_error("set: expected a containers.Map or dictionary");
    p->put(key, val);
}

Value get(const Value &m, const Value &key)
{
    const KVPayload *p = require(m);
    int idx = p->find(key);
    if (idx < 0)
        throw std::runtime_error("the specified key is not present in this container");
    return p->vals[idx];
}

bool isKey(const Value &m, const Value &key) { return require(m)->find(key) >= 0; }

std::size_t count(const Value &m) { return require(m)->count(); }

void remove(Value &m, const Value &key)
{
    KVPayload *p = mut(m);
    if (!p)
        throw std::runtime_error("remove: expected a containers.Map or dictionary");
    int idx = p->find(key);
    if (idx < 0)
        return;
    if (p->keyKind == KVPayload::Num)
        p->nkeys.erase(p->nkeys.begin() + idx);
    else
        p->skeys.erase(p->skeys.begin() + idx);
    p->vals.erase(p->vals.begin() + idx);
}

Value keys(const Value &m, std::pmr::memory_resource *mr)
{
    if (!mr)
        mr = std::pmr::get_default_resource();
    const KVPayload *p = require(m);
    const std::vector<size_t> ord = outputOrder(m, p);
    // containers.Map → cell row (sorted); dictionary → key-typed column.
    if (m.objectClassName() == "containers.Map") {
        Value c = Value::cell(1, p->count(), mr);
        for (size_t i = 0; i < ord.size(); ++i)
            c.cellAt(i) = (p->keyKind == KVPayload::Num)
                              ? Value::scalar(p->nkeys[ord[i]], mr)
                              : Value::fromString(p->skeys[ord[i]], mr);
        return c;
    }
    if (p->keyKind == KVPayload::Num) {
        Value v = Value::matrix(p->count(), 1, ValueType::DOUBLE, mr);
        for (size_t i = 0; i < ord.size(); ++i)
            v.doubleDataMut()[i] = p->nkeys[ord[i]];
        return v;
    }
    Value v = Value::stringArray(p->count(), 1, mr);
    for (size_t i = 0; i < ord.size(); ++i)
        v.stringElemSet(i, p->skeys[ord[i]]);
    return v;
}

Value values(const Value &m, std::pmr::memory_resource *mr)
{
    if (!mr)
        mr = std::pmr::get_default_resource();
    const KVPayload *p = require(m);
    const std::vector<size_t> ord = outputOrder(m, p);
    if (m.objectClassName() == "containers.Map") {
        Value c = Value::cell(1, p->count(), mr);
        for (size_t i = 0; i < ord.size(); ++i)
            c.cellAt(i) = p->vals[ord[i]];
        return c;
    }
    // dictionary: numeric scalars → column vector, otherwise a cell column.
    bool allNum = !p->vals.empty();
    for (const auto &v : p->vals)
        if (!(v.isScalar() && v.type() == ValueType::DOUBLE))
            allNum = false;
    if (allNum) {
        Value out = Value::matrix(p->count(), 1, ValueType::DOUBLE, mr);
        for (size_t i = 0; i < ord.size(); ++i)
            out.doubleDataMut()[i] = p->vals[ord[i]].toScalar();
        return out;
    }
    Value out = Value::cell(p->count(), 1, mr);
    for (size_t i = 0; i < ord.size(); ++i)
        out.cellAt(i) = p->vals[ord[i]];
    return out;
}

} // namespace containers

// ============================================================
// Registry hooks — thin adapters over the public C++ API.
// ============================================================

void registerContainersRuntime(Engine &engine)
{
    // ── dictionary (value semantics) ─────────────────────────
    {
        BuiltinClass dict;
        dict.name = "dictionary";
        dict.isHandle = false;

        dict.construct = [](Span<const Value> args, CallContext &ctx) -> Value {
            auto *mr = ctx.engine->resource();
            Value d = containers::dictionary(mr);
            if (args.size() >= 2) {
                std::vector<Value> ks, vs;
                extractKeys(args[0], ks, mr);
                extractVals(args[1], vs, mr);
                if (ks.size() != vs.size())
                    throw std::runtime_error("dictionary: keys and values must match in number");
                for (size_t i = 0; i < ks.size(); ++i)
                    containers::set(d, ks[i], vs[i]);
            }
            return d;
        };
        dict.subsref = [](Value &self, Span<const Value> args, size_t, Span<Value> out,
                          CallContext &) { out[0] = containers::get(self, args[0]); };
        dict.subsasgn = [](Value &self, Span<const Value> args, size_t, Span<Value>,
                           CallContext &) {
            containers::set(self, args[0], args[args.size() - 1]);
        };
        dict.methods["numEntries"] = [](Value &self, Span<const Value>, size_t,
                                        Span<Value> out, CallContext &ctx) {
            out[0] = Value::scalar(static_cast<double>(containers::count(self)),
                                   ctx.engine->resource());
        };
        dict.methods["isKey"] = [](Value &self, Span<const Value> a, size_t, Span<Value> out,
                                   CallContext &ctx) {
            out[0] = Value::logicalScalar(containers::isKey(self, a[0]), ctx.engine->resource());
        };
        dict.methods["keys"] = [](Value &self, Span<const Value>, size_t, Span<Value> out,
                                  CallContext &ctx) {
            out[0] = containers::keys(self, ctx.engine->resource());
        };
        dict.methods["values"] = [](Value &self, Span<const Value>, size_t, Span<Value> out,
                                    CallContext &ctx) {
            out[0] = containers::values(self, ctx.engine->resource());
        };
        dict.methods["isConfigured"] = [](Value &self, Span<const Value>, size_t,
                                          Span<Value> out, CallContext &ctx) {
            out[0] = Value::logicalScalar(cst(self)->configured, ctx.engine->resource());
        };
        dict.dispText = [](const Value &self) -> std::string {
            const KVPayload *p = cst(self);
            std::ostringstream os;
            const char *kt = (p->keyKind == KVPayload::Num) ? "double" : "string";
            os << "  dictionary (" << kt << " --> " << p->valType << ") with " << p->count()
               << " entries:\n";
            for (size_t i = 0; i < p->count(); ++i) {
                os << "    ";
                if (p->keyKind == KVPayload::Num)
                    os << p->nkeys[i];
                else
                    os << '"' << p->skeys[i] << '"';
                os << " --> ";
                const Value &v = p->vals[i];
                if (v.isScalar() && v.type() == ValueType::DOUBLE)
                    os << v.toScalar();
                else
                    os << mtypeName(v.type());
                os << "\n";
            }
            return os.str();
        };
        engine.registerClass(std::move(dict));
    }

    // ── containers.Map (handle semantics) ────────────────────
    {
        BuiltinClass cm;
        cm.name = "containers.Map";
        cm.isHandle = true;
        cm.propNames = {"Count", "KeyType", "ValueType"};

        cm.construct = [](Span<const Value> args, CallContext &ctx) -> Value {
            auto *mr = ctx.engine->resource();
            Value m = containers::map(mr);
            if (args.size() >= 2 && !args[0].isChar() && !args[0].isString()) {
                std::vector<Value> ks, vs;
                extractKeys(args[0], ks, mr);
                extractVals(args[1], vs, mr);
                for (size_t i = 0; i < ks.size() && i < vs.size(); ++i)
                    containers::set(m, ks[i], vs[i]);
            } else if (args.size() >= 2 && (args[0].isChar() || args[0].isString())) {
                KVPayload *p = mut(m); // configure KeyType/ValueType options
                for (size_t i = 0; i + 1 < args.size(); i += 2) {
                    std::string opt = args[i].toString();
                    if (opt == "KeyType")
                        p->keyType = args[i + 1].toString();
                    else if (opt == "ValueType")
                        p->valType = args[i + 1].toString();
                }
            }
            return m;
        };
        cm.subsref = [](Value &self, Span<const Value> args, size_t, Span<Value> out,
                        CallContext &) { out[0] = containers::get(self, args[0]); };
        cm.subsasgn = [](Value &self, Span<const Value> args, size_t, Span<Value>,
                         CallContext &) {
            containers::set(self, args[0], args[args.size() - 1]);
        };
        cm.methods["keys"] = [](Value &self, Span<const Value>, size_t, Span<Value> out,
                                CallContext &ctx) {
            out[0] = containers::keys(self, ctx.engine->resource());
        };
        cm.methods["values"] = [](Value &self, Span<const Value>, size_t, Span<Value> out,
                                  CallContext &ctx) {
            out[0] = containers::values(self, ctx.engine->resource());
        };
        cm.methods["isKey"] = [](Value &self, Span<const Value> a, size_t, Span<Value> out,
                                 CallContext &ctx) {
            out[0] = Value::logicalScalar(containers::isKey(self, a[0]), ctx.engine->resource());
        };
        cm.methods["length"] = [](Value &self, Span<const Value>, size_t, Span<Value> out,
                                  CallContext &ctx) {
            out[0] = Value::scalar(static_cast<double>(containers::count(self)),
                                   ctx.engine->resource());
        };
        cm.methods["remove"] = [](Value &self, Span<const Value> a, size_t, Span<Value> out,
                                  CallContext &) {
            containers::remove(self, a[0]);
            out[0] = self; // MATLAB returns the (mutated) map
        };
        cm.propGet = [](const Value &self, const std::string &name, Value &out,
                        CallContext &ctx) -> bool {
            const KVPayload *p = cst(self);
            auto *mr = ctx.engine->resource();
            if (name == "Count") {
                out = Value::scalar(static_cast<double>(p->count()), mr);
                return true;
            }
            if (name == "KeyType") {
                out = Value::fromString(p->keyType, mr);
                return true;
            }
            if (name == "ValueType") {
                out = Value::fromString(p->valType, mr);
                return true;
            }
            return false;
        };
        cm.dispText = [](const Value &self) -> std::string {
            const KVPayload *p = cst(self);
            std::ostringstream os;
            os << "  Map with properties:\n\n"
               << "        Count: " << p->count() << "\n"
               << "      KeyType: " << p->keyType << "\n"
               << "    ValueType: " << p->valType << "\n";
            return os.str();
        };
        engine.registerClass(std::move(cm));
    }
}

} // namespace numkit::runtime
