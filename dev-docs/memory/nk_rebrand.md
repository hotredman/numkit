# nk rebrand — how it went and what users must know

Executed 2026-09-12 as ONE atomic commit per the plan in (now-deleted)
todo nk_rebrand.md: numkit → nk everywhere.

- Names now: namespace `nk::`, includes `<nk/...>`, binaries
  `nk_repl` / `nk_gtest` / `nk_codegen`, CMake targets `nk_*`,
  env vars `NK_FS` / `NK_CXX` / `NK_FUSE` (breaking for any user
  scripts that set NUMKIT_* — they must rename), npm package `nk`
  (`npx nk`), wasm artifact `nk_ide.js/.wasm`.
- Mechanics: 318 git-mv renames (24 include dirs, apps/nk,
  apps/nk_codegen, packages/nk + nk-mcp, brand assets, cmake helpers
  incl. NumkitOptions.cmake → NKOptions.cmake, dev-docs/todo file)
  + byte-level content sweep over 2 622 tracked files (~21.4k
  replacements, ordered rules NUMKIT→NK, NumKit/Numkit→NK,
  numkit::→nk::, numkit_→nk_, numkit→nk). Fresh CMake configure
  required (build dir wiped — stale caches reference dead targets).
- Verification: full Release suite 13381/0, WASM (nk_ide.js linked),
  web bundle, frontend 567/567, CLI smoke through the renamed binary
  path, parity spot-check OK, bugs_audit clean, grep -rniE numkit
  over tracked files = ZERO.
- One test casualty of the sweep: a `contains('Numkit Engine','kit')`
  sample string lost its 'kit' substring — sample data must survive
  brand renames semantically (now 'NK toolkit').
- Windows gotcha: one include dir refused `git mv` with "Permission
  denied" (transient FS handle); copy + git rm + add achieved the
  same rename — if a rename fails on Windows, retry, then copy-delete.
- USER-SIDE PENDING: rename the origin repo (git.megahard.ru) and the
  public mirror to `nk`, republish npm as `nk`. Git HISTORY still
  says numkit in old commits (immutable; no force-push) — the only
  sanctioned remainder, per plan.
