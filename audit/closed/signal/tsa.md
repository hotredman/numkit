# signal/tsa — ТЗ for completion

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
- Closed in commit: pending (cycle 5)
- Closed date: 2026-05-09
- Notes: Initial closure was DEFERRED -- numkit only had legacy 4-arg form tsa(x, fs, rpm, fs_rpm) using a continuously-sampled rotation-rate signal. MATLAB tsa(x, fs, tPulse) takes a pulse-arrival-time vector. Fix: added a 3-arg dispatcher in libs/signal/src/measurements/vibration.cpp::tsa_reg that detects the MATLAB form and computes the time-synchronous average using linear interpolation between consecutive pulses. The legacy 4-arg form continues to work. Bit-identical with MATLAB R2025b on standard probe (sin(2pi*10*t), pulses spaced 0.1s -> 100 samples averaged over 9 revolutions).
