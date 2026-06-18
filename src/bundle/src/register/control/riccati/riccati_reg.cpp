// toolboxes/control/src/riccati/riccati_reg.cpp
//
// Register half of the algebraic Riccati solvers: the CallContext
// builtins care / dare that delegate to the engine-free compute in
// riccati.cpp. library.cpp forward-declares + registers these by name.
//
// MATLAB output order is [X, L, G]: solution, closed-loop eigenvalues,
// gain. R defaults to the m×m identity when omitted (care(A,B,Q)).
//
// Phase 2b compute/register split — see project_layering_refactor memory.

#include <numkit/control/riccati/riccati.hpp>

#include <numkit/core/engine.hpp>   // CallContext, ctx.engine->resource()
#include <numkit/value/error.hpp>

namespace numkit::control {
namespace detail {

namespace {

// m×m identity Value (default R for the 3-argument forms).
Value eyeVal(size_t m, std::pmr::memory_resource *mr) {
    Value I = Value::matrix(m, m, ValueType::DOUBLE, mr);
    double *d = I.doubleDataMut();
    for (size_t i = 0; i < m; ++i) d[i * m + i] = 1.0;
    return I;
}

void emit(const RiccatiResult &r, Span<Value> o) {
    if (o.size() >= 1) o[0] = r.X;
    if (o.size() >= 2) o[1] = r.L;
    if (o.size() >= 3) o[2] = r.G;
}

} // anonymous

void care_reg(Span<const Value> a, size_t, Span<Value> o, CallContext &c)
{
    if (a.size() < 3)
        throw Error("care: requires (A, B, Q) and optionally R",
                    0, 0, "care", "", "numkit:care:nargin");
    auto *mr = c.engine->resource();
    Value R = (a.size() >= 4) ? a[3] : eyeVal(a[1].dims().cols(), mr);
    emit(care(a[0], a[1], a[2], R, mr), o);
}

void dare_reg(Span<const Value> a, size_t, Span<Value> o, CallContext &c)
{
    if (a.size() < 3)
        throw Error("dare: requires (A, B, Q) and optionally R",
                    0, 0, "dare", "", "numkit:dare:nargin");
    auto *mr = c.engine->resource();
    Value R = (a.size() >= 4) ? a[3] : eyeVal(a[1].dims().cols(), mr);
    emit(dare(a[0], a[1], a[2], R, mr), o);
}

namespace {
// lqr / dlqr are thin wrappers on care / dare. MATLAB returns [K, S, P]:
// optimal gain, Riccati solution, closed-loop poles — i.e. care's
// {G, X, L} re-ordered. (Cross-term N and the lqr(sys,…) form are
// deferred; the (A,B,Q[,R]) signature covers the common case.)
void emitLqr(const RiccatiResult &r, Span<Value> o) {
    if (o.size() >= 1) o[0] = r.G;   // K — optimal gain
    if (o.size() >= 2) o[1] = r.X;   // S — Riccati solution
    if (o.size() >= 3) o[2] = r.L;   // P — closed-loop poles
}
} // anonymous

void lqr_reg(Span<const Value> a, size_t, Span<Value> o, CallContext &c)
{
    if (a.size() < 3)
        throw Error("lqr: requires (A, B, Q) and optionally R",
                    0, 0, "lqr", "", "numkit:lqr:nargin");
    auto *mr = c.engine->resource();
    Value R = (a.size() >= 4) ? a[3] : eyeVal(a[1].dims().cols(), mr);
    emitLqr(care(a[0], a[1], a[2], R, mr), o);
}

void dlqr_reg(Span<const Value> a, size_t, Span<Value> o, CallContext &c)
{
    if (a.size() < 3)
        throw Error("dlqr: requires (A, B, Q) and optionally R",
                    0, 0, "dlqr", "", "numkit:dlqr:nargin");
    auto *mr = c.engine->resource();
    Value R = (a.size() >= 4) ? a[3] : eyeVal(a[1].dims().cols(), mr);
    emitLqr(dare(a[0], a[1], a[2], R, mr), o);
}

} // namespace detail
} // namespace numkit::control
