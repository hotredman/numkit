// libs/signal/src/digital_filtering/poly_utils.cpp
//
// MATLAB Signal Toolbox polyscale + polystab (Phase 4.3 of audio sweep).
// Bit-equal MATLAB R2025b — formulas extracted from polyscale.m / polystab.m.

#include <numkit/signal/digital_filtering/poly_utils.hpp>

#include <numkit/builtin/math/poly/polynomials.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/scratch.hpp>
#include <numkit/core/types.hpp>

#include <algorithm>
#include <cmath>
#include <complex>

namespace numkit::signal {

namespace {

// Whether v looks like a real value (numeric, not complex-typed).
bool isRealValue(const Value &v)
{
    return v.type() != ValueType::COMPLEX;
}

} // anon

Value polyscale(const Value &p, const Value &scale, std::pmr::memory_resource *mr)
{
    const size_t N = p.numel();
    if (N == 0) {
        return Value::matrix(0, 0, ValueType::DOUBLE, mr);
    }
    // Determine output complexity: if either input is complex, output complex.
    const bool cplx = !isRealValue(p) || !isRealValue(scale);
    // scale must be scalar.
    if (scale.numel() != 1)
        throw Error("polyscale: SCALE must be a scalar",
                    0, 0, "polyscale", "", "m:polyscale:BadScale");

    if (cplx) {
        Value out = Value::matrix(p.dims().rows(), p.dims().cols(),
                                   ValueType::COMPLEX, mr);
        Complex *od = out.complexDataMut();
        const Complex *pcd = (p.type() == ValueType::COMPLEX) ? p.complexData() : nullptr;
        const Complex sc = (scale.type() == ValueType::COMPLEX)
                            ? scale.complexData()[0]
                            : Complex(scale.toScalar(), 0.0);
        Complex pow = Complex(1.0, 0.0);
        for (size_t k = 0; k < N; ++k) {
            const Complex pk = pcd ? pcd[k] : Complex(p.elemAsDouble(k), 0.0);
            od[k] = pk * pow;
            pow *= sc;
        }
        return out;
    } else {
        Value out = Value::matrix(p.dims().rows(), p.dims().cols(),
                                   ValueType::DOUBLE, mr);
        double *od = out.doubleDataMut();
        const double sc = scale.toScalar();
        double pow = 1.0;
        for (size_t k = 0; k < N; ++k) {
            od[k] = p.elemAsDouble(k) * pow;
            pow *= sc;
        }
        return out;
    }
}

Value polystab(const Value &a, std::pmr::memory_resource *mr)
{
    const size_t N = a.numel();
    if (N == 0 || N == 1) {
        // Empty / scalar — return as-is (preserve type).
        if (N == 0) return Value::matrix(0, 0, ValueType::DOUBLE, mr);
        Value out = Value::matrix(a.dims().rows(), a.dims().cols(),
                                   isRealValue(a) ? ValueType::DOUBLE : ValueType::COMPLEX, mr);
        if (isRealValue(a))
            out.doubleDataMut()[0] = a.toScalar();
        else
            out.complexDataMut()[0] = a.complexData()[0];
        return out;
    }

    // 1. Compute roots(a) via libs/builtin.
    Value rv = builtin::roots(mr, a);
    const size_t R = rv.numel();
    // Roots may come back as DOUBLE (real-only) or COMPLEX.
    const bool rootsCplx = (rv.type() == ValueType::COMPLEX);

    // 2. Reflect outside-unit-circle roots inward.
    // Build a Complex vector of stabilized roots.
    ScratchArena scratch(mr);
    ScratchVec<Complex> stable(R, &scratch);
    for (size_t i = 0; i < R; ++i) {
        Complex v = rootsCplx ? rv.complexData()[i]
                              : Complex(rv.doubleData()[i], 0.0);
        if (v.real() == 0.0 && v.imag() == 0.0) {
            // Skip zero roots (MATLAB excludes via `i = v ~= 0`).
            stable[i] = v;
            continue;
        }
        const double mag = std::abs(v);
        // vs = 0.5 * (sign(|v|-1) + 1)
        // sign returns 0 for 0, 1 for >0, -1 for <0.
        // vs == 1 if |v| > 1, vs == 0 if |v| < 1, vs == 0.5 if |v| == 1.
        double vs;
        if (mag > 1.0)        vs = 1.0;
        else if (mag < 1.0)   vs = 0.0;
        else                  vs = 0.5;
        // stable = (1-vs)*v + vs/conj(v)
        const Complex inv_conj = Complex(1.0, 0.0) / std::conj(v);
        stable[i] = (1.0 - vs) * v + vs * inv_conj;
    }

    // 3. Build complex root Value, call poly() to get coefficients back.
    Value rootsStableV = Value::matrix(R, 1, ValueType::COMPLEX, mr);
    {
        Complex *p = rootsStableV.complexDataMut();
        std::copy(stable.data(), stable.data() + R, p);
    }
    Value bRaw = builtin::poly(mr, rootsStableV);

    // 4. Multiply by leading nonzero coefficient of a (matches MATLAB:
    //    `b = a(find(ind, 1)) * poly(v)`).
    double leadReal = 0.0;
    Complex leadCplx(0.0, 0.0);
    bool aIsReal = isRealValue(a);
    for (size_t i = 0; i < N; ++i) {
        if (aIsReal) {
            const double v = a.elemAsDouble(i);
            if (v != 0.0) { leadReal = v; break; }
        } else {
            const Complex c = a.complexData()[i];
            if (c.real() != 0.0 || c.imag() != 0.0) { leadCplx = c; break; }
        }
    }

    const size_t M = bRaw.numel();
    // poly() returns COMPLEX in general; multiply by leading coef.
    Value out;
    if (aIsReal) {
        // Output real (matches MATLAB `if isreal(a): b = real(b)`).
        out = Value::matrix(1, M, ValueType::DOUBLE, mr);
        double *od = out.doubleDataMut();
        if (bRaw.type() == ValueType::COMPLEX) {
            const Complex *bd = bRaw.complexData();
            for (size_t k = 0; k < M; ++k) od[k] = leadReal * bd[k].real();
        } else {
            const double *bd = bRaw.doubleData();
            for (size_t k = 0; k < M; ++k) od[k] = leadReal * bd[k];
        }
    } else {
        out = Value::matrix(1, M, ValueType::COMPLEX, mr);
        Complex *od = out.complexDataMut();
        if (bRaw.type() == ValueType::COMPLEX) {
            const Complex *bd = bRaw.complexData();
            for (size_t k = 0; k < M; ++k) od[k] = leadCplx * bd[k];
        } else {
            const double *bd = bRaw.doubleData();
            for (size_t k = 0; k < M; ++k) od[k] = leadCplx * Complex(bd[k], 0.0);
        }
    }
    return out;
}

namespace detail {

void polyscale_reg(Span<const Value> args, size_t /*nargout*/,
                    Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("polyscale: requires (p, scale)",
                    0, 0, "polyscale", "", "m:polyscale:nargin");
    outs[0] = polyscale(args[0], args[1], ctx.engine->resource());
}

void polystab_reg(Span<const Value> args, size_t /*nargout*/,
                   Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("polystab: requires (a)",
                    0, 0, "polystab", "", "m:polystab:nargin");
    outs[0] = polystab(args[0], ctx.engine->resource());
}

} // namespace detail

} // namespace numkit::signal
