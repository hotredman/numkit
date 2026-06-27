# lang.cell — `c{:}` comma-separated-list expansion errors ("Cell index out of bounds")

- **Status:** ✅ FIXED (2026-06-27) — fully closed: first-class CSL on both backends; the former vector-variable-subscript gap is closed too (see "Gap closed" below)
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

## Fixed
- ✅ COMPLETE (2026-06-27) on BOTH backends across every CSL-consuming context, for
  the full 1-D and 2-D subscript surface:
  - **Concat** `[c{:}]` / `[c{vec}]` / `[c{1,:}]` / `[c{:,j}]`.
  - **Call args** `f(c{:})` sole, mixed `f(a, c{:}, b)`, `f(c{vec})`, `f(c{1,:})`.
  - **Cell literal** `{c{:}}` / `{c{vec}}` / `{c{1,:}}` / `{0, c{:}, 9}`.
  - **Multi-assign** `[a,b] = c{:}` / `c{vec}` / `c{r,:}` / `c{:,j}`.
  - **Multi-output call** `[a,b] = deal(c{:})` (and any named fn / builtin).
- TreeWalker: `cellBraceContents` (1-D + 2-D) feeds the matrix-literal builder, the
  cell-literal builder, the single- and multi-output call-arg builders, and
  `execMultiAssign`.
- VM opcodes: `HORZCAT_APPEND_CELL_CSL` / `_2D` (concat), `CELL_APPEND_ELEM` /
  `CELL_APPEND_SLICE_2D` (cell-literal + lowered call args), `CALL_VARARGS` /
  `CALL_VARARGS_MULTI` (call-arg splicing), `CELL_GET_MULTI` / `_2D` (multi-assign).
  Commits e43b5d89 / c3357596 / 5676f0c7 / b55bb32d / cf0651d6 / be739aa1 / 9453ca73 /
  8f8881e8 / f7da641d / 20472d63 / d32f644d / 43b32f4e. Live guards: the `CellTest`
  `CellCommaList*` family (both backends) + `BuiltinKnownBug.CellCommaListExpansion`.

## Gap closed (2026-06-27) — first-class CSL
The former deliberate gap — a **vector-VARIABLE** subscript in a call arg or cell
literal (`idx=[1 3]; f(c{idx})` or `{c{idx}}`) — is now closed on **both backends** by
the first-class CSL rework (dev-docs/CSL_FIRST_CLASS.md). A comma-separated list is a
transient `Value` (`ValueType::CSL`): the producer (`c{sub}`) emits one for a multi-
select (any subscript, including a runtime variable); splicing contexts flatten it; a
single-value context collapses it (1 → the element; 0 / >1 → "too many values"). The VM
mirrors the TreeWalker via the `compileNode` / `compileNodeExpand` split. `f(c{idx})` and
`[a,b]=f(x,c{idx})` route through `CALL_FLATTEN` / `CALL_FLATTEN_MULTI`, which flatten CSL
args and run the FULL target dispatch (incl. class ctors / object methods), so
`MyClass(c{idx})` works without regressing the hot `f(c{i})` scalar path (a no-CSL fast
path keeps it cheap). The old syntactically-multi restriction and its campaign opcodes
(`CALL_VARARGS`, `CELL_APPEND_ELEM`, `HORZCAT_APPEND_CELL_CSL`, …) are deleted. Guards:
the `CellTest` `CellCommaList*` + `CallArgVariableSubscript` + `CellLiteralVariableSubscript`
+ `MultiOutputCallVariableSubscript` families, both backends; `CslValue.*` value-level
tests. **The whole CSL surface (1-D + 2-D; concat / call-arg / cell-literal / multi-assign;
single + multi output; syntactic AND variable subscripts) is first-class on both backends.**

## References
- dev-docs/CSL_FIRST_CLASS.md (first-class CSL design + brick log)
- src/core/src/tree_walker.cpp (`execCellIndex`, `execNode` / `execNodeExpand`, splicers)
- src/core/src/vm.cpp (`CELL_GET` producer, `CALL_FLATTEN` / `HORZCAT_APPEND_FLATTEN` / `CELL_APPEND_FLATTEN`)
- MATLAB: comma-separated lists (`doc "comma-separated lists"`)
