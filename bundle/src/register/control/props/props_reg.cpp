// toolboxes/control/src/props/props_reg.cpp
//
// Register half of the system-property builtins: the CallContext wrappers
// (isct/isdt/issiso/isproper/isstable/order/pole/zero/damp/pzmap/isstatic/
// tzero) that delegate to the engine-free compute in props.cpp. library.cpp
// forward-declares + registers these by name.
//
// Phase 2b compute/register split — see project_layering_refactor memory.

#include <numkit/control/props/props.hpp>

#include <numkit/core/engine.hpp>   // CallContext, ctx.engine->resource()
#include <numkit/value/error.hpp>

#include <utility>

namespace numkit::control {
namespace detail {

void isct_reg(Span<const Value> a, size_t, Span<Value> o, CallContext &c)
{ if (a.empty()) throw Error("isct: needs sys", 0, 0, "isct", "", "numkit:isct:nargin");
  o[0] = isct(a[0], c.engine->resource()); }

void isdt_reg(Span<const Value> a, size_t, Span<Value> o, CallContext &c)
{ if (a.empty()) throw Error("isdt: needs sys", 0, 0, "isdt", "", "numkit:isdt:nargin");
  o[0] = isdt(a[0], c.engine->resource()); }

void issiso_reg(Span<const Value> a, size_t, Span<Value> o, CallContext &c)
{ if (a.empty()) throw Error("issiso: needs sys", 0, 0, "issiso", "", "numkit:issiso:nargin");
  o[0] = issiso(a[0], c.engine->resource()); }

void isproper_reg(Span<const Value> a, size_t, Span<Value> o, CallContext &c)
{ if (a.empty()) throw Error("isproper: needs sys", 0, 0, "isproper", "", "numkit:isproper:nargin");
  o[0] = isproper(a[0], c.engine->resource()); }

void isstable_reg(Span<const Value> a, size_t, Span<Value> o, CallContext &c)
{ if (a.empty()) throw Error("isstable: needs sys", 0, 0, "isstable", "", "numkit:isstable:nargin");
  o[0] = isstable(a[0], c.engine->resource()); }

void order_reg(Span<const Value> a, size_t, Span<Value> o, CallContext &c)
{ if (a.empty()) throw Error("order: needs sys", 0, 0, "order", "", "numkit:order:nargin");
  o[0] = order(a[0], c.engine->resource()); }

void pole_reg(Span<const Value> a, size_t, Span<Value> o, CallContext &c)
{ if (a.empty()) throw Error("pole: needs sys", 0, 0, "pole", "", "numkit:pole:nargin");
  o[0] = pole(a[0], c.engine->resource()); }

void zero_reg(Span<const Value> a, size_t, Span<Value> o, CallContext &c)
{ if (a.empty()) throw Error("zero: needs sys", 0, 0, "zero", "", "numkit:zero:nargin");
  o[0] = zero(a[0], c.engine->resource()); }

void damp_reg(Span<const Value> a, size_t, Span<Value> o, CallContext &c)
{
    if (a.empty()) throw Error("damp: needs sys", 0, 0, "damp", "", "numkit:damp:nargin");
    auto d = damp(a[0], c.engine->resource());
    if (o.size() >= 1) o[0] = std::move(d.wn);
    if (o.size() >= 2) o[1] = std::move(d.zeta);
    if (o.size() >= 3) o[2] = std::move(d.p);
}

void pzmap_reg(Span<const Value> a, size_t, Span<Value> o, CallContext &c)
{
    if (a.empty()) throw Error("pzmap: needs sys", 0, 0, "pzmap", "", "numkit:pzmap:nargin");
    auto [p, z] = pzmap(a[0], c.engine->resource());
    if (o.size() >= 1) o[0] = std::move(p);
    if (o.size() >= 2) o[1] = std::move(z);
}

void isstatic_reg(Span<const Value> a, size_t, Span<Value> o, CallContext &c)
{ if (a.empty()) throw Error("isstatic: needs sys", 0, 0, "isstatic", "", "numkit:isstatic:nargin");
  o[0] = isstatic(a[0], c.engine->resource()); }

void tzero_reg(Span<const Value> a, size_t, Span<Value> o, CallContext &c)
{ if (a.empty()) throw Error("tzero: needs sys", 0, 0, "tzero", "", "numkit:tzero:nargin");
  o[0] = tzero(a[0], c.engine->resource()); }

} // namespace detail
} // namespace numkit::control
