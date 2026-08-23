// src/builtin/src/iofun/formatted_io.cpp
//
// Formatted input/output and stream printing implementations for numkit::builtin.

#include <numkit/builtin/iofun.hpp>
#include <numkit/value/value.hpp>
#include <numkit/value/span.hpp>
#include <numkit/lang/strings/format.hpp>
#include <numkit/lang/strings/print.hpp>
#include <numkit/lang/strings/scan.hpp>

namespace numkit::builtin {

Value sprintf(const Value &fmt, Span<const Value> args, std::pmr::memory_resource *mr) {
    return numkit::lang::sprintf(fmt, args, mr);
}

std::string sprintf(const std::string &fmt, Span<const Value> args, std::pmr::memory_resource *mr) {
    return numkit::lang::formatCyclic(fmt, args, 0, mr);
}

void disp(const Value &v, std::ostream &os) {
    os << numkit::lang::dispFormat(v);
}

int fprintf(std::ostream &os, const std::string &fmt, Span<const Value> args, std::pmr::memory_resource *mr) {
    std::string s = numkit::lang::formatCyclic(fmt, args, 0, mr);
    os << s;
    return static_cast<int>(s.size());
}

Value sscanf(const std::string &str, const std::string &fmt, std::pmr::memory_resource *mr) {
    Value args[2] = { Value::fromString(str, mr), Value::fromString(fmt, mr) };
    Value out;
    numkit::lang::sscanf(Span<const Value>(args, 2), 1, Span<Value>(&out, 1), mr);
    return out;
}

Value textscan(const std::string &str, const std::string &fmt, std::pmr::memory_resource *mr) {
    Value args[2] = { Value::fromString(str, mr), Value::fromString(fmt, mr) };
    Value out;
    numkit::lang::sscanf(Span<const Value>(args, 2), 1, Span<Value>(&out, 1), mr);
    return out;
}

} // namespace numkit::builtin
