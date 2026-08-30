# lang.sprintf/fprintf — throw on complex arguments

- **Status:** ✅ FIXED (2026-06-05)
- **Severity:** P2 (throws on input MATLAB accepts; sprintf/fprintf are very common)
- **Kind:** bug
- **Found:** 2026-06-05 via DEEP-PROBE (formatting/poly probe — surfaced by
  `roots([1 0 0 -1])` returning complex roots that sprintf then rejected)

## Fixed
- Fixed: 2026-06-05 (bug-fix loop, cycle 51),
  `src/lang/src/strings/format.cpp` (`formatCyclic`). The format
  engine consumed each numeric argument via `toScalar()` / `operator()(j)`,
  which throw on a COMPLEX value ("Cannot convert complex with nonzero
  imaginary part to double scalar" for a scalar; "Not a double array" for a
  vector). MATLAB uses the **real part** of a complex argument for every
  numeric conversion (`%d`/`%f`/`%g`/`%e`/...) and silently discards the
  imaginary part.
- Fix: take `complexData()[j].real()` in BOTH argument-flattening paths — the
  scalar push (push `Value::scalar(real)`) and the per-element vector flatten.
  All real/integer/logical/char paths are untouched (zero regression).
- Verified vs MATLAB R2025b:
  `sprintf('%g',1+2i)`=`"1"`; `sprintf('%d ',[1+2i 3+4i])`=`"1 3 "`;
  `sprintf('%.2f',3.5-1.5i)`=`"3.50"`; `sprintf('%g ',[1.5+0i 2.5])`=`"1.5 2.5 "`;
  `sprintf('%d',complex(7,0))`=`"7"`.
- Live guard: `src/lang/tests/sprintf_complex_test.cpp` (5 TEST_F) +
  `BuiltinKnownBug.SprintfComplex` flipped live. Parity:
  `tools/parity/specs/sprintf_complex.json` (correctness=OK). Smoke:
  `src/lang/tests/smoke/sprintf_complex_smoke.m`.

## Symptom
`sprintf` / `fprintf` throw when a numeric conversion is fed a complex value;
MATLAB formats the real part.

## Repro
```matlab
sprintf('%g', 1+2i)            % numkit: ERROR "Cannot convert complex ..."; MATLAB: '1'
sprintf('%d ', [1+2i 3+4i])    % numkit: ERROR "Not a double array";        MATLAB: '1 3 '
sprintf('%.2f', 3.5-1.5i)      % MATLAB: '3.50'
```

## Root cause
`formatCyclic` flattened arguments into scalar doubles via `toScalar()` /
`operator()(j)`, with no complex handling — so a complex element threw instead
of contributing its real part.

## References
- `src/lang/src/strings/format.cpp` (`formatCyclic`)
- MATLAB `doc sprintf` (numeric conversions use the real part of complex values)
