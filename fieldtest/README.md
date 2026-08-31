# fieldtest/ — real-world differential testing vs MATLAB

Testing numkit against **real third-party MATLAB code** (GitHub), run through
both MATLAB R2025b and numkit (WASM CLI + native REPL), with every divergence
classified and routed into the repo's bug protocol. This is deliberately
complementary to:

- `tools/parity/` — *spec-driven* parity (curated calls, MATLAB R2025b ground
  truth, fingerprint diff);
- `examples/` — the *synthetic* corpus (written by the engine's authors —
  blind spots correlate). Fieldtest exists because synthetic corpora share the
  author's blind spots; real code does not.

## Layout

```
fieldtest/
  README.md        this file — the method + the workflow rules
  fetch.py         clone/update catalog repos into corpus/ + prepare work copy
  harness.py       dual-run (MATLAB vs numkit WASM vs native), classify, report
  signprobe.py     signature check: `help fn` (arg list) diff, MATLAB vs numkit
  reports/         committed run reports (JSON + summary per batch)
  runnable.json    COMMITTED — the run corpus: scripts verified to execute in
                   MATLAB (paths + verdicts only, no third-party source text)
  compare.py       comparison groups (R5): a curated group dual-run with a
                   human-readable PASS / KNOWN / NEW report
  compare/groups/  COMMITTED — one group per file: script paths + optional
                   `# known: <bug-id>` annotations
  corpus/          GITIGNORED — cloned repos (corpus/repos/), the prepared
                   work copy (corpus/work/), and the cached catalog.json.
                   Third-party code NEVER enters git.
```

Sources come from the external catalog
[awesome-matlab-books](https://github.com/hotredman/awesome-matlab-books)
(`catalog.json`: companion-code repositories for 75+ MATLAB books across 12
categories). The catalog is cached at `corpus/catalog.json` on first fetch and
reused from cache afterwards, so a batch stays reproducible against the
catalog it was fetched with; `--refresh-catalog` pulls a newer one.

## Workflow

1. **Fetch** — `python fetch.py` clones/updates every catalog repo into
   `corpus/repos/<owner--repo>` (filters: `--category <id>[,<id>]`,
   `--book <id>[,<id>]`; `--list` shows what's available), then prepares
   `corpus/work/`: a UTF-8-transcoded mirror (many real-world files are GBK;
   transcoding gives BOTH engines the same text — MATLAB on a non-Chinese
   locale garbles GBK just like numkit does, so the comparison stays fair).

2. **Qualify** — `python harness.py qualify [N] [filter]` harvests heuristic
   candidates (self-contained scripts, static token filter for interactive /
   system / nondeterministic constructs) and then **verifies runnability
   empirically**: each candidate is executed in MATLAB R2025b; the ones that
   run to completion form the run corpus, committed as `runnable.json`.
   Graphics calls are NOT excluded: both engines execute them headless
   (numkit runs its graphics system, MATLAB -batch creates invisible
   figures) and the R4 workspace verdict is blind to plots. This replaces
   trusting the static harvest: heuristic filters both miss runnable
   scripts and admit scripts that error in MATLAB (worthless for diffing —
   there is no ground truth to diverge from). Runnable = MATLAB exit 0;
   determinism is still probed later by the double numkit run at diff
   time. The catalog of runnable scripts is the stable comparison set
   batches track progress against.

3. **Run** — `python harness.py run [N] [filter]` executes each `runnable.json`
   script through the three engines (timeout-protected, determinism-probed by
   a double numkit run), then compares **workspaces via .mat files** (see
   [Comparison semantics](#comparison-semantics)), and writes `reports/` + a
   verdict per script:

   | verdict | meaning | action |
   |---|---|---|
   | `pass` | outputs match numerically | record timings (speed stats) |
   | `runtime-error` / `workspace-mismatch` | numkit fails or a numeric variable diverges where MATLAB succeeds | **file a bug** (see below) |
   | `parse-error` | numkit's parser rejects valid MATLAB | **file a bug** |
   | `absent-fn` | function not implemented | add to `bugs/missing.md` (a parity gap, NOT a defect per the Kind legend) |
   | `both-error` | both engines error | eyeball: same construct → error-parity PASS; different → bug on the diverging side |
   | `numkit-hang` / `matlab-timeout` | hang | bug (hang) |
   | `nondeterministic` | double-run differs | exclude, note in report |

4. **File every finding — self-contained.** Runtime/parse divergence →
   `bugs/opened/<ns>/<fn>.md` (Kind: bug) **+ a reproducing gtest** per the
   repo protocol (`DISABLED_` until fixed, live after). The bug file's Repro
   is a **minimal inline snippet** distilled from the corpus script — no
   fieldtest/corpus references (the bug must survive the corpus being wiped;
   see the Self-Contained Repro Rule in AGENTS.md). Absent function → a row
   in `missing.md`. Signature divergence →
   `bugs/opened/<ns>/<fn>-signature.md` (Kind: bug).

5. **Signature audit** — `python signprobe.py <fn> [<fn>…]` captures MATLAB's
   documented signature (`help fn`) and numkit's, diffs them, and reports the
   first divergent line. Any arg-list mismatch (missing option, different
   default, different nargout shape) becomes a signature bug. Batch-audit the
   functions a corpus batch actually used:
   `python signprobe.py $(python harness.py used-fns <report>)`.

## Comparison semantics

Results are compared as **workspaces via .mat files**, not as printed text:

- The harness appends `save` to each run — `run('<script>');
  save('out_<engine>.mat')` — in BOTH engines, with cwd set to a per-script
  temp dir (scripts themselves are never modified; the corpus stays
  untouched). A run therefore yields two .mat files: numkit's and MATLAB's.
- Both files are read by **scipy.io.loadmat** — an engine-neutral reader.
  Comparing numkit's output with numkit's own `load` would be circular.
- Compared: **numeric variables present in both files** (intersection).
  Per variable: class bucket (real / complex / logical) and shape must match
  exactly; values compared at rel 1e-9 (the bug playbook's algorithmic
  threshold); NaN == NaN and ±Inf compare equal to themselves. Non-numeric
  variables (char/struct/cell) are listed in the report but not compared —
  MAT type fidelity beyond numeric is out of scope for the diff verdict.
  (Class is compared at bucket level, not dtype level, because MATLAB
  R2025b packs integral-valued doubles into the smallest integer type when
  saving — v6 and v7 alike — and promotes back on load; the stored dtype is
  not a reliable comparison channel.)
- Variables present on only one side are reported as info (a numkit error
  mid-script is already a `runtime-error` verdict regardless).
- stdout and exit status are still captured — they drive the
  `parse-error` / `runtime-error` classes and diagnostics — but the numeric
  verdict comes from the workspace. **Print-format divergences (`fprintf`
  formatting, `disp` precision) no longer count as mismatches.**
- Determinism probe: the double numkit run is compared via its two .mat
  files (previously: stdout).
- The .mat artifacts are transient (gitignored temp dirs) and never
  committed — they are derivatives of third-party code; reports carry
  variable-level verdicts only.

## Comparison groups (R5)

`compare.py` is the **user-facing** front-end to the same comparison
machinery: a curated group of scripts, one dual-run, a human-readable
report distinguishing filed divergences from new ones.

```
python compare.py --list          # available groups
python compare.py mdadams         # dual-run the group
python compare.py mdadams -v      # + per-variable diff lines
```

- A group is `compare/groups/<name>.txt`: one script path per line
  (relative to `fieldtest/`), optional trailing `# known: <bug-id>`
  marking an already-filed divergence.
