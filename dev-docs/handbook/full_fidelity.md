# full_fidelity.md — the complete-function rule

**User rule (2026-09-06):** if you implement a function, you implement it
COMPLETELY — behavior identical to MATLAB R2025b for every documented
argument form and input type, with test coverage that proves it. An
implementation that handles only the happy path which unblocked the
current corpus script / test is NOT done.

This extends the /loop four-artefact rule in
[`../../AGENTS.md`](../../AGENTS.md): artefact 3 already demands "one
TEST_F per documented branch"; this file defines what a "branch" is and
what to do when a branch is out of scope.

## What "complete" means

Before writing code, enumerate the contract from MATLAB — `help fn` +
`doc fn` + live probes of every form you are unsure about:

1. **Every documented signature variant**: argument counts, overloads,
   name-value options, output-count behavior (nargout 1/2/3+), command
   vs function form where MATLAB has both.
2. **Every input type branch**: `double`, `complex`, `logical`, integer
   classes, `char` where accepted; and the value edge cases — `0`/`-0`,
   `eps`, `Inf`/`-Inf`, `NaN`; empty `[]` in each shape (0×0, 0×1, 1×0,
   0×N); scalar vs row vs column vs matrix vs N-D.
3. **The output contract per branch**: values, SHAPE, and class typing
   (real vs complex vs logical — see the eig-Hermitian and num2str
   fixes: right values with wrong typing is still wrong).
4. **Error behavior**: for invalid input, MATLAB's own error (message
   family and which argument it names). A different rejection than
   MATLAB's is a divergence, not "input validation".

## Coverage obligation

Every enumerated branch is covered — not only the ones the current task
happens to exercise:

- **gtest**: one `TEST_F` per documented branch, extended to one per
  branch × input-type family wherever behavior differs (type
  promotion, empty handling, special values).
- **Dual-engine**: any change that alters how CODE EXECUTES (compiler,
  VM opcodes, TreeWalker paths, engine evaluation flow) gets coverage
  on BOTH backends — a `TEST_P` fixture instantiated over
  VM + TreeWalker. A VM-only test of an execution change is incomplete
  coverage, even when it passes.
- **Parity spec** (`tools/parity/specs/<fn>.json`) validating numeric
  behavior against MATLAB R2025b — expected values come from MATLAB
  probes, NEVER from your own reasoning ("trust the reference engine";
  three real bugs in cycles 65–75 had passed hand-written expectations).
- **Smoke** printing MATLAB-expected values inline per branch.

## Portion-close coverage gate (user rule, 2026-09-07)

**The tests are written BEFORE the portion closes, by the author —
completeness is not something the user should have to challenge.**
A portion is NOT done when the code works; it is done when the code
works AND its coverage is complete:

1. Enumerate the diff: every new branch, form, and input-type family
   the change introduces must have a named test in the same commit.
2. Execution changes: dual-engine (above) in the same commit.
3. A branch that cannot be covered through the public interface gets
   an inline comment saying why (honest-uncoverable) and a line in the
   commit message — never silence, never a fake test that doesn't
   reach the branch.
4. The "minimum targeted run" that gates the commit INCLUDES the new
   tests. Coverage added only after a user challenge means the portion
   was closed incomplete — flag it, don't normalize it.

Origin: portion 23b shipped the command-glue rewrite with VM-only
verification; the TreeWalker backend stayed broken
(`x-1` → "Undefined function: x") until the user asked
"все покрыл тестами?" — the missing dual test hid a live defect that
the audit then had to fix retroactively (commit 51b122ec8).

## Honest deferral

If a branch is genuinely out of scope for the current portion, it is
deferred EXPLICITLY, never silently:

- real defect / stub / missing output → `bugs/opened/<ns>/<fn>.md` with
  a self-contained MATLAB-vs-numkit repro + a `DISABLED_` guard in
  `known_bugs_test.cpp` (the bug-catalog protocol);
- parity gap (whole function absent) → PROGRESS.md, not bugs/;
- the divergence is then visible: in the bug catalog, in
  `--gtest_also_run_disabled_tests`, and in compare-group `# known:`
  annotations.

A function is DONE only when every documented branch either matches
MATLAB or carries a filed, guarded, reproducible divergence entry.

## Why this rule exists

The 2026-09-05 fidelity pass paid for earlier partial implementations:
`inline` shipped as a thin wrapper until probed as a real object;
`impulse(b,a,t)` shipped simple-poles-only (repeated poles silently
wrong); corpus scripts switch branches freely — a book script called
`buttap`/`cheb1ap`/`ellipap` overload forms nobody had probed. Every
partial implementation eventually meets the branch it skipped, in
front of a user.
