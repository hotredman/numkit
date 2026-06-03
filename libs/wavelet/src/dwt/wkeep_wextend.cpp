// libs/wavelet/src/dwt/wkeep_wextend.cpp
//
// Vector-form wkeep and wextend (Wavelet Toolbox boundary helpers).

#include <numkit/wavelet/dwt/wkeep_wextend.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>
#include <numkit/core/value.hpp>

#include <algorithm>
#include <string>
#include <vector>

namespace numkit::wavelet {

namespace {

void readShape(const Value &x, size_t &rows, size_t &cols)
{
    rows = x.dims().rows();
    cols = x.dims().cols();
    if (rows == 0 && cols == 0) { rows = 1; cols = 0; }
}

bool isCol(size_t rows, size_t cols)
{
    return cols == 1 && rows > 1;
}

void outShape(bool col, size_t outN,
              size_t &outRows, size_t &outCols)
{
    if (col) { outRows = outN; outCols = 1; }
    else     { outRows = 1;    outCols = outN; }
}

std::string lower(std::string s)
{
    for (auto &c : s)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

} // anonymous

// ── Public C++ API (see dwt/wkeep_wextend.hpp) ────────────────────────

Value wkeep(const Value &x, const Value &len, const Value &opt,
            std::pmr::memory_resource *mr)
{
    size_t rows, cols;
    readShape(x, rows, cols);

    // 2-D form: `len` is a 2-element vector → extract an R×C sub-matrix.
    if (len.numel() == 2) {
        const long long R = static_cast<long long>(len.elemAsDouble(0));
        const long long C = static_cast<long long>(len.elemAsDouble(1));
        if (R < 0 || C < 0 ||
            static_cast<size_t>(R) > rows || static_cast<size_t>(C) > cols)
            throw Error("wkeep: requested [R C] out of bounds",
                        0, 0, "wkeep", "", "numkit:wkeep:range");
        long long fr1, fc1;  // 1-based corners
        if (!opt.isEmpty() && opt.numel() == 2) {
            fr1 = static_cast<long long>(opt.elemAsDouble(0));
            fc1 = static_cast<long long>(opt.elemAsDouble(1));
        } else {
            // Central — same formula as 1-D: floor((N - n) / 2) + 1.
            fr1 = static_cast<long long>((rows - static_cast<size_t>(R)) / 2) + 1;
            fc1 = static_cast<long long>((cols - static_cast<size_t>(C)) / 2) + 1;
        }
        if (fr1 < 1 || fc1 < 1 ||
            fr1 + R - 1 > static_cast<long long>(rows) ||
            fc1 + C - 1 > static_cast<long long>(cols))
            throw Error("wkeep: 2-D window out of range",
                        0, 0, "wkeep", "", "numkit:wkeep:range");
        Value y = Value::matrix(static_cast<size_t>(R),
                                static_cast<size_t>(C), ValueType::DOUBLE, mr);
        if (R == 0 || C == 0) return y;
        double *yd = y.doubleDataMut();
        for (long long c = 0; c < C; ++c) {
            const size_t srcC = static_cast<size_t>(fc1 - 1 + c);
            for (long long r = 0; r < R; ++r) {
                const size_t srcR = static_cast<size_t>(fr1 - 1 + r);
                yd[c * R + r] = x.elemAsDouble(srcC * rows + srcR);
            }
        }
        return y;
    }

    // 1-D form.
    const size_t N = rows * cols;
    const long long n = static_cast<long long>(len.toScalar());
    if (n < 0 || static_cast<size_t>(n) > N)
        throw Error("wkeep: n must satisfy 0 ≤ n ≤ length(x)",
                    0, 0, "wkeep", "", "numkit:wkeep:n");

    long long start1 = 1;                                  // 1-based start
    if (!opt.isEmpty()) {
        if (opt.isChar() || opt.isString()) {
            const std::string s = lower(opt.toString());
            if (s == "c" || s == "central" || s == "centered" || s == "centred")
                start1 = static_cast<long long>((N - static_cast<size_t>(n)) / 2) + 1;
            else if (s == "l")
                start1 = 1;
            else if (s == "r")
                start1 = static_cast<long long>(N) - n + 1;
            else
                throw Error("wkeep: unknown option '" + s +
                            "' (expected 'c', 'l', 'r' or a numeric start)",
                            0, 0, "wkeep", "", "numkit:wkeep:opt");
        } else {
            start1 = static_cast<long long>(opt.toScalar());
        }
    } else {
        start1 = static_cast<long long>((N - static_cast<size_t>(n)) / 2) + 1;
    }
    if (start1 < 1 || start1 + n - 1 > static_cast<long long>(N))
        throw Error("wkeep: requested window is out of range",
                    0, 0, "wkeep", "", "numkit:wkeep:range");

    const bool col = isCol(rows, cols);
    size_t outRows, outCols;
    outShape(col, static_cast<size_t>(n), outRows, outCols);
    Value y = Value::matrix(outRows, outCols, ValueType::DOUBLE, mr);
    if (n == 0) return y;
    double *yd = y.doubleDataMut();
    for (long long k = 0; k < n; ++k)
        yd[k] = x.elemAsDouble(static_cast<size_t>(start1 - 1 + k));
    return y;
}

namespace detail {

// y = wkeep(x, n[, OPT])             — 1-D form
// y = wkeep(x, [R C][, [fr fc]])     — 2-D form (matrix sub-extraction)
//   OPT == 'c' (default) → centred:   start = floor((N-n)/2) + 1   (1-based)
//   OPT == 'l'           → first n
//   OPT == 'r'           → last n
//   OPT numeric (FIRST)  → x(FIRST : FIRST+n-1)   (1-based start)
//
// 2-D: when args[1] has numel()==2, we extract a central [R x C] sub-matrix
// (default) or an explicit corner [fr fc] when args[2] is a 2-vector.
//
// Verified vs MATLAB R2025b:
//   wkeep(1:10, 4)            → [4 5 6 7]
//   wkeep(1:10, 4, 'l')       → [1 2 3 4]
//   wkeep(magic(5), [3 3])    → [5 7 14; 6 13 20; 12 19 21]   (central)
//   wkeep(magic(5), [3 3], [1 1]) → [17 24 1; 23 5 7; 4 6 13] (top-left)
//
// Bug fix 2026-05-08: 2-D form was throwing "Cannot convert double to scalar"
// because adapter did args[1].toScalar() unconditionally.
void wkeep_reg(Span<const Value> args, size_t /*nargout*/,
               Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("wkeep: requires (x, n[, OPT]) or (X, [R C][, [fr fc]])",
                    0, 0, "wkeep", "", "numkit:wkeep:nargin");
    outs[0] = wkeep(args[0], args[1],
                    args.size() >= 3 ? args[2] : Value::Empty,
                    ctx.engine->resource());
}

// 1-D extension core. Takes a length-N source `xv` and produces an output
// vector with `leftN` left-pad samples + the (possibly edge-padded for
// 'per') middle + `rightN` right-pad samples.
//
// Modes (verified vs MATLAB R2025b on x = [1 2 3 4 5], lf = 2):
//   'sym' / 'symh' — half-point symmetric (edge sample replicated)
//                    pre = [x(lf), …, x(1)],   post = [x(N), …, x(N-lf+1)]
//                    [2 1 | 1 2 3 4 5 | 5 4]
//   'symw'         — whole-point symmetric (edge NOT replicated)
//                    pre = [x(lf+1), …, x(2)], post = [x(N-1), …, x(N-lf)]
//                    [3 2 | 1 2 3 4 5 | 4 3]
//   'asym' / 'asymh' — half-point antisymmetric
//                    pre = -[x(lf), …, x(1)],  post = -[x(N), …, x(N-lf+1)]
//                    [-2 -1 | 1 2 3 4 5 | -5 -4]
//   'asymw'        — whole-point antisymmetric (reflect through endpoint)
//                    pre[i]  = 2·x(1) - x(lf-i+1)  reversed → 2·x(1) - x(j)
//                    [-1 0 | 1 2 3 4 5 | 6 7]   (linear-like for x=1..5)
//   'sp0'          — order-0 spline = replicate edge sample
//                    [1 1 | 1..5 | 5 5]
//   'sp1'          — order-1 spline = linear extrapolation
//                    pre[i]  = x(1) - (lf-i)·(x(2) - x(1))   for i=0..lf-1
//                    post[i] = x(N) + (i+1)·(x(N) - x(N-1))
//                    [-1 0 | 1..5 | 6 7]
//   'per'          — periodic with edge-pad on odd N. If N odd, MATLAB pads
//                    x → x' = [x x(end)]; pre = last lf of x', post = first lf.
//   'zpd'          — zero pad
//   'ppd'          — pure periodic: pre = last lf of x; post = first lf
static std::vector<double> extend1D(const std::vector<double> &xv,
                                    long long lf, char side,
                                    const std::string &mode)
{
    const long long N = static_cast<long long>(xv.size());
    if (lf < 0) lf = 0;
    // For 'per' the data segment is the (possibly edge-padded) version of x.
    std::vector<double> src = xv;
    if (mode == "per" && (N % 2) == 1 && N > 0) src.push_back(xv.back());
    const long long M = static_cast<long long>(src.size());

    auto sampleLeft = [&](long long k) -> double {
        // k = 0..lf-1, with k=0 = furthest from data (output[0]).
        if (mode == "sym" || mode == "symh") {
            return xv[static_cast<size_t>(lf - 1 - k)];
        }
        if (mode == "symw") {
            // pre = [x(lf+1), …, x(2)] in 1-based → indices [lf, lf-1, …, 1] 0-based.
            // For output position k (0..lf-1), src idx = lf - k.
            const long long idx = lf - k;
            return (idx < N) ? xv[static_cast<size_t>(idx)] : xv.back();
        }
        if (mode == "asym" || mode == "asymh") {
            return -xv[static_cast<size_t>(lf - 1 - k)];
        }
        if (mode == "asymw") {
            // 2·x(1) - x(j) where j is the symw index (1-based j = lf+1-k = lf-k+1).
            // 0-based: idx = lf - k.
            const long long idx = lf - k;
            const double xj = (idx < N) ? xv[static_cast<size_t>(idx)] : xv.back();
            return 2.0 * xv[0] - xj;
        }
        if (mode == "sp0") {
            return xv[0];
        }
        if (mode == "sp1") {
            // Linear extrapolation using slope x(2) - x(1).
            const double slope = (N >= 2) ? (xv[1] - xv[0]) : 0.0;
            return xv[0] - (lf - k) * slope;
        }
        if (mode == "per") {
            return src[static_cast<size_t>(M - lf + k)];
        }
        if (mode == "ppd") {
            return xv[static_cast<size_t>(N - lf + k)];
        }
        return 0.0;  // zpd
    };
    auto sampleRight = [&](long long k) -> double {
        // k = 0..lf-1, k=0 = closest to data.
        if (mode == "sym" || mode == "symh") {
            return xv[static_cast<size_t>(N - 1 - k)];
        }
        if (mode == "symw") {
            // post = [x(N-1), …, x(N-lf)] in 1-based → indices [N-2, N-3, …, N-lf-1] 0-based.
            const long long idx = N - 2 - k;
            return (idx >= 0) ? xv[static_cast<size_t>(idx)] : xv.front();
        }
        if (mode == "asym" || mode == "asymh") {
            return -xv[static_cast<size_t>(N - 1 - k)];
        }
        if (mode == "asymw") {
            const long long idx = N - 2 - k;
            const double xj = (idx >= 0) ? xv[static_cast<size_t>(idx)] : xv.front();
            return 2.0 * xv[static_cast<size_t>(N - 1)] - xj;
        }
        if (mode == "sp0") {
            return xv[static_cast<size_t>(N - 1)];
        }
        if (mode == "sp1") {
            const double slope = (N >= 2) ? (xv[static_cast<size_t>(N - 1)]
                                              - xv[static_cast<size_t>(N - 2)]) : 0.0;
            return xv[static_cast<size_t>(N - 1)] + (k + 1) * slope;
        }
        if (mode == "per") {
            return src[static_cast<size_t>(k)];
        }
        if (mode == "ppd") {
            return xv[static_cast<size_t>(k)];
        }
        return 0.0;
    };

    const long long leftN  = (side == 'r') ? 0 : lf;
    const long long rightN = (side == 'l') ? 0 : lf;
    const std::vector<double> &mid = (mode == "per") ? src : xv;
    const long long Mlen = static_cast<long long>(mid.size());
    std::vector<double> out;
    out.reserve(static_cast<size_t>(leftN + Mlen + rightN));
    for (long long i = 0; i < leftN; ++i) out.push_back(sampleLeft(i));
    for (long long i = 0; i < Mlen;  ++i) out.push_back(mid[static_cast<size_t>(i)]);
    for (long long i = 0; i < rightN; ++i) out.push_back(sampleRight(i));
    return out;
}

// y = wextend(type, mode, x, lf[, side])
//   type = 1                 — 1-D vector extension
//   type = 2                 — 2-D matrix extension (extend rows AND cols)
//   type = 'ar' (along row)  — extend cols only
//   type = 'ac' (along col)  — extend rows only
// Bug fix 2026-05-08: added 'symw' / 'asym' / 'asymw' / 'sp0' / 'sp1'
// modes and the type=2 / 'ar' / 'ac' forms.
void wextend_reg(Span<const Value> args, size_t /*nargout*/,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 4)
        throw Error("wextend: requires (type, mode, x, lf[, side])",
                    0, 0, "wextend", "", "numkit:wextend:nargin");

