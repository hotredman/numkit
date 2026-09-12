// src/builtin/src/iofun/formatted_io.cpp
//
// Formatted input/output and stream printing implementations for nk::builtin.

#include <nk/builtin/iofun.hpp>
#include <nk/value/value.hpp>
#include <nk/value/span.hpp>
#include <nk/builtin/strfun.hpp>
#include <nk/builtin/strfun.hpp>
#include <nk/builtin/strfun.hpp>

namespace nk::builtin {

std::string sprintf(const std::string &fmt, Span<const Value> args, std::pmr::memory_resource *mr) {
    return nk::builtin::formatCyclic(fmt, args, 0, mr);
}

void disp(const Value &v, std::ostream &os) {
    os << nk::builtin::dispFormat(v);
}

int fprintf(std::ostream &os, const std::string &fmt, Span<const Value> args, std::pmr::memory_resource *mr) {
    std::string s = nk::builtin::formatCyclic(fmt, args, 0, mr);
    os << s;
    return static_cast<int>(s.size());
}

Value sscanf(const std::string &str, const std::string &fmt, std::pmr::memory_resource *mr) {
    Value args[2] = { Value::fromString(str, mr), Value::fromString(fmt, mr) };
    Value out;
    nk::builtin::sscanf(Span<const Value>(args, 2), 1, Span<Value>(&out, 1), mr);
    return out;
}

Value textscan(const std::string &str, const std::string &fmt, std::pmr::memory_resource *mr) {
    Value args[2] = { Value::fromString(str, mr), Value::fromString(fmt, mr) };
    Value out;
    nk::builtin::sscanf(Span<const Value>(args, 2), 1, Span<Value>(&out, 1), mr);
    return out;
}

} // namespace nk::builtin
