// libs/wavelet/src/dwt/ihaart.cpp
//
// 1-D inverse Haar discrete wavelet transform — ihaart.
//
// MATLAB R2025b semantics (verified end-to-end via probe scripts):
//
//   xrec = ihaart(a, d[, level[, integerflag]])
//
//   * `a` — approximation column / matrix from haart.
//   * `d` — detail. Plain matrix when haart was called with level=1,
//     a length-Nlevels cell array otherwise; d{1} is the finest scale.
//   * `level` (default 0) — non-negative integer in [0, Nlevels). When
//     level=K the detail bands d{1..K} are zeroed BEFORE reconstruction
//     (so xrec is still full-length but with the finest K scales
//     suppressed). level==0 means lossless reconstruction.
//   * `integerflag` ∈ {'noninteger' (default), 'integer'} chooses the
//     orthogonal pair (factor 1/√2) or the lifting integer Haar inverse:
//
//       noninteger:  x[2k]   = (a[k] - d[k]) / √2
//                    x[2k+1] = (a[k] + d[k]) / √2
//       integer:     x[2k]   = a[k] - floor(d[k] / 2)
//                    x[2k+1] = x[2k] + d[k]
//
//   * `a` may be complex; `d` MUST be real and finite — a non-real or
//     non-finite `d` is rejected with an error.
//   * Empty `a` errors. Negative level errors. level >= Nlevels errors.
//   * Output orientation: vector-shaped a returns column; matrix
//     returns matrix (columns reconstructed independently).

#include <numkit/wavelet/dwt/ihaart.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>
#include <numkit/core/value.hpp>

#include <cmath>
#include <complex>
#include <string>
#include <vector>

