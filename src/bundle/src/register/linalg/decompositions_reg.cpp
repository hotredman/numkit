// toolboxes/linalg/src/decompositions_reg.cpp
//
// Register half of the matrix-factorisation builtins: the CallContext
// wrappers chol / lu / qr / svd / qrupdate / qrinsert / qrdelete /
// cholupdate that delegate to the engine-free compute in decompositions.cpp.
// The no-throw [R,p]=chol form and the [Q,R,P]=qr pivot form reach into the
// shared raw-buffer kernels (cholUpperFactor / transposeSquare / qr_pivoted)
// declared in the private decompositions_detail.hpp. Register-only output
// shaping helpers (firstCols / topLeftBlock / permMatrixToVector / wantsEcon)
// live in this TU's anonymous namespace. library.cpp forward-declares +
// registers the *_reg fns by name.
//
// Phase 2b compute/register split — see project_layering_refactor memory.

#include <numkit/linalg/decompositions.hpp>
#include <numkit/linalg/qz.hpp>
#include <numkit/linalg/gsvd.hpp>
#include <numkit/linalg/ordschur.hpp>
#include <numkit/linalg/eigs.hpp>
#include <numkit/linalg/svd_sketch.hpp>
#include "decompositions_detail.hpp"   // cholUpperFactor / transposeSquare / qr_pivoted

#include <numkit/core/engine.hpp>   // CallContext, Span, ctx.engine->resource()
#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <string>
#include <tuple>
#include <vector>

namespace numkit::linalg {
namespace detail {

void chol_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("chol: requires at least 1 argument",
                    0, 0, "chol", "", "numkit:chol:nargin");
    auto *mr = ctx.engine->resource();
    const Value &A = args[0];

