// libs/builtin/include/numkit/builtin/language/strings/strings.hpp
#pragma once

#include <memory_resource>
#include <numkit/core/span.hpp>
#include <numkit/core/value.hpp>

namespace numkit::builtin {

// ── Conversion ───────────────────────────────────────────────────────
/// num2str(x) — scalar number → char array (decimal, default formatting).
Value num2str(std::pmr::memory_resource *mr, const Value &x);

/// str2num(s) — parse string as a number; returns empty Value on failure.
Value str2num(std::pmr::memory_resource *mr, const Value &s);

/// str2double(s) — parse string as a number; returns NaN on failure.
Value str2double(std::pmr::memory_resource *mr, const Value &s);

/// string(x) — convert numeric/char/logical to MATLAB string type.
/// Named `toString` in C++ to avoid `std::string` lookup ambiguity.
Value toString(std::pmr::memory_resource *mr, const Value &x);

/// char(x) — convert numeric/string to char array. Named `toChar`
/// in C++ because `char` is a keyword.
Value toChar(std::pmr::memory_resource *mr, const Value &x);

// ── Comparisons ──────────────────────────────────────────────────────
/// strcmp(a, b) — equal-strings test (case-sensitive). Logical scalar.
Value strcmp(std::pmr::memory_resource *mr, const Value &a, const Value &b);

/// strcmpi(a, b) — equal-strings test (case-insensitive, ASCII).
Value strcmpi(std::pmr::memory_resource *mr, const Value &a, const Value &b);

// ── Case transforms ──────────────────────────────────────────────────
/// upper(s) — ASCII uppercase.
Value upper(std::pmr::memory_resource *mr, const Value &s);

/// lower(s) — ASCII lowercase.
Value lower(std::pmr::memory_resource *mr, const Value &s);

// ── Trim / split / concat ────────────────────────────────────────────
/// strtrim(s) — strip leading/trailing whitespace (space/tab/CR/LF).
Value strtrim(std::pmr::memory_resource *mr, const Value &s);

/// strsplit(s) — split on whitespace (default delim = space).
Value strsplit(std::pmr::memory_resource *mr, const Value &s);

/// strsplit(s, delim) — split on first char of delim. Empty tokens dropped.
/// Returns a 1×N cell of char arrays.
Value strsplit(std::pmr::memory_resource *mr, const Value &s, const Value &delim);

/// strcat(parts) — concatenate N strings/char arrays into one char array.
Value strcat(std::pmr::memory_resource *mr, Span<const Value> parts);

// ── Length ───────────────────────────────────────────────────────────
/// strlength(s) — length of each string (elementwise for string array).
Value strlength(std::pmr::memory_resource *mr, const Value &s);

// ── Search / replace ─────────────────────────────────────────────────
/// strrep(s, oldPat, newPat) — replace all non-overlapping occurrences.
/// Output is string-typed if s was string-typed, else char.
Value strrep(std::pmr::memory_resource *mr, const Value &s, const Value &oldPat, const Value &newPat);

/// contains(s, pat) — logical scalar: does s contain pat as substring?
Value contains(std::pmr::memory_resource *mr, const Value &s, const Value &pat);

/// startsWith(s, prefix) — logical scalar.
Value startsWith(std::pmr::memory_resource *mr, const Value &s, const Value &prefix);

/// endsWith(s, suffix) — logical scalar.
Value endsWith(std::pmr::memory_resource *mr, const Value &s, const Value &suffix);

// ── Pack 10: extra string utilities ──────────────────────────────────
/// strncmp(a, b, n) — first n chars equal, case-sensitive. Logical scalar.
Value strncmp(std::pmr::memory_resource *mr, const Value &a, const Value &b, size_t n);
/// strncmpi(a, b, n) — first n chars equal, case-insensitive ASCII.
Value strncmpi(std::pmr::memory_resource *mr, const Value &a, const Value &b, size_t n);
/// strfind(s, pat) — 1-based positions of all non-overlapping pat
/// occurrences in s. Returns 1×K row vector or empty (0×0).
Value strfind(std::pmr::memory_resource *mr, const Value &s, const Value &pat);
/// blanks(n) — char row of n spaces.
Value blanks(std::pmr::memory_resource *mr, size_t n);
/// deblank(s) — strip trailing whitespace only.
Value deblank(std::pmr::memory_resource *mr, const Value &s);
/// mat2str(A) — convert numeric matrix to a parseable MATLAB-syntax
/// string, e.g. "[1 2;3 4]". 2-D only; vectors don't get the surrounding
/// brackets when they are scalar.
Value mat2str(std::pmr::memory_resource *mr, const Value &x, int precision = 15);
/// strjoin(c, delim?) — join a 1-D cell of strings with `delim` (default
/// space). Returns a single char row.
Value strjoin(std::pmr::memory_resource *mr, const Value &c, const Value *delim = nullptr);

// ── Pack 18: extra string utilities ──────────────────────────────────
/// append(s1, s2, ...) — concatenate strings, preserving trailing
/// whitespace (unlike strcat).
Value append(std::pmr::memory_resource *mr, Span<const Value> parts);
/// count(s, pat) — number of non-overlapping occurrences of pat in s.
Value count(std::pmr::memory_resource *mr, const Value &s, const Value &pat);
/// erase(s, pat) — return s with every non-overlapping pat removed.
Value erase(std::pmr::memory_resource *mr, const Value &s, const Value &pat);
/// replace(s, old, new) — alias for strrep with overlapping-match
/// semantics matching MATLAB's `replace`.
Value replace(std::pmr::memory_resource *mr, const Value &s,
              const Value &oldPat, const Value &newPat);
/// reverse(s) — reverse character order.
Value reverse(std::pmr::memory_resource *mr, const Value &s);
/// splitlines(s) — split on CRLF / LF / CR; returns N×1 cell. Trailing
/// newline does not introduce a final empty token.
Value splitlines(std::pmr::memory_resource *mr, const Value &s);
/// pad(s, n[, side[, padChar]]) — pad to length n. `side` ∈
/// {"right","left","both"}, default "right". `padChar` default ' '.
Value pad(std::pmr::memory_resource *mr, const Value &s, size_t n,
          const Value *side = nullptr, const Value *padChar = nullptr);
/// strip(s[, side[, ch]]) — strip whitespace (or `ch`). side ∈
/// {"both","left","right"}, default "both".
Value strip(std::pmr::memory_resource *mr, const Value &s,
            const Value *side = nullptr, const Value *ch = nullptr);
/// matches(s, pat) — logical: s exactly equals pat. For pat a cell
/// of strings, true iff s equals any element of pat.
Value matches(std::pmr::memory_resource *mr, const Value &s, const Value &pat);

} // namespace numkit::builtin
