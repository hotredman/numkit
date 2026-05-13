// libs/control/src/lti/lti.cpp
//
// LTI constructors. Each builds a numkit struct value tagged by the
// `kind` field. Discrete-time systems carry Ts > 0; continuous = 0.
// We don't enforce algebraic invariants here (e.g. real coefficients,
// proper rational form) — that's left to the user / converters which
// check on-demand.

#include <numkit/control/lti/lti.hpp>
#include <numkit/control/conversion/conversion.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include <algorithm>
#include <cmath>
#include <vector>

namespace numkit::control {

namespace {

Value rowVec(const Value &v, std::pmr::memory_resource *mr) {
    // Always represent coefficient lists as a row vector, copying
    // through the user memory resource so the struct fields don't
    // alias caller-side scratch memory.
    const size_t N = v.numel();
    if (v.type() == ValueType::COMPLEX) {
        Value r = Value::matrix(1, N, ValueType::COMPLEX, mr);
        const std::complex<double> *src = v.complexData();
        std::complex<double> *rd = r.complexDataMut();
        for (size_t i = 0; i < N; ++i) rd[i] = src[i];
        return r;
    }
    Value r = Value::matrix(1, N, ValueType::DOUBLE, mr);
    double *rd = r.doubleDataMut();
    for (size_t i = 0; i < N; ++i) rd[i] = v.elemAsDouble(i);
    return r;
}

Value tagStruct(const char *kind, double Ts, std::pmr::memory_resource *mr) {
    Value s = Value::structure(mr);
    s.field("kind") = Value::fromString(kind, mr);
    s.field("Ts") = Value::scalar(Ts, mr);
    return s;
}

} // anonymous

Value tf(const Value &num, const Value &den, double Ts, std::pmr::memory_resource *mr)
{
    if (den.numel() == 0)
        throw Error("tf: denominator must not be empty",
                    0, 0, "tf", "", "m:tf:den");
    Value s = tagStruct("tf", Ts, mr);
    s.field("num") = rowVec(num, mr);
    s.field("den") = rowVec(den, mr);
    return s;
}

Value zpk(const Value &z, const Value &p, double k, double Ts,
          std::pmr::memory_resource *mr)
{
    Value s = tagStruct("zpk", Ts, mr);
    s.field("z") = rowVec(z, mr);
    s.field("p") = rowVec(p, mr);
    s.field("k") = Value::scalar(k, mr);
    return s;
}

Value ss(const Value &A, const Value &B, const Value &C, const Value &D, double Ts, std::pmr::memory_resource *mr)
{
    Value s = tagStruct("ss", Ts, mr);
    // Copy state-space matrices through the user resource. We don't
    // dimensional-check here (zero-state, no inputs, etc. are valid).
    auto copyMat = [&](const Value &m) {
        const size_t r = m.dims().rows();
        const size_t c = m.dims().cols();
        if (m.type() == ValueType::COMPLEX) {
            Value out = Value::matrix(r, c, ValueType::COMPLEX, mr);
            const std::complex<double> *src = m.complexData();
            std::complex<double> *od = out.complexDataMut();
            for (size_t i = 0; i < r * c; ++i) od[i] = src[i];
            return out;
        }
        Value out = Value::matrix(r, c, ValueType::DOUBLE, mr);
        double *od = out.doubleDataMut();
        for (size_t i = 0; i < r * c; ++i) od[i] = m.elemAsDouble(i);
        return out;
    };
    s.field("A") = copyMat(A);
    s.field("B") = copyMat(B);
    s.field("C") = copyMat(C);
    s.field("D") = copyMat(D);
    return s;
}

Value filt(const Value &num, const Value &den, double Ts, std::pmr::memory_resource *mr)
{
    if (den.numel() == 0)
        throw Error("filt: denominator must not be empty",
                    0, 0, "filt", "", "m:filt:den");
    Value s = tagStruct("tf", Ts, mr);
    s.field("num") = rowVec(num, mr);
    s.field("den") = rowVec(den, mr);
    s.field("variable") = Value::fromString("z^-1", mr);
    return s;
}

Value frd(const Value &response, const Value &frequency, double Ts, std::pmr::memory_resource *mr)
{
    if (response.numel() != frequency.numel())
        throw Error("frd: response and frequency must have the same length",
                    0, 0, "frd", "", "m:frd:size");
    Value s = tagStruct("frd", Ts, mr);
    const size_t N = frequency.numel();
    // resp may be complex; copy as column vector.
    if (response.type() == ValueType::COMPLEX) {
        Value rv = Value::matrix(N, 1, ValueType::COMPLEX, mr);
        const std::complex<double> *src = response.complexData();
        std::complex<double> *rd = rv.complexDataMut();
        for (size_t i = 0; i < N; ++i) rd[i] = src[i];
        s.field("resp") = std::move(rv);
    } else {
        Value rv = Value::matrix(N, 1, ValueType::DOUBLE, mr);
        double *rd = rv.doubleDataMut();
        for (size_t i = 0; i < N; ++i) rd[i] = response.elemAsDouble(i);
        s.field("resp") = std::move(rv);
    }
    Value fv = Value::matrix(N, 1, ValueType::DOUBLE, mr);
    double *fd = fv.doubleDataMut();
    for (size_t i = 0; i < N; ++i) fd[i] = frequency.elemAsDouble(i);
    s.field("freq") = std::move(fv);
    return s;
}

// ──────────────────────────────────────────────────────────────────────
// Small LU helpers (used by ss2ss for the similarity transform).
// ──────────────────────────────────────────────────────────────────────

namespace {

// LU decomposition with partial pivoting on n×n column-major matrix.
// On return, A holds L (below diagonal, unit diagonal) and U (upper
// triangle, including diagonal). perm[i] = row that ends up in slot i.
bool lu_decomp(double *A, int *perm, size_t n) {
    for (size_t i = 0; i < n; ++i) perm[i] = static_cast<int>(i);
    for (size_t k = 0; k < n; ++k) {
        size_t piv = k;
        double best = std::fabs(A[k + k * n]);
        for (size_t i = k + 1; i < n; ++i) {
            const double v = std::fabs(A[i + k * n]);
            if (v > best) { best = v; piv = i; }
        }
        if (best == 0.0) return false;
        if (piv != k) {
            for (size_t j = 0; j < n; ++j) std::swap(A[k + j * n], A[piv + j * n]);
            std::swap(perm[k], perm[piv]);
        }
        const double pivVal = A[k + k * n];
        for (size_t i = k + 1; i < n; ++i) {
            const double m = A[i + k * n] / pivVal;
            A[i + k * n] = m;
            for (size_t j = k + 1; j < n; ++j)
                A[i + j * n] -= m * A[k + j * n];
        }
    }
    return true;
}

// Solve A·x = b given LU factorisation (n×n column-major A holds LU).
void lu_solve(const double *LU, const int *perm,
              double *x, const double *b, size_t n) {
    std::vector<double> z(n);
    for (size_t i = 0; i < n; ++i) z[i] = b[perm[i]];
    // L·y = P·b (unit-diag L)
    for (size_t i = 0; i < n; ++i) {
        double s = z[i];
        for (size_t j = 0; j < i; ++j) s -= LU[i + j * n] * z[j];
        z[i] = s;
    }
    // U·x = y
    for (size_t i = n; i-- > 0;) {
        double s = z[i];
        for (size_t j = i + 1; j < n; ++j) s -= LU[i + j * n] * x[j];
        x[i] = s / LU[i + i * n];
    }
}

// Compute n×n inverse via LU + column-by-column solve. Returns false if
// the matrix is singular.
bool inverse_n(const Value &T, std::vector<double> &out, size_t n) {
    if (T.dims().rows() != n || T.dims().cols() != n) return false;
    std::vector<double> LU(n * n);
    for (size_t i = 0; i < n * n; ++i) LU[i] = T.elemAsDouble(i);
    std::vector<int> perm(n);
    if (!lu_decomp(LU.data(), perm.data(), n)) return false;
    out.assign(n * n, 0.0);
    std::vector<double> ej(n), x(n);
    for (size_t k = 0; k < n; ++k) {
        std::fill(ej.begin(), ej.end(), 0.0);
        ej[k] = 1.0;
        lu_solve(LU.data(), perm.data(), x.data(), ej.data(), n);
        for (size_t i = 0; i < n; ++i) out[i + k * n] = x[i];
    }
    return true;
}

void matmul_n(const std::vector<double> &A, size_t aR, size_t aC,
              const std::vector<double> &B, size_t bR, size_t bC,
              std::vector<double> &C)
{
    (void)bR;
    C.assign(aR * bC, 0.0);
    for (size_t j = 0; j < bC; ++j)
        for (size_t k = 0; k < aC; ++k) {
            const double bjk = B[k + j * bR];
            for (size_t i = 0; i < aR; ++i)
                C[i + j * aR] += A[i + k * aR] * bjk;
        }
}

std::vector<double> readMat(const Value &v) {
    const size_t r = v.dims().rows();
    const size_t c = v.dims().cols();
    std::vector<double> out(r * c);
    for (size_t i = 0; i < r * c; ++i) out[i] = v.elemAsDouble(i);
    return out;
}

} // anonymous

// ──────────────────────────────────────────────────────────────────────
// Extractors: tfdata / zpkdata / ssdata / frdata
// ──────────────────────────────────────────────────────────────────────

namespace {

const std::string &kindOf(const Value &sys) {
    static const std::string empty;
    if (!sys.isStruct() || !sys.hasField("kind")) return empty;
    static thread_local std::string s;
    s = sys.field("kind").toString();
    return s;
}

// Pad num with leading zeros to match den length (MATLAB tfdata convention).
Value padToLen(const Value &row, size_t targetLen, std::pmr::memory_resource *mr) {
    const size_t N = row.numel();
    if (N >= targetLen) return rowVec(row, mr);
    Value out = Value::matrix(1, targetLen, ValueType::DOUBLE, mr);
    double *od = out.doubleDataMut();
    const size_t pad = targetLen - N;
    for (size_t i = 0; i < pad; ++i)     od[i] = 0.0;
    for (size_t i = 0; i < N;   ++i)     od[pad + i] = row.elemAsDouble(i);
    return out;
}

// Wrap a single value in a 1×1 cell.
Value wrapCell(const Value &v, std::pmr::memory_resource *mr) {
    Value c = Value::cell(1, 1, mr);
    c.cellAt(0) = v;
    return c;
}

// Column-vector copy (used for zpkdata's z/p outputs).
Value colVec(const Value &v, std::pmr::memory_resource *mr) {
    const size_t N = v.numel();
    if (v.type() == ValueType::COMPLEX) {
        Value r = Value::matrix(N, 1, ValueType::COMPLEX, mr);
        const std::complex<double> *src = v.complexData();
        std::complex<double> *rd = r.complexDataMut();
        for (size_t i = 0; i < N; ++i) rd[i] = src[i];
        return r;
    }
    Value r = Value::matrix(N, 1, ValueType::DOUBLE, mr);
    double *rd = r.doubleDataMut();
    for (size_t i = 0; i < N; ++i) rd[i] = v.elemAsDouble(i);
    return r;
}

} // anonymous

std::tuple<Value, Value>
tfdata(const Value &sys, bool asVector, std::pmr::memory_resource *mr)
{
    Value num, den;
    const std::string &k = kindOf(sys);
    if (k == "tf") {
        num = sys.field("num");
        den = sys.field("den");
    } else if (k == "zpk") {
        std::tie(num, den) = zp2tf(sys.field("z"), sys.field("p"), sys.field("k"), mr);
    } else if (k == "ss") {
        std::tie(num, den) = ss2tf(sys.field("A"), sys.field("B"),
                                    sys.field("C"), sys.field("D"), 1, mr);
    } else {
        throw Error("tfdata: input must be tf / zpk / ss",
                    0, 0, "tfdata", "", "m:tfdata:kind");
    }
    const size_t L = std::max(num.numel(), den.numel());
    Value np = padToLen(num, L, mr);
    Value dp = padToLen(den, L, mr);
    if (asVector) return {std::move(np), std::move(dp)};
    return {wrapCell(np, mr), wrapCell(dp, mr)};
}

std::tuple<Value, Value, Value>
zpkdata(const Value &sys, bool asVector, std::pmr::memory_resource *mr)
{
    Value z, p, k;
    const std::string &kind = kindOf(sys);
    if (kind == "zpk") {
        z = colVec(sys.field("z"), mr);
        p = colVec(sys.field("p"), mr);
        k = Value::scalar(sys.field("k").toScalar(), mr);
    } else if (kind == "tf") {
        auto r = tf2zp(sys.field("num"), sys.field("den"), mr);
        z = colVec(r.z, mr);
        p = colVec(r.p, mr);
        k = Value::scalar(r.k.toScalar(), mr);
    } else if (kind == "ss") {
        auto [n, d] = ss2tf(sys.field("A"), sys.field("B"),
                            sys.field("C"), sys.field("D"), 1, mr);
        auto r = tf2zp(n, d, mr);
        z = colVec(r.z, mr);
        p = colVec(r.p, mr);
        k = Value::scalar(r.k.toScalar(), mr);
    } else {
        throw Error("zpkdata: input must be tf / zpk / ss",
                    0, 0, "zpkdata", "", "m:zpkdata:kind");
    }
    if (asVector) return {std::move(z), std::move(p), std::move(k)};
    return {wrapCell(z, mr), wrapCell(p, mr), std::move(k)};
}

std::tuple<Value, Value, Value, Value>
ssdata(const Value &sys, std::pmr::memory_resource *mr)
{
    const std::string &kind = kindOf(sys);
    Value A, B, C, D;
    if (kind == "ss") {
        A = sys.field("A");
        B = sys.field("B");
        C = sys.field("C");
        D = sys.field("D");
    } else if (kind == "tf") {
        auto ss = tf2ss(sys.field("num"), sys.field("den"), mr);
        A = std::move(ss.A); B = std::move(ss.B);
        C = std::move(ss.C); D = std::move(ss.D);
    } else if (kind == "zpk") {
        auto [n, d] = zp2tf(sys.field("z"), sys.field("p"), sys.field("k"), mr);
        auto ss = tf2ss(n, d, mr);
        A = std::move(ss.A); B = std::move(ss.B);
        C = std::move(ss.C); D = std::move(ss.D);
    } else {
        throw Error("ssdata: input must be tf / zpk / ss",
                    0, 0, "ssdata", "", "m:ssdata:kind");
    }
    return {std::move(A), std::move(B), std::move(C), std::move(D)};
}

std::tuple<Value, Value>
frdata(const Value &sys, std::pmr::memory_resource *mr)
{
    if (kindOf(sys) != "frd")
        throw Error("frdata: input must be an frd model",
                    0, 0, "frdata", "", "m:frdata:kind");
    // Shallow copy through the user resource; resp / freq are already
    // stored as column vectors by frd().
    Value resp = sys.field("resp");
    Value freq = sys.field("freq");
    // Defensive copy so callers don't observe aliasing into the struct.
    auto copyV = [&](const Value &v) -> Value {
        const size_t N = v.numel();
        if (v.type() == ValueType::COMPLEX) {
            Value out = Value::matrix(N, 1, ValueType::COMPLEX, mr);
            const std::complex<double> *src = v.complexData();
            std::complex<double> *od = out.complexDataMut();
            for (size_t i = 0; i < N; ++i) od[i] = src[i];
            return out;
        }
        Value out = Value::matrix(N, 1, ValueType::DOUBLE, mr);
        double *od = out.doubleDataMut();
        for (size_t i = 0; i < N; ++i) od[i] = v.elemAsDouble(i);
        return out;
    };
    return {copyV(resp), copyV(freq)};
}

Value ss2ss(const Value &sys, const Value &T, std::pmr::memory_resource *mr)
{
    if (kindOf(sys) != "ss")
        throw Error("ss2ss: input must be an ss model",
                    0, 0, "ss2ss", "", "m:ss2ss:kind");
    const Value &Av = sys.field("A");
    const Value &Bv = sys.field("B");
    const Value &Cv = sys.field("C");
    const Value &Dv = sys.field("D");
    const size_t n = Av.dims().rows();
    if (Av.dims().cols() != n || T.dims().rows() != n || T.dims().cols() != n)
        throw Error("ss2ss: T must be n×n with n = order(sys)",
                    0, 0, "ss2ss", "", "m:ss2ss:size");
    const size_t m = Bv.dims().cols();
    const size_t p = Cv.dims().rows();

    std::vector<double> Tinv;
    if (!inverse_n(T, Tinv, n))
        throw Error("ss2ss: T is singular",
                    0, 0, "ss2ss", "", "m:ss2ss:singular");

    std::vector<double> Tm = readMat(T);
    std::vector<double> Am = readMat(Av);
    std::vector<double> Bm = readMat(Bv);
    std::vector<double> Cm = readMat(Cv);

    std::vector<double> M, Anew, Bnew, Cnew;
    matmul_n(Am, n, n, Tinv, n, n, M);
    matmul_n(Tm, n, n, M, n, n, Anew);
    matmul_n(Tm, n, n, Bm, n, m, Bnew);
    matmul_n(Cm, p, n, Tinv, n, n, Cnew);

    auto putMat = [&](const std::vector<double> &v, size_t rr, size_t cc) {
        Value out = Value::matrix(rr, cc, ValueType::DOUBLE, mr);
        double *od = out.doubleDataMut();
        for (size_t i = 0; i < rr * cc; ++i) od[i] = v[i];
        return out;
    };

    Value out = tagStruct("ss", sys.field("Ts").toScalar(), mr);
    out.field("A") = putMat(Anew, n, n);
    out.field("B") = putMat(Bnew, n, m);
    out.field("C") = putMat(Cnew, p, n);
    out.field("D") = Dv;
    return out;
}

namespace detail {

static double argTs(Span<const Value> args, size_t pos) {
    if (args.size() <= pos || args[pos].isEmpty()) return 0.0;
    return args[pos].toScalar();
}

void tf_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
            CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("tf: requires (num, den [, Ts])",
                    0, 0, "tf", "", "m:tf:nargin");
    outs[0] = tf(args[0], args[1], argTs(args, 2), ctx.engine->resource());
}

void zpk_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
             CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("zpk: requires (z, p, k [, Ts])",
                    0, 0, "zpk", "", "m:zpk:nargin");
    outs[0] = zpk(args[0], args[1], args[2].toScalar(), argTs(args, 3),
                  ctx.engine->resource());
}

