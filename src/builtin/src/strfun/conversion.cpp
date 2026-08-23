// src/builtin/src/strfun/conversion.cpp
//
// String conversion and construction implementations for numkit::builtin.

#include <numkit/builtin/strfun.hpp>
#include <numkit/value/value.hpp>
#include <numkit/value/span.hpp>
#include <numkit/lang/strings/strings.hpp>

namespace numkit::builtin {

Value num2str(const Value &x, std::pmr::memory_resource *mr) { return numkit::lang::num2str(x, mr); }
Value int2str(const Value &x, std::pmr::memory_resource *mr) { return numkit::lang::int2str(x, mr); }
Value mat2str(const Value &x, std::pmr::memory_resource *mr) { return numkit::lang::mat2str(x, 15, mr); }
Value str2num(const Value &s, std::pmr::memory_resource *mr) { return numkit::lang::str2num(s, mr); }
Value str2double(const Value &s, std::pmr::memory_resource *mr) { return numkit::lang::str2double(s, mr); }
Value char_array(const Value &x, std::pmr::memory_resource *mr) { return numkit::lang::toChar(x, mr); }
Value string_array(const Value &x, std::pmr::memory_resource *mr) { return numkit::lang::toString(x, mr); }
Value blanks(size_t n, std::pmr::memory_resource *mr) { return numkit::lang::blanks(n, mr); }
Value newline(std::pmr::memory_resource *mr) { return Value::fromString("\n", mr); }

Value dec2bin(const Value &d, int minDigits, std::pmr::memory_resource *mr) { return numkit::lang::dec2bin(d, minDigits, mr); }
Value dec2hex(const Value &d, int minDigits, std::pmr::memory_resource *mr) { return numkit::lang::dec2hex(d, minDigits, mr); }
Value dec2base(const Value &d, int base, int minDigits, std::pmr::memory_resource *mr) { return numkit::lang::dec2base(d, base, minDigits, mr); }
Value base2dec(const Value &s, int base, std::pmr::memory_resource *mr) { return numkit::lang::base2dec(s, base, mr); }
Value bin2dec(const Value &s, std::pmr::memory_resource *mr) { return numkit::lang::bin2dec(s, mr); }
Value hex2dec(const Value &s, std::pmr::memory_resource *mr) { return numkit::lang::hex2dec(s, mr); }

} // namespace numkit::builtin
