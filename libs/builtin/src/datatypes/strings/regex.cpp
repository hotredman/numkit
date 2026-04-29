// libs/builtin/src/datatypes/strings/regex.cpp
//
// regexp / regexpi / regexprep — ECMAScript regex via std::regex.

#include <numkit/builtin/library.hpp>
#include <numkit/builtin/datatypes/strings/regex.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/scratch_arena.hpp>
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
                     0, 0, "regexp", "", "m:regexp:badPattern");
    }
}

Value rowFromIndices(std::pmr::memory_resource *mr, const double *v, std::size_t n)
{
    auto out = Value::matrix(1, n, ValueType::DOUBLE, mr);
    for (std::size_t i = 0; i < n; ++i)
        out.doubleDataMut()[i] = v[i];
    return out;
}

Value rowCellOfStrings(std::pmr::memory_resource *mr, const std::string *v, std::size_t n)
{
    auto out = Value::cell(1, n);
    for (std::size_t i = 0; i < n; ++i)
        out.cellAt(i) = Value::fromString(v[i], mr);
    return out;
}

} // namespace

Value regexpFind(std::pmr::memory_resource *mr, const Value &s, const Value &pat,
                  const std::string &option, bool ignoreCase)
{
    if ((!s.isChar() && !s.isString()) || (!pat.isChar() && !pat.isString()))
        throw Error("regexp: s and pat must be strings",
                     0, 0, "regexp", "", "m:regexp:badArg");
    const std::string text = s.toString();
    const std::regex  re   = compileRegex(pat.toString(), ignoreCase);

    std::string opt = option;
    for (auto &c : opt)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    ScratchArena scratch_arena(mr);

    if (opt == "split") {
        ScratchVec<std::string> parts(&scratch_arena);
        auto begin = std::sregex_iterator(text.begin(), text.end(), re);
        auto end   = std::sregex_iterator();
        std::size_t prev = 0;
        for (auto it = begin; it != end; ++it) {
            const auto &m = *it;
            parts.emplace_back(text.substr(prev, m.position() - prev));
            prev = m.position() + m.length();
        }
        parts.emplace_back(text.substr(prev));
        return rowCellOfStrings(mr, parts.data(), parts.size());
    }

    if (opt == "match") {
        ScratchVec<std::string> matches(&scratch_arena);
        for (auto it = std::sregex_iterator(text.begin(), text.end(), re),
                  end = std::sregex_iterator(); it != end; ++it)
            matches.emplace_back(it->str());
        return rowCellOfStrings(mr, matches.data(), matches.size());
    }

    if (opt == "tokens") {
        // 1×N cell, each entry is a 1×k cell of capture group strings.
        // Outer + inner both ScratchVec — uses-allocator construction
        // propagates the arena to the inner pmr-vectors automatically.
        ScratchVec<ScratchVec<std::string>> all(&scratch_arena);
        for (auto it = std::sregex_iterator(text.begin(), text.end(), re),
                  end = std::sregex_iterator(); it != end; ++it) {
            ScratchVec<std::string> grp(&scratch_arena);
            for (std::size_t g = 1; g < it->size(); ++g)
                grp.emplace_back(it->str(g));
            all.push_back(std::move(grp));
        }
        auto out = Value::cell(1, all.size());
        for (std::size_t i = 0; i < all.size(); ++i)
            out.cellAt(i) = rowCellOfStrings(mr, all[i].data(), all[i].size());
        return out;
    }

    if (!opt.empty())
        throw Error("regexp: unknown option '" + option
                     + "' (supported: 'match' / 'tokens' / 'split')",
                     0, 0, "regexp", "", "m:regexp:badOption");

    // Default: 1-based start indices.
    ScratchVec<double> idx(&scratch_arena);
    for (auto it = std::sregex_iterator(text.begin(), text.end(), re),
              end = std::sregex_iterator(); it != end; ++it)
        idx.push_back(static_cast<double>(it->position() + 1));
    return rowFromIndices(mr, idx.data(), idx.size());
}

Value regexprep(std::pmr::memory_resource *mr, const Value &s, const Value &pat,
                 const Value &rep, bool ignoreCase)
{
    if ((!s.isChar() && !s.isString())
        || (!pat.isChar() && !pat.isString())
        || (!rep.isChar() && !rep.isString()))
        throw Error("regexprep: s, pat, rep must be strings",
                     0, 0, "regexprep", "", "m:regexprep:badArg");
    const std::string text    = s.toString();
    const std::regex  re      = compileRegex(pat.toString(), ignoreCase);
    const std::string repText = rep.toString();
    const std::string out     = std::regex_replace(text, re, repText);
    return Value::fromString(out, mr);
}

// ════════════════════════════════════════════════════════════════════════
// Adapters
// ════════════════════════════════════════════════════════════════════════

namespace detail {

void regexp_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("regexp: requires at least 2 arguments (s, pat)",
                     0, 0, "regexp", "", "m:regexp:nargin");
    std::string opt;
    if (args.size() >= 3) {
        if (!args[2].isChar() && !args[2].isString())
            throw Error("regexp: option must be a string",
                         0, 0, "regexp", "", "m:regexp:badOption");
        opt = args[2].toString();
    }
    outs[0] = regexpFind(ctx.engine->resource(), args[0], args[1], opt, false);
}

void regexpi_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("regexpi: requires at least 2 arguments (s, pat)",
                     0, 0, "regexpi", "", "m:regexpi:nargin");
    std::string opt;
    if (args.size() >= 3) {
        if (!args[2].isChar() && !args[2].isString())
            throw Error("regexpi: option must be a string",
                         0, 0, "regexpi", "", "m:regexpi:badOption");
        opt = args[2].toString();
    }
    outs[0] = regexpFind(ctx.engine->resource(), args[0], args[1], opt, true);
}

void regexprep_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("regexprep: requires 3 arguments (s, pat, rep)",
                     0, 0, "regexprep", "", "m:regexprep:nargin");
    outs[0] = regexprep(ctx.engine->resource(), args[0], args[1], args[2], false);
}

} // namespace detail

} // namespace numkit::builtin
