// toolboxes/signal/src/math/interp/interp_reg.cpp
//
// CallContext register half of math/interp/interp.cpp (Phase 2b compute/register split).
// Engine-coupled glue: marshals CallContext args/outs into the engine-free
// compute API declared in the headers below. See project_layering_refactor.
#include <numkit/core/engine.hpp>
#include <numkit/builtin/library.hpp>
#include <numkit/math/interp/interp.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/value.hpp>
#include "helpers.hpp"
#include "interp/interp_detail.hpp"
#include <numkit/value/error.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/span.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <complex>
#include <cstddef>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace numkit::builtin {
using namespace numkit::math;  // C4c localized (umbrella removed)

namespace detail {

void interp1_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("interp1: requires at least 3 arguments",
                     0, 0, "interp1", "", "numkit:interp1:nargin");
    // Method may be a char ('linear') OR a string ("linear") — MATLAB
    // accepts both. Previously only isChar() was honored, so a double-quoted
    // method was silently ignored and fell back to linear.
    std::string method = "linear";
    if (args.size() >= 4 && (args[3].isChar() || args[3].isString()))
        method = args[3].toString();

    // 5th arg = extrapolation spec: the literal 'extrap'/"extrap"
    // (extrapolate using the method) or a numeric extrapval (fill
    // out-of-range with it).
    Interp1Extrap mode = Interp1Extrap::Default;
    double fill = std::numeric_limits<double>::quiet_NaN();
    if (args.size() >= 5) {
        const Value &e = args[4];
        if (e.isChar() || e.isString()) {
            std::string es = e.toString();
            for (char &c : es) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if (es == "extrap")
                mode = Interp1Extrap::Method;
            else
                throw Error("interp1: unknown extrapolation option '" + e.toString() + "'",
                             0, 0, "interp1", "", "numkit:interp1:badExtrap");
        } else {
            mode = Interp1Extrap::Const;
            fill = e.toScalar();
        }
    }

    outs[0] = interp1Dispatch(args[0], args[1], args[2], method, mode, fill,
                              ctx.engine->resource());
}

