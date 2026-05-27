// libs/stats/src/distributions/dist_helpers.hpp
//
// Shared helpers for the distribution-CDF adapters. Currently exposes
// the `'upper'` flag detection that all MATLAB *cdf functions accept:
//
//   p = <dist>cdf(x, ..., 'upper')   →   1 - F(x; ...)
//
// Used by every cdf_reg adapter under libs/stats/src/distributions/.

#pragma once

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>
#include <numkit/core/value.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <string>
#include <tuple>
#include <utility>

namespace numkit::stats::detail {

// If the last positional arg is a string equal to 'upper' (any case),
// strip it and set `upper = true`. Returns the effective trailing-arg
// count. Adapters should consume args[0..count-1] for positional
// parsing and ignore args[count..] (which is at most the 'upper' flag).
inline size_t stripUpperFlag(Span<const Value> args, bool &upper)
{
    upper = false;
    if (args.empty()) return 0;
    const Value &last = args[args.size() - 1];
    if (!last.isChar() && !last.isString()) return args.size();
    std::string s = last.toString();
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    if (s == "upper") {
        upper = true;
        return args.size() - 1;
    }
    return args.size();
}

// Apply the upper-tail transform in place: y = 1 - y for every finite
// element. NaN passes through unchanged.
inline void applyUpperInPlace(Value &y)
{
    if (y.type() != ValueType::DOUBLE) return;
    double *d = y.doubleDataMut();
    const size_t n = y.numel();
    for (size_t i = 0; i < n; ++i) {
        const double v = d[i];
        if (!std::isnan(v)) d[i] = 1.0 - v;
    }
}

// ── *stat vectorisation helpers ───────────────────────────────────────
//
// Every `<dist>stat(args...)` returns `tuple<double, double>` (mean,
// variance). MATLAB vectorises these element-wise with broadcasting
// (equal sizes OR one scalar). The helpers below take a scalar impl
// and apply the broadcasting boilerplate once.
//
// Edge: 0-dim scalar inputs use a fast path (no allocation).

template <class Stat1>
inline void emit_vec_stat_1arg(Span<const Value> args, size_t nargout,
                                Span<Value> outs, CallContext &ctx,
                                const char *fnName, Stat1 stat_impl)
{
    if (args.empty())
        throw Error(std::string(fnName) + ": requires 1 arg",
                    0, 0, fnName, "", "numkit:nargin");
    auto *mr = ctx.engine->resource();
    const Value &p = args[0];
    if (p.isScalar()) {
        auto [m, v] = stat_impl(p.toScalar());
        outs[0] = Value::scalar(m, mr);
        if (nargout > 1) outs[1] = Value::scalar(v, mr);
        return;
    }
    const auto &d = p.dims();
    Value out_m = d.is3D()
        ? Value::matrix3d(d.rows(), d.cols(), d.pages(), ValueType::DOUBLE, mr)
        : Value::matrix(d.rows(), d.cols(), ValueType::DOUBLE, mr);
    Value out_v = d.is3D()
        ? Value::matrix3d(d.rows(), d.cols(), d.pages(), ValueType::DOUBLE, mr)
        : Value::matrix(d.rows(), d.cols(), ValueType::DOUBLE, mr);
    double *pm = out_m.doubleDataMut();
    double *pv = out_v.doubleDataMut();
    const size_t n = p.numel();
    for (size_t i = 0; i < n; ++i) {
        auto [m, v] = stat_impl(p.elemAsDouble(i));
        pm[i] = m;
        pv[i] = v;
    }
    outs[0] = std::move(out_m);
    if (nargout > 1) outs[1] = std::move(out_v);
}

template <class Stat2>
inline void emit_vec_stat_2arg(Span<const Value> args, size_t nargout,
                                Span<Value> outs, CallContext &ctx,
                                const char *fnName, Stat2 stat_impl)
{
    if (args.size() < 2)
        throw Error(std::string(fnName) + ": requires 2 args",
                    0, 0, fnName, "", "numkit:nargin");
    auto *mr = ctx.engine->resource();
    const Value &av = args[0];
    const Value &bv = args[1];
    const size_t na = av.numel();
    const size_t nb = bv.numel();
    if (na == 1 && nb == 1) {
        auto [m, v] = stat_impl(av.toScalar(), bv.toScalar());
        outs[0] = Value::scalar(m, mr);
        if (nargout > 1) outs[1] = Value::scalar(v, mr);
        return;
    }
    if (na > 1 && nb > 1 && na != nb)
        throw Error(std::string(fnName) + ": args must be same size or scalar",
                    0, 0, fnName, "", "numkit:dim");
    const Value &ref = (na >= nb) ? av : bv;
    const auto &d = ref.dims();
    Value out_m = d.is3D()
        ? Value::matrix3d(d.rows(), d.cols(), d.pages(), ValueType::DOUBLE, mr)
        : Value::matrix(d.rows(), d.cols(), ValueType::DOUBLE, mr);
    Value out_v = d.is3D()
        ? Value::matrix3d(d.rows(), d.cols(), d.pages(), ValueType::DOUBLE, mr)
        : Value::matrix(d.rows(), d.cols(), ValueType::DOUBLE, mr);
    double *pm = out_m.doubleDataMut();
    double *pv = out_v.doubleDataMut();
    const size_t n = ref.numel();
    for (size_t i = 0; i < n; ++i) {
        const double a = av.elemAsDouble(na == 1 ? 0 : i);
        const double b = bv.elemAsDouble(nb == 1 ? 0 : i);
        auto [m, v] = stat_impl(a, b);
        pm[i] = m;
        pv[i] = v;
    }
    outs[0] = std::move(out_m);
    if (nargout > 1) outs[1] = std::move(out_v);
}

template <class Stat3>
inline void emit_vec_stat_3arg(Span<const Value> args, size_t nargout,
                                Span<Value> outs, CallContext &ctx,
                                const char *fnName, Stat3 stat_impl)
{
    if (args.size() < 3)
        throw Error(std::string(fnName) + ": requires 3 args",
                    0, 0, fnName, "", "numkit:nargin");
    auto *mr = ctx.engine->resource();
    const Value &av = args[0];
    const Value &bv = args[1];
    const Value &cv = args[2];
    const size_t na = av.numel(), nb = bv.numel(), nc = cv.numel();
    const size_t nmax = std::max({na, nb, nc});
    if (nmax == 1) {
        auto [m, v] = stat_impl(av.toScalar(), bv.toScalar(), cv.toScalar());
        outs[0] = Value::scalar(m, mr);
        if (nargout > 1) outs[1] = Value::scalar(v, mr);
        return;
    }
    auto sizeOK = [&](size_t n){ return n == 1 || n == nmax; };
    if (!sizeOK(na) || !sizeOK(nb) || !sizeOK(nc))
        throw Error(std::string(fnName) + ": args must be same size or scalar",
                    0, 0, fnName, "", "numkit:dim");
    const Value &ref = (na == nmax) ? av : (nb == nmax ? bv : cv);
    const auto &d = ref.dims();
    Value out_m = d.is3D()
        ? Value::matrix3d(d.rows(), d.cols(), d.pages(), ValueType::DOUBLE, mr)
        : Value::matrix(d.rows(), d.cols(), ValueType::DOUBLE, mr);
    Value out_v = d.is3D()
        ? Value::matrix3d(d.rows(), d.cols(), d.pages(), ValueType::DOUBLE, mr)
        : Value::matrix(d.rows(), d.cols(), ValueType::DOUBLE, mr);
    double *pm = out_m.doubleDataMut();
    double *pv = out_v.doubleDataMut();
    for (size_t i = 0; i < nmax; ++i) {
        const double a = av.elemAsDouble(na == 1 ? 0 : i);
        const double b = bv.elemAsDouble(nb == 1 ? 0 : i);
        const double c = cv.elemAsDouble(nc == 1 ? 0 : i);
        auto [m, v] = stat_impl(a, b, c);
        pm[i] = m;
        pv[i] = v;
    }
    outs[0] = std::move(out_m);
    if (nargout > 1) outs[1] = std::move(out_v);
}

// ── RNG size-arg parser ──────────────────────────────────────────────
// Parses MATLAB-style trailing size arguments shared by every *rnd
// distribution adapter:
//
//   <fn>(...params)              → scalar (1×1)
//   <fn>(...params, n)           → n × n
//   <fn>(...params, [m n])       → m × n  (vector size)
//   <fn>(...params, m, n)        → m × n  (multi-arg size)
//
// `start` is the index of the first size-argument candidate (0-based).
// On exit, sets rows/cols. Throws on invalid input.
inline void parse_rng_size(Span<const Value> args, size_t start,
                            size_t &rows, size_t &cols)
{
    rows = 1;
    cols = 1;
    if (args.size() <= start) return;
    const Value &a0 = args[start];
    if (a0.isEmpty()) return;
    // Vector size form: numel > 1.
    if (a0.numel() > 1) {
        const size_t n = a0.numel();
        if (n == 1) {
            rows = static_cast<size_t>(a0.elemAsDouble(0));
            cols = rows;
        } else if (n == 2) {
            rows = static_cast<size_t>(a0.elemAsDouble(0));
            cols = static_cast<size_t>(a0.elemAsDouble(1));
        } else {
            // 3+ dims: emit as flattened first×rest. MATLAB returns
            // an N-D array; numkit's 2-D Value can only represent the
            // first dim × product of the rest.
            rows = static_cast<size_t>(a0.elemAsDouble(0));
            cols = 1;
            for (size_t i = 1; i < n; ++i)
                cols *= static_cast<size_t>(a0.elemAsDouble(i));
        }
        return;
    }
    // Scalar size — 1 or 2 trailing args.
    rows = static_cast<size_t>(a0.toScalar());
    if (args.size() > start + 1 && !args[start + 1].isEmpty()) {
        cols = static_cast<size_t>(args[start + 1].toScalar());
    } else {
        cols = rows;  // n×n
    }
}

} // namespace numkit::stats::detail
