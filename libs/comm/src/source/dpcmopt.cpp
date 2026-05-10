// libs/comm/src/source/dpcmopt.cpp
//
// DPCM parameter optimiser:
//   predictor                       = dpcmopt(training, ord)
//   [predictor, codebook, partition] = dpcmopt(training, ord, ini_codebook)
//
// Algorithm (per MATLAB R2025b's dpcmopt.m):
//
//   1. Estimate autocorrelations r(i) for i = 1..ord+2 using
//      MATLAB's biased / "denominator (N - i + 1)" formula: in
//      the source it's `training(1:N-i+1)' * training(i:N) / (N-i)`.
//      Note: divisor is (N - i), NOT (N - i + 1). This is intentional.
//
//   2. Levinson-Durbin recursion to solve the Yule-Walker system,
//      producing the AR coefficients A(z).
//
//   3. The DPCM predictor is `P = [0, p1, ..., pM]` with
//      `p_k = -a_{k+1}` (after dropping the leading 1 of A(z)).
//
//   4. If a codebook is requested, run lloyds() on the prediction
//      residual to get optimal partition + codebook.

#include <numkit/comm/source/dpcmopt.hpp>
#include <numkit/comm/source/lloyds.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include <algorithm>
#include <cmath>
#include <vector>

namespace numkit::comm {

namespace {

void readVec(const Value &v, std::vector<double> &out)
{
    const size_t N = v.numel();
    out.resize(N);
    for (size_t i = 0; i < N; ++i) out[i] = v.elemAsDouble(i);
}

Value rowFrom(std::pmr::memory_resource *mr,
              const std::vector<double> &v)
{
    Value out = Value::matrix(1, v.size(), ValueType::DOUBLE, mr);
    if (!v.empty()) std::copy(v.begin(), v.end(), out.doubleDataMut());
    return out;
}

} // namespace

DpcmOptResult
dpcmopt(std::pmr::memory_resource *mr, const Value &training_set,
        int ord, const Value *ini_codebook)
{
    if (ord < 1)
        throw Error("dpcmopt: ord must be a positive integer",
                    0, 0, "dpcmopt", "", "m:dpcmopt:InvalidOrd");
    std::vector<double> training;
    readVec(training_set, training);
    const size_t N = training.size();
    if (N < static_cast<size_t>(ord + 3))
        throw Error("dpcmopt: training_set too short for given ord",
                    0, 0, "dpcmopt", "", "m:dpcmopt:InvalidInput");

    // ── Autocorrelation r(i) for i = 1..ord+2 (1-based) ──
    // r(i) = sum(training(1:N-i+1) .* training(i:N)) / (N - i)
    std::vector<double> r(static_cast<size_t>(ord + 2), 0.0);
    for (int i = 1; i <= ord + 2; ++i) {
        double s = 0.0;
        const size_t L = N - static_cast<size_t>(i - 1);   // length(1:N-i+1)
        for (size_t j = 0; j < L; ++j)
            s += training[j] * training[j + static_cast<size_t>(i - 1)];
        r[static_cast<size_t>(i - 1)] =
            s / static_cast<double>(N - static_cast<size_t>(i));
    }

    // ── Levinson-Durbin recursion ──
    // predictor (1-based) = [1, 0, 0, ..., 0]; length ord+1.
    std::vector<double> predictor(static_cast<size_t>(ord + 1), 0.0);
    predictor[0] = 1.0;
    double D = r[0];
    for (int m = 0; m < ord; ++m) {
        // beta = predictor(1:m+1) * r(m+2:-1:2)
        // i.e. dot(predictor[0..m], r[m+1..1] reversed)
        double beta = 0.0;
        for (int j = 0; j <= m; ++j)
            beta += predictor[static_cast<size_t>(j)]
                  * r[static_cast<size_t>(m + 1 - j)];
        const double K = -beta / D;
        // predictor(2:m+2) += K * predictor(m+1:-1:1)
        // i.e. predictor[1..m+1] += K * reverse(predictor[0..m])
        std::vector<double> upd(static_cast<size_t>(m + 1));
        for (int j = 0; j <= m; ++j)
            upd[static_cast<size_t>(j)] =
                predictor[static_cast<size_t>(m - j)];
        for (int j = 0; j <= m; ++j)
            predictor[static_cast<size_t>(j + 1)] +=
                K * upd[static_cast<size_t>(j)];
        D = (1.0 - K * K) * D;
    }

    // ── DPCM predictor: P = [0, p1, ..., pM] = -A(z) with leading 0 ──
    predictor[0] = 0.0;
    for (auto &p : predictor) p = -p;

    DpcmOptResult res;
    res.predictor = rowFrom(mr, predictor);

    if (ini_codebook) {
        // Compute prediction residual.
        // err(k) = training(ord+1+k) - predictor * training(ord+1+k:-1:1+k)
        // 0-based: for i = ord..N-1,
        //   err[i - ord] = training[i] - dot(predictor[1..ord], training[i-1..i-ord])
        std::vector<double> err(N - static_cast<size_t>(ord));
        for (size_t i = static_cast<size_t>(ord); i < N; ++i) {
            double pred = 0.0;
            for (int k = 1; k <= ord; ++k)
                pred += predictor[static_cast<size_t>(k)]
                      * training[i - static_cast<size_t>(k)];
            err[i - static_cast<size_t>(ord)] = training[i] - pred;
        }
        Value err_v = rowFrom(mr, err);
        auto [partition, codebook, distor, rel] =
            lloyds(mr, err_v, *ini_codebook, 1e-7);
        res.codebook  = std::move(codebook);
        res.partition = std::move(partition);
    }
    return res;
}

namespace detail {

void dpcmopt_reg(Span<const Value> args, size_t nargout,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("dpcmopt: requires (training_set, ord [, ini_codebook])",
                    0, 0, "dpcmopt", "", "m:dpcmopt:nargin");
    auto *mr = ctx.engine->resource();
    const int ord = static_cast<int>(args[1].toScalar());

    const Value *ini = nullptr;
    if (args.size() >= 3 && !args[2].isEmpty()) {
        ini = &args[2];
    } else if (nargout > 1) {
        throw Error("dpcmopt: ini_codebook required for codebook/partition outputs",
                    0, 0, "dpcmopt", "", "m:dpcmopt:NeedIniCodebook");
    }

    auto res = dpcmopt(mr, args[0], ord, ini);
    outs[0] = std::move(res.predictor);
    if (nargout > 1) outs[1] = std::move(res.codebook);
    if (nargout > 2) outs[2] = std::move(res.partition);
}

} // namespace detail

} // namespace numkit::comm
