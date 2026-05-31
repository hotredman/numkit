// libs/wavelet/src/dwt/dwt2.cpp
//
// 2-D separable wavelet transform. Built on the cycle-26 1-D dwt /
// idwt: apply a row pass, then a column pass (or the reverse for the
// inverse). MATLAB output order is [cA, cH, cV, cD] with the labels:
//
//   cA (approx)            = Lo-rows then Lo-cols   (LL)
//   cH (horizontal detail) = Lo-rows then Hi-cols   (LH)
//   cV (vertical detail)   = Hi-rows then Lo-cols   (HL)
//   cD (diagonal detail)   = Hi-rows then Hi-cols   (HH)
//
// Internally we slice rows/columns as length-K Value vectors, hand
// them to dwt / idwt, and stitch the results back into MxN matrices.

#include <numkit/wavelet/dwt/dwt2.hpp>
#include <numkit/wavelet/dwt/dwt.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include <algorithm>
#include <utility>
#include <vector>

namespace numkit::wavelet {

namespace {

// Apply 1-D dwt to every row of X. Returns (Lo, Hi), each M × outCols.
std::pair<Value, Value>
dwt_rows(const Value &X, const std::string &wname,
         std::pmr::memory_resource *mr)
{
    const size_t M = X.dims().rows();
    const size_t N = X.dims().cols();
    if (M == 0 || N == 0)
        return {Value::matrix(M, 0, ValueType::DOUBLE, mr),
                Value::matrix(M, 0, ValueType::DOUBLE, mr)};

    // Determine the per-row output length by running one row first.
    auto row = Value::matrix(1, N, ValueType::DOUBLE, mr);
    double *rd = row.doubleDataMut();
    for (size_t c = 0; c < N; ++c) rd[c] = X.elemAsDouble(c * M + 0);
    auto [cA0, cD0] = dwt(row, wname, mr);
    const size_t outN = cA0.numel();

    Value loM = Value::matrix(M, outN, ValueType::DOUBLE, mr);
    Value hiM = Value::matrix(M, outN, ValueType::DOUBLE, mr);
    double *ld = loM.doubleDataMut();
    double *hd = hiM.doubleDataMut();
    // First row already done.
    {
        const double *aL = cA0.doubleData();
        const double *aH = cD0.doubleData();
        for (size_t c = 0; c < outN; ++c) {
            ld[c * M + 0] = aL[c];
            hd[c * M + 0] = aH[c];
        }
    }
    for (size_t r = 1; r < M; ++r) {
        for (size_t c = 0; c < N; ++c) rd[c] = X.elemAsDouble(c * M + r);
        auto [cA, cD] = dwt(row, wname, mr);
        const double *aL = cA.doubleData();
        const double *aH = cD.doubleData();
        for (size_t c = 0; c < outN; ++c) {
            ld[c * M + r] = aL[c];
            hd[c * M + r] = aH[c];
        }
    }
    return {std::move(loM), std::move(hiM)};
}

// Apply 1-D dwt to every column of X. Returns (Lo, Hi), each outRows × N.
std::pair<Value, Value>
dwt_cols(const Value &X, const std::string &wname,
         std::pmr::memory_resource *mr)
{
    const size_t M = X.dims().rows();
    const size_t N = X.dims().cols();
    if (M == 0 || N == 0)
        return {Value::matrix(0, N, ValueType::DOUBLE, mr),
                Value::matrix(0, N, ValueType::DOUBLE, mr)};

    auto col = Value::matrix(M, 1, ValueType::DOUBLE, mr);
    double *cd = col.doubleDataMut();
    for (size_t r = 0; r < M; ++r) cd[r] = X.elemAsDouble(0 * M + r);
    auto [cA0, cD0] = dwt(col, wname, mr);
    const size_t outM = cA0.numel();

    Value loM = Value::matrix(outM, N, ValueType::DOUBLE, mr);
    Value hiM = Value::matrix(outM, N, ValueType::DOUBLE, mr);
    double *ld = loM.doubleDataMut();
    double *hd = hiM.doubleDataMut();
    {
        const double *aL = cA0.doubleData();
        const double *aH = cD0.doubleData();
        for (size_t r = 0; r < outM; ++r) {
            ld[0 * outM + r] = aL[r];
            hd[0 * outM + r] = aH[r];
        }
    }
    for (size_t c = 1; c < N; ++c) {
        for (size_t r = 0; r < M; ++r) cd[r] = X.elemAsDouble(c * M + r);
        auto [cA, cD] = dwt(col, wname, mr);
        const double *aL = cA.doubleData();
        const double *aH = cD.doubleData();
        for (size_t r = 0; r < outM; ++r) {
            ld[c * outM + r] = aL[r];
            hd[c * outM + r] = aH[r];
        }
    }
    return {std::move(loM), std::move(hiM)};
}

// Inverse: combine a (lo, hi) pair into a single matrix by running
// 1-D idwt along each column, with target length outRows.
Value idwt_cols(const Value &Lo, const Value &Hi,
                const std::string &wname,
                size_t outRows,
                std::pmr::memory_resource *mr)
{
    const size_t M = Lo.dims().rows();
    const size_t N = Lo.dims().cols();
    Value out = Value::matrix(outRows, N, ValueType::DOUBLE, mr);
    if (M == 0 || N == 0 || outRows == 0) return out;
    double *od = out.doubleDataMut();

    auto colA = Value::matrix(M, 1, ValueType::DOUBLE, mr);
    auto colD = Value::matrix(M, 1, ValueType::DOUBLE, mr);
    double *ad = colA.doubleDataMut();
    double *dd = colD.doubleDataMut();
    for (size_t c = 0; c < N; ++c) {
        for (size_t r = 0; r < M; ++r) {
            ad[r] = Lo.elemAsDouble(c * M + r);
            dd[r] = Hi.elemAsDouble(c * M + r);
        }
        Value y = idwt(colA, colD, wname,
                       static_cast<long long>(outRows), mr);
        const double *yd = y.doubleData();
        for (size_t r = 0; r < outRows; ++r)
            od[c * outRows + r] = yd[r];
    }
    return out;
}

// Inverse along rows, target length outCols.
Value idwt_rows(const Value &Lo, const Value &Hi,
                const std::string &wname,
                size_t outCols,
                std::pmr::memory_resource *mr)
{
    const size_t M = Lo.dims().rows();
    const size_t N = Lo.dims().cols();
    Value out = Value::matrix(M, outCols, ValueType::DOUBLE, mr);
    if (M == 0 || N == 0 || outCols == 0) return out;
    double *od = out.doubleDataMut();

    auto rowA = Value::matrix(N, 1, ValueType::DOUBLE, mr);
    auto rowD = Value::matrix(N, 1, ValueType::DOUBLE, mr);
    double *ad = rowA.doubleDataMut();
    double *dd = rowD.doubleDataMut();
    for (size_t r = 0; r < M; ++r) {
        for (size_t c = 0; c < N; ++c) {
            ad[c] = Lo.elemAsDouble(c * M + r);
            dd[c] = Hi.elemAsDouble(c * M + r);
        }
        Value y = idwt(rowA, rowD, wname,
                       static_cast<long long>(outCols), mr);
        const double *yd = y.doubleData();
        for (size_t c = 0; c < outCols; ++c)
            od[c * M + r] = yd[c];
    }
    return out;
}

} // anonymous

Dwt2Result dwt2(const Value &X, const std::string &wname,
                std::pmr::memory_resource *mr)
{
    // Pass 1: rows (each row → Lo + Hi halves, each M × outN).
    auto [LoR, HiR] = dwt_rows(X, wname, mr);

    // Pass 2: columns of LoR  → cA / cH; columns of HiR → cV / cD.
    auto [Lo_of_LoR, Hi_of_LoR] = dwt_cols(LoR, wname, mr);
    auto [Lo_of_HiR, Hi_of_HiR] = dwt_cols(HiR, wname, mr);

    return {std::move(Lo_of_LoR),   // cA = LL
            std::move(Hi_of_LoR),   // cH = LH (horizontal detail)
            std::move(Lo_of_HiR),   // cV = HL (vertical detail)
            std::move(Hi_of_HiR)};  // cD = HH (diagonal detail)
}

Value idwt2(const Value &cA, const Value &cH,
            const Value &cV, const Value &cD,
            const std::string &wname,
            long long outRows, long long outCols,
            std::pmr::memory_resource *mr)
{
    // Default reconstruction lengths (MATLAB's "2*la - Lf + 2" rule
    // applied per dim). We pick from cA's dims and the filter length
    // by inspecting the natural idwt result on a single row/column.
    size_t la_rows = cA.dims().rows();
    size_t la_cols = cA.dims().cols();

    // Probe filter length by running a tiny idwt.
    size_t natRows = 0, natCols = 0;
    {
        // Use a dummy column of length la_rows to discover natural row length.
        auto a = Value::matrix(la_rows, 1, ValueType::DOUBLE, mr);
        auto d = Value::matrix(la_rows, 1, ValueType::DOUBLE, mr);
        Value probe = idwt(a, d, wname, /*len=*/-1, mr);
        natRows = probe.numel();
    }
    {
        auto a = Value::matrix(la_cols, 1, ValueType::DOUBLE, mr);
        auto d = Value::matrix(la_cols, 1, ValueType::DOUBLE, mr);
        Value probe = idwt(a, d, wname, /*len=*/-1, mr);
        natCols = probe.numel();
    }

    const size_t targetRows = (outRows >= 0)
                              ? static_cast<size_t>(outRows) : natRows;
    const size_t targetCols = (outCols >= 0)
                              ? static_cast<size_t>(outCols) : natCols;

    // Step 1: invert the column pass, recovering LoR (from cA, cH) and
    // HiR (from cV, cD), each with `targetRows` rows.
    Value LoR = idwt_cols(cA, cH, wname, targetRows, mr);
    Value HiR = idwt_cols(cV, cD, wname, targetRows, mr);

    // Step 2: invert the row pass.
    return idwt_rows(LoR, HiR, wname, targetCols, mr);
}

namespace detail {

static std::string argString(const Value &v) {
    if (!v.isChar() && !v.isString())
        throw Error("wavelet: expected string argument",
                    0, 0, "", "", "numkit:wavelet:type");
    return v.toString();
}

void dwt2_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
              CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("dwt2: requires (X, wname)",
                    0, 0, "dwt2", "", "numkit:dwt2:nargin");
    auto *mr = ctx.engine->resource();
    auto r = dwt2(args[0], argString(args[1]), mr);
    if (outs.size() >= 1) outs[0] = std::move(r.cA);
    if (outs.size() >= 2) outs[1] = std::move(r.cH);
    if (outs.size() >= 3) outs[2] = std::move(r.cV);
    if (outs.size() >= 4) outs[3] = std::move(r.cD);
}

void idwt2_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
               CallContext &ctx)
{
    if (args.size() < 5)
        throw Error("idwt2: requires (cA, cH, cV, cD, wname [, sx])",
                    0, 0, "idwt2", "", "numkit:idwt2:nargin");
    long long outRows = -1, outCols = -1;
    if (args.size() >= 6 && !args[5].isEmpty()) {
        // sx form: a length-2 vector [rows cols], or a scalar (square).
        if (args[5].numel() == 1) {
            outRows = outCols = static_cast<long long>(args[5].toScalar());
        } else if (args[5].numel() >= 2) {
            outRows = static_cast<long long>(args[5].elemAsDouble(0));
            outCols = static_cast<long long>(args[5].elemAsDouble(1));
        }
    }
    outs[0] = idwt2(args[0], args[1], args[2], args[3],
                    argString(args[4]), outRows, outCols,
                    ctx.engine->resource());
}

} // namespace detail

} // namespace numkit::wavelet
