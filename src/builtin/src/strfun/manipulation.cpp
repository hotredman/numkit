// src/builtin/src/strfun/manipulation.cpp
//
// String comparison, case conversion, trimming, joining, splitting, and search for numkit::builtin.

#include <numkit/builtin/strfun.hpp>
#include <numkit/value/value.hpp>
#include <numkit/value/span.hpp>
#include <numkit/lang/strings/strings.hpp>

namespace numkit::builtin {

Value strcmp(const Value &s1, const Value &s2, std::pmr::memory_resource *mr) { return numkit::lang::strcmp(s1, s2, mr); }
Value strncmp(const Value &s1, const Value &s2, size_t n, std::pmr::memory_resource *mr) { return numkit::lang::strncmp(s1, s2, n, mr); }
Value strcmpi(const Value &s1, const Value &s2, std::pmr::memory_resource *mr) { return numkit::lang::strcmpi(s1, s2, mr); }
Value strncmpi(const Value &s1, const Value &s2, size_t n, std::pmr::memory_resource *mr) { return numkit::lang::strncmpi(s1, s2, n, mr); }
Value matches(const Value &str, const Value &pat, std::pmr::memory_resource *mr) { return numkit::lang::matches(str, pat, mr); }

Value upper(const Value &s, std::pmr::memory_resource *mr) { return numkit::lang::upper(s, mr); }
Value lower(const Value &s, std::pmr::memory_resource *mr) { return numkit::lang::lower(s, mr); }
Value strtrim(const Value &s, std::pmr::memory_resource *mr) { return numkit::lang::strtrim(s, mr); }
Value deblank(const Value &s, std::pmr::memory_resource *mr) { return numkit::lang::deblank(s, mr); }
Value strip(const Value &s, std::pmr::memory_resource *mr) { return numkit::lang::strip(s, Value::Empty, Value::Empty, mr); }

Value strcat(Span<const Value> args, std::pmr::memory_resource *mr) { return numkit::lang::strcat(args, mr); }
Value strjoin(const Value &c, const std::string &delim, std::pmr::memory_resource *mr) {
    return numkit::lang::strjoin(c, delim.empty() ? Value::Empty : Value::fromString(delim, mr), mr);
}
Value strsplit(const Value &s, const std::string &delim, std::pmr::memory_resource *mr) {
    return delim.empty() ? numkit::lang::strsplit(s, mr) : numkit::lang::strsplit(s, Value::fromString(delim, mr), mr);
}
Value splitlines(const Value &s, std::pmr::memory_resource *mr) { return numkit::lang::splitlines(s, mr); }

Value strfind(const Value &text, const Value &pattern, std::pmr::memory_resource *mr) { return numkit::lang::strfind(text, pattern, mr); }
Value strrep(const Value &orig, const Value &oldStr, const Value &newStr, std::pmr::memory_resource *mr) { return numkit::lang::strrep(orig, oldStr, newStr, mr); }
Value contains(const Value &str, const Value &pat, std::pmr::memory_resource *mr) { return numkit::lang::contains(str, pat, mr); }
Value startsWith(const Value &str, const Value &pat, std::pmr::memory_resource *mr) { return numkit::lang::startsWith(str, pat, mr); }
Value endsWith(const Value &str, const Value &pat, std::pmr::memory_resource *mr) { return numkit::lang::endsWith(str, pat, mr); }
Value count(const Value &str, const Value &pat, std::pmr::memory_resource *mr) { return numkit::lang::count(str, pat, mr); }
Value reverse(const Value &str, std::pmr::memory_resource *mr) { return numkit::lang::reverse(str, mr); }

} // namespace numkit::builtin
