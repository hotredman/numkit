// libs/builtin/src/datatypes/strings/regex.cpp
//
// regexp / regexpi / regexprep — ECMAScript regex via std::regex.

#include <numkit/m/builtin/MStdLibrary.hpp>
#include <numkit/m/builtin/datatypes/strings/regex.hpp>

#include <numkit/m/core/MEngine.hpp>
#include <numkit/m/core/MTypes.hpp>

#include <cctype>
#include <regex>
#include <string>
#include <vector>

namespace numkit::m::builtin {

namespace {

std::regex compileRegex(const std::string &pat, bool ignoreCase)
{
    auto flags = std::regex::ECMAScript;
    if (ignoreCase) flags |= std::regex::icase;
    try {
        return std::regex(pat, flags);
    } catch (const std::regex_error &e) {
        throw MError(std::string("regex: invalid pattern — ") + e.what(),
                     0, 0, "regexp", "", "m:regexp:badPattern");
    }
}

MValue rowFromIndices(Allocator &alloc, const std::vector<double> &v)
{
    auto out = MValue::matrix(1, v.size(), MType::DOUBLE, &alloc);
    for (std::size_t i = 0; i < v.size(); ++i)
        out.doubleDataMut()[i] = v[i];
    return out;
}

MValue rowCellOfStrings(Allocator &alloc, const std::vector<std::string> &v)
{
    auto out = MValue::cell(1, v.size());
    for (std::size_t i = 0; i < v.size(); ++i)
        out.cellAt(i) = MValue::fromString(v[i], &alloc);
    return out;
}

} // namespace

MValue regexpFind(Allocator &alloc, const MValue &s, const MValue &pat,
                  const std::string &option, bool ignoreCase)
{
    if ((!s.isChar() && !s.isString()) || (!pat.isChar() && !pat.isString()))
        throw MError("regexp: s and pat must be strings",
                     0, 0, "regexp", "", "m:regexp:badArg");
    const std::string text = s.toString();
    const std::regex  re   = compileRegex(pat.toString(), ignoreCase);

    std::string opt = option;
    for (auto &c : opt)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    if (opt == "split") {
        std::vector<std::string> parts;
        auto begin = std::sregex_iterator(text.begin(), text.end(), re);
        auto end   = std::sregex_iterator();
        std::size_t prev = 0;
        for (auto it = begin; it != end; ++it) {
            const auto &m = *it;
            parts.emplace_back(text.substr(prev, m.position() - prev));
            prev = m.position() + m.length();
        }
        parts.emplace_back(text.substr(prev));
        return rowCellOfStrings(alloc, parts);
    }

    if (opt == "match") {
        std::vector<std::string> matches;
        for (auto it = std::sregex_iterator(text.begin(), text.end(), re),
                  end = std::sregex_iterator(); it != end; ++it)
            matches.emplace_back(it->str());
        return rowCellOfStrings(alloc, matches);
    }

    if (opt == "tokens") {
        // 1×N cell, each entry is a 1×k cell of capture group strings.
        std::vector<std::vector<std::string>> all;
        for (auto it = std::sregex_iterator(text.begin(), text.end(), re),
                  end = std::sregex_iterator(); it != end; ++it) {
            std::vector<std::string> grp;
            for (std::size_t g = 1; g < it->size(); ++g)
                grp.emplace_back(it->str(g));
            all.push_back(std::move(grp));
        }
        auto out = MValue::cell(1, all.size());
        for (std::size_t i = 0; i < all.size(); ++i)
            out.cellAt(i) = rowCellOfStrings(alloc, all[i]);
        return out;
    }

    if (!opt.empty())
        throw MError("regexp: unknown option '" + option
                     + "' (supported: 'match' / 'tokens' / 'split')",
                     0, 0, "regexp", "", "m:regexp:badOption");

    // Default: 1-based start indices.
    std::vector<double> idx;
    for (auto it = std::sregex_iterator(text.begin(), text.end(), re),
              end = std::sregex_iterator(); it != end; ++it)
        idx.push_back(static_cast<double>(it->position() + 1));
    return rowFromIndices(alloc, idx);
}

MValue regexprep(Allocator &alloc, const MValue &s, const MValue &pat,
                 const MValue &rep, bool ignoreCase)
{
    if ((!s.isChar() && !s.isString())
        || (!pat.isChar() && !pat.isString())
        || (!rep.isChar() && !rep.isString()))
        throw MError("regexprep: s, pat, rep must be strings",
                     0, 0, "regexprep", "", "m:regexprep:badArg");
    const std::string text    = s.toString();
    const std::regex  re      = compileRegex(pat.toString(), ignoreCase);
    const std::string repText = rep.toString();
    const std::string out     = std::regex_replace(text, re, repText);
    return MValue::fromString(out, &alloc);
}

// ════════════════════════════════════════════════════════════════════════
// Adapters
// ════════════════════════════════════════════════════════════════════════

namespace detail {

void regexp_reg(Span<const MValue> args, size_t, Span<MValue> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw MError("regexp: requires at least 2 arguments (s, pat)",
                     0, 0, "regexp", "", "m:regexp:nargin");
    std::string opt;
    if (args.size() >= 3) {
        if (!args[2].isChar() && !args[2].isString())
            throw MError("regexp: option must be a string",
                         0, 0, "regexp", "", "m:regexp:badOption");
        opt = args[2].toString();
    }
    outs[0] = regexpFind(ctx.engine->allocator(), args[0], args[1], opt, false);
}

void regexpi_reg(Span<const MValue> args, size_t, Span<MValue> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw MError("regexpi: requires at least 2 arguments (s, pat)",
                     0, 0, "regexpi", "", "m:regexpi:nargin");
    std::string opt;
    if (args.size() >= 3) {
        if (!args[2].isChar() && !args[2].isString())
            throw MError("regexpi: option must be a string",
                         0, 0, "regexpi", "", "m:regexpi:badOption");
        opt = args[2].toString();
    }
    outs[0] = regexpFind(ctx.engine->allocator(), args[0], args[1], opt, true);
}

void regexprep_reg(Span<const MValue> args, size_t, Span<MValue> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw MError("regexprep: requires 3 arguments (s, pat, rep)",
                     0, 0, "regexprep", "", "m:regexprep:nargin");
    outs[0] = regexprep(ctx.engine->allocator(), args[0], args[1], args[2], false);
}

} // namespace detail

} // namespace numkit::m::builtin
