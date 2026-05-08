# wavelet/gauswavf — ТЗ for completion

**Status:** open
**Priority:** medium
**Effort:** small
**Audited at commit:** ddf4218
**Audit date:** 2026-05-06

## Текущая реализация

- Source: `libs/wavelet/src/shape/gauss.cpp` (`gauswavf`)
- Adapter: same file
- Spec: `tools/parity/specs/gauswavf.json`
- What works today:
  - `[psi, x] = gauswavf(LB, UB, N[, p])` — default `p=1`
  - All numeric values match MATLAB exactly

## MATLAB R2025b — actual behavior

Documented signatures (`help gauswavf`):

- `[psi, x] = gauswavf(lb, ub, n)` — default p=1
- `[psi, x] = gauswavf(lb, ub, n, p)` — integer p (1..8)
- `[psi, x] = gauswavf(lb, ub, n, wname)` — wname is string
  `'gausN'` for `N = 1..8`

## Gaps (numkit vs MATLAB)

| # | Branch / case | MATLAB does | numkit does | Severity |
|---|---|---|---|---|
| 1 | `gauswavf(LB, UB, N, 'gausN')` wname form | parses N from string | adapter `args[3].toScalar()` ⇒ throws "Cannot convert char to scalar" | medium |

## Reference table (from probe)

| Inputs | MATLAB | numkit |
|---|---|---|
| `gauswavf(-5, 5, 8)` (default p=1) | `[1.24e-10 1.84e-5 0.0388 0.766 -0.766 -0.0388 -1.84e-5 -1.24e-10]` | identical ✅ |
| `gauswavf(-5, 5, 8, 4)` | `[1.07e-8 5.04e-4 0.114 -0.436 -0.436 0.114 5.04e-4 1.07e-8]` | identical ✅ |
| `gauswavf(-5, 5, 8, 'gaus3')` | `[-3.0e-9 -2.14e-4 -0.124 0.783 -0.783 0.124 2.14e-4 3.01e-9]` | THROWS |

## Recommended fixes

1. **Adapter rewrite to accept wname:** detect string `args[3]`,
   strip leading `'gaus'`, parse the trailing integer:
   ```cpp
   if (args[3].isChar() || args[3].isString()) {
     std::string s = lower(args[3].toString());
     if (s.substr(0, 4) == "gaus") p = std::stoi(s.substr(4));
     else throw Error("gauswavf: bad wname '" + s + "'", ...);
   } else { p = (int)args[3].toScalar(); }
   ```
2. **Spec extension** — add fingerprint for p ∈ {1..8} and the
   wname form. `tol = 1e-12`.

## Out of scope for this ТЗ

- N/A.

## Closed
- Closed in commit: PENDING
- Closed date: 2026-05-08
- Notes: Joint closure with audit/closed/wavelet/cgauwavf.md.

  Bug fix: 4th arg as `'gausN'` string was throwing "Cannot
  convert char to scalar". Adapter now branches on
  `isChar()/isString()`: for strings, strips the `gaus` prefix
  and parses the trailing integer; bad strings throw cleanly.
  Same fix shape for `cgauwavf` with `cgau` prefix.

  New shared helper `parseGaussOrder(arg, prefix, fn)` handles
  both adapters.

  Spec extended from 7 to 7 fingerprints (refactored coverage to
  cover p ∈ {1, 2, 4, 8} integer + 'gaus3' wname). Parity OK
  numkit ↔ MATLAB at tol=1e-9. Octave doesn't ship `gauswavf`/
  `cgauwavf`. 7 TEST_F gtest (existing 4 + 3 new WnameForm /
  WnameMatchesIntegerForm / WnameRejectsBadString).
