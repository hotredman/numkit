// libs/stats/src/sampling/lhs.cpp
//
// Latin Hypercube sampling (lhsdesign / lhsnorm).

#include <numkit/stats/sampling/lhs.hpp>
#include <numkit/stats/distributions/normal.hpp>

#include <numkit/builtin/math/random/rng.hpp>
#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include <algorithm>
#include <cmath>
#include <mutex>
#include <numeric>
#include <random>
#include <vector>

namespace numkit::stats {

namespace {

// In-place Cholesky factorisation (upper triangle on return; lower zeroed).
// MATLAB's chol(Sigma) returns the upper R such that R' * R = Sigma.
void choleskyUpperInPlace(double *S, std::size_t n)
{
    // Standard column-by-column Cholesky producing the upper triangle.
    // We treat S as row-major.
    for (std::size_t j = 0; j < n; ++j) {
        double diag = S[j * n + j];
        for (std::size_t k = 0; k < j; ++k)
            diag -= S[k * n + j] * S[k * n + j];
        if (diag <= 0.0)
            throw Error("lhsnorm: Sigma must be positive definite "
                        "(Cholesky failed at row " + std::to_string(j + 1) + ")",
                        0, 0, "lhsnorm", "", "numkit:lhsnorm:notPD");
        const double Rjj = std::sqrt(diag);
        S[j * n + j] = Rjj;
        for (std::size_t i = j + 1; i < n; ++i) {
            double s = S[j * n + i];
            for (std::size_t k = 0; k < j; ++k)
                s -= S[k * n + j] * S[k * n + i];
            S[j * n + i] = s / Rjj;
        }
        // Zero strict-lower triangle column for clarity.
        for (std::size_t i = j + 1; i < n; ++i)
            S[i * n + j] = 0.0;
    }
}

} // anonymous

namespace {

// Build a single LHS design into `od` (col-major n × p). Caller holds
// the RNG mutex. Template'd over RNG type to accept the project's
// MATLAB-compatible MT19937 stream.
template <typename Rng>
void buildOneLhs(double *od, std::size_t n, std::size_t p, bool smooth,
                 Rng &gen)
{
    std::uniform_real_distribution<double> U(0.0, 1.0);
    std::vector<std::size_t> perm(n);
    const double inv_n = 1.0 / static_cast<double>(n);
    for (std::size_t j = 0; j < p; ++j) {
        std::iota(perm.begin(), perm.end(), std::size_t(1));
        for (std::size_t i = n - 1; i > 0; --i) {
            std::uniform_int_distribution<std::size_t> dist(0, i);
            std::swap(perm[i], perm[dist(gen)]);
        }
        if (smooth) {
            for (std::size_t i = 0; i < n; ++i)
                od[j * n + i] = (static_cast<double>(perm[i]) - U(gen)) * inv_n;
        } else {
            for (std::size_t i = 0; i < n; ++i)
                od[j * n + i] = (static_cast<double>(perm[i]) - 0.5) * inv_n;
        }
    }
}

// Min pairwise squared Euclidean distance (rows of X, col-major n × p).
double minPairwiseDistSquared(const double *X, std::size_t n, std::size_t p)
{
    if (n < 2) return 0.0;
    double best = std::numeric_limits<double>::infinity();
    for (std::size_t i = 0; i < n; ++i) {
        for (std::size_t k = i + 1; k < n; ++k) {
            double s = 0.0;
            for (std::size_t j = 0; j < p; ++j) {
                const double d = X[j * n + i] - X[j * n + k];
                s += d * d;
            }
            if (s < best) best = s;
        }
    }
    return best;
}

// Max absolute off-diagonal Pearson correlation between columns of X.
double maxAbsColCorrelation(const double *X, std::size_t n, std::size_t p)
{
    if (p < 2 || n < 2) return 0.0;
    // Compute column means and SDs.
    std::vector<double> mu(p), sd(p);
    for (std::size_t j = 0; j < p; ++j) {
        double s = 0.0;
        for (std::size_t i = 0; i < n; ++i) s += X[j * n + i];
        mu[j] = s / static_cast<double>(n);
    }
    for (std::size_t j = 0; j < p; ++j) {
        double v = 0.0;
        for (std::size_t i = 0; i < n; ++i) {
            const double d = X[j * n + i] - mu[j];
            v += d * d;
        }
        sd[j] = std::sqrt(v / static_cast<double>(n - 1));
        if (sd[j] < 1e-300) sd[j] = 1e-300;
    }
    double best = 0.0;
    for (std::size_t a = 0; a < p; ++a) {
        for (std::size_t b = a + 1; b < p; ++b) {
            double s = 0.0;
            for (std::size_t i = 0; i < n; ++i)
                s += (X[a * n + i] - mu[a]) * (X[b * n + i] - mu[b]);
            const double r = s / (static_cast<double>(n - 1) * sd[a] * sd[b]);
            const double abr = std::fabs(r);
            if (abr > best) best = abr;
        }
    }
    return best;
}

} // anonymous

Value lhsdesign(std::size_t n, std::size_t p,
                bool smooth, LhsCriterion criterion, std::size_t iterations,
                std::pmr::memory_resource *mr)
{
    auto out = Value::matrix(n, p, ValueType::DOUBLE, mr);
    if (n == 0 || p == 0) return out;
    double *od = out.doubleDataMut();
    if (iterations < 1) iterations = 1;
    if (criterion == LhsCriterion::None) iterations = 1;

    auto &gen = ::numkit::builtin::sharedEngine();
    auto &mtx = ::numkit::builtin::rngMutex();
    std::lock_guard<std::mutex> lk(mtx);

    // First trial → direct write into `od`. Subsequent trials go to a
    // scratch buffer; replace `od` when the score improves.
    buildOneLhs(od, n, p, smooth, gen);

    if (iterations == 1 || criterion == LhsCriterion::None) return out;

    double best_score;
    auto score_of = [&](const double *X) {
        if (criterion == LhsCriterion::Maximin)
            return -minPairwiseDistSquared(X, n, p); // minimise neg → max dist
        return maxAbsColCorrelation(X, n, p);
    };
    best_score = score_of(od);

    std::vector<double> trial(n * p);
    for (std::size_t it = 1; it < iterations; ++it) {
        buildOneLhs(trial.data(), n, p, smooth, gen);
        const double s = score_of(trial.data());
        if (s < best_score) {
            best_score = s;
            std::copy(trial.begin(), trial.end(), od);
        }
    }
    return out;
}

Value lhsdesign(std::size_t n, std::size_t p,
                std::pmr::memory_resource *mr)
{
    // MATLAB defaults: smooth=on, criterion=maximin, iterations=5.
    return lhsdesign(n, p, /*smooth=*/true, LhsCriterion::Maximin, 5, mr);
}

Value lhsnorm(const Value &mu, const Value &Sigma, std::size_t n,
              std::pmr::memory_resource *mr)
{
    // Resolve d from mu's length, validate against Sigma's shape.
    const std::size_t d = mu.numel();
    if (d == 0)
        return Value::matrix(0, 0, ValueType::DOUBLE, mr);
    if (Sigma.dims().rows() != d || Sigma.dims().cols() != d)
        throw Error("lhsnorm: Sigma must be d × d where d = length(mu)",
                    0, 0, "lhsnorm", "", "numkit:lhsnorm:shape");

    // U is the LHS uniform design (n × d).
    Value U = lhsdesign(n, d, mr);
    // Z = norminv(U), n × d standard-normal samples.
    Value Z = norminv(U, 0.0, 1.0, mr);

    // Build the Cholesky factor R (upper) of Sigma, row-major.
    std::vector<double> R(d * d);
    for (std::size_t i = 0; i < d; ++i)
        for (std::size_t j = 0; j < d; ++j)
            R[i * d + j] = Sigma.elemAsDouble(j * d + i);   // col-major → row-major copy
    choleskyUpperInPlace(R.data(), d);

    // mu values (row vector).
    std::vector<double> muv(d);
    for (std::size_t k = 0; k < d; ++k) muv[k] = mu.elemAsDouble(k);

    auto out = Value::matrix(n, d, ValueType::DOUBLE, mr);
    double *od = out.doubleDataMut();   // col-major

    if (n == 0) return out;

    // Y[i, j] = mu[j] + Σ_k Z[i, k] * R[k, j]
    for (std::size_t i = 0; i < n; ++i) {
        for (std::size_t j = 0; j < d; ++j) {
            double s = muv[j];
            for (std::size_t k = 0; k <= j; ++k)
                s += Z.elemAsDouble(k * n + i) * R[k * d + j];
            od[j * n + i] = s;
        }
    }
    return out;
}

namespace detail {

void lhsdesign_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("lhsdesign: requires (n, p[, Name, Value, ...])",
                    0, 0, "lhsdesign", "", "numkit:lhsdesign:nargin");
    const std::size_t n = static_cast<std::size_t>(args[0].toScalar());
    const std::size_t p = static_cast<std::size_t>(args[1].toScalar());
    bool smooth = true;
    LhsCriterion crit = LhsCriterion::Maximin;
    std::size_t iters = 5;
    // Parse name-value pairs from args[2..].
    for (std::size_t k = 2; k + 1 < args.size(); k += 2) {
        if (!args[k].isChar() && !args[k].isString())
            throw Error("lhsdesign: name-value arguments expected",
                        0, 0, "lhsdesign", "", "numkit:lhsdesign:badNameValue");
        std::string name = args[k].toString();
        for (auto &c : name) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (name == "smooth") {
            std::string v = args[k + 1].toString();
            for (auto &c : v) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            smooth = (v != "off");
        } else if (name == "criterion") {
            std::string v = args[k + 1].toString();
            for (auto &c : v) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if      (v == "none")        crit = LhsCriterion::None;
            else if (v == "maximin")     crit = LhsCriterion::Maximin;
            else if (v == "correlation") crit = LhsCriterion::Correlation;
            else throw Error("lhsdesign: unknown criterion '" + v + "'",
                             0, 0, "lhsdesign", "", "numkit:lhsdesign:badCriterion");
        } else if (name == "iterations") {
            iters = static_cast<std::size_t>(args[k + 1].toScalar());
            if (iters < 1) iters = 1;
        } else {
            throw Error("lhsdesign: unknown option '" + name + "'",
                        0, 0, "lhsdesign", "", "numkit:lhsdesign:badOption");
        }
    }
    outs[0] = lhsdesign(n, p, smooth, crit, iters, ctx.engine->resource());
}

void lhsnorm_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("lhsnorm: requires (mu, Sigma, n)",
                    0, 0, "lhsnorm", "", "numkit:lhsnorm:nargin");
    const std::size_t n = static_cast<std::size_t>(args[2].toScalar());
    outs[0] = lhsnorm(args[0], args[1], n, ctx.engine->resource());
}

} // namespace detail
} // namespace numkit::stats
