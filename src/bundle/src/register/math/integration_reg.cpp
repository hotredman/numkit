// bundle/src/register/math/integration_reg.cpp
//
// Engine adapters + the embedded-.m `integral` registration for the math
// integration compute. Relocated from runtime in Phase E: the pure calculus +
// Gauss-Kronrod compute now lives core-free in numkit::math (integration.cpp);
// this Engine-coupled glue stays in bundle. The adapter functions keep the
// numkit::builtin::detail namespace (incidental registration glue, matching
// every other bundle *_reg, and the forward-decls in builtin_library.cpp).

#include <numkit/math/integration/integration.hpp>
#include <numkit/math/integration/integration_detail.hpp>
#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>
#include <numkit/ops/helpers.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/value.hpp>
#include <numkit/value/scratch.hpp>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <string>

namespace numkit::builtin {
// ── Engine adapters ──────────────────────────────────────────────────
namespace detail {
// Bring the relocated math compute (numkit::math, integration.cpp)
// into adapter scope so the bodies below stay verbatim.
using ::numkit::math::gradient;     using ::numkit::math::gradient2;
using ::numkit::math::del2;         using ::numkit::math::cumtrapz;
using ::numkit::math::cumtrapzDim;
using ::numkit::math::integral;     using ::numkit::math::trapz;
using ::numkit::math::integral2;    using ::numkit::math::integral3;
using ::numkit::math::detail::gradientND;
using ::numkit::math::detail::toDoubleCopy;
using ::numkit::math::detail::cumtrapzMatrixRows;
using ::numkit::math::detail::cumtrapzMatrixRowsC;
using ::numkit::math::detail::cumtrapzMatrixCols;
using ::numkit::math::detail::cumtrapzMatrixColsC;
using ::numkit::math::detail::trapzImpl;
using ::numkit::math::detail::kKronrodX;
using ::numkit::math::detail::kKronrodW;
using ::numkit::math::detail::kGaussW;

void gradient_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("gradient: requires at least 1 argument",
                     0, 0, "gradient", "", "numkit:gradient:nargin");
    std::pmr::memory_resource *mr = ctx.engine->resource();
    const Dims &shape = args[0].dims();
    const bool isND = shape.is3D() || shape.ndim() > 2;

    if (!isND) {
        // Vector / 2-D matrix — unchanged fast paths.
        double hx = 1.0, hy = 1.0;
        if (args.size() >= 2) hx = args[1].toScalar();
        if (args.size() >= 3) hy = args[2].toScalar();
        else                  hy = hx;  // single spacing applies to both axes

        if (nargout <= 1) {
            outs[0] = gradient(args[0], hx, mr);
            return;
        }
        auto [fx, fy] = gradient2(args[0], hx, hy, mr);
        outs[0] = std::move(fx);
        outs[1] = std::move(fy);
        return;
    }

    // N-D (3-D+) array: one gradient per dimension, up to nargout.
    const size_t nout = (nargout < 1) ? 1 : nargout;
    ScratchArena arena(mr);
    auto hs = ScratchVec<double>(&arena);
    const size_t nspac = (args.size() >= 2) ? args.size() - 1 : 0;
    if (nspac == 0) {
        hs.assign(nout, 1.0);
    } else if (nspac == 1) {
        hs.assign(nout, args[1].toScalar());  // single spacing → every dim
    } else {
        for (size_t i = 1; i < args.size(); ++i) hs.push_back(args[i].toScalar());
        while (hs.size() < nout) hs.push_back(1.0);
    }
    auto results = gradientND(args[0], hs.data(), hs.size(), nout, mr);
    for (size_t o = 0; o < nout && o < outs.size(); ++o)
        outs[o] = std::move(results[o]);
}

void del2_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("del2: requires at least 1 argument",
                     0, 0, "del2", "", "numkit:del2:nargin");
    std::pmr::memory_resource *mr = ctx.engine->resource();
    double h = 1.0;
    if (args.size() >= 2) {
        if (!args[1].isScalar())
            throw Error("del2: coordinate-vector spacing is not supported in "
                        "this revision (use a scalar spacing h)",
                        0, 0, "del2", "", "numkit:del2:spacing");
        h = args[1].toScalar();
    }
    if (args.size() >= 3)
        throw Error("del2: per-axis spacing (hx, hy) is not supported in this "
                    "revision (use a single scalar h)",
                    0, 0, "del2", "", "numkit:del2:spacing");
    outs[0] = del2(args[0], h, mr);
}

