// libs/wavelet/src/shape/gauss_reg.cpp
//
// Register half of the Gaussian wavelets: the CallContext builtins
// gauswavf / cgauwavf that delegate to the engine-free compute in
// gauss.cpp. parseGaussOrder lives here because it is a register-side
// parser (accepts an int order or a 'gausN'/'cgauN' wname string).
// library.cpp forward-declares + registers these by name.
//
// Phase 2b compute/register split — see project_layering_refactor memory.

#include <numkit/wavelet/shape/shape.hpp>

#include <numkit/core/engine.hpp>   // CallContext, ctx.engine->resource()
#include <numkit/value/error.hpp>

#include <cctype>
#include <stdexcept>
#include <string>
#include <utility>

namespace numkit::wavelet {
namespace detail {

// Parse the optional `p` argument: either an integer (1..8) or a string
// of the form `'gausN'` (real form) / `'cgauN'` (complex form). The
// `prefix` is the expected wname stem.
static int parseGaussOrder(const Value &arg, const char *prefix, const char *fn)
{
    if (arg.isChar() || arg.isString()) {
        std::string s = arg.toString();
        // Lowercase the prefix portion for comparison.
        for (auto &c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        const size_t plen = std::char_traits<char>::length(prefix);
        if (s.size() <= plen || s.compare(0, plen, prefix) != 0)
            throw Error(std::string(fn) + ": bad wname '" + s +
                        "' (expected " + prefix + "N)",
                         0, 0, fn, "", "numkit:wname");
        try { return std::stoi(s.substr(plen)); }
        catch (...) {
            throw Error(std::string(fn) + ": bad wname '" + s +
                        "' (cannot parse order N)",
                         0, 0, fn, "", "numkit:wname");
        }
    }
    return static_cast<int>(arg.toScalar());
}

void gauswavf_reg(Span<const Value> args, size_t nargout,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("gauswavf: requires (LB, UB, N[, p|'gausN'])",
                    0, 0, "gauswavf", "", "numkit:gauswavf:nargin");
    const double lb = args[0].toScalar();
    const double ub = args[1].toScalar();
    const size_t N  = static_cast<size_t>(args[2].toScalar());
    int p = 1;
    if (args.size() >= 4) p = parseGaussOrder(args[3], "gaus", "gauswavf");
    auto [psi, x] = gauswavf(lb, ub, N, p, ctx.engine->resource());
    outs[0] = std::move(psi);
    if (nargout > 1) outs[1] = std::move(x);
}

void cgauwavf_reg(Span<const Value> args, size_t nargout,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("cgauwavf: requires (LB, UB, N[, p|'cgauN'])",
                    0, 0, "cgauwavf", "", "numkit:cgauwavf:nargin");
    const double lb = args[0].toScalar();
    const double ub = args[1].toScalar();
    const size_t N  = static_cast<size_t>(args[2].toScalar());
    int p = 1;
    if (args.size() >= 4) p = parseGaussOrder(args[3], "cgau", "cgauwavf");
    auto [psi, x] = cgauwavf(lb, ub, N, p, ctx.engine->resource());
    outs[0] = std::move(psi);
    if (nargout > 1) outs[1] = std::move(x);
}

} // namespace detail
} // namespace numkit::wavelet
