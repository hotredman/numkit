// src/builtin/src/polyfun.cpp
//
// Polynomials, interpolation, and integration implementations.
#include <numkit/builtin/polyfun.hpp>
#include <numkit/core/engine.hpp>
#include <numkit/math/poly/polynomials.hpp>
#include <numkit/math/interp/interp.hpp>
#include <numkit/math/integration/integration.hpp>

namespace numkit::builtin {

Value roots(const Value &p, std::pmr::memory_resource *mr) { return numkit::math::roots(p, mr); }
Value poly(const Value &r, std::pmr::memory_resource *mr) { return numkit::math::poly(r, mr); }
Value polyval(const Value &p, const Value &x, std::pmr::memory_resource *mr) { return numkit::math::polyval(p, x, mr); }
Value polyder(const Value &p, std::pmr::memory_resource *mr) { return numkit::math::polyder(p, mr); }
Value polyint(const Value &p, double k, std::pmr::memory_resource *mr) { return numkit::math::polyint(p, k, mr); }
Value polyfit(const Value &x, const Value &y, size_t n, std::pmr::memory_resource *mr) { return numkit::math::polyfit(x, y, n, mr); }

Value interp1(const Value &x, const Value &v, const Value &xq, const std::string &method, std::pmr::memory_resource *mr)
{
    return numkit::math::interp1(x, v, xq, method, mr);
}

Value interp2(const Value &x, const Value &y, const Value &v, const Value &xq, const Value &yq, const std::string &method, std::pmr::memory_resource *mr)
{
    return numkit::math::interp2(x, y, v, xq, yq, method, mr);
}

Value spline(const Value &x, const Value &y, const Value &xq, std::pmr::memory_resource *mr)
{
    return numkit::math::spline(x, y, xq, mr);
}

Value pchip(const Value &x, const Value &y, const Value &xq, std::pmr::memory_resource *mr)
{
    return numkit::math::pchip(x, y, xq, mr);
}

Value mkpp(const Value &breaks, const Value &coefs, std::pmr::memory_resource *mr)
{
    return numkit::math::mkpp(breaks, coefs, mr);
}

Value unmkpp(const Value &pp, std::pmr::memory_resource *mr)
{
    (void)mr;
    if (pp.isStruct() && pp.hasField("breaks"))
        return pp.field("breaks");
    return Value();
}

Value ppval(const Value &pp, const Value &xq, std::pmr::memory_resource *mr)
{
    return numkit::math::ppval(pp, xq, mr);
}

Value trapz(const Value &y, std::pmr::memory_resource *mr)
{
    return numkit::math::trapz(y, mr);
}

Value trapz(const Value &x, const Value &y, int /*dim*/, std::pmr::memory_resource *mr)
{
    return numkit::math::trapz(x, y, mr);
}

Value cumtrapz(const Value &y, std::pmr::memory_resource *mr)
{
    return numkit::math::cumtrapz(y, mr);
}

Value cumtrapz(const Value &x, const Value &y, int dim, std::pmr::memory_resource *mr)
{
    if (dim > 0) {
        return numkit::math::cumtrapzDim(y, dim, mr);
    }
    return numkit::math::cumtrapz(x, y, mr);
}

namespace detail {
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
} // namespace detail

void registerIntegralM(Engine &engine);

void register_polyfun(Engine &engine) {
    engine.registerFunction("__gk15_nodes", &::numkit::builtin::detail::gk15_nodes_reg);
    ::numkit::builtin::registerIntegralM(engine);
    engine.registerFunction("integral2", &::numkit::builtin::detail::integral2_reg);
    engine.registerFunction("integral3", &::numkit::builtin::detail::integral3_reg);
    engine.registerFunction("quadgk",    &::numkit::builtin::detail::quadgk_reg);
    engine.registerFunction("quad2d",    &::numkit::builtin::detail::quad2d_reg);

    engine.registerFunction("cumtrapz",  &::numkit::builtin::detail::cumtrapz_reg);
    engine.registerFunction("interp1",   &::numkit::builtin::detail::interp1_reg);
    engine.registerFunction("interp2",   &::numkit::builtin::detail::interp2_reg);
    engine.registerFunction("interp3",   &::numkit::builtin::detail::interp3_reg);
    engine.registerFunction("interpn",   &::numkit::builtin::detail::interpn_reg);
    engine.registerFunction("makima",    &::numkit::builtin::detail::makima_reg);
    engine.registerFunction("mkpp",      &::numkit::builtin::detail::mkpp_reg);
    engine.registerFunction("padecoef",  &::numkit::builtin::detail::padecoef_reg);
    engine.registerFunction("pchip",     &::numkit::builtin::detail::pchip_reg);
    engine.registerFunction("poly",      &::numkit::builtin::detail::poly_reg);
    engine.registerFunction("polyder",   &::numkit::builtin::detail::polyder_reg);
    engine.registerFunction("polydiv",   &::numkit::builtin::detail::polydiv_reg);
    engine.registerFunction("polyfit",   &::numkit::builtin::detail::polyfit_reg);
    engine.registerFunction("polyint",   &::numkit::builtin::detail::polyint_reg);
    engine.registerFunction("polyval",   &::numkit::builtin::detail::polyval_reg);
    engine.registerFunction("polyvalm",  &::numkit::builtin::detail::polyvalm_reg);
    engine.registerFunction("ppval",     &::numkit::builtin::detail::ppval_reg);
    engine.registerFunction("residue",   &::numkit::builtin::detail::residue_reg);
    engine.registerFunction("residuez",  &::numkit::builtin::detail::residuez_reg);
    engine.registerFunction("roots",     &::numkit::builtin::detail::roots_reg);
    engine.registerFunction("spline",    &::numkit::builtin::detail::spline_reg);
    engine.registerFunction("tf2zp",     &::numkit::builtin::detail::tf2zp_reg);
    engine.registerFunction("trapz",     &::numkit::builtin::detail::trapz_reg);
    engine.registerFunction("unmkpp",    &::numkit::builtin::detail::unmkpp_reg);
    engine.registerFunction("zp2tf",     &::numkit::builtin::detail::zp2tf_reg);
}

} // namespace numkit::builtin
