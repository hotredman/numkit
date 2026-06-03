# `bugs/` — one file per bug

Structured bug catalog. **Every bug gets its own `.md` file** here, with a
self-contained repro (numkit output vs MATLAB R2025b) so any session can
pick it up cold. This complements the flat append-only [BUGS.md](../BUGS.md)
(quick running log) and is distinct from `audit/findings/**` (the parallel
auditor worker's territory — do not write there from the main worker).

## Layout

```
bugs/
  README.md              ← this file (index + conventions)
  <namespace>/<fn>.md    ← one bug (e.g. signal/dct-types.md)
```

Use `<fn>.md` when a function has one open bug; `<fn>-<aspect>.md` when it
has several distinct ones (e.g. `cceps-nd-phase.md`).

## File template

```markdown
# <namespace>.<fn> — <one-line title>

- **Status:** 🔴 OPEN  |  ✅ FIXED (<commit>, YYYY-MM-DD)
- **Severity:** P1 wrong result · P2 missing feature · P3 minor/style
- **Found:** YYYY-MM-DD via <how>

## Symptom
What is wrong, in one or two sentences.

## Repro
​```matlab
<exact call>
% numkit: <output>
% MATLAB: <output>
​```

## Root cause
If known.

## Suggested fix
Approach + scope estimate; note any deferral reason (objects, core change,
large algorithm).

## References
Source files, related commits, related specs/tests.
```

## Severity legend (matches BUGS.md)

- **P0** crash / data loss
- **P1** wrong result (silently incorrect output)
- **P2** missing feature / option / output relative to MATLAB
- **P3** test-only / style

## Lifecycle

1. Find a bug → create `bugs/<ns>/<fn>.md` (status OPEN) with full repro.
2. Fix it (4 artefacts) → flip status to ✅ FIXED, add the commit hash +
   one-line note. Keep the file (the repro stays useful as a regression
   record); update the row below.

## Index

| Bug | Sev | Status | Notes |
|---|---|---|---|
| [signal/rceps-cceps-padding](signal/rceps-cceps-padding.md) | P1 | ✅ FIXED 9fcf6872 | cepstrum garbage on non-2ⁿ lengths + rceps 2nd output |
| [signal/spectrogram-ps](signal/spectrogram-ps.md) | P2 | ✅ FIXED 1128db65 | missing 4th output (PSD) |
| [signal/dct-types](signal/dct-types.md) | P2 | 🔴 OPEN | Type 1/3/4 stubbed |
| [signal/cceps-nd-phase](signal/cceps-nd-phase.md) | P1 | 🔴 OPEN | non-2ⁿ phase (rcunwrap) + missing `nd` output |
| [signal/risetime-falltime-outputs](signal/risetime-falltime-outputs.md) | P2 | 🔴 OPEN | only 1 of up to 5 outputs |
| [signal/findpeaks-widthreference](signal/findpeaks-widthreference.md) | P2 | 🔴 OPEN | 'halfheight'/'halfprom' option |
| [signal/pmusic-peig](signal/pmusic-peig.md) | P2 | 🔴 OPEN | functions missing |
| [signal/fillgaps](signal/fillgaps.md) | P2 | 🔴 OPEN | function missing |
| [signal/ellipord-bandstop](signal/ellipord-bandstop.md) | P2 | 🔴 OPEN | bandstop case throws |
| [image/regionprops-perimeter](image/regionprops-perimeter.md) | P1 | 🔴 OPEN | unknown property silently dropped |
| [stats/smoothdata-methods](stats/smoothdata-methods.md) | P2 | 🔴 OPEN | sgolay/lowess/loess throw |
| [stats/isoutlier-gesd](stats/isoutlier-gesd.md) | P2 | 🔴 OPEN | 'gesd' method throws |
