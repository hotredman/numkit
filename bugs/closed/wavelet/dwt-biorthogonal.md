# wavelet.wfilters / dwt — biorthogonal (bior*/rbio*) families unsupported

- **Status:** ✅ FIXED (2026-06-19) — all 15 bior + 15 rbio families, bit-exact
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

## Fix (2026-06-19)
The transform machinery (`dwt`/`idwt`/`wavedec`/`waverec`) already threads
all four filters independently — `dwt` consumes `Lo_D/Hi_D`, `idwt` consumes
`Lo_R/Hi_R`, with no orthogonal-symmetry assumption. The ONLY orthogonal
coupling was in `wavelet_filters()` (the name→filter lookup), which derived
all four from one scaling filter via QMF. So the fix is purely additive:

1. New TU `filter/biorfilt.cpp` tabulates the four **distinct** filters for
   all 15 `bior*` + 15 `rbio*` families (zero-padded to a common length,
   MATLAB convention) — the Cohen-Daubechies-Feauveau spline biorthogonal
   coefficients (public math, same provenance as the db/sym/coif tables).
2. `wavelet_filters()` falls back to `bior_filterbank(name, …)` when the
   orthogonal lookup misses; the gate message now lists bior/rbio.

Because everything routes through `wavelet_filters()`, the entire family
(`wfilters`/`dwt`/`idwt`/`wavedec`/`waverec`/`dwt2`/`swt`/…) lights up at
once.

**Validation:** all 30 families' four filters are **bit-exact** vs MATLAB
R2025b `wfilters` (worst abs diff = 0 across 30 × 4 filters). Transform
parity (vs MATLAB): `dwt([1..8],'bior2.2')` a=[2.6516504294, 1.2374368671,
…], `dwt(…,'bior4.4')`, `dwt(…,'rbio3.3')`, `wavedec(…,2,'bior2.2')`
C(1)=2.03125 L=[5 5 6 8], idwt/waverec perfect reconstruction; `bior1.1`
== Haar. Coefficient values were obtained by probing MATLAB `wfilters`
(reference validation) and round-trip through a generator — no source read.

## References
- `src/toolboxes/wavelet/src/filter/biorfilt.cpp` (tables + `bior_filterbank`),
  `filter/wfilters.cpp` (fallback wiring), `filter/wfilters.hpp` (decl)
- `tools/parity/specs/bior.json`,
  `src/toolboxes/wavelet/tests/biorfilt_test.cpp` (10 cases),
  `known_bugs_test.cpp` (`DwtBiorthogonal`, promoted live),
  smoke `tests/smoke/bior_smoke.m`
- reused: the existing dwt/idwt/wavedec/waverec 4-filter machinery
- MATLAB `doc waveinfo` (bior, rbio)
