// libs/stats/src/descriptive/normalize.cpp
//
// normalize / rescale / zscore — basic data pre-processing.
// Operate column-wise on matrices, whole-vector on 1-D inputs (MATLAB
// convention). Output is always DOUBLE regardless of input class.

#include <numkit/stats/descriptive/descriptive.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace numkit::stats {

namespace {

// Iterate over columns of A and call `op(col_idx, ptr_to_double_buf, len)`
// for each. Vectors are treated as a single "column" of length numel.
template <typename Op>
void forEachColumn(const Value &A, Op op) {
    const size_t H = A.dims().rows();
    const size_t W = A.dims().cols();
    const size_t N = A.numel();
    if (N == H * W && (H == 1 || W == 1)) {
        // 1-D: pull whole vector as a single column.
        std::vector<double> buf(N);
        for (size_t i = 0; i < N; ++i) buf[i] = A.elemAsDouble(i);
        op(0, buf.data(), N);
        return;
    }
    // Matrix / N-D: walk columns. Column j spans linear indices
    // [j*H .. j*H + H - 1]; for N-D we still treat the FIRST dim as
    // "rows" (first non-singleton — same simplification as numkit's
    // mean/var on 2-D path).
    if (N != H * W)
        throw Error("normalize/rescale/zscore: only 1-D or 2-D inputs supported",
                    0, 0, "normalize", "", "numkit:normalize:nd");
    std::vector<double> buf(H);
    for (size_t j = 0; j < W; ++j) {
        for (size_t i = 0; i < H; ++i) buf[i] = A.elemAsDouble(j * H + i);
        op(j, buf.data(), H);
    }
}

void writeColumn(Value &out, size_t colIdx, const double *src, size_t len,
                 size_t H, size_t W)
{
    double *od = out.doubleDataMut();
    if (W == 1 || H == 1) {
        for (size_t i = 0; i < len; ++i) od[i] = src[i];
    } else {
        for (size_t i = 0; i < len; ++i) od[colIdx * H + i] = src[i];
    }
}

double colMean(const double *x, size_t n) {
    double s = 0.0;
    for (size_t i = 0; i < n; ++i) s += x[i];
    return (n > 0) ? s / static_cast<double>(n) : 0.0;
}

double colStdPop(const double *x, size_t n) {
    if (n == 0) return 0.0;
    const double m = colMean(x, n);
    double s = 0.0;
    for (size_t i = 0; i < n; ++i) {
        const double d = x[i] - m;
        s += d * d;
    }
    return std::sqrt(s / static_cast<double>(n));
}

// Sample (N-1) standard deviation — MATLAB's default for std/zscore/normalize.
double colStdSample(const double *x, size_t n) {
    if (n < 2) return 0.0;
    const double m = colMean(x, n);
    double s = 0.0;
    for (size_t i = 0; i < n; ++i) {
        const double d = x[i] - m;
        s += d * d;
    }
    return std::sqrt(s / static_cast<double>(n - 1));
}

double colMedian(std::vector<double> v) {
    std::sort(v.begin(), v.end());
    const size_t n = v.size();
    if (n == 0) return 0.0;
    return (n & 1) ? v[n / 2]
                   : 0.5 * (v[n / 2 - 1] + v[n / 2]);
}

double colIQR(std::vector<double> v) {
    std::sort(v.begin(), v.end());
    const size_t n = v.size();
    if (n < 2) return 0.0;
    auto q = [&](double p) {
        const double pos = p * (n - 1);
        const size_t lo = static_cast<size_t>(std::floor(pos));
        const size_t hi = static_cast<size_t>(std::ceil(pos));
        const double frac = pos - lo;
        return v[lo] * (1.0 - frac) + v[hi] * frac;
    };
    return q(0.75) - q(0.25);
}

bool methodMatches(const std::string &m, const char *want) {
    if (m.size() != std::strlen(want)) return false;
    for (size_t i = 0; i < m.size(); ++i) {
        char a = m[i], b = want[i];
        if (a >= 'A' && a <= 'Z') a = char(a + 32);
        if (b >= 'A' && b <= 'Z') b = char(b + 32);
        if (a != b) return false;
    }
    return true;
}

} // anonymous