// 2-arg `spline(x, y)` returns a pp struct (piecewise polynomial form)
// usable with `ppval`. Coefficients are derived from the natural cubic
// spline's second-derivative form via the standard transformation
// (see comment block in implementation). See BUGS.md #22.
namespace {

Value splinePp(const Value &x, const Value &y, std::pmr::memory_resource *mr)
{
    const size_t n = x.numel();
    if (n != y.numel())
        throw Error("spline: x and y must have same length",
                     0, 0, "spline", "", "numkit:spline:lengthMismatch");
    if (n < 2)
        throw Error("spline: need at least 2 data points",
                     0, 0, "spline", "", "numkit:spline:tooFewPoints");

    ScratchArena scratch(mr);
    const double *xd = x.doubleData();
    const double *yd = y.doubleData();

    // Reuse interpSpline's not-a-knot sigma helper. See BUGS.md #22.
    const size_t nm1 = n - 1;
    ScratchVec<double> h(nm1, &scratch);
    for (size_t i = 0; i < nm1; ++i) h[i] = xd[i + 1] - xd[i];
    auto sigma = computeSplineSigma(xd, yd, n, h.data(), &scratch);

    // Build [nm1 x 4] coefficient matrix in column-major order.
    // For each interval i, with dx = x - xd[i] in [0, h_i]:
    //   y(dx) = a*dx^3 + b*dx^2 + c*dx + d
    //   a = (sigma_{i+1} - sigma_i) / (6 * h_i)
    //   b = sigma_i / 2
    //   c = (y_{i+1} - y_i) / h_i - h_i * (2*sigma_i + sigma_{i+1}) / 6
    //   d = y_i
    auto coefs = Value::matrix(nm1, 4, ValueType::DOUBLE, mr);
    double *cp = coefs.doubleDataMut();
    for (size_t i = 0; i < nm1; ++i) {
        const double hi = h[i];
        const double a  = (sigma[i + 1] - sigma[i]) / (6.0 * hi);
        const double b  = sigma[i] / 2.0;
        const double c  = (yd[i + 1] - yd[i]) / hi
                          - hi * (2.0 * sigma[i] + sigma[i + 1]) / 6.0;
        const double d  = yd[i];
        cp[i + 0 * nm1] = a;   // col 0
        cp[i + 1 * nm1] = b;
        cp[i + 2 * nm1] = c;
        cp[i + 3 * nm1] = d;
    }
    return mkpp(x, coefs, mr);
}

// 2-arg `pchip(x, y)` returns a pp struct (piecewise polynomial form)
// usable with `ppval`, mirroring spline(x, y). Uses the same shape-
// preserving derivatives as the value-form interpPchip, then converts the
// cubic Hermite segments to MATLAB's [pieces x 4] coefficient layout in
// powers of dx = x - breaks(i):  a*dx^3 + b*dx^2 + c*dx + d with
//   a = (d_i + d_{i+1} - 2*delta_i) / h_i^2
//   b = (3*delta_i - 2*d_i - d_{i+1}) / h_i
//   c = d_i,  d = y_i        (delta_i = (y_{i+1}-y_i)/h_i)
Value pchipPp(const Value &x, const Value &y, std::pmr::memory_resource *mr)
{
    const size_t n = x.numel();
    if (n != y.numel())
        throw Error("pchip: x and y must have same length",
                     0, 0, "pchip", "", "numkit:pchip:lengthMismatch");
    if (n < 2)
        throw Error("pchip: need at least 2 data points",
                     0, 0, "pchip", "", "numkit:pchip:tooFewPoints");

    ScratchArena scratch(mr);
    const double *xd = x.doubleData();
    const double *yd = y.doubleData();
    const size_t nm1 = n - 1;

    ScratchVec<double> h(nm1, &scratch), delta(nm1, &scratch);
    for (size_t i = 0; i < nm1; ++i) {
        h[i] = xd[i + 1] - xd[i];
        delta[i] = (yd[i + 1] - yd[i]) / h[i];
    }

    // Shape-preserving slopes d[0..n-1] (identical to interpPchip).
    ScratchVec<double> d(n, 0.0, &scratch);
    if (n == 2) {
        d[0] = delta[0];
        d[1] = delta[0];                       // 2 points → a straight line
    } else {
        for (size_t i = 1; i < nm1; ++i) {
            if (delta[i - 1] * delta[i] <= 0.0) {
                d[i] = 0.0;
            } else {
                const double w1 = 2.0 * h[i] + h[i - 1];
                const double w2 = h[i] + 2.0 * h[i - 1];
                d[i] = (w1 + w2) / (w1 / delta[i - 1] + w2 / delta[i]);
            }
        }
        d[0] = ((2.0 * h[0] + h[1]) * delta[0] - h[0] * delta[1]) / (h[0] + h[1]);
        if (d[0] * delta[0] < 0.0)
            d[0] = 0.0;
        else if (delta[0] * delta[1] < 0.0 && std::abs(d[0]) > std::abs(3.0 * delta[0]))
            d[0] = 3.0 * delta[0];
        d[nm1] = ((2.0 * h[nm1 - 1] + h[nm1 - 2]) * delta[nm1 - 1]
                  - h[nm1 - 1] * delta[nm1 - 2]) / (h[nm1 - 1] + h[nm1 - 2]);
        if (d[nm1] * delta[nm1 - 1] < 0.0)
            d[nm1] = 0.0;
        else if (delta[nm1 - 2] * delta[nm1 - 1] < 0.0
                 && std::abs(d[nm1]) > std::abs(3.0 * delta[nm1 - 1]))
            d[nm1] = 3.0 * delta[nm1 - 1];
    }

    auto coefs = Value::matrix(nm1, 4, ValueType::DOUBLE, mr);
    double *cp = coefs.doubleDataMut();
    for (size_t i = 0; i < nm1; ++i) {
        const double hi = h[i];
        const double a  = (d[i] + d[i + 1] - 2.0 * delta[i]) / (hi * hi);
        const double b  = (3.0 * delta[i] - 2.0 * d[i] - d[i + 1]) / hi;
        const double c  = d[i];
        const double dd = yd[i];
        cp[i + 0 * nm1] = a;
        cp[i + 1 * nm1] = b;
        cp[i + 2 * nm1] = c;
        cp[i + 3 * nm1] = dd;
    }
    return mkpp(x, coefs, mr);
}

// 2-arg `makima(x, y)` returns a pp struct, mirroring spline/pchip. Uses
// the same modified-Akima derivatives as the value-form interpMakima,
// then the identical cubic-Hermite → dx-power coefficient conversion as
// pchipPp (makima and pchip share the Hermite basis; only the slopes d_i
// differ).
Value makimaPp(const Value &x, const Value &y, std::pmr::memory_resource *mr)
{
    const size_t n = x.numel();
    if (n != y.numel())
        throw Error("makima: x and y must have same length",
                     0, 0, "makima", "", "numkit:makima:lengthMismatch");
    if (n < 2)
        throw Error("makima: need at least 2 data points",
                     0, 0, "makima", "", "numkit:makima:tooFewPoints");

    ScratchArena scratch(mr);
    const double *xd = x.doubleData();
    const double *yd = y.doubleData();
    const size_t nm1 = n - 1;

    ScratchVec<double> h(nm1, &scratch), delta(nm1, &scratch);
    for (size_t i = 0; i < nm1; ++i) {
        h[i] = xd[i + 1] - xd[i];
        delta[i] = (yd[i + 1] - yd[i]) / h[i];
    }

    ScratchVec<double> d(n, 0.0, &scratch);
    if (n == 2) {
        d[0] = delta[0];
        d[1] = delta[0];                       // 2 points → a straight line
    } else {
        // Slopes m[-2..n] with Akima's quadratic extrapolation, offset 2.
        ScratchVec<double> mExt(n + 3, &scratch);
        for (size_t i = 0; i < nm1; ++i) mExt[2 + i] = delta[i];
        mExt[1] = 2.0 * mExt[2] - mExt[3];
        mExt[0] = 2.0 * mExt[1] - mExt[2];
        mExt[2 + nm1]     = 2.0 * mExt[2 + nm1 - 1] - mExt[2 + nm1 - 2];
        mExt[2 + nm1 + 1] = 2.0 * mExt[2 + nm1]     - mExt[2 + nm1 - 1];
        for (size_t i = 0; i < n; ++i) {
            const double ml2 = mExt[i];
            const double ml1 = mExt[i + 1];
            const double mr1 = mExt[i + 2];
            const double mr2 = mExt[i + 3];
            const double w1 = std::abs(mr2 - mr1) + std::abs(mr2 + mr1) * 0.5;
            const double w2 = std::abs(ml1 - ml2) + std::abs(ml1 + ml2) * 0.5;
            const double wsum = w1 + w2;
            d[i] = (wsum == 0.0) ? 0.0 : (w1 * ml1 + w2 * mr1) / wsum;
        }
    }

    auto coefs = Value::matrix(nm1, 4, ValueType::DOUBLE, mr);
    double *cp = coefs.doubleDataMut();
    for (size_t i = 0; i < nm1; ++i) {
        const double hi = h[i];
        const double a  = (d[i] + d[i + 1] - 2.0 * delta[i]) / (hi * hi);
        const double b  = (3.0 * delta[i] - 2.0 * d[i] - d[i + 1]) / hi;
        const double c  = d[i];
        const double dd = yd[i];
        cp[i + 0 * nm1] = a;
        cp[i + 1 * nm1] = b;
        cp[i + 2 * nm1] = c;
        cp[i + 3 * nm1] = dd;
    }
    return mkpp(x, coefs, mr);
}

} // namespace

