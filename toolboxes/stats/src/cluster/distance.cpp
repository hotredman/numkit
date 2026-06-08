// toolboxes/stats/src/cluster/distance.cpp

#include <numkit/stats/cluster/distance.hpp>

#include <numkit/value/value.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/error.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <limits>
#include <memory_resource>

#include "distance_detail.hpp"

namespace numkit::stats {


Value pdist(const Value &X, const std::string &metric, double p, const Value *C_opt, std::pmr::memory_resource *mr)
{
    const Metric m = parse_metric(metric);
    const size_t M = X.dims().rows();
    const size_t D = X.dims().cols();
    if (M < 2) return Value::matrix(1, 0, ValueType::DOUBLE, mr);
    const size_t Npairs = M * (M - 1) / 2;
    Value out = Value::matrix(1, Npairs, ValueType::DOUBLE, mr);
    double *od = out.doubleDataMut();

    ScratchArena scratch(mr);
    ScratchVec<double> xi(D, &scratch), xj(D, &scratch);

    if (m == Metric::Mahalanobis) {
        // Build Cinv once.
        ScratchVec<double> Cinv(D * D, 0.0, &scratch);
        if (C_opt) {
            const Value &C = *C_opt;
            if (C.dims().rows() != D || C.dims().cols() != D)
                throw Error("pdist: mahalanobis covariance C must be D×D",
                            0, 0, "pdist", "", "numkit:pdist:mahal");
            for (size_t r = 0; r < D; ++r)
                for (size_t c = 0; c < D; ++c)
                    Cinv[r * D + c] = C.elemAsDouble(c * D + r);
        } else {
            data_cov(X, Cinv.data(), &scratch);
        }
        invert_small(Cinv.data(), D, &scratch);
        ScratchVec<double> diff(D, &scratch);
        size_t idx = 0;
        for (size_t i = 0; i < M; ++i) {
            read_row(X, i, xi.data());
            for (size_t j = i + 1; j < M; ++j) {
                read_row(X, j, xj.data());
                od[idx++] = mahal_distance(xi.data(), xj.data(), D,
                                           Cinv.data(), diff.data());
            }
        }
        return out;
    }

    if (m == Metric::Seuclidean) {
        // Per-coordinate scaling: divide each difference by Sₖ (default
        // Sₖ = per-column sample std of X; explicit scale vector via C_opt).
        ScratchVec<double> inv_scale(D, &scratch);
        if (C_opt) {
            const Value &S = *C_opt;
            if (S.numel() != D)
                throw Error("pdist: seuclidean scale must have one element "
                            "per column", 0, 0, "pdist", "",
                            "numkit:pdist:seuclidean");
            for (size_t k = 0; k < D; ++k) inv_scale[k] = 1.0 / S.elemAsDouble(k);
        } else {
            ScratchVec<double> sd(D, &scratch);
            col_std(X, sd.data(), &scratch);
            for (size_t k = 0; k < D; ++k) inv_scale[k] = 1.0 / sd[k];
        }
        size_t idx = 0;
        for (size_t i = 0; i < M; ++i) {
            read_row(X, i, xi.data());
            for (size_t j = i + 1; j < M; ++j) {
                read_row(X, j, xj.data());
                od[idx++] = seuclidean_distance(xi.data(), xj.data(),
                                                inv_scale.data(), D);
            }
        }
        return out;
    }

    if (m == Metric::Spearman) {
        // Tied-rank each row, then correlation distance on the ranks.
        ScratchVec<double> R(M * D, &scratch);
        tiedrank_rows(X, R.data(), &scratch);
        size_t idx = 0;
        for (size_t i = 0; i < M; ++i)
            for (size_t j = i + 1; j < M; ++j)
                od[idx++] = correlation_distance(R.data() + i * D,
                                                 R.data() + j * D, D);
        return out;
    }

    size_t idx = 0;
    for (size_t i = 0; i < M; ++i) {
        read_row(X, i, xi.data());
        for (size_t j = i + 1; j < M; ++j) {
            read_row(X, j, xj.data());
            od[idx++] = row_distance(xi.data(), xj.data(), D, m, p);
        }
    }
    return out;
}

// Backward-compat wrapper without C.
Value pdist(const Value &X, const std::string &metric, double p, std::pmr::memory_resource *mr)
{
    return pdist(X, metric, p, nullptr, mr);
}

Value pdist2(const Value &X, const Value &Y, const std::string &metric, double p, const Value *C_opt, std::pmr::memory_resource *mr)
{
    const Metric m = parse_metric(metric);
    const size_t Mx = X.dims().rows();
    const size_t Dx = X.dims().cols();
    const size_t My = Y.dims().rows();
    const size_t Dy = Y.dims().cols();
    if (Dx != Dy)
        throw Error("pdist2: column counts must match", 0, 0, "pdist2", "",
                    "numkit:pdist2:size");
    Value out = Value::matrix(Mx, My, ValueType::DOUBLE, mr);
    if (Mx == 0 || My == 0) return out;
    double *od = out.doubleDataMut();

    ScratchArena scratch(mr);
    ScratchVec<double> xi(Dx, &scratch), yj(Dy, &scratch);

    if (m == Metric::Mahalanobis) {
        // For pdist2 with Mahalanobis, MATLAB uses cov(X) by default
        // (the *first* arg). Verified via R2025b probe.
        ScratchVec<double> Cinv(Dx * Dx, 0.0, &scratch);
        if (C_opt) {
            const Value &C = *C_opt;
            if (C.dims().rows() != Dx || C.dims().cols() != Dx)
                throw Error("pdist2: mahalanobis covariance C must be D×D",
                            0, 0, "pdist2", "", "numkit:pdist2:mahal");
            for (size_t r = 0; r < Dx; ++r)
                for (size_t c = 0; c < Dx; ++c)
                    Cinv[r * Dx + c] = C.elemAsDouble(c * Dx + r);
        } else {
            data_cov(X, Cinv.data(), &scratch);
        }
        invert_small(Cinv.data(), Dx, &scratch);
        ScratchVec<double> diff(Dx, &scratch);
        for (size_t j = 0; j < My; ++j) {
            read_row(Y, j, yj.data());
            for (size_t i = 0; i < Mx; ++i) {
                read_row(X, i, xi.data());
                od[j * Mx + i] = mahal_distance(xi.data(), yj.data(), Dx,
                                                Cinv.data(), diff.data());
            }
        }
        return out;
    }

    if (m == Metric::Seuclidean) {
        // Default scale = per-column sample std of X (the FIRST arg), matching
        // MATLAB; explicit scale vector via C_opt.
        ScratchVec<double> inv_scale(Dx, &scratch);
        if (C_opt) {
            const Value &S = *C_opt;
            if (S.numel() != Dx)
                throw Error("pdist2: seuclidean scale must have one element "
                            "per column", 0, 0, "pdist2", "",
                            "numkit:pdist2:seuclidean");
            for (size_t k = 0; k < Dx; ++k) inv_scale[k] = 1.0 / S.elemAsDouble(k);
        } else {
            ScratchVec<double> sd(Dx, &scratch);
            col_std(X, sd.data(), &scratch);
            for (size_t k = 0; k < Dx; ++k) inv_scale[k] = 1.0 / sd[k];
        }
        for (size_t j = 0; j < My; ++j) {
            read_row(Y, j, yj.data());
            for (size_t i = 0; i < Mx; ++i) {
                read_row(X, i, xi.data());
                od[j * Mx + i] = seuclidean_distance(xi.data(), yj.data(),
                                                     inv_scale.data(), Dx);
            }
        }
        return out;
    }

    if (m == Metric::Spearman) {
        // Tied-rank rows of X and Y independently, then correlation distance.
        ScratchVec<double> RX(Mx * Dx, &scratch);
        ScratchVec<double> RY(My * Dy, &scratch);
        tiedrank_rows(X, RX.data(), &scratch);
        tiedrank_rows(Y, RY.data(), &scratch);
        for (size_t j = 0; j < My; ++j)
            for (size_t i = 0; i < Mx; ++i)
                od[j * Mx + i] = correlation_distance(RX.data() + i * Dx,
                                                      RY.data() + j * Dy, Dx);
        return out;
    }

    for (size_t j = 0; j < My; ++j) {
        read_row(Y, j, yj.data());
        for (size_t i = 0; i < Mx; ++i) {
            read_row(X, i, xi.data());
            od[j * Mx + i] = row_distance(xi.data(), yj.data(), Dx, m, p);
        }
    }
    return out;
}

Value pdist2(const Value &X, const Value &Y, const std::string &metric, double p, std::pmr::memory_resource *mr)
{
    return pdist2(X, Y, metric, p, nullptr, mr);
}

// pdist2 with per-column top-k selection ('Smallest' / 'Largest').
// Returns D (k × My, sorted asc/desc) and I (k × My, 1-based row indices
// into X). On exit, D and I are k × My or min(Mx,k) × My when Mx < k.
void pdist2_topk(const Value &X, const Value &Y, const std::string &metric, double p, const Value *C_opt, size_t k, bool largest, Value &Dout, Value &Iout, std::pmr::memory_resource *mr)
{
    Value full = pdist2(X, Y, metric, p, C_opt, mr);
    const size_t Mx = X.dims().rows();
    const size_t My = Y.dims().rows();
    const size_t kk = std::min(k, Mx);
    Dout = Value::matrix(kk, My, ValueType::DOUBLE, mr);
    Iout = Value::matrix(kk, My, ValueType::DOUBLE, mr);
    double *dd = Dout.doubleDataMut();
    double *ii = Iout.doubleDataMut();
    const double *fd = full.doubleData();

    // Per-column sort.
    ScratchArena scratch(mr);
    ScratchVec<std::pair<double, size_t>> col(Mx, &scratch);
    for (size_t j = 0; j < My; ++j) {
        for (size_t i = 0; i < Mx; ++i) {
            col[i].first = fd[j * Mx + i];
            col[i].second = i;
        }
        if (largest) {
            std::partial_sort(col.begin(), col.begin() + kk, col.end(),
                [](const auto &a, const auto &b) {
                    if (a.first != b.first) return a.first > b.first;
                    return a.second < b.second;
                });
        } else {
            std::partial_sort(col.begin(), col.begin() + kk, col.end(),
                [](const auto &a, const auto &b) {
                    if (a.first != b.first) return a.first < b.first;
                    return a.second < b.second;
                });
        }
        for (size_t r = 0; r < kk; ++r) {
            dd[j * kk + r] = col[r].first;
            ii[j * kk + r] = double(col[r].second + 1);
        }
    }
}

Value squareform(const Value &d, std::pmr::memory_resource *mr) {
    // Detect whether `d` is a row vector (pdist form) or a square matrix.
    const auto &dims = d.dims();
    const size_t r = dims.rows(), c = dims.cols();
    const size_t n = d.numel();

    if (r == 1 || c == 1) {
        // Row/col → square: solve N(N-1)/2 = n.
        const double Nf = (1.0 + std::sqrt(1.0 + 8.0 * double(n))) / 2.0;
        const size_t N = (size_t)std::round(Nf);
        if (N * (N - 1) / 2 != n)
            throw Error("squareform: vector length must be triangular number",
                        0, 0, "squareform", "", "numkit:squareform:size");
        Value out = Value::matrix(N, N, ValueType::DOUBLE, mr);
        double *od = out.doubleDataMut();
        size_t k = 0;
        for (size_t i = 0; i < N; ++i)
            for (size_t j = i + 1; j < N; ++j) {
                const double v = d.elemAsDouble(k++);
                od[j * N + i] = v;  // (i, j)
                od[i * N + j] = v;  // (j, i)
            }
        return out;
    }
    // Square → vector.
    if (r != c)
        throw Error("squareform: matrix must be square", 0, 0, "squareform", "",
                    "numkit:squareform:size");
    const size_t N = r;
    Value out = Value::matrix(1, N * (N - 1) / 2, ValueType::DOUBLE, mr);
    double *od = out.doubleDataMut();
    size_t k = 0;
    for (size_t i = 0; i < N; ++i)
        for (size_t j = i + 1; j < N; ++j)
            od[k++] = d.elemAsDouble(j * N + i);
    return out;
}

Value mahal(const Value &Y, const Value &X, std::pmr::memory_resource *mr)
{
    const size_t Mx = X.dims().rows();
    const size_t D  = X.dims().cols();
    const size_t My = Y.dims().rows();
    if (Y.dims().cols() != D)
        throw Error("mahal: Y and X must have same column count",
                    0, 0, "mahal", "", "numkit:mahal:size");
    if (Mx < 2)
        throw Error("mahal: X must have at least 2 rows for covariance",
                    0, 0, "mahal", "", "numkit:mahal:size");

    ScratchArena scratch(mr);

    // Mean of X.
    ScratchVec<double> mu(D, 0.0, &scratch);
    for (size_t i = 0; i < Mx; ++i)
        for (size_t k = 0; k < D; ++k) mu[k] += X.elemAsDouble(k * Mx + i);
    for (auto &m : mu) m /= double(Mx);

    // Sample covariance (unbiased, divisor n-1).
    ScratchVec<double> C(D * D, 0.0, &scratch);
    ScratchVec<double> dx(D, &scratch);
    for (size_t i = 0; i < Mx; ++i) {
        for (size_t k = 0; k < D; ++k) dx[k] = X.elemAsDouble(k * Mx + i) - mu[k];
        for (size_t a = 0; a < D; ++a)
            for (size_t b = 0; b < D; ++b)
                C[a * D + b] += dx[a] * dx[b];
    }
    const double inv_n = 1.0 / double(Mx - 1);
    for (auto &c : C) c *= inv_n;

    // Cholesky decomposition: C = L · Lᵀ. Solve L · z = (y - μ) and return
    // |z|² (Mahalanobis distance squared).
    ScratchVec<double> L(D * D, 0.0, &scratch);
    for (size_t i = 0; i < D; ++i) {
        for (size_t j = 0; j <= i; ++j) {
            double s = C[i * D + j];
            for (size_t k = 0; k < j; ++k) s -= L[i * D + k] * L[j * D + k];
            if (i == j) {
                if (s <= 0.0)
                    throw Error("mahal: covariance matrix is not positive definite",
                                0, 0, "mahal", "", "numkit:mahal:notpd");
                L[i * D + j] = std::sqrt(s);
            } else {
                L[i * D + j] = s / L[j * D + j];
            }
        }
    }

    Value out = Value::matrix(My, 1, ValueType::DOUBLE, mr);
    double *od = out.doubleDataMut();
    ScratchVec<double> dy(D, &scratch);
    ScratchVec<double> z(D, 0.0, &scratch);
    for (size_t r = 0; r < My; ++r) {
        for (size_t k = 0; k < D; ++k) dy[k] = Y.elemAsDouble(k * My + r) - mu[k];
        // Forward substitution: L · z = dy.
        std::fill(z.begin(), z.end(), 0.0);
        for (size_t i = 0; i < D; ++i) {
            double s = dy[i];
            for (size_t k = 0; k < i; ++k) s -= L[i * D + k] * z[k];
            z[i] = s / L[i * D + i];
        }
        double m2 = 0.0;
        for (auto v : z) m2 += v * v;
        od[r] = m2;
    }
    return out;
}

} // namespace numkit::stats
