// src/bundle/src/register/builtin/iofun_reg.cpp

#include <nk/builtin/iofun.hpp>
#include <nk/core/engine.hpp>
#include <nk/core/types.hpp>
#include <nk/value/value.hpp>
#include <nk/value/span.hpp>

namespace nk::builtin::detail {
void disp_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void fprintf_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void fscanf_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void sprintf_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void sscanf_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void textscan_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
} // namespace nk::builtin::detail

namespace nk::bundle::builtin {

void register_iofun(Engine &engine) {
    engine.registerFunction("sprintf",    &::nk::builtin::detail::sprintf_reg);
    engine.registerFunction("disp",       &::nk::builtin::detail::disp_reg);
    engine.registerFunction("fprintf",    &::nk::builtin::detail::fprintf_reg);
    engine.registerFunction("fscanf",     &::nk::builtin::detail::fscanf_reg);
    engine.registerFunction("sscanf",     &::nk::builtin::detail::sscanf_reg);
    engine.registerFunction("textscan",   &::nk::builtin::detail::textscan_reg);
}

} // namespace nk::bundle::builtin
