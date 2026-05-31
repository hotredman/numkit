// libs/wavelet/src/dwt/haart.cpp
//
// 1-D Haar discrete wavelet transform — haart.
//
// MATLAB R2025b semantics (verified end-to-end via probe scripts):
//
//   [a, d] = haart(x[, level[, integerflag]])
//
//   * x is a vector (row or column) or a matrix; column orientation is
//     forced on the output regardless of input orientation. For matrix
//     input each column is processed independently and `rows` must be
//     even (and divisible by 2^level at each level).
//   * `level` (default = max k such that 2^k divides length(x))
//     positive integer; must satisfy level ≤ max level.
//   * `integerflag` ∈ {'noninteger' (default), 'integer'} selects the
//     orthogonal Haar pair (factor 1/√2) or the lifting integer Haar:
//
//       noninteger:   a[k] = (x[2k] + x[2k+1]) / √2
//                     d[k] = (x[2k+1] - x[2k]) / √2
//       integer:      d[k] = x[2k+1] - x[2k]
//                     a[k] = x[2k] + floor(d[k] / 2)
//
//   * d is a plain matrix when level == 1 and a length-`level` cell
//     array d{1..level} when level > 1. d{1} is the finest-scale detail.
//   * Empty x, scalar x, odd length, level<=0 — MATLAB throws; we mirror
//     the messages.
//
// Complex input is supported only in 'noninteger' mode (lifting floor
// is not defined on complex values; MATLAB also disallows it).

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>
#include <numkit/core/value.hpp>

#include <cmath>
#include <complex>
#include <string>
#include <vector>

