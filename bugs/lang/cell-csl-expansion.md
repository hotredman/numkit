# lang.cell — `c{:}` comma-separated-list expansion errors ("Cell index out of bounds")

- **Status:** 🔴 OPEN
- **Severity:** P2 (a valid MATLAB form errors; CSL expansion is missing)
- **Kind:** bug
- **Found:** 2026-06-26 via the codegen CSL audit (multi-fire design item 1)

## Symptom
A brace index with a bare colon, `c{:}` (or a vector subscript `c{vec}`), should
expand to a **comma-separated list** (CSL) — multiple values that the surrounding
context splices in. numkit's interpreter instead errors "Cell index out of
bounds" in every CSL-consuming position (call args, concatenation). Explicit
single-element indexing `c{i}` works.

## Repro
```matlab
c = {3, 8, 4};
[c{:}]           % numkit: ERROR "Cell index out of bounds";  MATLAB: [3 8 4]
sum([c{:}])      % numkit: ERROR;                               MATLAB: 15
d = {3, 8};
max(d{:})        % numkit: ERROR "Cell index out of bounds";  MATLAB: 8  (= max(3,8))
d{1}             % numkit: 3   (explicit single index is fine)
```

## Root cause
`TreeWalker::execCellIndex` (src/core/src/tree_walker.cpp) treats a single-
subscript brace `c{sub}` as a SCALAR index: it evaluates `sub` and calls
`cellAt(checkedScalarIndex(sub.toScalar()))`. For a bare colon the subscript is a
childless `COLON_EXPR`; evaluating it does not yield "all elements", so
`checkedScalarIndex` sees an out-of-range value → "Cell index out of bounds".
There is no CSL machinery: `c{:}` / `c{vec}` should produce a MULTI-VALUE result
that the surrounding context (call-arg list, `[...]`, `{...}`, multi-assign)
splices in. The bytecode VM backend has the same gap.

## Suggested fix
A core (interpreter) feature, not a small patch: introduce a comma-separated-list
value (or an out-param "expansion" path) so `c{:}` / `c{vec}` yields N values, and
teach the CSL-consuming sites to splice it — function-call argument lists,
`[ ... ]` / `{ ... }` concatenation, and `[a,b,...] = c{:}` multi-assign. Both
backends (TreeWalker + bytecode VM) need it. Medium–large; spans the evaluator and
every consumer. Until then `c{:}` correctly errors (no silent wrong result).

## Impact on codegen
The codegen's correctness contract is the INTERPRETER (DESIGN.md §10), so the
codegen cannot compile `c{:}` expansion while the interpreter errors on it — doing
so would make compiled code MORE capable than the interpreter and FAIL the
differential gate (this was confirmed: a prototype `f(c{:})` expander returned the
correct value while the interpreter threw). The codegen therefore (correctly)
refuses `c{:}` — `CellCommaListRefusedUnderBridge`, documented in
src/codegen/DESIGN.md §10a. **Codegen CSL support is BLOCKED on this interpreter
feature.**

## References
- src/core/src/tree_walker.cpp (`execCellIndex`)
- src/codegen/DESIGN.md §10a (sound-refusal catalog) + `CellCommaListRefusedUnderBridge`
- MATLAB: comma-separated lists (`doc "comma-separated lists"`)
