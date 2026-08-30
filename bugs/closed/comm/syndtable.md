# comm.syndtable — syndrome decoding table missing

- **Status:** ✅ FIXED (2026-06-19) — min-weight coset-leader enumeration
- **Severity:** P2 (missing function)
- **Kind:** missing-fn
- **Found:** 2026-06-04 via missing-fn sweep

## Symptom
`syndtable(H)` — build the syndrome-decoding table (coset-leader lookup)
for a linear block code from its parity-check matrix `H` — is not
registered. It is the companion that `decode`/`syndrome` decoding needs.

## Repro
```matlab
H = [1 0 1 1 0 0; 0 1 1 0 1 0; 1 1 0 0 0 1];   % [n-k]x[n] = 3x6
t = syndtable(H);
% MATLAB: size(t) = [8 6]  (2^(n-k)=8 coset leaders, each length n=6)
% numkit: Error — VM: undefined function 'syndtable'
```

## Fix (2026-06-19)
Implemented `numkit::comm::syndtable` (`blockcoding.cpp`, next to
`hammgen`/`gen2par`/`decode`). Returns the `2^(n-k) × n` table whose row
`s+1` is the minimum-weight error pattern with syndrome `s = bi2de(mod(H·eᵀ,
2), 'left-msb')` (row 1 of `H` is the MSB; each column's single-bit
syndrome is precomputed as a packed bitmask). Error patterns are enumerated
by **ascending Hamming weight**, and within a weight by **lexicographic bit
position**; the first pattern reaching each syndrome fills its row (so among
equal-weight leaders the lowest-position one wins — MATLAB's tie-break). The
scan stops once all `2^(n-k)` rows are filled. Guards `n-k ≤ 24`.

Verified vs MATLAB R2025b (parity `syndtable.json` → OK, **exact full-table
match**): (7,4) Hamming → 8×7, all-weight-1 leaders (7 set bits), bit-1 →
row 5; a `3×4` code needing weight-2 leaders + ties → 10 set bits, `s=3`
leader `[1 0 0 1]`, with the H3 flat signature bit-identical to MATLAB;
repro `3×6` → 8×6. Guards: `syndtable_test.cpp` (3 TEST_F:
Hamming-perfect / weight-2+ties / repro-size), `known_bugs_test.cpp`
(`Syndtable`, promoted live); smoke `syndtable_smoke.m`.

## References
- `src/toolboxes/comm/src/coding/blockcoding.cpp` (`syndtable`,
  `nextCombination`), `.../include/numkit/comm/coding/blockcoding.hpp`,
  `src/bundle/src/register/comm/coding/blockcoding_reg.cpp` (`syndtable_reg`).
- `tools/parity/specs/syndtable.json`.
- shipped + reused: `hammgen`/`gen2par`
- MATLAB `doc syndtable`
