# wavelet.wfilters / dwt — biorthogonal (bior*/rbio*) families unsupported

- **Status:** 🔴 OPEN
- **Severity:** P2 (missing wavelet family)
- **Kind:** stub
- **Found:** 2026-06 via DEEP-PROBE (long-standing KNOWN GAP)

## Symptom
`wfilters`/`dwt`/`wavedec` reject biorthogonal (`bior*`) and reverse-
biorthogonal (`rbio*`) wavelet names; only haar/db/sym/coif are known.

## Repro
```matlab
[a, d] = dwt([1 2 3 4 5 6 7 8], 'bior2.2')
% numkit: Error — wfilters: unsupported wavelet name 'bior2.2'
%         (try haar, db1..db10, sym2..sym10, coif1..coif5)
% MATLAB: returns the bior2.2 approximation/detail coefficients
```

## Root cause
`src/toolboxes/wavelet/src/.../wfilters*` only tabulates the orthogonal families.
Biorthogonal wavelets have **distinct decomposition and reconstruction**
filter pairs (Lo_D/Hi_D ≠ Lo_R/Hi_R), which the current orthogonal-only
filter machinery doesn't model.

## Suggested fix
Add the bior/rbio filter tables (bior1.1/1.3/1.5/2.2/.../6.8 and the rbio
reverses) and thread the separate decomposition vs reconstruction filter
pairs through dwt/idwt/wavedec/waverec. Medium (mostly the coefficient
tables + the analysis/synthesis filter split). Validate coefficients vs
MATLAB per family.

## References
- `src/toolboxes/wavelet/src/.../wfilters*`, `dwt*`, `idwt*`
- MATLAB `doc waveinfo` (bior, rbio)
