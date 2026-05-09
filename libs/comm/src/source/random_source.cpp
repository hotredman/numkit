// libs/comm/src/source/random_source.cpp
//
// Random data sources: randsrc.
// Future: randerr planned for the same TU (random binary error matrix).

#include <numkit/comm/source/random_source.hpp>

#include <numkit/builtin/math/random/rng.hpp>
#include <numkit/builtin/math/random/matlab_mt19937.hpp>
#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include <cmath>
#include <mutex>

namespace numkit::comm {

namespace {

// Gather alphabet + cumulative-probability arrays from the alphabet
// argument. MATLAB accepts:
//   row vector       -> alphabet, uniform probabilities
//   2-row matrix     -> [alphabet; probabilities]
// Returns the symbol count K via len_out.
void parseAlphabet(const Value &a,
                   std::pmr::memory_resource *mr,
                   double **alpha_out, double **prob_out, size_t *len_out)
{
    const size_t R = a.dims().rows();
    const size_t C = a.dims().cols();

    if (R == 1 && C >= 1) {
        // Plain alphabet row -> uniform probs.
        const size_t K = C;
        auto *al = static_cast<double *>(
            mr->allocate(K * sizeof(double), alignof(double)));
        auto *pr = static_cast<double *>(
            mr->allocate(K * sizeof(double), alignof(double)));
        for (size_t k = 0; k < K; ++k) {
            al[k] = a.elemAsDouble(k);
            pr[k] = static_cast<double>(k + 1) / static_cast<double>(K);
        }
        *alpha_out = al;
        *prob_out  = pr;
        *len_out   = K;
        return;
    }

    if (R == 2 && C >= 1) {
        const size_t K = C;
        auto *al = static_cast<double *>(
            mr->allocate(K * sizeof(double), alignof(double)));
        auto *pr = static_cast<double *>(
            mr->allocate(K * sizeof(double), alignof(double)));
        // Column-major: alphabet[k] = a(0, k) at index 0 + k*R = k*2;
        // prob[k] = a(1, k) at index 1 + k*2.
        double sum = 0.0;
        for (size_t k = 0; k < K; ++k) {
            const double sym = a.elemAsDouble(k * R + 0);
            const double pk  = a.elemAsDouble(k * R + 1);
            if (!(pk >= 0.0 && pk <= 1.0))
                throw Error("randsrc: probabilities must be in [0, 1]",
                            0, 0, "randsrc", "",
                            "m:randsrc:InvalidProbabilityVal");
            al[k] = sym;
            sum  += pk;
            pr[k] = sum;          // cumulative
        }
        if (std::abs(sum - 1.0) > std::sqrt(2.220446049250313e-16))
            throw Error("randsrc: probabilities must sum to 1",
                        0, 0, "randsrc", "",
                        "m:randsrc:InvalidProbabilitySum");
        *alpha_out = al;
        *prob_out  = pr;
        *len_out   = K;
        return;
    }

    if (R == 1 && C == 0)
        throw Error("randsrc: alphabet must be non-empty",
                    0, 0, "randsrc", "", "m:randsrc:EmptyAlphabet");

    throw Error("randsrc: alphabet must be a row vector or 2-row matrix",
                0, 0, "randsrc", "", "m:randsrc:InvalidAlphabet");
}

} // namespace

Value randsrc(std::pmr::memory_resource *mr, size_t m, size_t n,
              const Value &alphabet, bool have_state, uint32_t state)
{
    using ::numkit::builtin::detail::MatlabMT19937;

    // Parse alphabet → cumulative probability table.
    double *alpha = nullptr;
    double *prob  = nullptr;
    size_t  K     = 0;
    parseAlphabet(alphabet, mr, &alpha, &prob, &K);

    Value out = Value::matrix(m, n, ValueType::DOUBLE, mr);
    double *o = out.doubleDataMut();
    const size_t N = m * n;

    // Choose generator. With explicit state -> dedicated engine for
    // reproducibility (matches MATLAB's RandStream(...,'Seed',state)).
    if (have_state) {
        MatlabMT19937 local;
        local.seed(state);
        for (size_t i = 0; i < N; ++i) {
            const double r = local.genRes53();
            // idx = 1 + sum(r >= prob(k)) → (1-based) → 0-based = sum(r >= prob).
            size_t idx = 0;
            while (idx < K - 1 && r >= prob[idx]) ++idx;
            o[i] = alpha[idx];
        }
    } else {
        auto &gen = ::numkit::builtin::sharedEngine();
        auto &mtx = ::numkit::builtin::rngMutex();
        std::lock_guard<std::mutex> lk(mtx);
        for (size_t i = 0; i < N; ++i) {
            const double r = gen.genRes53();
            size_t idx = 0;
            while (idx < K - 1 && r >= prob[idx]) ++idx;
            o[i] = alpha[idx];
        }
    }

    return out;
}

namespace detail {

void randsrc_reg(Span<const Value> args, size_t /*nargout*/,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("randsrc: requires (m, n [, alphabet [, state]])",
                    0, 0, "randsrc", "", "m:randsrc:nargin");
    const size_t m = static_cast<size_t>(args[0].toScalar());
    const size_t n = static_cast<size_t>(args[1].toScalar());
    auto *mr = ctx.engine->resource();

    // Default alphabet = [-1, 1] if not provided.
    Value default_alphabet;
    const Value *alphabet = nullptr;
    if (args.size() >= 3 && !args[2].isEmpty()) {
        alphabet = &args[2];
    } else {
        default_alphabet = Value::matrix(1, 2, ValueType::DOUBLE, mr);
        double *d = default_alphabet.doubleDataMut();
        d[0] = -1.0;
        d[1] =  1.0;
        alphabet = &default_alphabet;
    }

    bool have_state = false;
    uint32_t state = 0;
    if (args.size() >= 4 && !args[3].isEmpty()) {
        have_state = true;
        const double s = args[3].toScalar();
        if (s < 0.0)
            throw Error("randsrc: state must be a non-negative integer",
                        0, 0, "randsrc", "", "m:randsrc:InvalidState");
        state = static_cast<uint32_t>(s);
    }

    outs[0] = randsrc(mr, m, n, *alphabet, have_state, state);
}

} // namespace detail

} // namespace numkit::comm
