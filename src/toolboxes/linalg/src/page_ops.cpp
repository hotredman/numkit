// toolboxes/linalg/src/page_ops.cpp
//
// Page-wise linalg ops on 3-D arrays. All wrappers iterate over
// pages and dispatch to the 2-D variant.

#include <numkit/linalg/page_ops.hpp>

#include <numkit/linalg/decompositions.hpp>   // chol/lu/qr/svd
#include <numkit/linalg/eig.hpp>              // eig_symmetric / eig_general_VD / eig_values / eig_general_values
#include <numkit/linalg/norms.hpp>            // norm_value / norm_inf / norm_fro
#include <numkit/linalg/properties.hpp>       // inv
#include <numkit/linalg/pseudo_subspace.hpp>  // pinv
#include <numkit/linalg/solvers.hpp>          // linsolve / lsqminnorm
#include <numkit/ops/la_solve.hpp>   // numkit::ops::la_solve

// Compute-only TU: Value substrate + Error, no engine. The page* builtins
// (CallContext wrappers) live in page_ops_reg.cpp.
#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/span.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>

namespace numkit::linalg {

namespace {

void fillIdentity(double *buf, std::size_t n)
{
    std::fill(buf, buf + n * n, 0.0);
    for (std::size_t i = 0; i < n; ++i)
        buf[i + i * n] = 1.0;
}

// Make a stand-alone 2-D Value from page `p` of a 3-D source. We
// copy the m×n block into a fresh allocation so the 2-D linalg fn
// can do whatever scratch it needs without seeing 3-D dims.
Value pageView(const Value &A, std::size_t p, std::size_t m, std::size_t n,
               std::pmr::memory_resource *mr)
{
    auto out = Value::matrix(m, n, ValueType::DOUBLE, mr);
    const double *src = A.doubleData() + p * m * n;
    std::copy(src, src + m * n, out.doubleDataMut());
    return out;
}

// Return (m, n, pages) for a 2-D or 3-D Value. Throws on rank ≥ 4.
std::tuple<std::size_t, std::size_t, std::size_t>
pageShape(const Value &A, const char *who)
{
    const int nd = A.dims().ndim();
    if (nd < 2 || nd > 3)
        throw Error(std::string(who) + ": input must be 2-D or 3-D",
                    0, 0, who, "", std::string("numkit:") + who + ":badDim");
    const std::size_t m = static_cast<std::size_t>(A.dims().dim(0));
    const std::size_t n = static_cast<std::size_t>(A.dims().dim(1));
    const std::size_t p = (nd == 2) ? 1 : static_cast<std::size_t>(A.dims().dim(2));
    return {m, n, p};
}

// Make a 3-D output (or 2-D if pages == 1, matching input rank).
Value makePageOutput(std::size_t m, std::size_t n, std::size_t pages, bool input_was_3d,
                     std::pmr::memory_resource *mr)
{
    if (!input_was_3d)
        return Value::matrix(m, n, ValueType::DOUBLE, mr);
    return Value::matrix3d(m, n, pages, ValueType::DOUBLE, mr);
}

} // anonymous namespace

// ────────────────────────────────────────────────────────────────────────
// pageinv
// ────────────────────────────────────────────────────────────────────────

Value pageinv(const Value &A, std::pmr::memory_resource *mr)
{
    auto [m, n, pages] = pageShape(A, "pageinv");
    if (m != n)
        throw Error("pageinv: each page must be square",
                    0, 0, "pageinv", "", "numkit:pageinv:notSquare");
    auto out = makePageOutput(m, n, pages, A.dims().ndim() == 3, mr);
    if (m == 0) return out;

    ScratchArena scratch(mr);
    ScratchVec<double> A_buf(m * n, &scratch);
    ScratchVec<double> I_buf(n * n, &scratch);
    const double *src = A.doubleData();
    double *dst = out.doubleDataMut();
    const std::size_t stride = m * n;

    for (std::size_t p = 0; p < pages; ++p) {
        std::copy(src + p * stride, src + (p + 1) * stride, A_buf.begin());
        fillIdentity(I_buf.data(), n);
        if (!numkit::ops::la_solve(A_buf.data(), m, n,
                                                I_buf.data(), n,
                                                dst + p * stride, &scratch))
            throw Error("pageinv: page is singular",
                        0, 0, "pageinv", "", "numkit:pageinv:singular");
    }
    return out;
}

// ────────────────────────────────────────────────────────────────────────
// pageeig
// ────────────────────────────────────────────────────────────────────────

Value pageeig_values(const Value &A, std::pmr::memory_resource *mr)
{
    auto [m, n, pages] = pageShape(A, "pageeig");
    if (m != n)
        throw Error("pageeig: each page must be square",
                    0, 0, "pageeig", "", "numkit:pageeig:notSquare");

    if (A.dims().ndim() == 2) {
        // Reuse 2-D dispatch — symmetric → real eigvals; non-sym → general.
        try { return eig_values(A, mr); }
        catch (const Error &) { return eig_general_values(A, mr); }
    }

    auto out = Value::matrix3d(n, 1, pages, ValueType::DOUBLE, mr);
    double *od = out.doubleDataMut();
    for (std::size_t p = 0; p < pages; ++p) {
        auto pg = pageView(A, p, m, n, mr);
        Value evs;
        try { evs = eig_values(pg, mr); }
        catch (const Error &) { evs = eig_general_values(pg, mr); }
        if (evs.isComplex())
            throw Error("pageeig: page has complex eigenvalues (deferred)",
                        0, 0, "pageeig", "", "numkit:pageeig:complex");
        std::copy(evs.doubleData(), evs.doubleData() + n, od + p * n);
    }
    return out;
}

std::tuple<Value, Value>
pageeig_VD(const Value &A, std::pmr::memory_resource *mr)
{
    auto [m, n, pages] = pageShape(A, "pageeig");
    if (m != n)
        throw Error("pageeig: each page must be square",
                    0, 0, "pageeig", "", "numkit:pageeig:notSquare");

    if (A.dims().ndim() == 2) {
        try { return eig_symmetric(A, mr); }
        catch (const Error &) { return eig_general_VD(A, mr); }
    }

    auto V = Value::matrix3d(n, n, pages, ValueType::DOUBLE, mr);
    auto D = Value::matrix3d(n, n, pages, ValueType::DOUBLE, mr);
    double *Vd = V.doubleDataMut();
    double *Dd = D.doubleDataMut();
    const std::size_t stride = n * n;
    for (std::size_t p = 0; p < pages; ++p) {
        auto pg = pageView(A, p, m, n, mr);
        Value Vp, Dp;
        try {
            auto [v, d] = eig_symmetric(pg, mr);
            Vp = std::move(v); Dp = std::move(d);
        } catch (const Error &) {
            auto [v, d] = eig_general_VD(pg, mr);
            Vp = std::move(v); Dp = std::move(d);
        }
        std::copy(Vp.doubleData(), Vp.doubleData() + stride, Vd + p * stride);
        std::copy(Dp.doubleData(), Dp.doubleData() + stride, Dd + p * stride);
    }
    return std::make_tuple(std::move(V), std::move(D));
}

// ────────────────────────────────────────────────────────────────────────
// pagesvd
// ────────────────────────────────────────────────────────────────────────

Value pagesvd_values(const Value &A, std::pmr::memory_resource *mr)
{
    auto [m, n, pages] = pageShape(A, "pagesvd");
    const std::size_t k = std::min(m, n);

    if (A.dims().ndim() == 2)
        return svd_values(A, mr);

    auto out = Value::matrix3d(k, 1, pages, ValueType::DOUBLE, mr);
    double *od = out.doubleDataMut();
    for (std::size_t p = 0; p < pages; ++p) {
        auto pg = pageView(A, p, m, n, mr);
        auto sv = svd_values(pg, mr);
        std::copy(sv.doubleData(), sv.doubleData() + k, od + p * k);
    }
    return out;
}

std::tuple<Value, Value, Value>
pagesvd_decompose(const Value &A, std::pmr::memory_resource *mr)
{
    auto [m, n, pages] = pageShape(A, "pagesvd");

    if (A.dims().ndim() == 2)
        return svd_decompose(A, mr);

    auto U = Value::matrix3d(m, m, pages, ValueType::DOUBLE, mr);
    auto S = Value::matrix3d(m, n, pages, ValueType::DOUBLE, mr);
    auto V = Value::matrix3d(n, n, pages, ValueType::DOUBLE, mr);
    double *Ud = U.doubleDataMut();
    double *Sd = S.doubleDataMut();
    double *Vd = V.doubleDataMut();
    const std::size_t uStride = m * m, sStride = m * n, vStride = n * n;
    for (std::size_t p = 0; p < pages; ++p) {
        auto pg = pageView(A, p, m, n, mr);
        auto [u, s, v] = svd_decompose(pg, mr);
        std::copy(u.doubleData(), u.doubleData() + uStride, Ud + p * uStride);
        std::copy(s.doubleData(), s.doubleData() + sStride, Sd + p * sStride);
        std::copy(v.doubleData(), v.doubleData() + vStride, Vd + p * vStride);
    }
    return std::make_tuple(std::move(U), std::move(S), std::move(V));
}

// ────────────────────────────────────────────────────────────────────────
// pagepinv
// ────────────────────────────────────────────────────────────────────────

Value pagepinv(const Value &A, double tol, std::pmr::memory_resource *mr)
{
    auto [m, n, pages] = pageShape(A, "pagepinv");

    if (A.dims().ndim() == 2)
        return pinv(A, tol, mr);

    // Output is (n × m × pages) — pinv swaps shape.
    auto out = Value::matrix3d(n, m, pages, ValueType::DOUBLE, mr);
    double *od = out.doubleDataMut();
    const std::size_t inStride = m * n, outStride = n * m;
    for (std::size_t p = 0; p < pages; ++p) {
        auto pg = pageView(A, p, m, n, mr);
        auto pi = pinv(pg, tol, mr);
        std::copy(pi.doubleData(), pi.doubleData() + outStride, od + p * outStride);
        (void) inStride;
    }
    return out;
}

// ────────────────────────────────────────────────────────────────────────
// pagenorm
// ────────────────────────────────────────────────────────────────────────

namespace {

Value scalarNormDispatch(const Value &page, double p, bool fro_flag, bool inf_flag,
                         std::pmr::memory_resource *mr)
{
    if (fro_flag) return norm_fro(page, mr);
    if (inf_flag || std::isinf(p)) return norm_inf(page, mr);
    return norm_value(page, p, mr);
}

} // anonymous namespace

Value pagenorm(const Value &A, double p, std::pmr::memory_resource *mr)
{
    auto [m, n, pages] = pageShape(A, "pagenorm");

    if (A.dims().ndim() == 2)
        return scalarNormDispatch(A, p, /*fro=*/false, /*inf=*/false, mr);

    auto out = Value::matrix3d(1, 1, pages, ValueType::DOUBLE, mr);
    double *od = out.doubleDataMut();
    for (std::size_t pp = 0; pp < pages; ++pp) {
        auto pg = pageView(A, pp, m, n, mr);
        auto sv = scalarNormDispatch(pg, p, /*fro=*/false, /*inf=*/false, mr);
        od[pp] = sv.toScalar();
    }
    return out;
}

// ────────────────────────────────────────────────────────────────────────
// pagemldivide / pagemrdivide / pagelsqminnorm
// ────────────────────────────────────────────────────────────────────────

namespace {

// Validate compatible page counts. Allow one of the operands to be
// 2-D (broadcast over pages).
void checkPagePair(const Value &A, const Value &B, const char *who,
                   std::size_t &pages)
{
    const int nda = A.dims().ndim();
    const int ndb = B.dims().ndim();
    if (nda < 2 || nda > 3 || ndb < 2 || ndb > 3)
        throw Error(std::string(who) + ": inputs must be 2-D or 3-D",
                    0, 0, who, "", std::string("numkit:") + who + ":badDim");
    const std::size_t pa = (nda == 2) ? 1 : static_cast<std::size_t>(A.dims().dim(2));
    const std::size_t pb = (ndb == 2) ? 1 : static_cast<std::size_t>(B.dims().dim(2));
    if (pa == pb)            pages = pa;
    else if (pa == 1)        pages = pb;
    else if (pb == 1)        pages = pa;
    else
        throw Error(std::string(who) + ": page counts of A and B must match (or broadcast)",
                    0, 0, who, "", std::string("numkit:") + who + ":pageMismatch");
}

Value pageOfA(const Value &A, std::size_t p, std::size_t m, std::size_t n,
              std::pmr::memory_resource *mr)
{
    if (A.dims().ndim() == 2 || A.dims().dim(2) == 1)
        return pageView(A, 0, m, n, mr);
    return pageView(A, p, m, n, mr);
}

} // anonymous namespace

Value pagemldivide(const Value &A, const Value &B, std::pmr::memory_resource *mr)
{
    std::size_t pages = 0;
    checkPagePair(A, B, "pagemldivide", pages);
    const std::size_t mA = static_cast<std::size_t>(A.dims().dim(0));
    const std::size_t nA = static_cast<std::size_t>(A.dims().dim(1));
    const std::size_t mB = static_cast<std::size_t>(B.dims().dim(0));
    const std::size_t nB = static_cast<std::size_t>(B.dims().dim(1));
    if (mA != mB)
        throw Error("pagemldivide: row counts of A and B must match",
                    0, 0, "pagemldivide", "", "numkit:pagemldivide:badRows");

    const bool any3d = A.dims().ndim() == 3 || B.dims().ndim() == 3;
    auto out = any3d
        ? Value::matrix3d(nA, nB, pages, ValueType::DOUBLE, mr)
        : Value::matrix(nA, nB, ValueType::DOUBLE, mr);
    double *od = out.doubleDataMut();
    const std::size_t outStride = nA * nB;

    for (std::size_t p = 0; p < pages; ++p) {
        auto ap = pageOfA(A, p, mA, nA, mr);
        auto bp = pageOfA(B, p, mB, nB, mr);
        auto xp = linsolve(ap, bp, mr);
        std::copy(xp.doubleData(), xp.doubleData() + outStride, od + p * outStride);
    }
    return out;
}

Value pagemrdivide(const Value &A, const Value &B, std::pmr::memory_resource *mr)
{
    // X * B = A  ⇔  B' * X' = A'  ⇔  X' = B' \ A'.
    // We use that identity instead of duplicating the QR/LU dispatch.
    std::size_t pages = 0;
    checkPagePair(A, B, "pagemrdivide", pages);
    const std::size_t mA = static_cast<std::size_t>(A.dims().dim(0));
    const std::size_t nA = static_cast<std::size_t>(A.dims().dim(1));
    const std::size_t mB = static_cast<std::size_t>(B.dims().dim(0));
    const std::size_t nB = static_cast<std::size_t>(B.dims().dim(1));
    if (nA != nB)
        throw Error("pagemrdivide: column counts of A and B must match",
                    0, 0, "pagemrdivide", "", "numkit:pagemrdivide:badCols");

    const bool any3d = A.dims().ndim() == 3 || B.dims().ndim() == 3;
    auto out = any3d
        ? Value::matrix3d(mA, mB, pages, ValueType::DOUBLE, mr)
        : Value::matrix(mA, mB, ValueType::DOUBLE, mr);
    double *od = out.doubleDataMut();
    const std::size_t outStride = mA * mB;

    for (std::size_t p = 0; p < pages; ++p) {
        auto ap = pageOfA(A, p, mA, nA, mr);
        auto bp = pageOfA(B, p, mB, nB, mr);
        // Transpose Ap → ap_T (nA × mA) and Bp → bp_T (nB × mB) for
        // the "(B' \ A')' " identity.
        auto ap_T = Value::matrix(nA, mA, ValueType::DOUBLE, mr);
        auto bp_T = Value::matrix(nB, mB, ValueType::DOUBLE, mr);
        {
            const double *as = ap.doubleData();
            double *at = ap_T.doubleDataMut();
            for (std::size_t i = 0; i < mA; ++i)
                for (std::size_t j = 0; j < nA; ++j)
                    at[j + i * nA] = as[i + j * mA];
        }
        {
            const double *bs = bp.doubleData();
            double *bt = bp_T.doubleDataMut();
            for (std::size_t i = 0; i < mB; ++i)
                for (std::size_t j = 0; j < nB; ++j)
                    bt[j + i * nB] = bs[i + j * mB];
        }
        auto xt = linsolve(bp_T, ap_T, mr);  // (nB × mA), since nA == nB
        // Transpose xt back into out[:, :, p].
        const double *xs = xt.doubleData();
        double *dst = od + p * outStride;
        for (std::size_t i = 0; i < mA; ++i)
            for (std::size_t j = 0; j < mB; ++j)
                dst[i + j * mA] = xs[j + i * mB];
    }
    return out;
}

Value pagelsqminnorm(const Value &A, const Value &B, bool have_tol, double tol_user,
                     std::pmr::memory_resource *mr)
{
    std::size_t pages = 0;
    checkPagePair(A, B, "pagelsqminnorm", pages);
    const std::size_t mA = static_cast<std::size_t>(A.dims().dim(0));
    const std::size_t nA = static_cast<std::size_t>(A.dims().dim(1));
    const std::size_t mB = static_cast<std::size_t>(B.dims().dim(0));
    const std::size_t nB = static_cast<std::size_t>(B.dims().dim(1));
    if (mA != mB)
        throw Error("pagelsqminnorm: row counts of A and B must match",
                    0, 0, "pagelsqminnorm", "", "numkit:pagelsqminnorm:badRows");

    const bool any3d = A.dims().ndim() == 3 || B.dims().ndim() == 3;
    auto out = any3d
        ? Value::matrix3d(nA, nB, pages, ValueType::DOUBLE, mr)
        : Value::matrix(nA, nB, ValueType::DOUBLE, mr);
    double *od = out.doubleDataMut();
    const std::size_t outStride = nA * nB;

    for (std::size_t p = 0; p < pages; ++p) {
        auto ap = pageOfA(A, p, mA, nA, mr);
        auto bp = pageOfA(B, p, mB, nB, mr);
        auto xp = lsqminnorm(ap, bp, have_tol, tol_user, mr);
        std::copy(xp.doubleData(), xp.doubleData() + outStride, od + p * outStride);
    }
    return out;
}

// ════════════════════════════════════════════════════════════════════════
// Engine adapters
// ════════════════════════════════════════════════════════════════════════

} // namespace numkit::linalg