Value normalize(const Value &A, const std::string &method, std::pmr::memory_resource *mr,
                const Value *param)
{
    const size_t H = A.dims().rows();
    const size_t W = A.dims().cols();
    Value out = Value::matrix(H, W, ValueType::DOUBLE, mr);
    if (H == 0 || W == 0) return out;

    const std::string m = method.empty() ? std::string("zscore") : method;

    // Decode the optional method parameter once. A string param (e.g.
    // 'first'/'median') vs a numeric param (range bounds / norm-p / divisor).
    const bool hasParam = (param != nullptr && !param->isEmpty());
    const bool paramIsStr = hasParam && (param->isChar() || param->isString());
    std::string paramStr;
    if (paramIsStr) {
        paramStr = param->toString();
        for (char &c : paramStr) if (c >= 'A' && c <= 'Z') c = char(c + 32);
    }
    const double paramNum = (hasParam && !paramIsStr) ? param->elemAsDouble(0) : 0.0;

    forEachColumn(A, [&](size_t j, double *col, size_t n) {
        std::vector<double> y(n);
        if (methodMatches(m, "zscore")) {
            const double mu = colMean(col, n);
            const double sd = colStdSample(col, n);  // MATLAB default: N-1
            const double inv = (sd != 0.0) ? 1.0 / sd : 0.0;
            for (size_t i = 0; i < n; ++i) y[i] = (col[i] - mu) * inv;
        } else if (methodMatches(m, "center")) {
            // default 'mean'; 'median' or a numeric centre also accepted.
            double c;
            if (hasParam && !paramIsStr)              c = paramNum;
            else if (paramIsStr && paramStr == "median") {
                std::vector<double> tmp(col, col + n); c = colMedian(tmp);
            } else                                     c = colMean(col, n);
            for (size_t i = 0; i < n; ++i) y[i] = col[i] - c;
        } else if (methodMatches(m, "scale")) {
            // default 'std'; 'first'/'iqr'/'mad' or a numeric divisor.
            double sc;
            if (hasParam && !paramIsStr)               sc = paramNum;
            else if (paramIsStr && paramStr == "first") sc = col[0];
            else if (paramIsStr && paramStr == "iqr") {
                std::vector<double> tmp(col, col + n);  sc = colIQR(std::move(tmp));
            } else if (paramIsStr && paramStr == "mad") {
                std::vector<double> tmp(col, col + n);
                const double med = colMedian(tmp);
                for (double &v : tmp) v = std::fabs(v - med);
                sc = colMedian(tmp);
            } else                                      sc = colStdSample(col, n);
            const double inv = (sc != 0.0) ? 1.0 / sc : 0.0;
            for (size_t i = 0; i < n; ++i) y[i] = col[i] * inv;
        } else if (methodMatches(m, "range")) {
            // default [0 1]; custom [lo hi] supported.
            double rlo = 0.0, rhi = 1.0;
            if (hasParam && !paramIsStr && param->numel() >= 2) {
                rlo = param->elemAsDouble(0);
                rhi = param->elemAsDouble(1);
            }
            double lo = col[0], hi = col[0];
            for (size_t i = 1; i < n; ++i) {
                if (col[i] < lo) lo = col[i];
                if (col[i] > hi) hi = col[i];
            }
            const double r = hi - lo;
            if (r != 0.0) {
                const double scl = (rhi - rlo) / r;
                for (size_t i = 0; i < n; ++i) y[i] = rlo + (col[i] - lo) * scl;
            } else {
                for (size_t i = 0; i < n; ++i) y[i] = rlo;
            }
        } else if (methodMatches(m, "norm")) {
            // default p=2; p=1 / p=Inf / general p supported.
            const double p = hasParam && !paramIsStr ? paramNum : 2.0;
            double L;
            if (std::isinf(p)) {
                L = 0.0;
                for (size_t i = 0; i < n; ++i) L = std::max(L, std::fabs(col[i]));
            } else if (p == 1.0) {
                L = 0.0;
                for (size_t i = 0; i < n; ++i) L += std::fabs(col[i]);
            } else if (p == 2.0) {
                double s = 0.0;
                for (size_t i = 0; i < n; ++i) s += col[i] * col[i];
                L = std::sqrt(s);
            } else {
                double s = 0.0;
                for (size_t i = 0; i < n; ++i) s += std::pow(std::fabs(col[i]), p);
                L = std::pow(s, 1.0 / p);
            }
            const double inv = (L != 0.0) ? 1.0 / L : 0.0;
            for (size_t i = 0; i < n; ++i) y[i] = col[i] * inv;
        } else if (methodMatches(m, "medianiqr")) {
            std::vector<double> tmp(col, col + n);
            const double med = colMedian(tmp);
            const double iqr = colIQR(std::move(tmp));
            const double inv = (iqr != 0.0) ? 1.0 / iqr : 0.0;
            for (size_t i = 0; i < n; ++i) y[i] = (col[i] - med) * inv;
        } else {
            throw Error("normalize: unknown method '" + m + "'",
                        0, 0, "normalize", "", "numkit:normalize:method");
        }
        writeColumn(out, j, y.data(), n, H, W);
    });
    return out;
}

