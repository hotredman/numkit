# clean_code.md — no crutches, no dirty solutions

**User rule (2026-09-06): костыли и грязные решения запрещены — только
чистый код.** A fix addresses the root cause in the right layer; it does
not patch the symptom just far enough to make today's test green.

Companion to [full_fidelity.md](full_fidelity.md): fidelity says WHAT
must match MATLAB; this file says HOW the code must look while doing it.

## Forbidden

- **Working around a defect instead of fixing it** — e.g. routing a call
  through `feval` because direct dispatch has a bug; catching an error
  and retrying through a back door; disabling the failing engine.
- **Teaching to the test**: special-casing exact inputs/shapes that the
  current corpus script or gtest happens to use, while the general
  behavior stays wrong.
- **Silent fallbacks**: swallowing an error, degrading to a slower/
  wrong path, or returning "something" instead of failing loudly.
- **Semantics-changing "make it not crash" numerics** — the rejected
  `pinv` fix for rank-deficient linprog is the canonical example: it
  un-crashed the solver by returning constraint-violating points.
- **Junk left in the tree**: dead code, commented-out experiments,
  debug prints, magic constants whose derivation is not stated, copy-
  pasted logic that should be one helper.
- **Layer violations / API contamination** as a shortcut (see
  [library_api.md](library_api.md)).

## Required

- Fix the root cause, in the layer that owns it; the fix must make the
  failing case AND its neighbors correct (a correct deconv fix repairs
  every strictly-proper call, not one book script).
- If the clean fix is genuinely out of scope for the current portion:
  file the bug and defer it explicitly (the honest-deferral protocol of
  full_fidelity.md). A **temporary bridge** is acceptable ONLY when all
  of the following hold:
  1. the underlying defect is filed in `bugs/opened/`;
  2. the bridge carries a comment naming that bug id;
  3. the bridge is searched for and removed when the bug closes
     (`git grep <bug-id>` finds it).
- Match the surrounding code's style, naming and comment density; a
  comment states a constraint the code cannot show, nothing else.

## Self-check at portion close

Ask once per portion: *"Is there anything in this diff I would be
uncomfortable defending line-by-line?"* — any workaround must be
grep-referenced to a bug id; anything unexplained is a reason to redo
it before committing, not after.

And one more question: *"Is every new branch in this diff covered by a
test in this same commit — dual-engine for execution changes?"*
(full_fidelity.md's portion-close coverage gate; user rule 2026-09-07).
Uncovered branches are the same class of debt as unexplained
workarounds: found now, cheap; found by the user, expensive.
