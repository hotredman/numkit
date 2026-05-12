// libs/builtin/include/numkit/builtin/language/strings/strings.hpp
#pragma once

#include <memory_resource>
#include <numkit/core/span.hpp>
#include <numkit/core/value.hpp>

namespace numkit::builtin {

//// num2str(x) — scalar number → char array (5 significant digits).
Value num2str(const Value &x, std::pmr::memory_resource *mr = nullptr);
/// num2str(x, N) — N significant digits if N is an integer.
/// num2str(x, fmt) — printf-style format string.
Value num2str(const Value &x, const Value &spec, std::pmr::memory_resource *mr = nullptr);

/// str2num(s) — parse string as a number; returns empty Value on failure.
Value str2num(const Value &s, std::pmr::memory_resource *mr = nullptr);

/// str2double(s) — parse string as a number; returns NaN on failure.
Value str2double(const Value &s, std::pmr::memory_resource *mr = nullptr);

/// string(x) — convert numeric/char/logical to MATLAB string type.
/// Named `toString` in C++ to avoid `std::string` lookup ambiguity.
Value toString(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// char(x) — convert numeric/string to char array. Named `toChar`
/// in C++ because `char` is a keyword.
Value toChar(const Value &x, std::pmr::memory_resource *mr = nullptr);

//// strcmp(a, b) — equal-strings test (case-sensitive). Logical scalar.
Value strcmp(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);

/// strcmpi(a, b) — equal-strings test (case-insensitive, ASCII).
Value strcmpi(const Value &a, const Value &b, std::pmr::memory_resource *mr = nullptr);

//// upper(s) — ASCII uppercase.
Value upper(const Value &s, std::pmr::memory_resource *mr = nullptr);

/// lower(s) — ASCII lowercase.
Value lower(const Value &s, std::pmr::memory_resource *mr = nullptr);

//// strtrim(s) — strip leading/trailing whitespace (space/tab/CR/LF).
Value strtrim(const Value &s, std::pmr::memory_resource *mr = nullptr);

/// strsplit(s) — split on whitespace (default delim = space).
Value strsplit(const Value &s, std::pmr::memory_resource *mr = nullptr);

/// strsplit(s, delim) — split on first char of delim. Empty tokens dropped.
/// Returns a 1×N cell of char arrays.
Value strsplit(const Value &s, const Value &delim, std::pmr::memory_resource *mr = nullptr);

/// strcat(parts) — concatenate N strings/char arrays into one char array.
Value strcat(Span<const Value> parts, std::pmr::memory_resource *mr = nullptr);

//// strlength(s) — length of each string (elementwise for string array).
Value strlength(const Value &s, std::pmr::memory_resource *mr = nullptr);

//// strrep(s, oldPat, newPat) — replace all non-overlapping occurrences.
//// Output is string-typed if s was string-typed, else char.
Value strrep(const Value &s, const Value &oldPat, const Value &newPat, std::pmr::memory_resource *mr = nullptr);

/// contains(s, pat) — logical scalar: does s contain pat as substring?
Value contains(const Value &s, const Value &pat, std::pmr::memory_resource *mr = nullptr);

/// startsWith(s, prefix) — logical scalar.
Value startsWith(const Value &s, const Value &prefix, std::pmr::memory_resource *mr = nullptr);

/// endsWith(s, suffix) — logical scalar.
Value endsWith(const Value &s, const Value &suffix, std::pmr::memory_resource *mr = nullptr);

//// strncmp(a, b, n) — first n chars equal, case-sensitive. Logical scalar.
Value strncmp(const Value &a, const Value &b, size_t n, std::pmr::memory_resource *mr = nullptr);
/// strncmpi(a, b, n) — first n chars equal, case-insensitive ASCII.
Value strncmpi(const Value &a, const Value &b, size_t n, std::pmr::memory_resource *mr = nullptr);
/// strfind(s, pat) — 1-based positions of all non-overlapping pat
/// occurrences in s. Returns 1×K row vector or empty (0×0).
Value strfind(const Value &s, const Value &pat, std::pmr::memory_resource *mr = nullptr);
/// blanks(n) — char row of n spaces.
Value blanks(size_t n, std::pmr::memory_resource *mr = nullptr);
/// deblank(s) — strip trailing whitespace only.
Value deblank(const Value &s, std::pmr::memory_resource *mr = nullptr);
/// mat2str(A) — convert numeric matrix to a parseable MATLAB-syntax
/// string, e.g. "[1 2;3 4]". 2-D only; vectors don't get the surrounding
/// brackets when they are scalar.
Value mat2str(const Value &x, int precision = 15, std::pmr::memory_resource *mr = nullptr);
/// strjoin(c, delim?) — join a 1-D cell of strings with `delim` (default
/// space). Returns a single char row.
Value strjoin(const Value &c, const Value &delim = Value::Empty, std::pmr::memory_resource *mr = nullptr);

//// append(s1, s2, ...) — concatenate strings, preserving trailing
//// whitespace (unlike strcat).
Value append(Span<const Value> parts, std::pmr::memory_resource *mr = nullptr);
/// count(s, pat) — number of non-overlapping occurrences of pat in s.
Value count(const Value &s, const Value &pat, std::pmr::memory_resource *mr = nullptr);
/// erase(s, pat) — return s with every non-overlapping pat removed.
Value erase(const Value &s, const Value &pat, std::pmr::memory_resource *mr = nullptr);
/// replace(s, old, new) — alias for strrep with overlapping-match
/// semantics matching MATLAB's `replace`.
Value replace(const Value &s, const Value &oldPat, const Value &newPat, std::pmr::memory_resource *mr = nullptr);
/// reverse(s) — reverse character order.
Value reverse(const Value &s, std::pmr::memory_resource *mr = nullptr);
/// splitlines(s) — split on CRLF / LF / CR; returns N×1 cell. Trailing
/// newline does not introduce a final empty token.
Value splitlines(const Value &s, std::pmr::memory_resource *mr = nullptr);
/// pad(s, n[, side[, padChar]]) — pad to length n. `side` ∈
/// {"right","left","both"}, default "right". `padChar` default ' '.
Value pad(const Value &s, size_t n, const Value &side = Value::Empty, const Value &padChar = Value::Empty, std::pmr::memory_resource *mr = nullptr);
/// strip(s[, side[, ch]]) — strip whitespace (or `ch`). side ∈
/// {"both","left","right"}, default "both".
Value strip(const Value &s, const Value &side = Value::Empty, const Value &ch = Value::Empty, std::pmr::memory_resource *mr = nullptr);
/// matches(s, pat) — logical: s exactly equals pat. For pat a cell
/// of strings, true iff s equals any element of pat.
Value matches(const Value &s, const Value &pat, std::pmr::memory_resource *mr = nullptr);

//// convertCharsToStrings(x) — char array → string scalar. Already-string
//// inputs pass through unchanged; cells of chars become string arrays.
Value convertCharsToStrings(const Value &x, std::pmr::memory_resource *mr = nullptr);
/// convertStringsToChars(x) — string → char row. Already-char inputs
/// pass through. String array → cell of char rows.
Value convertStringsToChars(const Value &x, std::pmr::memory_resource *mr = nullptr);
/// isstringscalar(x) — true iff x is a 1×1 string array.
Value isstringscalar(const Value &x, std::pmr::memory_resource *mr = nullptr);
/// isstrprop(s, category) — elementwise classification.
/// Categories supported: 'alpha', 'digit', 'alphanum', 'lower', 'upper',
/// 'punct', 'space', 'wspace', 'xdigit', 'cntrl', 'graphic', 'print'.
Value isstrprop(const Value &s, const Value &category, std::pmr::memory_resource *mr = nullptr);
/// isletter(s) — true for [a-zA-Z]. Same shape as s.
Value isletter(const Value &s, std::pmr::memory_resource *mr = nullptr);
/// isspace(s) — true for ASCII whitespace (' ', '\t', '\n', '\r', '\f', '\v').
Value isspaceFn(const Value &s, std::pmr::memory_resource *mr = nullptr);

/// Each function accepts a position-or-pattern argument:
///   numeric scalar  — 1-based character index
///   char/string     — first occurrence of the literal pattern in s
/// `Between` variants take two such arguments.

Value extractAfter(const Value &s, const Value &p, std::pmr::memory_resource *mr = nullptr);
Value extractBefore(const Value &s, const Value &p, std::pmr::memory_resource *mr = nullptr);
Value extractBetween(const Value &s, const Value &start, const Value &end, std::pmr::memory_resource *mr = nullptr);
Value insertAfter(const Value &s, const Value &p, const Value &newText, std::pmr::memory_resource *mr = nullptr);
Value insertBefore(const Value &s, const Value &p, const Value &newText, std::pmr::memory_resource *mr = nullptr);
Value eraseBetween(const Value &s, const Value &start, const Value &end, std::pmr::memory_resource *mr = nullptr);
Value replaceBetween(const Value &s, const Value &start, const Value &end, const Value &newText, std::pmr::memory_resource *mr = nullptr);

//// dec2bin(d[, n]) — non-negative integer → binary char row, padded to
//// at least n digits. Vector input → 2-D char matrix (one row per d_i).
Value dec2bin(const Value &d, int minWidth, std::pmr::memory_resource *mr = nullptr);
/// dec2hex(d[, n]) — uppercase hex char row, padded to ≥ n.
Value dec2hex(const Value &d, int minWidth, std::pmr::memory_resource *mr = nullptr);
/// bin2dec(s) — parse binary digit string → double.
Value bin2dec(const Value &s, std::pmr::memory_resource *mr = nullptr);
/// hex2dec(s) — parse hex (case-insensitive, no '0x') → double.
Value hex2dec(const Value &s, std::pmr::memory_resource *mr = nullptr);

/// rat(x[, tol]) — continued-fraction approximation. Returns a string
/// of the form "n / d" (matches MATLAB's "rat" with one output).
Value rat(const Value &x, double tol, std::pmr::memory_resource *mr = nullptr);
/// rats(x[, len]) — same as rat but pads to fixed width `len`.
Value rats(const Value &x, int len, std::pmr::memory_resource *mr = nullptr);

//// newline — ASCII LF as a 1×1 char. Equivalent to `char(10)` /
//// `sprintf('\n')`. Takes no input; the `mr` argument is just for the
//// allocator-passing convention.
Value newlineFn(std::pmr::memory_resource *mr = nullptr);

/// strings(d1, d2, ...) — string array of given shape, every element "".
/// Same dim-arg conventions as `zeros` / `cell`: scalar `n` → n×n,
/// `(m,n)` → m×n, `(m,n,p)` → m×n×p, single vector arg → its elements.
/// `dims` may have 0..N entries; ndim<2 is normalized to {n,n} or {1,1}.
Value stringsND(const size_t *dims, size_t ndim, std::pmr::memory_resource *mr = nullptr);

/// compose(fmt, x) — apply sprintf-style `fmt` to each element of `x`,
/// returning a same-shaped cell of char arrays. fmt is consumed once
/// per element (single-spec broadcast). Multi-spec / multi-column
/// formatting is currently not supported.
Value compose(const Value &fmt, const Value &x, std::pmr::memory_resource *mr = nullptr);

/// strjust(M, side) — justify each row of char matrix `M`. `side`
/// is "right" (default), "left", or "center".
Value strjust(const Value &M, const std::string &side, std::pmr::memory_resource *mr = nullptr);

/// extract(s, pat) — every non-overlapping literal occurrence of `pat`
/// in `s`. Returns a K×1 cell column of matched substrings (empty
/// 0×0 cell if no matches). MATLAB Pattern objects are not supported.
Value extract(const Value &s, const Value &pat, std::pmr::memory_resource *mr = nullptr);

/// split(s, delim) — split `s` on every occurrence of `delim` (single
/// char or string). Returns N×1 cell column. Empty tokens are kept
/// (matching MATLAB; this differs from strsplit, which drops them).
Value split(const Value &s, const Value &delim, std::pmr::memory_resource *mr = nullptr);

/// join(arr, delim) — concatenate elements of string array `arr`
/// separated by `delim`. 2-D arrays are joined along columns,
/// producing one row per source row (N×1). Default delim is ' '.
Value join(const Value &arr, const Value &delim = Value::Empty, std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::builtin
