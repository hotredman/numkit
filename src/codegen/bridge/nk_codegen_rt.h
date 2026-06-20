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

/* Error channel. code 0 == success. A C++ exception never crosses the C
 * boundary — it is caught and reported here (DESIGN.md §6b). */
typedef struct nk_error {
    int  code;            /* 0 = ok, non-zero = failure */
    char message[256];    /* NUL-terminated diagnostic (empty on success) */
} nk_error;

/* Box an unboxed value into a handle (boundary only — never a hot loop). */
nk_val nk_box_scalar(double v);
nk_val nk_box_array(const double *p, size_t len);  /* copies the data in */

/* Invoke builtin/registered function `name` on `args` (borrowed), producing
 * `nargout` results. Returns the first (owned); results [1..nargout-1] are
 * written (owned) into extra_outs[0..nargout-2] (extra_outs may be null when
 * nargout <= 1). On failure returns NULL and sets *err (if non-null);
 * never throws across this boundary. */
nk_val nk_call(const char *name, const nk_val *args, size_t nargs,
               size_t nargout, nk_val *extra_outs, nk_error *err);

/* Unbox. */
double nk_unbox_scalar(nk_val v);
void   nk_unbox_array(nk_val v, double *out, size_t len);  /* copies min(len,numel) */
size_t nk_numel(nk_val v);

/* Free an owned handle. */
void nk_release(nk_val v);

/* ---- Plugin / extension ABI (DESIGN.md §6b) -------------------------------
 *
 * A plugin PROVIDES functions with this signature — the mirror image of
 * nk_call. It receives borrowed `args` (do not release them), must produce
 * `nargout` results: return the first (OWNED — the runtime releases it) and
 * write results [1..nargout-1] (OWNED) into extra_outs[0..nargout-2]
 * (extra_outs is sized nargout-1, valid only when nargout > 1). To report a
 * failure, set err->code non-zero with a message and return NULL; the runtime
 * raises it as a numkit error at the call site. A plugin function must NEVER
 * throw a C++ exception across this boundary. */
typedef nk_val (*nk_fn)(const nk_val *args, size_t nargs, size_t nargout,
                        nk_val *extra_outs, nk_error *err);

/* Register `fn` under `name` into the numkit runtime. Thereafter `name` is
 * resolvable from numkit source, the embedding API, and nk_call — exactly
 * like a builtin. Returns 0 on success, non-zero on failure. Re-registering
 * an existing name overrides it. */
int nk_register_fn(const char *name, nk_fn fn);

/* Load a plugin shared library from `path`, verify its ABI version, and run
 * its registration hook (which registers the plugin's functions into this
 * runtime via the host API; see nk_plugin.h). Returns 0 on success; on
 * failure returns non-zero and sets *err. On success the library stays loaded
 * for the process lifetime — its registered function pointers remain live.
 * (DESIGN.md §6b) */
int nk_load_plugin(const char *path, nk_error *err);

#ifdef __cplusplus
}
#endif

#endif /* NK_CODEGEN_RT_H */
