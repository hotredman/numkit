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
