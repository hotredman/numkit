// src/bundle/src/register/builtin/polyfun_reg.cpp

#include <nk/builtin/polyfun.hpp>
#include <nk/core/engine.hpp>
#include <nk/value/value.hpp>
#include <nk/value/span.hpp>

namespace nk::builtin {
void registerIntegralM(Engine &engine);
void registerInlineImpulseM(Engine &engine);
}

namespace nk::builtin::detail {

void cumtrapz_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void gk15_nodes_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void integral2_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void integral3_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void interp1_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void interp2_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void interp3_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void interpn_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void makima_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void mkpp_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void padecoef_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void pchip_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void poly_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void polyder_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void polydiv_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void polyfit_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void polyint_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void polyval_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void polyvalm_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void ppval_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void quad2d_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void quadgk_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void residue_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void residuez_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void roots_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void spline_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void tf2zp_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void trapz_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void unmkpp_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void zp2tf_reg(Span<const Value>, size_t, Span<Value>, CallContext&);

} // namespace nk::builtin::detail

namespace nk::bundle::builtin {

void register_polyfun(Engine &engine) {
    engine.registerFunction("__gk15_nodes", &::nk::builtin::detail::gk15_nodes_reg);
    ::nk::builtin::registerIntegralM(engine);
    ::nk::builtin::registerInlineImpulseM(engine);
    engine.registerFunction("integral2", &::nk::builtin::detail::integral2_reg);
    engine.registerFunction("integral3", &::nk::builtin::detail::integral3_reg);
    engine.registerFunction("quadgk",    &::nk::builtin::detail::quadgk_reg);
    engine.registerFunction("quad2d",    &::nk::builtin::detail::quad2d_reg);

    engine.registerFunction("cumtrapz",  &::nk::builtin::detail::cumtrapz_reg);
    engine.registerFunction("interp1",   &::nk::builtin::detail::interp1_reg);
    engine.registerFunction("interp2",   &::nk::builtin::detail::interp2_reg);
    engine.registerFunction("interp3",   &::nk::builtin::detail::interp3_reg);
    engine.registerFunction("interpn",   &::nk::builtin::detail::interpn_reg);
    engine.registerFunction("makima",    &::nk::builtin::detail::makima_reg);
    engine.registerFunction("mkpp",      &::nk::builtin::detail::mkpp_reg);
    engine.registerFunction("padecoef",  &::nk::builtin::detail::padecoef_reg);
    engine.registerFunction("pchip",     &::nk::builtin::detail::pchip_reg);
    engine.registerFunction("poly",      &::nk::builtin::detail::poly_reg);
    engine.registerFunction("polyder",   &::nk::builtin::detail::polyder_reg);
    engine.registerFunction("polydiv",   &::nk::builtin::detail::polydiv_reg);
    engine.registerFunction("polyfit",   &::nk::builtin::detail::polyfit_reg);
    engine.registerFunction("polyint",   &::nk::builtin::detail::polyint_reg);
    engine.registerFunction("polyval",   &::nk::builtin::detail::polyval_reg);
    engine.registerFunction("polyvalm",  &::nk::builtin::detail::polyvalm_reg);
    engine.registerFunction("ppval",     &::nk::builtin::detail::ppval_reg);
    engine.registerFunction("residue",   &::nk::builtin::detail::residue_reg);
    engine.registerFunction("residuez",  &::nk::builtin::detail::residuez_reg);
    engine.registerFunction("roots",     &::nk::builtin::detail::roots_reg);
    engine.registerFunction("spline",    &::nk::builtin::detail::spline_reg);
    engine.registerFunction("tf2zp",     &::nk::builtin::detail::tf2zp_reg);
    engine.registerFunction("trapz",     &::nk::builtin::detail::trapz_reg);
    engine.registerFunction("unmkpp",    &::nk::builtin::detail::unmkpp_reg);
    engine.registerFunction("zp2tf",     &::nk::builtin::detail::zp2tf_reg);
}

} // namespace nk::bundle::builtin