    // Optional 'lower'/'upper' triangle selector (case-insensitive).
    // Default is 'upper': R is upper-triangular with R'*R = A.
    bool lower = false;
    for (size_t i = 1; i < args.size(); ++i) {
        if (!args[i].isChar() && !args[i].isString())
            throw Error("chol: option must be 'lower' or 'upper'",
                        0, 0, "chol", "", "numkit:chol:BadOpt");
        std::string s = args[i].toString();
        for (char &c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if      (s == "lower") lower = true;
        else if (s == "upper") lower = false;
        else
            throw Error("chol: unknown option '" + args[i].toString() + "'",
                        0, 0, "chol", "", "numkit:chol:BadOpt");
    }

    if (A.dims().ndim() != 2)
        throw Error("chol: input must be a 2D matrix",
                    0, 0, "chol", "", "numkit:chol:notMatrix");
    const std::size_t m = static_cast<std::size_t>(A.dims().dim(0));
    const std::size_t n = static_cast<std::size_t>(A.dims().dim(1));
    if (m != n)
        throw Error("chol: matrix must be square",
                    0, 0, "chol", "", "numkit:chol:notSquare");

    if (n == 0) {
        outs[0] = Value::matrix(0, 0, ValueType::DOUBLE, mr);
        if (nargout > 1) outs[1] = Value::scalar(0.0);
        return;
    }

    auto R = Value::matrix(n, n, ValueType::DOUBLE, mr);
    const std::size_t fail = cholUpperFactor(A.doubleData(), R.doubleDataMut(), n);

    if (fail != 0) {
        // Not positive-definite. With <2 outputs MATLAB errors; with the
        // [R,p] form it returns p = failure column and R = the leading
        // (p-1)×(p-1) factor (no error).
        if (nargout < 2)
            throw Error("chol: matrix is not positive-definite",
                        0, 0, "chol", "", "numkit:chol:notPosDef");
        const std::size_t k = fail - 1;
        Value sub = Value::matrix(k, k, ValueType::DOUBLE, mr);
        if (k > 0) {
            const double *rf = R.doubleData();
            double *rs = sub.doubleDataMut();
            for (std::size_t col = 0; col < k; ++col)
                for (std::size_t row = 0; row < k; ++row)
                    rs[row + col * k] = rf[row + col * n];
            if (lower) sub = transposeSquare(sub.doubleData(), k, mr);
        }
        outs[0] = std::move(sub);
        outs[1] = Value::scalar(static_cast<double>(fail));
        return;
    }

    if (lower) R = transposeSquare(R.doubleData(), n, mr);
    outs[0] = std::move(R);
    if (nargout > 1) outs[1] = Value::scalar(0.0);
}

namespace {

// First `k` columns of a DOUBLE matrix (column-major → block copy).
Value firstCols(const Value &A, size_t k, std::pmr::memory_resource *mr)
{
    const size_t r = A.dims().rows();
    auto out = Value::matrix(r, k, ValueType::DOUBLE, mr);
    const double *src = A.doubleData();
    double *dst = out.doubleDataMut();
    for (size_t i = 0; i < r * k; ++i) dst[i] = src[i];
    return out;
}

// Top-left kr×kc block of a DOUBLE matrix.
Value topLeftBlock(const Value &A, size_t kr, size_t kc, std::pmr::memory_resource *mr)
{
    const size_t r = A.dims().rows();
    auto out = Value::matrix(kr, kc, ValueType::DOUBLE, mr);
    const double *src = A.doubleData();
    double *dst = out.doubleDataMut();
    for (size_t c = 0; c < kc; ++c)
        for (size_t i = 0; i < kr; ++i)
            dst[c * kr + i] = src[c * r + i];
    return out;
}

// Convert a permutation matrix P (n×n, P·A = L·U) to a 1-based row-index
// row vector p such that A(p,:) = L·U.
Value permMatrixToVector(const Value &P, std::pmr::memory_resource *mr)
{
    const size_t n = P.dims().rows();
    auto p = Value::matrix(1, n, ValueType::DOUBLE, mr);
    const double *pd = P.doubleData();
    double *out = p.doubleDataMut();
    for (size_t i = 0; i < n; ++i) {
        size_t col = 0;
        double best = -1.0;
        for (size_t j = 0; j < n; ++j) {
            const double v = pd[j * n + i];   // P(i, j), column-major
            if (v > best) { best = v; col = j; }
        }
        out[i] = static_cast<double>(col + 1);   // 1-based
    }
    return p;
}

// Parse a trailing 'econ' / 0 economy flag for svd/qr.
bool wantsEcon(Span<const Value> args, const char *fn)
{
    if (args.size() < 2) return false;
    if (args.size() > 2)
        throw Error(std::string(fn) + ": too many arguments",
                    0, 0, fn, "", std::string("numkit:") + fn + ":nargin");
    const Value &o = args[1];
    if (o.type() == ValueType::CHAR) {
        std::string s = o.toString();
        std::transform(s.begin(), s.end(), s.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        if (s == "econ") return true;
        throw Error(std::string(fn) + ": unknown option '" + s + "'",
                    0, 0, fn, "", std::string("numkit:") + fn + ":badOption");
    }
    if (o.isScalar() && o.toScalar() == 0.0) return true;   // legacy svd(A,0)/qr(A,0)
    throw Error(std::string(fn) + ": invalid second argument",
                0, 0, fn, "", std::string("numkit:") + fn + ":badArg");
}

} // namespace

void lu_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.empty() || args.size() > 2)
        throw Error("lu: requires 1 or 2 arguments",
                    0, 0, "lu", "", "numkit:lu:nargin");
    auto *mr = ctx.engine->resource();

