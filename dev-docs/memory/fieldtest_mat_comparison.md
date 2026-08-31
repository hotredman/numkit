# fieldtest: .mat workspace comparison (R4) — design decisions and gotchas

*Date:* 2026-08-31 · *Scope:* `fieldtest/harness.py`, the four-rule field
protocol (catalog-only sources, self-contained bug repros, MATLAB-qualified
run corpus, .mat comparison) agreed with the user.

## Context

The stdout fuzzy-diff compared printed TEXT: `fprintf` formats, `disp`
precision and mojibake masking were inseparable from real numeric
divergence (the sa-knapsack "one token diverges" ambiguity lived here for
a day). The user set rule R4: compare workspaces through saved .mat files.
Rules R1–R3 (external catalog as the only source; self-contained bug
repros; MATLAB-qualified runnable catalog) landed together — see
`fieldtest/README.md`.

## Decisions (and why)

- **Reader = scipy.io.loadmat** (Python-side, engine-neutral). Comparing
  numkit's output with numkit's own `load` would be circular: a loader bug
  would hide a compute bug.
- **Class compared at bucket level** (real / complex / logical), NOT dtype
  level. Discovered while building: **MATLAB R2025b packs integral-valued
  doubles into the smallest integer type when SAVING** (v6 AND v7 alike;
  verified: `x=[1 2;3 4]` stores as uint8, `x=[1.5 2.5]` as f8) and
  promotes back on load — `whos('-file')` reports the logical class, the
  raw stored class differs. scipy shows the stored class. A dtype-level
  compare false-positives on every integral double.
- **Values at rel 1e-9** (the bug-playbook algorithmic threshold), NaN==NaN,
  ±Inf equal to themselves. Workspace values carry full precision — no
  print rounding masks accumulated ULP drift, so the old 1e-6 stdout
  tolerance tightened to the documented threshold.
- **Determinism probe** = matdiff of two numkit runs (was: stdout equality).
- **Wrappers, not script edits**: numkit kinds run a generated
  `__nk_wrap_<kind>.m` (`run('<abs>'); save('<abs>')`) — the CLI executes
  exactly one file per invocation. MATLAB appends `; save(...)` to
  `-batch`. cwd stays the script's dir so sibling data files resolve.
- **runnable.json is committed** (paths + verdicts only — no third-party
  text), produced by `harness.py qualify` (MATLAB exit 0 = runnable).
  Static harvest no longer requires printed output — silent scripts are
  now comparable, the corpus grew accordingly.

## Triage lessons (first R4 batch)

- **Stochastic scripts**: sa-knapsack closed as not-a-defect — rand
  streams match MATLAB's seed-0 head exactly, but any 1-ulp difference in
  `exp()` at an acceptance boundary flips a decision, shifting the draw
  index alignment; trajectories diverge chaotically while the FINAL
  workspace (optimum) matches. Only final-state variables of stochastic
  code are comparable.
- The R4 smoke immediately surfaced real bugs the stdout diff could not
  localise: AHP eig-vector selection divergence (`w1 = e1` basis vector,
  wrong CR — open, awaiting the 8×8 repro) and the CLI
  run(abs)-sibling-resolution loss (open, CLI-bridge only — engine-level
  gtest is green, guard lives in `packages/numkit/test/`).

## Gotchas

- `fieldtest/corpus/` is disposable BY DESIGN (R1): every bug repro must
  survive `rm -rf fieldtest/corpus` (AGENTS.md Self-Contained Repro Rule).
  The corpus is rebuilt from the external catalog
  (`hotredman/awesome-matlab-books`, cached at `corpus/catalog.json`).
- Catalog `name` fields are free-form ('2nd edition'); derive clone dir
  names from the URL (`<owner>--<repo>`).
- eig column ORDER differs across engines (numkit idx=3, MATLAB idx=1 on
  the same matrix — both legal). Real code must select via
  `[~,i] = max(diag(D))`, never positional `V(:,1)`; pinned green by
  `EigAHPConsistentPerronSelection`.
