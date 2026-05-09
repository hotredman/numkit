// libs/comm/src/source/lloyds.cpp
//
// Lloyd-Max scalar quantizer designer.
//
//   [partition, codebook, distor, rel] = lloyds(training, ini_codebook [, tol])
//
// Algorithm: alternate centroid update (each codebook entry becomes the
// mean of its bin) with partition update (midpoint between consecutive
// codebook entries) until the relative distortion change drops below
// `tol` (default 1e-7) or the absolute distortion drops below
// eps*max(training).
//
// `ini_codebook` may be either an integer K (initialise K linspace-
// centred codebook entries across the training range) or a vector
// (used directly, sorted ascending).
//
// Plotting form (4-arg) deferred -- numkit doesn't ship plot().

#include <numkit/comm/source/lloyds.hpp>
#include <numkit/comm/source/quantiz.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace numkit::comm {

namespace {

// Fill out_v with double values sourced from the training-set Value.
void readVector(const Value &v, std::vector<double> &out)
{
    const size_t N = v.numel();
    out.resize(N);
    for (size_t i = 0; i < N; ++i) out[i] = v.elemAsDouble(i);
}

// Build a Value(1, n, DOUBLE) from a std::vector<double> on `mr`.
Value vecToRow(std::pmr::memory_resource *mr, const std::vector<double> &v)
{
    Value out = Value::matrix(1, v.size(), ValueType::DOUBLE, mr);
    if (!v.empty()) std::copy(v.begin(), v.end(), out.doubleDataMut());
    return out;
}

} // namespace

std::tuple<Value, Value, double, double>
lloyds(std::pmr::memory_resource *mr, const Value &training_set,
       const Value &ini_codebook, double tol)
{
    std::vector<double> training;
    readVector(training_set, training);
    if (training.empty())
        throw Error("lloyds: training set must be non-empty",
                    0, 0, "lloyds", "", "m:lloyds:EmptyTRAINING_SET");
    const double min_train =
        *std::min_element(training.begin(), training.end());
    const double max_train =
        *std::max_element(training.begin(), training.end());
    if (max_train <= min_train)
        throw Error("lloyds: training set must span a positive range",
                    0, 0, "lloyds", "", "m:lloyds:InvalidTRAINING_SET");

    // Build initial codebook.
    std::vector<double> codebook;
    if (ini_codebook.numel() == 1) {
        const double Kd = ini_codebook.toScalar();
        const long K = static_cast<long>(std::floor(Kd));
        if (K < 1)
            throw Error("lloyds: ini_codebook count must be positive",
                        0, 0, "lloyds", "",
                        "m:lloyds:NonPositiveINI_CODEBOOK");
        const double step = (max_train - min_train) / static_cast<double>(K);
        codebook.resize(static_cast<size_t>(K));
        for (long i = 0; i < K; ++i) {
            codebook[static_cast<size_t>(i)] =
                min_train + step * 0.5 + step * static_cast<double>(i);
        }
    } else {
        readVector(ini_codebook, codebook);
        std::sort(codebook.begin(), codebook.end());
    }
    const size_t K = codebook.size();
    if (K < 1)
        throw Error("lloyds: ini_codebook must have at least 1 entry",
                    0, 0, "lloyds", "", "m:lloyds:InvalidINI_CODEBOOK");

    if (tol <= 0.0) tol = 1e-7;

    // Helper: midpoint partition from codebook.
    auto midpoint = [&]() -> std::vector<double> {
        std::vector<double> p(K > 0 ? K - 1 : 0);
        for (size_t i = 0; i + 1 < K; ++i) {
            p[i] = 0.5 * (codebook[i] + codebook[i + 1]);
        }
        std::sort(p.begin(), p.end());
        return p;
    };

    // Helper: classify training samples into bins given partition.
    // bin index = sum(partition < x).
    auto classify = [&](const std::vector<double> &part,
                        std::vector<size_t> &bins) {
        bins.resize(training.size());
        for (size_t i = 0; i < training.size(); ++i) {
            const double x = training[i];
            size_t b = 0;
            while (b < part.size() && part[b] < x) ++b;
            bins[i] = b;
        }
    };

    // Helper: distortion = mean((x - codebook(bin))^2).
    auto distortion = [&](const std::vector<size_t> &bins) {
        double sse = 0.0;
        for (size_t i = 0; i < training.size(); ++i) {
            const double d = training[i] - codebook[bins[i]];
            sse += d * d;
        }
        return sse / static_cast<double>(training.size());
    };

    std::vector<double> partition = midpoint();
    std::vector<size_t> bins;
    classify(partition, bins);
    double distor      = distortion(bins);
    double last_distor = 0.0;

    const double ter_cond2 = std::numeric_limits<double>::epsilon()
                             * std::abs(max_train);
    double rel_distor = (distor > ter_cond2)
                            ? std::abs(distor - last_distor) / distor
                            : distor;

    // Iterate.
    while (rel_distor > tol && rel_distor > ter_cond2) {
        // Centroid update per bin.
        for (size_t i = 0; i < K; ++i) {
            double sum = 0.0;
            size_t cnt = 0;
            for (size_t j = 0; j < training.size(); ++j) {
                if (bins[j] == i) { sum += training[j]; ++cnt; }
            }
            if (cnt > 0) {
                codebook[i] = sum / static_cast<double>(cnt);
            } else {
                // Empty bin: per MATLAB lloyds.m, fall back to a midpoint
                // between adjacent partition boundaries (or training extrema).
                if (i == 0) {
                    if (partition.empty()) {
                        codebook[i] = (max_train + min_train) * 0.5;
                    } else {
                        // Try mean of training_set <= partition[0]
                        double s = 0.0; size_t c = 0;
                        for (double x : training)
                            if (x <= partition[0]) { s += x; ++c; }
                        codebook[i] = c ? s / c
                                        : 0.5 * (partition[0] + min_train);
                    }
                } else if (i == K - 1) {
                    double s = 0.0; size_t c = 0;
                    for (double x : training)
                        if (x >= partition[i - 1]) { s += x; ++c; }
                    codebook[i] = c ? s / c
                                    : 0.5 * (max_train + partition[i - 1]);
                } else {
                    double s = 0.0; size_t c = 0;
                    for (double x : training)
                        if (x >= partition[i - 1] && x <= partition[i]) {
                            s += x; ++c;
                        }
                    codebook[i] = c ? s / c
                                    : 0.5 * (partition[i] + partition[i - 1]);
                }
            }
        }
        partition = midpoint();
        classify(partition, bins);
        last_distor = distor;
        distor      = distortion(bins);
        rel_distor  = (distor > ter_cond2)
                          ? std::abs(distor - last_distor) / distor
                          : distor;
    }

    return {vecToRow(mr, partition), vecToRow(mr, codebook),
            distor, rel_distor};
}

namespace detail {

void lloyds_reg(Span<const Value> args, size_t nargout,
                Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("lloyds: requires (training_set, ini_codebook [, tol])",
                    0, 0, "lloyds", "", "m:lloyds:nargin");
    auto *mr = ctx.engine->resource();
    double tol = 1e-7;
    if (args.size() >= 3 && !args[2].isEmpty())
        tol = args[2].toScalar();
    auto [partition, codebook, distor, rel] =
        lloyds(mr, args[0], args[1], tol);
    outs[0] = std::move(partition);
    if (nargout > 1) outs[1] = std::move(codebook);
    if (nargout > 2) outs[2] = Value::scalar(distor, mr);
    if (nargout > 3) outs[3] = Value::scalar(rel, mr);
}

} // namespace detail

} // namespace numkit::comm
