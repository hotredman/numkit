// libs/builtin/include/numkit/m/builtin/MStdStrings.hpp
#pragma once

#include <numkit/m/core/MAllocator.hpp>
#include <numkit/m/core/MSpan.hpp>
#include <numkit/m/core/MValue.hpp>

#include <string>

namespace numkit::m::builtin {

// ── Conversion ───────────────────────────────────────────────────────
/// num2str(x) — scalar number → char array (decimal, default formatting).
MValue num2str(Allocator &alloc, const MValue &x);

/// str2num(s) — parse string as a number; returns empty MValue on failure.
MValue str2num(Allocator &alloc, const MValue &s);

/// str2double(s) — parse string as a number; returns NaN on failure.
MValue str2double(Allocator &alloc, const MValue &s);

/// string(x) — convert numeric/char/logical to MATLAB string type.
/// Named `toString` in C++ to avoid `std::string` lookup ambiguity.
MValue toString(Allocator &alloc, const MValue &x);

/// char(x) — convert numeric/string to char array. Named `toChar`
/// in C++ because `char` is a keyword.
MValue toChar(Allocator &alloc, const MValue &x);

// ── Comparisons ──────────────────────────────────────────────────────
/// strcmp(a, b) — equal-strings test (case-sensitive). Logical scalar.
MValue strcmp(Allocator &alloc, const MValue &a, const MValue &b);

/// strcmpi(a, b) — equal-strings test (case-insensitive, ASCII).
MValue strcmpi(Allocator &alloc, const MValue &a, const MValue &b);

// ── Case transforms ──────────────────────────────────────────────────
/// upper(s) — ASCII uppercase.
MValue upper(Allocator &alloc, const MValue &s);

/// lower(s) — ASCII lowercase.
MValue lower(Allocator &alloc, const MValue &s);

// ── Trim / split / concat ────────────────────────────────────────────
/// strtrim(s) — strip leading/trailing whitespace (space/tab/CR/LF).
MValue strtrim(Allocator &alloc, const MValue &s);

/// strsplit(s) — split on whitespace (default delim = space).
MValue strsplit(Allocator &alloc, const MValue &s);

/// strsplit(s, delim) — split on first char of delim. Empty tokens dropped.
/// Returns a 1×N cell of char arrays.
MValue strsplit(Allocator &alloc, const MValue &s, const MValue &delim);

/// strcat(parts) — concatenate N strings/char arrays into one char array.
MValue strcat(Allocator &alloc, Span<const MValue> parts);

// ── Length ───────────────────────────────────────────────────────────
/// strlength(s) — length of each string (elementwise for string array).
MValue strlength(Allocator &alloc, const MValue &s);

// ── Search / replace ─────────────────────────────────────────────────
/// strrep(s, oldPat, newPat) — replace all non-overlapping occurrences.
/// Output is string-typed if s was string-typed, else char.
MValue strrep(Allocator &alloc, const MValue &s, const MValue &oldPat, const MValue &newPat);

/// contains(s, pat) — logical scalar: does s contain pat as substring?
MValue contains(Allocator &alloc, const MValue &s, const MValue &pat);

/// startsWith(s, prefix) — logical scalar.
MValue startsWith(Allocator &alloc, const MValue &s, const MValue &prefix);

/// endsWith(s, suffix) — logical scalar.
MValue endsWith(Allocator &alloc, const MValue &s, const MValue &suffix);

// ── Regex (ECMAScript syntax via std::regex) ─────────────────────────
//
// regexp(str, pat[, opt])
//   No opt           → row vector of 1-based start indices.
//   "match"          → 1×N cell of matched substrings.
//   "tokens"         → 1×N cell of cell rows, each holding the capture
//                      groups for one match.
//   "split"          → 1×(N+1) cell of substrings between matches.
// regexpi: same, but case-insensitive (set ignoreCase=true).
//
// regexprep(str, pat, rep) — substitute every non-overlapping match.
// `rep` may use `$1`/`$2`/... back-references (ECMAScript syntax).
MValue regexpFind(Allocator &alloc, const MValue &s, const MValue &pat,
                  const std::string &option = "", bool ignoreCase = false);
MValue regexprep(Allocator &alloc, const MValue &s, const MValue &pat,
                 const MValue &rep, bool ignoreCase = false);

} // namespace numkit::m::builtin
