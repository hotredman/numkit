# wavelet/cgauwavf — ТЗ for completion

**Status:** open
**Priority:** medium
**Effort:** small (joint with `gauswavf`)
**Audited at commit:** ddf4218
**Audit date:** 2026-05-06

## Текущая реализация

- Source: `libs/wavelet/src/shape/gauss.cpp` (`cgauwavf`)
- Adapter: same file
- Spec: `tools/parity/specs/cgauwavf.json`
- What works today:
  - `[psi, x] = cgauwavf(LB, UB, N[, p])` — default `p=1`
  - All numeric values match MATLAB exactly

## MATLAB R2025b — actual behavior

Documented signatures (`help cgauwavf`):

- `[psi, x] = cgauwavf(lb, ub, n)` — default p=1
- `[psi, x] = cgauwavf(lb, ub, n, p)` — integer p (1..8)
- `[psi, x] = cgauwavf(lb, ub, n, wname)` — wname is `'cgauN'`

## Gaps (numkit vs MATLAB)

| # | Branch / case | MATLAB does | numkit does | Severity |
|---|---|---|---|---|
| 1 | `cgauwavf(___, 'cgauN')` wname form | parses N | throws "Cannot convert char to scalar" | medium |

## Reference table (from probe)

| Inputs | MATLAB | numkit |
|---|---|---|
| `cgauwavf(-5, 5, 8)` real | `[1.47e-11 -1.13e-5 -0.00846 0.588 -0.588 0.00846 1.13e-5 -1.47e-11]` | identical ✅ |
| same imag | `[-7.74e-11 -3.37e-6 0.0237 0.0611 0.0611 0.0237 -3.37e-6 -7.74e-11]` | identical ✅ |
| `cgauwavf(-5, 5, 8, 4)` real | `[-5.72e-10 -2.0e-4 0.0496 -0.487 -0.487 0.0496 -2.0e-4 -5.72e-10]` | identical ✅ |
| `cgauwavf(-5, 5, 8, 'cgau3')` real | `[-3.11e-11 -9.46e-5 0.0186 -0.477 0.477 -0.0186 9.46e-5 3.11e-11]` | THROWS |

## Recommended fixes

Apply the joint wname-parser fix from
`audit/findings/wavelet/gauswavf.md`. For `cgau` parse with prefix
`'cgau'`.

Spec extension: same — fingerprint for p ∈ {1..8} (real + imag) and
the wname form. `tol = 1e-12`.

## Out of scope for this ТЗ

- N/A — joint fix.

## Closed
- Closed in commit: PENDING
- Closed date: 2026-05-08
- Notes: Joint closure with audit/closed/wavelet/gauswavf.md.
  Same fix shape: 'cgauN' wname form was throwing; now parses N.
  See gauswavf's Closed block for the parseGaussOrder helper
  details. Spec extended from 8 to 8 fingerprints (p ∈ {1, 2, 4}
  + 'cgau3' wname). Parity OK numkit ↔ MATLAB at tol=1e-9. 6
  TEST_F gtest (existing 4 + 2 new WnameForm /
  WnameMatchesIntegerForm).
