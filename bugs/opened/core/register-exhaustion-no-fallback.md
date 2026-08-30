# core.compiler — register exhaustion (>255) surfaces as a user error on real code instead of the TreeWalker fallback

- **Status:** 🔴 OPEN (needs investigation — fallback contract vs. real-code ceiling)
- **Severity:** P2 (works in MATLAB, refused in numkit)
- **Kind:** bug
- **Found:** 2026-08-30 via fieldtest (real-world `sa_tsp.m` — simulated
  annealing TSP, batch 20260830-003212)

## Symptom

```
Error: Compiler: register exhaustion (>255 registers needed in chunk)
    at sa_tsp.m
```

MATLAB R2025b runs the same script to completion (correct output). The
documented design (CORE_ARCHITECTURE / dual-engine) says VM compile
failure — including register exhaustion — falls back to the TreeWalker
reference engine; here the error surfaced to the user instead.

## Repro

Run the fieldtest corpus script (fieldtest/corpus/work/…/sa_tsp.m):
`node packages/numkit/bin/cli.js sa_tsp.m`. (A minimal in-repo reproducer
is the follow-up: extract the expression shape that needs >255 live
registers in one chunk.)

## Root cause (hypotheses to check)

1. The fallback exists only for FUNCTION bodies, not top-level scripts
   (script chunk compile throws straight through);
2. or the fallback triggers but the TreeWalker itself also fails and the
   original compile error is what gets reported;
3. or the expression legitimately needs deep unrolled registers that the
   compiler could reuse (register-lifetime bug) — a real compiler fix.

## Suggested fix

Whatever the branch: a real-world script that MATLAB executes must never
die on an internal register ceiling — either the fallback or register
reuse must cover it. Measure which hypothesis holds first.

## References
- **Guard:** deferred — minimal standalone repro pending (corpus-referenced).

fieldtest batch `reports/20260830-003212.json` (`sa_tsp.m` — runtime-error).
Related: stack_safety.md (the same >255 `uint8_t` register-file ceiling).
