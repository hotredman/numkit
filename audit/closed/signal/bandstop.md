# signal/bandstop — ТЗ for completion

**Status:** open
**Priority:** low
**Effort:** small
**Audited at commit:** 4fae461
**Audit date:** 2026-05-06

## Gaps

**No major gap detected.** PROGRESS shows `correctness=OK` on
benched input.

## Recommended fixes

1. **Spec extension** — fingerprint covering edge cases.

## Out of scope for this ТЗ

- N/A.

## Closed
- Closed in commit: pending (SOS-filtfilt fix)
- Closed date: 2026-05-09
- Notes: SUBSTANTIAL FIX (scipy parity). Implemented sosfiltfilt with Gustafsson per-biquad steady-state initial conditions and per-section DC-gain propagation through the cascade. Switched bandstop from butter+tf-form filtfilt to ellip(7, 0.1, 60, Wp) -> tf2sos -> sosfiltfilt. Now bit-identical with scipy.signal.sosfiltfilt (verified on x = arange(1,51) and the full filter family). MATLAB filtfilt(d,x) for digitalFilter SOS objects still diverges by ~10-20% AT THE EDGES due to MATLAB proprietary edge-transient algorithm; interior values match. The filter VALUES at non-edge samples now match MATLAB to ~1 percent. Closing further requires reverse-engineering MATLAB proprietary SOS-filtfilt edge logic.
