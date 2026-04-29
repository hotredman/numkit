// libs/stats/src/moments/moments.cpp
//
// skewness + kurtosis. Extracted from libs/builtin/src/stats.cpp
// during the MATLAB-taxonomy refactor — they're Statistics Toolbox
// content (var/std/median stay in libs/builtin as base MATLAB).

#include <numkit/stats/moments/moments.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include "helpers.hpp"
#include "reduction_helpers.hpp"

#include <cmath>
#include <utility>

namespace numkit::stats {

using ::numkit::builtin::detail::applyAlongDim;
using ::numkit::builtin::detail::resolveDim;
using ::numkit::createForDims;

namespace {

// Cast a DOUBLE result to SINGLE in place. Used to preserve SINGLE
// input type — arithmetic happens at double precision then narrows.
Value narrowToSingle(Value d, std::pmr::memory_resource *mr)
{
    if (d.type() != ValueType::DOUBLE) return d;
    Value r = createForDims(d.dims(), ValueType::SINGLE, mr);
    const double *src = d.doubleData();
    float *dst = r.singleDataMut();
    for (size_t i = 0; i < d.numel(); ++i)
        dst[i] = static_cast<float>(src[i]);
    return r;
}

// Per-slice skewness. n < 2: NaN (m2 == 0 by definition). normFlag=0
// requires n >= 3 for the bias correction; otherwise NaN.
double skewnessFromSlice(const double *data, size_t n, int normFlag)
{
    if (n < 2) return std::nan("");
    double mean = 0.0;
    for (size_t i = 0; i < n; ++i) mean += data[i];
    mean /= static_cast<double>(n);
    double m2 = 0.0, m3 = 0.0;
    for (size_t i = 0; i < n; ++i) {
        const double d = data[i] - mean;
        const double d2 = d * d;
        m2 += d2;
        m3 += d2 * d;
    }
    m2 /= static_cast<double>(n);
    m3 /= static_cast<double>(n);
    if (m2 <= 0.0) return std::nan("");
    double y = m3 / std::pow(m2, 1.5);
    if (normFlag == 0) {
        if (n < 3) return std::nan("");
        const double nd = static_cast<double>(n);
        y *= std::sqrt(nd * (nd - 1.0)) / (nd - 2.0);
    }
    return y;
}

// Per-slice kurtosis (NON-excess; equals 3 for normal). normFlag=0
// requires n >= 4 for the bias correction; otherwise NaN.
double kurtosisFromSlice(const double *data, size_t n, int normFlag)
{
    if (n < 2) return std::nan("");
    double mean = 0.0;
    for (size_t i = 0; i < n; ++i) mean += data[i];
    mean /= static_cast<double>(n);
    double m2 = 0.0, m4 = 0.0;
    for (size_t i = 0; i < n; ++i) {
        const double d = data[i] - mean;
        const double d2 = d * d;
        m2 += d2;
        m4 += d2 * d2;
    }
    m2 /= static_cast<double>(n);
    m4 /= static_cast<double>(n);
    if (m2 <= 0.0) return std::nan("");
    double y = m4 / (m2 * m2);
    if (normFlag == 0) {
        if (n < 4) return std::nan("");
        const double nd = static_cast<double>(n);
        y = ((nd - 1.0) / ((nd - 2.0) * (nd - 3.0)))
                * ((nd + 1.0) * y - 3.0 * (nd - 1.0))
            + 3.0;
    }
    return y;
}

Value dispatchMomentReduction(std::pmr::memory_resource *mr, const Value &x, int dim,
                               int normFlag, const char *fn,
                               double (*fromSlice)(const double *, size_t, int))
{
    if (normFlag != 0 && normFlag != 1)
        throw Error(std::string(fn) + ": normalization flag must be 0 or 1",
                     0, 0, fn, "", std::string("m:") + fn + ":badFlag");
    if (x.type() == ValueType::COMPLEX)
        throw Error(std::string(fn) + ": complex inputs are not supported",
                     0, 0, fn, "", std::string("m:") + fn + ":complex");
    if (x.isEmpty())
        return Value::matrix(0, 0, ValueType::DOUBLE, mr);
    const int d = resolveDim(x, dim, fn);
    Value r = applyAlongDim(x, d,
        [normFlag, fromSlice](size_t, double *slice, size_t n) {
            return fromSlice(slice, n, normFlag);
        }, mr);
    if (x.type() == ValueType::SINGLE)
        r = narrowToSingle(std::move(r), mr);
    return r;
}

} // namespace

Value skewness(std::pmr::memory_resource *mr, const Value &x, int normFlag, int dim)
{
    return dispatchMomentReduction(mr, x, dim, normFlag, "skewness",
                                   skewnessFromSlice);
}

Value kurtosis(std::pmr::memory_resource *mr, const Value &x, int normFlag, int dim)
{
    return dispatchMomentReduction(mr, x, dim, normFlag, "kurtosis",
                                   kurtosisFromSlice);
}

// ── Engine adapters ──────────────────────────────────────────────────
namespace detail {

void skewness_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
                  CallContext &ctx)
{
    if (args.empty())
        throw Error("skewness: requires at least 1 argument",
                     0, 0, "skewness", "", "m:skewness:nargin");
    int normFlag = 1;  // MATLAB default
    int dim = 0;
    if (args.size() >= 2 && !args[1].isEmpty())
        normFlag = static_cast<int>(args[1].toScalar());
    if (args.size() >= 3 && !args[2].isEmpty())
        dim = static_cast<int>(args[2].toScalar());
    outs[0] = skewness(ctx.engine->resource(), args[0], normFlag, dim);
}

void kurtosis_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
                  CallContext &ctx)
{
    if (args.empty())
        throw Error("kurtosis: requires at least 1 argument",
                     0, 0, "kurtosis", "", "m:kurtosis:nargin");
    int normFlag = 1;
    int dim = 0;
    if (args.size() >= 2 && !args[1].isEmpty())
        normFlag = static_cast<int>(args[1].toScalar());
    if (args.size() >= 3 && !args[2].isEmpty())
        dim = static_cast<int>(args[2].toScalar());
    outs[0] = kurtosis(ctx.engine->resource(), args[0], normFlag, dim);
}

} // namespace detail

} // namespace numkit::stats
