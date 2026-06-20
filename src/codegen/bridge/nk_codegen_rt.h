/* nk_codegen_rt.h — the Value-ABI bridge (DESIGN.md §6a).
 *
 * A minimal, stable, OPAQUE C ABI that lets AOT-compiled numkit code call
 * uncompiled builtins / library functions and hold Dynamic values, WITHOUT
 * depending on numkit's C++ Value type, headers, binary layout, or its
 * transitive dependency graph. The implementation (nk_codegen_rt.cpp) owns
 * numkit (Value + a private StandardEngine) entirely behind this boundary.
 *
 * Ownership: box_* and call return OWNED handles the caller must nk_release.
 * call BORROWS its argument handles (caller still owns them). A handle is
 * opaque — never dereference it.
 */
#ifndef NK_CODEGEN_RT_H
#define NK_CODEGEN_RT_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct nk_val_s *nk_val;  /* opaque boxed numkit value */

/* Box an unboxed value into a handle (boundary only — never a hot loop). */
nk_val nk_box_scalar(double v);
nk_val nk_box_array(const double *p, size_t len);  /* copies the data in */

/* Invoke builtin/registered function `name` on `args` (borrowed), producing
 * `nargout` results. Returns the first (owned); results [1..nargout-1] are
 * written (owned) into extra_outs[0..nargout-2] (extra_outs may be null when
 * nargout <= 1). */
nk_val nk_call(const char *name, const nk_val *args, size_t nargs,
               size_t nargout, nk_val *extra_outs);

/* Unbox. */
double nk_unbox_scalar(nk_val v);
void   nk_unbox_array(nk_val v, double *out, size_t len);  /* copies min(len,numel) */
size_t nk_numel(nk_val v);

/* Free an owned handle. */
void nk_release(nk_val v);

#ifdef __cplusplus
}
#endif

#endif /* NK_CODEGEN_RT_H */