void spline_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    auto *mr = ctx.engine->resource();
    if (args.size() == 2) {
        // pp-struct form. See BUGS.md #22.
        outs[0] = splinePp(args[0], args[1], mr);
        return;
    }
    if (args.size() < 3)
        throw Error("spline: requires (x, y) or (x, y, xq)",
                     0, 0, "spline", "", "numkit:spline:nargin");
    outs[0] = spline(args[0], args[1], args[2], mr);
}

void interp2_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("interp2: requires at least 3 arguments",
                     0, 0, "interp2", "", "numkit:interp2:nargin");
    std::pmr::memory_resource *mr = ctx.engine->resource();
    auto isMethodArg = [](const Value &v) {
        return v.isChar() || v.isString();
    };
    // Form A: interp2(V, Xq, Yq[, method]) — first arg is the matrix.
    // Form B: interp2(X, Y, V, Xq, Yq[, method]) — 5 or 6 numeric args.
    if (args.size() == 3 || (args.size() == 4 && isMethodArg(args[3]))) {
        std::string method = "linear";
        if (args.size() == 4) method = args[3].toString();
        outs[0] = interp2(args[0], args[1], args[2], method, mr);
        return;
    }
    if (args.size() == 5 || (args.size() == 6 && isMethodArg(args[5]))) {
        std::string method = "linear";
        if (args.size() == 6) method = args[5].toString();
        outs[0] = interp2(args[0], args[1], args[2], args[3], args[4], method, mr);
        return;
    }
    throw Error("interp2: invalid argument count or types",
                 0, 0, "interp2", "", "numkit:interp2:nargin");
}