void cumtrapz_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("cumtrapz: requires at least 1 argument",
                     0, 0, "cumtrapz", "", "numkit:cumtrapz:nargin");
    std::pmr::memory_resource *mr = ctx.engine->resource();
    if (args.size() == 1) {
        outs[0] = cumtrapz(args[0], mr);
        return;
    }
    if (args.size() == 2) {
        // A scalar 2nd arg is always the dim → cumtrapz(Y, dim).
        // Otherwise it is X in cumtrapz(X, Y) (X is a coordinate vector
        // or same-size matrix).
        if (args[1].isScalar()) {
            outs[0] = cumtrapzDim(args[0], static_cast<int>(args[1].toScalar()), mr);
            return;
        }
        outs[0] = cumtrapz(args[0], args[1], mr);
        return;
    }
    // cumtrapz(X, Y, dim): X is a coordinate vector of length size(Y,dim)
    // (MATLAB), broadcast across the other dimension. numkit also accepts
    // an X matrix the same size as Y (per-element spacing — extension).
    const int dim = static_cast<int>(args[2].toScalar());
    const Value &x = args[0], &y = args[1];
    if (x.type() == ValueType::COMPLEX)
        throw Error("cumtrapz: the X coordinate must be real",
                     0, 0, "cumtrapz", "", "numkit:cumtrapz:complexX");
    const bool yIsC = (y.type() == ValueType::COMPLEX);
    const size_t rows = y.dims().rows(), cols = y.dims().cols();
    Value ys;
    if (!yIsC) ys = toDoubleCopy(y, mr);
    auto xs = toDoubleCopy(x, mr);
    const double *xsrc = xs.doubleData();

    auto xMat = Value::matrix(rows, cols, ValueType::DOUBLE, mr);
    double *dx = xMat.doubleDataMut();
    const bool xIsVec = x.dims().isVector() || x.isScalar();
    if (x.dims().rows() == rows && x.dims().cols() == cols) {
        std::memcpy(dx, xsrc, rows * cols * sizeof(double));   // per-element
    } else if (xIsVec && dim == 2 && x.numel() == cols) {
        for (size_t c = 0; c < cols; ++c)
            for (size_t r = 0; r < rows; ++r) dx[c * rows + r] = xsrc[c];
    } else if (xIsVec && dim != 2 && x.numel() == rows) {
        for (size_t c = 0; c < cols; ++c)
            for (size_t r = 0; r < rows; ++r) dx[c * rows + r] = xsrc[r];
    } else {
        throw Error("cumtrapz: X must be a vector of length size(Y,dim)",
                     0, 0, "cumtrapz", "", "numkit:cumtrapz:shapeMismatch");
    }
    if (dim == 2)
        outs[0] = yIsC ? cumtrapzMatrixRowsC(y.complexData(), dx, rows, cols, mr)
                       : cumtrapzMatrixRows(ys.doubleData(), dx, rows, cols, mr);
    else
        outs[0] = yIsC ? cumtrapzMatrixColsC(y.complexData(), dx, rows, cols, mr)
                       : cumtrapzMatrixCols(ys.doubleData(), dx, rows, cols, mr);
}

