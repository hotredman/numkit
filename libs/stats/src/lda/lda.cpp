// libs/stats/src/lda/lda.cpp

#include <numkit/stats/lda/lda.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace numkit::stats {

namespace {

constexpr double kLog2Pi = 1.8378770664093454835606594728112352;

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

void fwd_solve(const double *L, double *z, const double *b, size_t d)
{
    for (size_t i = 0; i < d; ++i) {
        double s = b[i];
        for (size_t k = 0; k < i; ++k) s -= L[i + k * d] * z[k];
        z[i] = s / L[i + i * d];
    }
}

} // anonymous

std::tuple<Value, Value, Value, Value>
classify(const Value &sample, const Value &training, const Value &group, const std::string &type_in, std::pmr::memory_resource *mr)
{
    const size_t d     = training.dims().cols();
    const size_t Ntr   = training.dims().rows();
    const size_t Nsamp = sample.dims().rows();
    if (sample.dims().cols() != d)
        throw Error("classify: sample and training must have the same number of columns",
                    0, 0, "classify", "", "numkit:classify:cols");
    if (group.numel() != Ntr)
        throw Error("classify: group must have one entry per training row",
                    0, 0, "classify", "", "numkit:classify:group");

    std::string type = type_in;
    for (auto &c : type) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (type.empty()) type = "linear";
    const bool quadratic = (type == "quadratic" || type == "diagquadratic");
    const bool diagonal  = (type == "diaglinear" || type == "diagquadratic");
    if (type != "linear" && type != "diaglinear"
        && type != "quadratic" && type != "diagquadratic")
        throw Error("classify: unsupported type (linear|diaglinear|quadratic|diagquadratic)",
                    0, 0, "classify", "", "numkit:classify:type");

    // Identify unique group labels (sorted ascending).
    std::vector<double> labels;
    labels.reserve(Ntr);
    for (size_t i = 0; i < Ntr; ++i) labels.push_back(group.elemAsDouble(i));
    std::vector<double> uniq = labels;
    std::sort(uniq.begin(), uniq.end());
    uniq.erase(std::unique(uniq.begin(), uniq.end()), uniq.end());
    const size_t K = uniq.size();

    // Class counts and means.
    std::vector<size_t> n_k(K, 0);
    std::vector<double> mu(K * d, 0.0);
    for (size_t i = 0; i < Ntr; ++i) {
        const auto it = std::find(uniq.begin(), uniq.end(), labels[i]);
        const size_t k = static_cast<size_t>(it - uniq.begin());
        ++n_k[k];
        for (size_t j = 0; j < d; ++j)
            mu[j + k * d] += training.elemAsDouble(i + j * Ntr);
    }
    for (size_t k = 0; k < K; ++k)
        for (size_t j = 0; j < d; ++j)
            mu[j + k * d] /= double(n_k[k]);

    // Covariance matrix/matrices (pooled or per-class).
    std::vector<double> Sigma((quadratic ? K : 1) * d * d, 0.0);

    auto sigBlock = [&](size_t k) -> double * {
        return Sigma.data() + (quadratic ? k * d * d : 0);
    };

    for (size_t i = 0; i < Ntr; ++i) {
        const auto it = std::find(uniq.begin(), uniq.end(), labels[i]);
        const size_t k = static_cast<size_t>(it - uniq.begin());
        std::vector<double> diff(d);
        for (size_t j = 0; j < d; ++j)
            diff[j] = training.elemAsDouble(i + j * Ntr) - mu[j + k * d];
        double *S = sigBlock(k);
        for (size_t a = 0; a < d; ++a)
            for (size_t b = 0; b < d; ++b)
                S[a + b * d] += diff[a] * diff[b];
    }
    if (quadratic) {
        for (size_t k = 0; k < K; ++k) {
            double *S = sigBlock(k);
            const double denom = (n_k[k] > 1) ? double(n_k[k] - 1) : 1.0;
            for (size_t i = 0; i < d * d; ++i) S[i] /= denom;
        }
    } else {
        // Pool across classes.
        const double denom = (Ntr > K) ? double(Ntr - K) : 1.0;
        double *S = Sigma.data();
        for (size_t i = 0; i < d * d; ++i) S[i] /= denom;
    }

    // For diagonal variants, zero off-diagonals.
    if (diagonal) {
        const size_t blocks = quadratic ? K : 1;
        for (size_t k = 0; k < blocks; ++k) {
            double *S = sigBlock(k);
            for (size_t a = 0; a < d; ++a)
                for (size_t b = 0; b < d; ++b)
                    if (a != b) S[a + b * d] = 0.0;
        }
    }

    // Pre-factor each covariance block.
    const size_t blocks = quadratic ? K : 1;
    std::vector<double> Lblocks(blocks * d * d, 0.0);
    std::vector<double> sumLogDiag(blocks, 0.0);
    for (size_t k = 0; k < blocks; ++k) {
        double *L = Lblocks.data() + k * d * d;
        if (!cholesky(sigBlock(k), L, d))
            throw Error("classify: covariance is not positive definite",
                        0, 0, "classify", "", "numkit:classify:psd");
        double s = 0.0;
        for (size_t i = 0; i < d; ++i) s += std::log(L[i + i * d]);
        sumLogDiag[k] = s;
    }

    // Prior: empirical n_k / Ntr.
    std::vector<double> logPrior(K);
    for (size_t k = 0; k < K; ++k) logPrior[k] = std::log(double(n_k[k]) / double(Ntr));

    // Iterate over sample rows; compute class log-likelihood, pick argmax.
    Value cV    = Value::matrix(Nsamp, 1, ValueType::DOUBLE, mr);
    Value postV = Value::matrix(Nsamp, K, ValueType::DOUBLE, mr);
    Value logpV = Value::matrix(Nsamp, 1, ValueType::DOUBLE, mr);
    double *cd = cV.doubleDataMut();
    double *pd = postV.doubleDataMut();
    double *lpd = logpV.doubleDataMut();

    std::vector<double> lp(K), z(d), x(d);
    for (size_t r = 0; r < Nsamp; ++r) {
        for (size_t j = 0; j < d; ++j) x[j] = sample.elemAsDouble(r + j * Nsamp);
        double maxLp = -std::numeric_limits<double>::infinity();
        size_t kBest = 0;
        for (size_t k = 0; k < K; ++k) {
            const double *L = Lblocks.data() + (quadratic ? k * d * d : 0);
            const double sld = quadratic ? sumLogDiag[k] : sumLogDiag[0];
            std::vector<double> diff(d);
            for (size_t j = 0; j < d; ++j) diff[j] = x[j] - mu[j + k * d];
            fwd_solve(L, z.data(), diff.data(), d);
            double q = 0.0;
            for (size_t j = 0; j < d; ++j) q += z[j] * z[j];
            // log f_k(x) = -0.5·d·log(2π) − sumLogDiag − 0.5·q
            // log discrim_k = log f_k(x) + log π_k
            const double logFk = -0.5 * double(d) * kLog2Pi - sld - 0.5 * q;
            lp[k] = logFk + logPrior[k];
            if (lp[k] > maxLp) { maxLp = lp[k]; kBest = k; }
        }
        cd[r] = uniq[kBest];

        // posterior + logp via log-sum-exp
        double sumExp = 0.0;
        for (size_t k = 0; k < K; ++k) sumExp += std::exp(lp[k] - maxLp);
        const double logSum = maxLp + std::log(sumExp);
        for (size_t k = 0; k < K; ++k)
            pd[r + k * Nsamp] = std::exp(lp[k] - logSum);
        lpd[r] = logSum;
    }

    // Apparent training error rate.
    double mis = 0.0;
    {
        std::vector<double> z2(d), x2(d);
        for (size_t i = 0; i < Ntr; ++i) {
            for (size_t j = 0; j < d; ++j) x2[j] = training.elemAsDouble(i + j * Ntr);
            double maxLp = -std::numeric_limits<double>::infinity();
            size_t kBest = 0;
            for (size_t k = 0; k < K; ++k) {
                const double *L = Lblocks.data() + (quadratic ? k * d * d : 0);
                const double sld = quadratic ? sumLogDiag[k] : sumLogDiag[0];
                std::vector<double> diff(d);
                for (size_t j = 0; j < d; ++j) diff[j] = x2[j] - mu[j + k * d];
                fwd_solve(L, z2.data(), diff.data(), d);
                double q = 0.0;
                for (size_t j = 0; j < d; ++j) q += z2[j] * z2[j];
                const double logFk = -0.5 * double(d) * kLog2Pi - sld - 0.5 * q;
                const double l = logFk + logPrior[k];
                if (l > maxLp) { maxLp = l; kBest = k; }
            }
            if (uniq[kBest] != labels[i]) mis += 1.0;
        }
    }
    Value errV = Value::scalar(mis / double(Ntr), mr);

    return {std::move(cV), std::move(errV), std::move(postV), std::move(logpV)};
}

// ════════════════════════════════════════════════════════════════════
// Engine adapter
// ════════════════════════════════════════════════════════════════════

namespace detail {

void classify_reg(Span<const Value> args, size_t nargout,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("classify: requires (sample, training, group[, type])",
                    0, 0, "classify", "", "numkit:classify:nargin");
    std::string type;
    if (args.size() >= 4 && (args[3].isChar() || args[3].isString()))
        type = args[3].toString();
    auto [c, err, post, logp] = classify(args[0], args[1], args[2], type, ctx.engine->resource());
    outs[0] = std::move(c);
    if (nargout > 1) outs[1] = std::move(err);
    if (nargout > 2) outs[2] = std::move(post);
    if (nargout > 3) outs[3] = std::move(logp);
}

} // namespace detail
} // namespace numkit::stats
