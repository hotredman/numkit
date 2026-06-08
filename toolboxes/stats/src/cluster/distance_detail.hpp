// toolboxes/.../distance_detail.hpp — private compute/register substrate (anon-in-
// header, internal linkage per TU) shared by distance.cpp + distance_reg.cpp.
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

// Distance metric IDs.
enum class Metric { Euclidean, SqEuclidean, Cityblock, Chebychev, Minkowski,
                    Cosine, Correlation, Hamming, Jaccard, Mahalanobis,
                    Seuclidean, Spearman };

Metric parse_metric(const std::string &raw) {
    // Case-insensitive + accept MATLAB-style aliases (e.g. 'sqEuclidean'
    // for squared-euclidean).
    std::string s; s.reserve(raw.size());
    for (char c : raw) s.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    if (s == "euclidean")        return Metric::Euclidean;
    if (s == "squaredeuclidean" ||
        s == "sqeuclidean")      return Metric::SqEuclidean;
    if (s == "cityblock")        return Metric::Cityblock;
    if (s == "chebychev")        return Metric::Chebychev;
    if (s == "minkowski")        return Metric::Minkowski;
    if (s == "cosine")           return Metric::Cosine;
    if (s == "correlation")      return Metric::Correlation;
    if (s == "hamming")          return Metric::Hamming;
    if (s == "jaccard")          return Metric::Jaccard;
    if (s == "mahalanobis")      return Metric::Mahalanobis;
    if (s == "seuclidean")       return Metric::Seuclidean;
    if (s == "spearman")         return Metric::Spearman;
    throw Error("pdist: unknown metric '" + raw + "'", 0, 0, "pdist", "",
                "numkit:pdist:badmetric");
}

// Invert a small dense matrix in-place (D × D, row-major in `A`) using
// Gauss-Jordan with partial pivoting. Uses the supplied scratch arena
// for the 2D-wide augmented matrix. Throws on singular matrix.
inline void invert_small(double *A, size_t D,
                         std::pmr::memory_resource *scratch_mr)
{
    ScratchVec<double> aug(D * 2 * D, 0.0, scratch_mr);
    // Build augmented [A | I].
    for (size_t r = 0; r < D; ++r) {
        for (size_t c = 0; c < D; ++c) aug[r * 2 * D + c] = A[r * D + c];
        aug[r * 2 * D + D + r] = 1.0;
    }
    for (size_t k = 0; k < D; ++k) {
        // Partial pivot.
        size_t piv = k;
        double pmax = std::fabs(aug[k * 2 * D + k]);
        for (size_t r = k + 1; r < D; ++r) {
            const double v = std::fabs(aug[r * 2 * D + k]);
            if (v > pmax) { pmax = v; piv = r; }
        }
        if (pmax < 1e-14)
            throw Error("pdist: covariance matrix is singular for "
                        "Mahalanobis distance",
                        0, 0, "pdist", "", "numkit:pdist:singular");
        if (piv != k) {
            for (size_t c = 0; c < 2 * D; ++c)
                std::swap(aug[k * 2 * D + c], aug[piv * 2 * D + c]);
        }
        const double pivval = aug[k * 2 * D + k];
        const double inv_pivval = 1.0 / pivval;
        for (size_t c = 0; c < 2 * D; ++c) aug[k * 2 * D + c] *= inv_pivval;
        for (size_t r = 0; r < D; ++r) {
            if (r == k) continue;
            const double factor = aug[r * 2 * D + k];
            if (factor == 0.0) continue;
            for (size_t c = 0; c < 2 * D; ++c)
                aug[r * 2 * D + c] -= factor * aug[k * 2 * D + c];
        }
    }
    for (size_t r = 0; r < D; ++r)
        for (size_t c = 0; c < D; ++c)
            A[r * D + c] = aug[r * 2 * D + D + c];
}

