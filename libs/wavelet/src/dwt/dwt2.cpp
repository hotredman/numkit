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
#include <vector>

namespace numkit::wavelet {

namespace {

// Apply 1-D dwt to every row of X. Returns Lo (M × outCols) and Hi.
void dwt_rows(std::pmr::memory_resource *mr,
              const Value &X, const std::string &wname,
              Value *Lo, Value *Hi)
{
    const size_t M = X.dims().rows();
    const size_t N = X.dims().cols();
    if (M == 0 || N == 0) {
        if (Lo) *Lo = Value::matrix(M, 0, ValueType::DOUBLE, mr);
        if (Hi) *Hi = Value::matrix(M, 0, ValueType::DOUBLE, mr);
        return;
    }

    // Determine the per-row output length by running one row first.
    auto row = Value::matrix(1, N, ValueType::DOUBLE, mr);
    double *rd = row.doubleDataMut();
    for (size_t c = 0; c < N; ++c) rd[c] = X.elemAsDouble(c * M + 0);
    Value cA0, cD0;
    dwt(mr, row, wname, &cA0, &cD0);
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
        Value cA, cD;
        dwt(mr, row, wname, &cA, &cD);
        const double *aL = cA.doubleData();
        const double *aH = cD.doubleData();
        for (size_t c = 0; c < outN; ++c) {
            ld[c * M + r] = aL[c];
            hd[c * M + r] = aH[c];
        }
    }
    if (Lo) *Lo = loM;
    if (Hi) *Hi = hiM;
}

// Apply 1-D dwt to every column of X. Returns Lo (outRows × N) and Hi.
void dwt_cols(std::pmr::memory_resource *mr,
              const Value &X, const std::string &wname,
              Value *Lo, Value *Hi)
{
    const size_t M = X.dims().rows();
    const size_t N = X.dims().cols();
    if (M == 0 || N == 0) {
        if (Lo) *Lo = Value::matrix(0, N, ValueType::DOUBLE, mr);
        if (Hi) *Hi = Value::matrix(0, N, ValueType::DOUBLE, mr);
        return;
    }

    auto col = Value::matrix(M, 1, ValueType::DOUBLE, mr);
    double *cd = col.doubleDataMut();
    for (size_t r = 0; r < M; ++r) cd[r] = X.elemAsDouble(0 * M + r);
    Value cA0, cD0;
    dwt(mr, col, wname, &cA0, &cD0);
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
        Value cA, cD;
        dwt(mr, col, wname, &cA, &cD);
        const double *aL = cA.doubleData();
        const double *aH = cD.doubleData();
        for (size_t r = 0; r < outM; ++r) {
            ld[c * outM + r] = aL[r];
            hd[c * outM + r] = aH[r];
        }
    }
    if (Lo) *Lo = loM;
    if (Hi) *Hi = hiM;
}

// Inverse: combine a (lo, hi) pair into a single matrix by running
// 1-D idwt along each column, with target length outRows.
Value idwt_cols(std::pmr::memory_resource *mr,
                const Value &Lo, const Value &Hi,
                const std::string &wname,
                size_t outRows)
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
        Value y = idwt(mr, colA, colD, wname,
                       static_cast<long long>(outRows));
        const double *yd = y.doubleData();
        for (size_t r = 0; r < outRows; ++r)
            od[c * outRows + r] = yd[r];
    }
    return out;
}

// Inverse along rows, target length outCols.
Value idwt_rows(std::pmr::memory_resource *mr,
                const Value &Lo, const Value &Hi,
                const std::string &wname,
                size_t outCols)
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
        Value y = idwt(mr, rowA, rowD, wname,
                       static_cast<long long>(outCols));
        const double *yd = y.doubleData();
        for (size_t c = 0; c < outCols; ++c)
            od[c * M + r] = yd[c];
    }
    return out;
}

} // anonymous

void dwt2(std::pmr::memory_resource *mr,
          const Value &X, const std::string &wname,
          Value *cA, Value *cH, Value *cV, Value *cD)
{
    // Pass 1: rows (each row → Lo + Hi halves, each M × outN).
    Value LoR, HiR;
    dwt_rows(mr, X, wname, &LoR, &HiR);

    // Pass 2: columns of LoR  → cA / cH; columns of HiR → cV / cD.
    Value Lo_of_LoR, Hi_of_LoR, Lo_of_HiR, Hi_of_HiR;
    dwt_cols(mr, LoR, wname, &Lo_of_LoR, &Hi_of_LoR);
    dwt_cols(mr, HiR, wname, &Lo_of_HiR, &Hi_of_HiR);

    if (cA) *cA = Lo_of_LoR;          // LL
    if (cH) *cH = Hi_of_LoR;          // LH (horizontal detail)
    if (cV) *cV = Lo_of_HiR;          // HL (vertical detail)
    if (cD) *cD = Hi_of_HiR;          // HH (diagonal detail)
}

Value idwt2(std::pmr::memory_resource *mr,
            const Value &cA, const Value &cH,
            const Value &cV, const Value &cD,
            const std::string &wname,
            long long outRows, long long outCols)
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
        Value probe = idwt(mr, a, d, wname, /*len=*/-1);
        natRows = probe.numel();
    }
    {
        auto a = Value::matrix(la_cols, 1, ValueType::DOUBLE, mr);
        auto d = Value::matrix(la_cols, 1, ValueType::DOUBLE, mr);
        Value probe = idwt(mr, a, d, wname, /*len=*/-1);
        natCols = probe.numel();
    }

    const size_t targetRows = (outRows >= 0)
                              ? static_cast<size_t>(outRows) : natRows;
    const size_t targetCols = (outCols >= 0)
                              ? static_cast<size_t>(outCols) : natCols;

    // Step 1: invert the column pass, recovering LoR (from cA, cH) and
    // HiR (from cV, cD), each with `targetRows` rows.
    Value LoR = idwt_cols(mr, cA, cH, wname, targetRows);
    Value HiR = idwt_cols(mr, cV, cD, wname, targetRows);

    // Step 2: invert the row pass.
    return idwt_rows(mr, LoR, HiR, wname, targetCols);
}

namespace detail {

static std::string argString(const Value &v) {
    if (!v.isChar() && !v.isString())
        throw Error("wavelet: expected string argument",
                    0, 0, "", "", "m:wavelet:type");
    return v.toString();
}

void dwt2_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
              CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("dwt2: requires (X, wname)",
                    0, 0, "dwt2", "", "m:dwt2:nargin");
    auto *mr = ctx.engine->resource();
    Value cA, cH, cV, cD;
    dwt2(mr, args[0], argString(args[1]), &cA, &cH, &cV, &cD);
    if (outs.size() >= 1) outs[0] = cA;
    if (outs.size() >= 2) outs[1] = cH;
    if (outs.size() >= 3) outs[2] = cV;
    if (outs.size() >= 4) outs[3] = cD;
}

void idwt2_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
               CallContext &ctx)
{
    if (args.size() < 5)
        throw Error("idwt2: requires (cA, cH, cV, cD, wname [, sx])",
                    0, 0, "idwt2", "", "m:idwt2:nargin");
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
    outs[0] = idwt2(ctx.engine->resource(),
                    args[0], args[1], args[2], args[3],
                    argString(args[4]), outRows, outCols);
}

} // namespace detail

} // namespace numkit::wavelet
