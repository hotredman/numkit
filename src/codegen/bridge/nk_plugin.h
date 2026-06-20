/* nk_plugin.h — the numkit plugin-author ABI (DESIGN.md §6b).
 *
 * A plugin is a shared library (.dll / .so / .dylib) that exports two C entry
 * points. The host (numkit runtime) loads it via nk_load_plugin (see
 * nk_codegen_rt.h), checks the ABI version, then calls nk_plugin_register,
 * passing a table of host functions (nk_host_api). The plugin uses that table
 * to register its own functions and to box / unbox / call values.
 *
 * The plugin links against NOTHING from the host — it only includes this
 * header and uses the passed-in function pointers. No import library, no
 * symbol or binary-layout coupling: the ABI is a hard, versioned boundary.
 */
#ifndef NK_PLUGIN_H
#define NK_PLUGIN_H

#include "nk_codegen_rt.h" /* nk_val, nk_error, nk_fn */

#ifdef __cplusplus
extern "C" {
#endif

/* Bump on ANY incompatible change to nk_host_api or the value ABI. The host
 * refuses to load a plugin built against a different version. */
#define NK_PLUGIN_ABI_VERSION 1

/* Export macro for the plugin's two entry points. */
#if defined(_WIN32)
#  define NK_PLUGIN_EXPORT __declspec(dllexport)
#else
#  define NK_PLUGIN_EXPORT __attribute__((visibility("default")))
#endif

/* The table of host functions handed to the plugin at registration time.
 * Mirrors the free functions in nk_codegen_rt.h, but passed by pointer so the
 * plugin needs no import library and no link against the host. */
typedef struct nk_host_api {
    int abi_version; /* == NK_PLUGIN_ABI_VERSION the host was built with */
    int (*register_fn)(const char *name, nk_fn fn);
    nk_val (*box_scalar)(double v);
    nk_val (*box_array)(const double *p, size_t len);
    nk_val (*call)(const char *name, const nk_val *args, size_t nargs,
                   size_t nargout, nk_val *extra_outs, nk_error *err);
    double (*unbox_scalar)(nk_val v);
    void (*unbox_array)(nk_val v, double *out, size_t len);
    size_t (*numel)(nk_val v);
    void (*release)(nk_val v);
} nk_host_api;

/* The two entry points a plugin MUST export.
 *   nk_plugin_abi_version — return NK_PLUGIN_ABI_VERSION (the value you
 *                           compiled against); the host refuses a mismatch.
 *   nk_plugin_register    — register the plugin's functions via host->register_fn;
 *                           return 0 on success, non-zero to abort the load.
 *
 * Lifetime: the `host` table pointer stays valid for the process lifetime, so
 * a plugin MAY store it and use it later from its registered functions (as the
 * sample plugin does). */
NK_PLUGIN_EXPORT int nk_plugin_abi_version(void);
NK_PLUGIN_EXPORT int nk_plugin_register(const nk_host_api *host);

#ifdef __cplusplus
}
#endif

#endif /* NK_PLUGIN_H */