// C++ primitive for the embedded-.m integral: returns the Gauss-Kronrod-15
// abscissae + weights (NO f-calls). The .m wrapper does the f-evaluations and
// the adaptive recursion, so the integrand is pausable. xk/wk: 15-vectors,
// wg: the 7 Gauss weights. (VM_CALLBACKS_PLAN.md)
void gk15_nodes_reg(Span<const Value> /*args*/, size_t /*nargout*/, Span<Value> outs,
                    CallContext &ctx)
{
    auto *mr = ctx.engine->resource();
    Value xk = Value::matrix(1, 15, ValueType::DOUBLE, mr);
    Value wk = Value::matrix(1, 15, ValueType::DOUBLE, mr);
    Value wg = Value::matrix(1, 7, ValueType::DOUBLE, mr);
    for (int i = 0; i < 15; ++i) {
        xk.doubleDataMut()[i] = kKronrodX[i];
        wk.doubleDataMut()[i] = kKronrodW[i];
    }
    for (int i = 0; i < 7; ++i)
        wg.doubleDataMut()[i] = kGaussW[i];
    if (outs.size() >= 1) outs[0] = std::move(xk);
    if (outs.size() >= 2) outs[1] = std::move(wk);
    if (outs.size() >= 3) outs[2] = std::move(wg);
}

void integral_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("integral: requires at least 3 arguments (fn, a, b)",
                     0, 0, "integral", "", "numkit:integral:nargin");
    if (!args[0].isFuncHandle()
        && !(args[0].isCell() && args[0].numel() >= 1
             && args[0].cellAt(0).isFuncHandle()))
        throw Error("integral: 1st argument must be a function handle",
                     0, 0, "integral", "", "numkit:integral:fnType");
    const double a = args[1].toScalar();
    const double b = args[2].toScalar();
    double absTol = 1e-10;
    for (size_t i = 3; i + 1 < args.size(); i += 2) {
        if (!args[i].isChar() && !args[i].isString())
            throw Error("integral: expected option name (string)",
                         0, 0, "integral", "", "numkit:integral:badFlag");
        std::string key = args[i].toString();
        for (auto &c : key) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (key == "abstol") {
            absTol = args[i + 1].toScalar();
        } else {
            throw Error("integral: unsupported option '" + key + "'",
                         0, 0, "integral", "", "numkit:integral:badFlag");
        }
    }
    auto handle = args[0];
    auto cb = [&ctx, &handle](Span<const Value> ar, Span<Value> ou,
                               std::pmr::memory_resource * /*mr*/) {
        auto r = ctx.engine->callFunctionHandleMulti(handle, ar, ou.size());
        for (size_t i = 0; i < ou.size() && i < r.size(); ++i)
            ou[i] = std::move(r[i]);
    };
    outs[0] = integral(cb, a, b, absTol, ctx.engine->resource());
}

static double integral2AbsTol(Span<const Value> args, size_t firstOpt)
{
    double absTol = 1e-10;
    for (size_t i = firstOpt; i + 1 < args.size(); i += 2) {
        std::string key = args[i].toString();
        for (auto &c : key) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (key == "abstol") absTol = args[i + 1].toScalar();
    }
    return absTol;
}

void integral2_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 5)
        throw Error("integral2: requires (fn, a, b, c, d)",
                     0, 0, "integral2", "", "numkit:integral2:nargin");
    if (!args[0].isFuncHandle())
        throw Error("integral2: 1st argument must be a function handle",
                     0, 0, "integral2", "", "numkit:integral2:fnType");
    const double a = args[1].toScalar(), b = args[2].toScalar();
    const double c = args[3].toScalar(), d = args[4].toScalar();
    const double absTol = integral2AbsTol(args, 5);
    auto handle = args[0];
    auto cb = [&ctx, &handle](Span<const Value> ar, Span<Value> ou,
                               std::pmr::memory_resource * /*mr*/) {
        auto r = ctx.engine->callFunctionHandleMulti(handle, ar, ou.size());
        for (size_t i = 0; i < ou.size() && i < r.size(); ++i)
            ou[i] = std::move(r[i]);
    };
    outs[0] = integral2(cb, a, b, c, d, absTol, ctx.engine->resource());
}