    // type: 1, 2, 'ar', 'ac'.
    int dim = 0;          // 0 = 2-D both axes (when type=2)
    bool extRows = true;
    bool extCols = true;
    if (args[0].isChar() || args[0].isString()) {
        const std::string s = lower(args[0].toString());
        if (s == "1") dim = 1;
        else if (s == "2") dim = 2;
        // MATLAB convention: 'ar' = along row direction = add rows (extend
        // each column), so cols stay fixed. 'ac' = along col direction =
        // add cols (extend each row), so rows stay fixed. (Counter-
        // intuitive naming but matches help wextend.)
        else if (s == "ar") { dim = 2; extRows = true;  extCols = false; }
        else if (s == "ac") { dim = 2; extRows = false; extCols = true; }
        else
            throw Error("wextend: type must be 1, 2, 'ar', or 'ac'",
                        0, 0, "wextend", "", "numkit:wextend:dim");
    } else {
        const int t = static_cast<int>(args[0].toScalar());
        if (t != 1 && t != 2)
            throw Error("wextend: type must be 1, 2, 'ar', or 'ac'",
                        0, 0, "wextend", "", "numkit:wextend:dim");
        dim = t;
    }
    if (!args[1].isChar() && !args[1].isString())
        throw Error("wextend: mode must be a character vector",
                    0, 0, "wextend", "", "numkit:wextend:mode");
    const std::string mode = lower(args[1].toString());
    static const char *kModes[] = {"sym", "symh", "symw",
                                    "asym", "asymh", "asymw",
                                    "sp0", "sp1",
                                    "per", "zpd", "ppd"};
    bool modeOK = false;
    for (const char *m : kModes) if (mode == m) { modeOK = true; break; }
    if (!modeOK)
        throw Error("wextend: unsupported mode '" + mode +
                    "' (supported: sym/symh/symw, asym/asymh/asymw, "
                    "sp0/sp1, per, zpd, ppd)",
                    0, 0, "wextend", "", "numkit:wextend:mode");
    const Value &x = args[2];
    const long long lf = static_cast<long long>(args[3].toScalar());
    if (lf < 0)
        throw Error("wextend: lf must be ≥ 0",
                    0, 0, "wextend", "", "numkit:wextend:lf");

