# todo: fieldtest graphics phase — add display/plotting repos back to the corpus

*Kind:* tech-debt · *Status:* open · *Surfaced:* 2026-08-30 (user decision: compute/processing only for the current phase)

> Lifecycle: open → done. On completion, record the outcome in
> `dev-docs/memory/` (per the AGENTS.md project-memory protocol) and
> delete this file — the todo list holds open work only.

**Problem.** The fieldtest sources list deliberately excludes graphics/display
repos (export_fig, matlab2tikz, ZoomPlot), and the harness harvest drops any
script containing a drawing call. Real-world plotting code therefore exercises
neither the parser surface nor the graphics API against MATLAB.

**Why deferred.** Phase focus: computation/processing correctness. Plot calls
print nothing to stdout, and an absent plot option must not fail an otherwise
computational script.

**Fix.** A later phase: add graphics/display companion repos to the catalog
(awesome-matlab-books) — or a supplementary local list if they are not book
companions — drop the display-token filter from `harness.py` harvest for a
graphics batch, and compare what is comparable (exit status, non-plot stdout,
figure COUNT via a headless figure-counter if one exists).

**Affected.** `fieldtest/fetch.py` (catalog filters), `fieldtest/harness.py`
(BAD_TOKENS display block).
