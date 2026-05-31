// libs/builtin/include/numkit/builtin/language/strings/regex.hpp
#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

#include <string>

namespace numkit::builtin {

/// @brief `regexp` / `regexpi` (`y = regexpFind(s, pat, option, ignoreCase)`).
///
/// Find non-overlapping matches of `pat` in `s`. Output shape depends
/// on `option`:
/// - `""` (default)  → row vector of 1-based start indices
/// - `"match"`       → `1 × N` cell of matched substrings
/// - `"tokens"`      → `1 × N` cell of cell rows, each holding the
///                     capture groups for one match
/// - `"split"`       → `1 × (N+1)` cell of substrings between matches
///
/// @param s           Source string (CHAR / STRING).
/// @param pat         Regex pattern (CHAR / STRING).
/// @param option      Output mode (see above).
/// @param ignoreCase  When `true`, behaves like `regexpi`.
/// @param mr          Memory resource (nullptr → process default).
/// @return            Output Value per the selected mode.
/// @see regexprep, regexptranslate
Value regexpFind(const Value &s, const Value &pat,
                 const std::string &option = "", bool ignoreCase = false,
                 std::pmr::memory_resource *mr = nullptr);

/// @brief `regexprep` (`y = regexprep(s, pat, rep, ignoreCase)`).
///
/// Substitutes every non-overlapping match of `pat` in `s` with `rep`.
/// `rep` may use `$1` / `$2` / … ECMAScript back-references.
///
/// @param s           Source string.
/// @param pat         Regex pattern.
/// @param rep         Replacement template.
/// @param ignoreCase  When `true`, case-insensitive matching.
/// @param once        When `true` (MATLAB 'once'), replace only the FIRST
///                    match of each pattern instead of all of them.
/// @param mr          Memory resource (nullptr → process default).
/// @return            Substituted string.
/// @see regexpFind
Value regexprep(const Value &s, const Value &pat, const Value &rep,
                bool ignoreCase = false, bool once = false,
                std::pmr::memory_resource *mr = nullptr);

/// @brief `regexptranslate` (`y = regexptranslate(op, s)`).
///
/// Supported `op`:
/// - `"escape"`   → escape regex metacharacters with `\`
/// - `"wildcard"` → translate glob wildcards to a regex
///                  (`*` → `.*`, `?` → `.`, escape rest)
///
/// `"compose"` / `"flexible"` are not implemented (NaN-string case +
/// multi-arg).
///
/// @param op  Translation mode (see above).
/// @param s   Input string.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Translated regex pattern.
/// @throws Error  Unknown `op` (`m:regexptranslate:badOp`).
/// @see regexpFind
Value regexptranslate(const std::string &op, const std::string &s,
                      std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::builtin
