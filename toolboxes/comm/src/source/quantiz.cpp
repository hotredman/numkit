// toolboxes/comm/src/source/quantiz.cpp
//
// Scalar quantizer applier (companion to dpcmenco's inline bin search).
//
//   indx                       = quantiz(sig, partition)
//   [indx, quantv]             = quantiz(sig, partition, codebook)
//   [indx, quantv, distor]     = quantiz(sig, partition, codebook)
//
// indx(i) = sum(partition < sig(i)) ∈ [0, K-1]   (K = numel(codebook))
// quantv  = codebook(indx + 1)
// distor  = mean((sig - quantv).^2)

#include <numkit/comm/source/quantiz.hpp>

#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>

#include <utility>

namespace numkit::comm {

namespace {

bool isRow(const Value &v)
{
    return v.dims().rows() == 1 && v.dims().cols() >= 1;
}

} // namespace

Value quantiz_indx(const Value &sig, const Value &partition,
                   std::pmr::memory_resource *mr)
{
    const size_t N = sig.numel();
    const size_t M = partition.numel();
    const bool row = isRow(sig);
    Value indx = Value::matrix(row ? 1 : N, row ? N : 1,
                               ValueType::DOUBLE, mr);
    double *o = indx.doubleDataMut();
    for (size_t i = 0; i < N; ++i) {
        const double xi = sig.elemAsDouble(i);
        size_t cnt = 0;
        for (size_t k = 0; k < M; ++k) {
            if (partition.elemAsDouble(k) < xi) ++cnt;
            else break;   // partition is monotonic non-decreasing
        }
        o[i] = static_cast<double>(cnt);
    }
    return indx;
}

QuantizResult
quantiz(const Value &sig, const Value &partition, const Value &codebook,
        std::pmr::memory_resource *mr)
{
    const size_t N = sig.numel();
    if (codebook.numel() != partition.numel() + 1)
        throw Error("quantiz: length(codebook) must equal "
                    "length(partition) + 1",
                    0, 0, "quantiz", "", "numkit:quantiz:invalidCodebook");

    Value indx = quantiz_indx(sig, partition, mr);
    const double *idx = indx.doubleData();

    Value quantv = Value::matrix(indx.dims().rows(), indx.dims().cols(),
                                 ValueType::DOUBLE, mr);
    double *q = quantv.doubleDataMut();
    double sse = 0.0;
    for (size_t i = 0; i < N; ++i) {
        const size_t k = static_cast<size_t>(idx[i]);
        const double cv = codebook.elemAsDouble(k);
        q[i] = cv;
        const double d = sig.elemAsDouble(i) - cv;
        sse += d * d;
    }
    return {std::move(indx), std::move(quantv),
            N > 0 ? sse / static_cast<double>(N) : 0.0};
}

} // namespace numkit::comm
