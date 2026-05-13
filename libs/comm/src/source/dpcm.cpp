// libs/comm/src/source/dpcm.cpp
//
// Differential Pulse Code Modulation: encoder + decoder.
//
//   [indx, quanterr] = dpcmenco(sig, codebook, partition, predictor)
//   [sig,  quanterr] = dpcmdeco(indx, codebook, predictor)
//
// dpcmopt deferred -- it is a training-set optimisation step that
// alternates Lloyd-Max quantizer design with predictor estimation.
// Heavier than the encoder/decoder; will get its own cycle.
//
// Predictor format mirrors MATLAB: `[0, n1, n2, ..., nM]` (length
// M+1). The leading 0 is a placeholder; the M-tap FIR predictor
// uses elements (2:end). Prediction step: out = predictor[1..M] · x
// where x is the most-recent M reconstructed samples (newest first).

#include <numkit/comm/source/dpcm.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include <vector>

namespace numkit::comm {

namespace {

// Find quantization bin index: smallest i such that e < partition[i],
// or K-1 if e >= partition[K-2] (K-1 thresholds for K bins).
size_t partitionBin(const Value &partition, double e)
{
    const size_t M = partition.numel();
    size_t i = 0;
    while (i < M && partition.elemAsDouble(i) < e) ++i;
    return i;
}

void readPredictor(const Value &predictor, std::vector<double> &p)
{
    const size_t L = predictor.numel();
    if (L < 2)
        throw Error("dpcm: predictor must have at least 2 elements",
                    0, 0, "dpcm", "", "m:dpcm:InvalidPredictor");
    p.resize(L - 1);
    // Skip the leading 0 (predictor[0] in MATLAB's 1-based form).
    for (size_t k = 0; k < L - 1; ++k)
        p[k] = predictor.elemAsDouble(k + 1);
}

bool isRow(const Value &v)
{
    return v.dims().rows() == 1 && v.dims().cols() >= 1;
}

} // namespace

std::pair<Value, Value>
dpcmenco(const Value &sig, const Value &codebook,
         const Value &partition, const Value &predictor,
         std::pmr::memory_resource *mr)
{
    if (codebook.numel() != partition.numel() + 1)
        throw Error("dpcmenco: length(codebook) must equal "
                    "length(partition) + 1",
                    0, 0, "dpcmenco", "",
                    "m:dpcmenco:InvalidPartitionSize");
    std::vector<double> pred;
    readPredictor(predictor, pred);
    const size_t M = pred.size();

    const size_t N = sig.numel();
    const bool row = isRow(sig);
    Value indx_v     = Value::matrix(row ? 1 : N, row ? N : 1,
                                     ValueType::DOUBLE, mr);
    Value quanterr_v = Value::matrix(row ? 1 : N, row ? N : 1,
                                     ValueType::DOUBLE, mr);
    double *o_indx = indx_v.doubleDataMut();
    double *o_qerr = quanterr_v.doubleDataMut();

    std::vector<double> x(M, 0.0);  // predictor state (newest first)
    for (size_t i = 0; i < N; ++i) {
        double out = 0.0;
        for (size_t k = 0; k < M; ++k) out += pred[k] * x[k];
        const double e   = sig.elemAsDouble(i) - out;
        const size_t bin = partitionBin(partition, e);
        const double q   = codebook.elemAsDouble(bin);
        o_indx[i] = static_cast<double>(bin);
        o_qerr[i] = q;
        const double inp = q + out;
        // Shift state: x = [inp; x(1..M-1)]
        for (size_t k = M; k-- > 1;) x[k] = x[k - 1];
        if (M > 0) x[0] = inp;
    }
    return {std::move(indx_v), std::move(quanterr_v)};
}

std::pair<Value, Value>
dpcmdeco(const Value &indx, const Value &codebook,
         const Value &predictor,
         std::pmr::memory_resource *mr)
{
    std::vector<double> pred;
    readPredictor(predictor, pred);
    const size_t M = pred.size();

    const size_t K = codebook.numel();
    const size_t N = indx.numel();
    const bool row = isRow(indx);
    Value sig_v      = Value::matrix(row ? 1 : N, row ? N : 1,
                                     ValueType::DOUBLE, mr);
    Value quanterr_v = Value::matrix(row ? 1 : N, row ? N : 1,
                                     ValueType::DOUBLE, mr);
    double *o_sig  = sig_v.doubleDataMut();
    double *o_qerr = quanterr_v.doubleDataMut();

    // Lookup quanterr first (matches MATLAB ordering of work).
    for (size_t i = 0; i < N; ++i) {
        const double iv = indx.elemAsDouble(i);
        if (!(iv >= 0.0) || iv >= static_cast<double>(K))
            throw Error("dpcmdeco: index out of codebook range",
                        0, 0, "dpcmdeco", "", "m:dpcmdeco:OutOfRange");
        o_qerr[i] = codebook.elemAsDouble(static_cast<size_t>(iv));
    }

    std::vector<double> x(M, 0.0);
    for (size_t i = 0; i < N; ++i) {
        double out = 0.0;
        for (size_t k = 0; k < M; ++k) out += pred[k] * x[k];
        o_sig[i] = o_qerr[i] + out;
        for (size_t k = M; k-- > 1;) x[k] = x[k - 1];
        if (M > 0) x[0] = o_sig[i];
    }
    return {std::move(sig_v), std::move(quanterr_v)};
}

namespace detail {

void dpcmenco_reg(Span<const Value> args, size_t nargout,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 4)
        throw Error("dpcmenco: requires (sig, codebook, partition, predictor)",
                    0, 0, "dpcmenco", "", "m:dpcmenco:nargin");
    auto *mr = ctx.engine->resource();
    auto [indx, quanterr] = dpcmenco(args[0], args[1], args[2], args[3], mr);
    outs[0] = std::move(indx);
    if (nargout > 1) outs[1] = std::move(quanterr);
}

void dpcmdeco_reg(Span<const Value> args, size_t nargout,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("dpcmdeco: requires (indx, codebook, predictor)",
                    0, 0, "dpcmdeco", "", "m:dpcmdeco:nargin");
    auto *mr = ctx.engine->resource();
    auto [sig, quanterr] = dpcmdeco(args[0], args[1], args[2], mr);
    outs[0] = std::move(sig);
    if (nargout > 1) outs[1] = std::move(quanterr);
}

} // namespace detail

} // namespace numkit::comm