namespace numkit::wavelet {

namespace {

// Largest k such that 2^k divides N (with 2^k ≤ N).
int max_haar_level(size_t N)
{
    int k = 0;
    while (N > 1 && (N % 2 == 0)) { ++k; N /= 2; }
    return k;
}

// One Haar level on a single contiguous slice of length `inLen` (even).
// Writes inLen/2 entries to `a` and inLen/2 entries to `d`.
void haar_step_real(const double *x, size_t inLen, double *a, double *d,
                    bool integer)
{
    const size_t H = inLen / 2;
    if (integer) {
        for (size_t i = 0; i < H; ++i) {
            const double dv = x[2 * i + 1] - x[2 * i];
            d[i] = dv;
            a[i] = x[2 * i] + std::floor(dv / 2.0);
        }
    } else {
        const double s = 1.0 / std::sqrt(2.0);
        for (size_t i = 0; i < H; ++i) {
            a[i] = (x[2 * i] + x[2 * i + 1]) * s;
            d[i] = (x[2 * i + 1] - x[2 * i]) * s;
        }
    }
}

void haar_step_cplx(const Complex *x, size_t inLen, Complex *a, Complex *d)
{
    const size_t H = inLen / 2;
    const double s = 1.0 / std::sqrt(2.0);
    for (size_t i = 0; i < H; ++i) {
        a[i] = (x[2 * i] + x[2 * i + 1]) * s;
        d[i] = (x[2 * i + 1] - x[2 * i]) * s;
    }
}

// Materialize x into a column-major DOUBLE buffer of (colLen × ncols).
// For a vector the whole content is one column of length N; for a
// matrix we keep the original (rows × cols) shape.
struct Buf {
    std::vector<double> real;
    std::vector<Complex> cplx;
    size_t colLen = 0;
    size_t ncols  = 0;
    bool   isComplex = false;
};

Buf load_x(const Value &x, bool /*integer_mode*/)
{
    Buf b;
    const size_t rows = x.dims().rows();
    const size_t cols = x.dims().cols();
    const size_t N    = rows * cols;
    const bool isVec  = (rows <= 1) || (cols <= 1);
    if (isVec) { b.colLen = N; b.ncols = 1; }
    else       { b.colLen = rows; b.ncols = cols; }

    b.isComplex = x.isComplex();
    if (b.isComplex) {
        b.cplx.resize(N);
        for (size_t i = 0; i < N; ++i) b.cplx[i] = x.complexElem(i);
    } else {
        b.real.resize(N);
        for (size_t i = 0; i < N; ++i) b.real[i] = x.elemAsDouble(i);
    }
    return b;
}

} // anonymous

namespace detail {

void haart_reg(Span<const Value> args, size_t nargout,
               Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("haart: requires (x[, level[, integerflag]])",
                    0, 0, "haart", "", "numkit:haart:nargin");
    const Value &x = args[0];
    const size_t numel = x.numel();
    if (numel == 0)
        throw Error("haart: expected input number 1, X, to be nonempty",
                    0, 0, "haart", "", "numkit:haart:empty");

    Buf buf = load_x(x, false);
    const size_t colLen = buf.colLen;
    const size_t ncols  = buf.ncols;

    if (colLen < 2 || (colLen % 2) != 0)
        throw Error("haart: the data length must be even. Consider replicating "
                    "for an even length signal, or removing the last element.",
                    0, 0, "haart", "", "numkit:haart:even");

    const int max_lev = max_haar_level(colLen);

    int level = max_lev;
    if (args.size() >= 2 && !args[1].isEmpty()) {
        const double lvld = args[1].toScalar();
        if (!(lvld > 0.0))
            throw Error("haart: expected LEVEL to be positive",
                        0, 0, "haart", "", "numkit:haart:level");
        if (lvld != std::floor(lvld))
            throw Error("haart: expected LEVEL to be an integer",
                        0, 0, "haart", "", "numkit:haart:level");
        level = static_cast<int>(lvld);
        if (level > max_lev)
            throw Error("haart: expected LEVEL to be a scalar with value <= " +
                        std::to_string(max_lev),
                        0, 0, "haart", "", "numkit:haart:level");
    }

    bool integer_mode = false;
    if (args.size() >= 3) {
        if (!args[2].isChar() && !args[2].isString())
            throw Error("haart: integerflag must be 'noninteger' or 'integer'",
                        0, 0, "haart", "", "numkit:haart:flag");
        std::string flag = args[2].toString();
        for (auto &c : flag)
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if      (flag == "integer")    integer_mode = true;
        else if (flag == "noninteger") integer_mode = false;
        else
            throw Error("haart: integerflag must be 'noninteger' or 'integer' (got '" +
                        flag + "')",
                        0, 0, "haart", "", "numkit:haart:flag");
    }
    if (integer_mode && buf.isComplex)
        throw Error("haart: integer mode is not supported for complex data",
                    0, 0, "haart", "", "numkit:haart:flag");

    auto *mr = ctx.engine->resource();
    const ValueType vt = buf.isComplex ? ValueType::COMPLEX : ValueType::DOUBLE;

    // Working "current" buffer: starts as x, halves each level. Stored
    // column-major (size = inLen × ncols).
    std::vector<double>  curR = buf.real;
    std::vector<Complex> curC = buf.cplx;
    size_t inLen = colLen;

    std::vector<Value> details;
    details.reserve(static_cast<size_t>(level));

    for (int k = 0; k < level; ++k) {
        const size_t H = inLen / 2;
        Value aMat = Value::matrix(H, ncols, vt, mr);
        Value dMat = Value::matrix(H, ncols, vt, mr);

        if (buf.isComplex) {
            Complex *aP = aMat.complexDataMut();
            Complex *dP = dMat.complexDataMut();
            for (size_t c = 0; c < ncols; ++c)
                haar_step_cplx(curC.data() + c * inLen, inLen,
                               aP + c * H, dP + c * H);
        } else {
            double *aP = aMat.doubleDataMut();
            double *dP = dMat.doubleDataMut();
            for (size_t c = 0; c < ncols; ++c)
                haar_step_real(curR.data() + c * inLen, inLen,
                               aP + c * H, dP + c * H, integer_mode);
        }

        // Move the new approximation back into the working buffer.
        if (buf.isComplex) {
            const Complex *aP = aMat.complexData();
            curC.assign(aP, aP + H * ncols);
        } else {
            const double *aP = aMat.doubleData();
            curR.assign(aP, aP + H * ncols);
        }
        details.push_back(std::move(dMat));

        // Final-level approximation goes out as outs[0].
        if (k == level - 1)
            outs[0] = std::move(aMat);

        inLen = H;
    }

    if (nargout >= 2) {
        if (level == 1) {
            outs[1] = std::move(details[0]);          // plain matrix
        } else {
            // Cell column of length `level` — d{1} is finest.
            Value cellOut = Value::cell(static_cast<size_t>(level), 1, mr);
            for (int k = 0; k < level; ++k)
                cellOut.cellAt(static_cast<size_t>(k)) = std::move(details[k]);
            outs[1] = std::move(cellOut);
        }
    }
}

} // namespace detail
} // namespace numkit::wavelet
