// toolboxes/signal/src/windows/windows_reg.cpp
//
// Register half of the signal windows builtins: the CallContext wrappers
// delegating to the engine-free compute in windows.cpp. library.cpp
// forward-declares + registers these by name.
//
// Phase 2b compute/register split — see project_layering_refactor memory.

#include <numkit/signal/windows/windows.hpp>

#include <numkit/core/engine.hpp>   // CallContext, Span, ctx.engine->resource()
#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/span.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace numkit::signal {

namespace detail {

namespace {

// Detect MATLAB's `'symmetric'` (default) / `'periodic'` window flag at
// the trailing argument position. Returns the effective positional arg
// count; sets `periodic = true` for the periodic form.
size_t parseSflag(Span<const Value> args, bool &periodic)
{
    periodic = false;
    if (args.empty()) return 0;
    const Value &last = args[args.size() - 1];
    if (!last.isChar() && !last.isString()) return args.size();
    std::string s = last.toString();
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    if (s == "symmetric") return args.size() - 1;
    if (s == "periodic") { periodic = true; return args.size() - 1; }
    return args.size();   // not an sflag — leave for caller
}

// Periodic-form trick: for any window function f(N) computing the
// symmetric variant, the MATLAB periodic variant is the first N samples
// of f(N+1). This avoids modifying every window's implementation.
template <typename Fn>
Value applySflag(size_t N, bool periodic, Fn impl, std::pmr::memory_resource *mr)
{
    if (!periodic) return impl(N);
    Value full = impl(N + 1);
    Value out = Value::matrix(N, 1, ValueType::DOUBLE, mr);
    if (N > 0)
        std::copy(full.doubleData(), full.doubleData() + N, out.doubleDataMut());
    return out;
}

// Some windows (bartlett, triang, parzenwin, bohmanwin, barthannwin,
// rectwin) accept ONLY a `typeName` flag ('double' / 'single') — they
// reject 'periodic' explicitly with the documented MATLAB error.
// `single` output cast is currently a no-op: numkit emits double
// regardless of the requested typeName.
void parseTypeNameOnly(Span<const Value> args, const char *fn)
{
    if (args.size() < 2) return;
    const Value &last = args[args.size() - 1];
    if (!last.isChar() && !last.isString()) return;
    std::string s = last.toString();
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    if (s != "double" && s != "single")
        throw Error(std::string(fn) + ": Expected TYPENAME to match one of "
                    "these values: 'double', 'single' (got '" + s + "')",
                    0, 0, fn, "", std::string("numkit:") + fn + ":typeName");
}

} // anonymous

void hamming_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("hamming: requires (N[, sflag])",
                     0, 0, "hamming", "", "numkit:hamming:nargin");
    bool periodic = false; (void)parseSflag(args, periodic);
    auto *mr = ctx.engine->resource();
    outs[0] = applySflag(static_cast<size_t>(args[0].toScalar()), periodic, [&](size_t M){ return hamming(M, mr); }, mr);
}

void hann_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("hann: requires (N[, sflag])",
                     0, 0, "hann", "", "numkit:hann:nargin");
    bool periodic = false; (void)parseSflag(args, periodic);
    auto *mr = ctx.engine->resource();
    outs[0] = applySflag(static_cast<size_t>(args[0].toScalar()), periodic, [&](size_t M){ return hann(M, mr); }, mr);
}

void blackman_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("blackman: requires (N[, sflag])",
                     0, 0, "blackman", "", "numkit:blackman:nargin");
    bool periodic = false; (void)parseSflag(args, periodic);
    auto *mr = ctx.engine->resource();
    outs[0] = applySflag(static_cast<size_t>(args[0].toScalar()), periodic, [&](size_t M){ return blackman(M, mr); }, mr);
}

void kaiser_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("kaiser: requires at least 1 argument",
                     0, 0, "kaiser", "", "numkit:kaiser:nargin");
    const size_t N = static_cast<size_t>(args[0].toScalar());
    const double beta = (args.size() >= 2) ? args[1].toScalar() : 0.5;
    outs[0] = kaiser(N, beta, ctx.engine->resource());
}

void rectwin_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("rectwin: requires (N[, typeName])",
                     0, 0, "rectwin", "", "numkit:rectwin:nargin");
    parseTypeNameOnly(args, "rectwin");
    outs[0] = rectwin(static_cast<size_t>(args[0].toScalar()), ctx.engine->resource());
}