void interp3_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 4)
        throw Error("interp3: requires at least 4 arguments",
                     0, 0, "interp3", "", "numkit:interp3:nargin");
    std::pmr::memory_resource *mr = ctx.engine->resource();
    auto isMethodArg = [](const Value &v) {
        return v.isChar() || v.isString();
    };
    // Form A: interp3(V, Xq, Yq, Zq[, method]).
    if (args.size() == 4 || (args.size() == 5 && isMethodArg(args[4]))) {
        std::string method = "linear";
        if (args.size() == 5) method = args[4].toString();
        outs[0] = interp3(args[0], args[1], args[2], args[3], method, mr);
        return;
    }
    // Form B: interp3(X, Y, Z, V, Xq, Yq, Zq[, method]) — 7 or 8 args.
    if (args.size() == 7 || (args.size() == 8 && isMethodArg(args[7]))) {
        std::string method = "linear";
        if (args.size() == 8) method = args[7].toString();
        outs[0] = interp3(args[0], args[1], args[2], args[3], args[4], args[5], args[6], method, mr);
        return;
    }
    throw Error("interp3: invalid argument count or types",
                 0, 0, "interp3", "", "numkit:interp3:nargin");
}

void pchip_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    auto *mr = ctx.engine->resource();
    if (args.size() == 2) {
        // pp-struct form, mirroring spline(x, y).
        outs[0] = pchipPp(args[0], args[1], mr);
        return;
    }
    if (args.size() < 3)
        throw Error("pchip: requires (x, y) or (x, y, xq)",
                     0, 0, "pchip", "", "numkit:pchip:nargin");
    outs[0] = pchip(args[0], args[1], args[2], mr);
}

void makima_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    auto *mr = ctx.engine->resource();
    if (args.size() == 2) {
        // pp-struct form, mirroring spline(x, y) and pchip(x, y).
        outs[0] = makimaPp(args[0], args[1], mr);
        return;
    }
    if (args.size() < 3)
        throw Error("makima: requires (x, y) or (x, y, xq)",
                     0, 0, "makima", "", "numkit:makima:nargin");
    outs[0] = makima(args[0], args[1], args[2], mr);
}

// interpn — dispatch to interp2 / interp3 based on V's ndim. Form A
// (V, Xq1..XqN[, method]) inspects args[0]; Form B
// (X1..XN, V, Xq1..XqN[, method]) follows the same dispatch pattern
// because V always lives at args[0] in Form A and the implementation
// distinguishes them inside interp2_reg / interp3_reg.
void interpn_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("interpn: requires at least 2 arguments",
                     0, 0, "interpn", "", "numkit:interpn:nargin");
    const auto &V0 = args[0];
    const int ndV = V0.dims().is3D() ? 3
                  : (V0.dims().ndim() <= 2 ? 2 : V0.dims().ndim());
    if (ndV == 2) {
        interp2_reg(args, nargout, outs, ctx);
        return;
    }
    if (ndV == 3) {
        interp3_reg(args, nargout, outs, ctx);
        return;
    }
    throw Error("interpn: 4+-D inputs are not yet supported",
                 0, 0, "interpn", "", "numkit:interpn:rank");
}

// polyfit_reg / polyval_reg → math/elementary/polynomials.cpp
// trapz_reg                 → math/integration/integration.cpp

void mkpp_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("mkpp: requires (breaks, coefs)",
                     0, 0, "mkpp", "", "numkit:mkpp:nargin");
    outs[0] = mkpp(args[0], args[1], ctx.engine->resource());
}

void ppval_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("ppval: requires (pp, x)",
                     0, 0, "ppval", "", "numkit:ppval:nargin");
    outs[0] = ppval(args[0], args[1], ctx.engine->resource());
}

void unmkpp_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("unmkpp: requires 1 argument",
                     0, 0, "unmkpp", "", "numkit:unmkpp:nargin");
    const Value &pp = args[0];
    if (!pp.isStruct() || !pp.hasField("breaks") || !pp.hasField("coefs"))
        throw Error("unmkpp: input must be a pp struct",
                     0, 0, "unmkpp", "", "numkit:unmkpp:notPp");
    outs[0] = pp.field("breaks");
    if (nargout > 1) outs[1] = pp.field("coefs");
    if (nargout > 2) outs[2] = pp.hasField("pieces") ? pp.field("pieces")
                                                     : Value::scalar(0.0, ctx.engine->resource());
    if (nargout > 3) outs[3] = pp.hasField("order")  ? pp.field("order")
                                                     : Value::scalar(0.0, ctx.engine->resource());
    if (nargout > 4) outs[4] = pp.hasField("dim")    ? pp.field("dim")
                                                     : Value::scalar(1.0, ctx.engine->resource());
}

} // namespace detail

} // namespace numkit::builtin
