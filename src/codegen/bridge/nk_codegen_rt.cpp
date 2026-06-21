// nk_codegen_rt.cpp — implementation of the Value-ABI bridge (see the
// header + DESIGN.md §6a). numkit (Value + a private StandardEngine) lives
// entirely behind the opaque C ABI; nothing leaks to the generated code.

#include "nk_codegen_rt.h"

#include "nk_plugin.h"

#include <numkit/bundle/standard_engine.hpp>
#include <numkit/core/engine.hpp>
#include <numkit/value/span.hpp>
#include <numkit/value/value.hpp>
#include <numkit/value/value_type.hpp>

#include <atomic>
#include <complex>
#include <cstdio>
#include <exception>
#include <limits>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#if defined(_WIN32)
#  define WIN32_LEAN_AND_MEAN
#  define NOMINMAX
#  include <windows.h>
#else
#  include <dlfcn.h>
#endif

namespace {

using numkit::Value;

// The encapsulated default engine — it carries the builtin registry. A
// function-local static (built on first use), NOT a global the generated
// code can see.
numkit::StandardEngine &engine()
{
    static numkit::StandardEngine e;
    return e;
}

// Function handle for `name` (`@name`), cached. Resolves builtins and
// path functions exactly as the interpreter would.
const Value &handleFor(const std::string &name)
{
    static std::unordered_map<std::string, Value> cache;
    const auto it = cache.find(name);
    if (it != cache.end()) return it->second;
    // suppressTopLevelDisplay = true: don't print `ans = @name`.
    return cache.emplace(name, engine().eval("@" + name, true)).first->second;
}

Value *unwrap(nk_val v) { return reinterpret_cast<Value *>(v); }
nk_val  wrap(Value *v) { return reinterpret_cast<nk_val>(v); }

// Live-handle balance counter. EVERY owned handle is created via make() and
// freed via destroy(), so `liveHandles` is the number of outstanding nk_val
// the runtime has handed out. A test snapshots it around a sequence and
// asserts it returns to baseline — leak-freedom of the bridge's own handle
// lifecycle, proven portably (MSVC's ASan has no LeakSanitizer). The inc/dec
// is on the boundary, never a hot loop.
std::atomic<long long> &liveHandles()
{
    static std::atomic<long long> n{0};
    return n;
}
nk_val make(Value v)
{
    nk_val h = wrap(new Value(std::move(v)));
    liveHandles().fetch_add(1, std::memory_order_relaxed);
    return h;
}
void destroy(nk_val v)
{
    if (!v) return;  // a null handle was never a live allocation
    liveHandles().fetch_sub(1, std::memory_order_relaxed);
    delete unwrap(v);
}

void setError(nk_error *err, const char *msg)
{
    if (!err) return;
    err->code = 1;
    std::snprintf(err->message, sizeof(err->message), "%s", msg ? msg : "error");
}

} // namespace

