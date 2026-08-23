// include/numkit/builtin/strfun.hpp
//
// String and character array manipulation functions (MATLAB parity).
#pragma once

#include <memory_resource>
#include <string>
#include <numkit/value/value.hpp>
#include <numkit/value/span.hpp>

namespace numkit {
class Engine;
}

namespace numkit::builtin {

/// @file
/// @brief String and character array builtins (MATLAB parity).

// ── Conversion & Construction ───────────────────────────────────────────────

/// @brief Formats numeric array into character vector.
Value num2str(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Formats integer into character vector.
Value int2str(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Formats matrix into MATLAB syntax string.
Value mat2str(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Converts string representation to numeric matrix.
Value str2num(const Value &s, std::pmr::memory_resource *mr = nullptr);

/// @brief Converts text to double precision numeric values.
Value str2double(const Value &s, std::pmr::memory_resource *mr = nullptr);

/// @brief Converts input to character array.
Value char_array(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Converts input to string array.
Value string_array(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Creates string of blank spaces.
Value blanks(size_t n, std::pmr::memory_resource *mr = nullptr);

/// @brief Creates newline character string.
Value newline(std::pmr::memory_resource *mr = nullptr);

// ── Comparison ─────────────────────────────────────────────────────────────

/// @brief Compares strings for exact equality (case-sensitive).
Value strcmp(const Value &s1, const Value &s2, std::pmr::memory_resource *mr = nullptr);

/// @brief Compares first `n` characters of strings (case-sensitive).
Value strncmp(const Value &s1, const Value &s2, size_t n, std::pmr::memory_resource *mr = nullptr);

/// @brief Compares strings for equality ignoring case.
Value strcmpi(const Value &s1, const Value &s2, std::pmr::memory_resource *mr = nullptr);

/// @brief Compares first `n` characters of strings ignoring case.
Value strncmpi(const Value &s1, const Value &s2, size_t n, std::pmr::memory_resource *mr = nullptr);

/// @brief Tests if strings match pattern.
Value matches(const Value &str, const Value &pat, std::pmr::memory_resource *mr = nullptr);

// ── Case & Trimming ─────────────────────────────────────────────────────────

/// @brief Converts string to uppercase.
Value upper(const Value &s, std::pmr::memory_resource *mr = nullptr);

/// @brief Converts string to lowercase.
Value lower(const Value &s, std::pmr::memory_resource *mr = nullptr);

/// @brief Removes leading and trailing whitespace.
Value strtrim(const Value &s, std::pmr::memory_resource *mr = nullptr);

/// @brief Removes trailing whitespace.
Value deblank(const Value &s, std::pmr::memory_resource *mr = nullptr);

/// @brief Strips leading and trailing whitespace from string array.
Value strip(const Value &s, std::pmr::memory_resource *mr = nullptr);

// ── Joining & Splitting ─────────────────────────────────────────────────────

/// @brief Concatenates strings horizontally.
Value strcat(Span<const Value> args, std::pmr::memory_resource *mr = nullptr);

/// @brief Joins string array or cell array of strings with delimiter.
Value strjoin(const Value &c, const std::string &delim = " ", std::pmr::memory_resource *mr = nullptr);

/// @brief Splits string by delimiter.
Value strsplit(const Value &s, const std::string &delim = " ", std::pmr::memory_resource *mr = nullptr);

/// @brief Splits string into separate lines.
Value splitlines(const Value &s, std::pmr::memory_resource *mr = nullptr);

// ── Search & Replace ────────────────────────────────────────────────────────

/// @brief Finds occurrences of substring within string.
Value strfind(const Value &text, const Value &pattern, std::pmr::memory_resource *mr = nullptr);

/// @brief Replaces substring within string.
Value strrep(const Value &orig, const Value &oldStr, const Value &newStr, std::pmr::memory_resource *mr = nullptr);

/// @brief Checks if string contains pattern.
Value contains(const Value &str, const Value &pat, std::pmr::memory_resource *mr = nullptr);

/// @brief Checks if string starts with pattern.
Value startsWith(const Value &str, const Value &pat, std::pmr::memory_resource *mr = nullptr);

/// @brief Checks if string ends with pattern.
Value endsWith(const Value &str, const Value &pat, std::pmr::memory_resource *mr = nullptr);

/// @brief Counts occurrences of pattern in string.
Value count(const Value &str, const Value &pat, std::pmr::memory_resource *mr = nullptr);

/// @brief Reverses characters in string.
Value reverse(const Value &str, std::pmr::memory_resource *mr = nullptr);

// ── Classification & Validation ─────────────────────────────────────────────

/// @brief Tests if characters are letters.
Value isletter(const Value &s, std::pmr::memory_resource *mr = nullptr);

/// @brief Tests if characters are whitespace.
Value isspace(const Value &s, std::pmr::memory_resource *mr = nullptr);

/// @brief Tests if characters have specified string property (e.g. 'alpha', 'digit', 'alphanum').
Value isstrprop(const Value &s, const std::string &category, std::pmr::memory_resource *mr = nullptr);

/// @brief Tests if input is a scalar string.
Value isstringscalar(const Value &s, std::pmr::memory_resource *mr = nullptr);

/// @brief Validates text against list of valid strings.
Value validatestring(const Value &str, const Value &validStrings, std::pmr::memory_resource *mr = nullptr);

/// @brief Recursively converts all contained strings in cells/structs to character vectors.
Value convertContainedStringsToChars(const Value &v, std::pmr::memory_resource *mr = nullptr);

// ── Base & Hex Conversions ──────────────────────────────────────────────────

/// @brief Decimal to binary string.
Value dec2bin(const Value &d, int minDigits = 0, std::pmr::memory_resource *mr = nullptr);

/// @brief Decimal to hexadecimal string.
Value dec2hex(const Value &d, int minDigits = 0, std::pmr::memory_resource *mr = nullptr);

/// @brief Decimal to base-N string.
Value dec2base(const Value &d, int base, int minDigits = 0, std::pmr::memory_resource *mr = nullptr);

/// @brief Base-N string to decimal.
Value base2dec(const Value &s, int base, std::pmr::memory_resource *mr = nullptr);

/// @brief Binary string to decimal.
Value bin2dec(const Value &s, std::pmr::memory_resource *mr = nullptr);

/// @brief Hexadecimal string to decimal.
Value hex2dec(const Value &s, std::pmr::memory_resource *mr = nullptr);

// ── Regular Expressions ─────────────────────────────────────────────────────

/// @brief Regular expression matching.
Value regexp(const Value &str, const Value &pat, std::pmr::memory_resource *mr = nullptr);

/// @brief Case-insensitive regular expression matching.
Value regexpi(const Value &str, const Value &pat, std::pmr::memory_resource *mr = nullptr);

/// @brief Regular expression replacement.
Value regexprep(const Value &str, const Value &pat, const Value &rep, std::pmr::memory_resource *mr = nullptr);

/// @brief Translates wildcard strings to regular expressions.
Value regexptranslate(const std::string &type, const Value &str, std::pmr::memory_resource *mr = nullptr);

// ── Registration ────────────────────────────────────────────────────────────

/// @brief Registers all string builtins into the engine instance.
void register_strfun(Engine &engine);

} // namespace numkit::builtin
