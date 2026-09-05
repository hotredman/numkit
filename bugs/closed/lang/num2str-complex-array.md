# lang.num2str — complex ARRAY formatting unsupported (6 scripts of one book die on it)

- **Status:** ✅ FIXED (2026-09-05)
- **Severity:** P2 (whole-script failure for any code printing complex
  arrays — 6 of 72 scripts in portion 10 hit exactly this)
- **Kind:** stub (documented formatting branch not implemented)
- **Found:** 2026-09-05 via fieldtest portion 10 (springer-math numerical
  methods book: HessenbergQR, InverseIteration, MethodOrtIter,
  MethodQR_iter, MethodQR_shift, PowerM — all line ~70-91)

## Symptom

`num2str` on a complex ARRAY throws "complex array formatting
(column-aligned) is not supported in this revision; only scalar
complex". Even all-real-valued data stored complex-typed fails.

## Repro (self-contained)

```matlab
clear;
s = num2str(complex([1 2; 3 4]));
disp(s)
% numkit:  Error: complex array formatting not supported
% MATLAB R2025b: '13    24'  (imag==0 elements print as real)
```

MATLAB semantics: per-element — zero-imaginary elements print as real
numbers; nonzero as `a±bi`; columns aligned (width = max element width
of the column).

## Suggested fix

Element-wise formatter honoring the zero-imag case first (covers the
common complex-typed-storage-of-real-data), then `%.4g%+.4gi` style for
general complex, column-aligned like the real path.

## References

- **Guard:** `DISABLED_Num2strComplexArray` in
  `src/builtin/tests/strfun_test.cpp` — RED under
  --gtest_also_run_disabled_tests.


## Resolution (2026-09-05)

All three num2str overloads format complex arrays: elements go through
num2strComplexScalar (imag==0 prints as plain real — the common
complex-typed-storage-of-real-data case), columns are padded to the max
element width, rows join with newline; the FMT overload formats both
parts per element. Verified vs MATLAB semantics; the 6 scripts of the
book print their complex spectra.

Bonus bug found by the guard: complex(A) on an ALREADY-COMPLEX A
silently dropped the imaginary parts (elemAsDouble through a complex
matrix); MATLAB returns A unchanged — fixed with a passthrough in
src/builtin/src/elfun/complex.cpp.


## Fidelity addendum (2026-09-05, follow-up "full MATLAB parity")

Byte-exact vs MATLAB R2025b (probed, guarded):
- complex([1.5 2.25 3.125]) -> '1.5        2.25       3.125'
- complex([1 2; 3 4]) -> '1  2' / '3  4' as a 2x4 CHAR MATRIX
  (return shape fixed: was a 1-row char with embedded newlines)
- complex([1+2i 3-4i]) -> '1+2i   3-4i'
- any nonzero imag -> EVERY element in a+bi form (imag==0 get '+0i')

Known residual (cosmetic): the exact field-width model for mixed
integer-magnitude complex arrays (complex([100 200 3000]) — MATLAB gives
spacing 3/2, derivable neither from maxChars+2 nor P+7 consistently with
the other probes) is not reproduced; spacing may differ by a column.
Documented divergence; values and the a+bi forms are correct.
