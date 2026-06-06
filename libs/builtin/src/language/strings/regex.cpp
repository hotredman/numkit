// libs/builtin/src/datatypes/strings/regex.cpp
//
// regexp / regexpi / regexprep — ECMAScript regex via std::regex.

#include <numkit/builtin/library.hpp>
#include <numkit/builtin/language/strings/regex.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/core/types.hpp>

#include <cctype>
#include <regex>
#include <string>
#include <utility>
#include <vector>

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

// Named-token support. std::regex (ECMAScript) does not accept MATLAB's
// `(?<name>...)` named groups, so rewrite each one to a plain capture
// group `(...)` and record name → 1-based capture-group index. Lookbehind
// `(?<=` / `(?<!`, lookahead `(?=` / `(?!`, and non-capturing `(?:` are
// left untouched (and don't consume a capture index). Escapes and
// character classes are honoured so `\(` and `[(]` are not miscounted.
struct NamedGroups {
    std::string cleaned;
    std::vector<std::pair<std::string, std::size_t>> names;  // name → group #
};

NamedGroups extractNamedGroups(const std::string &pat)
{
    NamedGroups ng;
    ng.cleaned.reserve(pat.size());
    std::size_t group = 0;
    bool inClass = false;
    for (std::size_t i = 0; i < pat.size();) {
        const char c = pat[i];
        if (c == '\\' && i + 1 < pat.size()) {           // escaped pair
            ng.cleaned += c;
            ng.cleaned += pat[i + 1];
            i += 2;
            continue;
        }
        if (inClass) {
            if (c == ']') inClass = false;
            ng.cleaned += c;
            ++i;
            continue;
        }
        if (c == '[') { inClass = true; ng.cleaned += c; ++i; continue; }
        if (c == '(') {
            const bool named = i + 3 < pat.size() && pat[i + 1] == '?'
                               && pat[i + 2] == '<'
                               && (std::isalpha(static_cast<unsigned char>(pat[i + 3]))
                                   || pat[i + 3] == '_');
            if (named) {
                std::size_t j = i + 3;
                std::string name;
                while (j < pat.size() && pat[j] != '>') name += pat[j++];
                ++group;
                ng.names.emplace_back(name, group);
                ng.cleaned += '(';          // plain capture group
                i = (j < pat.size()) ? j + 1 : j;   // skip past '>'
                continue;
            }
            if (i + 1 < pat.size() && pat[i + 1] == '?') {   // (?: (?= (?! (?<= (?<!
                ng.cleaned += c;             // non-capturing / lookaround
                ++i;
                continue;
            }
            ++group;                         // plain capture group
            ng.cleaned += c;
            ++i;
            continue;
        }
        ng.cleaned += c;
        ++i;
    }
    return ng;
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
    // Strip MATLAB named-group syntax to a std::regex-compatible pattern,
    // keeping the name → capture-group-index map for the 'names' option.
    const NamedGroups ng = extractNamedGroups(pat.toString());
    const std::regex  re = compileRegex(ng.cleaned, ignoreCase);

    std::string opt = option;
    for (auto &c : opt)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    ScratchArena scratch(mr);

    if (opt == "names") {
        // Count matches first to choose scalar struct (1) vs struct array.
        std::size_t count = 0;
        for (auto it = std::sregex_iterator(text.begin(), text.end(), re),
                  end = std::sregex_iterator(); it != end; ++it)
            ++count;

        if (count == 0) {
            // MATLAB returns a 0×0 struct that still carries the field names.
            Value out = Value::structArray(0, 0, mr);
            for (const auto &nm : ng.names) out.setFieldAll(nm.first, Value::Empty);
            return out;
        }
        if (count == 1) {
            auto it = std::sregex_iterator(text.begin(), text.end(), re);
            const auto &m = *it;
            Value out = Value::structure(mr);
            for (const auto &nm : ng.names)
                out.setFieldAll(nm.first, Value::fromString(m.str(nm.second), mr));
            return out;
        }
        Value out = Value::structArray(1, count, mr);
        std::size_t i = 0;
        for (auto it = std::sregex_iterator(text.begin(), text.end(), re),
                  end = std::sregex_iterator(); it != end; ++it, ++i) {
            const auto &m = *it;
            for (const auto &nm : ng.names)
                out.setField(i, nm.first, Value::fromString(m.str(nm.second), mr));
        }
        return out;
    }

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

    if (opt == "end") {
        // 1-based end index of each match (= position + length).
        ScratchVec<double> idx(&scratch);
        for (auto it = std::sregex_iterator(text.begin(), text.end(), re),
                  e = std::sregex_iterator(); it != e; ++it)
            idx.push_back(static_cast<double>(it->position() + it->length()));
        return rowFromIndices(idx.data(), idx.size(), mr);
    }

    if (opt == "tokenextents") {
        // 1×N cell; each entry is a k×2 matrix of [startCol endCol] (1-based)
        // for the capture groups. With no capture groups, the whole match is
        // the single token.
        ScratchVec<std::sregex_iterator::value_type> matches(&scratch);
        for (auto it = std::sregex_iterator(text.begin(), text.end(), re),
                  e = std::sregex_iterator(); it != e; ++it)
            matches.push_back(*it);
        auto out = Value::cell(1, matches.size());
        for (std::size_t i = 0; i < matches.size(); ++i) {
            const auto &m = matches[i];
            const std::size_t ng2 = (m.size() > 1) ? m.size() - 1 : 1;
            auto te = Value::matrix(ng2, 2, ValueType::DOUBLE, mr);
            double *td = te.doubleDataMut();
            for (std::size_t g = 0; g < ng2; ++g) {
                const auto &sub = (m.size() > 1) ? m[g + 1] : m[0];
                const std::ptrdiff_t st = sub.first - text.begin();
                td[0 * ng2 + g] = static_cast<double>(st + 1);
                td[1 * ng2 + g] = static_cast<double>(st + sub.length());
            }
            out.cellAt(i) = te;
        }
        return out;
    }

    if (!opt.empty() && opt != "start")
        throw Error("regexp: unknown option '" + option
                     + "' (supported: 'start' / 'end' / 'tokenExtents' / "
                       "'match' / 'tokens' / 'names' / 'split')",
                     0, 0, "regexp", "", "numkit:regexp:badOption");

    // Default ('start'): 1-based start indices.
    ScratchVec<double> idx(&scratch);
    for (auto it = std::sregex_iterator(text.begin(), text.end(), re),
              end = std::sregex_iterator(); it != end; ++it)
        idx.push_back(static_cast<double>(it->position() + 1));
    return rowFromIndices(idx.data(), idx.size(), mr);
}

