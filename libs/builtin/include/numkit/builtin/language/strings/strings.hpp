// libs/builtin/include/numkit/builtin/language/strings/strings.hpp
#pragma once

#include <memory_resource>
#include <numkit/core/span.hpp>
#include <numkit/core/value.hpp>

#include <string>

namespace numkit::builtin {

/// @file
/// @brief MATLAB-parity string and char builtins.
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

/// @brief Number → CHAR row with precision / format spec
/// (`s = num2str(x, spec)`).
/// @param x     Scalar / matrix to format.
/// @param spec  Integer scalar `N` → N significant digits;
///              CHAR / STRING → printf-style format.
/// @param mr    Memory resource (nullptr → process default).
/// @return      CHAR formatted string.
/// @see num2str(x, mr)
Value num2str(const Value &x, const Value &spec, std::pmr::memory_resource *mr = nullptr);

/// @brief Parse string as number (`x = str2num(s)`).
///
/// Returns empty Value on parse failure (matches MATLAB).
///
/// @param s   CHAR / STRING input.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Numeric value or empty.
/// @see str2double
Value str2num(const Value &s, std::pmr::memory_resource *mr = nullptr);

/// @brief Parse string as scalar number (`x = str2double(s)`).
///
/// Returns `NaN` on parse failure (matches MATLAB).
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
/// Empty tokens KEPT (matches MATLAB; differs from `strsplit`).
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
/// MATLAB convention: `strcat` strips trailing whitespace from each
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
/// @param s   Source. @param pat  Pattern.
/// @param mr  Memory resource. @return  LOGICAL scalar. @see strfind
Value contains(const Value &s, const Value &pat, std::pmr::memory_resource *mr = nullptr);

/// @brief Prefix test (`tf = startsWith(s, prefix)`).
/// @param s    Source. @param prefix  Prefix.
/// @param mr   Memory resource. @return  LOGICAL scalar. @see endsWith
Value startsWith(const Value &s, const Value &prefix, std::pmr::memory_resource *mr = nullptr);

/// @brief Suffix test (`tf = endsWith(s, suffix)`).
/// @param s    Source. @param suffix  Suffix.
/// @param mr   Memory resource. @return  LOGICAL scalar. @see startsWith
Value endsWith(const Value &s, const Value &suffix, std::pmr::memory_resource *mr = nullptr);

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

/// @brief MATLAB `replace` with overlapping-match semantics
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

/// @brief Numeric matrix → MATLAB-syntax string (`s = mat2str(A, precision)`).
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

// ── Rational approximation ───────────────────────────────────────────

/// @brief Continued-fraction approximation (`s = rat(x, tol)`).
///
/// Returns a CHAR string of the form `"n / d"` (matches MATLAB's `rat`
/// with one output).
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
/// if no matches). MATLAB Pattern objects are not supported.
///
/// @param s    Source string.
/// @param pat  Pattern (CHAR / STRING).
/// @param mr   Memory resource.
/// @return     Cell column of matches.
/// @see strfind, regexpFind
Value extract(const Value &s, const Value &pat, std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::builtin
