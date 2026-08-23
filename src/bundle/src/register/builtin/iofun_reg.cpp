// src/bundle/src/register/builtin/iofun_reg.cpp

#include <numkit/builtin/iofun.hpp>
#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>
#include <numkit/value/value.hpp>
#include <numkit/value/span.hpp>

namespace numkit::builtin::detail {
void disp_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void fprintf_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void fscanf_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void sprintf_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void sscanf_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void textscan_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
} // namespace numkit::builtin::detail

namespace numkit::bundle::builtin {

void register_iofun(Engine &engine) {
    engine.registerFunction("sprintf",    &::numkit::builtin::detail::sprintf_reg);
    engine.registerFunction("disp",       &::numkit::builtin::detail::disp_reg);
    engine.registerFunction("fprintf",    &::numkit::builtin::detail::fprintf_reg);
    engine.registerFunction("fscanf",     &::numkit::builtin::detail::fscanf_reg);
    engine.registerFunction("sscanf",     &::numkit::builtin::detail::sscanf_reg);
    engine.registerFunction("textscan",   &::numkit::builtin::detail::textscan_reg);
}

} // namespace numkit::bundle::builtin