// regexp(..., 'once'): match only the FIRST occurrence and return the
// "scalarised" form of the requested output (MATLAB R2025b):
//   'start'/'end'      -> scalar index (or [] when no match)
//   'match'            -> char row     (or '' when no match)
//   'tokens'           -> 1×k cell of capture-group chars (or {} no match)
//   'tokenExtents'     -> k×2 matrix   (or [] when no match)
//   'names'            -> scalar struct (or 0×0 struct w/ fields, no match)
//   'split'            -> {prefix, remainder} split at the first match only
// Kept separate from regexpFind so the all-matches paths stay byte-for-byte
// unchanged.
Value regexpFindOnce(const Value &s, const Value &pat, const std::string &option,
                     bool ignoreCase, std::pmr::memory_resource *mr)
{
    if ((!s.isChar() && !s.isString()) || (!pat.isChar() && !pat.isString()))
        throw Error("regexp: s and pat must be strings",
                     0, 0, "regexp", "", "numkit:regexp:badArg");
    const std::string text = s.toString();
    const NamedGroups ng = extractNamedGroups(pat.toString());
    const std::regex  re = compileRegex(ng.cleaned, ignoreCase);

    std::string opt = option;
    for (auto &c : opt)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    auto it  = std::sregex_iterator(text.begin(), text.end(), re);
    auto end = std::sregex_iterator();
    const bool has = (it != end);

    if (opt == "match")
        return Value::fromString(has ? it->str() : std::string(), mr);

    if (opt == "tokens") {
        if (!has) return Value::cell(0, 0, mr);
        const auto &m = *it;
        ScratchArena scratch(mr);
        ScratchVec<std::string> grp(&scratch);
        for (std::size_t g = 1; g < m.size(); ++g) grp.emplace_back(m.str(g));
        return rowCellOfStrings(grp.data(), grp.size(), mr);
    }

    if (opt == "split") {
        ScratchArena scratch(mr);
        ScratchVec<std::string> parts(&scratch);
        if (!has) {
            parts.emplace_back(text);
        } else {
            const auto &m = *it;
            parts.emplace_back(text.substr(0, m.position()));
            parts.emplace_back(text.substr(m.position() + m.length()));
        }
        return rowCellOfStrings(parts.data(), parts.size(), mr);
    }

    if (opt == "names") {
        if (!has) {
            Value out = Value::structArray(0, 0, mr);
            for (const auto &nm : ng.names) out.setFieldAll(nm.first, Value::Empty);
            return out;
        }
        const auto &m = *it;
        Value out = Value::structure(mr);
        for (const auto &nm : ng.names)
            out.setFieldAll(nm.first, Value::fromString(m.str(nm.second), mr));
        return out;
    }

    if (opt == "tokenextents") {
        if (!has) return Value::matrix(0, 0, ValueType::DOUBLE, mr);
        const auto &m = *it;
        const std::size_t ng2 = (m.size() > 1) ? m.size() - 1 : 1;
        auto te = Value::matrix(ng2, 2, ValueType::DOUBLE, mr);
        double *td = te.doubleDataMut();
        for (std::size_t g = 0; g < ng2; ++g) {
            const auto &sub = (m.size() > 1) ? m[g + 1] : m[0];
            const std::ptrdiff_t st = sub.first - text.begin();
            td[0 * ng2 + g] = static_cast<double>(st + 1);
            td[1 * ng2 + g] = static_cast<double>(st + sub.length());
        }
        return te;
    }

    if (opt == "end") {
        if (!has) return Value::matrix(0, 0, ValueType::DOUBLE, mr);
        return Value::scalar(static_cast<double>(it->position() + it->length()), mr);
    }

    if (!opt.empty() && opt != "start")
        throw Error("regexp: unknown option '" + option
                     + "' (supported: 'start' / 'end' / 'tokenExtents' / "
                       "'match' / 'tokens' / 'names' / 'split')",
                     0, 0, "regexp", "", "numkit:regexp:badOption");

    // 'start' (default): scalar 1-based index of the first match.
    if (!has) return Value::matrix(0, 0, ValueType::DOUBLE, mr);
    return Value::scalar(static_cast<double>(it->position() + 1), mr);
}

