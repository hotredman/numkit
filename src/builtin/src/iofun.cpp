// src/builtin/src/iofun.cpp
//
// Formatted input/output and stream printing implementations and registrations.
#include <numkit/builtin/iofun.hpp>
#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>
#include <numkit/value/value.hpp>
#include <numkit/value/span.hpp>
#include <numkit/lang/strings/format.hpp>
#include <numkit/lang/strings/print.hpp>
#include <numkit/lang/strings/scan.hpp>

namespace numkit::builtin {

namespace detail {
void disp_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void fprintf_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void fscanf_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void sprintf_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void sscanf_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void textscan_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
} // namespace detail

Value sprintf(const Value &fmt, Span<const Value> args, std::pmr::memory_resource *mr)
{
    return numkit::lang::sprintf(fmt, args, mr);
}

std::string sprintf(const std::string &fmt, Span<const Value> args, std::pmr::memory_resource *mr)
{
    return numkit::lang::formatCyclic(fmt, args, 0, mr);
}

void disp(const Value &v, std::ostream &os)
{
    os << numkit::lang::dispFormat(v);
}

int fprintf(std::ostream &os, const std::string &fmt, Span<const Value> args, std::pmr::memory_resource *mr)
{
    std::string s = numkit::lang::formatCyclic(fmt, args, 0, mr);
    os << s;
    return static_cast<int>(s.size());
}

Value sscanf(const std::string &str, const std::string &fmt, std::pmr::memory_resource *mr)
{
    Value args[2] = { Value::fromString(str, mr), Value::fromString(fmt, mr) };
    Value out;
    numkit::lang::sscanf(Span<const Value>(args, 2), 1, Span<Value>(&out, 1), mr);
    return out;
}

Value textscan(const std::string &str, const std::string &fmt, std::pmr::memory_resource *mr)
{
    Value args[2] = { Value::fromString(str, mr), Value::fromString(fmt, mr) };
    Value out;
    numkit::lang::sscanf(Span<const Value>(args, 2), 1, Span<Value>(&out, 1), mr);
    return out;
}

void register_iofun(Engine &engine) {
    engine.registerFunction("sprintf",    &::numkit::builtin::detail::sprintf_reg);
    engine.registerFunction("disp",       &::numkit::builtin::detail::disp_reg);
    engine.registerFunction("fprintf",    &::numkit::builtin::detail::fprintf_reg);
    engine.registerFunction("fscanf",     &::numkit::builtin::detail::fscanf_reg);
    engine.registerFunction("sscanf",     &::numkit::builtin::detail::sscanf_reg);
    engine.registerFunction("textscan",   &::numkit::builtin::detail::textscan_reg);
}

} // namespace numkit::builtin