    bool vectorP = false;
    if (args.size() == 2) {
        if (args[1].type() != ValueType::CHAR)
            throw Error("lu: second argument must be the flag 'vector'",
                        0, 0, "lu", "", "numkit:lu:badArg");
        std::string s = args[1].toString();
        std::transform(s.begin(), s.end(), s.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        if (s == "vector") vectorP = true;
        else if (s == "matrix") vectorP = false;   // explicit default
        else throw Error("lu: unknown option '" + s + "'",
                         0, 0, "lu", "", "numkit:lu:badOption");
    }

    if (nargout >= 2) {
        auto [L, U, P] = lu_decompose(args[0], mr);
        outs[0] = std::move(L);
        outs[1] = std::move(U);
        if (nargout >= 3)
            outs[2] = (vectorP && nargout >= 3) ? permMatrixToVector(P, mr)
                                                : std::move(P);
    } else {
        outs[0] = lu_combined(args[0], mr);
    }
}

void qr_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    auto *mr = ctx.engine->resource();

    // Options: 'econ' or 0 → economy; 'vector'/'matrix' → P form (3-output only).
    bool econ = false, vectorP = false;
    for (size_t i = 1; i < args.size(); ++i) {
        const Value &o = args[i];
        if (o.type() == ValueType::CHAR || o.isString()) {
            std::string s = o.toString();
            std::transform(s.begin(), s.end(), s.begin(),
                           [](unsigned char c) { return std::tolower(c); });
            if (s == "econ")        econ = true;
            else if (s == "vector") vectorP = true;
            else if (s == "matrix") vectorP = false;
            else throw Error("qr: unknown option '" + s + "'",
                             0, 0, "qr", "", "numkit:qr:option");
        } else if (o.numel() == 1 && o.toScalar() == 0.0) {
            econ = true;
        } else {
            throw Error("qr: invalid option argument",
                        0, 0, "qr", "", "numkit:qr:option");
        }
    }

    const size_t m = args[0].dims().rows(), n = args[0].dims().cols();

    if (nargout >= 3) {
        // Column-pivoted QR: A·P = Q·R. econ + 3-output → P as a vector (MATLAB).
        std::vector<std::size_t> perm;
        auto [Q, R] = qr_pivoted(args[0], perm, mr);
        if (econ) {
            const size_t k = std::min(m, n);
            Q = firstCols(Q, k, mr);
            R = topLeftBlock(R, k, n, mr);
            vectorP = true;
        }
        outs[0] = std::move(Q);
        outs[1] = std::move(R);
        if (vectorP) {
            Value p = Value::matrix(1, n, ValueType::DOUBLE, mr);
            double *pd = p.doubleDataMut();
            for (size_t k = 0; k < n; ++k) pd[k] = static_cast<double>(perm[k] + 1);
            outs[2] = std::move(p);
        } else {
            Value P = Value::matrix(n, n, ValueType::DOUBLE, mr);
            double *Pd = P.doubleDataMut();
            std::fill(Pd, Pd + n * n, 0.0);
            for (size_t k = 0; k < n; ++k) Pd[perm[k] + k * n] = 1.0;  // P[perm[k], k] = 1
            outs[2] = std::move(P);
        }
        return;
    }

    if (nargout >= 2) {
        auto [Q, R] = qr_decompose(args[0], mr);
        if (econ) {
            const size_t k = std::min(m, n);
            Q = firstCols(Q, k, mr);                // m×k
            R = topLeftBlock(R, k, n, mr);          // k×n
        }
        outs[0] = std::move(Q);
        outs[1] = std::move(R);
    } else {
        Value R = qr_R_only(args[0], mr);
        if (econ) R = topLeftBlock(R, std::min(m, n), n, mr);
        outs[0] = std::move(R);
    }
}

void svd_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    const bool econ = wantsEcon(args, "svd");
    auto *mr = ctx.engine->resource();
    if (nargout >= 2) {
        auto [U, S, V] = svd_decompose(args[0], mr);
        if (econ) {
            const size_t m = args[0].dims().rows(), n = args[0].dims().cols();
            const size_t k = std::min(m, n);
            U = firstCols(U, k, mr);                // m×k
            S = topLeftBlock(S, k, k, mr);          // k×k
            V = firstCols(V, k, mr);                // n×k
        }
        outs[0] = std::move(U);
        outs[1] = std::move(S);
        if (nargout >= 3) outs[2] = std::move(V);
    } else {
        outs[0] = svd_values(args[0], mr);          // 'econ' irrelevant for sv vector
    }
}

