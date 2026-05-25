// libs/linalg/src/library.cpp
//
// Registration hub for the Linear Algebra Toolbox.
//
// Naming convention:
//   Each function is registered under   `linalg.<sub>.<name>`
//   and aliased into                    `compat.<name>`.
// The `compat.<name>` alias keeps every script that does
//   import compat.*
//   y = svd(A);
// working without code changes — the same shape libs/{stats,signal}
// adopted when they moved out of libs/builtin.
//
// Functions land here per migration group as they move out of
// libs/builtin/src/language/arrays/.

#include <numkit/linalg/library.hpp>
#include <numkit/core/types.hpp>

namespace numkit::linalg::detail {
// vector_ops.cpp
void cross_reg(Span<const Value>, size_t, Span<Value>, CallContext &);
void dot_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void kron_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
// norms.cpp
void norm_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void vecnorm_reg(Span<const Value>, size_t, Span<Value>, CallContext &);
// properties.cpp
void inv_reg    (Span<const Value>, size_t, Span<Value>, CallContext &);
void trace_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void det_reg    (Span<const Value>, size_t, Span<Value>, CallContext &);
void rank_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void cond_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void normest_reg(Span<const Value>, size_t, Span<Value>, CallContext &);
void rcond_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void condest_reg(Span<const Value>, size_t, Span<Value>, CallContext &);
void condeig_reg(Span<const Value>, size_t, Span<Value>, CallContext &);
// decompositions.cpp
void chol_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void lu_reg     (Span<const Value>, size_t, Span<Value>, CallContext &);
void qr_reg     (Span<const Value>, size_t, Span<Value>, CallContext &);
void svd_reg    (Span<const Value>, size_t, Span<Value>, CallContext &);
void cholupdate_reg(Span<const Value>, size_t, Span<Value>, CallContext &);
void qrupdate_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void qrinsert_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void qrdelete_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
// pseudo_subspace.cpp
void pinv_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void orth_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void null_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void subspace_reg(Span<const Value>, size_t, Span<Value>, CallContext &);
// balance.cpp
void balance_reg(Span<const Value>, size_t, Span<Value>, CallContext &);
// ldl.cpp
void ldl_reg    (Span<const Value>, size_t, Span<Value>, CallContext &);
// eig.cpp
void eig_reg      (Span<const Value>, size_t, Span<Value>, CallContext &);
void hess_reg     (Span<const Value>, size_t, Span<Value>, CallContext &);
void schur_reg    (Span<const Value>, size_t, Span<Value>, CallContext &);
void sylvester_reg(Span<const Value>, size_t, Span<Value>, CallContext &);
// schur_convert.cpp
void cdf2rdf_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void rsf2csf_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
// matrix_functions.cpp
void expm_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
void logm_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
void sqrtm_reg(Span<const Value>, size_t, Span<Value>, CallContext &);
// predicates.cpp
void issymmetric_reg(Span<const Value>, size_t, Span<Value>, CallContext &);
void ishermitian_reg(Span<const Value>, size_t, Span<Value>, CallContext &);
void isbanded_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void isdiag_reg     (Span<const Value>, size_t, Span<Value>, CallContext &);
void istril_reg     (Span<const Value>, size_t, Span<Value>, CallContext &);
void istriu_reg     (Span<const Value>, size_t, Span<Value>, CallContext &);
void bandwidth_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
// solvers.cpp
void linsolve_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void lsqminnorm_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
void lsqnonneg_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
// misc.cpp
void rref_reg       (Span<const Value>, size_t, Span<Value>, CallContext &);
void planerot_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
// page_ops.cpp
void pageinv_reg        (Span<const Value>, size_t, Span<Value>, CallContext &);
void pageeig_reg        (Span<const Value>, size_t, Span<Value>, CallContext &);
void pagesvd_reg        (Span<const Value>, size_t, Span<Value>, CallContext &);
void pagepinv_reg       (Span<const Value>, size_t, Span<Value>, CallContext &);
void pagenorm_reg       (Span<const Value>, size_t, Span<Value>, CallContext &);
void pagemldivide_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void pagemrdivide_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void pagelsqminnorm_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
} // namespace numkit::linalg::detail

