# todo: Full rebrand numkit → nk (namespace `nk::`, dirs `nk/`, binaries `nk_*`)

*Kind:* refactor / rebrand · *Status:* open (plan approved, awaiting execution) ·
*Surfaced:* 2026-09-12 (user decision)

> Lifecycle: open → done. On completion record the outcome in
> `dev-docs/memory/` per protocol and delete this file — the todo list
> holds open work only.

## Goal

Rebrand the whole project from **numkit** to **nk**. `numkit` must not
remain ANYWHERE in the tree, scripts, docs, tooling, or artifacts —
one atomic commit, verified by the full test grid. Decided after the
with/without-underscore discussion: **`nk`** (no underscore) — typing
ergonomics (no SHIFT), `npx nk` / `nkplay` CLI consistency, the C++
two-letter-namespace convention, and no collision with math index
notation (`n_k` reads as "n at k" in a MATLAB-lands codebase).

## Locked decisions

| Surface | From | To |
|---|---|---|
| C++ namespace | `numkit::` (5 263 refs) | `nk::` |
| Include dirs | `include/numkit/` (22 dirs) | `include/nk/` |
| Includes | `#include <numkit/...>` (1 671 files) | `#include <nk/...>` (`.hpp` stays) |
| CMake targets | `numkit_core_obj`, `numkit_gtest`, `numkit_cli`, `numkit_codegen`, `numkit_*_obj`, `NUMKIT_CORE_SOURCES`... | `nk_*`, `NK_CORE_SOURCES`... |
| Binaries | `numkit_repl`, `numkit_gtest(.exe)` | `nk_repl`, `nk_gtest` |
| Env vars | `NUMKIT_FS`, `NUMKIT_CXX`, `NUMKIT_FUSE`, `NUMKIT_*` | `NK_FS`, `NK_CXX`, `NK_FUSE`, `NK_*` |
| Scripts | `scripts/**` path references (325 files incl. docs) | swept |
| IDE / npm | package name, `npx numkit` docs refs | `nk`, `npx nk` |
| Living docs | AGENTS.md, README.md, dev-docs/handbook, memory, todo, bugs/ (opened+closed paths), tools/ READMEs | swept |
| Repo remotes | `git.megahard.ru/numkit/numkit`, mirror `hotredman/numkit` | **USER-SIDE**: rename on both hosts |
| npm publish | — | **USER-SIDE** (agent never publishes) |

## Honest boundary: git history

Old COMMIT MESSAGES and merged diffs will still contain `numkit` —
removing them requires history rewrite, which the repo rules forbid
(never force-push main). The acceptance grep therefore covers the
WORKING TREE only; `git log` archaeology keeps the old name forever.
Closed-bug FILES are in-tree, so their paths ARE swept (bugs keep bug
ids/hashes/quoted output verbatim — only path/name tokens change).

## Execution phases (one atomic commit for ALL of it)

1. **Mechanical sweep** (scripted, word-boundary aware):
   - `git mv` the 22 `include/numkit/` dirs → `include/nk/`;
   - tracked-file replacements: `numkit::`→`nk::`,
     `<numkit/`→`<nk/`, `"numkit/`→`"nk/`, `namespace numkit`→
     `namespace nk`, `numkit_`→`nk_` (targets/binaries/paths),
     `NUMKIT_`→`NK_`, bare `numkit` words in docs/scripts → `nk`
     (case-by-case in prose: "NumKit" → "NK" where it names the
     product);
   - review EVERY hunk of the mechanical diff (portion-36/37 lesson:
     regex batches over string literals eat neighbours).
2. **Verification grid**: full Release suite (~13.4k), WASM build,
   web bundle, frontend vitest, `bugs_audit.py`, one CLI smoke
   (`nk_repl <smoke.m>` via the renamed path), parity runner still OK.
   Everything green inside the SAME commit; breakage fixed before it
   lands.
3. **Grep-zero audit**: `grep -rniE 'numkit'` over the tree → expected
   hits ONLY in: git history (out of scope, see boundary), and quoted
   historical output inside `bugs/**` where it is the record of what
   an old binary printed. Anything else is a violation.
4. **Memory + close this todo**: outcome to `dev-docs/memory/`
   (rename mechanics + the NK_* env rename note for users' scripts),
   delete this file.

## Sequencing

Execute as a standalone portion BEFORE Play portion B (ReactiveSession)
— the tree is clean right now (Play A pushed at d934702cc), no merge
hazard window. User-side host/npm renames can happen any time after
the commit lands; until then remotes keep working (repo name is not
part of the clone URL's path sensitivity for push rules — the AGENTS
push policy targets `origin`, unaffected by a host-side rename).

## Acceptance criteria

- [ ] Working tree greps zero for `numkit`/`NumKit` outside the
      boundary allowlist above.
- [ ] Full Release suite green post-rename.
- [ ] WASM + web + frontend green.
- [ ] `nk_repl` runs a smoke end-to-end via the renamed script paths.
- [ ] bugs_audit clean; parity runner green on a spot-check spec.
- [ ] Outcome recorded in memory; this file deleted.
