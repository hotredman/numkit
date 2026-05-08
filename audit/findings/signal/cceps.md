# signal/cceps — ТЗ for completion

**Status:** open
**Priority:** **high**
**Effort:** small
**Audited at commit:** d3d8da7
**Audit date:** 2026-05-06

## Текущая реализация

- Source: `libs/signal/src/transforms/...` (`cceps`)
- Spec: `tools/parity/specs/cceps.json`

## MATLAB R2025b — actual behavior

`y = cceps(x)` — complex cepstrum.

## Gaps (numkit vs MATLAB)

| # | Gap | Severity |
|---|---|---|
| 1 | output is **TIME-REVERSED** vs MATLAB. Probe: `cceps([1:8]')` MATLAB=`[2.008 -0.044 -0.008 0.038 0.101 0.200 0.384 0.904]` vs numkit=`[2.008 0.904 0.384 0.200 0.101 0.038 -0.008 -0.044]` — same elements, reversed order (except DC). | **high** |

## Reference table (from probe)

| Inputs | MATLAB | numkit |
|---|---|---|
| `cceps([1:8]')` | `[2.008 -0.0436 -0.00834 0.0375 0.1014 0.2002 0.3844 0.9045]` | `[2.008 0.9045 0.3844 0.2002 0.1014 0.0375 -0.00834 -0.0436]` (reversed) |

## Recommended fixes

1. **Reverse the output.** The cepstrum is the IFFT of log of the
   FFT magnitude. numkit appears to be returning the time-reversed
   version (or perhaps doing FFT instead of IFFT in the inverse
   step). Single transform-direction fix.
2. **Verify icceps cascade** — `icceps(cceps(x))` should return `x`;
   probe shows numkit returns `[8 7 6 5 ...]` (reversed), MATLAB
   returns `[8 1 2 3 ...]` (also wrong but differently). Both
   appear to have roundtrip bugs that need investigation.
3. **Spec extension** — fingerprint over basic cceps + roundtrip
   identity. `tol = 1e-9`.

## Out of scope for this ТЗ

- N/A.
