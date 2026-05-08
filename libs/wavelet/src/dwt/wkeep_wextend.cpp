// libs/wavelet/src/dwt/wkeep_wextend.cpp
//
// Vector-form wkeep and wextend (Wavelet Toolbox boundary helpers).

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
                    0, 0, "wkeep", "", "m:wkeep:nargin");
    const Value &x = args[0];
    auto *mr = ctx.engine->resource();
    size_t rows, cols;
    readShape(x, rows, cols);

    // Detect 2-D form: args[1] is a 2-element vector.
    if (args[1].numel() == 2) {
        const long long R = static_cast<long long>(args[1].elemAsDouble(0));
        const long long C = static_cast<long long>(args[1].elemAsDouble(1));
        if (R < 0 || C < 0 ||
            static_cast<size_t>(R) > rows || static_cast<size_t>(C) > cols)
            throw Error("wkeep: requested [R C] out of bounds",
                        0, 0, "wkeep", "", "m:wkeep:range");
        long long fr1, fc1;  // 1-based corners
        if (args.size() >= 3 && args[2].numel() == 2) {
            fr1 = static_cast<long long>(args[2].elemAsDouble(0));
            fc1 = static_cast<long long>(args[2].elemAsDouble(1));
        } else {
            // Central — same formula as 1-D: floor((N - n) / 2) + 1.
            fr1 = static_cast<long long>((rows - static_cast<size_t>(R)) / 2) + 1;
            fc1 = static_cast<long long>((cols - static_cast<size_t>(C)) / 2) + 1;
        }
        if (fr1 < 1 || fc1 < 1 ||
            fr1 + R - 1 > static_cast<long long>(rows) ||
            fc1 + C - 1 > static_cast<long long>(cols))
            throw Error("wkeep: 2-D window out of range",
                        0, 0, "wkeep", "", "m:wkeep:range");
        Value y = Value::matrix(static_cast<size_t>(R),
                                static_cast<size_t>(C), ValueType::DOUBLE, mr);
        if (R == 0 || C == 0) { outs[0] = y; return; }
        double *yd = y.doubleDataMut();
        // Column-major copy.
        for (long long c = 0; c < C; ++c) {
            const size_t srcC = static_cast<size_t>(fc1 - 1 + c);
            for (long long r = 0; r < R; ++r) {
                const size_t srcR = static_cast<size_t>(fr1 - 1 + r);
                yd[c * R + r] = x.elemAsDouble(srcC * rows + srcR);
            }
        }
        outs[0] = y;
        return;
    }

    // 1-D form (original logic).
    const size_t N = rows * cols;
    const long long n = static_cast<long long>(args[1].toScalar());
    if (n < 0 || static_cast<size_t>(n) > N)
        throw Error("wkeep: n must satisfy 0 ≤ n ≤ length(x)",
                    0, 0, "wkeep", "", "m:wkeep:n");

    long long start1 = 1;                                  // 1-based start
    if (args.size() >= 3) {
        const Value &opt = args[2];
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
                            0, 0, "wkeep", "", "m:wkeep:opt");
        } else {
            start1 = static_cast<long long>(opt.toScalar());
        }
    } else {
        start1 = static_cast<long long>((N - static_cast<size_t>(n)) / 2) + 1;
    }
    if (start1 < 1 || start1 + n - 1 > static_cast<long long>(N))
        throw Error("wkeep: requested window is out of range",
                    0, 0, "wkeep", "", "m:wkeep:range");

    const bool col = isCol(rows, cols);
    size_t outRows, outCols;
    outShape(col, static_cast<size_t>(n), outRows, outCols);
    Value y = Value::matrix(outRows, outCols, ValueType::DOUBLE, mr);
    if (n == 0) { outs[0] = y; return; }
    double *yd = y.doubleDataMut();
    for (long long k = 0; k < n; ++k)
        yd[k] = x.elemAsDouble(static_cast<size_t>(start1 - 1 + k));
    outs[0] = y;
}

