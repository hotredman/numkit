// libs/comm/src/source/dpcmopt.cpp
// DPCM parameter optimiser — clean-room reimplementation.
// Designs an FIR linear predictor of order `ord` from a training
// signal by the autocorrelation method (Yule-Walker normal equations
// solved by the Levinson-Durbin recursion), and optionally a Lloyd-Max
// scalar quantiser for the resulting prediction residual.
// Clean-room implementation written from public references and
// the public references it cites:
//  - J. Makhoul, "Linear Prediction: A Tutorial Review",
//    Proc. IEEE 63(4):561-580, 1975 (autocorrelation method +
//    Levinson-Durbin recursion);
//  - J. G. Proakis & D. G. Manolakis, "Digital Signal Processing",
//    4th ed., 2007 (Levinson-Durbin algorithm);
//  - N. S. Jayant & P. Noll, "Digital Coding of Waveforms", 1984
//    (DPCM predictive coding, residual quantisation);
//  - S. P. Lloyd, "Least Squares Quantization in PCM",
//    IEEE Trans. Inf. Theory 28(2):129-137, 1982.

#include <numkit/comm/source/dpcmopt.hpp>
#include <numkit/comm/source/lloyds.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/core/types.hpp>
#include <numkit/value/value.hpp>

#include <cstddef>
#include <tuple>
#include <utility>

