# tools/parity/ — MATLAB-parity validation harness

Cross-engine validation for numkit: every public function is checked against
**MATLAB R2025b** (and **Octave 11.1.0** when it ships the function) for
correctness, and timed for performance. This is **artefact #2 of the mandatory
4-artefact rule** (see `AGENTS.md`).

## Per-function validation — the inner loop

| File | What it does |
|------|--------------|
| `specs/<name>.json` | One spec per function/branch: `setup`, `expr`, `fingerprint`, `tol`, `iters`, `comment`. ~1500 of them — the accumulated parity corpus. |
| `run_parity.py` | The driver. Runs a spec on three engines (numkit native, MATLAB, Octave), compares output (`correctness=OK` / `N/A` / mismatch), measures wall-clock, and appends/updates the row in `PROGRESS.md` (+ `BENCHMARK.md`). |
| `PROGRESS.md` | Live parity map — every function with coverage + correctness + timing. The harness rewrites it in place. |
| `BENCHMARK.md` | Per-function timing (numkit vs MATLAB vs Octave). |

```
python tools/parity/run_parity.py tools/parity/specs/<name>.json
```

Must report `correctness=OK` before commit — **never** pass `--no-matlab`. If a
reference engine lacks the function, the run reports `correctness=N/A` (note it
in the spec `comment`).

## Coverage / gap analysis — the outer loop

Answers "what does MATLAB ship that numkit doesn't yet?" and feeds
[`../../bugs/missing.md`](../../bugs/missing.md):

```
extract_local_ref.py    →    diff_local_ref.py    →    bugs/missing.md
(MATLAB helpfuncbycat.xml     (diff that inventory       (missing / partial
 TOC  →  function list)        vs PROGRESS.md  →          functions, grouped
                               HAVE / MISS / SKIP)        by namespace)
```

- `extract_local_ref.py` — parses MATLAB R2025b's local TOC
  (`helpfuncbycat.xml`) into a category tree of sections + functions.
- `diff_local_ref.py` — diffs that inventory against `PROGRESS.md`, tagging
  each MATLAB function `HAVE` / `MISS` / `SKIP` (out-of-scope).

Re-run these two only when refreshing the MATLAB function inventory (e.g. after
a new MATLAB release) — the per-function loop above is the day-to-day path.

---

See also [`../../dev-docs/PARITY_AGENT_PROMPT.md`](../../dev-docs/PARITY_AGENT_PROMPT.md)
(autonomous-cycle runbook) and `AGENTS.md` (the 4-artefact rule).
