# control.allmargin — all stability margins struct missing

- **Status:** 🔴 OPEN
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
% MATLAB: struct with fields
%   GainMargin GMFrequency PhaseMargin PMFrequency DelayMargin DMFrequency Stable
%   S.GainMargin   = 60.0000  (≈60)
%   S.GMFrequency  = 3.31663  (= sqrt(11))
%   S.Stable       = 1
% numkit: Error — VM: undefined function 'allmargin'
```

## Root cause
Not implemented. The frequency-response machinery `allmargin` needs already
exists (`margin`/`bode`), but the all-crossover scan + struct packaging is
not wired.

## Suggested fix
Scan the open-loop frequency response for every gain crossover (|G|=1 →
phase margins) and every phase crossover (∠G=−180° → gain margins),
compute the delay margins, and assemble the 7-field struct (vectors for
GainMargin/GMFrequency/PhaseMargin/PMFrequency/DelayMargin/DMFrequency plus a
scalar logical Stable). Reuse `margin`'s crossover solver. Medium. Verify
the GM/GMFrequency and Stable flag vs MATLAB on a 3rd-order plant.

## References
- new file under `src/toolboxes/control/src/...`; reuse `margin`/`bode`
- MATLAB `doc allmargin`