void qrupdate_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.size() != 4)
        throw Error("qrupdate: requires (Q, R, u, v)",
                    0, 0, "qrupdate", "", "numkit:qrupdate:nargin");
    auto [Q1, R1] = qrupdate(args[0], args[1], args[2], args[3], ctx.engine->resource());
    outs[0] = std::move(Q1);
    if (nargout >= 2 && outs.size() >= 2) outs[1] = std::move(R1);
}

void qrinsert_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 4 || args.size() > 5)
        throw Error("qrinsert: requires (Q, R, k, x[, 'col']) — row form is not yet supported",
                    0, 0, "qrinsert", "", "numkit:qrinsert:nargin");
    if (args.size() == 5) {
        if (!(args[4].isChar() || args[4].isString())
            || args[4].toString() != "col")
            throw Error("qrinsert: row form not supported in v1 — pass 'col' or omit",
                        0, 0, "qrinsert", "", "numkit:qrinsert:rowDeferred");
    }
    const double kd = args[2].toScalar();
    if (kd < 1.0 || kd != std::floor(kd))
        throw Error("qrinsert: k must be a positive integer",
                    0, 0, "qrinsert", "", "numkit:qrinsert:badK");
    auto [Q1, R1] = qrinsert(args[0], args[1],
                             static_cast<std::size_t>(kd), args[3],
                             ctx.engine->resource());
    outs[0] = std::move(Q1);
    if (nargout >= 2 && outs.size() >= 2) outs[1] = std::move(R1);
}

void qrdelete_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3 || args.size() > 4)
        throw Error("qrdelete: requires (Q, R, k[, 'col']) — row form is not yet supported",
                    0, 0, "qrdelete", "", "numkit:qrdelete:nargin");
    if (args.size() == 4) {
        if (!(args[3].isChar() || args[3].isString())
            || args[3].toString() != "col")
            throw Error("qrdelete: row form not supported in v1 — pass 'col' or omit",
                        0, 0, "qrdelete", "", "numkit:qrdelete:rowDeferred");
    }
    const double kd = args[2].toScalar();
    if (kd < 1.0 || kd != std::floor(kd))
        throw Error("qrdelete: k must be a positive integer",
                    0, 0, "qrdelete", "", "numkit:qrdelete:badK");
    auto [Q1, R1] = qrdelete(args[0], args[1],
                             static_cast<std::size_t>(kd),
                             ctx.engine->resource());
    outs[0] = std::move(Q1);
    if (nargout >= 2 && outs.size() >= 2) outs[1] = std::move(R1);
}

void cholupdate_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2 || args.size() > 3)
        throw Error("cholupdate: requires (R, x[, sign])",
                    0, 0, "cholupdate", "", "numkit:cholupdate:nargin");
    int sign = 1;
    if (args.size() == 3) {
        if (args[2].isChar() || args[2].isString()) {
            std::string s = args[2].toString();
            if      (s == "+") sign = 1;
            else if (s == "-") sign = -1;
            else throw Error("cholupdate: sign must be '+' or '-'",
                             0, 0, "cholupdate", "", "numkit:cholupdate:badSign");
        } else {
            sign = (args[2].toScalar() >= 0.0) ? 1 : -1;
        }
    }
    outs[0] = cholupdate(args[0], args[1], sign, ctx.engine->resource());
}

void qz_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("qz: requires at least 2 arguments (A, B)", 0, 0, "qz", "", "numkit:qz:nargin");
    auto [AA, BB, Q, Z] = qz(args[0], args[1], ctx.engine->resource());
    outs[0] = std::move(AA);
    if (nargout >= 2 && outs.size() >= 2) outs[1] = std::move(BB);
    if (nargout >= 3 && outs.size() >= 3) outs[2] = std::move(Q);
    if (nargout >= 4 && outs.size() >= 4) outs[3] = std::move(Z);
}

