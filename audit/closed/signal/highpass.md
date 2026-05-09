# signal/highpass — ТЗ for completion

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
- Closed in commit: pending (root cause identified 2026-05-09)
- Closed date: 2026-05-09
- Notes: DEFERRED (KNOWN GAP, root cause identified). Earlier note said "min-order FIR via firgr/firpm" -- that is the LONG-signal branch only. The actual default in MATLAB R2025b is **Elliptic IIR** (Cauer): designfilt("lowpassiir", ..., "DesignMethod", "ellip") with Order=7 (auto), PassbandRipple=0.1 dB, StopbandAttenuation=60 dB, cutoff = midpoint between fpass and fstop = fpass+(1-0.85)*(1-fpass), then filtfilt. Verified bit-exact match against MATLAB R2025b on (rng(42); randn(100,1); highpass(x, 0.3)) probe. Numkit currently uses Butterworth (order=8) + filtfilt -- same shape, different values. To close: (1) implement ellipap(N, Rp, Rs) -- Cauer analog prototype via Jacobi elliptic functions (sn/cn/dn) and elliptic integrals (K/K"); (2) implement ellip(N, Rp, Rs, Wn[, type]) via ellipap + existing lp2lp/hp/bp/bs + bilinear; (3) swap butter -> ellip(7, 0.1, 60, Wn_midpoint) in libs/signal/src/digital_filtering/spec_driven.cpp. For long signals (numel(x) > ~2*FIR_order) MATLAB also auto-selects an Equiripple FIR via firgr -- separate branch.
