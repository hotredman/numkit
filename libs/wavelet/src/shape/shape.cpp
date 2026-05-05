// libs/wavelet/src/shape/shape.cpp
//
// Continuous wavelet shape primitives: mexihat, morlet, meyeraux.

#include <numkit/wavelet/shape/shape.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace numkit::wavelet {

namespace {

// linspace(LB, UB, N) into a freshly-allocated Value row.
Value linspace_row(std::pmr::memory_resource *mr, double lb, double ub, size_t N)
{
    Value xv = Value::matrix(1, N, ValueType::DOUBLE, mr);
    if (N == 0) return xv;
    double *xd = xv.doubleDataMut();
    if (N == 1) { xd[0] = ub; return xv; }
    const double step = (ub - lb) / static_cast<double>(N - 1);
    for (size_t i = 0; i < N; ++i)
        xd[i] = lb + step * static_cast<double>(i);
    xd[N - 1] = ub;  // exact endpoint
    return xv;
}

} // anonymous

std::tuple<Value, Value>
mexihat(std::pmr::memory_resource *mr, double lb, double ub, size_t N)
{
    Value xv = linspace_row(mr, lb, ub, N);
    Value pv = Value::matrix(1, N, ValueType::DOUBLE, mr);
    if (N == 0) return {std::move(pv), std::move(xv)};
    const double *xd = xv.doubleData();
    double *pd = pv.doubleDataMut();
    // C = 2/√3 · π^(-1/4)
    const double C = (2.0 / std::sqrt(3.0)) * std::pow(M_PI, -0.25);
    for (size_t i = 0; i < N; ++i) {
        const double t = xd[i];
        const double t2 = t * t;
        pd[i] = C * (1.0 - t2) * std::exp(-t2 / 2.0);
    }
    return {std::move(pv), std::move(xv)};
}

std::tuple<Value, Value>
morlet(std::pmr::memory_resource *mr, double lb, double ub, size_t N)
{
    Value xv = linspace_row(mr, lb, ub, N);
    Value pv = Value::matrix(1, N, ValueType::DOUBLE, mr);
    if (N == 0) return {std::move(pv), std::move(xv)};
    const double *xd = xv.doubleData();
    double *pd = pv.doubleDataMut();
    for (size_t i = 0; i < N; ++i) {
        const double t = xd[i];
        pd[i] = std::exp(-t * t / 2.0) * std::cos(5.0 * t);
    }
    return {std::move(pv), std::move(xv)};
}

Value meyeraux(std::pmr::memory_resource *mr, const Value &x)
{
    const auto poly = [](double v) {
        // 35v⁴ - 84v⁵ + 70v⁶ - 20v⁷, factored via Horner on v.
        const double v2 = v * v;
        const double v3 = v2 * v;
        const double v4 = v2 * v2;
        return v4 * (35.0 - 84.0 * v + 70.0 * v2 - 20.0 * v3);
    };
    if (x.isScalar()) return Value::scalar(poly(x.toScalar()), mr);
    const auto &d = x.dims();
    Value out;
    if (d.is3D()) out = Value::matrix3d(d.rows(), d.cols(), d.pages(), ValueType::DOUBLE, mr);
    else          out = Value::matrix(d.rows(), d.cols(), ValueType::DOUBLE, mr);
    const size_t N = x.numel();
    if (N == 0) return out;
    double *od = out.doubleDataMut();
    for (size_t i = 0; i < N; ++i) od[i] = poly(x.elemAsDouble(i));
    return out;
}

// ════════════════════════════════════════════════════════════════════
// Engine adapters
// ════════════════════════════════════════════════════════════════════

namespace detail {

static void shape_grid_reg(const char *fn,
                           std::tuple<Value, Value> (*impl)(
                               std::pmr::memory_resource *, double, double, size_t),
                           Span<const Value> args, size_t nargout,
                           Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error(std::string(fn) + ": requires (LB, UB, N)",
                    0, 0, fn, "", "m:wav:nargin");
    const double lb = args[0].toScalar();
    const double ub = args[1].toScalar();
    const double Nd = args[2].toScalar();
    if (!(Nd >= 0.0))
        throw Error(std::string(fn) + ": N must be non-negative",
                    0, 0, fn, "", "m:wav:N");
    const size_t N = static_cast<size_t>(Nd);
    auto [psi, x] = impl(ctx.engine->resource(), lb, ub, N);
    outs[0] = std::move(psi);
    if (nargout > 1) outs[1] = std::move(x);
}

void mexihat_reg(Span<const Value> args, size_t nargout,
                 Span<Value> outs, CallContext &ctx)
{
    shape_grid_reg("mexihat", &mexihat, args, nargout, outs, ctx);
}

void morlet_reg(Span<const Value> args, size_t nargout,
                Span<Value> outs, CallContext &ctx)
{
    shape_grid_reg("morlet", &morlet, args, nargout, outs, ctx);
}

void meyeraux_reg(Span<const Value> args, size_t /*nargout*/,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("meyeraux: requires x",
                    0, 0, "meyeraux", "", "m:meyeraux:nargin");
    outs[0] = meyeraux(ctx.engine->resource(), args[0]);
}

} // namespace detail
} // namespace numkit::wavelet