Value rescale(const Value &A, double lo, double hi, std::pmr::memory_resource *mr)
{
    const size_t H = A.dims().rows();
    const size_t W = A.dims().cols();
    Value out = Value::matrix(H, W, ValueType::DOUBLE, mr);
    if (H == 0 || W == 0) return out;
    forEachColumn(A, [&](size_t j, double *col, size_t n) {
        double mn = col[0], mx = col[0];
        for (size_t i = 1; i < n; ++i) {
            if (col[i] < mn) mn = col[i];
            if (col[i] > mx) mx = col[i];
        }
        const double r = mx - mn;
        std::vector<double> y(n);
        if (r == 0.0) {
            std::fill(y.begin(), y.end(), lo);
        } else {
            const double scl = (hi - lo) / r;
            for (size_t i = 0; i < n; ++i) y[i] = lo + (col[i] - mn) * scl;
        }
        writeColumn(out, j, y.data(), n, H, W);
    });
    return out;
}

// Flag-aware, dim-aware z-score. flag 0 (default) -> sample std (N-1),
// flag 1 -> population std (N). dim<=0 auto-selects the first non-singleton
// dimension (MATLAB convention); dim 1 = columns, dim 2 = rows.
static Value zscoreCore(const Value &A, int flag, int dim,
                        std::pmr::memory_resource *mr)
{
    const size_t H = A.dims().rows();
    const size_t W = A.dims().cols();
    Value out = Value::matrix(H, W, ValueType::DOUBLE, mr);
    if (H == 0 || W == 0) return out;
    double *od = out.doubleDataMut();
    auto sdf = [&](const double *x, size_t n) {
        return (flag == 1) ? colStdPop(x, n) : colStdSample(x, n);
    };
    int opDim = dim;
    if (opDim <= 0) opDim = (H == 1 && W > 1) ? 2 : 1;  // first non-singleton
    if (opDim == 1) {
        std::vector<double> col(H);
        for (size_t j = 0; j < W; ++j) {
            for (size_t i = 0; i < H; ++i) col[i] = A.elemAsDouble(j * H + i);
            const double mu = colMean(col.data(), H);
            const double sd = sdf(col.data(), H);
            const double inv = (sd != 0.0) ? 1.0 / sd : 0.0;
            for (size_t i = 0; i < H; ++i) od[j * H + i] = (col[i] - mu) * inv;
        }
    } else {  // opDim == 2, row-wise
        std::vector<double> row(W);
        for (size_t i = 0; i < H; ++i) {
            for (size_t j = 0; j < W; ++j) row[j] = A.elemAsDouble(j * H + i);
            const double mu = colMean(row.data(), W);
            const double sd = sdf(row.data(), W);
            const double inv = (sd != 0.0) ? 1.0 / sd : 0.0;
            for (size_t j = 0; j < W; ++j) od[j * H + i] = (row[j] - mu) * inv;
        }
    }
    return out;
}

Value zscore(const Value &A, std::pmr::memory_resource *mr)
{
    return zscoreCore(A, /*flag=*/0, /*dim=*/0, mr);
}

namespace detail {

void normalize_reg(Span<const Value> args, size_t /*nargout*/,
                   Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("normalize: requires (A [, method])",
                    0, 0, "normalize", "", "numkit:normalize:nargin");
    std::string method = "zscore";
    const Value *param = nullptr;
    if (args.size() >= 2 && !args[1].isEmpty()) {
        if (!args[1].isChar() && !args[1].isString())
            throw Error("normalize: method must be a string",
                        0, 0, "normalize", "", "numkit:normalize:type");
        method = args[1].toString();
        // Optional method parameter: range bounds / norm-p / scale divisor
        // / center reference. (Previously dropped -> options were ignored.)
        if (args.size() >= 3 && !args[2].isEmpty())
            param = &args[2];
    }
    outs[0] = normalize(args[0], method, ctx.engine->resource(), param);
}

void rescale_reg(Span<const Value> args, size_t /*nargout*/,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("rescale: requires (A [, lo, hi])",
                    0, 0, "rescale", "", "numkit:rescale:nargin");
    const double lo = (args.size() >= 2 && !args[1].isEmpty())
                      ? args[1].toScalar() : 0.0;
    const double hi = (args.size() >= 3 && !args[2].isEmpty())
                      ? args[2].toScalar() : 1.0;
    outs[0] = rescale(args[0], lo, hi, ctx.engine->resource());
}

void zscore_reg(Span<const Value> args, size_t /*nargout*/,
                Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("zscore: requires (A [, flag [, dim]])",
                    0, 0, "zscore", "", "numkit:zscore:nargin");
    int flag = 0, dim = 0;
    if (args.size() >= 2 && !args[1].isEmpty()) flag = static_cast<int>(args[1].toScalar());
    if (args.size() >= 3 && !args[2].isEmpty()) dim  = static_cast<int>(args[2].toScalar());
    outs[0] = zscoreCore(args[0], flag, dim, ctx.engine->resource());
}

} // namespace detail

} // namespace numkit::stats
