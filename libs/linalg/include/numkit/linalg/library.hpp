// libs/linalg/include/numkit/linalg/library.hpp
//
// Linear Algebra Toolbox — user-facing linalg functions (lu, qr, chol,
// svd, eig, det, inv, pinv, norm, expm, …). Mirrors MATLAB's Linear
// Algebra documentation tree.
//
// Layering note:
//   libs/builtin retains the operator-side implementations of the
//   matrix operators *, \, /, ^, ', .'  — those live in
//   libs/builtin/src/language/operators/ because they are compiled
//   directly from the parser/VM (`mldivide` is the runtime backing
//   of  `\` ). They depend only on the in-builtin internal kernel
//   `la_solve` (libs/builtin/src/language/operators/la_solve.hpp).
//
//   This toolbox library REUSES the same `la_solve` kernel for its
//   user-facing `lu` / `qr` / `linsolve` etc. Edge direction:
//     core → builtin → linalg
//   builtin MUST NOT include any header from linalg.

#pragma once

#include <numkit/core/engine.hpp>

namespace numkit {

class LinalgLibrary
{
public:
    /// Register every linalg function under the `linalg.<sub>.<name>`
    /// namespace and alias each into `compat.<name>` (so scripts using
    /// `import compat.*` see them flat — preserves the legacy surface
    /// they had while living in libs/builtin).
    static void install(Engine &engine);
};

} // namespace numkit