namespace numkit {

void LinalgLibrary::install(Engine &engine)
{
    // Every function lands in THREE places:
    //   1. Bare global name `<name>`           — matches MATLAB, which
    //      exposes the entire linalg surface unqualified (`eig(A)` works
    //      with no import). Also what BuiltinLibrary did for these
    //      functions before they migrated here.
    //   2. Namespaced `linalg.<sub>.<name>`    — addressable explicitly
    //      after `import linalg.*` or as a fully qualified call.
    //   3. `compat.<name>`                     — picked up by the standard
    //      `import compat.*` test fixture (libs/{stats,signal} pattern).
    auto reg = [&](const char *sub, const char *name, ExternalFunc fn) {
        engine.registerFunction(name, fn);                                    // 1
        engine.registerFunction(std::string("linalg.") + sub, name, fn);      // 2
        engine.registerFunction("compat", name, fn);                          // 3
    };

    // ── Vector ops ───────────────────────────────────────────────
    reg("vector", "cross", &linalg::detail::cross_reg);
    reg("vector", "dot",   &linalg::detail::dot_reg);
    reg("vector", "kron",  &linalg::detail::kron_reg);

    // ── Norms ────────────────────────────────────────────────────
    reg("norm", "norm",    &linalg::detail::norm_reg);
    reg("norm", "vecnorm", &linalg::detail::vecnorm_reg);

    // ── Properties ───────────────────────────────────────────────
    reg("prop", "inv",     &linalg::detail::inv_reg);
    reg("prop", "trace",   &linalg::detail::trace_reg);
    reg("prop", "det",     &linalg::detail::det_reg);
    reg("prop", "rank",    &linalg::detail::rank_reg);
    reg("prop", "cond",    &linalg::detail::cond_reg);
    reg("prop", "normest", &linalg::detail::normest_reg);
    reg("prop", "rcond",   &linalg::detail::rcond_reg);
    reg("prop", "condest", &linalg::detail::condest_reg);
    reg("prop", "condeig", &linalg::detail::condeig_reg);

    // ── Decompositions ───────────────────────────────────────────
    reg("decomp", "chol", &linalg::detail::chol_reg);
    reg("decomp", "lu",   &linalg::detail::lu_reg);
    reg("decomp", "qr",   &linalg::detail::qr_reg);
    reg("decomp", "svd",  &linalg::detail::svd_reg);
    reg("decomp", "cholupdate", &linalg::detail::cholupdate_reg);
    reg("decomp", "qrupdate",   &linalg::detail::qrupdate_reg);
    reg("decomp", "qrinsert",   &linalg::detail::qrinsert_reg);
    reg("decomp", "qrdelete",   &linalg::detail::qrdelete_reg);

    // ── Pseudo-inverse / subspace queries ────────────────────────
    reg("pseudo", "pinv",     &linalg::detail::pinv_reg);
    reg("pseudo", "orth",     &linalg::detail::orth_reg);
    reg("pseudo", "null",     &linalg::detail::null_reg);
    reg("pseudo", "subspace", &linalg::detail::subspace_reg);

    // ── Specialty decompositions ─────────────────────────────────
    reg("decomp", "balance", &linalg::detail::balance_reg);
    reg("decomp", "ldl",     &linalg::detail::ldl_reg);

    // ── Eig family + Hessenberg + Schur + Sylvester ──────────────
    reg("eig", "eig",       &linalg::detail::eig_reg);
    reg("eig", "hess",      &linalg::detail::hess_reg);
    reg("eig", "schur",     &linalg::detail::schur_reg);
    reg("eig", "sylvester", &linalg::detail::sylvester_reg);
    reg("eig", "cdf2rdf",   &linalg::detail::cdf2rdf_reg);
    reg("eig", "rsf2csf",   &linalg::detail::rsf2csf_reg);

    // ── Matrix functions ─────────────────────────────────────────
    reg("matfn", "expm",  &linalg::detail::expm_reg);
    reg("matfn", "logm",  &linalg::detail::logm_reg);
    reg("matfn", "sqrtm", &linalg::detail::sqrtm_reg);

    // ── Predicates ───────────────────────────────────────────────
    reg("pred", "issymmetric", &linalg::detail::issymmetric_reg);
    reg("pred", "ishermitian", &linalg::detail::ishermitian_reg);
    reg("pred", "isbanded",    &linalg::detail::isbanded_reg);
    reg("pred", "isdiag",      &linalg::detail::isdiag_reg);
    reg("pred", "istril",      &linalg::detail::istril_reg);
    reg("pred", "istriu",      &linalg::detail::istriu_reg);
    reg("pred", "bandwidth",   &linalg::detail::bandwidth_reg);

    // ── Solvers ──────────────────────────────────────────────────
    reg("solve", "linsolve",   &linalg::detail::linsolve_reg);
    reg("solve", "lsqminnorm", &linalg::detail::lsqminnorm_reg);
    reg("solve", "lsqnonneg",  &linalg::detail::lsqnonneg_reg);

    // ── Misc utilities ───────────────────────────────────────────
    reg("misc", "rref",     &linalg::detail::rref_reg);
    reg("misc", "planerot", &linalg::detail::planerot_reg);

    // ── Page ops ─────────────────────────────────────────────────
    reg("page", "pageinv",        &linalg::detail::pageinv_reg);
    reg("page", "pageeig",        &linalg::detail::pageeig_reg);
    reg("page", "pagesvd",        &linalg::detail::pagesvd_reg);
    reg("page", "pagepinv",       &linalg::detail::pagepinv_reg);
    reg("page", "pagenorm",       &linalg::detail::pagenorm_reg);
    reg("page", "pagemldivide",   &linalg::detail::pagemldivide_reg);
    reg("page", "pagemrdivide",   &linalg::detail::pagemrdivide_reg);
    reg("page", "pagelsqminnorm", &linalg::detail::pagelsqminnorm_reg);
}

} // namespace numkit