// y = wextend(1, mode, x, lf[, side])
//   side == 'b' (default) — extend both sides by lf
//   side == 'l'           — left only
//   side == 'r'           — right only
//
// Modes (1-D):
//   'sym' / 'symh' — whole-point symmetric (edge sample replicated)
//                    pre-pad = [x(lf), …, x(1)],  post-pad = [x(N), …, x(N-lf+1)]
//   'per'          — periodic with edge-pad on odd N. If N is odd MATLAB
//                    pads x → x' = [x x(end)] (length N+1, even); then
//                    pre-pad = last lf of x', post-pad = first lf of x'.
//   'zpd'          — zero pad
//   'ppd'          — pure periodic: pre-pad = last lf of x; post-pad = first lf
//
// Verified vs MATLAB R2025b for x = [1 2 3 4 5], lf = 2:
//   'sym' → [2 1 | 1 2 3 4 5 | 5 4]
//   'per' → [5 5 | 1 2 3 4 5 5 | 1 2]   (odd N → edge-padded internally)
//   'zpd' → [0 0 | 1 2 3 4 5 | 0 0]
//   'ppd' → [4 5 | 1 2 3 4 5 | 1 2]
void wextend_reg(Span<const Value> args, size_t /*nargout*/,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 4)
        throw Error("wextend: requires (dim, mode, x, lf[, side])",
                    0, 0, "wextend", "", "m:wextend:nargin");
    const int dim = static_cast<int>(args[0].toScalar());
    if (dim != 1)
        throw Error("wextend: only 1-D extension (dim==1) is supported",
                    0, 0, "wextend", "", "m:wextend:dim");
    if (!args[1].isChar() && !args[1].isString())
        throw Error("wextend: mode must be a character vector",
                    0, 0, "wextend", "", "m:wextend:mode");
    const std::string mode = lower(args[1].toString());
    const Value &x = args[2];
    const long long lf = static_cast<long long>(args[3].toScalar());
    if (lf < 0)
        throw Error("wextend: lf must be ≥ 0",
                    0, 0, "wextend", "", "m:wextend:lf");

    char side = 'b';
    if (args.size() >= 5 && (args[4].isChar() || args[4].isString())) {
        const std::string s = lower(args[4].toString());
        if      (s == "b" || s == "both")  side = 'b';
        else if (s == "l" || s == "left")  side = 'l';
        else if (s == "r" || s == "right") side = 'r';
        else
            throw Error("wextend: unknown side '" + s +
                        "' (expected 'b', 'l' or 'r')",
                        0, 0, "wextend", "", "m:wextend:side");
    }

    size_t rows, cols;
    readShape(x, rows, cols);
    const size_t N = rows * cols;
    const bool col = isCol(rows, cols);
    std::vector<double> xv(N);
    for (size_t i = 0; i < N; ++i) xv[i] = x.elemAsDouble(i);

    // Build "source" — used by 'per' for the odd-N edge replication.
    std::vector<double> src = xv;
    if (mode == "per" && (N % 2) == 1 && N > 0) {
        src.push_back(xv.back());                       // append x(end)
    }
    const size_t M = src.size();

    auto sampleLeft = [&](long long k) -> double {       // k = 0..lf-1, 0 = closest to data
        // 'sym': prepend [x(lf), …, x(1)]  → output position k holds x(k+1) reflected = x(lf-k)
        //   The sequence written in order [k=lf-1, lf-2, …, 0] is [x(lf), …, x(1)]
        //   so left-extension index j (0..lf-1) is x(lf - j) where j = pad-buffer index.
        // We'll allow caller to pass k = 0 meaning the FIRST left-pad sample, i.e. furthest from data.
        // sym pad order (left): [x(lf), x(lf-1), …, x(1)]    written for j = 0..lf-1
        //   → src index = lf-1-j        (0-based on x)
        if (mode == "sym" || mode == "symh") {
            const long long idx = lf - 1 - k;            // 0-based into x
            return xv[static_cast<size_t>(idx)];
        }
        if (mode == "per") {
            // pre-pad = last lf of src: src[M-lf .. M-1]   (in natural order)
            return src[static_cast<size_t>(M - static_cast<size_t>(lf) + k)];
        }
        if (mode == "ppd") {
            return xv[static_cast<size_t>(N - static_cast<size_t>(lf) + k)];
        }
        // zpd
        return 0.0;
    };
    auto sampleRight = [&](long long k) -> double {      // k = 0..lf-1, 0 = closest to data
        if (mode == "sym" || mode == "symh") {
            // post-pad = [x(N), x(N-1), …, x(N-lf+1)] (in order)
            const long long idx = static_cast<long long>(N) - 1 - k;
            return xv[static_cast<size_t>(idx)];
        }
        if (mode == "per") {
            return src[static_cast<size_t>(k)];
        }
        if (mode == "ppd") {
            return xv[static_cast<size_t>(k)];
        }
        // zpd
        return 0.0;
    };

    if (mode != "sym" && mode != "symh" && mode != "per" &&
        mode != "zpd" && mode != "ppd")
        throw Error("wextend: unsupported mode '" + mode +
                    "' (supported: sym, symh, per, zpd, ppd)",
                    0, 0, "wextend", "", "m:wextend:mode");

    // For 'per' the data segment in the output is the (possibly
    // edge-padded) src, not x itself.
    const std::vector<double> &mid = (mode == "per") ? src : xv;
    const size_t Mlen = mid.size();
    const size_t leftN  = (side == 'r') ? 0 : static_cast<size_t>(lf);
    const size_t rightN = (side == 'l') ? 0 : static_cast<size_t>(lf);
    const size_t outN = leftN + Mlen + rightN;

    size_t outRows, outCols;
    outShape(col, outN, outRows, outCols);
    Value y = Value::matrix(outRows, outCols, ValueType::DOUBLE,
                            ctx.engine->resource());
    if (outN == 0) { outs[0] = y; return; }
    double *yd = y.doubleDataMut();
    for (size_t i = 0; i < leftN; ++i)
        yd[i] = sampleLeft(static_cast<long long>(i));
    for (size_t i = 0; i < Mlen; ++i)
        yd[leftN + i] = mid[i];
    for (size_t i = 0; i < rightN; ++i)
        yd[leftN + Mlen + i] = sampleRight(static_cast<long long>(i));
    outs[0] = y;
}

} // namespace detail
} // namespace numkit::wavelet