    char side = 'b';
    if (args.size() >= 5 && (args[4].isChar() || args[4].isString())) {
        const std::string s = lower(args[4].toString());
        if      (s == "b" || s == "both")  side = 'b';
        else if (s == "l" || s == "left")  side = 'l';
        else if (s == "r" || s == "right") side = 'r';
        else
            throw Error("wextend: unknown side '" + s +
                        "' (expected 'b', 'l' or 'r')",
                        0, 0, "wextend", "", "numkit:wextend:side");
    }

    auto *mr = ctx.engine->resource();
    size_t rows, cols;
    readShape(x, rows, cols);

    if (dim == 1) {
        // 1-D path.
        const size_t N = rows * cols;
        const bool col = isCol(rows, cols);
        std::vector<double> xv(N);
        for (size_t i = 0; i < N; ++i) xv[i] = x.elemAsDouble(i);
        std::vector<double> ext = extend1D(xv, lf, side, mode);
        size_t outRows, outCols;
        outShape(col, ext.size(), outRows, outCols);
        Value y = Value::matrix(outRows, outCols, ValueType::DOUBLE, mr);
        std::copy(ext.begin(), ext.end(), y.doubleDataMut());
        outs[0] = std::move(y);
        return;
    }

    // 2-D matrix path (type=2 / 'ar' / 'ac'). Extend along columns first
    // (vary col indices = extend rows direction = adds new rows) when
    // extRows; then extend along rows (vary row index per column = adds
    // cols) when extCols. Either step can be skipped via 'ar'/'ac'.
    std::vector<std::vector<double>> data(rows, std::vector<double>(cols));
    for (size_t r = 0; r < rows; ++r)
        for (size_t c = 0; c < cols; ++c)
            data[r][c] = x.elemAsDouble(c * rows + r);

