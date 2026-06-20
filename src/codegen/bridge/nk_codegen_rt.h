/* nk_codegen_rt.h — the Value-ABI bridge (DESIGN.md §6a).
 *
 * A minimal, stable, OPAQUE C ABI that lets AOT-compiled numkit code call
 * uncompiled builtins / library functions and hold Dynamic values, WITHOUT
 * depending on numkit's C++ Value type, headers, binary layout, or its
 * transitive dependency graph. The implementation (nk_codegen_rt.cpp) owns
 * numkit (Value + a private StandardEngine) entirely behind this boundary.
 *
 * Ownership: box_* / eval / call return OWNED handles the caller must release
 * with nk_release — and ONLY with nk_release. A handle's storage belongs to
 * this runtime (the DLL when built as one); never free/delete it yourself or
 * across a different allocator/CRT. call BORROWS its argument handles (caller
 * still owns them). A handle is opaque — never dereference it.
 *
 * Threading: NOT thread-safe. All entry points share one process-wide engine
 * (+ caches); serialise calls, or give each thread its own process. (The
 * underlying interpreter is single-threaded.)
 */
#ifndef NK_CODEGEN_RT_H
#define NK_CODEGEN_RT_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Linkage. Default (static / same TU — e.g. compiled into a test, the driver,
 * or a self-contained artifact): plain. Building the nk_codegen_rt shared
 * lib: dllexport. A consumer linking that shared lib (a bridged AOT artifact):
 * define NK_RT_USE_DLL -> dllimport. No effect off Windows. */
#if defined(_WIN32) && defined(NK_RT_BUILDING_DLL)
#  define NK_RT_API __declspec(dllexport)
#elif defined(_WIN32) && defined(NK_RT_USE_DLL)
#  define NK_RT_API __declspec(dllimport)
#else
#  define NK_RT_API
#endif

typedef struct nk_val_s *nk_val;  /* opaque boxed numkit value */

/* Error channel. code 0 == success. A C++ exception never crosses the C
 * boundary — it is caught and reported here (DESIGN.md §6b). */
typedef struct nk_error {
    int  code;            /* 0 = ok, non-zero = failure */
    char message[256];    /* NUL-terminated diagnostic (empty on success) */
} nk_error;

/* Box an unboxed value into a handle (boundary only — never a hot loop). */
NK_RT_API nk_val nk_box_scalar(double v);
NK_RT_API nk_val nk_box_array(const double *p, size_t len);  /* copies the data in */

/* Evaluate numkit source `code` in the runtime's persistent workspace and
 * return the last expression's value (owned; an empty handle for pure
 * statements). State persists across calls — nk_eval("x=5;",0) then
 * nk_eval("x+1",0) yields 6. On failure returns NULL and sets *err. This is
 * the host embedding entry point (DESIGN.md §6b). */
NK_RT_API nk_val nk_eval(const char *code, nk_error *err);

/* Invoke builtin/registered function `name` on `args` (borrowed), producing
 * `nargout` results. Returns the first (owned); results [1..nargout-1] are
 * written (owned) into extra_outs[0..nargout-2] (extra_outs may be null when
 * nargout <= 1). On failure returns NULL and sets *err (if non-null);
 * never throws across this boundary. */
NK_RT_API nk_val nk_call(const char *name, const nk_val *args, size_t nargs,
                         size_t nargout, nk_val *extra_outs, nk_error *err);

/* Unbox. */
NK_RT_API double nk_unbox_scalar(nk_val v);
NK_RT_API void   nk_unbox_array(nk_val v, double *out, size_t len);  /* copies min(len,numel) */
NK_RT_API size_t nk_numel(nk_val v);

/* Free an owned handle. */
NK_RT_API void nk_release(nk_val v);

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
 * like a builtin. Returns 0 on success, non-zero on failure — including if
 * `name` is already registered (the runtime rejects duplicates; there is no
 * override without an unregister API). */
NK_RT_API int nk_register_fn(const char *name, nk_fn fn);

/* Load a plugin shared library from `path`, verify its ABI version, and run
 * its registration hook (which registers the plugin's functions into this
 * runtime via the host API; see nk_plugin.h). Returns 0 on success; on
 * failure returns non-zero and sets *err. On success the library stays loaded
 * for the process lifetime — its registered function pointers remain live.
 * Idempotent per path: loading an already-loaded plugin is a no-op success.
 * (DESIGN.md §6b) */
NK_RT_API int nk_load_plugin(const char *path, nk_error *err);

#ifdef __cplusplus
}
#endif

#endif /* NK_CODEGEN_RT_H */
