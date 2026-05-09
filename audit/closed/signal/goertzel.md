# signal/goertzel — ТЗ for completion

**Status:** closed
**Priority:** medium
**Effort:** small
**Audited at commit:** d3d8da7
**Audit date:** 2026-05-06

## Gaps (numkit vs MATLAB)

| # | Branch / case | MATLAB does | numkit does | Severity |
|---|---|---|---|---|
| 1 | `goertzel(x)` 1-arg form | computes full DFT (default freq indices = 1..N) | adapter throws "goertzel: requires (x, ind)" | medium |

## Reference table

| Inputs | MATLAB | numkit |
|---|---|---|
| `goertzel(sig)` (1-arg) | 64×1 result (full DFT) | THROWS |
| `goertzel(sig, [5 15])` | 2-element result at requested bins | likely OK |

## Recommended fixes

1. **Default `ind = 1:N`** when only the signal is provided.
2. **Spec extension** — add fingerprint with default ind + explicit
   ind list.

## Out of scope for this ТЗ

- N/A.

## Closed
- Closed in commit: TBD
- Closed date: 2026-05-09
- Notes: Adapter goertzel_reg now accepts the 1-arg form
  goertzel(x) — defaults ind = 1:N (full DFT) per MATLAB R2025b,
  with shape matching x (column input → column output, etc.).
  Also accepts goertzel(x, []) and treats empty 2nd arg as default.

  Default-ind buffer allocated on the per-call ScratchArena via
  ScratchVec semantics (PMR HARD RULE — no std::vector for scratch
  in the new code).

  4 artefacts:
  - impl: libs/signal/src/transforms/goertzel.cpp — adapter rewrite
  - parity: tools/parity/specs/goertzel.json — 11 fingerprints
    (4 partial-bin + 7 full-DFT structural), correctness=OK against
    MATLAB R2025b. Octave skipped (goertzel not in 11.1.0).
  - gtest: libs/signal/tests/goertzel_test.cpp — 5 tests
    (partial-bin matches MATLAB, 1-arg defaults to full DFT, full-DFT
    bin equals partial-bin call, empty 2nd arg behaves as default,
    no-args throws)
  - smoke: libs/signal/tests/smoke/goertzel_smoke.m

  Side observation noted in test comments: numkit's compat-aliased
  fft and goertzel have a per-bin imag-sign disagreement on the same
  input. Out of scope for this ТЗ — separate fft sign-convention
  audit needed.