void ss_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
            CallContext &ctx)
{
    if (args.size() < 4)
        throw Error("ss: requires (A, B, C, D [, Ts])",
                    0, 0, "ss", "", "m:ss:nargin");
    outs[0] = ss(args[0], args[1], args[2], args[3], argTs(args, 4), ctx.engine->resource());
}

void filt_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
              CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("filt: requires (num, den [, Ts])",
                    0, 0, "filt", "", "m:filt:nargin");
    // MATLAB default Ts for filt is -1 (unspecified discrete).
    const double Ts = (args.size() >= 3 && !args[2].isEmpty())
                      ? args[2].toScalar() : -1.0;
    outs[0] = filt(args[0], args[1], Ts, ctx.engine->resource());
}

void frd_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
             CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("frd: requires (response, frequency [, Ts])",
                    0, 0, "frd", "", "m:frd:nargin");
    outs[0] = frd(args[0], args[1], argTs(args, 2), ctx.engine->resource());
}

static bool wantVector(Span<const Value> args, size_t pos) {
    if (args.size() <= pos) return false;
    if (!args[pos].isChar() && !args[pos].isString()) return false;
    std::string s = args[pos].toString();
    for (auto &c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s == "v" || s == "vector";
}

void tfdata_reg(Span<const Value> args, size_t nargout, Span<Value> outs,
                CallContext &ctx)
{
    if (args.empty())
        throw Error("tfdata: requires (sys [, 'v'])",
                    0, 0, "tfdata", "", "m:tfdata:nargin");
    auto [num, den] = tfdata(args[0], wantVector(args, 1), ctx.engine->resource());
    outs[0] = std::move(num);
    if (nargout > 1) outs[1] = std::move(den);
}

void zpkdata_reg(Span<const Value> args, size_t nargout, Span<Value> outs,
                 CallContext &ctx)
{
    if (args.empty())
        throw Error("zpkdata: requires (sys [, 'v'])",
                    0, 0, "zpkdata", "", "m:zpkdata:nargin");
    auto [z, p, k] = zpkdata(args[0], wantVector(args, 1), ctx.engine->resource());
    outs[0] = std::move(z);
    if (nargout > 1) outs[1] = std::move(p);
    if (nargout > 2) outs[2] = std::move(k);
}

void ssdata_reg(Span<const Value> args, size_t nargout, Span<Value> outs,
                CallContext &ctx)
{
    if (args.empty())
        throw Error("ssdata: requires (sys)",
                    0, 0, "ssdata", "", "m:ssdata:nargin");
    auto [A, B, C, D] = ssdata(args[0], ctx.engine->resource());
    outs[0] = std::move(A);
    if (nargout > 1) outs[1] = std::move(B);
    if (nargout > 2) outs[2] = std::move(C);
    if (nargout > 3) outs[3] = std::move(D);
}

void ss2ss_reg(Span<const Value> args, size_t /*nargout*/,
               Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("ss2ss: requires (sys, T)",
                    0, 0, "ss2ss", "", "m:ss2ss:nargin");
    outs[0] = ss2ss(args[0], args[1], ctx.engine->resource());
}

void frdata_reg(Span<const Value> args, size_t nargout, Span<Value> outs,
                CallContext &ctx)
{
    if (args.empty())
        throw Error("frdata: requires (sys [, 'v'])",
                    0, 0, "frdata", "", "m:frdata:nargin");
    // 'v' flag accepted for MATLAB compatibility; frdata always returns
    // column vectors regardless (we don't model SISO 1×1×N tensors).
    (void)wantVector(args, 1);
    auto [resp, freq] = frdata(args[0], ctx.engine->resource());
    outs[0] = std::move(resp);
    if (nargout > 1) outs[1] = std::move(freq);
}

} // namespace detail

} // namespace numkit::control