void integral3_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 7)
        throw Error("integral3: requires (fn, a, b, c, d, e, f)",
                     0, 0, "integral3", "", "numkit:integral3:nargin");
    if (!args[0].isFuncHandle())
        throw Error("integral3: 1st argument must be a function handle",
                     0, 0, "integral3", "", "numkit:integral3:fnType");
    const double a = args[1].toScalar(), b = args[2].toScalar();
    const double c = args[3].toScalar(), d = args[4].toScalar();
    const double e = args[5].toScalar(), f = args[6].toScalar();
    const double absTol = integral2AbsTol(args, 7);
    auto handle = args[0];
    auto cb = [&ctx, &handle](Span<const Value> ar, Span<Value> ou,
                               std::pmr::memory_resource * /*mr*/) {
        auto r = ctx.engine->callFunctionHandleMulti(handle, ar, ou.size());
        for (size_t i = 0; i < ou.size() && i < r.size(); ++i)
            ou[i] = std::move(r[i]);
    };
    outs[0] = integral3(cb, a, b, c, d, e, f, absTol, ctx.engine->resource());
}

// quadgk = adaptive Gauss-Kronrod 1-D quadrature = the existing `integral`.
// [q, errbnd] = quadgk(...); we return q and the requested tolerance as a
// (conservative) error-bound placeholder.
void quadgk_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("quadgk: requires (fn, a, b)",
                     0, 0, "quadgk", "", "numkit:quadgk:nargin");
    if (!args[0].isFuncHandle())
        throw Error("quadgk: 1st argument must be a function handle",
                     0, 0, "quadgk", "", "numkit:quadgk:fnType");
    const double a = args[1].toScalar(), b = args[2].toScalar();
    const double absTol = integral2AbsTol(args, 3);
    auto handle = args[0];
    auto cb = [&ctx, &handle](Span<const Value> ar, Span<Value> ou,
                               std::pmr::memory_resource * /*mr*/) {
        auto r = ctx.engine->callFunctionHandleMulti(handle, ar, ou.size());
        for (size_t i = 0; i < ou.size() && i < r.size(); ++i)
            ou[i] = std::move(r[i]);
    };
    outs[0] = integral(cb, a, b, absTol, ctx.engine->resource());
    if (outs.size() >= 2) outs[1] = Value::scalar(absTol, ctx.engine->resource());
}

// quad2d = the older name for a rectangular double integral = integral2.
void quad2d_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 5)
        throw Error("quad2d: requires (fn, a, b, c, d)",
                     0, 0, "quad2d", "", "numkit:quad2d:nargin");
    if (!args[0].isFuncHandle())
        throw Error("quad2d: 1st argument must be a function handle",
                     0, 0, "quad2d", "", "numkit:quad2d:fnType");
    const double a = args[1].toScalar(), b = args[2].toScalar();
    const double c = args[3].toScalar(), d = args[4].toScalar();
    const double absTol = integral2AbsTol(args, 5);
    auto handle = args[0];
    auto cb = [&ctx, &handle](Span<const Value> ar, Span<Value> ou,
                               std::pmr::memory_resource * /*mr*/) {
        auto r = ctx.engine->callFunctionHandleMulti(handle, ar, ou.size());
        for (size_t i = 0; i < ou.size() && i < r.size(); ++i)
            ou[i] = std::move(r[i]);
    };
    outs[0] = integral2(cb, a, b, c, d, absTol, ctx.engine->resource());
}

