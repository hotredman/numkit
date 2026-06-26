# `bugs/` — one file per bug

Structured bug catalog. **Every bug gets its own `.md` file** here, with a
self-contained repro (numkit output vs MATLAB R2025b) so any session can
pick it up cold. This is the sole bug tracker (the old flat BUGS.md was retired —
its open items migrated here). The full MATLAB parity-gap inventory (missing / partial
functions) lives in [PARITY_GAPS.md](PARITY_GAPS.md).

## Layout

```
bugs/
  README.md              ← this file (index + conventions)
  <namespace>/<fn>.md    ← one bug (e.g. signal/dct-types.md)
```

Use `<fn>.md` when a function has one open bug; `<fn>-<aspect>.md` when it
has several distinct ones (e.g. `cceps-nd-phase.md`).

`<namespace>` is the toolbox or **source layer** the function lives in — i.e.
where its code (and its `known_bugs_test.cpp`) is found. Domain toolboxes keep
their own name (`signal/`, `image/`, `stats/`, `linalg/`, `control/`, `comm/`,
`optim/`, `wavelet/`, `ode/`, `io/`). The former `builtin` mega-library was
dissolved by the layering refactor and its defects are split across the three
source layers it became:

- **`math/`** — `src/math/` (trig, exp/log, special, poly, interp, integration,
  discrete/setops, reductions, complex): acos/asin, gamma, psi, log, sqrt,
  trapz, gradient, interp1/interpn, unique, histcounts, ismember/union, max/min…
- **`lang/`** — `src/lang/` (arrays, strings, format, types): cat, sort, find,
  diff, cumsum/cumprod/cummax/cummin, sprintf, str2double, integer casts…
- **`runtime/`** — `src/runtime/` (language-runtime builtins): accumarray,
  cellfun, func2str.

Each base layer keeps its gtests in its own module tree --
`src/math/tests/`, `src/lang/tests/`, `src/runtime/tests/`. The cross-cutting
batches and the base-layer `known_bugs_test.cpp` live in `src/bundle/tests/`.

## File template

```markdown
# <namespace>.<fn> — <one-line title>

- **Status:** 🔴 OPEN  |  ✅ FIXED (<commit>, YYYY-MM-DD)
- **Severity:** P1 wrong result · P2 missing feature · P3 minor/style
- **Found:** YYYY-MM-DD via <how>

## Symptom
What is wrong, in one or two sentences.

## Repro
​```matlab
<exact call>
% numkit: <output>
% MATLAB: <output>
​```

## Root cause
If known.

## Suggested fix
Approach + scope estimate; note any deferral reason (objects, core change,
large algorithm).

## References
Source files, related commits, related specs/tests.
```

## Severity legend

- **P0** crash / data loss
- **P1** wrong result (silently incorrect output)
- **P2** missing feature / option / output relative to MATLAB
- **P3** test-only / style

## Kind legend

Distinguishes a true defect from a parity feature-gap — so the count of
real bugs isn't inflated by unimplemented functions:

- **bug** — an IMPLEMENTED function produces a wrong/divergent result,
  crashes, or silently ignores a documented option. A genuine defect.
- **stub** — the function exists but a documented option/branch throws
  "not supported in this revision".
- **missing-output** — the function exists but a documented Nth output is
  missing ("Too many output arguments").
- **missing-fn** — the function is not implemented at all. This is a
  **parity feature-gap, not a defect** — also tracked in `PROGRESS.md`.
- **perf** — the function is CORRECT but significantly slower than MATLAB.
  Use a `**Slowdown:**` line (e.g. "1.2×–4.3× vs MATLAB") instead of a P0–P3
  severity, and reference a **benchmark** (`benchmarks/*.cpp`) rather than a
  `DISABLED_` gtest — timing assertions are too flaky for gtest. Always
  include the measured numbers + the bottleneck analysis.

  **When to flag as `perf`** (numkit is single-threaded; MATLAB is often
  multithreaded + MKL/FFTW, so a 1.5–3× gap on parallelisable ops is normal,
  not a bug):
  - **< 1.5×** — don't flag (noise / inherent).
  - **1.5×–3×** — flag only if the cause is FIXABLE (quadratic algorithm,
    redundant copies/allocs, a SIMD path that exists for sibling functions).
    If the only cause is "MATLAB threads, we don't", note it as *inherent*,
    low priority.
  - **≥ 3×** — flag (`perf` with measured numbers).
  - **≥ 10× OR worse big-O** (e.g. O(n²) where MATLAB is O(n log n)) —
    high priority; flag at ANY ratio.
  - An **algorithmic** inefficiency (worse big-O, allocs inside a loop) is a
    perf bug at ANY ratio — it scales and is fixable.

  Measure at a representative size (≥ ~10³–10⁴ elements), median of many
  iterations; ignore tiny arrays (wrapper overhead dominates both engines).
  Slowdown sub-scale: **S1** ≥10× or worse-big-O · **S2** 3–10× · **S3**
  1.5–3× with a fixable cause.

Add `- **Kind:** <kind>` to each file (right after Severity).

## Every bug also gets a test

**Found a bug → add a test.** Each OPEN bug has a matching `DISABLED_`
gtest in `src/toolboxes/<lib>/tests/known_bugs_test.cpp` that asserts the
MATLAB-correct behaviour. Disabled means it does NOT run in the normal
suite (the green baseline stays green), but it is visible
(`YOU HAVE N DISABLED TESTS`) and **fails when force-run**
(`--gtest_also_run_disabled_tests`), proving it captures the bug. When the
bug is fixed, just remove the `DISABLED_` prefix — the test becomes a live
regression guard with zero extra work.

Run all known-bug tests (to watch them fail until fixed):
```
numkit_gtest.exe --gtest_also_run_disabled_tests --gtest_filter='*KnownBug*'
```

## Lifecycle

1. Find a bug → create `bugs/<ns>/<fn>.md` (status OPEN) with full repro,
   AND add a `DISABLED_` test in `src/toolboxes/<ns>/tests/known_bugs_test.cpp`.
