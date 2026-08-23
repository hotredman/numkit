// src/builtin/src/datatypes/structs_cells.cpp
//
// Structure and cell array utility implementations for numkit::builtin.

#include <numkit/builtin/datatypes.hpp>
#include <numkit/value/value.hpp>
#include <numkit/value/span.hpp>
#include <numkit/runtime/language/structures/struct.hpp>

namespace numkit::builtin {

Value isfield(const Value &s, const std::string &field, std::pmr::memory_resource *mr) {
    return isfield(s, Value::fromString(field, mr), mr);
}

Value getfield(const Value &s, const std::string &field, std::pmr::memory_resource *mr) {
    return getfield(s, Value::fromString(field, mr), mr);
}

Value setfield(const Value &s, const std::string &field, const Value &val, std::pmr::memory_resource *mr) {
    return setfield(s, Value::fromString(field, mr), val, mr);
}

Value rmfield(const Value &s, const std::string &field, std::pmr::memory_resource *mr) {
    return rmfield(s, Value::fromString(field, mr), mr);
}

} // namespace numkit::builtin
