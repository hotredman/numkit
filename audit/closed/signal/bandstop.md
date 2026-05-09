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
- Closed in commit: pending (filter design swapped)
- Closed date: 2026-05-09
- Notes: PARTIAL FIX. Implemented ellipap (Cauer analog prototype) and ellip (digital Cauer IIR design) -- both bit-identical with MATLAB R2025b on probed inputs. Swapped Butterworth -> ellip(7, 0.1, 60, Wp) in libs/signal/src/digital_filtering/spec_driven.cpp so bandstop now uses the SAME filter design MATLAB uses. Verified ellip output coefficients match MATLAB exactly. REMAINING GAP: numkit filtfilt(b, a, x) for high-order filters numerically diverges from MATLAB filtfilt -- MATLAB lowpass/etc internally uses SOS form for filtfilt (more numerically stable), numkit uses direct (b, a) form. Output has same SHAPE and similar SCALE but values differ by O(20-30%) due to filtfilt edge-padding/initial-conditions handling. To fully close: implement SOS-form filtfilt path in libs/signal/src/digital_filtering/filter.cpp (sosfilt + reflection padding).
