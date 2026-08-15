# io.writelines — multi-element string array writes only the first line

- **Status:** ✅ FIXED (2026-06-08)
- **Severity:** P2 (silent data loss on a documented input form)
- **Kind:** bug
- **Found:** 2026-06-08 while reading writelines during the io Engine-free API refactor

## Symptom
`writelines(strArray, file)` for a multi-element string array writes only the
**first** element as a single line, silently dropping the rest. The documented
contract (and MATLAB R2025b) is one line per element.

## Repro
```matlab
writelines(["a";"b";"c"], 'out.txt');
L = readlines('out.txt');
numel(L)        % MATLAB: 3        numkit (before fix): 1
% MATLAB file: a\nb\nc        numkit file (before fix): a
```
Confirmed empirically via `numkit.exe` against the pre-fix binary:
`numel=1`, `[1]=a` (b and c lost).

## Root cause
`writelines` (`src/toolboxes/io/src/text/extras.cpp`) dispatched as:
```cpp
if (lines.isChar() || lines.isString()) {   // catches ANY string array
    append(lines.toString());
} else if (lines.isCell()) { ... }
} else if (lines.isString()) { ...per-element loop... }   // UNREACHABLE
```
A string array satisfies `isString()`, so it was captured by the first branch,
which calls `Value::toString()`. `Value::toString()` on a `STRING` array returns
only element `[0]` (`value/src/value.cpp` — `return (*heap_->cellData)[0].toString();`),
so only the first line was written. The dedicated string-array per-element loop
(third branch) was dead code, shadowed by the first branch.

## Fix
- Hoisted the string-array per-element loop **above** the scalar branch, guarded
  by `lines.isString() && lines.numel() != 1`, so a multi-element string array
  writes one line per element while a scalar string / char row still takes the
  single-line `toString()` path. The previously-unreachable third branch was
  removed (folded into the new first branch). `src/toolboxes/io/src/text/extras.cpp`.
- Behaviour now matches MATLAB R2025b: `writelines(["a";"b";"c"])` → 3 lines.
  Cell-array and scalar-string forms unchanged.

## Test
- Live regression guard: `src/toolboxes/io/tests/io_extras_test.cpp` →
  `IoExtrasTest.WritelinesStringArrayOneLinePerElement` (writes a 3-element
  string array, asserts `readlines` round-trips to 3 elements a/b/c).

## References
- `src/toolboxes/io/src/text/extras.cpp` (writelines)
- `value/src/value.cpp` (`Value::toString` STRING → element[0])
- MATLAB `doc writelines`
