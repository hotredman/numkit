# comm.syndtable — syndrome decoding table missing

- **Status:** 🔴 OPEN
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

## Root cause
Not implemented. numkit has `hammgen`/`gen2par`/`cyclgen`/`encode`/`decode`
but not the standalone coset-leader table builder.

## Suggested fix
For each of the `2^(n-k)` syndromes, find a minimum-weight error pattern
`e` with `H·eᵀ = syndrome` (the coset leader): iterate error patterns by
ascending Hamming weight, compute each syndrome (over GF(2)), and store the
first pattern that maps to each syndrome. Returns the `2^(n-k) × n` table
indexed by `bi2de(syndrome)+1`. Small-medium (the search is exponential in
`n-k` but fine for textbook codes). Verify table size + a few coset leaders
vs MATLAB.

## References
- new file under `src/toolboxes/comm/src/...`; reuse `gen2par`/`bi2de`
- MATLAB `doc syndtable`
