// toolboxes/stats/src/distributions/dist_dispatch_reg.cpp
//
// Generic distribution dispatchers: cdf / pdf / icdf / random. Each takes a
// distribution NAME as the first argument and forwards the remaining arguments
// to the matching per-family builtin (normcdf / poisspdf / norminv / normrnd …)
// — these already exist and are parity-validated, so this is pure register-level
// glue (a name → family-prefix table + a findExternal forward). MATLAB:
//   cdf('Normal', x, mu, sigma)  ==  normcdf(x, mu, sigma)
//   icdf(...)  -> <fam>inv ,  random(...) -> <fam>rnd .
// bugs/stats/distribution-dispatchers.
#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/value.hpp>
#include <numkit/value/span.hpp>

#include <algorithm>
#include <cctype>
#include <string>
#include <unordered_map>

namespace numkit::stats {

namespace detail {

namespace {

// MATLAB distribution name (lower-cased) → numkit family prefix. Covers every
// family that ships <prefix>{cdf,pdf,inv,rnd}; canonical names + common aliases.
const std::unordered_map<std::string, std::string> &distPrefixMap()
{
    static const std::unordered_map<std::string, std::string> m = {
        {"normal", "norm"}, {"norm", "norm"}, {"gaussian", "norm"},
        {"poisson", "poiss"}, {"poiss", "poiss"},
        {"binomial", "bino"}, {"bino", "bino"},
        {"exponential", "exp"}, {"exp", "exp"},
        {"gamma", "gam"}, {"gam", "gam"},
        {"beta", "beta"},
        {"uniform", "unif"}, {"unif", "unif"},
        {"discrete uniform", "unid"}, {"unid", "unid"},
        {"geometric", "geo"}, {"geo", "geo"},
        {"negative binomial", "nbin"}, {"nbin", "nbin"},
        {"hypergeometric", "hyge"}, {"hyge", "hyge"},
        {"weibull", "wbl"}, {"wbl", "wbl"},
        {"lognormal", "logn"}, {"logn", "logn"},
        {"extreme value", "ev"}, {"ev", "ev"},
        {"generalized extreme value", "gev"}, {"gev", "gev"},
        {"generalized pareto", "gp"}, {"gp", "gp"},
        {"rayleigh", "rayl"}, {"rayl", "rayl"},
        {"nakagami", "naka"}, {"naka", "naka"},
        {"rician", "rice"}, {"rice", "rice"},
        {"chisquare", "chi2"}, {"chi-square", "chi2"}, {"chi2", "chi2"},
        {"t", "t"}, {"students t", "t"},
        {"f", "f"},
        {"noncentral chi-square", "ncx2"}, {"ncx2", "ncx2"},
    };
    return m;
}

// Resolve the distribution name in args[0], forward args[1..] to the family
// builtin <prefix><suffix> (cdf/pdf/inv/rnd) found via the engine.
void dispatchDist(const char *fnName, const char *suffix,
                  Span<const Value> args, size_t nargout, Span<Value> outs,
                  CallContext &ctx)
{
    const std::string fn = fnName;
    if (args.empty() || !args[0].isChar())
        throw Error(fn + ": first argument must be a distribution name",
                    0, 0, fnName, "", "numkit:" + fn + ":name");

    std::string name = args[0].toString();
    std::transform(name.begin(), name.end(), name.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    const auto &m = distPrefixMap();
    auto it = m.find(name);
    if (it == m.end())
        throw Error(fn + ": unsupported distribution '" + args[0].toString() + "'",
                    0, 0, fnName, "", "numkit:" + fn + ":dist");

    const std::string target = "compat." + it->second + suffix;
    const ExternalFunc *target_fn = ctx.engine->findExternal(target, ctx.env);
    if (!target_fn)
        throw Error(fn + ": '" + it->second + suffix + "' is not available",
                    0, 0, fnName, "", "numkit:" + fn + ":missing");

    (*target_fn)(args.subspan(1), nargout, outs, ctx);
}

} // namespace

void cdf_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{ dispatchDist("cdf", "cdf", args, nargout, outs, ctx); }

void pdf_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{ dispatchDist("pdf", "pdf", args, nargout, outs, ctx); }

void icdf_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{ dispatchDist("icdf", "inv", args, nargout, outs, ctx); }

void random_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{ dispatchDist("random", "rnd", args, nargout, outs, ctx); }

} // namespace detail

} // namespace numkit::stats
