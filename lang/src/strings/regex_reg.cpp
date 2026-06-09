// toolboxes/signal/src/language/strings/regex_reg.cpp
//
// CallContext register half of language/strings/regex.cpp (Phase 2b compute/register split).
// Engine-coupled glue: marshals CallContext args/outs into the engine-free
// compute API declared in the headers below. See project_layering_refactor.
#include <numkit/core/engine.hpp>
#include <numkit/builtin/language/strings/regex.hpp>
#include <numkit/builtin/library.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/value.hpp>
#include "regex_detail.hpp"
#include <numkit/value/error.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/span.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <complex>
#include <cstddef>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace numkit::builtin {

namespace detail {

// The default positional output order of regexp/regexpi when no option
// string is given: [start, end, tokenExtents, match, tokens, names, split].
inline void regexpDispatch(Span<const Value> args, size_t nargout,
                           Span<Value> outs, bool ignoreCase, const char *fn,
                           CallContext &ctx)
{
    if (args.size() < 2)
        throw Error(std::string(fn) + ": requires at least 2 arguments (s, pat)",
                     0, 0, fn, "", std::string("numkit:") + fn + ":nargin");
    auto *mr = ctx.engine->resource();
    // Trailing option strings. 'once' is a modifier (match first occurrence,
    // return the scalarised form); the first non-'once' string selects the
    // output (as before). Additional output selectors are ignored, matching
    // the pre-existing single-option behaviour.
    std::string opt;
    bool once = false;
    for (size_t i = 2; i < args.size(); ++i) {
        if (!args[i].isChar() && !args[i].isString())
            throw Error(std::string(fn) + ": option must be a string",
                         0, 0, fn, "", std::string("numkit:") + fn + ":badOption");
        std::string o = args[i].toString();
        std::string lo = o;
        for (auto &c : lo) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (lo == "once") { once = true; continue; }
        if (opt.empty()) opt = o;
    }
    if (once) {
        // 'once' forces a single scalarised output (opt may be "" → 'start').
        outs[0] = regexpFindOnce(args[0], args[1], opt, ignoreCase, mr);
        return;
    }
    if (!opt.empty() || nargout <= 1) {
        outs[0] = regexpFind(args[0], args[1], opt, ignoreCase, mr);
        return;
    }
    static const char *order[] = {"start", "end", "tokenExtents",
                                  "match", "tokens", "names", "split"};
    const size_t k = std::min<size_t>(nargout, 7);
    for (size_t i = 0; i < k; ++i)
        outs[i] = regexpFind(args[0], args[1], order[i], ignoreCase, mr);
}

void regexp_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    regexpDispatch(args, nargout, outs, /*ignoreCase=*/false, "regexp", ctx);
}

void regexpi_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    regexpDispatch(args, nargout, outs, /*ignoreCase=*/true, "regexpi", ctx);
}

void regexprep_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("regexprep: requires 3 arguments (s, pat, rep)",
                     0, 0, "regexprep", "", "numkit:regexprep:nargin");
    // Trailing option strings: recognise 'ignorecase' and 'once' (other
    // documented options — 'preservecase', 'lineanchors', … — are not yet
    // parsed and are left to their default behaviour).
    bool ignoreCase = false;
    bool once = false;
    for (size_t i = 3; i < args.size(); ++i) {
        if (args[i].isChar() || args[i].isString()) {
            std::string o = args[i].toString();
            for (auto &ch : o) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
            if      (o == "ignorecase") ignoreCase = true;
            else if (o == "once")       once = true;
        }
    }
    outs[0] = regexprep(args[0], args[1], args[2], ignoreCase, once, ctx.engine->resource());
}

void regexptranslate_reg(Span<const Value> args, size_t, Span<Value> outs,
                         CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("regexptranslate: requires 2 arguments (op, str)",
                     0, 0, "regexptranslate", "", "numkit:regexptranslate:nargin");
    if (!args[0].isChar() && !args[0].isString())
        throw Error("regexptranslate: op must be a char or string",
                     0, 0, "regexptranslate", "", "numkit:regexptranslate:badOp");
    if (!args[1].isChar() && !args[1].isString())
        throw Error("regexptranslate: str must be a char or string",
                     0, 0, "regexptranslate", "", "numkit:regexptranslate:badStr");
    outs[0] = regexptranslate(args[0].toString(), args[1].toString(), ctx.engine->resource());
}

} // namespace detail

} // namespace numkit::builtin
