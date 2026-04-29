// libs/builtin/include/numkit/builtin/datatypes/strings/strings.hpp
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

} // namespace numkit::builtin