void bartlett_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("bartlett: requires (N[, typeName])",
                     0, 0, "bartlett", "", "numkit:bartlett:nargin");
    parseTypeNameOnly(args, "bartlett");
    outs[0] = bartlett(static_cast<size_t>(args[0].toScalar()), ctx.engine->resource());
}

// Local helper: extract N from arg[0] with a `name` for error messages.
static size_t windowN(const Value &a, const char *name)
{
    return static_cast<size_t>(a.toScalar());
    (void)name; // reserved for future range checks
}

void triang_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("triang: requires (N[, typeName])",
                     0, 0, "triang", "", "numkit:triang:nargin");
    parseTypeNameOnly(args, "triang");
    outs[0] = triang(windowN(args[0], "triang"), ctx.engine->resource());
}

void tukeywin_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("tukeywin: requires at least 1 argument",
                     0, 0, "tukeywin", "", "numkit:tukeywin:nargin");
    const size_t N = windowN(args[0], "tukeywin");
    const double r = (args.size() >= 2) ? args[1].toScalar() : 0.5;
    outs[0] = tukeywin(N, r, ctx.engine->resource());
}

void flattopwin_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("flattopwin: requires (N[, sflag])",
                     0, 0, "flattopwin", "", "numkit:flattopwin:nargin");
    bool periodic = false; (void)parseSflag(args, periodic);
    auto *mr = ctx.engine->resource();
    outs[0] = applySflag(windowN(args[0], "flattopwin"), periodic, [&](size_t M){ return flattopwin(M, mr); }, mr);
}

void gausswin_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("gausswin: requires at least 1 argument",
                     0, 0, "gausswin", "", "numkit:gausswin:nargin");
    const size_t N = windowN(args[0], "gausswin");
    const double alpha = (args.size() >= 2) ? args[1].toScalar() : 2.5;
    outs[0] = gausswin(N, alpha, ctx.engine->resource());
}

void chebwin_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("chebwin: requires at least 1 argument",
                     0, 0, "chebwin", "", "numkit:chebwin:nargin");
    const size_t N = windowN(args[0], "chebwin");
    const double at = (args.size() >= 2) ? args[1].toScalar() : 100.0;
    outs[0] = chebwin(N, at, ctx.engine->resource());
}

void parzenwin_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("parzenwin: requires (N[, typeName])",
                     0, 0, "parzenwin", "", "numkit:parzenwin:nargin");
    parseTypeNameOnly(args, "parzenwin");
    outs[0] = parzenwin(windowN(args[0], "parzenwin"), ctx.engine->resource());
}

void nuttallwin_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("nuttallwin: requires (N[, sflag])",
                     0, 0, "nuttallwin", "", "numkit:nuttallwin:nargin");
    bool periodic = false; (void)parseSflag(args, periodic);
    auto *mr = ctx.engine->resource();
    outs[0] = applySflag(windowN(args[0], "nuttallwin"), periodic, [&](size_t M){ return nuttallwin(M, mr); }, mr);
}

void taylorwin_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("taylorwin: requires at least 1 argument",
                     0, 0, "taylorwin", "", "numkit:taylorwin:nargin");
    const size_t N = windowN(args[0], "taylorwin");
    const int nbar = (args.size() >= 2) ? static_cast<int>(args[1].toScalar()) : 4;
    const double sll = (args.size() >= 3) ? args[2].toScalar() : -30.0;
    outs[0] = taylorwin(N, nbar, sll, ctx.engine->resource());
}

void blackmanharris_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("blackmanharris: requires (N[, sflag])",
                     0, 0, "blackmanharris", "", "numkit:blackmanharris:nargin");
    bool periodic = false; (void)parseSflag(args, periodic);
    auto *mr = ctx.engine->resource();
    outs[0] = applySflag(windowN(args[0], "blackmanharris"), periodic, [&](size_t M){ return blackmanharris(M, mr); }, mr);
}

void bohmanwin_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("bohmanwin: requires (N[, typeName])",
                     0, 0, "bohmanwin", "", "numkit:bohmanwin:nargin");
    parseTypeNameOnly(args, "bohmanwin");
    outs[0] = bohmanwin(windowN(args[0], "bohmanwin"), ctx.engine->resource());
}

void barthannwin_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("barthannwin: requires (N[, typeName])",
                     0, 0, "barthannwin", "", "numkit:barthannwin:nargin");
    parseTypeNameOnly(args, "barthannwin");
    outs[0] = barthannwin(windowN(args[0], "barthannwin"), ctx.engine->resource());
}

} // namespace detail

} // namespace numkit::signal
