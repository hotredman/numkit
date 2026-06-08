// toolboxes/.../regress_detail.hpp — private compute/register substrate (anon-in-
// header, internal linkage per TU) shared by regress.cpp + regress_reg.cpp.
// Phase 2b compute/register split — see project_layering_refactor memory.
#pragma once

#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/span.hpp>
#include "reduction_helpers.hpp"  // engine-free numkit::builtin::detail dim-infra (ops re-export)

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory_resource>
#include <numeric>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace numkit::stats {

namespace {

// Cholesky factor of d×d symmetric PSD matrix M (column-major)
// into L (lower-triangular, column-major). Returns true on success.
bool cholesky(const double *M, double *L, size_t d)
{
    for (size_t i = 0; i < d * d; ++i) L[i] = 0.0;
    for (size_t j = 0; j < d; ++j) {
        double s = M[j + j * d];
        for (size_t k = 0; k < j; ++k) s -= L[j + k * d] * L[j + k * d];
        if (s <= 0.0) return false;
        const double Ljj = std::sqrt(s);
        L[j + j * d] = Ljj;
        for (size_t i = j + 1; i < d; ++i) {
            double t = M[i + j * d];
            for (size_t k = 0; k < j; ++k) t -= L[i + k * d] * L[j + k * d];
            L[i + j * d] = t / Ljj;
        }
    }
    return true;
}

// Solve L · z = b (forward substitution).
void fwd_solve(const double *L, double *z, const double *b, size_t d)
{
    for (size_t i = 0; i < d; ++i) {
        double s = b[i];
        for (size_t k = 0; k < i; ++k) s -= L[i + k * d] * z[k];
        z[i] = s / L[i + i * d];
    }
}

// Solve L^T · x = z (backward substitution).
void back_solve(const double *L, double *x, const double *z, size_t d)
{
    for (size_t i = d; i-- > 0;) {
        double s = z[i];
        for (size_t k = i + 1; k < d; ++k) s -= L[k + i * d] * x[k];
        x[i] = s / L[i + i * d];
    }
}

} // anonymous

// regress_full: full regress() worker -> [b, bint, r, rint, stats]. Def in
// regress.cpp (external).
std::tuple<Value, Value, Value, Value, Value>
regress_full(const Value &y, const Value &X, double alpha,
             std::pmr::memory_resource *mr);

} // namespace numkit::stats
