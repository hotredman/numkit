# lang.[..] — all-empty concatenation returned an UNSET Value (assignment target never bound); empties ignored for dims

- **Status:** ✅ FIXED (HASH, 2026-09-06)
- **Kind:** bug
- **Severity:** P1 wrong result (silent: the variable simply stays undefined)
- **Found:** 2026-09-06 during the linprog portion — the rewritten quadprog m-source builds `A = [C; Be]` with empty C/Be for unconstrained problems and died with "Undefined function or variable"

## Symptom

Any all-empty bracket concatenation produced an UNSET value instead of
an empty matrix, so the assignment target never bound:

```matlab
clear;
a = [zeros(0,1); zeros(0,1)];
% numkit (pre-fix): "Undefined function or variable 'a'" on ANY later use
% MATLAB R2025b:  a is 0x1 double
```

Empty operands were skipped outright, so `[zeros(0,1); zeros(0,3)]`
also lost the width information entirely.

## Root cause

`Value::vertcat` / `Value::horzcat` (src/value/src/value.cpp) did
`if (elems[i].isEmpty()) continue;` in the dimension pass and then
`return Value();` when no non-empty input set the width — a
default-constructed (UNSET) Value, which the VM stores without binding
a variable. Additionally the fallthrough path forced `rows = 1` for
horzcat even when every operand was empty.

## Fix

MATLAB-probed empty rules (R2025b): empties participate with MAX
height/width in vertcat (`[zeros(0,1); zeros(0,3)]` → 0×3, no error;
`[]` neutral) and symmetric sums in horzcat; an all-empty concat
returns a PROPER empty (`0×cols` double / cell), never an unset Value.
Live guard: `ConcatTest.EmptyOperandsParticipateAndAllEmptyBinds`
(dual-engine, seven probed combinations).

## References

- `src/value/src/value.cpp` (Value::vertcat / Value::horzcat)
- Found via the quadprog ADMM rewrite (`A = [C; Be]` with empty inputs)