// Compute the unbiased covariance matrix of the M×D data `X` (column-major
// in the Value) into the supplied D*D row-major buffer.
void data_cov(const Value &X, double *cov_out,
              std::pmr::memory_resource *scratch_mr)
{
    const size_t M = X.dims().rows();
    const size_t D = X.dims().cols();
    ScratchVec<double> mean(D, 0.0, scratch_mr);
    for (size_t c = 0; c < D; ++c) {
        for (size_t r = 0; r < M; ++r) mean[c] += X.elemAsDouble(c * M + r);
        mean[c] /= static_cast<double>(M);
    }
    std::fill(cov_out, cov_out + D * D, 0.0);
    ScratchVec<double> dev(D, scratch_mr);
    for (size_t r = 0; r < M; ++r) {
        for (size_t c = 0; c < D; ++c)
            dev[c] = X.elemAsDouble(c * M + r) - mean[c];
        for (size_t a = 0; a < D; ++a)
            for (size_t b = 0; b < D; ++b)
                cov_out[a * D + b] += dev[a] * dev[b];
    }
    const double inv_dof = (M > 1) ? 1.0 / static_cast<double>(M - 1) : 1.0;
    for (size_t i = 0; i < D * D; ++i) cov_out[i] *= inv_dof;
}

// Mahalanobis distance: d = sqrt((u - v)ᵀ · C_inv · (u - v)). Uses the
// caller's `diff` scratch buffer to avoid per-call allocation.
inline double mahal_distance(const double *u, const double *v, size_t D,
                              const double *Cinv, double *diff)
{
    for (size_t k = 0; k < D; ++k) diff[k] = u[k] - v[k];
    double s = 0.0;
    for (size_t a = 0; a < D; ++a) {
        double row = 0.0;
        for (size_t b = 0; b < D; ++b) row += Cinv[a * D + b] * diff[b];
        s += diff[a] * row;
    }
    return std::sqrt(std::max(0.0, s));
}

// Standardized-euclidean distance: √Σ((xₖ−yₖ)·inv_scaleₖ)². `inv_scale[k]`
// is 1/Sₖ where S defaults to the per-column sample std of the data set.
inline double seuclidean_distance(const double *x, const double *y,
                                  const double *inv_scale, size_t D)
{
    double s = 0.0;
    for (size_t k = 0; k < D; ++k) {
        const double d = (x[k] - y[k]) * inv_scale[k];
        s += d * d;
    }
    return std::sqrt(s);
}

// Correlation distance 1 − corr(x, y). NaN when either vector is constant
// (zero variance) — MATLAB's behaviour. Shared by the 'correlation' metric
// and (applied to tied ranks) by 'spearman'.
inline double correlation_distance(const double *x, const double *y, size_t D)
{
    double mx = 0, my = 0;
    for (size_t k = 0; k < D; ++k) { mx += x[k]; my += y[k]; }
    mx /= static_cast<double>(D);
    my /= static_cast<double>(D);
    double xy = 0, xx = 0, yy = 0;
    for (size_t k = 0; k < D; ++k) {
        const double dx = x[k] - mx, dy = y[k] - my;
        xy += dx * dy; xx += dx * dx; yy += dy * dy;
    }
    const double denom = std::sqrt(xx) * std::sqrt(yy);
    return (denom > 0.0) ? (1.0 - xy / denom)
                         : std::numeric_limits<double>::quiet_NaN();
}

inline double row_distance(const double *x, const double *y, size_t D,
                           Metric m, double p)
{
    switch (m) {
        case Metric::Euclidean: {
            double s = 0.0;
            for (size_t k = 0; k < D; ++k) { double d = x[k] - y[k]; s += d * d; }
            return std::sqrt(s);
        }
        case Metric::SqEuclidean: {
            double s = 0.0;
            for (size_t k = 0; k < D; ++k) { double d = x[k] - y[k]; s += d * d; }
            return s;
        }
        case Metric::Cityblock: {
            double s = 0.0;
            for (size_t k = 0; k < D; ++k) s += std::fabs(x[k] - y[k]);
            return s;
        }
        case Metric::Chebychev: {
            double m_ = 0.0;
            for (size_t k = 0; k < D; ++k) {
                double d = std::fabs(x[k] - y[k]);
                if (d > m_) m_ = d;
            }
            return m_;
        }
        case Metric::Minkowski: {
            double s = 0.0;
            for (size_t k = 0; k < D; ++k) {
                double d = std::fabs(x[k] - y[k]);
                s += std::pow(d, p);
            }
            return std::pow(s, 1.0 / p);
        }
        case Metric::Cosine: {
            double xy = 0, xx = 0, yy = 0;
            for (size_t k = 0; k < D; ++k) {
                xy += x[k] * y[k];
                xx += x[k] * x[k];
                yy += y[k] * y[k];
            }
            const double denom = std::sqrt(xx) * std::sqrt(yy);
            // Zero-norm row → cosine undefined; MATLAB returns NaN (not 1).
            return (denom > 0.0) ? (1.0 - xy / denom)
                                 : std::numeric_limits<double>::quiet_NaN();
        }
        case Metric::Correlation:
            return correlation_distance(x, y, D);
        case Metric::Hamming: {
            int diff = 0;
            for (size_t k = 0; k < D; ++k) if (x[k] != y[k]) ++diff;
            return double(diff) / double(D);
        }
        case Metric::Jaccard: {
            int diff = 0, considered = 0;
            for (size_t k = 0; k < D; ++k) {
                const bool xi = x[k] != 0.0, yi = y[k] != 0.0;
                if (xi || yi) {
                    ++considered;
                    if (xi != yi) ++diff;
                }
            }
            return considered > 0 ? double(diff) / double(considered) : 0.0;
        }
        case Metric::Mahalanobis:
        case Metric::Seuclidean:
        case Metric::Spearman:
            // These need data-set-wide pre-computation (covariance, per-column
            // scale, or tied ranks) and are handled by dedicated paths in
            // pdist/pdist2 — never via the pairwise row_distance switch.
            break;
    }
    return 0.0;
}

