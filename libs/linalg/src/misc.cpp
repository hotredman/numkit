// libs/linalg/src/misc.cpp
//
// Miscellaneous linalg utilities: rref + planerot.
// Migrated 2026-05-25 from libs/builtin/src/language/arrays/linalg_extras.cpp.

#include <numkit/linalg/misc.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/span.hpp>
#include <numkit/core/types.hpp>

#include <algorithm>
#include <cmath>
#include <limits>

namespace numkit::linalg {

namespace {

double matrix_inf_norm(const double *A, size_t M, size_t N)
{
    if (M == 0 || N == 0) return 0.0;
    double maxv = 0.0;
    for (size_t i = 0; i < M; ++i) {
        double s = 0.0;
        for (size_t j = 0; j < N; ++j) s += std::abs(A[i + j * M]);
        if (s > maxv) maxv = s;
    }
    return maxv;
}

} // namespace

// rref: Gauss-Jordan elimination with partial pivoting and tolerance gate.
std::pair<Value, Value>
rref(const Value &A, bool have_tol, double tol_user, std::pmr::memory_resource *mr)
{
    if (A.dims().is3D())
        throw Error("rref: input must be 2D",
                    0, 0, "rref", "", "numkit:rref:Not2D");

    const size_t M = A.dims().rows();
    const size_t N = A.dims().cols();
    Value R = Value::matrix(M, N, ValueType::DOUBLE, mr);
    if (M == 0 || N == 0) {
        Value jb = Value::matrix(1, 0, ValueType::DOUBLE, mr);
        return {R, jb};
    }

    ScratchArena scratch(mr);
    ScratchVec<double> B(M * N, &scratch);
    if (A.isComplex())
        throw Error("rref: complex input not supported in v1",
                    0, 0, "rref", "", "numkit:rref:NoComplex");
    std::copy(A.doubleData(), A.doubleData() + M * N, B.begin());

    double tol = tol_user;
    if (!have_tol) {
        const double anorm = matrix_inf_norm(B.data(), M, N);
        const double eps = std::numeric_limits<double>::epsilon();
        const double eps_anorm = (anorm == 0.0) ? eps
                                                 : std::nextafter(anorm,
                                                       std::numeric_limits<double>::infinity()) - anorm;
        tol = static_cast<double>(std::max(M, N)) * eps_anorm;
    }

    ScratchVec<double> jb_buf(0, &scratch);
    size_t i = 0;
    for (size_t j = 0; j < N && i < M; ++j) {
        size_t piv_row = i;
        double piv_val = std::abs(B[i + j * M]);
        for (size_t k = i + 1; k < M; ++k) {
            double v = std::abs(B[k + j * M]);
            if (v > piv_val) { piv_val = v; piv_row = k; }
        }

        if (piv_val <= tol) {
            for (size_t k = i; k < M; ++k) B[k + j * M] = 0.0;
            continue;
        }

        if (piv_row != i) {
            for (size_t jj = 0; jj < N; ++jj) {
                std::swap(B[i + jj * M], B[piv_row + jj * M]);
            }
        }

        double piv = B[i + j * M];
        for (size_t jj = 0; jj < N; ++jj) B[i + jj * M] /= piv;
        B[i + j * M] = 1.0;

        for (size_t k = 0; k < M; ++k) {
            if (k == i) continue;
            double f = B[k + j * M];
            if (f == 0.0) continue;
            for (size_t jj = 0; jj < N; ++jj) {
                B[k + jj * M] -= f * B[i + jj * M];
            }
            B[k + j * M] = 0.0;
        }

        jb_buf.push_back(static_cast<double>(j + 1));
        ++i;
    }

    std::copy(B.begin(), B.end(), R.doubleDataMut());

    Value jb = Value::matrix(jb_buf.empty() ? 0 : 1, jb_buf.size(),
                             ValueType::DOUBLE, mr);
    if (!jb_buf.empty())
        std::copy(jb_buf.begin(), jb_buf.end(), jb.doubleDataMut());
    return {R, jb};
}

// planerot: G * [x; y] = [r; 0] where r = hypot(x, y).
std::pair<Value, Value>
planerot(const Value &xy, std::pmr::memory_resource *mr)
{
    if (xy.dims().is3D() || xy.numel() != 2)
        throw Error("planerot: input must be a 2-element vector",
                    0, 0, "planerot", "", "numkit:planerot:BadShape");
    if (xy.isComplex())
        throw Error("planerot: complex input not supported",
                    0, 0, "planerot", "", "numkit:planerot:NoComplex");

    const double x = xy.elemAsDouble(0);
    const double y = xy.elemAsDouble(1);

    Value G = Value::matrix(2, 2, ValueType::DOUBLE, mr);
    Value yo = Value::matrix(2, 1, ValueType::DOUBLE, mr);
    double *gd = G.doubleDataMut();
    double *yd = yo.doubleDataMut();

    if (x == 0.0 && y == 0.0) {
        gd[0] = 1.0; gd[1] = 0.0; gd[2] = 0.0; gd[3] = 1.0;
        yd[0] = 0.0; yd[1] = 0.0;
        return {G, yo};
    }

    const double r = std::hypot(x, y);
    const double c = x / r;
    const double s = y / r;
    gd[0] = c;
    gd[1] = -s;
    gd[2] = s;
    gd[3] = c;
    yd[0] = r;
    yd[1] = 0.0;
    return {G, yo};
}

// ════════════════════════════════════════════════════════════════════════
// Engine adapters
// ════════════════════════════════════════════════════════════════════════

namespace detail {

void rref_reg(Span<const Value> args, size_t nargout,
              Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("rref: requires (A [, tol])",
                    0, 0, "rref", "", "numkit:rref:nargin");
    bool have_tol = (args.size() >= 2);
    double tol = have_tol ? args[1].toScalar() : 0.0;
    auto [R, jb] = rref(args[0], have_tol, tol, ctx.engine->resource());
    outs[0] = R;
    if (nargout >= 2 && outs.size() >= 2) outs[1] = jb;
}

void planerot_reg(Span<const Value> args, size_t nargout,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("planerot: requires ([x; y])",
                    0, 0, "planerot", "", "numkit:planerot:nargin");
    auto [G, y] = planerot(args[0], ctx.engine->resource());
    outs[0] = G;
    if (nargout >= 2 && outs.size() >= 2) outs[1] = y;
}

} // namespace detail

} // namespace numkit::linalg
