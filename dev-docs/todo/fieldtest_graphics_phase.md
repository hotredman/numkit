# todo: fieldtest graphics — compare figure CONTENT (the token filter is gone)

*Kind:* tech-debt · *Status:* open (narrowed 2026-08-31) · *Surfaced:* 2026-08-30

> Lifecycle: open → done. On completion, record the outcome in
> `dev-docs/memory/` (per the AGENTS.md project-memory protocol) and
> delete this file — the todo list holds open work only.

**Problem (remaining).** Plotting scripts now RUN in the fieldtest corpus
(2026-08-31, user decision): the harvest display-token filter was removed —
both engines execute graphics headless and the R4 workspace verdict is blind
to plots. What is still NOT compared: the figure CONTENT itself. numkit
emits `__FIGURE_DATA__` JSON for the IDE; MATLAB has its graphics object
model. A divergent `plot(x, y)` (wrong data, wrong options accepted/ignored)
is invisible to the current verdicts.

**Fix (a later phase).** Diff the figure stream: capture numkit's
`__FIGURE_DATA__` payloads, extract comparable structure (dataset x/y/z,
axes config options) and check the script's plotted data against the
workspace values MATLAB computed (the data plotted should equal the
variables saved — a cross-check that needs no MATLAB graphics API).

**Affected.** `fieldtest/harness.py` (capture + verdict hook), possibly a
`figurediff` step. Related: the CLI `__FIGURE_DATA__` stdout leak
(bugs/opened/apps/wasm-cli-figure-data-leak.md).