- Verdict per script: `PASS` (workspaces match) / `KNOWN (<bug>)`
  (diverges as filed) / `NEW <what>` (divergence not in the catalog —
  the actionable class). Exit code 1 iff any NEW.
- Groups are committed and stable across corpus rebuilds (paths are
  deterministic `owner--repo` names). Groups are cut from
  MATLAB-qualified scripts — qualify first, then curate.
- After a bug fix: remove its `# known:` annotation, re-run the group —
  the fix must turn the script PASS (the group is the family-level
  regression check; the gtest guard is the unit-level one).

## Sources policy

- **The catalog is the ONLY source of repositories.** No supplementary local
  picks — new sources are added to awesome-matlab-books itself (it is a
  curated project for exactly this), then picked up by `fetch.py
  --refresh-catalog`. The local corpus is disposable: `rm -rf fieldtest/corpus`
  must lose nothing that isn't rebuildable or already distilled into a bug
  file.
- Source of truth: the awesome-matlab-books catalog (CC BY-SA 4.0 as a
  compilation). Per-repo licenses vary — book companion code is often
  all-rights-reserved with no LICENSE file; that is acceptable here because:
- `corpus/` is gitignored; third-party code is never committed, shipped, or
  redistributed — only executed for testing. Reports contain paths + verdicts,
  not third-party source text. The cached `catalog.json` stays under
  `corpus/` for the same reason.
- `python_companions` (books whose companion code is Python) are deliberately
  not fetched.

## Speed measurement rules

- Report wall time per engine per script, but **subtract the MATLAB batch
  startup** (measured ≈3.2 s on this machine) for compute comparisons.
- Startup-dominated scripts (< 1 s MATLAB compute) are excluded from speed
  claims; the speed table is built from `pass` verdicts only (comparable
  work), and reports both numkit builds (WASM = the npm artifact, native =
  the engine ceiling).