namespace numkit::comm {

namespace {

// Estimate the autocorrelation of `x` (length N) for lags 0..ord using
// the unbiased, sample-variance-style estimator
//   r[k] = ( sum_{n=0}^{N-1-k} x[n]*x[n+k] ) / (N - 1 - k)
// The lag-k sum has N-k terms; the denominator is N-1-k (one less than
// the term count). The caller guarantees N >= ord + 3 so every
// denominator (N-1-k for k <= ord) is strictly positive.
void autocorrelation(const double *x, std::size_t N, int ord, double *r)
{
    for (int k = 0; k <= ord; ++k) {
        double acc = 0.0;
        const std::size_t last = N - static_cast<std::size_t>(k);  // N-k terms
        for (std::size_t n = 0; n < last; ++n)
            acc += x[n] * x[n + static_cast<std::size_t>(k)];
        r[k] = acc / static_cast<double>(N - 1 - static_cast<std::size_t>(k));
    }
}

// Levinson-Durbin recursion: solve the order-`ord` Yule-Walker normal
// equations for the prediction-error filter A(z) = [1, a1, ..., a_ord].
// `a` is initialised to [1, 0, ..., 0] and D <- r[0]; then for
// m = 0..ord-1:
//   beta = sum_{j=0}^{m} a[j] * r[m+1-j]
//   K    = -beta / D
//   a[1..m+1] += K * reverse(a[0..m])     (update in place)
//   D    = (1 - K*K) * D
// Scratch `prev` (length ord+1) holds a snapshot of `a` before the
// in-place update so the reversed term is read from the pre-update
// coefficients.
void levinson_durbin(const double *r, int ord, double *a, double *prev)
{
    for (int i = 0; i <= ord; ++i)
        a[i] = 0.0;
    a[0] = 1.0;

    double D = r[0];

    for (int m = 0; m < ord; ++m) {
        double beta = 0.0;
        for (int j = 0; j <= m; ++j)
            beta += a[j] * r[m + 1 - j];

        // D == 0 only for a degenerate (all-zero / constant) signal;
        // guard against a division by zero — K = 0 leaves `a` and D
        // unchanged, yielding a zero predictor tail.
        const double K = (D != 0.0) ? (-beta / D) : 0.0;

        for (int i = 0; i <= m; ++i)
            prev[i] = a[i];
        // a[1..m+1] += K * reverse(a[0..m])
        for (int i = 1; i <= m + 1; ++i)
            a[i] += K * prev[m + 1 - i];

        D = (1.0 - K * K) * D;
    }
}

}  // namespace

DpcmOptResult dpcmopt(const Value &training_set, int ord,
                      const Value &ini_codebook,
                      std::pmr::memory_resource *mr)
{
    // ── Argument validation ──────────────────────────────────────
    if (ord < 1)
        throw numkit::Error("The predictor order must be a positive "
                            "integer.",
                            0, 0, "dpcmopt", "", "numkit:dpcmopt:InvalidOrd");

    const std::size_t N = training_set.numel();
    // N must be at least ord + 3 so the smallest autocorrelation
    // denominator (N-1-ord) is >= 2.
    if (N < static_cast<std::size_t>(ord) + 3)
        throw numkit::Error("The size of the training set is not large "
                            "enough for the given predictor order.",
                            0, 0, "dpcmopt", "", "numkit:dpcmopt:InvalidInput");

    numkit::ScratchArena arena(mr);

    // ── Read the training signal into contiguous scratch ─────────
    // elemAsDouble accepts a row or column vector.
    numkit::ScratchVec<double> x(N, &arena);
    for (std::size_t i = 0; i < N; ++i)
        x[i] = training_set.elemAsDouble(i);

    // ── Autocorrelation estimate, lags 0..ord ────────────────────
    numkit::ScratchVec<double> r(static_cast<std::size_t>(ord) + 1, &arena);
    autocorrelation(x.data(), N, ord, r.data());

    // ── Levinson-Durbin: prediction-error filter A(z) ────────────
    // a = [1, a1, ..., a_ord]
    numkit::ScratchVec<double> a(static_cast<std::size_t>(ord) + 1, &arena);
    numkit::ScratchVec<double> prev(static_cast<std::size_t>(ord) + 1,
                                    &arena);
    levinson_durbin(r.data(), ord, a.data(), prev.data());

    // ── Predictor = [0, -a1, -a2, ..., -a_ord] ───────────────────
    // The leading 0 occupies the "current sample" slot; predictor[k]
    // is the weight on x[n-k].
    DpcmOptResult result;
    result.predictor = Value::matrix(1, static_cast<std::size_t>(ord) + 1,
                                     ValueType::DOUBLE, mr);
    {
        double *p = result.predictor.doubleDataMut();
        p[0] = 0.0;
        for (int k = 1; k <= ord; ++k)
            p[k] = -a[k];
    }

    // ── Optional quantiser on the prediction residual ────────────
    if (ini_codebook.isEmpty()) {
        // No third argument: codebook / partition stay empty.
        return result;
    }

    // Prediction residual, length N - ord:
    //   e[i-ord] = x[i] - sum_{k=1}^{ord} predictor[k] * x[i-k]
    const std::size_t resLen = N - static_cast<std::size_t>(ord);
    const double *p = result.predictor.doubleData();

    Value residual = Value::matrix(1, resLen, ValueType::DOUBLE, mr);
    {
        double *e = residual.doubleDataMut();
        for (std::size_t i = static_cast<std::size_t>(ord); i < N; ++i) {
            double pred = 0.0;
            for (int k = 1; k <= ord; ++k)
                pred += p[k] * x[i - static_cast<std::size_t>(k)];
            e[i - static_cast<std::size_t>(ord)] = x[i] - pred;
        }
    }

    // Lloyd-Max quantiser. `lloyds` returns (partition, codebook,
    // distor, rel); ini_codebook is forwarded as-is — lloyds itself
    // disambiguates a scalar codebook-length from an explicit codebook.
    auto [partition, codebook, distor, rel] =
        numkit::comm::lloyds(residual, ini_codebook, 1e-7, mr);
    (void)distor;
    (void)rel;

    result.partition = std::move(partition);
    result.codebook = std::move(codebook);
    return result;
}

namespace detail {

void dpcmopt_reg(Span<const Value> args, size_t nargout,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("dpcmopt: requires (training_set, ord [, ini_codebook])",
                    0, 0, "dpcmopt", "", "numkit:dpcmopt:nargin");
    auto *mr = ctx.engine->resource();
    const int ord = static_cast<int>(args[1].toScalar());

    const Value &ini = (args.size() >= 3 && !args[2].isEmpty())
                          ? args[2] : Value::Empty;
    if (ini.isEmpty() && nargout > 1)
        throw Error("dpcmopt: ini_codebook required for codebook/partition outputs",
                    0, 0, "dpcmopt", "", "numkit:dpcmopt:NeedIniCodebook");

    auto res = dpcmopt(args[0], ord, ini, mr);
    outs[0] = std::move(res.predictor);
    if (nargout > 1) outs[1] = std::move(res.codebook);
    if (nargout > 2) outs[2] = std::move(res.partition);
}

} // namespace detail

} // namespace numkit::comm