// Apply ONE pattern/replacement to a single string. With `once`, only the
// first match is replaced (MATLAB's 'once' option).
static std::string regexrepOne(const std::string &text, const std::string &pat,
                               const std::string &rep, bool ignoreCase, bool once)
{
    const std::regex re = compileRegex(extractNamedGroups(pat).cleaned, ignoreCase);
    const auto flags = once ? std::regex_constants::format_first_only
                            : std::regex_constants::format_default;
    return std::regex_replace(text, re, rep, flags);
}

// Apply a LIST of patterns to one string, in sequence (MATLAB regexprep with
// a cell pattern array applies each pattern/replacement in turn, feeding the
// result of one into the next). A single replacement is recycled for every
// pattern; otherwise the replacement count must equal the pattern count.
static std::string regexrepSeq(const std::string &text0,
                               const ScratchVec<std::string> &pats,
                               const ScratchVec<std::string> &reps,
                               bool ignoreCase, bool once)
{
    std::string text = text0;
    for (std::size_t i = 0; i < pats.size(); ++i)
        text = regexrepOne(text, pats[i], reps.size() == 1 ? reps[0] : reps[i], ignoreCase, once);
    return text;
}

Value regexprep(const Value &s, const Value &pat, const Value &rep, bool ignoreCase, bool once, std::pmr::memory_resource *mr)
{
    auto isStrLike = [](const Value &v) { return v.isChar() || v.isString() || v.isCell(); };
    if (!isStrLike(s) || !isStrLike(pat) || !isStrLike(rep))
        throw Error("regexprep: s, pat, rep must be strings or cell arrays of strings",
                     0, 0, "regexprep", "", "numkit:regexprep:badArg");

    ScratchArena scratch(mr);
    ScratchVec<std::string> pats(&scratch), reps(&scratch);
    auto collect = [](const Value &v, ScratchVec<std::string> &out) {
        if (v.isCell()) {
            const std::size_t n = v.numel();
            for (std::size_t i = 0; i < n; ++i) out.push_back(v.cellAt(i).toString());
        } else {
            out.push_back(v.toString());
        }
    };
    collect(pat, pats);
    collect(rep, reps);
    if (reps.size() != 1 && reps.size() != pats.size())
        throw Error("regexprep: Multiple replace strings and patterns given "
                    "must have the same quantity.",
                     0, 0, "regexprep", "", "numkit:regexprep:repCount");

    // A cell string operand processes element-wise -> cell of char vectors,
    // same shape; a char/string scalar processes to a single char row (the
    // pre-existing scalar return type is preserved exactly).
    if (s.isCell()) {
        const std::size_t r = static_cast<std::size_t>(s.dims().rows());
        const std::size_t c = static_cast<std::size_t>(s.dims().cols());
        auto out = Value::cell(r, c, mr);
        const std::size_t n = s.numel();
        for (std::size_t i = 0; i < n; ++i)
            out.cellAt(i) = Value::fromString(
                regexrepSeq(s.cellAt(i).toString(), pats, reps, ignoreCase, once), mr);
        return out;
    }
    return Value::fromString(regexrepSeq(s.toString(), pats, reps, ignoreCase, once), mr);
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
