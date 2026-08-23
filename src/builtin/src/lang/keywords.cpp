// src/builtin/src/lang/keywords.cpp
//
// Reserved keywords and identifier predicates for numkit::builtin.

#include <numkit/builtin/lang.hpp>
#include <numkit/value/value.hpp>
#include <numkit/value/span.hpp>

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <string>
#include <vector>

namespace numkit::builtin {

const std::vector<std::string> &keywords()
{
    static const std::vector<std::string> kw = {
        "break", "case", "catch", "classdef", "continue", "else",
        "elseif", "end", "for", "function", "global", "if",
        "otherwise", "parfor", "persistent", "return", "spmd",
        "switch", "try", "while"
    };
    return kw;
}

bool iskeyword(const std::string &name)
{
    const auto &kw = keywords();
    return std::find(kw.begin(), kw.end(), name) != kw.end();
}

Value iskeyword(Span<const Value> args, std::pmr::memory_resource *mr)
{
    const auto &kw = keywords();
    if (args.empty()) {
        auto c = Value::cell(kw.size(), 1, mr);
        for (size_t i = 0; i < kw.size(); ++i)
            c.cellAt(i) = Value::fromString(kw[i], mr);
        return c;
    }
    const std::string s = args[0].toString();
    return Value::logicalScalar(iskeyword(s), mr);
}

bool isvarname(const std::string &s)
{
    if (s.empty()) return false;
    if (std::isalpha(static_cast<unsigned char>(s[0])) == 0) return false;
    for (size_t i = 1; i < s.size(); ++i) {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        if (!(std::isalnum(c) || c == '_')) return false;
    }
    return !iskeyword(s);
}

Value isvarname(const Value &a, std::pmr::memory_resource *mr)
{
    const bool isText = a.isChar() || (a.isString() && a.numel() == 1);
    if (!isText) return Value::logicalScalar(false, mr);
    return Value::logicalScalar(isvarname(a.toString()), mr);
}

Value isvarname(Span<const Value> args, std::pmr::memory_resource *mr)
{
    if (args.empty())
        throw std::runtime_error("isvarname requires 1 argument");
    return isvarname(args[0], mr);
}

} // namespace numkit::builtin