    // Step 1: if extRows, extend each column (add rows).
    if (extRows) {
        std::vector<std::vector<double>> ext_col;
        ext_col.reserve(cols);
        for (size_t c = 0; c < cols; ++c) {
            std::vector<double> col_v(rows);
            for (size_t r = 0; r < rows; ++r) col_v[r] = data[r][c];
            ext_col.push_back(extend1D(col_v, lf, side, mode));
        }
        const size_t newRows = ext_col[0].size();
        data.assign(newRows, std::vector<double>(cols));
        for (size_t c = 0; c < cols; ++c)
            for (size_t r = 0; r < newRows; ++r)
                data[r][c] = ext_col[c][r];
        rows = newRows;
    }

    // Step 2: if extCols, extend each row (add cols).
    if (extCols) {
        std::vector<std::vector<double>> ext_row;
        ext_row.reserve(rows);
        for (size_t r = 0; r < rows; ++r) {
            std::vector<double> row_v(cols);
            for (size_t c = 0; c < cols; ++c) row_v[c] = data[r][c];
            ext_row.push_back(extend1D(row_v, lf, side, mode));
        }
        const size_t newCols = ext_row[0].size();
        for (auto &row : data) row.assign(newCols, 0.0);
        for (size_t r = 0; r < rows; ++r)
            for (size_t c = 0; c < newCols; ++c)
                data[r][c] = ext_row[r][c];
        cols = newCols;
    }

    Value y = Value::matrix(rows, cols, ValueType::DOUBLE, mr);
    double *yd = y.doubleDataMut();
    for (size_t c = 0; c < cols; ++c)
        for (size_t r = 0; r < rows; ++r)
            yd[c * rows + r] = data[r][c];
    outs[0] = std::move(y);
}

} // namespace detail
} // namespace numkit::wavelet