// Read row r of an M×D Value into a flat raw buffer. Column-major storage.
inline void read_row(const Value &X, size_t r, double *out) {
    const size_t M = X.dims().rows();
    const size_t D = X.dims().cols();
    for (size_t k = 0; k < D; ++k) out[k] = X.elemAsDouble(k * M + r);
}

// Per-column sample standard deviation (divisor n−1) of an M×D Value into
// `scale_out` (length D). The default 'seuclidean' coordinate scale.
inline void col_std(const Value &X, double *scale_out,
                    std::pmr::memory_resource * /*scratch_mr*/)
{
    const size_t M = X.dims().rows();
    const size_t D = X.dims().cols();
    for (size_t c = 0; c < D; ++c) {
        double mean = 0.0;
        for (size_t r = 0; r < M; ++r) mean += X.elemAsDouble(c * M + r);
        mean /= static_cast<double>(M);
        double ss = 0.0;
        for (size_t r = 0; r < M; ++r) {
            const double d = X.elemAsDouble(c * M + r) - mean;
            ss += d * d;
        }
        scale_out[c] = (M > 1) ? std::sqrt(ss / static_cast<double>(M - 1)) : 0.0;
    }
}

// Average-tie ranks (MATLAB `tiedrank`) of the D values in `v`, written to
// `r` in the same order. Ties share the mean of their 1-based ranks.
inline void tiedrank_row(const double *v, size_t D, double *r,
                         std::pmr::memory_resource *scratch_mr)
{
    ScratchVec<size_t> idx(D, scratch_mr);
    for (size_t k = 0; k < D; ++k) idx[k] = k;
    std::sort(idx.begin(), idx.end(),
              [&](size_t a, size_t b) { return v[a] < v[b]; });
    size_t i = 0;
    while (i < D) {
        size_t j = i;
        while (j + 1 < D && v[idx[j + 1]] == v[idx[i]]) ++j;
        const double avg = (static_cast<double>(i + 1) +
                            static_cast<double>(j + 1)) / 2.0;
        for (size_t t = i; t <= j; ++t) r[idx[t]] = avg;
        i = j + 1;
    }
}

// Build the M×D row-major tied-rank transform of X (each row ranked across
// its D coordinates) into `R`. Backs the 'spearman' metric.
inline void tiedrank_rows(const Value &X, double *R,
                          std::pmr::memory_resource *scratch_mr)
{
    const size_t M = X.dims().rows();
    const size_t D = X.dims().cols();
    ScratchVec<double> row(D, scratch_mr);
    for (size_t r = 0; r < M; ++r) {
        read_row(X, r, row.data());
        tiedrank_row(row.data(), D, R + r * D, scratch_mr);
    }
}

} // anonymous

// Internal pdist/pdist2 overloads (with optional Mahalanobis covariance C_opt)
// + the top-k variant, used by the reg adapters. Defs in distance.cpp (external).
Value pdist(const Value &X, const std::string &metric, double p,
            const Value *C_opt, std::pmr::memory_resource *mr);
Value pdist2(const Value &X, const Value &Y, const std::string &metric, double p,
             const Value *C_opt, std::pmr::memory_resource *mr);
void pdist2_topk(const Value &X, const Value &Y, const std::string &metric,
                 double p, const Value *C_opt, std::size_t k, bool largest,
                 Value &Dout, Value &Iout, std::pmr::memory_resource *mr);

} // namespace numkit::stats
