// libs/linalg/src/norms.cpp
//
// Vector / matrix norms — implementations and engine adapters.
// Migrated from libs/builtin/src/language/arrays/{matrix,predicates}.cpp.
//
// `norm_value` for the matrix 2-norm routes through the SVD kernel
// in numkit/linalg/decompositions.hpp.

#include <numkit/linalg/norms.hpp>

#include <numkit/linalg/decompositions.hpp> // svd_values
#include <numkit/core/engine.hpp>
#include <numkit/core/span.hpp>
#include <numkit/core/types.hpp>

#include <algorithm>
#include <cmath>
#include <complex>
#include <limits>

namespace numkit::linalg {

namespace {

bool isVectorShape(const Value &x)
{
    if (x.dims().ndim() != 2) return false;
    return x.dims().dim(0) == 1 || x.dims().dim(1) == 1;
}

inline std::size_t lin(std::size_t i, std::size_t j, std::size_t R) { return i + j * R; }

} // anonymous namespace

Value norm_value(const Value &x, double p, std::pmr::memory_resource *mr)
{
    if (x.numel() == 0) return Value::scalar(0.0, mr);

    if (isVectorShape(x)) {
        const std::size_t n = x.numel();
        const double *d = x.doubleData();
        if (p == 2.0) {
            double s = 0.0;
            for (std::size_t i = 0; i < n; ++i) s += d[i] * d[i];
            return Value::scalar(std::sqrt(s), mr);
        } else if (p == 1.0) {
            double s = 0.0;
            for (std::size_t i = 0; i < n; ++i) s += std::fabs(d[i]);
            return Value::scalar(s, mr);
        } else {
            double s = 0.0;
            for (std::size_t i = 0; i < n; ++i) s += std::pow(std::fabs(d[i]), p);
            return Value::scalar(std::pow(s, 1.0 / p), mr);
        }
    }

    // Matrix forms.
    if (x.dims().ndim() != 2)
        throw Error("norm: input must be vector or 2D matrix",
                    0, 0, "norm", "", "m:norm:badShape");
    const std::size_t m = static_cast<std::size_t>(x.dims().dim(0));
    const std::size_t n = static_cast<std::size_t>(x.dims().dim(1));
    const double *d = x.doubleData();

    if (p == 2.0) {
        // Largest singular value.
        auto sv = svd_values(x, mr);
        if (sv.numel() == 0) return Value::scalar(0.0, mr);
        return Value::scalar(sv.doubleData()[0], mr);
    }
    if (p == 1.0) {
        double mx = 0.0;
        for (std::size_t j = 0; j < n; ++j) {
            double s = 0.0;
            for (std::size_t i = 0; i < m; ++i) s += std::fabs(d[i + j * m]);
            mx = std::max(mx, s);
        }
        return Value::scalar(mx, mr);
    }
    throw Error("norm: matrix p-norms only support 1, 2, inf, 'fro'",
                0, 0, "norm", "", "m:norm:badP");
}

Value norm_inf(const Value &x, std::pmr::memory_resource *mr)
{
    if (x.numel() == 0) return Value::scalar(0.0, mr);
    if (isVectorShape(x)) {
        const std::size_t n = x.numel();
        const double *d = x.doubleData();
        double mx = 0.0;
        for (std::size_t i = 0; i < n; ++i)
            mx = std::max(mx, std::fabs(d[i]));
        return Value::scalar(mx, mr);
    }
    // Matrix inf-norm: max row sum.
    if (x.dims().ndim() != 2)
        throw Error("norm: input must be vector or 2D matrix",
                    0, 0, "norm", "", "m:norm:badShape");
    const std::size_t m = static_cast<std::size_t>(x.dims().dim(0));
    const std::size_t n = static_cast<std::size_t>(x.dims().dim(1));
    const double *d = x.doubleData();
    double mx = 0.0;
    for (std::size_t i = 0; i < m; ++i) {
        double s = 0.0;
        for (std::size_t j = 0; j < n; ++j) s += std::fabs(d[i + j * m]);
        mx = std::max(mx, s);
    }
    return Value::scalar(mx, mr);
}

Value norm_fro(const Value &x, std::pmr::memory_resource *mr)
{
    const std::size_t n = x.numel();
    if (n == 0) return Value::scalar(0.0, mr);
    const double *d = x.doubleData();
    double s = 0.0;
    for (std::size_t i = 0; i < n; ++i) s += d[i] * d[i];
    return Value::scalar(std::sqrt(s), mr);
}

// vecnorm(A [, p [, dim]]) — vector p-norm along dim.
//   defaults: p = 2, dim = first non-singleton dimension.
//   p = Inf  → max(|A|)
//   p = -Inf → min(|A|)
//   else     → (sum |A|^p) ^ (1/p)
//
// Output shape matches A with the reduced dim collapsed to length 1.
Value vecnorm(const Value &A, double p, int dim, std::pmr::memory_resource *mr)
{
    if (A.dims().is3D())
        throw Error("vecnorm: 3-D arrays not supported",
                    0, 0, "vecnorm", "", "m:vecnorm:3D");
    const size_t R = A.dims().rows();
    const size_t C = A.dims().cols();

    // Default dim: first non-singleton (1-based). Empties default to 1.
    if (dim == 0) {
        if (R == 0 && C == 0) dim = 1;
        else if (R != 1)      dim = 1;
        else                  dim = 2;
    }
    if (dim != 1 && dim != 2)
        throw Error("vecnorm: dim must be 1 or 2",
                    0, 0, "vecnorm", "", "m:vecnorm:BadDim");

    auto p_norm = [&](auto getAbs, size_t n) -> double {
        if (n == 0) return 0.0;
        if (std::isinf(p) && p > 0) {
            double m = 0.0;
            for (size_t k = 0; k < n; ++k) {
                double v = getAbs(k);
                if (std::isnan(v)) return std::numeric_limits<double>::quiet_NaN();
                if (v > m) m = v;
            }
            return m;
        }
        if (std::isinf(p) && p < 0) {
            double m = std::numeric_limits<double>::infinity();
            for (size_t k = 0; k < n; ++k) {
                double v = getAbs(k);
                if (std::isnan(v)) return std::numeric_limits<double>::quiet_NaN();
                if (v < m) m = v;
            }
            return m;
        }
        if (p == 2.0) {
            double s = 0.0;
            for (size_t k = 0; k < n; ++k) {
                double v = getAbs(k);
                s += v * v;
            }
            return std::sqrt(s);
        }
        double s = 0.0;
        for (size_t k = 0; k < n; ++k) s += std::pow(getAbs(k), p);
        return std::pow(s, 1.0 / p);
    };

    auto getAbs = [&](size_t i, size_t j) -> double {
        if (A.isComplex()) return std::abs(A.complexElem(i, j));
        return std::abs(A.elemAsDouble(lin(i, j, R)));
    };

    // Special case: completely empty input (0×0) produces scalar 0
    // (matches MATLAB convention: vecnorm([]) → 0, not 0×0 empty).
    if (R == 0 && C == 0)
        return Value::scalar(0.0, mr);

    if (dim == 1) {
        // Reduce along rows: output is (1 × C). For 0×N input, produces
        // a row of zeros (empty-norm convention). For M×0 input, output
        // is 1×0 (no columns to fill).
        Value out = Value::matrix(1, C, ValueType::DOUBLE, mr);
        if (C == 0) return out;
        double *od = out.doubleDataMut();
        for (size_t j = 0; j < C; ++j) {
            od[j] = p_norm([&](size_t k){ return getAbs(k, j); }, R);
        }
        return out;
    }
    // dim == 2: reduce along cols, output (R × 1). For 0×N → 0×1; for
    // M×0 → M×1 col of zeros.
    Value out = Value::matrix(R, 1, ValueType::DOUBLE, mr);
    if (R == 0) return out;
    double *od = out.doubleDataMut();
    for (size_t i = 0; i < R; ++i) {
        od[i] = p_norm([&](size_t k){ return getAbs(i, k); }, C);
    }
    return out;
}

// ════════════════════════════════════════════════════════════════════════
// Engine adapters — registered in LinalgLibrary::install
// ════════════════════════════════════════════════════════════════════════

namespace detail {

void norm_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty() || args.size() > 2)
        throw Error("norm: requires (X) or (X, p)",
                    0, 0, "norm", "", "m:norm:nargin");
    auto *mr = ctx.engine->resource();
    if (args.size() == 1) {
        outs[0] = norm_value(args[0], 2.0, mr);
        return;
    }
    const Value &p = args[1];
    if (p.isChar() || p.isString()) {
        const auto s = p.toString();
        if (s == "fro" || s == "Fro") {
            outs[0] = norm_fro(args[0], mr);
            return;
        }
        if (s == "inf" || s == "Inf") {
            outs[0] = norm_inf(args[0], mr);
            return;
        }
        throw Error("norm: string p must be 'fro' or 'inf'",
                    0, 0, "norm", "", "m:norm:badStringP");
    }
    const double pv = p.toScalar();
    if (std::isinf(pv)) {
        outs[0] = norm_inf(args[0], mr);
        return;
    }
    outs[0] = norm_value(args[0], pv, mr);
}

void vecnorm_reg(Span<const Value> args, size_t /*nargout*/,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("vecnorm: requires (A [, p [, dim]])",
                    0, 0, "vecnorm", "", "m:vecnorm:nargin");
    double p = 2.0;
    int dim = 0;
    if (args.size() >= 2) p = args[1].toScalar();
    if (args.size() >= 3) dim = static_cast<int>(args[2].toScalar());
    outs[0] = vecnorm(args[0], p, dim, ctx.engine->resource());
}

} // namespace detail

} // namespace numkit::linalg