// Every entry is try/catch-guarded: a C++ exception (a MATLAB error, a bad
// argument, bad_alloc) is NEVER allowed to cross the extern "C" frame
// (that is UB). nk_call reports it via nk_error; the infallible-in-correct-
// use entries return a safe default.
extern "C" {

nk_val nk_box_scalar(double v)
{
    try {
        return make(Value::scalar(v));
    } catch (...) {
        return nullptr;
    }
}

nk_val nk_box_array(const double *p, size_t len)
{
    if (!p && len != 0) return nullptr;  // a null read is UB, not a throw
    try {
        Value   m = Value::matrix(1, len, numkit::ValueType::DOUBLE, nullptr);
        double *d = m.doubleDataMut();
        for (size_t i = 0; i < len; ++i) d[i] = p[i];
        return make(std::move(m));
    } catch (...) {
        return nullptr;
    }
}

nk_val nk_box_complex_array(const double *p, size_t len)
{
    if (!p && len != 0) return nullptr;  // p: interleaved re,im (2*len doubles)
    try {
        Value                 m = Value::complexMatrix(1, len, nullptr);
        std::complex<double> *d = m.complexDataMut();
        for (size_t i = 0; i < len; ++i) d[i] = std::complex<double>(p[2 * i], p[2 * i + 1]);
        return make(std::move(m));
    } catch (...) {
        return nullptr;
    }
}

nk_val nk_eval(const char *code, nk_error *err)
{
    if (err) { err->code = 0; err->message[0] = '\0'; }
    if (!code) { setError(err, "null code"); return nullptr; }
    try {
        // suppressTopLevelDisplay = true: an embedder wants the value back,
        // not `ans = ...` printed to the engine's output sink.
        return make(engine().eval(code, true));
    } catch (const std::exception &e) {
        setError(err, e.what());
        return nullptr;
    } catch (...) {
        setError(err, "unknown numkit error");
        return nullptr;
    }
}

nk_val nk_call(const char *name, const nk_val *args, size_t nargs,
               size_t nargout, nk_val *extra_outs, nk_error *err)
{
    if (err) { err->code = 0; err->message[0] = '\0'; }
    try {
        if (nargs != 0 && !args) throw std::runtime_error("nk_call: null args");
        std::vector<Value> a;
        a.reserve(nargs);
        for (size_t i = 0; i < nargs; ++i) {
            const Value *av = unwrap(args[i]);  // a null handle is UB to deref
            if (!av) throw std::runtime_error("nk_call: null argument handle");
            a.push_back(*av);
        }

        const size_t       nout = nargout == 0 ? 1 : nargout;
        std::vector<Value> outs = engine().callFunctionHandleMulti(
            handleFor(name), numkit::Span<const Value>(a.data(), a.size()), nout);

        for (size_t k = 1; k < nargout && extra_outs; ++k)
            extra_outs[k - 1] = make(k < outs.size() ? outs[k] : Value());
        return make(outs.empty() ? Value() : outs[0]);
    } catch (const std::exception &e) {
        setError(err, e.what());
        return nullptr;
    } catch (...) {
        setError(err, "unknown numkit error");
        return nullptr;
    }
}

double nk_unbox_scalar(nk_val v)
{
    try {
        return unwrap(v)->toScalar();
    } catch (...) {
        return std::numeric_limits<double>::quiet_NaN();
    }
}

void nk_unbox_array(nk_val v, double *out, size_t len)
{
    try {
        const Value  *val = unwrap(v);
        const double *d   = val->doubleData();
        const size_t  nm  = val->numel();
        for (size_t i = 0; i < len && i < nm; ++i) out[i] = d[i];
    } catch (...) {
        // leave `out` as the caller initialised it
    }
}

void nk_unbox_complex_array(nk_val v, double *out, size_t len)
{
    try {
        const Value *val = unwrap(v);
        const size_t nm  = val->numel();
        if (val->isComplex()) {  // out: interleaved re,im (2*len doubles)
            const std::complex<double> *d = val->complexData();
            for (size_t i = 0; i < len && i < nm; ++i) {
                out[2 * i]     = d[i].real();
                out[2 * i + 1] = d[i].imag();
            }
        } else {  // a real result (numkit narrowed a zero-imag complex) -> imag 0
            const double *d = val->doubleData();
            for (size_t i = 0; i < len && i < nm; ++i) {
                out[2 * i]     = d[i];
                out[2 * i + 1] = 0.0;
            }
        }
    } catch (...) {
        // leave `out` as the caller initialised it
    }
}

size_t nk_numel(nk_val v)
{
    try {
        return unwrap(v)->numel();
    } catch (...) {
        return 0;
    }
}

void nk_release(nk_val v) { destroy(v); }

long long nk_debug_live_handles(void)
{
    return liveHandles().load(std::memory_order_relaxed);
}

int nk_register_fn(const char *name, nk_fn fn)
{
    if (!name || !fn) return 1;
    try {
        // Adapt the plugin's C-ABI function into an ExternalFunc the engine
        // dispatches like any builtin. Marshalling crosses the boundary in
        // both directions; a plugin-signalled error becomes a thrown numkit
        // error (the engine's own dispatch catches + reports it — and so the
        // exception NEVER unwinds through the plugin's extern "C" frame, which
        // has already returned by the time we throw).
        nk_fn f = fn;
        engine().registerFunction(
            name,
            [f](numkit::Span<const Value> args, size_t nargout,
                numkit::Span<Value> outs, numkit::CallContext &) {
                std::vector<nk_val> in;  // owned by us, borrowed by the plugin
                in.reserve(args.size());
                for (const Value &a : args) in.push_back(make(a));

                const size_t        nout = nargout == 0 ? 1 : nargout;
                std::vector<nk_val> extra(nout > 1 ? nout - 1 : 0, nullptr);

                nk_error err;
                err.code    = 0;
                err.message[0] = '\0';
                nk_val r = f(in.data(), in.size(), nargout, extra.data(), &err);

                for (nk_val h : in) destroy(h);  // release inputs

                if (err.code != 0) {
                    std::string msg = err.message[0] ? err.message : "plugin error";
                    destroy(r);
                    for (nk_val h : extra) destroy(h);
                    throw std::runtime_error(msg);
                }

                if (!outs.empty()) outs[0] = r ? *unwrap(r) : Value();
                destroy(r);
                // Release EVERY owned extra output, whether or not it fits in
                // `outs` — decoupled from outs.size() so a slot count mismatch
                // can never leak a plugin-owned handle.
                for (size_t j = 0; j < extra.size(); ++j) {
                    const size_t k = j + 1;
                    if (k < outs.size()) outs[k] = extra[j] ? *unwrap(extra[j]) : Value();
                    destroy(extra[j]);
                }
            });
        return 0;
    } catch (...) {
        return 1;
    }
}

int nk_load_plugin(const char *path, nk_error *err)
{
    if (err) { err->code = 0; err->message[0] = '\0'; }
    if (!path) { setError(err, "null plugin path"); return 1; }
    try {
        // Idempotent per path: loading an already-loaded plugin is a no-op
        // success. Re-running its register hook would otherwise hit the
        // runtime's duplicate-registration guard. (Plugin loading is a setup
        // operation, not a concurrent hot path, so a plain static set is fine.)
        static std::unordered_set<std::string> loaded;
        if (loaded.count(path)) return 0;

        using version_fn  = int (*)();
        using register_fn = int (*)(const nk_host_api *);

#if defined(_WIN32)
        HMODULE lib = ::LoadLibraryA(path);
        if (!lib) { setError(err, "LoadLibrary failed (plugin not found or bad)"); return 1; }
        auto closeLib = [&] { ::FreeLibrary(lib); };
        auto ver = reinterpret_cast<version_fn>(
            reinterpret_cast<void *>(::GetProcAddress(lib, "nk_plugin_abi_version")));
        auto reg = reinterpret_cast<register_fn>(
            reinterpret_cast<void *>(::GetProcAddress(lib, "nk_plugin_register")));
#else
        void *lib = ::dlopen(path, RTLD_NOW | RTLD_LOCAL);
        if (!lib) { setError(err, ::dlerror()); return 1; }
        auto closeLib = [&] { ::dlclose(lib); };
        auto ver = reinterpret_cast<version_fn>(::dlsym(lib, "nk_plugin_abi_version"));
        auto reg = reinterpret_cast<register_fn>(::dlsym(lib, "nk_plugin_register"));
#endif

        // On any failure below, unmap the module (only a SUCCESSFUL load keeps
        // it resident, since its registered function pointers must stay live).
        if (!ver || !reg) {
            closeLib();
            setError(err, "plugin missing required entry points "
                          "(nk_plugin_abi_version / nk_plugin_register)");
            return 1;
        }
        if (ver() != NK_PLUGIN_ABI_VERSION) {
            closeLib();
            setError(err, "plugin ABI version mismatch");
            return 1;
        }

        // Hand the plugin a table of THIS runtime's functions (no symbol
        // coupling — see nk_plugin.h). The table is immutable and identical
        // for every plugin, so it is a single process-lifetime static: the
        // pointer the plugin receives stays valid forever (the plugin may
        // store it — the sample plugin does). A stack local would dangle the
        // moment this function returns.
        static const nk_host_api host = [] {
            nk_host_api h;
            h.abi_version  = NK_PLUGIN_ABI_VERSION;
            h.register_fn  = &nk_register_fn;
            h.box_scalar   = &nk_box_scalar;
            h.box_array    = &nk_box_array;
            h.call         = &nk_call;
            h.unbox_scalar = &nk_unbox_scalar;
            h.unbox_array  = &nk_unbox_array;
            h.numel        = &nk_numel;
            h.release      = &nk_release;
            return h;
        }();

        const int rc = reg(&host);
        if (rc != 0) {
            closeLib();
            setError(err, "plugin registration hook returned an error");
            return rc;
        }
        loaded.insert(path);  // record only on success, so a failed load retries
        // `lib` is intentionally left loaded: its registered function pointers
        // must stay live for the process lifetime. (A proper unload API would
        // first unregister those names; deferred.)
        return 0;
    } catch (const std::exception &e) {
        setError(err, e.what());
        return 1;
    } catch (...) {
        setError(err, "unknown error loading plugin");
        return 1;
    }
}

} // extern "C"