namespace numkit::wavelet {

namespace {

void ihaar_step_real(const double *a, const double *d, size_t H,
                     double *out, bool integer)
{
    if (integer) {
        for (size_t i = 0; i < H; ++i) {
            const double x0 = a[i] - std::floor(d[i] / 2.0);
            out[2 * i]     = x0;
            out[2 * i + 1] = x0 + d[i];
        }
    } else {
        const double s = 1.0 / std::sqrt(2.0);
        for (size_t i = 0; i < H; ++i) {
            out[2 * i]     = (a[i] - d[i]) * s;
            out[2 * i + 1] = (a[i] + d[i]) * s;
        }
    }
}

void ihaar_step_cplx(const Complex *a, const double *d, size_t H, Complex *out)
{
    const double s = 1.0 / std::sqrt(2.0);
    for (size_t i = 0; i < H; ++i) {
        out[2 * i]     = (a[i] - d[i]) * s;
        out[2 * i + 1] = (a[i] + d[i]) * s;
    }
}

// Pull a single detail level (length H × ncols) out of the user-supplied
// `d`. If `d` is a cell, take cell at `idx` (0-based, where 0 is finest).
// If `d` is a plain matrix the only valid idx is 0.
//
// Returns:
//   - rows / cols of the level
//   - a vector<double> with the level's values (column-major); if
//     `zero_out` is true the returned vector is zero-filled (used to
//     suppress the finest K bands when `level > 0`).
struct DetailLevel {
    std::vector<double> data;
    size_t rows = 0;
    size_t cols = 0;
};

DetailLevel grab_level(const Value &d, bool isCell, size_t idx,
                       size_t expectedRows, size_t expectedCols,
                       bool zero_out)
{
    DetailLevel L;
    const Value &m = isCell ? d.cellAt(idx) : d;
    if (m.numel() == 0)
        throw Error("ihaart: detail level is empty",
                    0, 0, "ihaart", "", "numkit:ihaart:detail");
    if (m.isComplex())
        throw Error("ihaart: expected input number 2, D, to be real",
                    0, 0, "ihaart", "", "numkit:ihaart:complexD");

    // Detail shape is read directly (no row-↔-column coercion). For a
    // vector-input haart this is rows×1; for a matrix-input haart at
    // a deep level it can legitimately be 1×ncols.
    const size_t rows = m.dims().rows();
    const size_t cols = m.dims().cols();

    if (rows != expectedRows || cols != expectedCols)
        throw Error("ihaart: detail dimensions are inconsistent with the "
                    "approximation",
                    0, 0, "ihaart", "", "numkit:ihaart:dimMismatch");

    const size_t mn = rows * cols;

    L.rows = rows;
    L.cols = cols;
    L.data.resize(rows * cols);
    if (zero_out) {
        // already zero
        return L;
    }
    for (size_t i = 0; i < mn; ++i) L.data[i] = m.elemAsDouble(i);
    return L;
}

} // anonymous

// ── Public C++ API (see dwt/ihaart.hpp) ───────────────────────────────

Value ihaart(const Value &aV, const Value &dV, int level, bool integer,
             std::pmr::memory_resource *mr)
{
    if (aV.numel() == 0)
        throw Error("ihaart: expected input number 1, A, to be nonempty",
                    0, 0, "ihaart", "", "numkit:ihaart:emptyA");

    // Determine cell-vs-plain detail and Nlevels.
    const bool isCell = dV.isCell();
    size_t Nlevels = 0;
    if (isCell) {
        const size_t nc = dV.dims().rows() * dV.dims().cols();
        if (nc == 0)
            throw Error("ihaart: detail cell array is empty",
                        0, 0, "ihaart", "", "numkit:ihaart:emptyD");
        Nlevels = nc;
    } else {
        if (dV.numel() == 0)
            throw Error("ihaart: detail array is empty",
                        0, 0, "ihaart", "", "numkit:ihaart:emptyD");
        if (dV.isComplex())
            throw Error("ihaart: expected input number 2, D, to be real",
                        0, 0, "ihaart", "", "numkit:ihaart:complexD");
        Nlevels = 1;
    }

    if (level < 0)
        throw Error("ihaart: expected input number 3, LEVEL, to be a scalar "
                    "with value >= 0",
                    0, 0, "ihaart", "", "numkit:ihaart:level");
    if (level >= static_cast<int>(Nlevels))
        throw Error("ihaart: expected input number 3, LEVEL, to be a scalar "
                    "with value < " + std::to_string(Nlevels),
                    0, 0, "ihaart", "", "numkit:ihaart:level");
    if (integer && aV.isComplex())
        throw Error("ihaart: integer mode is not supported for complex data",
                    0, 0, "ihaart", "", "numkit:ihaart:flag");

    // `a` is always shaped to match the detail dimensions (haart emits
    // columns for vector inputs), so the matrix shape is read directly.
    const size_t aRows = aV.dims().rows();
    const size_t aCols = aV.dims().cols();
    const size_t aLen  = aRows;
    const size_t ncols = aCols;

    const bool aIsComplex = aV.isComplex();
    const ValueType vt = aIsComplex ? ValueType::COMPLEX : ValueType::DOUBLE;

    // Working buffer: starts as `a`, doubles each level (column-major).
    std::vector<double>  curR;
    std::vector<Complex> curC;
    if (aIsComplex) {
        curC.resize(aLen * ncols);
        for (size_t c = 0; c < ncols; ++c)
            for (size_t r = 0; r < aLen; ++r)
                curC[c * aLen + r] = aV.complexElem(c * aRows + r);
    } else {
        curR.resize(aLen * ncols);
        for (size_t c = 0; c < ncols; ++c)
            for (size_t r = 0; r < aLen; ++r)
                curR[c * aLen + r] = aV.elemAsDouble(c * aRows + r);
    }
    size_t curLen = aLen;

    // Reconstruct coarsest -> finest. For cells d{1} is finest, so
    // iterating L = Nlevels down to 1 uses detail at index (L-1).
    for (int L = static_cast<int>(Nlevels); L >= 1; --L) {
        const size_t idx = static_cast<size_t>(L - 1);     // 0..Nlevels-1
        const bool zero = (idx < static_cast<size_t>(level));
        DetailLevel det = grab_level(dV, isCell, idx, curLen, ncols, zero);

        const size_t outLen = curLen * 2;
        std::vector<double>  newR;
        std::vector<Complex> newC;
        if (aIsComplex) {
            newC.resize(outLen * ncols);
            for (size_t c = 0; c < ncols; ++c)
                ihaar_step_cplx(curC.data() + c * curLen,
                                det.data.data() + c * curLen, curLen,
                                newC.data() + c * outLen);
            curC.swap(newC);
        } else {
            newR.resize(outLen * ncols);
            for (size_t c = 0; c < ncols; ++c)
                ihaar_step_real(curR.data() + c * curLen,
                                det.data.data() + c * curLen, curLen,
                                newR.data() + c * outLen, integer);
            curR.swap(newR);
        }
        curLen = outLen;
    }

    // Pack the result. Vector input -> column output; matrix -> matrix.
    Value out = Value::matrix(curLen, ncols, vt, mr);
    if (aIsComplex) {
        Complex *p = out.complexDataMut();
        std::copy(curC.begin(), curC.end(), p);
    } else {
        double *p = out.doubleDataMut();
        std::copy(curR.begin(), curR.end(), p);
    }
    return out;
}

namespace detail {

void ihaart_reg(Span<const Value> args, size_t /*nargout*/,
                Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("ihaart: requires (a, d[, level[, integerflag]])",
                    0, 0, "ihaart", "", "numkit:ihaart:nargin");
    // Lax positional parse: level (number) and/or integerflag (string), in
    // any order. The level < Nlevels range check is done inside ihaart().
    int level = 0;
    bool integer_mode = false;
    for (size_t i = 2; i < args.size(); ++i) {
        const Value &arg = args[i];
        if (arg.isChar() || arg.isString()) {
            std::string flag = arg.toString();
            for (auto &c : flag)
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if      (flag == "integer")    integer_mode = true;
            else if (flag == "noninteger") integer_mode = false;
            else
                throw Error("ihaart: integerflag must be 'noninteger' or "
                            "'integer' (got '" + flag + "')",
                            0, 0, "ihaart", "", "numkit:ihaart:flag");
        } else {
            const double lvld = arg.toScalar();
            if (!(lvld >= 0.0))
                throw Error("ihaart: expected input number 3, LEVEL, to be a "
                            "scalar with value >= 0",
                            0, 0, "ihaart", "", "numkit:ihaart:level");
            if (lvld != std::floor(lvld))
                throw Error("ihaart: expected LEVEL to be an integer",
                            0, 0, "ihaart", "", "numkit:ihaart:level");
            level = static_cast<int>(lvld);
        }
    }
    outs[0] = ihaart(args[0], args[1], level, integer_mode,
                     ctx.engine->resource());
}

} // namespace detail
} // namespace numkit::wavelet
