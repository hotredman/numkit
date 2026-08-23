// src/builtin/src/datatypes/limits.cpp
//
// Type casting and numeric limits implementations for numkit::builtin.

#include <numkit/builtin/datatypes.hpp>
#include <numkit/value/value.hpp>
#include <numkit/value/span.hpp>
#include <numkit/lang/types/types.hpp>

namespace numkit::builtin {

Value cast(const Value &v, const std::string &targetType, std::pmr::memory_resource *mr) {
    return numkit::lang::cast(v, targetType, mr);
}

Value realmin(const std::string &className, std::pmr::memory_resource *mr) {
    return numkit::lang::realmin(className.empty() ? Value::Empty : Value::fromString(className, mr), mr);
}

Value realmax(const std::string &className, std::pmr::memory_resource *mr) {
    return numkit::lang::realmax(className.empty() ? Value::Empty : Value::fromString(className, mr), mr);
}

Value intmin(const std::string &className, std::pmr::memory_resource *mr) {
    return numkit::lang::intmin(className.empty() ? Value::Empty : Value::fromString(className, mr), mr);
}

Value intmax(const std::string &className, std::pmr::memory_resource *mr) {
    return numkit::lang::intmax(className.empty() ? Value::Empty : Value::fromString(className, mr), mr);
}

Value flintmax(const std::string &className, std::pmr::memory_resource *mr) {
    return numkit::lang::flintmax(className.empty() ? Value::Empty : Value::fromString(className, mr), mr);
}

} // namespace numkit::builtin