void trapz_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("trapz: requires at least 1 argument",
                     0, 0, "trapz", "", "numkit:trapz:nargin");
    auto *mr = ctx.engine->resource();
    // MATLAB promotes a logical X and/or Y to double (trapz(logical([1 0 1 1]))
    // = 2, class double — bugs/builtin/trapz-logical.md). Only logical is
    // coerced; the doubleData() paths below still throw for char / other
    // non-numeric, which matches MATLAB ("numeric or logical" only). The
    // sibling cumtrapz already handles this via its toDoubleCopy reader.
    Value a0 = args[0].isLogical() ? toDoubleValue(args[0], mr) : args[0];
    if (args.size() == 1) {
        outs[0] = trapz(a0, mr);
        return;
    }
    Value a1 = args[1].isLogical() ? toDoubleValue(args[1], mr) : args[1];
    if (args.size() == 2) {
        // trapz(Y, dim) when the 2nd arg is a scalar; trapz(X, Y) otherwise.
        if (!a1.isChar() && !a1.isString() && a1.numel() == 1)
            outs[0] = trapzImpl(a0, static_cast<int>(a1.toScalar()),
                                nullptr, mr);
        else
            outs[0] = trapz(a0, a1, mr);
        return;
    }
    // trapz(X, Y, dim): integrate Y along dim with X spacing.
    const int dim = static_cast<int>(args[2].toScalar());
    const size_t along = (dim != 2) ? a1.dims().rows() : a1.dims().cols();
    if (a0.numel() != along)
        throw Error("trapz: numel(x) must match size(y, dim)",
                     0, 0, "trapz", "", "numkit:trapz:lengthMismatch");
    outs[0] = trapzImpl(a1, dim, a0.doubleData(), mr);
}

} // namespace detail

// ── integral as an embedded `.m` wrapper (VM_CALLBACKS_PLAN.md) ───────────────
// The registered integral is implemented in `.m`: the integrand is evaluated
// from bytecode (pausable), and the adaptive Gauss-Kronrod recursion is the
// natural `.m` recursion. The `__gk15_nodes` C++ primitive supplies the
// abscissae/weights (no f-calls); the per-node accumulation mirrors the C++
// gaussKronrod15 loop exactly (fused K/G in node order) so results are
// bit-identical to the synchronous `Value integral(...)` API, which is retained.
static const char *kIntegralMSource = R"NKM(
function r = integral(fn, a, b, opt, optval)
  if ~(strcmp(class(fn), 'function_handle') || iscell(fn))
    error('numkit:integral:fnType', 'integral: 1st argument must be a function handle');
  end
  absTol = 1e-10;
  if nargin >= 4
    if nargin < 5
      error('numkit:integral:badFlag', 'integral: option name without a value');
    end
    if ~strcmp(lower(opt), 'abstol')
      error('numkit:integral:badFlag', 'integral: unsupported option');
    end
    absTol = optval;
  end
  if ~(isfinite(a) && isfinite(b))
    error('numkit:integral:badBounds', 'integral: bounds must be finite');
  end
  if absTol <= 0
    error('numkit:integral:badTol', 'integral: absTol must be positive');
  end
  sgn = 1;
  if b < a
    t = a; a = b; b = t; sgn = -1;
  end
  if a == b
    r = 0; return;
  end
  [xk, wk, wg] = __gk15_nodes();
  r = sgn * nk_adaptint(fn, a, b, absTol, 0, xk, wk, wg);
end

function [K, G] = nk_gk15(fn, a, b, xk, wk, wg)
  half = 0.5*(b - a);
  mid = 0.5*(b + a);
  K = 0; G = 0; gi = 1;
  for j = 1:15
    fvj = fn(mid + half*xk(j));
    K = K + wk(j)*fvj;
    if mod(j, 2) == 0
      G = G + wg(gi)*fvj;
      gi = gi + 1;
    end
  end
  K = half*K;
  G = half*G;
end

function r = nk_adaptint(fn, a, b, tol, depth, xk, wk, wg)
  [K, G] = nk_gk15(fn, a, b, xk, wk, wg);
  if abs(K - G) < tol || depth >= 20
    r = K;
    return;
  end
  mid = 0.5*(a + b);
  r = nk_adaptint(fn, a, mid, tol*0.5, depth+1, xk, wk, wg) + nk_adaptint(fn, mid, b, tol*0.5, depth+1, xk, wk, wg);
end
)NKM";

void registerIntegralM(Engine &engine)
{
    engine.registerBuiltinMSource(kIntegralMSource);
}

} // namespace numkit::builtin
