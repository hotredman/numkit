// src/builtin/include/numkit/builtin/strfun.hpp
//
// Pure C++ String and character array manipulation functions (MATLAB parity).
#pragma once

#include <memory_resource>
#include <string>
#include <numkit/value/value.hpp>
#include <numkit/value/span.hpp>

namespace numkit::builtin {

/// @addtogroup group_strfun
/// @{


/// @file
/// @ingroup group_strfun
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
/// @brief Formats numeric array into character vector with specified precision digits (`num2str(x, N)`).
/// @param x Input numeric array or scalar.
/// @param N Maximum number of significant digits.
/// @param mr Memory resource.
/// @return Character vector representation.
/// @see int2str, mat2str, str2num
Value num2str(const Value &x, int N, std::pmr::memory_resource *mr = nullptr);

/// @brief Formats numeric array into character vector with format specification (`num2str(x, fmt)`).
/// @param x Input numeric array or scalar.
/// @param fmt C-style printf format string (e.g. `"%10.5f"`).
/// @param mr Memory resource.
/// @return Character vector representation.
/// @see int2str, mat2str, str2num
Value num2str(const Value &x, const std::string &fmt, std::pmr::memory_resource *mr = nullptr);

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

/// @brief Formats matrix into MATLAB syntax string with precision (`mat2str(x, precision)`).
/// @param x Input matrix.
/// @param precision Number of digits of precision.
/// @param mr Memory resource.
/// @return Formatted character vector.
/// @see num2str
Value mat2str(const Value &x, int precision, std::pmr::memory_resource *mr = nullptr);

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
/// @see string_array, toChar
Value char_array(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Alias for char_array conversion (`char(x)`).
/// @param x Input value.
/// @param mr Memory resource.
/// @return Character array.
/// @see char_array
Value toChar(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Converts input to string array (`string(x)`).
/// @param x Input value.
/// @param mr Memory resource.
/// @return String array.
/// @see char_array, toString
Value string_array(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Alias for string_array conversion (`string(x)`).
/// @param x Input value.
/// @param mr Memory resource.
/// @return String array.
/// @see string_array
Value toString(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Creates character vector of blank spaces (`blanks(n)`).
/// @param n Number of spaces.
/// @param mr Memory resource.
/// @return `1 x n` character vector containing ASCII space characters.
Value blanks(size_t n, std::pmr::memory_resource *mr = nullptr);

/// @brief Creates newline character string (`newline`).
/// @param mr Memory resource.
/// @return `1 x 1` string scalar containing `"\n"`.
Value newline(std::pmr::memory_resource *mr = nullptr);

/// @brief Generates newline character as string Value.
/// @param mr Memory resource.
/// @return Newline string value.
Value newlineFn(std::pmr::memory_resource *mr = nullptr);

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

/// @brief Strips specified characters from side of string array (`strip(s, side, ch)`).
/// @param s Input string array.
/// @param side Direction (`"left"`, `"right"`, or `"both"`).
/// @param ch Characters to strip.
/// @param mr Memory resource.
/// @return Stripped string array.
Value strip(const Value &s, const Value &side, const Value &ch, std::pmr::memory_resource *mr = nullptr);

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

/// @brief Joins string array with Value delimiter (`strjoin(arr, delim)`).
/// @param arr String array or cell array.
/// @param delim Delimiter Value.
/// @param mr Memory resource.
/// @return Joined character vector.
Value strjoin(const Value &arr, const Value &delim, std::pmr::memory_resource *mr = nullptr);

/// @brief Combines elements of string array into single string (`join(arr, delim)`).
/// @param arr Input string array.
/// @param delim Delimiter (default: empty).
/// @param mr Memory resource.
/// @return Joined string scalar.
/// @see split, strjoin
Value join(const Value &arr, const Value &delim = Value::Empty, std::pmr::memory_resource *mr = nullptr);

/// @brief Appends multiple strings element-wise (`append(s1, s2, ...)`).
/// @param parts Span of input string arrays.
/// @param mr Memory resource.
/// @return Element-wise concatenated string array.
Value append(Span<const Value> parts, std::pmr::memory_resource *mr = nullptr);

/// @brief Splits string by delimiter string (`strsplit(s, delim)`).
/// @param s Input string.
/// @param delim Delimiter string (default: `" "`).
/// @param mr Memory resource.
/// @return Cell array of split substrings.
/// @see strjoin, splitlines, split
Value strsplit(const Value &s, const std::string &delim = " ", std::pmr::memory_resource *mr = nullptr);

/// @brief Splits string by Value delimiter (`strsplit(s, delim)`).
/// @param s Input string.
/// @param delim Delimiter value.
/// @param mr Memory resource.
/// @return Cell array of split substrings.
Value strsplit(const Value &s, const Value &delim, std::pmr::memory_resource *mr = nullptr);

/// @brief Splits string by whitespace (`strsplit(s)`).
/// @param s Input string.
/// @param mr Memory resource.
/// @return Cell array of split substrings.
Value strsplit(const Value &s, std::pmr::memory_resource *mr);

/// @brief Splits string array elements by delimiter (`split(s, delim)`).
/// @param s Input string array.
/// @param delim Delimiter pattern.
/// @param mr Memory resource.
/// @return Split string array or cell array.
/// @see join, strsplit
Value split(const Value &s, const Value &delim = Value::Empty, std::pmr::memory_resource *mr = nullptr);

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
/// @param ignoreCase Case-insensitive flag.
/// @param mr Memory resource.
/// @return Logical scalar or array.
/// @see startsWith, endsWith, matches
Value contains(const Value &str, const Value &pat, bool ignoreCase = false, std::pmr::memory_resource *mr = nullptr);

/// @brief Checks if string starts with pattern (`startsWith(str, pat)`).
/// @param str Text to search.
/// @param pat Prefix pattern.
/// @param ignoreCase Case-insensitive flag.
/// @param mr Memory resource.
/// @return Logical scalar or array.
/// @see endsWith, contains
Value startsWith(const Value &str, const Value &pat, bool ignoreCase = false, std::pmr::memory_resource *mr = nullptr);

/// @brief Checks if string ends with pattern (`endsWith(str, pat)`).
/// @param str Text to search.
/// @param pat Suffix pattern.
/// @param ignoreCase Case-insensitive flag.
/// @param mr Memory resource.
/// @return Logical scalar or array.
/// @see startsWith, contains
Value endsWith(const Value &str, const Value &pat, bool ignoreCase = false, std::pmr::memory_resource *mr = nullptr);

/// @brief Returns number of characters in each string element (`strlength(s)`).
/// @param s Input string array.
/// @param mr Memory resource.
/// @return Array of element string lengths.
Value strlength(const Value &s, std::pmr::memory_resource *mr = nullptr);

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

/// @brief Erases occurrences of substring pattern from string (`erase(s, pat)`).
/// @param s Input string.
/// @param pat Pattern to erase.
/// @param mr Memory resource.
/// @return String with pattern removed.
Value erase(const Value &s, const Value &pat, std::pmr::memory_resource *mr = nullptr);

/// @brief Replaces substring pattern with new pattern in string array (`replace(s, oldPat, newPat)`).
/// @param s Input string array.
/// @param oldPat Pattern to replace.
/// @param newPat Replacement text.
/// @param mr Memory resource.
/// @return String array with replacements applied.
Value replace(const Value &s, const Value &oldPat, const Value &newPat, std::pmr::memory_resource *mr = nullptr);

/// @brief Replaces text between specified start and end boundaries (`replaceBetween(s, start, end, newText)`).
/// @param s Original text.
/// @param start 1-based start index or substring pattern.
/// @param end 1-based end index or substring pattern.
/// @param newText Replacement text.
/// @param mr Memory resource.
/// @return Text with range replaced.
/// @see extractBetween, eraseBetween
Value replaceBetween(const Value &s, const Value &start, const Value &end, const Value &newText, std::pmr::memory_resource *mr = nullptr);

/// @brief Extracts substrings after specified pattern or position (`extractAfter(s, p)`).
/// @param s Input string array.
/// @param p Position index or pattern.
/// @param mr Memory resource.
/// @return Substring after boundary.
Value extractAfter(const Value &s, const Value &p, std::pmr::memory_resource *mr = nullptr);

/// @brief Extracts substrings before specified pattern or position (`extractBefore(s, p)`).
/// @param s Input string array.
/// @param p Position index or pattern.
/// @param mr Memory resource.
/// @return Substring before boundary.
Value extractBefore(const Value &s, const Value &p, std::pmr::memory_resource *mr = nullptr);

/// @brief Extracts substrings between specified boundaries (`extractBetween(s, start, end)`).
/// @param s Input string array.
/// @param start 1-based start index or start pattern.
/// @param end 1-based end index or end pattern.
/// @param mr Memory resource.
/// @return Extracted substring.
Value extractBetween(const Value &s, const Value &start, const Value &end, std::pmr::memory_resource *mr = nullptr);

/// @brief Inserts text after specified position or pattern (`insertAfter(s, p, newText)`).
/// @param s Original string.
/// @param p Position index or pattern.
/// @param newText Text to insert.
/// @param mr Memory resource.
/// @return Modified string.
Value insertAfter(const Value &s, const Value &p, const Value &newText, std::pmr::memory_resource *mr = nullptr);

/// @brief Inserts text before specified position or pattern (`insertBefore(s, p, newText)`).
/// @param s Original string.
/// @param p Position index or pattern.
/// @param newText Text to insert.
/// @param mr Memory resource.
/// @return Modified string.
Value insertBefore(const Value &s, const Value &p, const Value &newText, std::pmr::memory_resource *mr = nullptr);

/// @brief Erases text between specified boundaries (`eraseBetween(s, start, end)`).
/// @param s Original string.
/// @param start 1-based start position or pattern.
/// @param end 1-based end position or pattern.
/// @param mr Memory resource.
/// @return Modified string.
Value eraseBetween(const Value &s, const Value &start, const Value &end, std::pmr::memory_resource *mr = nullptr);

/// @brief Extracts substring matching pattern (`extract(s, pat)`).
/// @param s Input string.
/// @param pat Pattern to extract.
/// @param mr Memory resource.
/// @return Extracted matching substring.
Value extract(const Value &s, const Value &pat, std::pmr::memory_resource *mr = nullptr);

/// @brief Pads string to fixed length (`pad(s, n, side, padChar)`).
/// @param s Input string array.
/// @param n Target minimum length.
/// @param side Direction (`"left"`, `"right"`, or `"both"`).
/// @param padChar Character used to pad (default space).
/// @param mr Memory resource.
/// @return Padded string array.
Value pad(const Value &s, size_t n, const Value &side = Value(), const Value &padChar = Value(), std::pmr::memory_resource *mr = nullptr);

/// @brief Justifies character matrix (`strjust(M, side)`).
/// @param M Input character matrix.
/// @param side Justification side (`"left"`, `"right"`, `"center"`).
/// @param mr Memory resource.
/// @return Justified character matrix.
Value strjust(const Value &M, const std::string &side = "left", std::pmr::memory_resource *mr = nullptr);

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

/// @brief Tests if characters are whitespace (internal helper returning Value).
/// @param s Input text.
/// @param mr Memory resource.
/// @return Logical array.
Value isspaceFn(const Value &s, std::pmr::memory_resource *mr = nullptr);

/// @brief Tests if characters have specified string property (`isstrprop(s, category)`).
/// @param s Input text.
/// @param category Property name (e.g. `'alpha'`, `'digit'`, `'alphanum'`, `'punct'`, `'upper'`, `'lower'`).
/// @param mr Memory resource.
/// @return Logical array.
/// @see isletter, isspace
Value isstrprop(const Value &s, const std::string &category, std::pmr::memory_resource *mr = nullptr);

/// @brief Tests if characters have specified string property via Value category (`isstrprop(s, category)`).
/// @param s Input text.
/// @param category Property name value.
/// @param mr Memory resource.
/// @return Logical array.
Value isstrprop(const Value &s, const Value &category, std::pmr::memory_resource *mr = nullptr);

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

/// @brief Converts character arrays in input to string arrays (`convertCharsToStrings(x)`).
/// @param x Input value.
/// @param mr Memory resource.
/// @return Converted string value.
Value convertCharsToStrings(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Converts string arrays in input to character vectors (`convertStringsToChars(x)`).
/// @param x Input value.
/// @param mr Memory resource.
/// @return Converted character array value.
Value convertStringsToChars(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Recursively converts all contained strings in cells/structs to character vectors.
/// @param v Container value.
/// @param mr Memory resource.
/// @return Container with strings converted to char vectors.
Value convertContainedStringsToChars(const Value &v, std::pmr::memory_resource *mr = nullptr);

/// @brief Constructs an N-dimensional array of empty strings.
/// @param dims Dimensions of the target string array.
/// @param mr Memory resource.
/// @return N-D string array initialized to empty strings.
Value stringsND(Span<const size_t> dims, std::pmr::memory_resource *mr = nullptr);

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

/// @brief Converts 16-character hexadecimal string to IEEE double-precision floating-point number (`hex2num(s)`).
/// @param s Hexadecimal character vector or string array.
/// @param mr Memory resource.
/// @return IEEE 754 double precision floating-point number.
/// @see num2hex, hex2dec
Value hex2num(const Value &s, std::pmr::memory_resource *mr = nullptr);

/// @brief Converts double-precision floating-point number to 16-character hexadecimal string (`num2hex(x)`).
/// @param x Numeric scalar or array.
/// @param mr Memory resource.
/// @return 16-character hexadecimal string representation.
/// @see hex2num, dec2hex
Value num2hex(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Rational approximation as fraction / continued fraction (`rat(x, tol)`).
/// @param x Input floating-point value.
/// @param tol Error tolerance (default: 0.0 for 1e-6 * norm(x,1)).
/// @param mr Memory resource.
/// @return Cell array `{N, D}` containing numerator and denominator.
/// @see rats
Value rat(const Value &x, double tol = 0.0, std::pmr::memory_resource *mr = nullptr);

/// @brief Formats numbers as rational string representations (`rats(x, len)`).
/// @param x Input numeric array.
/// @param len Field length for each rational string (default: 13).
/// @param mr Memory resource.
/// @return Character array of formatted rational strings.
/// @see rat
Value rats(const Value &x, int len = 13, std::pmr::memory_resource *mr = nullptr);

/// @brief Extracts first token from character vector up to delimiter (`[token, rem] = strtok(str, delim)`).
/// @param str Input string.
/// @param delim Delimiter characters (default: `" \t\n\r"`).
/// @param mr Memory resource.
/// @return Pair of `{token, remainder}` character vectors.
std::pair<Value, Value> strtok(const Value &str, const std::string &delim = " \t\n\r", std::pmr::memory_resource *mr = nullptr);

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

/// @brief Regular expression search helper.
/// @param s Input text.
/// @param pat Regular expression pattern.
/// @param option Match option string.
/// @param ignoreCase Case-insensitive match flag.
/// @param mr Memory resource.
/// @return Match results Value.
Value regexpFind(const Value &s, const Value &pat, const std::string &option, bool ignoreCase, std::pmr::memory_resource *mr = nullptr);

/// @brief Regular expression search once helper.
/// @param s Input text.
/// @param pat Regular expression pattern.
/// @param option Match option string.
/// @param ignoreCase Case-insensitive match flag.
/// @param mr Memory resource.
/// @return First match result Value.
Value regexpFindOnce(const Value &s, const Value &pat, const std::string &option, bool ignoreCase, std::pmr::memory_resource *mr = nullptr);

/// @brief Regular expression replacement (`regexprep(str, pat, rep, ignoreCase, once)`).
/// @param str Original text.
/// @param pat Regex pattern.
/// @param rep Replacement string.
/// @param ignoreCase Case-insensitive match flag.
/// @param once Replace first match only flag.
/// @param mr Memory resource.
/// @return Text with matches replaced.
/// @see regexp, strrep
Value regexprep(const Value &str, const Value &pat, const Value &rep, bool ignoreCase = false, bool once = false, std::pmr::memory_resource *mr = nullptr);

/// @brief Translates wildcard strings to regular expressions (`regexptranslate(type, str)`).
/// @param type Translation type (`"wildcard"` or `"escape"`).
/// @param str Input string.
/// @param mr Memory resource.
/// @return Regex pattern string.
/// @see regexp
Value regexptranslate(const std::string &type, const std::string &str, std::pmr::memory_resource *mr = nullptr);

/// @brief Translates wildcard Value to regular expression (`regexptranslate(type, str)`).
/// @param type Translation type (`"wildcard"` or `"escape"`).
/// @param str Input string value.
/// @param mr Memory resource.
/// @return Regex pattern Value.
Value regexptranslate(const std::string &type, const Value &str, std::pmr::memory_resource *mr = nullptr);

/// @brief Formats data into string array (`compose(fmt, x)`).
/// @param fmt Format string.
/// @param x Input arguments.
/// @param mr Memory resource.
/// @return Formatted string array.
Value compose(const Value &fmt, const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Formats single pass of format specification.
/// @param fmt Format string.
/// @param args Span of arguments.
/// @param argStart Starting index in args.
/// @param literalWhenShort Format literal string flag when short.
/// @return Formatted string.
std::string formatOnce(const std::string &fmt, Span<const Value> args, size_t argStart = 0, bool literalWhenShort = false);

/// @brief Formats cyclic repetition of format specification across array elements.
/// @param fmt Format string.
/// @param args Span of arguments.
/// @param argStart Starting index in args.
/// @param mr Memory resource.
/// @return Formatted string.
std::string formatCyclic(const std::string &fmt, Span<const Value> args, size_t argStart = 0, std::pmr::memory_resource *mr = nullptr);

/// @brief Counts number of conversion specifiers in format string.
/// @param fmt Format string.
/// @return Count of `%` conversion specifiers.
size_t countFormatSpecs(const std::string &fmt);

/// @brief Formats Value for terminal display via `disp`.
/// @param a Input value.
/// @return Rendered string for display.
std::string dispFormat(const Value &a);


/// @}
} // namespace numkit::builtin
