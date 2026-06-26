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
#include <stdint.h>

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
/* Box a NUL-terminated C string as a 1 x len CHAR row (one byte per code unit) -- for a
 * bridged char ARG such as a sprintf format string. */
NK_RT_API nk_val nk_box_string(const char *s);
/* Complex array: `p` is interleaved re,im — `len` complex elements = 2*len
 * doubles, matching std::complex<double>[len] (codegen passes a
 * reinterpret_cast<const double*> of its std::complex buffer). */
NK_RT_API nk_val nk_box_complex_array(const double *p, size_t len);
/* Shape-preserving boxers for a Dynamic ARRAY operand (column-major, matching
 * the codegen RawBuffer layout): a 2-D rows×cols matrix, and an N-D array whose
 * `nd` dims are `dims[0..nd)`. `p` holds rows*cols (resp. prod(dims)) doubles. */
NK_RT_API nk_val nk_box_matrix(const double *p, size_t rows, size_t cols);
NK_RT_API nk_val nk_box_array_nd(const double *p, const size_t *dims, int nd);
/* Complex counterparts: `p` is interleaved re,im (2*numel doubles, column-major),
 * matching std::complex<double>[numel]. 2-D rows×cols, and N-D `dims[0..nd)`. */
NK_RT_API nk_val nk_box_complex_matrix(const double *p, size_t rows, size_t cols);
NK_RT_API nk_val nk_box_complex_array_nd(const double *p, const size_t *dims, int nd);

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

/* ---- Dynamic-tier value operations (DESIGN.md §10 C1) ----------------------
 *
 * Apply an operator to boxed operands, identical to the interpreter (overload-
 * aware): the codegen Dynamic tier emits these where a value's type could not
 * be inferred, so a typed inline op is unavailable. Operands are BORROWED;
 * the result is OWNED (release with nk_release). On failure return NULL and
 * set *err. `op` is the numkit source token. */
NK_RT_API nk_val nk_binop(const char *op, nk_val a, nk_val b, nk_error *err);
/* Unary op: token "-", "+", "~", "'", ".'". Borrowed operand, owned result. */
NK_RT_API nk_val nk_unop(const char *op, nk_val a, nk_error *err);
/* MATLAB truthiness of `v` (an `if` / `while` condition): 1 iff `v` is
 * non-empty and EVERY element is non-zero, else 0. On failure returns 0 and
 * sets *err. (`v` is borrowed.) */
NK_RT_API int nk_truth(nk_val v, nk_error *err);
/* Apply subscripts to a boxed value: a(subs) — resolved EXACTLY as the
 * interpreter (`value(subs)` is index/call-ambiguous): a function handle /
 * closure is CALLED, an object dispatches to its subsref, otherwise it is
 * array indexing. v1 supports a single subscript (the subscript may be a
 * vector → a sub-array). `subs` borrowed; returns an OWNED result; on failure
 * NULL + *err. (DESIGN.md §10 C1 A4.) */
NK_RT_API nk_val nk_index(nk_val a, const nk_val *subs, size_t nsubs, nk_error *err);

/* Unbox. */
NK_RT_API double nk_unbox_scalar(nk_val v);
NK_RT_API void   nk_unbox_array(nk_val v, double *out, size_t len);  /* copies min(len,numel) */
/* Complex unbox: `out` is interleaved re,im (len complex = 2*len doubles).
 * Handles a REAL result too (imag = 0) — numkit may narrow a zero-imag complex
 * result back to a real array. */
NK_RT_API void   nk_unbox_complex_array(nk_val v, double *out, size_t len);
/* Char-array unbox: copies the result's char CODE UNITS (one per element) into
 * `out` (a uint16 buffer, the codegen's char width); copies min(len,numel). For a
 * CHAR result of num2str/sprintf/... — the runtime owns the formatting. */
NK_RT_API void   nk_unbox_char_array(nk_val v, uint16_t *out, size_t len);
NK_RT_API size_t nk_numel(nk_val v);

/* Duplicate a handle: a new OWNED handle holding a copy of `v`'s value (the
 * Dynamic tier's val wrapper is copyable, matching MATLAB value semantics).
 * Returns NULL if `v` is null. (`v` is borrowed.) */
NK_RT_API nk_val nk_clone(nk_val v);

/* Free an owned handle. */
NK_RT_API void nk_release(nk_val v);

/* The number of owned handles currently outstanding (created by a box / eval /
 * call entry, not yet nk_release'd). For leak testing: snapshot it around a
 * sequence and assert it returns to baseline. */
NK_RT_API long long nk_debug_live_handles(void);

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
