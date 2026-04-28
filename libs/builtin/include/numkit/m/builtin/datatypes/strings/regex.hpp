// libs/builtin/include/numkit/m/builtin/datatypes/strings/regex.hpp
#pragma once

#include <numkit/m/core/MAllocator.hpp>
#include <numkit/m/core/MValue.hpp>

#include <string>

namespace numkit::m::builtin {

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
