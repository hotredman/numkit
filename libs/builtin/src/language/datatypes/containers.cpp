// libs/builtin/src/language/datatypes/containers.cpp
//
// Key–value container classes built on the engine object model
// (OBJECT_MODEL.md): the modern value-semantics `dictionary` (R2022b+)
// and the legacy handle-semantics `containers.Map`. Both share one
// opaque KVPayload; the value/handle difference is entirely the
// registry `isHandle` flag + the engine's COW clone rule.
#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <numkit/core/object.hpp>

#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace numkit {
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

    void set(const Value &key, const Value &val)
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

KVPayload *payloadOf(const Value &self)
{
    auto *st = self.objectStateConst();
    return st ? static_cast<KVPayload *>(st->native.get()) : nullptr;
}
KVPayload *payloadOfMut(Value &self)
{
    auto *st = self.objectStateMut(); // detach (COW) → value/handle rule
    return st ? static_cast<KVPayload *>(st->native.get()) : nullptr;
}

// Build a keys Value (string array or numeric column) for keys()/disp.
Value keysValue(const KVPayload *p, std::pmr::memory_resource *mr)
{
    if (p->keyKind == KVPayload::Num) {
        Value v = Value::matrix(p->nkeys.size(), 1, ValueType::DOUBLE, mr);
        for (size_t i = 0; i < p->nkeys.size(); ++i)
            v.doubleDataMut()[i] = p->nkeys[i];
        return v;
    }
    // string keys → string column
    Value v = Value::stringArray(p->skeys.size(), 1, mr);
    for (size_t i = 0; i < p->skeys.size(); ++i)
        v.stringElemSet(i, p->skeys[i]);
    return v;
}
Value valuesValue(const KVPayload *p, std::pmr::memory_resource *mr)
{
    // Numeric values → numeric column; otherwise a cell column.
    bool allNum = true;
    for (const auto &v : p->vals)
        if (!(v.isScalar() && v.type() == ValueType::DOUBLE))
            allNum = false;
    if (allNum && !p->vals.empty()) {
        Value out = Value::matrix(p->vals.size(), 1, ValueType::DOUBLE, mr);
        for (size_t i = 0; i < p->vals.size(); ++i)
            out.doubleDataMut()[i] = p->vals[i].toScalar();
        return out;
    }
    Value out = Value::cell(p->vals.size(), 1, mr);
    for (size_t i = 0; i < p->vals.size(); ++i)
        out.cellAt(i) = p->vals[i];
    return out;
}

// Extract a flat list of keys from a constructor argument.
void extractKeys(const Value &k, std::vector<Value> &out, std::pmr::memory_resource *mr)
{
    if (k.isCell()) {
        for (size_t i = 0; i < k.numel(); ++i)
            out.push_back(k.cellAt(i));
    } else if (k.isString()) {
        for (size_t i = 0; i < k.numel(); ++i)
            out.push_back(Value::stringScalar(k.stringElem(i), mr));
    } else if (k.isChar()) {
        out.push_back(k); // a single char-row key
    } else {
        for (size_t i = 0; i < k.numel(); ++i)
            out.push_back(k.elemAt(i, mr));
    }
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

Value makeKV(const std::string &cls, std::shared_ptr<KVPayload> p, bool isHandle,
             std::pmr::memory_resource *mr)
{
    auto st = std::make_shared<ObjectState>(mr);
    st->native = std::move(p);
    return Value::object(cls, st, isHandle, mr);
}

} // namespace

