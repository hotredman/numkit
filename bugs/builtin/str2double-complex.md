# builtin.str2double — does not parse complex-number strings

- **Status:** 🔴 OPEN
- **Severity:** P3 (returns NaN where MATLAB parses a complex value)
- **Kind:** bug
- **Found:** 2026-06-05 via DEEP-PROBE (string-function sweep, cycle 57)

## Symptom
`str2double` of a complex-number string returns NaN; MATLAB parses it to the
complex value. Forms MATLAB accepts: `'1+2i'`, `'1-2i'`, `'2i'`, `'-3i'`,
`'i'`, `'-i'`, `'1+2j'` (j accepted), `'3.5+1.5i'`, and with internal/edge
spaces `' 2 + 3i '`.

## Repro (numkit vs MATLAB R2025b)
```matlab
str2double('1+2i')     % numkit: NaN ;  MATLAB: 1+2i
str2double('2i')       % numkit: NaN ;  MATLAB: 0+2i
str2double('i')        % numkit: NaN ;  MATLAB: 0+1i
str2double('1+2j')     % numkit: NaN ;  MATLAB: 1+2i  (j is accepted)
str2double('3.5+1.5i') % numkit: NaN ;  MATLAB: 3.5+1.5i
str2double(' 2 + 3i ') % numkit: NaN ;  MATLAB: 2+3i
str2double('1.5')      % 1.5 on both (real path unaffected)
```

## Root cause
`str2doubleOne` (libs/builtin/src/language/strings/strings.cpp ≈ line 423)
parses each token with `strtod` and requires the ENTIRE token to be a single
real number, so any imaginary suffix fails → NaN. Its own comment notes this:
"Complex literals like '2i'/'3+4i' remain a separate unimplemented gap -> NaN."
`str2double` also always allocates a DOUBLE output, so supporting complex
requires producing a COMPLEX Value when any element parses as complex.

## Suggested fix (LIBS, moderate)
1. Add a complex token parser: trim, then detect a trailing `i`/`j`; split the
   token into a real part and an imaginary part at the last `+`/`-` that is NOT
   an exponent sign (i.e. not immediately after `e`/`E`); handle the
   pure-imaginary forms (`'i'`→1i, `'-i'`→-1i, `'2i'`→2i) and the
   real-only/imag-only cases. Return `std::optional<Complex>` (NaN on failure,
   matching the real path).
2. In `str2double`, parse all elements; if ANY element is complex (has a
   nonzero imaginary part OR used the imaginary form), build a COMPLEX output
   Value of the same shape (real elements get zero imaginary); otherwise keep
   the existing DOUBLE path (zero regression). Mirror the cell / string-array /
   scalar shape handling.
3. Verify the exponent-sign edge (`'1e-3+2i'`) and that a bare real
   (`'1.5'`, `'Inf'`, `'NaN'`) still returns a real double.

## Guard
`libs/builtin/tests/known_bugs_test.cpp` → `DISABLED_Str2doubleComplex`
(asserts the MATLAB-correct complex parse; flip the prefix when implemented).

## References
- `libs/builtin/src/language/strings/strings.cpp` (`str2doubleOne`, `str2double`)
- MATLAB `doc str2double` (parses real and complex numeric text)
