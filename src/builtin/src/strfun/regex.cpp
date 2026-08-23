// src/builtin/src/strfun/regex.cpp
//
// Regular expression implementations for numkit::builtin.

#include <numkit/builtin/strfun.hpp>
#include <numkit/value/value.hpp>
#include <numkit/value/span.hpp>
#include <numkit/lang/strings/regex.hpp>

namespace numkit::builtin {

Value regexp(const Value &str, const Value &pat, std::pmr::memory_resource *mr)
{
    return numkit::lang::regexpFind(str, pat, "", false, mr);
}

Value regexpi(const Value &str, const Value &pat, std::pmr::memory_resource *mr)
{
    return numkit::lang::regexpFind(str, pat, "", true, mr);
}

Value regexprep(const Value &str, const Value &pat, const Value &rep, std::pmr::memory_resource *mr)
{
    return numkit::lang::regexprep(str, pat, rep, false, false, mr);
}

Value regexptranslate(const std::string &type, const Value &str, std::pmr::memory_resource *mr)
{
    return numkit::lang::regexptranslate(type, str.isChar() || str.isString() ? str.toString() : "", mr);
}

} // namespace numkit::builtin