// ============================================================
// Registration — called from BuiltinLibrary::install.
// ============================================================
void BuiltinLibrary::registerContainers(Engine &engine)
{
    auto *mr = engine.resource();
    (void) mr;

    // ── dictionary (value semantics) ─────────────────────────
    {
        BuiltinClass dict;
        dict.name = "dictionary";
        dict.isHandle = false;
        dict.propNames = {};

        dict.construct = [](Span<const Value> args, CallContext &ctx) -> Value {
            auto *m = ctx.engine->resource();
            auto p = std::make_shared<KVPayload>();
            if (args.size() >= 2) {
                std::vector<Value> ks, vs;
                extractKeys(args[0], ks, m);
                extractVals(args[1], vs, m);
                if (ks.size() != vs.size())
                    throw std::runtime_error("dictionary: keys and values must match in number");
                for (size_t i = 0; i < ks.size(); ++i)
                    p->set(ks[i], vs[i]);
            }
            return makeKV("dictionary", p, /*isHandle=*/false, m);
        };

        dict.subsref = [](Value &self, Span<const Value> args, size_t, Span<Value> out,
                          CallContext &) {
            KVPayload *p = payloadOf(self);
            int idx = p->find(args[0]);
            if (idx < 0)
                throw std::runtime_error("dictionary: key not found");
            out[0] = p->vals[idx];
        };
        dict.subsasgn = [](Value &self, Span<const Value> args, size_t, Span<Value>,
                           CallContext &) {
            // value class: payloadOfMut detaches → original copy untouched
            payloadOfMut(self)->set(args[0], args[args.size() - 1]);
        };

        dict.methods["numEntries"] = [](Value &self, Span<const Value>, size_t,
                                        Span<Value> out, CallContext &ctx) {
            out[0] = Value::scalar(static_cast<double>(payloadOf(self)->count()),
                                   ctx.engine->resource());
        };
        dict.methods["isKey"] = [](Value &self, Span<const Value> args, size_t,
                                   Span<Value> out, CallContext &ctx) {
            out[0] = Value::logicalScalar(payloadOf(self)->find(args[0]) >= 0,
                                          ctx.engine->resource());
        };
        dict.methods["keys"] = [](Value &self, Span<const Value>, size_t, Span<Value> out,
                                  CallContext &ctx) {
            out[0] = keysValue(payloadOf(self), ctx.engine->resource());
        };
        dict.methods["values"] = [](Value &self, Span<const Value>, size_t, Span<Value> out,
                                    CallContext &ctx) {
            out[0] = valuesValue(payloadOf(self), ctx.engine->resource());
        };
        dict.methods["isConfigured"] = [](Value &self, Span<const Value>, size_t,
                                          Span<Value> out, CallContext &ctx) {
            out[0] = Value::logicalScalar(payloadOf(self)->configured,
                                          ctx.engine->resource());
        };

        dict.dispText = [](const Value &self) -> std::string {
            const KVPayload *p = payloadOf(self);
            std::ostringstream os;
            const char *kt = (p->keyKind == KVPayload::Num) ? "double" : "string";
            os << "  dictionary (" << kt << " --> " << p->valType << ") with "
               << p->count() << " entries:\n";
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
            auto *m = ctx.engine->resource();
            auto p = std::make_shared<KVPayload>();
            // containers.Map(keys, values) — option-string form
            // ('KeyType',..,'ValueType',..) just configures empty.
            if (args.size() >= 2 && !args[0].isChar() && !args[0].isString()) {
                std::vector<Value> ks, vs;
                extractKeys(args[0], ks, m);
                extractVals(args[1], vs, m);
                for (size_t i = 0; i < ks.size() && i < vs.size(); ++i)
                    p->set(ks[i], vs[i]);
            } else if (args.size() >= 2
                       && (args[0].isChar() || args[0].isString())) {
                // 'KeyType'/'ValueType' name-value options.
                for (size_t i = 0; i + 1 < args.size(); i += 2) {
                    std::string opt = args[i].toString();
                    if (opt == "KeyType")
                        p->keyType = args[i + 1].toString();
                    else if (opt == "ValueType")
                        p->valType = args[i + 1].toString();
                }
            }
            return makeKV("containers.Map", p, /*isHandle=*/true, m);
        };

        cm.subsref = [](Value &self, Span<const Value> args, size_t, Span<Value> out,
                        CallContext &) {
            KVPayload *p = payloadOf(self);
            int idx = p->find(args[0]);
            if (idx < 0)
                throw std::runtime_error("containers.Map: the specified key is not present");
            out[0] = p->vals[idx];
        };
        cm.subsasgn = [](Value &self, Span<const Value> args, size_t, Span<Value>,
                         CallContext &) {
            payloadOfMut(self)->set(args[0], args[args.size() - 1]); // handle: shared
        };

        cm.methods["keys"] = [](Value &self, Span<const Value>, size_t, Span<Value> out,
                                CallContext &ctx) {
            // containers.Map keys() returns a cell row.
            const KVPayload *p = payloadOf(self);
            auto *m = ctx.engine->resource();
            Value c = Value::cell(1, p->count(), m);
            for (size_t i = 0; i < p->count(); ++i)
                c.cellAt(i) = (p->keyKind == KVPayload::Num)
                                  ? Value::scalar(p->nkeys[i], m)
                                  : Value::fromString(p->skeys[i], m);
            out[0] = c;
        };
        cm.methods["values"] = [](Value &self, Span<const Value>, size_t, Span<Value> out,
                                  CallContext &ctx) {
            const KVPayload *p = payloadOf(self);
            auto *m = ctx.engine->resource();
            Value c = Value::cell(1, p->count(), m);
            for (size_t i = 0; i < p->count(); ++i)
                c.cellAt(i) = p->vals[i];
            out[0] = c;
        };
        cm.methods["isKey"] = [](Value &self, Span<const Value> args, size_t,
                                 Span<Value> out, CallContext &ctx) {
            out[0] = Value::logicalScalar(payloadOf(self)->find(args[0]) >= 0,
                                          ctx.engine->resource());
        };
        cm.methods["length"] = [](Value &self, Span<const Value>, size_t, Span<Value> out,
                                  CallContext &ctx) {
            out[0] = Value::scalar(static_cast<double>(payloadOf(self)->count()),
                                   ctx.engine->resource());
        };
        cm.methods["remove"] = [](Value &self, Span<const Value> args, size_t,
                                  Span<Value> out, CallContext &ctx) {
            KVPayload *p = payloadOfMut(self);
            int idx = p->find(args[0]);
            if (idx >= 0) {
                if (p->keyKind == KVPayload::Num)
                    p->nkeys.erase(p->nkeys.begin() + idx);
                else
                    p->skeys.erase(p->skeys.begin() + idx);
                p->vals.erase(p->vals.begin() + idx);
            }
            out[0] = self; // MATLAB returns the (mutated) map
            (void) ctx;
        };

        cm.propGet = [](const Value &self, const std::string &name, Value &out,
                        CallContext &ctx) -> bool {
            const KVPayload *p = payloadOf(self);
            auto *m = ctx.engine->resource();
            if (name == "Count") {
                out = Value::scalar(static_cast<double>(p->count()), m);
                return true;
            }
            if (name == "KeyType") {
                out = Value::fromString(p->keyType, m);
                return true;
            }
            if (name == "ValueType") {
                out = Value::fromString(p->valType, m);
                return true;
            }
            return false;
        };

        cm.dispText = [](const Value &self) -> std::string {
            const KVPayload *p = payloadOf(self);
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

} // namespace numkit