void gsvd_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("gsvd: requires at least 2 arguments (A, B)", 0, 0, "gsvd", "", "numkit:gsvd:nargin");
    if (nargout <= 1) {
        outs[0] = gsvd_values(args[0], args[1], ctx.engine->resource());
    } else {
        auto [U, V, X, C, S] = gsvd(args[0], args[1], ctx.engine->resource());
        outs[0] = std::move(U);
        if (outs.size() >= 2) outs[1] = std::move(V);
        if (nargout >= 3 && outs.size() >= 3) outs[2] = std::move(X);
        if (nargout >= 4 && outs.size() >= 4) outs[3] = std::move(C);
        if (nargout >= 5 && outs.size() >= 5) outs[4] = std::move(S);
    }
}

void ordschur_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("ordschur: requires at least 3 arguments (U, T, select/domain)", 0, 0, "ordschur", "", "numkit:ordschur:nargin");
    Value U, T;
    if (args[2].isChar() || args[2].isString()) {
        std::tie(U, T) = ordschur(args[0], args[1], args[2].toString(), ctx.engine->resource());
    } else {
        std::tie(U, T) = ordschur(args[0], args[1], args[2], ctx.engine->resource());
    }
    outs[0] = std::move(U);
    if (nargout >= 2 && outs.size() >= 2) outs[1] = std::move(T);
}

void eigs_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("eigs: requires at least 1 argument", 0, 0, "eigs", "", "numkit:eigs:nargin");
    std::size_t k = (args.size() >= 2 && !args[1].isEmpty()) ? static_cast<std::size_t>(args[1].toScalar()) : 6;
    if (nargout <= 1) {
        outs[0] = eigs_values(args[0], k, ctx.engine->resource());
    } else {
        auto [V, D] = eigs(args[0], k, ctx.engine->resource());
        outs[0] = std::move(V);
        if (outs.size() >= 2) outs[1] = std::move(D);
    }
}

void svds_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("svds: requires at least 1 argument", 0, 0, "svds", "", "numkit:svds:nargin");
    std::size_t k = (args.size() >= 2 && !args[1].isEmpty()) ? static_cast<std::size_t>(args[1].toScalar()) : 6;
    if (nargout <= 1) {
        outs[0] = svds_values(args[0], k, ctx.engine->resource());
    } else {
        auto [U, S, V] = svds(args[0], k, ctx.engine->resource());
        outs[0] = std::move(U);
        if (outs.size() >= 2) outs[1] = std::move(S);
        if (nargout >= 3 && outs.size() >= 3) outs[2] = std::move(V);
    }
}

void svdsketch_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("svdsketch: requires at least 1 argument (A)", 0, 0, "svdsketch", "", "numkit:svdsketch:nargin");
    double tol = (args.size() >= 2 && !args[1].isEmpty()) ? args[1].toScalar() : 1e-6;
    auto [U, S, V] = svdsketch(args[0], tol, ctx.engine->resource());
    outs[0] = std::move(U);
    if (nargout >= 2 && outs.size() >= 2) outs[1] = std::move(S);
    if (nargout >= 3 && outs.size() >= 3) outs[2] = std::move(V);
}

void svdappend_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 4)
        throw Error("svdappend: requires 4 arguments (U, S, V, A_new)", 0, 0, "svdappend", "", "numkit:svdappend:nargin");
    auto [U, S, V] = svdappend(args[0], args[1], args[2], args[3], ctx.engine->resource());
    outs[0] = std::move(U);
    if (nargout >= 2 && outs.size() >= 2) outs[1] = std::move(S);
    if (nargout >= 3 && outs.size() >= 3) outs[2] = std::move(V);
}

} // namespace detail
} // namespace numkit::linalg
