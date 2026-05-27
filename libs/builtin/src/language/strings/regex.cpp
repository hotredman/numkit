// libs/builtin/src/datatypes/strings/regex.cpp
//
// regexp / regexpi / regexprep — ECMAScript regex via std::regex.

#include <numkit/builtin/library.hpp>
#include <numkit/builtin/language/strings/regex.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/scratch.hpp>
#include <numkit/core/types.hpp>

#include <cctype>
#include <regex>
#include <string>

namespace numkit::builtin {

namespace {

std::regex compileRegex(const std::string &pat, bool ignoreCase)
{
    auto flags = std::regex::ECMAScript;
    if (ignoreCase) flags |= std::regex::icase;
    try {
        return std::regex(pat, flags);
    } catch (const std::regex_error &e) {
        throw Error(std::string("regex: invalid pattern — ") + e.what(),
                     0, 0, "regexp", "", "numkit:regexp:badPattern");
    }
}

Value rowFromIndices(const double *v, std::size_t n, std::pmr::memory_resource *mr)
{
    auto out = Value::matrix(1, n, ValueType::DOUBLE, mr);
    for (std::size_t i = 0; i < n; ++i)
        out.doubleDataMut()[i] = v[i];
    return out;
}

Value rowCellOfStrings(const std::string *v, std::size_t n, std::pmr::memory_resource *mr)
{
    auto out = Value::cell(1, n);
    for (std::size_t i = 0; i < n; ++i)
        out.cellAt(i) = Value::fromString(v[i], mr);
    return out;
}

} // namespace

Value regexpFind(const Value &s, const Value &pat, const std::string &option, bool ignoreCase, std::pmr::memory_resource *mr)
{
    if ((!s.isChar() && !s.isString()) || (!pat.isChar() && !pat.isString()))
        throw Error("regexp: s and pat must be strings",
                     0, 0, "regexp", "", "numkit:regexp:badArg");
    const std::string text = s.toString();
    const std::regex  re   = compileRegex(pat.toString(), ignoreCase);

    std::string opt = option;
    for (auto &c : opt)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    ScratchArena scratch(mr);

    if (opt == "split") {
        ScratchVec<std::string> parts(&scratch);
        auto begin = std::sregex_iterator(text.begin(), text.end(), re);
        auto end   = std::sregex_iterator();
        std::size_t prev = 0;
        for (auto it = begin; it != end; ++it) {
            const auto &m = *it;
            parts.emplace_back(text.substr(prev, m.position() - prev));
            prev = m.position() + m.length();
        }
        parts.emplace_back(text.substr(prev));
        return rowCellOfStrings(parts.data(), parts.size(), mr);
    }

    if (opt == "match") {
        ScratchVec<std::string> matches(&scratch);
        for (auto it = std::sregex_iterator(text.begin(), text.end(), re),
                  end = std::sregex_iterator(); it != end; ++it)
            matches.emplace_back(it->str());
        return rowCellOfStrings(matches.data(), matches.size(), mr);
    }

    if (opt == "tokens") {
        // 1×N cell, each entry is a 1×k cell of capture group strings.
        // Outer + inner both ScratchVec — uses-allocator construction
        // propagates the arena to the inner pmr-vectors automatically.
        ScratchVec<ScratchVec<std::string>> all(&scratch);
        for (auto it = std::sregex_iterator(text.begin(), text.end(), re),
                  end = std::sregex_iterator(); it != end; ++it) {
            ScratchVec<std::string> grp(&scratch);
            for (std::size_t g = 1; g < it->size(); ++g)
                grp.emplace_back(it->str(g));
            all.push_back(std::move(grp));
        }
        auto out = Value::cell(1, all.size());
        for (std::size_t i = 0; i < all.size(); ++i)
            out.cellAt(i) = rowCellOfStrings(all[i].data(), all[i].size(), mr);
        return out;
    }

    if (!opt.empty())
        throw Error("regexp: unknown option '" + option
                     + "' (supported: 'match' / 'tokens' / 'split')",
                     0, 0, "regexp", "", "numkit:regexp:badOption");

    // Default: 1-based start indices.
    ScratchVec<double> idx(&scratch);
    for (auto it = std::sregex_iterator(text.begin(), text.end(), re),
              end = std::sregex_iterator(); it != end; ++it)
        idx.push_back(static_cast<double>(it->position() + 1));
    return rowFromIndices(idx.data(), idx.size(), mr);
}

Value regexprep(const Value &s, const Value &pat, const Value &rep, bool ignoreCase, std::pmr::memory_resource *mr)
{
    if ((!s.isChar() && !s.isString())
        || (!pat.isChar() && !pat.isString())
        || (!rep.isChar() && !rep.isString()))
        throw Error("regexprep: s, pat, rep must be strings",
                     0, 0, "regexprep", "", "numkit:regexprep:badArg");
    const std::string text    = s.toString();
    const std::regex  re      = compileRegex(pat.toString(), ignoreCase);
    const std::string repText = rep.toString();
    const std::string out     = std::regex_replace(text, re, repText);
    return Value::fromString(out, mr);
}

// ── Pack 36: regexptranslate ─────────────────────────────────────────
Value regexptranslate(const std::string &op, const std::string &s, std::pmr::memory_resource *mr)
{
    // Characters that carry special meaning in ECMAScript regex syntax —
    // matches the set MATLAB's regexptranslate('escape', …) prepends `\`
    // to.
    auto isMeta = [](char c) {
        switch (c) {
        case '.': case '*': case '+': case '?': case '|':
        case '(': case ')': case '[': case ']': case '{': case '}':
        case '^': case '$': case '\\':
            return true;
        default:
            return false;
        }
    };

    std::string out;
    out.reserve(s.size() * 2);

    if (op == "escape") {
        for (char c : s) {
            if (isMeta(c)) out += '\\';
            out += c;
        }
        return Value::fromString(out, mr);
    }
    if (op == "wildcard") {
        // MATLAB rules: `*` → `.*`, `?` → `.`, every other regex meta is
        // escaped. Non-meta non-wildcard chars pass through.
        for (char c : s) {
            if (c == '*')      out += ".*";
            else if (c == '?') out += '.';
            else {
                if (isMeta(c)) out += '\\';
                out += c;
            }
        }
        return Value::fromString(out, mr);
    }
    throw Error("regexptranslate: unsupported op '" + op + "' "
                "(supported: 'escape', 'wildcard')",
                 0, 0, "regexptranslate", "", "numkit:regexptranslate:badOp");
}

// ════════════════════════════════════════════════════════════════════════
// Adapters
// ════════════════════════════════════════════════════════════════════════

namespace detail {

void regexp_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("regexp: requires at least 2 arguments (s, pat)",
                     0, 0, "regexp", "", "numkit:regexp:nargin");
    std::string opt;
    if (args.size() >= 3) {
        if (!args[2].isChar() && !args[2].isString())
            throw Error("regexp: option must be a string",
                         0, 0, "regexp", "", "numkit:regexp:badOption");
        opt = args[2].toString();
    }
    outs[0] = regexpFind(args[0], args[1], opt, false, ctx.engine->resource());
}

void regexpi_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("regexpi: requires at least 2 arguments (s, pat)",
                     0, 0, "regexpi", "", "numkit:regexpi:nargin");
    std::string opt;
    if (args.size() >= 3) {
        if (!args[2].isChar() && !args[2].isString())
            throw Error("regexpi: option must be a string",
                         0, 0, "regexpi", "", "numkit:regexpi:badOption");
        opt = args[2].toString();
    }
    outs[0] = regexpFind(args[0], args[1], opt, true, ctx.engine->resource());
}

void regexprep_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("regexprep: requires 3 arguments (s, pat, rep)",
                     0, 0, "regexprep", "", "numkit:regexprep:nargin");
    outs[0] = regexprep(args[0], args[1], args[2], false, ctx.engine->resource());
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
