# apps.wasm-cli — IDE protocol markers (`__FIGURE_CLOSE_ALL__`, `__CLEAR__`, …) leak into CLI stdout

- **Status:** ✅ FIXED (2026-08-30)
- **Severity:** P3 (product polish; corrupts scripted consumers)
- **Kind:** bug
- **Found:** 2026-08-30 via fieldtest (3 output mismatches in batch
  20260830-003212 that are pure marker noise)

## Symptom

```bash
$ npx numkit -e "clear; close all; disp(42)"
__FIGURE_CLOSE_ALL__
42
```

The sentinel lines are the browser-IDE's internal protocol (figure sync /
workspace clear) emitted by the shared repl bindings. In the CLI they are
garbage: they pollute piped output, break output-diffing, and will confuse
AI agents parsing stdout.

## Repro

`node packages/numkit/bin/cli.js -e "clear; close all; disp(42)"` — first
line is `__FIGURE_CLOSE_ALL__` (and `__CLEAR__` for `clear` in other
scripts). MATLAB prints only `42`.

## Root cause

`repl_execute` in the WASM bindings always emits the markers; the CLI has
no IDE attached. The markers-or-not switch lives on the wrong side of the
boundary.

## Suggested fix

Either the bindings take an `ideMode` flag (repl_init arg) and suppress
markers when absent, or the CLI filters `__(FIGURE_CLOSE_ALL|CLEAR|CLC)__`
lines from repl_execute's return. The engine-side flag is cleaner (the
protocol shouldn't leave the IDE path at all).

## References
- **Guard:** proof by removal — the fieldtest harness deleted its marker-filter workaround; a regressed leak now surfaces as an output mismatch in the batch.

fieldtest harness normalises around it (`fieldtest/harness.py`, noted);
affected corpus scripts: hopfield_hand.m, chap10_7.m, sa_01knapsack.m
(all pass after marker filtering — the numbers match).

## Resolution (2026-08-30)

stripIdeMarkers() in cli.js filters the IDE-protocol sentinel lines
(/^__[A-Z_]+__$/) from every repl_execute result before stdout — all
paths (-e, file argument, function-file invocation) verified clean.
Proof by removal: fieldtest/harness.py deleted its marker normalisation;
a leak regressing would now FAIL the batch comparison instead of being
hidden. (Engine-side repl_init flag remains the cleaner long-term home
if the IDE protocol ever grows non-line-shaped messages.)
