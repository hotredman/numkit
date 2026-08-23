/// @file library.hpp
/// @ingroup group_matfun
// toolboxes/linalg/include/numkit/linalg/library.hpp
//
// Linear Algebra Toolbox — user-facing linalg functions (lu, qr, chol,
// svd, eig, det, inv, pinv, norm, expm, …). Mirrors MATLAB's Linear
// Algebra documentation tree.
//
// Layering note:
//   toolboxes/builtin retains the operator-side implementations of the
//   matrix operators *, \, /, ^, ', .'  — those live in
//   toolboxes/builtin/src/language/operators/ because they are compiled
//   directly from the parser/VM (`mldivide` is the runtime backing
//   of  `\` ). Both they and this toolbox call the shared linear-solve
//   kernel `numkit::ops::la_solve` (<numkit/ops/la_solve.hpp>), which
//   lives in the L0.5 ops layer below both builtin and linalg.
//
//   This toolbox library REUSES that `la_solve` kernel for its
//   user-facing `lu` / `qr` / `linsolve` etc.

#pragma once

namespace numkit {

/// @addtogroup group_matfun
/// @{
 class Engine; }  // fwd-decl — keep this public header core-free

namespace numkit {

class LinalgLibrary
{
public:
    /// Register every linalg function under the `linalg.<sub>.<name>`
    /// namespace and alias each into `compat.<name>` (so scripts using
    /// `import compat.*` see them flat — preserves the legacy surface
    /// they had while living in toolboxes/builtin).
    static void install(Engine &engine);
};


/// @}
} // namespace numkit
