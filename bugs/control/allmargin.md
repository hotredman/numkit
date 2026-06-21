# control.allmargin — all stability margins struct missing

- **Status:** ✅ FIXED (2026-06-19) — exact G(jω) scan + bisection
- **Severity:** P2 (missing function)
- **Kind:** missing-fn
- **Found:** 2026-06-04 via missing-fn sweep

## Symptom
`allmargin(sys)` — return ALL gain/phase/delay margins and the stability
flag as one struct — is not registered. numkit ships `margin` (the single
gain+phase margin pair) and `bode`/`sigma`, but not the comprehensive
`allmargin` struct.

## Repro
```matlab
S = allmargin(tf(1, [1 6 11 6]));   % 1/((s+1)(s+2)(s+3))
% MATLAB: S.GainMargin = 60, S.GMFrequency = sqrt(11) = 3.31663, S.Stable = 1
% numkit: Error — VM: undefined function 'allmargin'
```

## Fix (2026-06-19)
Implemented `numkit::control::allmargin` (`analyze/hinfnorm`-sibling in
`analyze.cpp`) returning the 7-field struct (`GainMargin`, `GMFrequency`,
`PhaseMargin`, `PMFrequency`, `DelayMargin`, `DMFrequency` row vectors +
`Stable` logical).

Unlike `margin` (which interpolates a Bode grid), `allmargin` evaluates the
**exact** open-loop response `G(jω) = num(jω)/den(jω)` (from the model's
num/den): a fine log-frequency scan brackets every sign change of `|G|−1`
(gain crossovers → phase + delay margins) and of `Im(G)` with `Re(G)<0`
(phase crossovers → gain margins), and each bracket is **bisected on the
exact response**. So margins match MATLAB to ~6 digits even at sharp
resonances a grid would miss. `DelayMargin = PM(rad)/ω_gc`; `Stable` is the
unity-feedback closed-loop stability from `roots(den+num)` all in the LHP.

Verified vs MATLAB R2025b (parity `allmargin.json` → OK): A=`1/((s+1)(s+2)
(s+3))` → GM=60, GMf=√11=3.31662, no gain crossover (`numel(PhaseMargin)=0`),
Stable=1; B=`1/(s(s+1)(s+2))` → GM=6, GMf=√2, PM=53.4109°, PMf=0.445747,
DM=2.09131, Stable=1; high-gain `100/((s+1)(s+2)(s+3))` → Stable=0. Guards:
`allmargin_test.cpp` (6 TEST_F), `known_bugs_test.cpp` (`Allmargin`,
promoted live); smoke `allmargin_smoke.m`.

> Found a separate defect while wiring the zpk input path: `zpk([], poles, k)`
> (no finite zeros) drops the gain — see **bugs/control/zpk-empty-zeros.md**
> (zp2tf bug, OPEN). allmargin's tf path is unaffected; the zpk tests use a
> finite zero.

> Parity-harness note: spec exprs that read struct fields must avoid the
> `S.field(idx)` pattern (it mis-parses as a method call inside the harness's
> function wrapper — a VM quirk); index a temp or use 1×1 fields directly.

## References
- `src/toolboxes/control/src/analyze/analyze.cpp` (`allmargin`, exact
  `evalG`/`bisectRoot`), `.../include/numkit/control/analyze/analyze.hpp`,
  `src/bundle/src/register/control/analyze/analyze_reg.cpp` (`allmargin_reg`).
- `tools/parity/specs/allmargin.json`.
- related: control/zpk-empty-zeros.md (found here), margin (the single-margin
  sibling — still Bode-grid-based)
- MATLAB `doc allmargin`
