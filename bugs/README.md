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

**Tally (110 entries):** ✅ 80 fixed · 🔴 30 open = **5 bug** + 2 stub +
1 missing-output + **21 missing-fn** + 1 perf (the 21 missing-fns are parity
feature-gaps, not defects — also in PROGRESS.md; perf = correct-but-slow).

> **Full parity-gap inventory:** the 30 missing-fn rows below are the *curated /
> notable* gaps. The complete list of **839 missing + 25 partial** MATLAB
> functions (migrated out of PROGRESS.md, grouped by namespace) lives in
> [PARITY_GAPS.md](PARITY_GAPS.md). Those are parity gaps, **not defects** —
> they are NOT counted in the tally above.

### ✅ FIXED (75)

| Kind | Bug | Sev | Notes |
|---|---|---|---|
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
| [signal/resample-values](signal/resample-values.md) | P1 | wrong output values (multirate) |
| [signal/freqs-scalar-w](signal/freqs-scalar-w.md) | P3 | scalar w should be N points (needs freqint auto-range) |
| [stats/mahal-singular](stats/mahal-singular.md) | P2 | throws on rank-deficient reference |

### 🔴 OPEN — stub (option/branch throws "not supported") — 2

| Bug | Sev | Notes |
|---|---|---|
| [signal/findpeaks-widthreference](signal/findpeaks-widthreference.md) | P2 | 'halfheight'/'halfprom' throw |
| [wavelet/dwt-biorthogonal](wavelet/dwt-biorthogonal.md) | P2 | bior*/rbio* families throw |

### 🔴 OPEN — missing-output (Nth output not emitted) — 1

| Bug | Sev | Notes |
|---|---|---|
| [signal/spectrogram-fc-tc](signal/spectrogram-fc-tc.md) | P2 | 5th/6th outputs fc, tc (reassignment matrices, deferred) |

### 🔴 OPEN — missing-fn (not implemented — PARITY GAP, not a defect) — 25

*(Curated/notable subset — the full 839-missing + 25-partial inventory is in
[PARITY_GAPS.md](PARITY_GAPS.md).)*

| Bug | Sev | Notes |
|---|---|---|
| [signal/fillgaps](signal/fillgaps.md) | P2 | fillgaps |
| [image/watershed](image/watershed.md) | P2 | watershed |
| [image/imfindcircles](image/imfindcircles.md) | P2 | imfindcircles |
| [image/corner](image/corner.md) | P2 | corner-point detection (cornermetric exists) |
| [wavelet/wpdec](wavelet/wpdec.md) | P2 | wavelet packets (needs tree type) |
| [wavelet/wentropy-ddencmp](wavelet/wentropy-ddencmp.md) | P2 | wentropy / ddencmp |
| [wavelet/wenergy-upcoef](wavelet/wenergy-upcoef.md) | P2 | wenergy (energy %) / upcoef (coeff reconstruction) |
| [wavelet/cwt](wavelet/cwt.md) | P2 | continuous wavelet transform (Morse filter bank) — large |
| [wavelet/wavedec2-family](wavelet/wavedec2-family.md) | P2 | wavedec2/detcoef2/appcoef2 (2-D multilevel) |
| [wavelet/centfrq-scal2frq](wavelet/centfrq-scal2frq.md) | P2 | centfrq / scal2frq (scale↔frequency) |
| [control/allmargin](control/allmargin.md) | P2 | all gain/phase/delay margins struct |
| [control/covar](control/covar.md) | P2 | output covariance from white noise |
| [comm/analog-demodulators](comm/analog-demodulators.md) | P2 | am/fm/pm/ssb/msk demod |
| [comm/syndtable](comm/syndtable.md) | P2 | syndrome decoding table (coset leaders) |
| [math/numerical-integration-nd](math/numerical-integration-nd.md) | P2 | quadgk/integral2/integral3/quad2d |
| [ode/ode-stiff](ode/ode-stiff.md) | P2 | ode15s/ode23s/ode23t/ode23tb/ode113 (stiff/multistep) |
| [linalg/funm](linalg/funm.md) | P2 | general matrix function funm(A,fun) |
| [linalg/qz-gsvd](linalg/qz-gsvd.md) | P2 | qz (generalized Schur) / gsvd (generalized SVD) |
| [optim/fsolve](optim/fsolve.md) | P2 | nonlinear system solver fsolve |
| [optim/nonlinear-lsq](optim/nonlinear-lsq.md) | P2 | lsqcurvefit/lsqnonlin |
| [optim/constrained-solvers](optim/constrained-solvers.md) | P2 | fmincon/linprog/quadprog/fminunc |

### 🔴 OPEN — perf (correct but slower than MATLAB) — 1

| Entry | Slowdown | Notes |
|---|---|---|
| [signal/fft-speed](signal/fft-speed.md) | 1.2×–4.3× | single-threaded vs FFTW; Highway already present, gap is threading + MSVC codegen + wrapper |
