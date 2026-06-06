// libs/builtin/include/numkit/builtin/language/strings/strings.hpp
#pragma once

#include <memory_resource>
#include <numkit/value/span.hpp>
#include <numkit/value/value.hpp>

#include <string>
#include <utility>

namespace numkit::builtin {

/// @file
/// @brief String and char builtins.
///
/// **Type conventions:**
/// - `s` / `pat` arguments accept either CHAR row or STRING.
/// - For most functions, output is the same string type as the input
///   (CHAR in → CHAR out; STRING in → STRING out).
/// - Predicates return LOGICAL scalars.
/// - `toString` / `toChar` / `classOf` rename C++ keywords (`string` is
///   `std::string` adjacent, `char` is a keyword).
/// - All comparisons are ASCII-only unless noted; case-insensitive
///   variants (`*i`) fold only `[A-Z]` ↔ `[a-z]`.

// ── Numeric ↔ string conversion ──────────────────────────────────────

/// @brief Number → CHAR row, default 5 significant digits (`s = num2str(x)`).
/// @param x   Scalar / matrix to format.
/// @param mr  Memory resource (nullptr → process default).
/// @return    CHAR row (or matrix for non-scalar input).
/// @see num2str(x, spec, mr), mat2str, str2num
Value num2str(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Number → CHAR row with N significant digits
/// (`s = num2str(x, N)`).
/// @param x   Scalar / matrix to format.
/// @param N   Number of significant digits (clamped to `[1, 99]`).
/// @param mr  Memory resource (nullptr → process default).
/// @return    CHAR formatted string.
/// @see num2str(x, mr), num2str(x, fmt, mr)
Value num2str(const Value &x, int N,
              std::pmr::memory_resource *mr = nullptr);

/// @brief Number → CHAR row with printf-style format
/// (`s = num2str(x, fmt)`).
/// @param x    Scalar / matrix to format.
/// @param fmt  Printf-style format string.
/// @param mr   Memory resource (nullptr → process default).
/// @return     CHAR formatted string.
/// @see num2str(x, mr), num2str(x, N, mr)
Value num2str(const Value &x, const std::string &fmt,
              std::pmr::memory_resource *mr = nullptr);

/// @brief Round to nearest integer → CHAR row (`s = int2str(x)`).
///
/// Rounds half away from zero (like MATLAB `round`) and renders the integer
/// with no decimals or scientific notation: int2str(2.5)="3",
/// int2str(-2.5)="-3", int2str(1e10)="10000000000". Inf/-Inf/NaN pass through
/// as "Inf"/"-Inf"/"NaN". Scalar only; vector/matrix column-alignment is a
/// separate deferred gap.
/// @param x   Real scalar to round and format.
/// @param mr  Memory resource (nullptr → process default).
/// @return    CHAR rounded-integer string.
/// @see num2str, mat2str
Value int2str(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Validate a string against a set of allowed values
/// (`s = validatestring(str, validStrings)`).
///
/// Case-insensitive. An exact match wins; otherwise a unique case-insensitive
/// leading-substring (prefix) match is returned. If `str` is a prefix of
/// several candidates and the shortest of those is itself a prefix of all the
/// others, that shortest candidate is returned; otherwise it is ambiguous and
/// throws. No prefix match at all throws. The returned value is the canonical
/// candidate (with its original case) as a CHAR row. Whitespace is significant
/// (not trimmed).
/// @param str    Char/string scalar to validate.
/// @param valid  Cell array of char vectors or a string array of candidates.
/// @param mr     Memory resource (nullptr → process default).
/// @return       Matching canonical candidate as a CHAR row.
/// @see strcmp, strcmpi
Value validatestring(const Value &str, const Value &valid,
                     std::pmr::memory_resource *mr = nullptr);

/// @brief Parse string as number (`x = str2num(s)`).
///
/// Returns empty Value on parse failure.
///
/// @param s   CHAR / STRING input.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Numeric value or empty.
/// @see str2double
Value str2num(const Value &s, std::pmr::memory_resource *mr = nullptr);

/// @brief Parse string as scalar number (`x = str2double(s)`).
///
/// Returns `NaN` on parse failure.
///
/// @param s   CHAR / STRING input.
/// @param mr  Memory resource (nullptr → process default).
/// @return    DOUBLE scalar (or NaN).
/// @see str2num
Value str2double(const Value &s, std::pmr::memory_resource *mr = nullptr);

/// @brief Convert to STRING type (`s = string(x)`).
///
/// C++ name `toString` to avoid lookup ambiguity with `std::string`.
///
/// @param x   Numeric / CHAR / LOGICAL input.
/// @param mr  Memory resource (nullptr → process default).
/// @return    STRING array.
/// @see toChar, convertCharsToStrings
Value toString(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Convert to CHAR type (`s = char(x)`).
///
/// C++ name `toChar` because `char` is a keyword.
///
/// @param x   Numeric / STRING input.
/// @param mr  Memory resource (nullptr → process default).
/// @return    CHAR array.
/// @see toString, convertStringsToChars
Value toChar(const Value &x, std::pmr::memory_resource *mr = nullptr);

// ── Comparison ───────────────────────────────────────────────────────

/// @brief Equal-strings test, case-sensitive (`tf = strcmp(a, b)`).
/// @param a   First string. @param b   Second string. @param mr  Memory resource.
/// @return    LOGICAL scalar. @see strcmpi, strncmp
Value strcmp(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);

/// @brief Equal-strings test, case-insensitive ASCII (`tf = strcmpi(a, b)`).
/// @param a   First string. @param b   Second string. @param mr  Memory resource.
/// @return    LOGICAL scalar. @see strcmp
Value strcmpi(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);

/// @brief First-n equal, case-sensitive (`tf = strncmp(a, b, n)`).
/// @param a  First string. @param b  Second string. @param n  Char count.
/// @param mr Memory resource. @return LOGICAL scalar. @see strncmpi, strcmp
Value strncmp(const Value &a, const Value &b, size_t n, std::pmr::memory_resource *mr = nullptr);

/// @brief First-n equal, case-insensitive (`tf = strncmpi(a, b, n)`).
/// @param a  First string. @param b  Second string. @param n  Char count.
/// @param mr Memory resource. @return LOGICAL scalar. @see strncmp
Value strncmpi(const Value &a, const Value &b, size_t n, std::pmr::memory_resource *mr = nullptr);

// ── Case ─────────────────────────────────────────────────────────────

/// @brief ASCII uppercase (`s = upper(s)`).
/// @param s   Input string. @param mr  Memory resource.
/// @return    Same-typed string with letters uppercased. @see lower
Value upper(const Value &s, std::pmr::memory_resource *mr = nullptr);

/// @brief ASCII lowercase (`s = lower(s)`).
/// @param s   Input string. @param mr  Memory resource.
/// @return    Same-typed string with letters lowercased. @see upper
Value lower(const Value &s, std::pmr::memory_resource *mr = nullptr);

// ── Trim / pad ───────────────────────────────────────────────────────

/// @brief Strip leading and trailing whitespace (`s = strtrim(s)`).
///
/// Whitespace = space / tab / CR / LF.
///
/// @param s   Input string. @param mr  Memory resource.
/// @return    Trimmed string. @see deblank, strip
Value strtrim(const Value &s, std::pmr::memory_resource *mr = nullptr);

/// @brief Strip trailing whitespace only (`s = deblank(s)`).
/// @param s   Input string. @param mr  Memory resource.
/// @return    Right-trimmed string. @see strtrim
Value deblank(const Value &s, std::pmr::memory_resource *mr = nullptr);

/// @brief Pad to a given length (`s = pad(s, n, side, padChar)`).
///
/// @param s        Input string.
/// @param n        Target length (no-op if `strlength(s) >= n`).
/// @param side     `"right"` (default), `"left"`, or `"both"`.
/// @param padChar  Padding char (default `' '`).
/// @param mr       Memory resource.
/// @return         Padded string.
/// @see strip, strjust
Value pad(const Value &s, size_t n, const Value &side = Value::Empty,
          const Value &padChar = Value::Empty,
          std::pmr::memory_resource *mr = nullptr);

/// @brief Strip whitespace or a custom char from one or both sides
/// (`s = strip(s, side, ch)`).
///
/// @param side  `"both"` (default), `"left"`, or `"right"`.
/// @param ch    Char to strip (default whitespace).
/// @param s     Input string. @param mr  Memory resource.
/// @return      Stripped string.
/// @see strtrim, deblank
Value strip(const Value &s, const Value &side = Value::Empty,
            const Value &ch = Value::Empty,
            std::pmr::memory_resource *mr = nullptr);

/// @brief Char row of `n` spaces (`s = blanks(n)`).
/// @param n   Length. @param mr  Memory resource. @return  `1 × n` CHAR row.
Value blanks(size_t n, std::pmr::memory_resource *mr = nullptr);

// ── Split / join / concat ────────────────────────────────────────────

/// @brief Split on whitespace (`c = strsplit(s)`).
/// @param s   Input string. @param mr  Memory resource.
/// @return    `1 × N` cell of CHAR tokens (empty tokens dropped).
/// @see strsplit(s, delim, mr), split, splitlines
Value strsplit(const Value &s, std::pmr::memory_resource *mr = nullptr);

/// @brief Split on a delimiter (`c = strsplit(s, delim)`).
///
/// Splits on the first char of `delim`. Empty tokens dropped.
///
/// @param s      Input string. @param delim  Delimiter string.
/// @param mr     Memory resource. @return  `1 × N` cell of CHAR tokens.
/// @see split
Value strsplit(const Value &s, const Value &delim, std::pmr::memory_resource *mr = nullptr);

/// @brief Split on every delimiter occurrence (`c = split(s, delim)`).
///
/// Empty tokens KEPT (differs from `strsplit`).
///
/// @param s      Input string. @param delim  Delimiter (char or string).
/// @param mr     Memory resource. @return  `N × 1` cell column.
/// @see strsplit, splitlines
Value split(const Value &s, const Value &delim, std::pmr::memory_resource *mr = nullptr);

/// @brief Split on newlines (`c = splitlines(s)`).
///
/// Splits on `CRLF` / `LF` / `CR`. Trailing newline does NOT introduce
/// a final empty token.
///
/// @param s   Input string. @param mr  Memory resource.
/// @return    `N × 1` cell column.
Value splitlines(const Value &s, std::pmr::memory_resource *mr = nullptr);

/// @brief Concatenate strings, **dropping** trailing whitespace
/// (`s = strcat(parts)`).
///
/// `strcat` strips trailing whitespace from each
/// CHAR-array operand before joining.
///
/// @param parts  Vector of strings.
/// @param mr     Memory resource.
/// @return       Concatenated string.
/// @see append, strjoin
Value strcat(Span<const Value> parts, std::pmr::memory_resource *mr = nullptr);

/// @brief Concatenate strings, **preserving** trailing whitespace
/// (`s = append(parts)`).
/// @param parts  Vector of strings. @param mr  Memory resource.
/// @return       Concatenated string. @see strcat
Value append(Span<const Value> parts, std::pmr::memory_resource *mr = nullptr);

/// @brief Join a cell of strings (`s = strjoin(c, delim)`).
/// @param c      1-D cell of strings.
/// @param delim  Separator (default `' '`).
/// @param mr     Memory resource.
/// @return       Single CHAR row.
/// @see strsplit, join
Value strjoin(const Value &c, const Value &delim = Value::Empty,
              std::pmr::memory_resource *mr = nullptr);

/// @brief Join elements of a string array (`s = join(arr, delim)`).
///
/// 2-D arrays join along columns, producing one row per source row
/// (`N × 1`). Default delim is `' '`.
///
/// @param arr    STRING array.
/// @param delim  Separator (default `' '`).
/// @param mr     Memory resource.
/// @return       Joined STRING array.
/// @see strjoin
Value join(const Value &arr, const Value &delim = Value::Empty,
           std::pmr::memory_resource *mr = nullptr);

// ── Length / size / search ───────────────────────────────────────────

/// @brief Length of each string (`n = strlength(s)`).
///
/// Elementwise for STRING arrays.
///
/// @param s   Input string / array. @param mr  Memory resource.
/// @return    DOUBLE length(s).
Value strlength(const Value &s, std::pmr::memory_resource *mr = nullptr);

/// @brief Find all non-overlapping occurrences (`p = strfind(s, pat)`).
///
/// 1-based positions of every non-overlapping occurrence of `pat` in `s`.
/// Returns `1 × K` row or empty (`0 × 0`).
///
/// @param s    Source string. @param pat  Pattern.
/// @param mr   Memory resource. @return   Row of 1-based positions.
/// @see contains, count, regexpFind
Value strfind(const Value &s, const Value &pat, std::pmr::memory_resource *mr = nullptr);

/// @brief Substring containment (`tf = contains(s, pat)`).
/// @param s           Source. @param pat  Pattern (or cell/string array — any).
/// @param ignoreCase  MATLAB 'IgnoreCase' name-value: case-insensitive match.
/// @param mr  Memory resource. @return  LOGICAL scalar. @see strfind
Value contains(const Value &s, const Value &pat, bool ignoreCase = false,
               std::pmr::memory_resource *mr = nullptr);

/// @brief Prefix test (`tf = startsWith(s, prefix)`).
/// @param s           Source. @param prefix  Prefix (or cell/string array).
/// @param ignoreCase  MATLAB 'IgnoreCase' name-value: case-insensitive match.
/// @param mr   Memory resource. @return  LOGICAL scalar. @see endsWith
Value startsWith(const Value &s, const Value &prefix, bool ignoreCase = false,
                 std::pmr::memory_resource *mr = nullptr);

/// @brief Suffix test (`tf = endsWith(s, suffix)`).
/// @param s           Source. @param suffix  Suffix (or cell/string array).
/// @param ignoreCase  MATLAB 'IgnoreCase' name-value: case-insensitive match.
/// @param mr   Memory resource. @return  LOGICAL scalar. @see startsWith
Value endsWith(const Value &s, const Value &suffix, bool ignoreCase = false,
               std::pmr::memory_resource *mr = nullptr);

/// @brief Non-overlapping match count (`n = count(s, pat)`).
/// @param s   Source. @param pat  Pattern.
/// @param mr  Memory resource. @return  Scalar count. @see strfind, contains
Value count(const Value &s, const Value &pat, std::pmr::memory_resource *mr = nullptr);

/// @brief Exact-match test (`tf = matches(s, pat)`).
///
/// True iff `s` exactly equals `pat`. For cell-of-strings `pat`, true
/// iff `s` equals any element.
///
/// @param s   Source. @param pat  Pattern or cell of patterns.
/// @param mr  Memory resource. @return  LOGICAL.
/// @see strcmp
Value matches(const Value &s, const Value &pat, std::pmr::memory_resource *mr = nullptr);

// ── Replace / erase / reverse ────────────────────────────────────────

/// @brief Replace all non-overlapping occurrences
/// (`s = strrep(s, oldPat, newPat)`).
///
/// Output is STRING if `s` was STRING, else CHAR.
///
/// @param s       Source. @param oldPat  Pattern to replace.
/// @param newPat  Replacement.
/// @param mr      Memory resource. @return  Modified string.
/// @see replace, regexprep
Value strrep(const Value &s, const Value &oldPat, const Value &newPat,
             std::pmr::memory_resource *mr = nullptr);

/// @brief `replace` with overlapping-match semantics
/// (`s = replace(s, old, new)`).
/// @param s       Source. @param oldPat  Pattern. @param newPat  Replacement.
/// @param mr      Memory resource. @return  Modified string. @see strrep
Value replace(const Value &s, const Value &oldPat, const Value &newPat,
              std::pmr::memory_resource *mr = nullptr);

/// @brief Remove every non-overlapping occurrence (`s = erase(s, pat)`).
/// @param s   Source. @param pat  Pattern to erase.
/// @param mr  Memory resource. @return  Modified string. @see eraseBetween
Value erase(const Value &s, const Value &pat, std::pmr::memory_resource *mr = nullptr);

/// @brief Reverse character order (`s = reverse(s)`).
/// @param s   Input string. @param mr  Memory resource.
/// @return    Reversed string.
Value reverse(const Value &s, std::pmr::memory_resource *mr = nullptr);

// ── Position-or-pattern utilities ────────────────────────────────────

/// @brief Get the substring after a position or pattern
/// (`s2 = extractAfter(s, p)`).
///
/// `p` may be a numeric scalar (1-based char index) or a CHAR / STRING
/// (first occurrence of literal pattern in `s`).
///
/// @param s   Source string. @param p   Position or pattern.
/// @param mr  Memory resource. @return  Substring.
/// @see extractBefore, extractBetween
Value extractAfter(const Value &s, const Value &p, std::pmr::memory_resource *mr = nullptr);

/// @brief Get the substring before a position or pattern
/// (`s2 = extractBefore(s, p)`).
/// @param s   Source string. @param p   Position or pattern.
/// @param mr  Memory resource. @return  Substring. @see extractAfter
Value extractBefore(const Value &s, const Value &p, std::pmr::memory_resource *mr = nullptr);

/// @brief Get the substring between two markers
/// (`s2 = extractBetween(s, start, end)`).
/// @param s     Source. @param start  Start marker. @param end  End marker.
/// @param mr    Memory resource. @return  Substring. @see eraseBetween
Value extractBetween(const Value &s, const Value &start, const Value &end,
                     std::pmr::memory_resource *mr = nullptr);

/// @brief Insert text after a position or pattern
/// (`s2 = insertAfter(s, p, newText)`).
/// @param s        Source. @param p        Position or pattern.
/// @param newText  Inserted text. @param mr  Memory resource.
/// @return         Modified string. @see insertBefore
Value insertAfter(const Value &s, const Value &p, const Value &newText,
                  std::pmr::memory_resource *mr = nullptr);

/// @brief Insert text before a position or pattern
/// (`s2 = insertBefore(s, p, newText)`).
/// @param s        Source. @param p        Position or pattern.
/// @param newText  Inserted text. @param mr  Memory resource.
/// @return         Modified string. @see insertAfter
Value insertBefore(const Value &s, const Value &p, const Value &newText,
                   std::pmr::memory_resource *mr = nullptr);

/// @brief Erase the substring between two markers
/// (`s2 = eraseBetween(s, start, end)`).
/// @param s     Source. @param start  Start marker. @param end  End marker.
/// @param mr    Memory resource. @return  Modified string. @see erase
Value eraseBetween(const Value &s, const Value &start, const Value &end,
                   std::pmr::memory_resource *mr = nullptr);

/// @brief Replace the substring between two markers
/// (`s2 = replaceBetween(s, start, end, newText)`).
/// @param s        Source. @param start  Start marker. @param end  End marker.
/// @param newText  Replacement. @param mr  Memory resource.
/// @return         Modified string. @see eraseBetween
Value replaceBetween(const Value &s, const Value &start, const Value &end,
                     const Value &newText,
                     std::pmr::memory_resource *mr = nullptr);

// ── Conversion between CHAR and STRING ───────────────────────────────

/// @brief Convert CHAR(s) / cell-of-chars to STRING
/// (`s = convertCharsToStrings(x)`).
///
/// Already-STRING inputs pass through; cells of chars become string arrays.
///
/// @param x   Input. @param mr  Memory resource. @return  STRING-typed Value.
/// @see convertStringsToChars
Value convertCharsToStrings(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Convert STRING to CHAR / cell-of-chars
/// (`s = convertStringsToChars(x)`).
///
/// Scalar STRING → CHAR row; STRING array → cell of CHAR rows.
/// Already-CHAR inputs pass through.
///
/// @param x   Input. @param mr  Memory resource. @return  CHAR / cell.
/// @see convertCharsToStrings
Value convertStringsToChars(const Value &x, std::pmr::memory_resource *mr = nullptr);

// ── Predicates ───────────────────────────────────────────────────────

/// @brief True for `1 × 1` STRING (`tf = isstringscalar(x)`).
/// @param x   Input. @param mr  Memory resource. @return  LOGICAL scalar.
Value isstringscalar(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Elementwise character classification (`tf = isstrprop(s, category)`).
///
/// Categories supported: `"alpha"`, `"digit"`, `"alphanum"`, `"lower"`,
/// `"upper"`, `"punct"`, `"space"`, `"wspace"`, `"xdigit"`, `"cntrl"`,
/// `"graphic"`, `"print"`.
///
/// @param s         Input string.
/// @param category  Category name.
/// @param mr        Memory resource.
/// @return          LOGICAL array, same shape as `s`.
/// @throws Error  Unknown category (`m:isstrprop:badCategory`).
/// @see isletter, isspaceFn
Value isstrprop(const Value &s, const Value &category, std::pmr::memory_resource *mr = nullptr);

/// @brief Elementwise letter test (`tf = isletter(s)`).
///
/// True for `[a-zA-Z]`.
///
/// @param s   Input string. @param mr  Memory resource.
/// @return    LOGICAL array, same shape as `s`. @see isstrprop
Value isletter(const Value &s, std::pmr::memory_resource *mr = nullptr);

/// @brief Elementwise whitespace test (`tf = isspace(s)`).
///
/// True for ASCII whitespace (`' '`, `'\t'`, `'\n'`, `'\r'`, `'\f'`,
/// `'\v'`). C++ name `isspaceFn` to avoid clashing with `<cctype>`.
///
/// @param s   Input string. @param mr  Memory resource.
/// @return    LOGICAL array, same shape as `s`. @see isstrprop, isletter
Value isspaceFn(const Value &s, std::pmr::memory_resource *mr = nullptr);

// ── Matrix → string and base conversions ─────────────────────────────

/// @brief Numeric matrix → matrix-literal string (`s = mat2str(A, precision)`).
///
/// E.g. `[1 2; 3 4]`. 2-D only. Scalars are unbracketed.
///
/// @param x          Input matrix.
/// @param precision  Significant digits (default 15).
/// @param mr         Memory resource.
/// @return           CHAR row representation.
/// @see num2str
Value mat2str(const Value &x, int precision = 15, std::pmr::memory_resource *mr = nullptr);

/// @brief Integer → binary CHAR row (`s = dec2bin(d, minWidth)`).
///
/// Vector input → 2-D CHAR matrix (one row per `d_i`), padded to at
/// least `minWidth` digits.
///
/// @param d         Non-negative integers.
/// @param minWidth  Minimum width (pad with leading zeros).
/// @param mr        Memory resource.
/// @return          CHAR row / matrix.
/// @see bin2dec, dec2hex
Value dec2bin(const Value &d, int minWidth, std::pmr::memory_resource *mr = nullptr);

/// @brief Integer → uppercase hex CHAR row (`s = dec2hex(d, minWidth)`).
/// @param d         Non-negative integers.
/// @param minWidth  Minimum width.
/// @param mr        Memory resource. @return  CHAR row / matrix.
/// @see hex2dec, dec2bin
Value dec2hex(const Value &d, int minWidth, std::pmr::memory_resource *mr = nullptr);

/// @brief Parse binary digit string (`d = bin2dec(s)`).
/// @param s   Binary digit string. @param mr  Memory resource.
/// @return    DOUBLE value. @see dec2bin
Value bin2dec(const Value &s, std::pmr::memory_resource *mr = nullptr);

/// @brief Parse hex string (`d = hex2dec(s)`).
///
/// Case-insensitive, no `'0x'` prefix.
///
/// @param s   Hex digit string. @param mr  Memory resource.
/// @return    DOUBLE value. @see dec2hex
Value hex2dec(const Value &s, std::pmr::memory_resource *mr = nullptr);

/// @brief IEEE-754 hex → double (`x = hex2num(hexStr)`).
///
/// Interprets the hexadecimal text as the raw IEEE-754 *bit pattern* of a
/// double (NOT its decimal digits — use @ref hex2dec for that). A short
/// string is right-padded with `'0'` to 16 digits, so `hex2num('4')` is
/// `hex2num('4000000000000000')` = 2. A char MATRIX with N rows yields an
/// N×1 double column; a cellstr / string array yields a same-shape double.
/// Round-trips `Inf`/`-Inf`/`NaN`.
///
/// @param s   Hex text (char array, cellstr, or string array).
/// @param mr  Memory resource. @return DOUBLE. @see num2hex, hex2dec
Value hex2num(const Value &s, std::pmr::memory_resource *mr = nullptr);

/// @brief Double / single → IEEE-754 hex (`hexStr = num2hex(x)`).
///
/// Returns the raw IEEE-754 bit pattern as lowercase hex: 16 digits for a
/// double, 8 for a single. A vector / matrix yields a `numel × W` CHAR
/// matrix with one row per element (column-major order). The input must be
/// floating point — integer / logical input is an error.
///
/// @param x   single or double value(s).
/// @param mr  Memory resource. @return CHAR row / matrix. @see hex2num
Value num2hex(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Integer → base-`base` CHAR string (`s = dec2base(d, base[, len])`).
///
/// `base` in 2..36 (digits 0-9 then A-Z). Output left-padded with '0' to
/// at least `minWidth`. A vector `d` yields a CHAR matrix, one row per
/// element padded to the widest.
///
/// @param d         Non-negative integer value(s).
/// @param base      Radix, 2..36.
/// @param minWidth  Minimum field width (0 = natural).
/// @param mr        Memory resource (nullptr → process default).
/// @return          CHAR row / matrix. @see base2dec, dec2bin, dec2hex
Value dec2base(const Value &d, int base, int minWidth, std::pmr::memory_resource *mr = nullptr);

/// @brief Parse base-`base` digit string (`d = base2dec(s, base)`).
///
/// Inverse of `dec2base`; `base` in 2..36, digits case-insensitive. A char
/// matrix parses each row → column vector.
///
/// @param s     Digit string / char matrix.
/// @param base  Radix, 2..36.
/// @param mr    Memory resource (nullptr → process default).
/// @return      DOUBLE value / column vector. @see dec2base
Value base2dec(const Value &s, int base, std::pmr::memory_resource *mr = nullptr);

// ── Rational approximation ───────────────────────────────────────────

/// @brief Continued-fraction approximation (`s = rat(x, tol)`).
///
/// Returns a CHAR string of the form `"n / d"` (the single-output
/// `rat` form).
///
/// @param x    Input scalar.
/// @param tol  Tolerance (relative).
/// @param mr   Memory resource.
/// @return     CHAR row `"n / d"`.
/// @see rats
Value rat(const Value &x, double tol, std::pmr::memory_resource *mr = nullptr);

/// @brief Padded rational approximation (`s = rats(x, len)`).
///
/// Same as @ref rat but pads to fixed width `len`.
///
/// @param x    Input scalar.
/// @param len  Target field width.
/// @param mr   Memory resource.
/// @return     CHAR row of width `len`.
/// @see rat
Value rats(const Value &x, int len, std::pmr::memory_resource *mr = nullptr);

// ── Special / constructors ───────────────────────────────────────────

/// @brief Newline character (`s = newline`).
///
/// ASCII LF as a `1 × 1` CHAR. Equivalent to `char(10)` / `sprintf('\n')`.
///
/// @param mr  Memory resource (used for the result Value).
/// @return    `1 × 1` CHAR.
Value newlineFn(std::pmr::memory_resource *mr = nullptr);

/// @brief Empty-string array of given shape (`s = strings(dims)`).
///
/// Same dim-arg conventions as `zeros` / `cell`. `dims` may have
/// 0..N entries; rank < 2 is normalised to `{n, n}` or `{1, 1}`.
///
/// @param dims  Shape vector.
/// @param mr    Memory resource.
/// @return      STRING array of given shape, every element `""`.
/// @see toString
Value stringsND(Span<const size_t> dims, std::pmr::memory_resource *mr = nullptr);

/// @brief Apply `sprintf` per element (`c = compose(fmt, x)`).
///
/// Applies the single-spec `fmt` once per element of `x`, returning a
/// same-shaped cell of CHAR rows. Multi-spec / multi-column formatting
/// is not yet supported.
///
/// @param fmt  Format string (must contain one `%`-spec).
/// @param x    Input array.
/// @param mr   Memory resource.
/// @return     Cell of CHAR rows, same shape as `x`.
/// @see sprintf
Value compose(const Value &fmt, const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Justify each row of a char matrix (`M = strjust(M, side)`).
///
/// `side ∈ {"right" (default), "left", "center"}`.
///
/// @param M     Input CHAR matrix.
/// @param side  Justification mode.
/// @param mr    Memory resource.
/// @return      Justified matrix.
/// @throws Error  Unknown `side` (`m:strjust:badSide`).
/// @see pad
Value strjust(const Value &M, const std::string &side, std::pmr::memory_resource *mr = nullptr);

/// @brief Extract every literal match (`c = extract(s, pat)`).
///
/// Returns a `K × 1` cell column of matched substrings (empty `0 × 0`
/// if no matches). Pattern objects are not supported.
///
/// @param s    Source string.
/// @param pat  Pattern (CHAR / STRING).
/// @param mr   Memory resource.
/// @return     Cell column of matches.
/// @see strfind, regexpFind
Value extract(const Value &s, const Value &pat, std::pmr::memory_resource *mr = nullptr);

/// @brief First token + remainder (`[tok, rem] = strtok(str, delim)`).
///
/// Skips leading delimiter characters, then returns the first run of
/// non-delimiter characters as `tok`; `rem` is the rest of `str` starting
/// at the delimiter that ended the token (empty if the token reaches the
/// end). `delim` defaults to the whitespace set `" \t\r\n\f\v"`.
///
/// @param str    Source string (CHAR row / STRING scalar).
/// @param delim  Delimiter character set (default whitespace).
/// @param mr     Memory resource (nullptr → process default).
/// @return       Pair `{ tok, rem }` (both CHAR results).
/// @see split, extractBefore
std::pair<Value, Value> strtok(const Value &str,
                               const std::string &delim = " \t\r\n\f\v",
                               std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::builtin