2. Fix it (4 artefacts) → remove `DISABLED_` (or promote the assertion into
   the function's own test file), flip the md status to ✅ FIXED with the
   commit hash, and update the index row. Keep the md (repro stays useful).

## Index

**Tally (121 entries):** ✅ 105 fixed · 🔴 16 open = **5 bug** + 1 stub +
1 missing-output + **8 missing-fn** + 1 perf (the 8 missing-fns are parity
feature-gaps, not defects — also in PROGRESS.md; perf = correct-but-slow).

> **Full parity-gap inventory:** the 30 missing-fn rows below are the *curated /
> notable* gaps. The complete list of **839 missing + 25 partial** MATLAB
> functions (migrated out of PROGRESS.md, grouped by namespace) lives in
> [PARITY_GAPS.md](PARITY_GAPS.md). Those are parity gaps, **not defects** —
> they are NOT counted in the tally above.

### ✅ FIXED (100)

| Kind | Bug | Sev | Notes |
|---|---|---|---|
| bug | [lang/zeros-size-args](lang/zeros-size-args.md) | P2 | ✅ FIXED: the shared array-creation dim parser (`parseDimsArgs`/`parseDimsArgsND` in `numkit::ops`, used by zeros/ones/nan/inf/true/false/eye) mishandled edge-case sizes — a negative dim cast a negative double to size_t (UB → bad_alloc) and a non-integer dim silently truncated. Added `toDim()`: checks finite+integer BEFORE the cast (throws "Size inputs must be integers." on non-integer/non-finite), clamps a negative integer to 0 (empty array). Matches MATLAB R2025b: `zeros(-1,3)`→0×3, `zeros(2,3,-1)`→2×3×0, `zeros(2.5)`→error. Found during codegen work. 12400/0 + parity OK (2026-06-21) |
| bug | [lang/anonymous-multi-output](lang/anonymous-multi-output.md) | P2 | ✅ FIXED: anonymous fns now forward nargout. (1) core varargout (RET_VARARGOUT — `function varargout=f` dynamic-count returns). (2) compileAnonFunc lowers `@(p) g(...)` (global-fn call body) to `varargout=__nk_fwd_call__(nargout,'g',args)` — helper resolves via findExternal (import-aware, so toolbox fns like median work), returns n results in a cell that RET_VARARGOUT expands. Captured-handle/param callees keep single-output (composition unaffected). `[a,b]=(@(x)deal(x+1,x-1))(5)`→6,4. Unblocked fmincon nonlcon. 12397/0 (2026-06-19) |
| bug | [lang/multi-output-handle-call](lang/multi-output-handle-call.md) | P2 | ✅ FIXED: `[a,b]=h(x)` for a handle variable failed on the VM (resolved the name as a function). Added CALL_INDIRECT_MULTI opcode (handle resolution + nout frame-push) + the known-var gate in compileMultiAssign + execCallIndirectMulti. Named/user-fn handles now dispatch multi-output: (@size)(ones(2,3))→[2 3], @userfn→[a,b]. Remaining sub-gap: anonymous fns don't forward nargout (needs varargout) → split to lang/anonymous-multi-output. No regressions (12394/0) (2026-06-19) |
| missing-fn | [optim/fmincon](optim/fmincon.md) | P2 | ✅ FIXED: constrained minimization fmincon was missing (last of the constrained-solvers cluster). Embedded-.m SQP reusing quadprog as the QP subproblem: FD gradient + BFGS Hessian, step from min 0.5 d'Bd+g'd s.t. linearized constraints, backtracking line search on f (QP keeps linear/bound feasible). Parity on solution: bounds→[0 0], lin-ineq→[1 1], eq→[1 1], box-corner→[2 0]. SCOPE: nonlinear constraints (nonlcon) rejected — [c,ceq]=nonlcon(x) multi-output handle call unsupported by VM (see lang/multi-output-handle-call) (2026-06-19) |
| missing-fn | [optim/constrained-solvers](optim/constrained-solvers.md) | P2 | ✅ FIXED (cluster): fminunc + quadprog + linprog + fmincon all implemented (split into per-fn files). Cluster index resolved (2026-06-19) |
| missing-fn | [optim/linprog](optim/linprog.md) | P2 | ✅ FIXED: linear program linprog(f,A,b,...) was missing (split from constrained-solvers). Solved by proximal (Tikhonov) regularization reusing the quadprog active-set: min f'x + (ε/2)‖x‖² (ε=1e-9). EXACT (not O(ε)): at a vertex optimum n active constraints determine x independent of εI, so it returns the exact vertex (ε-invariant 1e-6..1e-10). Matches MATLAB on unique optima: lower-bound→[1 1], classic max→[4 0]/-12, bounded→[0 4], box→[2 3]. CAVEAT: degenerate optimum → min-norm point (MATLAB returns a vertex; objective matches); unboundedness heuristic only. Full simplex = follow-up (2026-06-19) |
| missing-fn | [optim/quadprog](optim/quadprog.md) | P2 | ✅ FIXED: quadratic program quadprog(H,f,...) was missing (split from constrained-solvers). Embedded-.m primal active-set for strictly-convex H (PD): no Phase-1 (starts from -H\\f), each iter solves the KKT saddle system [H B';B 0][x;λ]=[-f;c] over the working set, adds most-violated inequality / drops most-negative multiplier until KKT. Bounds folded into inequalities. Unique optimum -> matches MATLAB exactly across ALL constraint types: unconstrained→[1 1], ineq→[0.5 0.5], eq→[1.5 1.5], bounds→[0.3 0.3], mixed H→[-1/3 4/3], 2 active ineqs→[0.3 0.7] (2026-06-19) |
| missing-fn | [optim/fminunc](optim/fminunc.md) | P2 | ✅ FIXED: unconstrained gradient minimizer fminunc(fun,x0) was missing (split from constrained-solvers cluster). Embedded-.m BFGS quasi-Newton with central-difference gradient + Armijo line search (inverse-Hessian H, reset to I on non-descent). Like fminsearch but gradient-based. Parity on solution: parabola→3, quad bowl→[1 -2] (fval 3), Rosenbrock→[1 1]. Output mirrors x0 orientation. Supplied-gradient form + options deferred (2026-06-19) |
| missing-fn | [optim/nonlinear-lsq](optim/nonlinear-lsq.md) | P2 | ✅ FIXED: lsqnonlin/lsqcurvefit (nonlinear least squares) were missing. Embedded-.m Levenberg-Marquardt reusing the fsolve LM core, but terminating at the LSQ minimiser (handles over-determined residuals). lsqcurvefit=lsqnonlin(@(p)fun(p,x)-y,p0). Outputs [p,resnorm,residual,exitflag]. Parity on solution: lsqnonlin linear→[1 2] resnorm 0, Rosenbrock→[1 1], noise-free sin→[2 1.5]; a*exp(b*x) fit has FLAT minimum (params loose, resnorm tight 0.001248164767 matched 8 digits). Bounds deferred (rejected) (2026-06-19) |
| missing-fn | [optim/fsolve](optim/fsolve.md) | P2 | ✅ FIXED: nonlinear system solver fsolve(fun,x0) was missing. Embedded-.m Levenberg-Marquardt (forward-diff Jacobian, (J'J+λdiag)dx=-J'F, adaptive λ), mirroring the fzero/fminsearch pausable-objective pattern. Parity on the SOLUTION (root), not iterates. Verified vs MATLAB R2025b: x^2-2→1.41421356, 2x2 unit-circle→[0.70710678 0.70710678] (ef=1), Rosenbrock system→[1 1], 3-var multi-root system from [1 0 4]→[1 2 3] (same root as MATLAB). Output mirrors x0 orientation. options arg accepted+ignored (2026-06-19) |
| bug | [signal/resample-values](signal/resample-values.md) | P1 | ✅ FIXED: resample produced garbage (sum 10.87, ramped from ~0) — old custom Hamming windowed-sinc FIR with no group-delay compensation. Rewrote as MATLAB resample.m reusing the shipped firls/kaiser/upfirdn (all bit-exact): h = p*firls(2N*pqmax,[0 2fc 2fc 1],[1 1 0 0]).*kaiser(L,5)/sum(...) normalised so sum(h)=p (the KEY: MATLAB normalises over ALL taps, not the polyphase branch — a 1.0006 factor was the whole filter discrepancy), then upfirdn + group-delay trim to ceil(Lx*p/q), GCD-reduced first. Bit-exact vs MATLAB R2025b: repro [1.00061 ... 4.24029] + 2/1, 1/2, sine, DC, GCD-reduce. Strengthened the old length-only parity spec (2026-06-19) |
| missing-fn | [comm/analog-demodulators](comm/analog-demodulators.md) | P2 | ✅ FIXED (all 5): pmdemod/fmdemod/amdemod/ssbdemod (2026-06-19) + now mskdemod. MSK coherent differential demod: bit_k = (sum of within-symbol angle(y[n]*conj(y[n-1])) > 0) — phase-increment decision, invariant to constant rotation + noise-robust, ini_phase only feeds phaseout. numkit mskmod is bit-exact w/ MATLAB so demod matches MATLAB R2025b (20-bit random clean+noisy incl. last bit). Returns [z,phaseout], input orientation preserved. nondiff deferred (mskmod has no nondiff path). (2026-06-19) |
| missing-fn | [image/corner](image/corner.md) | P2 | ✅ FIXED: corner-point detection was missing (cornermetric shipped). corner(I) wraps cornermetric: local maxima > QualityLevel*max -> connected-peak centroids -> strength-descending sort (ties column-major) -> up to N [x y]=[col row] coords. Border excluded naturally (metric <=0 < thr). numkit cornermetric matches MATLAB to ~1e-8 (not bit-exact + ~1-ULP corner asymmetry), so the strength sort quantises to ~1e-9*max to reproduce MATLAB's bit-exact-symmetric equal-corner ordering. Verified vs MATLAB R2025b: square -> [6 6;6 15;15 6;15 15], two-contrast squares -> 8 strong-first, corner(W,1) -> strong-at-high-col (strength beats position), N truncation, MinimumEigenvalue (2026-06-19) |
| missing-fn | [wavelet/wavedec2-family](wavelet/wavedec2-family.md) | P2 | ✅ FIXED: 2-D multilevel DWT (wavedec2/waverec2/appcoef2/detcoef2) was missing. New TU dwt/multilevel2.cpp iterates the single-level dwt2 N times on the LL band and packs the MATLAB [C,S] layout (coarsest-first vector + (N+2)x2 size matrix); appcoef2 reconstructs via idwt2, detcoef2 slices H/V/D (+ 'all'), waverec2 = appcoef2 level 0. Built on dwt2/idwt2 so bior/rbio work in 2-D too. Verified vs MATLAB R2025b: db1 4x4 (c(1)=7, H=-1, V=-4, A(2,2)=27), db2 8x8 N=2 (numel(c)=139, appcoef2 L2=16.4557713660, detcoef2 L1=-0.8660254038), non-square, bior2.2 round-trip (2026-06-19) |
| stub | [wavelet/dwt-biorthogonal](wavelet/dwt-biorthogonal.md) | P2 | ✅ FIXED: bior*/rbio* wavelet families were rejected (only haar/db/sym/coif known). New TU filter/biorfilt.cpp tabulates the four DISTINCT filters (Lo_D/Hi_D != Lo_R/Hi_R) for all 15 bior + 15 rbio families (CDF spline coeffs, public math); wavelet_filters() falls back to bior_filterbank() on an orthogonal-table miss. The dwt/idwt/wavedec/waverec machinery already threaded all four filters independently, so the whole family lit up at once. All 30 families bit-exact vs MATLAB R2025b wfilters (worst diff 0); dwt/wavedec/round-trip parity OK; bior1.1==Haar (2026-06-19) |
| missing-fn | [linalg/funm](linalg/funm.md) | P2 | ✅ FIXED: general matrix function funm(A,fun) (scalar fun OF a matrix, not element-wise) was undefined. Implemented via eigendecomposition — F=V·diag(fun(diag(D)))/V where [V,D]=eig(A), real(F) for real A — as an embedded .m reusing the eig builtin. Matches MATLAB exactly for diagonalizable real-eigenvalue matrices: funm(diag(2,3),@exp)=diag(7.38906,20.0855), funm([1 2;3 4],@exp)=expm, @sin/@cos exact, funm(sym,@sqrt)=sqrtm. MATLAB's funm errors on @sqrt/anon (its generic path passes a derivative-order arg); ours is more lenient. DEFERRED: complex-eigenvalue + defective matrices error (numkit eig [V,D] needs Francis QR) — would need full Schur-Parlett. Parity OK vs MATLAB R2025b (2026-06-19) |
| missing-fn | [math/numerical-integration-nd](math/numerical-integration-nd.md) | P2 | ✅ FIXED: quadgk/integral2/integral3/quad2d were undefined. Built on numkit's 1-D adaptive Gauss-Kronrod integral: integral2(fn,a,b,c,d) = iterated quadrature (outer x-sweep, inner y-sweep of fn(x,·)) by composing FnHandles in the math layer (inner 1-arg callback wraps the user's 2-arg fn with captured x); integral3 triple-nests; quadgk = the 1-D integral; quad2d = older name for integral2. 'AbsTol' accepted; works on both backends. Parity OK vs MATLAB R2025b: integral2(x·y)=0.25, integral2(exp(x·y))=1.317902151454, integral3(x+y+z)=1.5, quadgk(exp(−x²))=0.746824132812427 (2026-06-19) |
| bug | [signal/hilbert-nonpow2](signal/hilbert-nonpow2.md) | P1 | ✅ FIXED: hilbert(x) returned a wrong analytic signal for non-power-of-2 lengths (constant envelope lost, \|z\|≈0.75 instead of 1) — hilbertBuf padded to nextPow2(N), transformed the padded length, then sliced N. Root: zero-padding changes the spectrum so the doubled-positive-freq mask + ifft operate on the wrong-length DFT (pow2 N unaffected → hid behind pow2 test inputs). Fix: transform at length N — pow2 keeps the fftRadix2 fast path, non-pow2 routes through the general fft/ifft (Bluestein); mask handles even/odd parity. Parity OK vs MATLAB R2025b: hilbert([1:6]') imag=[2.3094,-1.1547,…], L=100 tone \|hilbert\|=1, ramp \|z(1)\|=128.524726. Also fixes envelope/ssbmod/instfreq/vibration for non-pow2 + unblocks pmdemod/fmdemod. Full suite 12294 pass / 0 fail (2026-06-19) |
| missing-fn | [wavelet/ddencmp](wavelet/ddencmp.md) | P2 | ✅ FIXED: ddencmp (default denoise/compress params) was undefined. Noise estimate from finest-detail of a 1-level db1 dwt: σ̂=median(\|cD1\|)/0.6745. 'den': thr=sqrt(2·log(n))·σ̂ (universal), sorh='s'; 'cmp': thr=median(\|cD1\|), sorh='h'; keepapp=1. numkit dwt('db1') matches MATLAB's finest detail bit-for-bit (incl. odd-length) so thresholds match exactly. Parity OK vs MATLAB R2025b: den/wv [1 2 3 8 3 2 1 2]→2.137919772574, cmp→0.707106781187, den [1..5]→1.880854323469. 'wp' deferred. Closes the wentropy/ddencmp cluster (2026-06-19) |
| missing-fn | [wavelet/wentropy](wavelet/wentropy.md) | P2 | ✅ FIXED: wentropy (coefficient entropy "cost") was undefined. Closed-form additive entropy: 'shannon'=-Σs²·log(s²) (zeros→0), 'log energy'=Σlog(s²) nonzero, 'threshold'(P)=#{\|s\|>P}, 'sure'(P)=n-2·#{\|s\|≤P}+Σmin(s²,P²), 'norm'(P≥1)=Σ\|s\|ᴾ. Parity OK vs MATLAB R2025b on [0.5 -0.3 0.8 0 -0.1 0.2]: shannon=1.023719, logenergy=-12.064573, threshold(0.2)=3, sure(0.2)=0.17, norm(1.5)=1.354477; shannon([1 2 3 4])=-69.6816182. Split from wentropy-ddencmp; ddencmp still open (2026-06-19) |
| missing-fn | [comm/syndtable](comm/syndtable.md) | P2 | ✅ FIXED: syndtable (syndrome decoding table / coset-leader lookup) was undefined. Returns the 2^(n-k)×n table whose row s+1 is the min-weight error with syndrome s=bi2de(mod(H·eᵀ,2),'left-msb'); enumerate error patterns by ascending Hamming weight, within a weight by lexicographic bit position, first to reach each syndrome wins (lowest-position tie-break = MATLAB's). Parity OK vs MATLAB R2025b (exact full-table match incl. weight-2 leaders + ties): (7,4) Hamming→8×7 all-weight-1; 3×4 code→weight-2 leaders, s=3→[1 0 0 1]; repro 3×6→8×6 (2026-06-19) |
| missing-fn | [wavelet/upcoef](wavelet/upcoef.md) | P2 | ✅ FIXED: upcoef (single-branch coefficient reconstruction) was undefined. Synthesis cascade: per level interleave zeros ([x0,0,x1,0,…]) + full-conv with Lo_R (or Hi_R on the first level for a detail branch); no idwt trim, so length grows to 2n-1+\|F\|-1. Filters from wavelet_filters; optional L center-trims; N=0 returns X. Parity OK vs MATLAB R2025b: upcoef('a',5,'db1',2)=[2.5 2.5 2.5 2.5], upcoef('d',5,'db1',2)=[2.5 2.5 -2.5 -2.5], upcoef('a',[1 2],'db2',1)=[0.482963 0.836516 1.190074 1.543628 0.448288 -0.258819]. Closes the wenergy/upcoef cluster (2026-06-19) |
| missing-fn | [wavelet/wenergy](wavelet/wenergy.md) | P2 | ✅ FIXED: wenergy (energy distribution of a wavedec) was undefined. Ea=100·‖cA_N‖²/‖C‖² (approximation), Ed(i)=100·‖cD‖²/‖C‖² per detail; Ea+sum(Ed)=100. Ordering gotcha: C packs details coarsest-first (cD_N…cD_1) but MATLAB's Ed is by level number (Ed(1)=level 1 finest … Ed(N)=coarsest) — reverse of the C walk. Parity OK vs MATLAB R2025b: [1..8] db1 L2→Ea=95.0980392, Ed=[0.98039216,3.92156863]; 1:16 db1 L3→Ea=94.385, Ed=[0.267,1.069,4.278]; sin db2→Ea=34.188. Split from wenergy-upcoef; upcoef still open (2026-06-19) |
| missing-fn | [control/covar](control/covar.md) | P2 | ✅ FIXED: covar (steady-state output+state covariance under white noise) was undefined. Reuses lyap/dlyap (pullABC): state cov Q solves the gramian Lyapunov eqn with B·W·Bᵀ (continuous→lyap, discrete→dlyap); output cov P=C·Q·Cᵀ (+D·W·Dᵀ discrete; ∞ if continuous & D≠0). W scalar (W·I) or m×m. Returns [P,Q]. Parity OK vs MATLAB R2025b: 1/(s+1) W=1→P=0.5, 2-state→P=1.41667 Q=[0.5 0.3333;0.3333 0.25], linear in W, discrete→9.570351. Closes the control namespace (no OPEN control bugs left) (2026-06-19) |
| bug | [control/zpk-empty-zeros](control/zpk-empty-zeros.md) | P2 | ✅ FIXED: zpk([],poles,k) with no finite zeros dropped the gain k (num came out all-zero → zero system after any tf op). Root: math::poly([]) returns an empty row not [1], collapsing num=k·poly([]). Localized guard in zp2tf (conversion.cpp): empty math::poly(z) → num=[1] before the k multiply, so num=[k] (zero-padded downstream). Non-empty-zero path unchanged. Parity OK vs MATLAB R2025b: zpk([],[-1 -2],2)→[0 0 2], zpk([],[-1 -2 -3],1)→[0 0 0 1], zpk(-5,...)→[0 0 1 5]. Found while wiring allmargin (2026-06-19) |
| missing-fn | [control/allmargin](control/allmargin.md) | P2 | ✅ FIXED: allmargin (all gain/phase/delay margins + Stable as a 7-field struct) was undefined. Unlike margin (Bode-grid interp), evaluates the EXACT open-loop G(jω)=num(jω)/den(jω): fine log scan brackets sign changes of \|G\|−1 (gain crossovers→phase+delay margins) and Im(G) with Re(G)<0 (phase crossovers→gain margins), each bisected on the exact response (matches MATLAB even at sharp resonances a grid misses). DelayMargin=PM(rad)/ω_gc; Stable=roots(den+num) all in LHP. Parity OK vs MATLAB R2025b: 1/((s+1)(s+2)(s+3))→GM=60 at √11, no gain crossover, Stable=1; 1/(s(s+1)(s+2))→GM=6, PM=53.41°, DM=2.0913. Found control/zpk-empty-zeros (zp2tf) en route (2026-06-19) |
| missing-fn | [control/minreal](control/minreal.md) | P2 | ✅ FIXED: minreal (minimal realization) was undefined. tf/zpk: roots(num)/roots(den), greedily cancel each pole against nearest zero within rel tol (default sqrt(eps)), rebuild num=num_lead·∏(surviving zeros) / den=den_lead·∏(surviving poles) via complex ∏(s−r) expansion (real part — conjugate pairs cancel symmetrically), gain preserved via leading-coeff ratio → tf. SISO ss: ss2tf→cancel→tf2ss → reduced-order ss (order+transfer-fn parity, realization non-unique); MIMO ss throws. Parity OK vs MATLAB R2025b: (s+1)/(s+1)²→[0 1]/[1 1], 2(s+1)/(s+1)²→[0 2]/[1 1] (gain), complex (s²+1)/((s²+1)(s+3))→[0 1]/[1 3], SISO ss uncontrollable mode order 2→1 (2026-06-19) |
| missing-fn | [control/initial](control/initial.md) | P2 | ✅ FIXED: initial (initial-condition response) was undefined. Reuses the existing ZOH state propagator (simulate) with u≡0 and x(0)=x0, output y=C·x — initial(sys,x0[,tArg]), tArg semantics match step/impulse (Empty→auto grid, scalar→tFinal, vector→explicit), returns [y,t,x] by nargout. Parity OK vs MATLAB R2025b on the explicit grid (machine precision): 1st-order A=−2,x0=1→y=e^{−2t}, e^{−6}=0.002478752177; 2-state→y(t=1)=0.6004235991. Auto-grid horizon matches to ~1e-7 (heuristic, same caveat as step/impulse auto-time; explicit-t exact) (2026-06-19) |
| missing-fn | [control/hinfnorm](control/hinfnorm.md) | P2 | ✅ FIXED: hinfnorm (H∞ norm ‖G‖∞=sup_ω σmax(G(jω))) was undefined. Bruinsma–Steinbuch Hamiltonian test + bisection on γ: γ is an upper bound iff R=γ²I−DᵀD≻0 and M(γ)=[Ā, BR⁻¹Bᵀ; −Cᵀ(I+DR⁻¹Dᵀ)C, −Āᵀ] (Ā=A+BR⁻¹DᵀC) has no purely imaginary eigenvalue. No frequency sweep (exact test catches resonances a grid misses); M's spectrum via charPoly→roots (all real, no complex SVD); lower bracket = DC/∞ gains via lower-bound-safe Rayleigh power iteration. Returns Inf for jω-axis/unstable poles. Parity OK vs MATLAB R2025b: 1/(s+1)→1, ±i→Inf, resonance 1/(s²+0.1s+1)→10.012523, static→0.8333, D=0.5→1.5. Closes the original lqr/hinfnorm/dlqr/gram cluster. Discrete deferred (2026-06-19) |
| missing-fn | [control/lqr-dlqr-gram](control/lqr-dlqr-gram.md) | P2 | ✅ FIXED: lqr/dlqr (optimal LQR gain) + gram (controllability/observability gramian) were undefined. Thin wrappers — no new numerics: lqr [K,S,P]=lqr(A,B,Q[,R]) calls care and re-orders {X,L,G}→[gain, Riccati solution, closed-loop poles]; dlqr the same on dare; gram(sys,'c'\|'o') solves the gramian Lyapunov eqn ('c': A·Wc+Wc·Aᵀ+B·Bᵀ=0→lyap(A,B·Bᵀ); 'o': lyap(Aᵀ,Cᵀ·C); discrete→dlyap, reusing pullABC). R defaults to I. Parity OK vs MATLAB R2025b: lqr K=[1,√3] poles -0.866±0.5i; dlqr sum(K)=0.71004388; gram Wc=[0.5 0.3333;0.3333 0.25] sum=1.41667 residual 0. Split off hinfnorm (own algorithm) → control/hinfnorm.md. Deferred: lqr cross-term N + lqr(sys,…) form (2026-06-19) |
| missing-fn | [control/care-dare](control/care-dare.md) | P2 | ✅ FIXED: continuous/discrete algebraic Riccati solvers care/dare were undefined. Implemented by the matrix sign-function method (no Schur ordering — only inv + a small LS solve on toolboxes/control's own LU kernel). care: Hamiltonian H=[A -BR⁻¹Bᵀ; -Q -Aᵀ] → scaled Newton Z←½(cZ+(cZ)⁻¹) to sign(H) → stable subspace W1=[Z12;Z22+I], W2=-[Z11+I;Z21], X=W1\W2, symmetrize. dare: no Hamiltonian → build symplectic matrix + Cayley transform C=(S-I)(S+I)⁻¹ (unit disk→LHP), reuse the same sign machinery; needs A nonsingular (singular-A QZ path deferred, throws). Outputs match MATLAB [X, L=eig(A-BG) poles, G gain]; R defaults to I. Parity OK vs MATLAB R2025b: care X(1,1)=√3, trace=3.46410161513776, poles -0.866±0.5i, residual ~4e-16; dare X(1,1)=2.94712296779058, trace=7.56025722770319, \|poles\|=0.4221. Lets lqr/dlqr become thin wrappers (still open) (2026-06-19) |
| stub | [linalg/schur-nonsymmetric](linalg/schur-nonsymmetric.md) | P2 | ✅ FIXED: schur(A) threw on non-symmetric A. Implemented the real Schur form — Hessenberg reduction + Francis double-shift QR (francisSchur, bulge-chase + deflation, U accumulation) + dlanv2 2×2 block standardization (real eig → triangular, complex pair → 2×2 block). schur_reg dispatches symmetric→Jacobi / general→Francis. Parity OK vs MATLAB R2025b on the invariants (A=U·T·U', U orthogonal, eigenvalues, triangular-for-real): [1 2;3 4] diag [-0.372 5.372], [1 2 3;4 5 6;7 8 10] eig [-0.906 0.198 16.708], 4×4 complex-pair recon ~1e-14. Unblocks care/dare + complex-eig (2026-06-18) |
| bug | [image/imresize-interp](image/imresize-interp.md) | P2 | ✅ FIXED: imresize bilinear/bicubic diverged from MATLAB (wrong corners/grid on upscale, no antialias on downscale). Rewrote the 2-D interp to MATLAB's separable algorithm: pixel-centre map u=o/scale+0.5(1-1/scale), mirror boundary (folded taps sum; clamp gave 0.789 vs MATLAB 0.71875), antialias kernel stretched by 1/scale on shrink; default method now bicubic (reg defaulted to bilinear). Parity OK vs MATLAB R2025b: bilinear x2, bicubic x2 (1,1)=0.71875, downscale [1..6]->[1 3]=[1.44922 3.5 5.55078]. nearest unchanged. Reused the (correct) imresize3 machinery's approach (2026-06-18) |
| stub | [stats/smoothdata-methods](stats/smoothdata-methods.md) | P2 | ✅ FIXED: smoothdata sgolay/lowess/loess methods threw. Implemented inline in stats (no cross-dep on signal): sgolay = degree-2 Savitzky-Golay (B=A(A'A)^-1A'); lowess/loess = tricube-weighted local linear/quadratic regression (F-nearest window, fit at query point). Explicit-window matches MATLAB R2025b exactly (sgolay w5/w7, lowess/loess w5/w7); loess interior-identity explained (3-pt quad interpolates). Auto default window approximate (MATLAB's is data-dependent — same caveat as the existing movmean default) (2026-06-18) |
| missing-fn | [signal/pmusic-peig](signal/pmusic-peig.md) | P2 | ✅ FIXED (peak-freq parity): pmusic (MUSIC) + peig pseudospectra were undefined. R=X'X (order 2p via corrmtx) -> Jacobi eig -> noise subspace (smallest p) -> P(w)=1/sum\|e'vk\|^2 (pmusic) / weighted 1/lambda_k (peig). Validated by PEAK FREQUENCIES (the estimator's purpose); absolute pseudospectrum NOT bit-matched (peaks are 1/near-zero, eigendecomposition-sensitive, scale-arbitrary). Parity OK vs MATLAB R2025b: 2-tone -> peaks at 0.6381, 1.5708 rad for both (2026-06-18) |
| missing-fn | [signal/stmcb](signal/stmcb.md) | P2 | ✅ FIXED: Steiglitz-McBride IIR identification stmcb(h,nb,na,niter) was undefined. Init A via prony, then niter (default 5) iterations: prefilter unit impulse + h by 1/A, solve the Toeplitz LS [E\|-G][b;a]~g (normal equations). Parity OK vs MATLAB R2025b: stmcb([1 .5 .25 .125 .0625],1,1)=a[1 -0.5]; 2nd-order B/A recovered exactly. Two-signal form stmcb(y,x,...) + explicit ai init rejected (deferred) (2026-06-18) |
| missing-fn | [stats/autocorr](stats/autocorr.md) | P2 | ✅ FIXED: autocorr + crosscorr + parcorr (Econometrics ACF/CCF/PACF) were undefined. autocorr/crosscorr = biased estimator normalised to a correlation (lag-0 ACF=1), default NumLags=min(20,N-1), ±2/√N bounds; crosscorr normalised by √(c1(0)c2(0)). parcorr matches MATLAB's DEFAULT OLS Method (AR(k) lag regression, PACF(k)=deepest coeff; can exceed 1), via normal equations — full precision on well-conditioned lags. Parity OK vs MATLAB R2025b. parcorr 'yule-walker' (Durbin-Levinson) spelling not wired (2026-06-18) |
| missing-fn | [stats/friedman](stats/friedman.md) | P2 | ✅ FIXED (reps=1): Friedman nonparametric two-way ANOVA by ranks was undefined. Ranks k treatments within each of n blocks (mid-ranks for ties) → tie-corrected Q = [12/(n·k·(k+1))·ΣRj² − 3n(k+1)]/C, C=1−Σ(t³−t)/(n(k³−k)), p=1−chi2cdf(Q,k−1). Returns [p, Q, df] (statistic+df, like kruskalwallis — not MATLAB's tbl/stats); primary p matches MATLAB exactly. Parity OK: p=0.0183156 (no ties), 0.8464817 (ties). reps>1 (replicated layout) rejected with a clear error — its ranking doesn't reduce to averaging-then-rank, deferred (2026-06-18) |
| missing-fn | [stats/distribution-dispatchers](stats/distribution-dispatchers.md) | P2 | ✅ FIXED: generic cdf/pdf/icdf/random(distName, params...) were undefined. Added register-level dispatchers — a name→family-prefix table (case-insensitive + aliases) forwarding to the existing per-family builtins (cdf→<fam>cdf, pdf→<fam>pdf, icdf→<fam>inv, random→<fam>rnd) via findExternal. Covers all 23 shipped families; unknown names throw. Parity OK vs MATLAB R2025b: cdf('Normal',1,0,1)=0.84134, icdf('Chisquare',0.95,3)=7.81473, pdf('Binomial',2,5,0.3)=0.3087. The per-family math was already parity-validated; this is name-dispatch glue (2026-06-18) |
| bug | [signal/periodogram-nonpow2-nfft](signal/periodogram-nonpow2-nfft.md) | P2 | ✅ FIXED: periodogram(x,win,nfft,fs) with an explicit non-power-of-two nfft returned a garbage spectrum (transform was fftRadix2, pow2-only — peak in the wrong bin, Parseval broken). Now a non-pow2 nfft routes the windowed/zero-padded signal through the general fft (Bluestein); the pow2 path is unchanged (zero risk for the default + existing callers). pwelch/cpsd/mscohere/tfestimate/computePsd inherit the fix. Parity OK vs MATLAB R2025b: periodogram(2-tone,[],1000,1000) → P(100)=0.5, P(200)=0.125, sum(P)*df=0.625=mean(x^2) (was peak ~256, sum ~21.5). Found while fixing obw (2026-06-18) |
| bug | [signal/obw-value-outputs](signal/obw-value-outputs.md) | P1 | ✅ FIXED: obw 99% bandwidth was ~8% wrong (108.77 vs MATLAB 100.97) + [bw,flo,fhi,power] outputs missing. Three compounded bugs: PSD used the default nfft=1024 (zero-pad) not nfft=N; trapezoid not the rectangle-rule cumulative; band edge at bin centre not the upper edge F+df/2. Window is rectangular (not Kaiser). numkit periodogram is radix-2 only (can't do nfft=N for non-pow2 N → garbage), so obw now computes the length-N DFT via the general fft (Bluestein) and forms the one-sided PSD inline. Returns (bw,flo,fhi,power) by nargout. Parity OK vs MATLAB R2025b: bw=100.96875, flo=99.50625, fhi=200.475, power=0.61875 (2026-06-18) |
| missing-output | [signal/periodogram-pxxc](signal/periodogram-pxxc.md) | P2 | ✅ FIXED: [pxx,f,pxxc]=periodogram(...,'ConfidenceLevel',p) threw ('ConfidenceLevel' unparsed + pxxc not computed). Now emits the chi-square CI as the 3rd output: pxxc = pxx.*v./chi2inv([1-a/2,a/2],v) with INTEGER v (=2 for interior bins, =1 for the real DC & even-nfft Nyquist bins; black-box-pinned from MATLAB outputs, no source read — it's a standard textbook method), closed forms chi2inv(q,2)=-2ln(1-q) & chi2inv(q,1)=(sqrt2*erfinv(q))^2 (erfinv from the math layer, no stats dep). Default level 0.95. New compute fn periodogramConf. Parity OK vs MATLAB R2025b: lower [32.25 7.40 2.17 1.27 0.40], DC/Nyquist ratio 0.19905, interior 0.27108. pwelch/cpsd CI still open (averaged EDOF) (2026-06-18) |
| perf | [ops/cheap-elementwise-simd-small-n](ops/cheap-elementwise-simd-small-n.md) | P3 | ✅ FIXED: Highway SIMD cheap element-wise (+ - .* ./) ran 0.1-0.3x of scalar at cache-resident N on native MSVC = HWY_DYNAMIC_DISPATCH indirect-call overhead vs MSVC autovec (WASM static dispatch had no crater -> MSVC-specific). Size-gated before dispatch (kSimdInlineThreshold, native 256K / WASM 0): plus 0.10→0.97x, times 0.12→1.54x. Fused affine/abs/sq same gate — but their parallel_for lambda captured [&], escaping out/x and defeating MSVC alias analysis so the gate loop wouldn't vectorize (vec-report 1104); switching to [=] fixed it (0.68→1.0x). big-N + WASM unchanged, suite bit-identical (2026-06-18) |
| bug | [signal/cceps-nd-phase](signal/cceps-nd-phase.md) | P1 | ✅ FIXED: non-2ⁿ complex-cepstrum phase was garbage past DC + the nd 2nd output was missing. Ported MATLAB rcunwrap (full unwrap + linear-phase removal: nd=round(unwrapped(nh+1)/pi), subtract pi*nd*(0:n-1)/nh) → nd is the 2nd output. Also fixed a latent time-reversal (the forward fft used dir=+1 like the inverse pass) in cceps + icceps. Parity OK vs MATLAB R2025b: [1 2 3 4 3 2 1] and (1:8) bit-identical, nd=1/−3 (2026-06-18) |
| bug | [image/regionprops-perimeter](image/regionprops-perimeter.md) | P1 | ✅ FIXED: requesting an unimplemented property (e.g. Perimeter) was silently dropped → confusing downstream field error. Now (a) unknown/unimplemented property names throw clearly, and (b) Perimeter is implemented: outer 8-conn Moore boundary trace + MATLAB's Vossepoel-Smeulders estimator 0.980·Ne+1.406·No−0.091·Nc. Matches MATLAB R2025b exactly (3x3=7.476, 4x4=11.396, plus=5.624, eye4=8.436). Harness N/A for regionprops (pre-existing); verified by direct MATLAB run (2026-06-18) |
| bug | [runtime/func2str-anonymous](runtime/func2str-anonymous.md) | P2 | ✅ FIXED: func2str(@(x)x+1) returned the internal '@__anon_0', not the source. Parser reconstructs the anon source from its token span (no inter-token whitespace, char/string literals re-quoted — matches MATLAB normalization) and stores it on the handle (HeapObject.funcSource, set by both engines); func2str returns it. str2func(func2str(h)) round-trips. Captured-var closures (@(x)x+a; VM packs as a {handle,caps} cell) also work — func2str unwraps the closure cell, so it works on both engines. Parity OK vs MATLAB R2025b (2026-06-18) |
| stub | [math/histcounts-autobinning](math/histcounts-autobinning.md) | P2 | ✅ FIXED: automatic binning was a stub (edges required). Ported MATLAB R2025b binpicker/binpickerbl/integerrule + autorule (integer bins for integer data range<=50, else Scott) + scott/fd/sturges/sqrt. histcounts_reg now takes histcounts(x), a scalar 2nd arg as bin COUNT (vs vector edges), and NumBins/BinWidth/BinLimits/BinMethod. histcounts([1 2 2 3 3 3])=[1 2 3]; histcounts(1:10,3)=[3 4 3]. Verified 16/16 fingerprints vs MATLAB directly (parity harness reports N/A for histcounts — pre-existing) (2026-06-18) |
| bug | [math/interpn-nan](math/interpn-nan.md) | P2 | ✅ FIXED: interpn 1-D grid-vector query interpn(X,V,Xq) returned NaN — the dispatch keyed off args[0], which in Form B is the grid vector X, so it misrouted to interp2. Now the 1-D case (no 2-D+ data arg) with >=3 leading data args delegates to interp1 → 6.5. Parity caught an overreach: interpn(V,scalar) is MATLAB's grid-REFINEMENT form (returns a refined grid, not a query), so only the grid-vector spelling is delegated; refinement + 4+-D stay parity gaps (2026-06-18) |
| stub | [math/maxmin-complex](math/maxmin-complex.md) | P2 | binary max(A,B)/min(A,B) + clamp accept complex - by modulus then angle(z), NaN-component omitted (omitnan default); no all-real fallback (max(complex(-3,0),1)=-3). Prior VM-dispatch-blocker note was a misdiagnosis: default omitnan routes to maxOmitNanBinary, not max (2026-06-17) |
| bug | [math/complex-zero-imag-narrowing](math/complex-zero-imag-narrowing.md) | P2 | MATLAB narrows an all-zero-imaginary complex RESULT back to real (isreal(2+0i)=1); complex() stays forced. narrowComplex (value layer) applied across arithmetic/unary/matmul/fused/indexing/reductions(sum,prod,mean,var,std,cumsum,cumprod,diff,median)/linalg(dot,kron,cross,diag)/reorder; reshape/transpose/sort/cat/unique correctly preserve. In-place indexed-assign deliberately NOT narrowed (eager scan is O(n^2) in fill loops, ~250x; narrows on next op). Both backends (2026-06-17) |
| bug | [image/adapthisteq-mapping](image/adapthisteq-mapping.md) | P2 | CLAHE was ~54% too bright; ported MATLAB clip (ceil/round + step-redistribute) + map (rayleigh vmax) + integer-weight region interpolation over even-tile padding → matches MATLAB to ±1 level (was a regression; tonemap re-matches too) (2026-06-14) |
| bug | [lang/int-cast-rtne](lang/int-cast-rtne.md) | P2 | int32/int64/uint32 SIMD cast rounded ties-to-even; now half-away-from-zero (MATLAB) via trunc(v+copysign(0.5,v)) (2026-06-14) |
| stub | [stats/jackknife](stats/jackknife.md) | P2 | jackknife(fn,X) now loops leave-one-out inline via callFunctionHandle (like bootstrp_reg) instead of a dead stub; vector reshaped to column observations (2026-06-14) |
| bug | [linalg/cross-integer-class](linalg/cross-integer-class.md) | P2 | cross preserves the integer class of integer operands with per-operation saturation (cross(int8([100 100 0]),int8([0 100 100]))=[127 -127 127], not -128); int+double→int; different-int/int+logical lenient→double (2026-06-05) |
| bug | [runtime/accumarray-integer-vals](runtime/accumarray-integer-vals.md) | P2 | accumarray accepts integer/logical vals (was: throw "vals must be DOUBLE") — sum/prod/mean→double, max/min preserve int class; logical+max/min & custom-handle class are lenient niches (2026-06-05) |
| bug | [signal/deconv-integer-input](signal/deconv-integer-input.md) | P2 | deconv accepts integer/logical input (was: throw "Not a double array") — promotes to double (reuses convPromoteToDouble); q+r always double. na>nb int is numkit-lenient (MATLAB errors there) (2026-06-05) |
| bug | [signal/conv-integer-input](signal/conv-integer-input.md) | P2 | conv accepts integer/logical input (was: throw "Not a double array") — promotes to double; result always double (every shape), never the int class (2026-06-05) |
| bug | [linalg/kron-integer-class](linalg/kron-integer-class.md) | P2 | kron preserves the integer class of integer operands (saturating) — same-int→that class, int+scalar-double→int (round+saturate), double*double unchanged (2026-06-05) |
| bug | [lang/concat-integer-types](lang/concat-integer-types.md) | P2 | CORE (user-approved): cat/[;]/[,]/vertcat/horzcat preserve integer class — first-int wins, others cast (round+saturate); concat in double then narrow (2026-06-05) |
| bug | [lang/str2double-complex](lang/str2double-complex.md) | P3 | str2double parses complex strings ('1+2i'/'2i'/'i'/'1+2j'/'1e-3+2i'); COMPLEX output when any element complex, real path unchanged (2026-06-05) |
| bug | [math/psi-zero-pole](math/psi-zero-pole.md) | P3 | psi(0) returns -Inf (digamma pole), was NaN; finite values + negative-domain unchanged, matches MATLAB (2026-06-05) |
| bug | [math/polyder-product](math/polyder-product.md) | P2 | polyder(a,b) single-output = derivative of the PRODUCT a*b (was the quotient numerator); 2-output quotient form unchanged (2026-06-05) |
| bug | [math/gamma-negative-integer-poles](math/gamma-negative-integer-poles.md) | P3 | gamma returns +Inf at non-positive integer poles (was NaN via std::tgamma); gamma(-Inf)=Inf, matches MATLAB (2026-06-05) |
| bug | [math/maxmin-char-double](math/maxmin-char-double.md) | P2 | max/min of a char array return double (the code point), not char — MATLAB does not preserve char for max/min (mode does); flipped the stale char-return test (2026-06-05) |
| bug | [lang/sprintf-complex](lang/sprintf-complex.md) | P2 | sprintf/fprintf use the real part of a complex argument for numeric conversions (was: throw); imaginary discarded, as MATLAB (2026-06-05) |
| bug | [stats/movfun-order-stats](stats/movfun-order-stats.md) | P3 | movmax/movmin/movmedian accept integer/logical — movmax/movmin preserve class, movmedian rounds int half-away & logical→double (completes mov* sweep) (2026-06-05) |
| bug | [stats/movfun-typeclass](stats/movfun-typeclass.md) | P3 | movsum/movprod/movmean accept integer/logical — promote to double (char still errors, as MATLAB) (2026-06-05) |
| bug | [lang/cummax-cummin-integer](lang/cummax-cummin-integer.md) | P3 | cummax/cummin accept integer — preserve int class (promote→cummax/cummin→doubleToIntegerExact; exact, order stats) (2026-06-05) |
| bug | [math/setops-typeclass](math/setops-typeclass.md) | P2 | ismember/intersect/setdiff/union accept char/logical/integer — values preserve class, ia/ib & ismember loc stay double (2026-06-05) |
| bug | [math/unique-typeclass](math/unique-typeclass.md) | P2 | unique accepts char/logical/integer — preserves class on values, ia/ic stay double (promote→unique→narrowUniqueClass) (2026-06-05) |
| bug | [lang/sort-char](lang/sort-char.md) | P2 | sort accepts char — sorts by code point, preserves char class, index double (charizeSortResult narrow; shape-preserving, unlike toChar) (2026-06-05) |
| bug | [lang/sort-logical](lang/sort-logical.md) | P2 | sort accepts logical — values preserve logical class, index stays double (mirrors integer path) (2026-06-05) |
| bug | [math/trapz-logical](math/trapz-logical.md) | P2 | trapz accepts logical X/Y — promote→double at trapz_reg entry (class not preserved; matches cumtrapz) (2026-06-05) |
| bug | [lang/cumulative-logical](lang/cumulative-logical.md) | P2 | cumsum/cumprod/cummax/cummin accept logical — cumsum/cumprod→double, cummax/cummin→logical (class preserved) (2026-06-05) |
| bug | [signal/impinvar-repeated-poles](signal/impinvar-repeated-poles.md) | P1 | repeated-pole impinvar numerator — multiplicity partial fractions + Eulerian impulse-invariant z-kernel + centroid/Newton pole refine (clean-room) (2026-06-05) |
| stub | [signal/ellipord-bandstop](signal/ellipord-bandstop.md) | P2 | ellipord bandstop order/Wn — reciprocal bandpass→LP map WA=(WS·(WP1-WP2))/(WS²-WP1·WP2) (clean-room) (2026-06-05) |
| bug | [stats/distribution-array-params](stats/distribution-array-params.md) | P2 | *pdf/*cdf/*inv broadcast ARRAY params across all 16 distribution families — continuous + discrete (c29-38) |
| missing-output | [stats/corr-pvalue](stats/corr-pvalue.md) | P2 | [r,p]=corr 2nd output: Pearson p=2·tcdf(-\|t\|,n-2); Kendall/Spearman EXACT permutation p (small n); matrix diag=1 (2026-06-05) |
| missing-output | [stats/mle-output](stats/mle-output.md) | P2 | [phat,pci]=mle(...) confidence intervals (normal/exp/poisson/lognormal, Alpha) via *fit CIs (2026-06-05) |
| missing-output | [linalg/eig-left-vectors](linalg/eig-left-vectors.md) | P2 | [V,D,W]=eig(A) left eigenvectors W (W'*A=D*W', unit-norm, eig(A') reordered) (2026-06-05) |
| missing-output | [linalg/qr-pivoting](linalg/qr-pivoting.md) | P2 | column-pivoting [Q,R,P]=qr(A) — A*P=Q*R, decreasing-norm order, 'vector'/econ P (2026-06-05) |
| stub | [stats/isoutlier-gesd](stats/isoutlier-gesd.md) | P2 | isoutlier 'gesd' (Rosner generalized ESD) + MaxNumOutliers/ThresholdFactor (2026-06-05) |
| bug | [stats/kstest-pvalue](stats/kstest-pvalue.md) | P1 | kstest/kstest2 exact KS p-value (Marsaglia + Birnbaum-Tingey) + cv (2026-06-05) |
| bug | [stats/dwtest-pvalue](stats/dwtest-pvalue.md) | P2 | exact Durbin-Watson p-value via Imhof CF inversion + Tail option (2026-06-05) |
| bug | [runtime/cellfun-inputforms](runtime/cellfun-inputforms.md) | P2 | cellfun multi-cell zip + legacy string-name forms ('isempty'/'size'/'isclass'/…) (2026-06-05) |
| bug | [lang/diff-zero-order](lang/diff-zero-order.md) | P3 | diff order N must be a positive integer scalar — 0/neg/frac/non-scalar now error (was identity at 0) (2026-06-05) |
| bug | [math/gradient-3d](math/gradient-3d.md) | P2 | gradient supports N-D (3-D+) arrays — one gradient per dim up to nargout (2026-06-05) |
| bug | [stats/pdist-metrics](stats/pdist-metrics.md) | P2 | pdist/pdist2 gain 'seuclidean' + 'spearman'; cosine/correlation → NaN (not 1) on zero-norm/const row (2026-06-05) |
| bug | [lang/find-count-direction](lang/find-count-direction.md) | P1 | find(X,k[,'first'/'last']) now honours count + direction (single + multi-output) (2026-06-05) |
| bug | [math/complex-input-unsupported](math/complex-input-unsupported.md) | P2 | complex now accepted by trapz/cumtrapz/median/interp1/gradient/movmean/detrend/conv/filter (umbrella closed) (2026-06-05) |
| bug | [math/log-complex-promotion-arrays](math/log-complex-promotion-arrays.md) | P2 | log/log10/log2/log1p promote whole real arrays to complex out of domain (log1p: x<-1) (2026-06-05) |
| bug | [linalg/norm-complex](linalg/norm-complex.md) | P2 | norm() of a complex array by \|z\| (vector 1/2/Inf/p + matrix 1/Inf/'fro'; spectral deferred) (2026-06-05) |
| bug | [lang/diff-complex](lang/diff-complex.md) | P1 | diff() now differences real + imaginary parts (n-th order + dim) (2026-06-05) |
| bug | [lang/cumsum-complex](lang/cumsum-complex.md) | P2 | cumsum/cumprod accumulate complex element-wise (dim + reverse) (2026-06-05) |
| bug | [math/acos-asin-complex](math/acos-asin-complex.md) | P2 | acos/asin go complex for \|x\|>1 (via acosh for the correct branch; array promotes) (2026-06-05) |
| bug | [math/complex-promotion-arrays](math/complex-promotion-arrays.md) | P2 | sqrt/acosh/atanh promote whole real arrays to complex (+ atanh x<-1 branch sign) (2026-06-05) |
| bug | [lang/sort-missingplacement](lang/sort-missingplacement.md) | P1 | 'MissingPlacement' option was ignored |
| bug | [signal/rceps-cceps-padding](signal/rceps-cceps-padding.md) | P1 | cepstrum garbage on non-2ⁿ + rceps 2nd output (9fcf6872) |
| bug | [signal/besself-digital](signal/besself-digital.md) | P1 | ran digital path → binomial garbage |
| bug | [math/max-all-linear](math/max-all-linear.md) | P1 | max/min(A,[],'all') was entirely broken |
| bug | [stats/combnk-scalar](stats/combnk-scalar.md) | P3 | scalar v is the 1-element set {v}; K>N → empty 0×K (c179) |
| bug | [stats/anova1-matrix-input](stats/anova1-matrix-input.md) | P2 | matrix columns-as-groups input form (c179) |
| bug | [math/unique-last](math/unique-last.md) | P1 | 'last' selects last occurrence (sorted; stable+last sub-gap deferred) (c180) |
| stub | [signal/dct-types](signal/dct-types.md) | P2 | dct/idct Type 1/3/4 implemented (c181) |
| missing-output (+bug) | [signal/risetime-falltime-outputs](signal/risetime-falltime-outputs.md) | P1 | [R,LT,UT,LL,UL] outputs + sharp-edge value fix 0.224→0.198 (c182) |
| missing-output | [signal/spectrogram-ps](signal/spectrogram-ps.md) | P2 | missing 4th output PSD (1128db65) |
| bug | [io/writelines](io/writelines.md) | P2 | writelines string-array writes one line per element (was: only first) (2026-06-08) |

### 🔴 OPEN — bug (defect on an implemented function) — 5

| Bug | Sev | Notes |
|---|---|---|
| [linalg/complex-matrix-unsupported](linalg/complex-matrix-unsupported.md) | P2 | entire linalg suite (eig/svd/qr/lu/chol/det/inv/trace/…) rejects complex matrices |
| [signal/instfreq-instbw](signal/instfreq-instbw.md) | P1 | wrong values (negative on a chirp) |
| [signal/freqs-scalar-w](signal/freqs-scalar-w.md) | P3 | scalar w should be N points (needs freqint auto-range) |
| [stats/mahal-singular](stats/mahal-singular.md) | P2 | throws on rank-deficient reference |
| [lang/cell-csl-expansion](lang/cell-csl-expansion.md) | P2 | `c{:}` / `c{vec}` comma-separated-list expansion errors "Cell index out of bounds" (interpreter has no CSL machinery; both backends). Blocks codegen CSL. Found via the codegen CSL audit (2026-06-26) |

### 🔴 OPEN — stub (option/branch throws "not supported") — 1

| Bug | Sev | Notes |
|---|---|---|
| [signal/findpeaks-widthreference](signal/findpeaks-widthreference.md) | P2 | 'halfheight'/'halfprom' throw |

### 🔴 OPEN — missing-output (Nth output not emitted) — 1

| Bug | Sev | Notes |
|---|---|---|
| [signal/spectrogram-fc-tc](signal/spectrogram-fc-tc.md) | P2 | 5th/6th outputs fc, tc (reassignment matrices, deferred) |

### 🔴 OPEN — missing-fn (not implemented — PARITY GAP, not a defect) — 12

*(Curated/notable subset — the full 839-missing + 25-partial inventory is in
[PARITY_GAPS.md](PARITY_GAPS.md).)*

| Bug | Sev | Notes |
|---|---|---|
| [signal/fillgaps](signal/fillgaps.md) | P2 | fillgaps |
| [image/watershed](image/watershed.md) | P2 | watershed |
| [image/imfindcircles](image/imfindcircles.md) | P2 | imfindcircles |
| [wavelet/wpdec](wavelet/wpdec.md) | P2 | wavelet packets (needs tree type) |
| [wavelet/cwt](wavelet/cwt.md) | P2 | continuous wavelet transform (Morse filter bank) — large |
| [wavelet/centfrq-scal2frq](wavelet/centfrq-scal2frq.md) | P2 | centfrq / scal2frq (scale↔frequency) |
| [ode/ode-stiff](ode/ode-stiff.md) | P2 | ode15s/ode23s/ode23t/ode23tb/ode113 (stiff/multistep) |
| [linalg/qz-gsvd](linalg/qz-gsvd.md) | P2 | qz (generalized Schur) / gsvd (generalized SVD) |

### 🔴 OPEN — perf (correct but slower than MATLAB) — 1

| Entry | Slowdown | Notes |
|---|---|---|
| [signal/fft-speed](signal/fft-speed.md) | 1.2×–4.3× | single-threaded vs FFTW; Highway already present, gap is threading + MSVC codegen + wrapper |
