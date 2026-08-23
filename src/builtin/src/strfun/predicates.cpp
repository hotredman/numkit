// src/builtin/src/strfun/predicates.cpp
//
// String classification, predicates, and struct/cell conversions for numkit::builtin.

#include <numkit/builtin/strfun.hpp>
#include <numkit/value/value.hpp>
#include <numkit/value/span.hpp>
#include <numkit/lang/strings/strings.hpp>

namespace numkit::builtin {

Value isletter(const Value &s, std::pmr::memory_resource *mr) { return numkit::lang::isletter(s, mr); }
Value isspace(const Value &s, std::pmr::memory_resource *mr) { return numkit::lang::isspaceFn(s, mr); }
Value isstrprop(const Value &s, const std::string &category, std::pmr::memory_resource *mr) {
    return numkit::lang::isstrprop(s, Value::fromString(category, mr), mr);
}
Value isstringscalar(const Value &s, std::pmr::memory_resource *mr) { return numkit::lang::isstringscalar(s, mr); }
Value validatestring(const Value &str, const Value &validStrings, std::pmr::memory_resource *mr) {
    return numkit::lang::validatestring(str, validStrings, mr);
}

Value convertContainedStringsToChars(const Value &v, std::pmr::memory_resource *mr)
{
    if (v.isString()) {
        if (v.numel() <= 1)
            return Value::fromString(v.toString(), mr);
        auto c = Value::cell(v.numel(), 1, mr);
        for (size_t i = 0; i < v.numel(); ++i)
            c.cellAt(i) = Value::fromString(v.stringElem(i), mr);
        return c;
    }
    if (v.isCell()) {
        const auto &d = v.dims();
        auto c = d.is3D()
                    ? Value::cell3D(d.rows(), d.cols(), d.pages(), mr)
                    : Value::cell(d.rows(), d.cols(), mr);
        for (size_t i = 0; i < v.numel(); ++i)
            c.cellAt(i) = convertContainedStringsToChars(v.cellAt(i), mr);
        return c;
    }
    if (v.isStruct() && !v.isStructArray()) {
        auto s = Value::structure(mr);
        for (auto &kv : v.structFields())
            s.field(kv.first) = convertContainedStringsToChars(kv.second, mr);
        return s;
    }
    return v;
}

} // namespace numkit::builtin
