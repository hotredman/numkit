// ops/src/fused_kernels_portable.cpp
//
// Scalar fallbacks for the fused element-wise kernels (non-SIMD builds). One
// file for all kernels — scalar loops are tiny and carry no Highway
// multi-target / inliner-budget cost, so there is no reason to split them.
// Semantics match the SIMD versions bit-for-bit (std::fmin/fmax NaN rules).

#include <numkit/ops/fused_kernels.hpp>

#include <cmath>
#include <cstddef>

namespace numkit::ops {

void fusedAffineClamp(const double *x, double scale, double offset,
                      double lo, double hi, double *out, std::size_t n) {
    for (std::size_t i = 0; i < n; ++i) {
        const double v = scale * x[i] + offset;
        out[i] = std::fmax(lo, std::fmin(hi, v));
    }
}

void fusedAffineClampMinOuter(const double *x, double scale, double offset,
                              double lo, double hi, double *out, std::size_t n) {
    for (std::size_t i = 0; i < n; ++i) {
        const double v = scale * x[i] + offset;
        out[i] = std::fmin(hi, std::fmax(lo, v));
    }
}

void fusedAffineClampShiftDiv(const double *x, double sub, double div,
                              double lo, double hi, double *out, std::size_t n) {
    for (std::size_t i = 0; i < n; ++i) {
        const double v = (x[i] - sub) / div;
        out[i] = std::fmax(lo, std::fmin(hi, v));
    }
}

void fusedAffineClampMinOuterShiftDiv(const double *x, double sub, double div,
                                      double lo, double hi, double *out,
                                      std::size_t n) {
    for (std::size_t i = 0; i < n; ++i) {
        const double v = (x[i] - sub) / div;
        out[i] = std::fmin(hi, std::fmax(lo, v));
    }
}

void fusedAffine(const double *x, double scale, double offset,
                 double *out, std::size_t n) {
    for (std::size_t i = 0; i < n; ++i) out[i] = scale * x[i] + offset;
}

void fusedAxpby(const double *x, double a, const double *y, double b,
                double *out, std::size_t n) {
    for (std::size_t i = 0; i < n; ++i) out[i] = a * x[i] + b * y[i];
}

void fusedShiftScaleMul(const double *x, double sub, double mul,
                        double *out, std::size_t n) {
    for (std::size_t i = 0; i < n; ++i) out[i] = (x[i] - sub) * mul;
}

void fusedShiftScaleDiv(const double *x, double sub, double div,
                        double *out, std::size_t n) {
    for (std::size_t i = 0; i < n; ++i) out[i] = (x[i] - sub) / div;
}

void fusedAbsAffine(const double *x, double scale, double offset,
                    double *out, std::size_t n) {
    for (std::size_t i = 0; i < n; ++i) out[i] = std::fabs(scale * x[i] + offset);
}

void fusedAbsDiff(const double *x, const double *y, double *out, std::size_t n) {
    for (std::size_t i = 0; i < n; ++i) out[i] = std::fabs(x[i] - y[i]);
}

void fusedAbsShiftDiv(const double *x, double sub, double div,
                      double *out, std::size_t n) {
    for (std::size_t i = 0; i < n; ++i) out[i] = std::fabs((x[i] - sub) / div);
}

void fusedUnaryAffine(const double *x, double scale, double offset,
                      UnaryAffineFn fn, double *out, std::size_t n) {
    switch (fn) {
        case UnaryAffineFn::Sqrt:
            for (std::size_t i = 0; i < n; ++i) out[i] = std::sqrt(scale * x[i] + offset);
            break;
        case UnaryAffineFn::Floor:
            for (std::size_t i = 0; i < n; ++i) out[i] = std::floor(scale * x[i] + offset);
            break;
        case UnaryAffineFn::Ceil:
            for (std::size_t i = 0; i < n; ++i) out[i] = std::ceil(scale * x[i] + offset);
            break;
    }
}

void fusedUnaryShiftDiv(const double *x, double sub, double div,
                        UnaryAffineFn fn, double *out, std::size_t n) {
    switch (fn) {
        case UnaryAffineFn::Sqrt:
            for (std::size_t i = 0; i < n; ++i) out[i] = std::sqrt((x[i] - sub) / div);
            break;
        case UnaryAffineFn::Floor:
            for (std::size_t i = 0; i < n; ++i) out[i] = std::floor((x[i] - sub) / div);
            break;
        case UnaryAffineFn::Ceil:
            for (std::size_t i = 0; i < n; ++i) out[i] = std::ceil((x[i] - sub) / div);
            break;
    }
}

void fusedSqAffine(const double *x, double scale, double offset,
                   double *out, std::size_t n) {
    for (std::size_t i = 0; i < n; ++i) {
        const double v = scale * x[i] + offset;
        out[i] = v * v;
    }
}

void fusedSqDiff(const double *x, const double *y, double *out, std::size_t n) {
    for (std::size_t i = 0; i < n; ++i) {
        const double v = x[i] - y[i];
        out[i] = v * v;
    }
}

void fusedSqShiftDiv(const double *x, double sub, double div,
                     double *out, std::size_t n) {
    for (std::size_t i = 0; i < n; ++i) {
        const double v = (x[i] - sub) / div;
        out[i] = v * v;
    }
}

void fusedSqrtSumSq(const double *x, const double *y, double *out, std::size_t n) {
    for (std::size_t i = 0; i < n; ++i) out[i] = std::sqrt(x[i] * x[i] + y[i] * y[i]);
}

void fusedSoftThreshold(const double *x, double t, double *out, std::size_t n) {
    for (std::size_t i = 0; i < n; ++i) {
        const double v = x[i];
        const double s = std::isnan(v) ? v : (v > 0 ? 1.0 : (v < 0 ? -1.0 : 0.0));
        out[i] = s * std::fmax(0.0, std::fabs(v) - t);
    }
}

void fusedTransAffine(const double *x, double scale, double offset,
                      TransAffineFn fn, double *out, std::size_t n) {
    // Non-SIMD build: numkit's exp/expm1 are scalar std:: here too, so the
    // affine + scalar transcendental matches the per-op path bit-for-bit.
    switch (fn) {
        case TransAffineFn::Exp:
            for (std::size_t i = 0; i < n; ++i) out[i] = std::exp(scale * x[i] + offset);
            break;
        case TransAffineFn::Expm1:
            for (std::size_t i = 0; i < n; ++i) out[i] = std::expm1(scale * x[i] + offset);
            break;
        case TransAffineFn::Log:
            for (std::size_t i = 0; i < n; ++i) out[i] = std::log(scale * x[i] + offset);
            break;
        case TransAffineFn::Log2:
            for (std::size_t i = 0; i < n; ++i) out[i] = std::log2(scale * x[i] + offset);
            break;
        case TransAffineFn::Log10:
            for (std::size_t i = 0; i < n; ++i) out[i] = std::log10(scale * x[i] + offset);
            break;
        case TransAffineFn::Sin:
            for (std::size_t i = 0; i < n; ++i) out[i] = std::sin(scale * x[i] + offset);
            break;
        case TransAffineFn::Cos:
            for (std::size_t i = 0; i < n; ++i) out[i] = std::cos(scale * x[i] + offset);
            break;
        case TransAffineFn::Tanh:
            for (std::size_t i = 0; i < n; ++i) out[i] = std::tanh(scale * x[i] + offset);
            break;
        case TransAffineFn::Sinh:
            for (std::size_t i = 0; i < n; ++i) out[i] = std::sinh(scale * x[i] + offset);
            break;
        case TransAffineFn::Atan:
            for (std::size_t i = 0; i < n; ++i) out[i] = std::atan(scale * x[i] + offset);
            break;
        case TransAffineFn::Asinh:
            for (std::size_t i = 0; i < n; ++i) out[i] = std::asinh(scale * x[i] + offset);
            break;
        case TransAffineFn::Asin:
            for (std::size_t i = 0; i < n; ++i) out[i] = std::asin(scale * x[i] + offset);
            break;
        case TransAffineFn::Acos:
            for (std::size_t i = 0; i < n; ++i) out[i] = std::acos(scale * x[i] + offset);
            break;
        case TransAffineFn::Acosh:
            for (std::size_t i = 0; i < n; ++i) out[i] = std::acosh(scale * x[i] + offset);
            break;
        case TransAffineFn::Atanh:
            for (std::size_t i = 0; i < n; ++i) out[i] = std::atanh(scale * x[i] + offset);
            break;
        case TransAffineFn::Log1p:
            for (std::size_t i = 0; i < n; ++i) out[i] = std::log1p(scale * x[i] + offset);
            break;
        case TransAffineFn::Cosh:
            for (std::size_t i = 0; i < n; ++i) out[i] = std::cosh(scale * x[i] + offset);
            break;
        case TransAffineFn::Tan:
            for (std::size_t i = 0; i < n; ++i) out[i] = std::tan(scale * x[i] + offset);
            break;
    }
}

void fusedTransShiftDiv(const double *x, double sub, double div,
                        TransAffineFn fn, double *out, std::size_t n) {
    for (std::size_t i = 0; i < n; ++i) {
        const double v = (x[i] - sub) / div;
        switch (fn) {
            case TransAffineFn::Exp:   out[i] = std::exp(v);   break;
            case TransAffineFn::Expm1: out[i] = std::expm1(v); break;
            case TransAffineFn::Log:   out[i] = std::log(v);   break;
            case TransAffineFn::Log2:  out[i] = std::log2(v);  break;
            case TransAffineFn::Log10: out[i] = std::log10(v); break;
            case TransAffineFn::Sin:   out[i] = std::sin(v);   break;
            case TransAffineFn::Cos:   out[i] = std::cos(v);   break;
            case TransAffineFn::Tanh:  out[i] = std::tanh(v);  break;
            case TransAffineFn::Sinh:  out[i] = std::sinh(v);  break;
            case TransAffineFn::Atan:  out[i] = std::atan(v);  break;
            case TransAffineFn::Asinh: out[i] = std::asinh(v); break;
            case TransAffineFn::Asin:  out[i] = std::asin(v);  break;
            case TransAffineFn::Acos:  out[i] = std::acos(v);  break;
            case TransAffineFn::Acosh: out[i] = std::acosh(v); break;
            case TransAffineFn::Atanh: out[i] = std::atanh(v); break;
            case TransAffineFn::Log1p: out[i] = std::log1p(v); break;
            case TransAffineFn::Cosh:  out[i] = std::cosh(v);  break;
            case TransAffineFn::Tan:   out[i] = std::tan(v);   break;
        }
    }
}

} // namespace numkit::ops
