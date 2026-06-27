# First-class comma-separated lists (CSL) — design + brick plan

## Goal
Make a comma-separated list a **first-class transient Value** (`ValueType::CSL`), the
way MATLAB's evaluator models it, so that `c{:}` / `c{idx}` / `s.field` expansion works
**uniformly** in every context — including the one form the context-opcode campaign left
as a deliberate gap: a **vector-variable** subscript `f(c{idx})` / `{c{idx}}`.

This REPLACES the campaign's context-specific opcodes (`HORZCAT_APPEND_CELL_CSL[_2D]`,
`CELL_APPEND_ELEM`, `CELL_APPEND_SLICE_2D`, `CALL_VARARGS[_MULTI]`, `CELL_GET_MULTI[_2D]`)
with one model: **producers emit a CSL Value; splicing contexts flatten it; single-value
contexts collapse-or-error**. Those opcodes get deleted once their consumers route through
the CSL model.

## The model
- A CSL is a transient holding 0..N values. It is produced by `c{:}` / `c{idx}` /
  `c{r,:}` / struct-array `s.field`, and by nothing else.
- **Splicing contexts** flatten a CSL into their element list: call args (single +
  multi-output), `[..]` concat, `{..}` cell literal, multi-assign LHS, `varargout`.
- **Single-value contexts** must `collapse` a CSL: exactly 1 element -> that element;
  0 or >1 -> error ("too many"/"not enough"). These are RHS of `x = ..`, operator
  operands, conditions, index values — everywhere a lone value is required.
- A CSL must **never** be stored in a variable or survive a statement.

## Storage (DONE — brick 1)
`ValueType::CSL` reuses `HeapObject::cellData` (a `pmr::vector<Value>`); only the type tag
differs from CELL. `clone()` / `~HeapObject` already handle `cellData` generically, so no
lifetime changes were needed. API: `Value::csl(n, mr)`, `isCsl()`, `cslCount()`,
`cslAt(i)` (aliases `cellAt`). `mtypeName(CSL)` = "comma-separated list".

## TreeWalker plan
Natural fit: `execNode` returns a Value, which may now be a CSL.
- **Producers:** the `c{sub}` evaluator returns `Value::csl(...)` when the subscript
  selects N!=1 elements (replacing the per-context `cellBraceContents` plumbing).
- **Splicers:** `buildArgs`, `execMatrixLiteral`, `execCellLiteral`, `execMultiAssign`,
  return/`varargout` flatten any CSL operand.
- **Collapsers:** a `collapse(Value&&)` helper applied at every `execNode` call in a
  single-value position (assignment RHS, operator operands, `if`/`while` cond, index).

## VM plan — DATAFLOW approach (chosen)
The register file holds Values; a CSL in a register is a landmine for every opcode that
reads it as a single value. We do **not** guard every opcode. Instead:
- **Producers** (`CELL_GET[_2D]` when N!=1) write a CSL into their dst register.
- The **compiler runs a dataflow pass** marking which registers may hold a CSL (only
  producer outputs that flow directly into a splice context). Everywhere else it emits an
  explicit `COLLAPSE` opcode (CSL->element or error) so downstream opcodes never see a CSL.
- **Splice consumers** (`CALL`/`CALL_MULTI` arg scan, `HORZCAT`, `CELL_LITERAL`,
  multi-assign distribute) flatten CSL operands. The call ABI becomes: scan the arg
  registers, flatten any CSL into a runtime arg vector, dispatch. Keep a fast path when
  the dataflow proves no arg register holds a CSL (the common case) so hot calls are
  unaffected.

## Brick sequence
1. **Value CSL kind** — storage + API. *(DONE)*
2. **`COLLAPSE` opcode + `collapseCsl` helper** — VM opcode + Value-level collapse
   (1->elem, else throw; non-CSL pass-through). Inert until producers emit CSL. *(DONE 8a10353b)*
3. **TreeWalker producers + collapsers** — `c{:}`/`c{idx}` return CSL; add `collapse` at
   single-value `execNode` sites. Splicers updated to flatten. Suite green (TW only).
4. **TreeWalker splicers parity** — matrix/cell/call/multi-assign/varargout flatten CSL;
   delete TW `cellBraceContents` special-casing. Full TW behaviour matches today + adds
   `f(c{idx})` vector-variable.
5. **VM producers** — `CELL_GET[_2D]` emit CSL; **dataflow pass** in the compiler marks
   CSL-tainted registers and inserts `COLLAPSE`.
6. **VM splicers** — `CALL`/`CALL_MULTI`/`HORZCAT`/`CELL_LITERAL`/distribute flatten CSL;
   add the fast path gated by the dataflow taint bit.
7. **Delete campaign opcodes** — remove `HORZCAT_APPEND_CELL_CSL[_2D]`,
   `CELL_APPEND_ELEM`, `CELL_APPEND_SLICE_2D`, `CALL_VARARGS[_MULTI]`,
   `CELL_GET_MULTI[_2D]` and their compiler emitters once both backends route through CSL.
8. **Close the gap** — `f(c{idx})` / `{c{idx}}` with a vector-variable subscript work on
   both backends; flip `bugs/lang/cell-csl-expansion.md` (gap closed), update this doc.

## Risk
Centre of gravity: the VM dataflow pass (5) + the call-ABI flatten (6), plus the
`collapse` coverage (3) — a missed single-value site lets a CSL leak and corrupt a result.
Each brick keeps the full dual-engine suite green. Branch: `core-dev`.
