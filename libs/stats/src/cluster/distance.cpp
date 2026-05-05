// libs/stats/src/cluster/distance.cpp

#include <numkit/stats/cluster/distance.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <limits>
#include <vector>

namespace numkit::stats {

namespace {

// Distance metric IDs.
enum class Metric { Euclidean, SqEuclidean, Cityblock, Chebychev, Minkowski,
                    Cosine, Correlation, Hamming, Jaccard };

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
    throw Error("pdist: unknown metric '" + raw + "'", 0, 0, "pdist", "",
                "m:pdist:badmetric");
}

inline double row_distance(const std::vector<double> &x,
                           const std::vector<double> &y,
                           Metric m, double p)
{
    const size_t D = x.size();
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

// Read row r of an M×D Value into a buffer. Column-major storage.
inline void read_row(const Value &X, size_t r, std::vector<double> &out) {
    const size_t M = X.dims().rows();
    const size_t D = X.dims().cols();
    out.resize(D);
    for (size_t k = 0; k < D; ++k) out[k] = X.elemAsDouble(k * M + r);
}

} // anonymous

Value pdist(std::pmr::memory_resource *mr, const Value &X,
            const std::string &metric, double p)
{
    const Metric m = parse_metric(metric);
    const size_t M = X.dims().rows();
    const size_t D = X.dims().cols();
    if (M < 2) return Value::matrix(1, 0, ValueType::DOUBLE, mr);
    const size_t Npairs = M * (M - 1) / 2;
    Value out = Value::matrix(1, Npairs, ValueType::DOUBLE, mr);
    double *od = out.doubleDataMut();

    std::vector<double> xi(D), xj(D);
    size_t idx = 0;
    for (size_t i = 0; i < M; ++i) {
        read_row(X, i, xi);
        for (size_t j = i + 1; j < M; ++j) {
            read_row(X, j, xj);
            od[idx++] = row_distance(xi, xj, m, p);
        }
    }
    return out;
}

Value pdist2(std::pmr::memory_resource *mr, const Value &X, const Value &Y,
             const std::string &metric, double p)
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

    std::vector<double> xi(Dx), yj(Dy);
    for (size_t j = 0; j < My; ++j) {
        read_row(Y, j, yj);
        for (size_t i = 0; i < Mx; ++i) {
            read_row(X, i, xi);
            od[j * Mx + i] = row_distance(xi, yj, m, p);
        }
    }
    return out;
}

Value squareform(std::pmr::memory_resource *mr, const Value &d) {
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

Value mahal(std::pmr::memory_resource *mr, const Value &Y, const Value &X)
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

    // Mean of X.
    std::vector<double> mu(D, 0.0);
    for (size_t i = 0; i < Mx; ++i)
        for (size_t k = 0; k < D; ++k) mu[k] += X.elemAsDouble(k * Mx + i);
    for (auto &m : mu) m /= double(Mx);

    // Sample covariance (unbiased, divisor n-1).
    std::vector<double> C(D * D, 0.0);
    for (size_t i = 0; i < Mx; ++i) {
        std::vector<double> dx(D);
        for (size_t k = 0; k < D; ++k) dx[k] = X.elemAsDouble(k * Mx + i) - mu[k];
        for (size_t a = 0; a < D; ++a)
            for (size_t b = 0; b < D; ++b)
                C[a * D + b] += dx[a] * dx[b];
    }
    const double inv_n = 1.0 / double(Mx - 1);
    for (auto &c : C) c *= inv_n;

    // Cholesky decomposition: C = L · Lᵀ. Solve L · z = (y - μ) and return
    // |z|² (Mahalanobis distance squared).
    std::vector<double> L(D * D, 0.0);
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
    for (size_t r = 0; r < My; ++r) {
        std::vector<double> dy(D);
        for (size_t k = 0; k < D; ++k) dy[k] = Y.elemAsDouble(k * My + r) - mu[k];
        // Forward substitution: L · z = dy.
        std::vector<double> z(D, 0.0);
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
std::pair<std::string, double> parse_metric_args(Span<const Value> args, size_t start) {
    std::string metric = "euclidean";
    double p = 2.0;
    for (size_t i = start; i < args.size(); ++i) {
        if (args[i].isChar() || args[i].isString()) {
            metric = args[i].toString();
        } else if (args[i].numel() == 1) {
            p = args[i].toScalar();
        }
    }
    return {metric, p};
}
} // anonymous

void pdist_reg(Span<const Value> args, size_t /*nargout*/,
               Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("pdist: requires X[, metric, p]", 0, 0, "pdist", "",
                    "m:pdist:nargin");
    auto [m, p] = parse_metric_args(args, 1);
    outs[0] = pdist(ctx.engine->resource(), args[0], m, p);
}

void pdist2_reg(Span<const Value> args, size_t /*nargout*/,
                Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("pdist2: requires (X, Y[, metric, p])", 0, 0, "pdist2", "",
                    "m:pdist2:nargin");
    auto [m, p] = parse_metric_args(args, 2);
    outs[0] = pdist2(ctx.engine->resource(), args[0], args[1], m, p);
}

void squareform_reg(Span<const Value> args, size_t /*nargout*/,
                    Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("squareform: requires d", 0, 0, "squareform", "",
                    "m:squareform:nargin");
    outs[0] = squareform(ctx.engine->resource(), args[0]);
}

void mahal_reg(Span<const Value> args, size_t /*nargout*/,
               Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("mahal: requires (Y, X)", 0, 0, "mahal", "",
                    "m:mahal:nargin");
    outs[0] = mahal(ctx.engine->resource(), args[0], args[1]);
}

} // namespace detail
} // namespace numkit::stats
