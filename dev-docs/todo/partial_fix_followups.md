# todo: legacy RNG 'state'/'twister' generators (remainder of the portion-5 follow-ups)

*Kind:* tech-debt · *Status:* open (narrowed 2026-08-31 — most items done, see below) · *Surfaced:* 2026-08-31

> Lifecycle: open → done. On completion record the outcome in
> `dev-docs/memory/` per protocol and delete this file.

**Done in the "идентично MATLAB" pass (2026-08-31):**
- freqs(b,a): classic freqint (Grace 1990) — endpoints bit-exact vs
  R2025b, parity spec `freqs_2arg`; interior ≤2 ulp (libm pow/log10).
- rand/randn('seed', S): TRUE v4 generator bit-exact (Park–Miller +
  polar, first-of-pair), parity spec `rand_legacy_v4` (tol=0).
- rand/randn('seed') query form returns the last seed.
- varargin surface pinned (local functions, classdef methods,
  mid-list-as-plain-param — MATLAB semantics confirmed by probe).
- Full suite 13,156/13,158 (2 env skips), corpus gate 179/3-known.

**Remaining:**

1. **rand/randn('state', S)** — MATLAB v5: MT19937 with MATLAB's v5
   seeding (first draws probed: rand 0.9501…, randn −0.4326… — the
   classic v5 defaults) and v5 randn = Marsaglia-Tsang ziggurat with
   MATLAB's tables. Currently seeds the modern stream (documented).
2. **rand/randn('twister', S)** — MT19937 with init_by_array seeding of
   the 32-bit seed (first draws = numpy RandomState(S) values, probed:
   S=0 → 0.5488…). Currently seeds the modern stream (documented).
3. **Modern randn parity** — std::normal_distribution ≠ MATLAB ziggurat
   (pre-existing; the v5 ziggurat work above would close this too).
4. Related filed bugs open: `lang/handle-to-file-function-unresolved`,
   `lang/run-abs-path-sibling-resolution`, `lang/cell-growth-loses-value`
   (same resolution family).
