// toolboxes/.../regex_detail.hpp — private compute/register substrate (anon-in-
// header, internal linkage per TU) shared by regex.cpp + regex_reg.cpp.
// Phase 2b compute/register split — see project_layering_refactor memory.
#pragma once

#include <numkit/value/value.hpp>

#include <regex>
#include <numkit/value/error.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/span.hpp>
#include "reduction_helpers.hpp"  // engine-free numkit::builtin::detail dim-infra (ops re-export)

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory_resource>
#include <numeric>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace numkit::lang {

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

// regexp single-match worker (def in regex.cpp, external).
Value regexpFindOnce(const Value &s, const Value &pat, const std::string &option,
                     bool ignoreCase, std::pmr::memory_resource *mr);

} // namespace numkit::lang
