// libs/builtin/include/numkit/builtin/datatypes/strings/regex.hpp
#pragma once

#include <numkit/core/allocator.hpp>
#include <numkit/core/value.hpp>

#include <string>

namespace numkit::builtin {

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
Value regexpFind(Allocator &alloc, const Value &s, const Value &pat,
                  const std::string &option = "", bool ignoreCase = false);

Value regexprep(Allocator &alloc, const Value &s, const Value &pat,
                 const Value &rep, bool ignoreCase = false);

} // namespace numkit::builtin
