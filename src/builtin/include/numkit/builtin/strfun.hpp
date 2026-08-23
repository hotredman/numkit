// src/builtin/include/numkit/builtin/strfun.hpp
//
// Pure C++ String and character array manipulation functions (MATLAB parity).
#pragma once

#include <memory_resource>
#include <string>
#include <numkit/value/value.hpp>
#include <numkit/value/span.hpp>

namespace numkit::builtin {

/// @file
/// @brief String, character vector, and regular expression operations.
///
/// Provides a clean, engine-free C++ API for string comparisons, formatting,
/// pattern searching, regex replacements, base conversions, and predicates.

// ── Conversion & Construction ───────────────────────────────────────────────

/// @brief Formats numeric array into character vector (`num2str(x)`).
/// @param x Input numeric array or scalar.
/// @param mr Memory resource for allocations (nullptr for default).
/// @return Character vector representation.
/// @see int2str, mat2str, str2num
Value num2str(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Formats integer into character vector (`int2str(x)`).
/// @param x Input integer or numeric value.
/// @param mr Memory resource.
/// @return Character vector representation.
/// @see num2str, mat2str
Value int2str(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Formats matrix into MATLAB syntax string (`mat2str(x)`).
/// @param x Input matrix.
/// @param mr Memory resource.
/// @return Character vector formatted as MATLAB matrix literal `[1, 2; 3, 4]`.
/// @see num2str
Value mat2str(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Converts string representation to numeric matrix (`str2num(s)`).
/// @param s Character vector or string.
/// @param mr Memory resource.
/// @return Parsed numeric matrix or empty array on parse failure.
/// @see str2double, num2str
Value str2num(const Value &s, std::pmr::memory_resource *mr = nullptr);

/// @brief Converts text to double precision numeric values (`str2double(s)`).
/// @param s Text scalar or string array.
/// @param mr Memory resource.
/// @return Double-precision numeric values (NaN for non-convertible elements).
/// @see str2num
Value str2double(const Value &s, std::pmr::memory_resource *mr = nullptr);

/// @brief Converts input to character array (`char(x)`).
/// @param x Input value (numeric array of codes, cell of strings, etc.).
/// @param mr Memory resource.
/// @return Character array.
/// @see string_array
Value char_array(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Converts input to string array (`string(x)`).
/// @param x Input value.
/// @param mr Memory resource.
/// @return String array.
/// @see char_array
Value string_array(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Creates character vector of blank spaces (`blanks(n)`).
/// @param n Number of spaces.
/// @param mr Memory resource.
/// @return `1 x n` character vector containing ASCII space characters.
Value blanks(size_t n, std::pmr::memory_resource *mr = nullptr);

/// @brief Creates newline character string (`newline`).
/// @param mr Memory resource.
/// @return `1 x 1` string scalar containing `"\n"`.
Value newline(std::pmr::memory_resource *mr = nullptr);

// ── Comparison ─────────────────────────────────────────────────────────────

/// @brief Compares strings for exact equality (case-sensitive) (`strcmp(s1, s2)`).
/// @param s1 First string or cell/array.
/// @param s2 Second string or cell/array.
/// @param mr Memory resource.
/// @return Logical array or boolean indicating exact elementwise equality.
/// @see strcmpi, strncmp, strncmpi
Value strcmp(const Value &s1, const Value &s2, std::pmr::memory_resource *mr = nullptr);

/// @brief Compares first `n` characters of strings (case-sensitive) (`strncmp(s1, s2, n)`).
/// @param s1 First string.
/// @param s2 Second string.
/// @param n Number of characters to compare.
/// @param mr Memory resource.
/// @return Logical result.
/// @see strcmp, strncmpi
Value strncmp(const Value &s1, const Value &s2, size_t n, std::pmr::memory_resource *mr = nullptr);

/// @brief Compares strings for equality ignoring case (`strcmpi(s1, s2)`).
/// @param s1 First string.
/// @param s2 Second string.
/// @param mr Memory resource.
/// @return Logical result.
/// @see strcmp, strncmpi
Value strcmpi(const Value &s1, const Value &s2, std::pmr::memory_resource *mr = nullptr);

/// @brief Compares first `n` characters of strings ignoring case (`strncmpi(s1, s2, n)`).
/// @param s1 First string.
/// @param s2 Second string.
/// @param n Number of characters to compare.
/// @param mr Memory resource.
/// @return Logical result.
/// @see strcmpi, strncmp
Value strncmpi(const Value &s1, const Value &s2, size_t n, std::pmr::memory_resource *mr = nullptr);

/// @brief Tests if strings match pattern (`matches(str, pat)`).
/// @param str Text array.
/// @param pat Pattern string.
/// @param mr Memory resource.
/// @return Logical array.
/// @see contains, startsWith, endsWith
Value matches(const Value &str, const Value &pat, std::pmr::memory_resource *mr = nullptr);

// ── Case & Trimming ─────────────────────────────────────────────────────────

/// @brief Converts string to uppercase (`upper(s)`).
/// @param s Input text.
/// @param mr Memory resource.
/// @return Uppercase text.
/// @see lower
Value upper(const Value &s, std::pmr::memory_resource *mr = nullptr);

/// @brief Converts string to lowercase (`lower(s)`).
/// @param s Input text.
/// @param mr Memory resource.
/// @return Lowercase text.
/// @see upper
Value lower(const Value &s, std::pmr::memory_resource *mr = nullptr);

/// @brief Removes leading and trailing whitespace (`strtrim(s)`).
/// @param s Input text.
/// @param mr Memory resource.
/// @return Trimmed text.
/// @see deblank, strip
Value strtrim(const Value &s, std::pmr::memory_resource *mr = nullptr);

/// @brief Removes trailing whitespace (`deblank(s)`).
/// @param s Input text.
/// @param mr Memory resource.
/// @return Deblanked text.
/// @see strtrim
Value deblank(const Value &s, std::pmr::memory_resource *mr = nullptr);

/// @brief Strips leading and trailing whitespace from string array (`strip(s)`).
/// @param s Input string array.
/// @param mr Memory resource.
/// @return Stripped string array.
/// @see strtrim
Value strip(const Value &s, std::pmr::memory_resource *mr = nullptr);

// ── Joining & Splitting ─────────────────────────────────────────────────────

/// @brief Concatenates strings horizontally (`strcat(s1, s2, ...)`).
/// @param args Span of string arguments.
/// @param mr Memory resource.
/// @return Concatenated string.
/// @see strjoin
Value strcat(Span<const Value> args, std::pmr::memory_resource *mr = nullptr);

/// @brief Joins string array or cell array of strings with delimiter (`strjoin(c, delim)`).
/// @param c Cell array or string array.
/// @param delim Delimiter string (default: `" "`).
/// @param mr Memory resource.
/// @return Joined character vector.
/// @see strsplit, strcat
Value strjoin(const Value &c, const std::string &delim = " ", std::pmr::memory_resource *mr = nullptr);

/// @brief Splits string by delimiter (`strsplit(s, delim)`).
/// @param s Input string.
/// @param delim Delimiter string (default: `" "`).
/// @param mr Memory resource.
/// @return Cell array of split substrings.
/// @see strjoin, splitlines
Value strsplit(const Value &s, const std::string &delim = " ", std::pmr::memory_resource *mr = nullptr);

/// @brief Splits string into separate lines (`splitlines(s)`).
/// @param s Input string.
/// @param mr Memory resource.
/// @return String array or cell array of individual lines.
/// @see strsplit
Value splitlines(const Value &s, std::pmr::memory_resource *mr = nullptr);

// ── Search & Replace ────────────────────────────────────────────────────────

/// @brief Finds occurrences of substring within string (`strfind(text, pattern)`).
/// @param text Input text.
/// @param pattern Substring to find.
/// @param mr Memory resource.
/// @return 1-based start indices of matches.
/// @see strrep, contains
Value strfind(const Value &text, const Value &pattern, std::pmr::memory_resource *mr = nullptr);

/// @brief Replaces occurrences of substring within string (`strrep(orig, old, new)`).
/// @param orig Original string.
/// @param oldStr Pattern to replace.
/// @param newStr Replacement string.
/// @param mr Memory resource.
/// @return Modified string.
/// @see regexprep, strfind
Value strrep(const Value &orig, const Value &oldStr, const Value &newStr, std::pmr::memory_resource *mr = nullptr);

/// @brief Checks if string contains pattern (`contains(str, pat)`).
/// @param str Text to search.
/// @param pat Substring pattern.
/// @param mr Memory resource.
/// @return Logical scalar or array.
/// @see startsWith, endsWith, matches
Value contains(const Value &str, const Value &pat, std::pmr::memory_resource *mr = nullptr);

/// @brief Checks if string starts with pattern (`startsWith(str, pat)`).
/// @param str Text to check.
/// @param pat Prefix pattern.
/// @param mr Memory resource.
/// @return Logical scalar or array.
/// @see endsWith, contains
Value startsWith(const Value &str, const Value &pat, std::pmr::memory_resource *mr = nullptr);

/// @brief Checks if string ends with pattern (`endsWith(str, pat)`).
/// @param str Text to check.
/// @param pat Suffix pattern.
/// @param mr Memory resource.
/// @return Logical scalar or array.
/// @see startsWith, contains
Value endsWith(const Value &str, const Value &pat, std::pmr::memory_resource *mr = nullptr);

/// @brief Counts occurrences of pattern in string (`count(str, pat)`).
/// @param str Text to search.
/// @param pat Substring pattern.
/// @param mr Memory resource.
/// @return Count of occurrences.
/// @see contains, strfind
Value count(const Value &str, const Value &pat, std::pmr::memory_resource *mr = nullptr);

/// @brief Reverses characters in string (`reverse(str)`).
/// @param str Input string.
/// @param mr Memory resource.
/// @return Reversed string.
Value reverse(const Value &str, std::pmr::memory_resource *mr = nullptr);

// ── Classification & Validation ─────────────────────────────────────────────

/// @brief Tests if characters are letters (`isletter(s)`).
/// @param s Input text.
/// @param mr Memory resource.
/// @return Logical array matching shape of `s`.
/// @see isspace, isstrprop
Value isletter(const Value &s, std::pmr::memory_resource *mr = nullptr);

/// @brief Tests if characters are whitespace (`isspace(s)`).
/// @param s Input text.
/// @param mr Memory resource.
/// @return Logical array.
/// @see isletter, isstrprop
Value isspace(const Value &s, std::pmr::memory_resource *mr = nullptr);

/// @brief Tests if characters have specified string property (`isstrprop(s, category)`).
/// @param s Input text.
/// @param category Property name (e.g. `'alpha'`, `'digit'`, `'alphanum'`, `'punct'`, `'upper'`, `'lower'`).
/// @param mr Memory resource.
/// @return Logical array.
/// @see isletter, isspace
Value isstrprop(const Value &s, const std::string &category, std::pmr::memory_resource *mr = nullptr);

/// @brief Tests if input is a scalar string (`isstringscalar(s)`).
/// @param s Input value.
/// @param mr Memory resource.
/// @return True if `s` is a 1x1 string array.
Value isstringscalar(const Value &s, std::pmr::memory_resource *mr = nullptr);

/// @brief Validates text against list of valid strings (`validatestring(str, validStrings)`).
/// @param str Query string.
/// @param validStrings Cell or array of valid candidate strings.
/// @param mr Memory resource.
/// @return Unambiguous matched string from `validStrings`.
Value validatestring(const Value &str, const Value &validStrings, std::pmr::memory_resource *mr = nullptr);

/// @brief Recursively converts all contained strings in cells/structs to character vectors.
/// @param v Container value.
/// @param mr Memory resource.
/// @return Container with strings converted to char vectors.
Value convertContainedStringsToChars(const Value &v, std::pmr::memory_resource *mr = nullptr);

// ── Base & Hex Conversions ──────────────────────────────────────────────────

/// @brief Decimal to binary string (`dec2bin(d, minDigits)`).
/// @param d Non-negative integer value(s).
/// @param minDigits Minimum number of binary digits to output.
/// @param mr Memory resource.
/// @return Character array of binary representations.
/// @see bin2dec, dec2hex
Value dec2bin(const Value &d, int minDigits = 0, std::pmr::memory_resource *mr = nullptr);

/// @brief Decimal to hexadecimal string (`dec2hex(d, minDigits)`).
/// @param d Non-negative integer value(s).
/// @param minDigits Minimum number of hex digits to output.
/// @param mr Memory resource.
/// @return Character array of hex representations.
/// @see hex2dec, dec2bin
Value dec2hex(const Value &d, int minDigits = 0, std::pmr::memory_resource *mr = nullptr);

/// @brief Decimal to base-N string (`dec2base(d, base, minDigits)`).
/// @param d Non-negative integer value(s).
/// @param base Radix base (2 to 36).
/// @param minDigits Minimum digits.
/// @param mr Memory resource.
/// @return Base-N character array.
/// @see base2dec
Value dec2base(const Value &d, int base, int minDigits = 0, std::pmr::memory_resource *mr = nullptr);

/// @brief Base-N string to decimal (`base2dec(s, base)`).
/// @param s Base-N text.
/// @param base Radix base (2 to 36).
/// @param mr Memory resource.
/// @return Decimal double.
/// @see dec2base
Value base2dec(const Value &s, int base, std::pmr::memory_resource *mr = nullptr);

/// @brief Binary string to decimal (`bin2dec(s)`).
/// @param s Binary text.
/// @param mr Memory resource.
/// @return Decimal value.
/// @see dec2bin
Value bin2dec(const Value &s, std::pmr::memory_resource *mr = nullptr);

/// @brief Hexadecimal string to decimal (`hex2dec(s)`).
/// @param s Hexadecimal text.
/// @param mr Memory resource.
/// @return Decimal value.
/// @see dec2hex
Value hex2dec(const Value &s, std::pmr::memory_resource *mr = nullptr);

// ── Regular Expressions ─────────────────────────────────────────────────────

/// @brief Regular expression matching (`regexp(str, pat)`).
/// @param str Text to search.
/// @param pat Regular expression pattern.
/// @param mr Memory resource.
/// @return 1-based match start indices or match tokens.
/// @see regexpi, regexprep
Value regexp(const Value &str, const Value &pat, std::pmr::memory_resource *mr = nullptr);

/// @brief Case-insensitive regular expression matching (`regexpi(str, pat)`).
/// @param str Text to search.
/// @param pat Regular expression pattern.
/// @param mr Memory resource.
/// @return Match start indices.
/// @see regexp, regexprep
Value regexpi(const Value &str, const Value &pat, std::pmr::memory_resource *mr = nullptr);

/// @brief Regular expression replacement (`regexprep(str, pat, rep)`).
/// @param str Original text.
/// @param pat Regex pattern.
/// @param rep Replacement string.
/// @param mr Memory resource.
/// @return Text with matches replaced.
/// @see regexp, strrep
Value regexprep(const Value &str, const Value &pat, const Value &rep, std::pmr::memory_resource *mr = nullptr);

/// @brief Translates wildcard strings to regular expressions (`regexptranslate(type, str)`).
/// @param type Translation type (`"wildcard"` or `"escape"`).
/// @param str Input string.
/// @param mr Memory resource.
/// @return Regex pattern string.
/// @see regexp
Value regexptranslate(const std::string &type, const Value &str, std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::builtin
