// runtime/include/numkit/runtime/function_file.hpp
//
// MATLAB `run`/file-execution semantics for FUNCTION files
// (bugs/opened/lang/run-invokes-nullary-function-file.md): a file whose
// first code construct is a function definition EXECUTES that function
// when run — a nullary function runs to completion; a function with
// required inputs is still CALLED and fails with the natural
// "not enough input arguments" (exactly what MATLAB R2025b does —
// verified on the fieldtest corpus: extract_firms_data.m).
//
// Header-only: shared by the `run` builtin (runtime) and the native CLI
// (apps/numkit); the WASM CLI mirrors the same scan in JS.

#pragma once

#include <cctype>
#include <string>

namespace numkit::runtime {

// Returns true and fills `name` with the PRIMARY function's identifier if
// `src` is a function file (first non-comment, non-blank construct is a
// function definition). Returns false for scripts.
inline bool primaryFunctionOfFile(const std::string &src, std::string &name)
{
    std::string line;
    bool haveFirst = false;
    size_t i = 0;
    const size_t n = src.size();
    auto trim = [](std::string &s) {
        size_t a = 0, b = s.size();
        while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
        while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
        s = s.substr(a, b - a);
    };
    while (i < n && !haveFirst) {
        // read one physical line
        line.clear();
        while (i < n && src[i] != '\n') line += src[i++];
        if (i < n) ++i;
        while (!line.empty() && (line.back() == '\r')) line.pop_back();
        // strip a trailing comment (naive: first '%' — function signature
        // lines do not carry literal % in strings in practice)
        size_t pct = line.find('%');
        if (pct != std::string::npos) line.resize(pct);
        trim(line);
        if (line.empty()) continue;
        if (line.rfind("function", 0) == 0
            && (line.size() == 8 || std::isspace(static_cast<unsigned char>(line[8])))) {
            haveFirst = true;
            // join continuations ("...") of the signature line
            while (line.size() >= 3 && line.compare(line.size() - 3, 3, "...") == 0 && i < n) {
                line.resize(line.size() - 3);
                std::string more;
                while (i < n && src[i] != '\n') more += src[i++];
                if (i < n) ++i;
                size_t p = more.find('%');
                if (p != std::string::npos) more.resize(p);
                trim(more);
                line += more;
            }
        } else {
            return false; // first code construct is not a function → script
        }
    }
    if (!haveFirst)
        return false;

    // Parse the signature:  function [out] = name(args)   |   function name(args)
    size_t eq = line.find('=');
    size_t namePos = std::string::npos;
    if (eq != std::string::npos) {
        // ensure the '=' is before any '(' (output-list form)
        size_t lp = line.find('(');
        if (lp != std::string::npos && lp < eq) eq = std::string::npos;
    }
    if (eq != std::string::npos) {
        namePos = eq + 1;
    } else {
        namePos = 8; // past "function"
    }
    while (namePos < line.size() && std::isspace(static_cast<unsigned char>(line[namePos])))
        ++namePos;
    if (namePos >= line.size()) return false;
    if (!std::isalpha(static_cast<unsigned char>(line[namePos])) && line[namePos] != '_')
        return false;
    size_t end = namePos;
    while (end < line.size()
           && (std::isalnum(static_cast<unsigned char>(line[end])) || line[end] == '_'))
        ++end;
    name = line.substr(namePos, end - namePos);
    return !name.empty();
}

} // namespace numkit::runtime
