// sample_plugin.cpp — a minimal numkit plugin (DESIGN.md §6b), built as a
// standalone shared library and loaded at runtime by the plugin e2e test.
//
// It links against NOTHING from the host: it includes only nk_plugin.h and
// uses the host function table passed to nk_plugin_register. It registers one
// function, nk_sample_triple, which triples its scalar argument.

#include "nk_plugin.h"

namespace {

// Saved host API table — the plugin's registered function reaches box/unbox
// (and could reach call/numel/...) through it, never through a linked symbol.
const nk_host_api *g_host = nullptr;

nk_val sample_triple(const nk_val *args, size_t nargs, size_t nargout,
                     nk_val *extra_outs, nk_error *err)
{
    (void)nargs;
    (void)nargout;
    (void)extra_outs;
    (void)err;
    return g_host->box_scalar(g_host->unbox_scalar(args[0]) * 3.0);
}

} // namespace

extern "C" {

NK_PLUGIN_EXPORT int nk_plugin_abi_version(void) { return NK_PLUGIN_ABI_VERSION; }

NK_PLUGIN_EXPORT int nk_plugin_register(const nk_host_api *host)
{
    if (!host || host->abi_version != NK_PLUGIN_ABI_VERSION) return 1;
    g_host = host;
    return host->register_fn("nk_sample_triple", &sample_triple);
}

} // extern "C"
