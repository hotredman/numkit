// libs/stats/src/spline/spline.cpp

#include <numkit/stats/spline/spline.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include <vector>

namespace numkit::stats {

Value aveknt(std::pmr::memory_resource *mr, const Value &t, int k)
{
    const size_t N = t.numel();
    if (k < 2 || static_cast<size_t>(k) > N)
        throw Error("aveknt: order k must satisfy 2 ≤ k ≤ length(t)",
                    0, 0, "aveknt", "", "m:aveknt:k");
    const size_t M = N - static_cast<size_t>(k);
    Value out = Value::matrix(1, M, ValueType::DOUBLE, mr);
    if (M == 0) return out;
    double *od = out.doubleDataMut();
    const double inv = 1.0 / static_cast<double>(k - 1);
    for (size_t i = 0; i < M; ++i) {
        double sum = 0.0;
        for (size_t j = 1; j < static_cast<size_t>(k); ++j)
            sum += t.elemAsDouble(i + j);
        od[i] = sum * inv;
    }
    return out;
}

Value augknt(std::pmr::memory_resource *mr, const Value &knots, int k)
{
    const size_t N = knots.numel();
    if (k < 1 || N == 0)
        throw Error("augknt: k must be ≥ 1 and knots non-empty",
                    0, 0, "augknt", "", "m:augknt:k");
    // The first knot becomes k copies; the last knot becomes k copies;
    // any interior knots stay single. So size = N + 2*(k-1).
    const size_t outN = N + 2 * (static_cast<size_t>(k) - 1);
    Value out = Value::matrix(1, outN, ValueType::DOUBLE, mr);
    double *od = out.doubleDataMut();
    const double first = knots.elemAsDouble(0);
    const double last  = knots.elemAsDouble(N - 1);
    size_t idx = 0;
    for (int i = 0; i < k; ++i) od[idx++] = first;
    for (size_t i = 1; i + 1 < N; ++i) od[idx++] = knots.elemAsDouble(i);
    for (int i = 0; i < k; ++i) od[idx++] = last;
    return out;
}

Value brk2knt(std::pmr::memory_resource *mr, const Value &breaks, const Value &mults)
{
    const size_t N = breaks.numel();
    if (mults.numel() != N)
        throw Error("brk2knt: breaks and mults must have same length",
                    0, 0, "brk2knt", "", "m:brk2knt:size");
    size_t total = 0;
    for (size_t i = 0; i < N; ++i) {
        const double mi = mults.elemAsDouble(i);
        if (!(mi >= 0.0))
            throw Error("brk2knt: multiplicities must be non-negative",
                        0, 0, "brk2knt", "", "m:brk2knt:m");
        total += static_cast<size_t>(mi);
    }
    Value out = Value::matrix(1, total, ValueType::DOUBLE, mr);
    if (total == 0) return out;
    double *od = out.doubleDataMut();
    size_t idx = 0;
    for (size_t i = 0; i < N; ++i) {
        const size_t mi = static_cast<size_t>(mults.elemAsDouble(i));
        const double bi = breaks.elemAsDouble(i);
        for (size_t j = 0; j < mi; ++j) od[idx++] = bi;
    }
    return out;
}

std::tuple<Value, Value>
knt2brk(std::pmr::memory_resource *mr, const Value &knots)
{
    const size_t N = knots.numel();
    std::vector<double> b;
    std::vector<size_t> m;
    if (N == 0) {
        Value bv = Value::matrix(0, 0, ValueType::DOUBLE, mr);
        Value mv = Value::matrix(0, 0, ValueType::DOUBLE, mr);
        return {std::move(bv), std::move(mv)};
    }
    double cur = knots.elemAsDouble(0);
    size_t cnt = 1;
    for (size_t i = 1; i < N; ++i) {
        const double ki = knots.elemAsDouble(i);
        if (ki == cur) {
            ++cnt;
        } else {
            b.push_back(cur);
            m.push_back(cnt);
            cur = ki;
            cnt = 1;
        }
    }
    b.push_back(cur);
    m.push_back(cnt);

    const size_t K = b.size();
    Value bv = Value::matrix(1, K, ValueType::DOUBLE, mr);
    Value mv = Value::matrix(1, K, ValueType::DOUBLE, mr);
    double *bd = bv.doubleDataMut();
    double *md = mv.doubleDataMut();
    for (size_t i = 0; i < K; ++i) { bd[i] = b[i]; md[i] = double(m[i]); }
    return {std::move(bv), std::move(mv)};
}

Value ppmak(std::pmr::memory_resource *mr, const Value &breaks,
            const Value &coefs, int d)
{
    const size_t L1 = breaks.numel();
    if (L1 < 2)
        throw Error("ppmak: breaks must have at least 2 entries",
                    0, 0, "ppmak", "", "m:ppmak:breaks");
    const size_t L  = L1 - 1;
    const size_t cR = coefs.dims().rows();
    const size_t cC = coefs.dims().cols();
    if (d < 1) d = 1;
    if (cR != static_cast<size_t>(d) * L)
        throw Error("ppmak: coefs must be d·L × K (rows = dim·pieces)",
                    0, 0, "ppmak", "", "m:ppmak:coefs");

    Value bv = Value::matrix(1, L1, ValueType::DOUBLE, mr);
    {
        double *bd = bv.doubleDataMut();
        for (size_t i = 0; i < L1; ++i) bd[i] = breaks.elemAsDouble(i);
    }
    Value cv = Value::matrix(cR, cC, ValueType::DOUBLE, mr);
    {
        double *cd = cv.doubleDataMut();
        for (size_t i = 0; i < cR * cC; ++i) cd[i] = coefs.elemAsDouble(i);
    }

    Value s = Value::structure(mr);
    s.field("form")   = Value::fromString("pp", mr);
    s.field("breaks") = std::move(bv);
    s.field("coefs")  = std::move(cv);
    s.field("pieces") = Value::scalar(double(L),  mr);
    s.field("order")  = Value::scalar(double(cC), mr);
    s.field("dim")    = Value::scalar(double(d),  mr);
    return s;
}

Value fnval(std::pmr::memory_resource *mr, const Value &pp, const Value &xv)
{
    if (!pp.hasField("form") || pp.field("form").toString() != "pp")
        throw Error("fnval: only pp form supported in this release",
                    0, 0, "fnval", "", "m:fnval:form");
    const Value &breaks = pp.field("breaks");
    const Value &coefs  = pp.field("coefs");
    const size_t L  = static_cast<size_t>(pp.field("pieces").toScalar());
    const size_t K  = static_cast<size_t>(pp.field("order").toScalar());
    const size_t d  = static_cast<size_t>(pp.field("dim").toScalar());
    const size_t cR = coefs.dims().rows();
    if (cR != d * L || coefs.dims().cols() != K || breaks.numel() != L + 1)
        throw Error("fnval: pp struct fields are inconsistent",
                    0, 0, "fnval", "", "m:fnval:struct");

    const size_t Nx = xv.numel();
    Value out;
    if (d == 1) {
        const auto &dx = xv.dims();
        out = Value::matrix(dx.rows(), dx.cols(), ValueType::DOUBLE, mr);
    } else {
        out = Value::matrix(d, Nx, ValueType::DOUBLE, mr);
    }
    if (Nx == 0) return out;
    double *od = out.doubleDataMut();

    auto findPiece = [&](double x) -> size_t {
        if (x <= breaks.elemAsDouble(0)) return 0;
        if (x >= breaks.elemAsDouble(L)) return L - 1;
        size_t lo = 0, hi = L - 1;
        while (lo < hi) {
            const size_t m = (lo + hi + 1) / 2;
            if (breaks.elemAsDouble(m) <= x) lo = m;
            else                              hi = m - 1;
        }
        return lo;
    };

    for (size_t i = 0; i < Nx; ++i) {
        const double x = xv.elemAsDouble(i);
        const size_t j = findPiece(x);
        const double dx = x - breaks.elemAsDouble(j);
        for (size_t r = 0; r < d; ++r) {
            const size_t row = r + j * d;
            double y = coefs.elemAsDouble(row + 0 * cR);
            for (size_t m = 1; m < K; ++m)
                y = y * dx + coefs.elemAsDouble(row + m * cR);
            if (d == 1) od[i] = y;
            else        od[r + i * d] = y;
        }
    }
    return out;
}

namespace {

struct PPView {
    Value breaks;
    Value coefs;
    size_t L, K, d;
};

PPView readPP(const Value &pp)
{
    if (!pp.hasField("form") || pp.field("form").toString() != "pp")
        throw Error("fnder/fnint: only pp form supported",
                    0, 0, "fn", "", "m:fn:form");
    PPView v;
    v.breaks = pp.field("breaks");
    v.coefs  = pp.field("coefs");
    v.L = static_cast<size_t>(pp.field("pieces").toScalar());
    v.K = static_cast<size_t>(pp.field("order").toScalar());
    v.d = static_cast<size_t>(pp.field("dim").toScalar());
    return v;
}

Value buildPP(std::pmr::memory_resource *mr, const Value &breaks,
              const std::vector<double> &coefsCM, size_t cR, size_t cC,
              size_t L, size_t d)
{
    Value bv = Value::matrix(1, breaks.numel(), ValueType::DOUBLE, mr);
    {
        double *bd = bv.doubleDataMut();
        for (size_t i = 0; i < breaks.numel(); ++i) bd[i] = breaks.elemAsDouble(i);
    }
    Value cv = Value::matrix(cR, cC, ValueType::DOUBLE, mr);
    {
        double *cd = cv.doubleDataMut();
        for (size_t i = 0; i < cR * cC; ++i) cd[i] = coefsCM[i];
    }
    Value s = Value::structure(mr);
    s.field("form")   = Value::fromString("pp", mr);
    s.field("breaks") = std::move(bv);
    s.field("coefs")  = std::move(cv);
    s.field("pieces") = Value::scalar(double(L), mr);
    s.field("order")  = Value::scalar(double(cC), mr);
    s.field("dim")    = Value::scalar(double(d), mr);
    return s;
}

} // anonymous

Value fnder(std::pmr::memory_resource *mr, const Value &pp, int order)
{
    if (order < 0)
        throw Error("fnder: order must be >= 0",
                    0, 0, "fnder", "", "m:fnder:order");
    PPView v = readPP(pp);
    if (order == 0) {
        std::vector<double> coefs(v.coefs.numel());
        for (size_t i = 0; i < coefs.size(); ++i) coefs[i] = v.coefs.elemAsDouble(i);
        return buildPP(mr, v.breaks, coefs, v.coefs.dims().rows(),
                       v.coefs.dims().cols(), v.L, v.d);
    }
    if (static_cast<size_t>(order) >= v.K) {
        const size_t cR = v.d * v.L;
        std::vector<double> zero(cR, 0.0);
        return buildPP(mr, v.breaks, zero, cR, 1, v.L, v.d);
    }
    const size_t cR = v.d * v.L;
    const size_t newK = v.K - static_cast<size_t>(order);
    std::vector<double> nc(cR * newK, 0.0);

    for (size_t r = 0; r < cR; ++r) {
        std::vector<double> cur(v.K);
        for (size_t m = 0; m < v.K; ++m) cur[m] = v.coefs.elemAsDouble(r + m * cR);
        size_t curK = v.K;
        for (int it = 0; it < order; ++it) {
            std::vector<double> nxt(curK - 1, 0.0);
            for (size_t m = 0; m + 1 < curK; ++m)
                nxt[m] = cur[m] * double(curK - 1 - m);
            cur = std::move(nxt);
            --curK;
        }
        for (size_t m = 0; m < newK; ++m) nc[r + m * cR] = cur[m];
    }
    return buildPP(mr, v.breaks, nc, cR, newK, v.L, v.d);
}

Value fnint(std::pmr::memory_resource *mr, const Value &pp)
{
    PPView v = readPP(pp);
    const size_t cR = v.d * v.L;
    const size_t newK = v.K + 1;
    std::vector<double> nc(cR * newK, 0.0);
    for (size_t r = 0; r < v.d; ++r) {
        double prev_const = 0.0;
        for (size_t j = 0; j < v.L; ++j) {
            const size_t row = r + j * v.d;
            for (size_t m = 0; m < v.K; ++m) {
                nc[row + m * cR] = v.coefs.elemAsDouble(row + m * cR)
                                   / double(v.K - m);
            }
            nc[row + v.K * cR] = prev_const;
            const double h = v.breaks.elemAsDouble(j + 1) - v.breaks.elemAsDouble(j);
            // Q(b_{j+1}) = prev_const + Σ_{m=0..K-1} new_c[m]·h^(K-m)
            double y = nc[row + 0 * cR];
            for (size_t m = 1; m < v.K; ++m)
                y = y * h + nc[row + m * cR];
            y = y * h;
            prev_const += y;
        }
    }
    return buildPP(mr, v.breaks, nc, cR, newK, v.L, v.d);
}

Value csapi(std::pmr::memory_resource *mr, const Value &x, const Value &y)
{
    const size_t N = x.numel();
    if (y.numel() != N || N < 2)
        throw Error("csapi: x and y must be vectors of the same length ≥ 2",
                    0, 0, "csapi", "", "m:csapi:size");
    std::vector<double> xv(N), yv(N);
    for (size_t i = 0; i < N; ++i) {
        xv[i] = x.elemAsDouble(i);
        yv[i] = y.elemAsDouble(i);
    }

    if (N == 2) {
        Value bv = Value::matrix(1, 2, ValueType::DOUBLE, mr);
        bv.doubleDataMut()[0] = xv[0];
        bv.doubleDataMut()[1] = xv[1];
        std::vector<double> coefs(4, 0.0);
        coefs[0] = 0.0;
        coefs[1] = 0.0;
        coefs[2] = (yv[1] - yv[0]) / (xv[1] - xv[0]);
        coefs[3] = yv[0];
        return buildPP(mr, bv, coefs, 1, 4, 1, 1);
    }

    const size_t L = N - 1;
    std::vector<double> h(L);
    for (size_t i = 0; i < L; ++i) h[i] = xv[i + 1] - xv[i];

    std::vector<double> A(N * N, 0.0), rhs(N, 0.0);
    auto idx = [&](size_t r, size_t c) -> double & { return A[r + c * N]; };

    if (L >= 2) {
        idx(0, 0) = h[1];
        idx(0, 1) = -(h[0] + h[1]);
        idx(0, 2) = h[0];
        rhs[0] = 0.0;
    }
    for (size_t i = 1; i + 1 < N; ++i) {
        idx(i, i - 1) = h[i - 1];
        idx(i, i)     = 2.0 * (h[i - 1] + h[i]);
        idx(i, i + 1) = h[i];
        rhs[i] = 6.0 * ((yv[i + 1] - yv[i]) / h[i] - (yv[i] - yv[i - 1]) / h[i - 1]);
    }
    if (L >= 2) {
        const size_t r = N - 1;
        idx(r, r - 2) = h[L - 1];
        idx(r, r - 1) = -(h[L - 2] + h[L - 1]);
        idx(r, r)     = h[L - 2];
        rhs[r] = 0.0;
    }

    std::vector<int> perm(N);
    for (size_t i = 0; i < N; ++i) perm[i] = static_cast<int>(i);
    for (size_t k = 0; k < N; ++k) {
        size_t piv = k;
        double best = std::fabs(A[k + k * N]);
        for (size_t i = k + 1; i < N; ++i) {
            const double vv = std::fabs(A[i + k * N]);
            if (vv > best) { best = vv; piv = i; }
        }
        if (best == 0.0)
            throw Error("csapi: singular system (duplicate breaks?)",
                        0, 0, "csapi", "", "m:csapi:singular");
        if (piv != k) {
            for (size_t j = 0; j < N; ++j)
                std::swap(A[k + j * N], A[piv + j * N]);
            std::swap(perm[k], perm[piv]);
        }
        const double pv = A[k + k * N];
        for (size_t i = k + 1; i < N; ++i) {
            const double m = A[i + k * N] / pv;
            A[i + k * N] = m;
            for (size_t j = k + 1; j < N; ++j)
                A[i + j * N] -= m * A[k + j * N];
        }
    }
    std::vector<double> z(N), Mvec(N);
    for (size_t i = 0; i < N; ++i) z[i] = rhs[perm[i]];
    for (size_t i = 0; i < N; ++i) {
        double s = z[i];
        for (size_t j = 0; j < i; ++j) s -= A[i + j * N] * z[j];
        z[i] = s;
    }
    for (size_t i = N; i-- > 0;) {
        double s = z[i];
        for (size_t j = i + 1; j < N; ++j) s -= A[i + j * N] * Mvec[j];
        Mvec[i] = s / A[i + i * N];
    }

    Value bv = Value::matrix(1, N, ValueType::DOUBLE, mr);
    {
        double *bd = bv.doubleDataMut();
        for (size_t i = 0; i < N; ++i) bd[i] = xv[i];
    }
    std::vector<double> coefs(L * 4, 0.0);
    for (size_t i = 0; i < L; ++i) {
        const double Mi  = Mvec[i];
        const double Mn  = Mvec[i + 1];
        const double hi  = h[i];
        coefs[i + 0 * L] = (Mn - Mi) / (6.0 * hi);
        coefs[i + 1 * L] = Mi / 2.0;
        coefs[i + 2 * L] = (yv[i + 1] - yv[i]) / hi - hi * (2.0 * Mi + Mn) / 6.0;
        coefs[i + 3 * L] = yv[i];
    }
    return buildPP(mr, bv, coefs, L, 4, L, 1);
}

// ════════════════════════════════════════════════════════════════════
// Engine adapters
// ════════════════════════════════════════════════════════════════════

namespace detail {

void aveknt_reg(Span<const Value> args, size_t /*nargout*/,
                Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("aveknt: requires (t, k)",
                    0, 0, "aveknt", "", "m:aveknt:nargin");
    outs[0] = aveknt(ctx.engine->resource(), args[0],
                     static_cast<int>(args[1].toScalar()));
}

void augknt_reg(Span<const Value> args, size_t /*nargout*/,
                Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("augknt: requires (knots, k)",
                    0, 0, "augknt", "", "m:augknt:nargin");
    outs[0] = augknt(ctx.engine->resource(), args[0],
                     static_cast<int>(args[1].toScalar()));
}

void brk2knt_reg(Span<const Value> args, size_t /*nargout*/,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("brk2knt: requires (breaks, mults)",
                    0, 0, "brk2knt", "", "m:brk2knt:nargin");
    outs[0] = brk2knt(ctx.engine->resource(), args[0], args[1]);
}

void ppmak_reg(Span<const Value> args, size_t /*nargout*/,
               Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("ppmak: requires (breaks, coefs[, d])",
                    0, 0, "ppmak", "", "m:ppmak:nargin");
    int d = 1;
    if (args.size() >= 3 && !args[2].isEmpty())
        d = static_cast<int>(args[2].toScalar());
    outs[0] = ppmak(ctx.engine->resource(), args[0], args[1], d);
}

void fnval_reg(Span<const Value> args, size_t /*nargout*/,
               Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("fnval: requires (pp, x)",
                    0, 0, "fnval", "", "m:fnval:nargin");
    outs[0] = fnval(ctx.engine->resource(), args[0], args[1]);
}

void fnder_reg(Span<const Value> args, size_t /*nargout*/,
               Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("fnder: requires (pp[, order])",
                    0, 0, "fnder", "", "m:fnder:nargin");
    int order = 1;
    if (args.size() >= 2 && !args[1].isEmpty())
        order = static_cast<int>(args[1].toScalar());
    outs[0] = fnder(ctx.engine->resource(), args[0], order);
}

void fnint_reg(Span<const Value> args, size_t /*nargout*/,
               Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("fnint: requires (pp)",
                    0, 0, "fnint", "", "m:fnint:nargin");
    outs[0] = fnint(ctx.engine->resource(), args[0]);
}

void csapi_reg(Span<const Value> args, size_t /*nargout*/,
               Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("csapi: requires (x, y)",
                    0, 0, "csapi", "", "m:csapi:nargin");
    outs[0] = csapi(ctx.engine->resource(), args[0], args[1]);
}

void knt2brk_reg(Span<const Value> args, size_t nargout,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("knt2brk: requires (knots)",
                    0, 0, "knt2brk", "", "m:knt2brk:nargin");
    auto [b, m] = knt2brk(ctx.engine->resource(), args[0]);
    outs[0] = std::move(b);
    if (nargout > 1) outs[1] = std::move(m);
}

} // namespace detail
} // namespace numkit::stats
