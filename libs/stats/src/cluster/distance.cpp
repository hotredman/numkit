// libs/stats/src/cluster/distance.cpp

#include <numkit/stats/cluster/distance.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/scratch.hpp>
#include <numkit/core/types.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <limits>
#include <memory_resource>

namespace numkit::stats {

namespace {

// Distance metric IDs.
enum class Metric { Euclidean, SqEuclidean, Cityblock, Chebychev, Minkowski,
                    Cosine, Correlation, Hamming, Jaccard, Mahalanobis };

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
    throw Error("pdist: unknown metric '" + raw + "'", 0, 0, "pdist", "",
                "m:pdist:badmetric");
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
                        0, 0, "pdist", "", "m:pdist:singular");
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
            return (denom > 0.0) ? (1.0 - xy / denom) : 1.0;
        }
        case Metric::Correlation: {
            double mx = 0, my = 0;
            for (size_t k = 0; k < D; ++k) { mx += x[k]; my += y[k]; }
            mx /= D; my /= D;
            double xy = 0, xx = 0, yy = 0;
            for (size_t k = 0; k < D; ++k) {
                const double dx = x[k] - mx, dy = y[k] - my;
                xy += dx * dy; xx += dx * dx; yy += dy * dy;
            }
            const double denom = std::sqrt(xx) * std::sqrt(yy);
            return (denom > 0.0) ? (1.0 - xy / denom) : 1.0;
        }
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
    }
    return 0.0;
}

// Read row r of an M×D Value into a flat raw buffer. Column-major storage.
inline void read_row(const Value &X, size_t r, double *out) {
    const size_t M = X.dims().rows();
    const size_t D = X.dims().cols();
    for (size_t k = 0; k < D; ++k) out[k] = X.elemAsDouble(k * M + r);
}

} // anonymous

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
                            0, 0, "pdist", "", "m:pdist:mahal");
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
                    "m:pdist2:size");
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
                            0, 0, "pdist2", "", "m:pdist2:mahal");
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
                        0, 0, "squareform", "", "m:squareform:size");
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
                    "m:squareform:size");
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
                    0, 0, "mahal", "", "m:mahal:size");
    if (Mx < 2)
        throw Error("mahal: X must have at least 2 rows for covariance",
                    0, 0, "mahal", "", "m:mahal:size");

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
                                0, 0, "mahal", "", "m:mahal:notpd");
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

// ════════════════════════════════════════════════════════════════════
// Engine adapters
// ════════════════════════════════════════════════════════════════════

namespace detail {

namespace {
struct MetricArgs {
    std::string metric;
    double p;
    const Value *C;       // Mahalanobis covariance (nullable).
};
MetricArgs parse_metric_args(Span<const Value> args, size_t start) {
    MetricArgs out{"euclidean", 2.0, nullptr};
    for (size_t i = start; i < args.size(); ++i) {
        if (args[i].isChar() || args[i].isString()) {
            out.metric = args[i].toString();
        } else if (args[i].numel() == 1) {
            out.p = args[i].toScalar();
        } else if (args[i].numel() > 1) {
            // Multi-element arg: treated as Mahalanobis covariance C.
            out.C = &args[i];
        }
    }
    return out;
}
} // anonymous

void pdist_reg(Span<const Value> args, size_t /*nargout*/,
               Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("pdist: requires X[, metric[, p|C]]", 0, 0, "pdist", "",
                    "m:pdist:nargin");
    auto a = parse_metric_args(args, 1);
    outs[0] = pdist(args[0], a.metric, a.p, a.C, ctx.engine->resource());
}

void pdist2_reg(Span<const Value> args, size_t /*nargout*/,
                Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("pdist2: requires (X, Y[, metric[, p|C]])", 0, 0, "pdist2", "",
                    "m:pdist2:nargin");

    // Walk args[2..]: extract optional 'Smallest'/'Largest' N-V pair, then
    // pass the rest to the standard metric/p/C parser.
    ScratchArena scratch(ctx.engine->resource());
    std::pmr::vector<Value> filtered(&scratch);
    filtered.reserve(args.size());
    bool topk_mode = false;
    bool largest = false;
    size_t k = 0;
    for (size_t i = 2; i < args.size(); ++i) {
        if ((args[i].isChar() || args[i].isString()) && i + 1 < args.size()) {
            const std::string s = args[i].toString();
            std::string sl; sl.reserve(s.size());
            for (char c : s) sl.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
            if (sl == "smallest" || sl == "largest") {
                topk_mode = true;
                largest = (sl == "largest");
                k = static_cast<size_t>(args[i + 1].toScalar());
                ++i; // skip value
                continue;
            }
        }
        filtered.push_back(args[i]);
    }

    // Build a mini-args view for parse_metric_args (using filtered).
    // parse_metric_args expects a Span starting at offset; we re-use by
    // creating a small temporary Vector of Values prepended with two dummies
    // (skipped via start=2). Simpler: inline parse here on the filtered vec.
    MetricArgs a{"euclidean", 2.0, nullptr};
    for (size_t i = 0; i < filtered.size(); ++i) {
        if (filtered[i].isChar() || filtered[i].isString()) {
            a.metric = filtered[i].toString();
        } else if (filtered[i].numel() == 1) {
            a.p = filtered[i].toScalar();
        } else if (filtered[i].numel() > 1) {
            a.C = &filtered[i];
        }
    }

    if (topk_mode) {
        Value D, I;
        pdist2_topk(args[0], args[1], a.metric, a.p, a.C, k, largest, D, I, ctx.engine->resource());
        outs[0] = D;
        if (outs.size() > 1) outs[1] = I;
    } else {
        outs[0] = pdist2(args[0], args[1], a.metric, a.p, a.C, ctx.engine->resource());
    }
}

void squareform_reg(Span<const Value> args, size_t /*nargout*/,
                    Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("squareform: requires d", 0, 0, "squareform", "",
                    "m:squareform:nargin");
    outs[0] = squareform(args[0], ctx.engine->resource());
}

void mahal_reg(Span<const Value> args, size_t /*nargout*/,
               Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("mahal: requires (Y, X)", 0, 0, "mahal", "",
                    "m:mahal:nargin");
    outs[0] = mahal(args[0], args[1], ctx.engine->resource());
}

} // namespace detail
} // namespace numkit::stats
