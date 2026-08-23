#include <numkit/builtin/datatypes.hpp>
#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/span.hpp>

namespace numkit::builtin {

Value isfield(const Value &s, const std::string &field, std::pmr::memory_resource *mr) {
    if (!s.isStruct()) return Value::logicalScalar(false, mr);
    if (s.isStructArray()) {
        if (s.numel() == 0) return Value::logicalScalar(false, mr);
        const auto &elem0 = s.structArrayElem(0);
        return Value::logicalScalar(elem0.count(field) > 0, mr);
    }
    return Value::logicalScalar(s.hasField(field), mr);
}

Value isfield(const Value &s, const Value &field, std::pmr::memory_resource *mr) {
    return isfield(s, field.toString(), mr);
}

Value getfield(const Value &s, const std::string &field, std::pmr::memory_resource *) {
    if (!s.isStruct())
        throw Error("getfield requires a struct", 0, 0, "getfield", "", "numkit:getfield:notStruct");
    if (s.isStructArray()) {
        if (s.numel() == 0)
            throw Error("getfield: struct array is empty", 0, 0, "getfield", "", "numkit:getfield:emptyArray");
        const auto &elem0 = s.structArrayElem(0);
        auto it = elem0.find(field);
        if (it == elem0.end())
            throw Error("getfield: no such field '" + field + "'", 0, 0, "getfield", "", "numkit:getfield:noField");
        return it->second;
    }
    if (!s.hasField(field))
        throw Error("getfield: no such field '" + field + "'", 0, 0, "getfield", "", "numkit:getfield:noField");
    return s.field(field);
}

Value getfield(const Value &s, const Value &field, std::pmr::memory_resource *mr) {
    return getfield(s, field.toString(), mr);
}

Value setfield(const Value &s, const std::string &field, const Value &val, std::pmr::memory_resource *mr) {
    Value out;
    if (s.isStruct()) {
        out = s;
    } else if (s.isEmpty()) {
        out = Value::structure(mr);
    } else {
        throw Error("setfield: first argument must be a struct or []", 0, 0, "setfield", "", "numkit:setfield:notStruct");
    }
    if (out.isStructArray()) {
        out.setFieldAll(field, val);
    } else {
        out.field(field) = val;
    }
    return out;
}

Value setfield(const Value &s, const Value &field, const Value &val, std::pmr::memory_resource *mr) {
    return setfield(s, field.toString(), val, mr);
}

Value rmfield(const Value &s, const std::string &field, std::pmr::memory_resource *) {
    if (!s.isStruct())
        throw Error("rmfield requires a struct", 0, 0, "rmfield", "", "numkit:rmfield:notStruct");
    Value out = s;
    out.removeField(field);
    return out;
}

Value rmfield(const Value &s, const Value &field, std::pmr::memory_resource *mr) {
    return rmfield(s, field.toString(), mr);
}

} // namespace numkit::builtin
