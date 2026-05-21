# Clean-room specification — `dpcmopt`

Optimise differential pulse-code-modulation (DPCM) parameters — the
linear predictor and, optionally, the quantiser — for a training
signal (Communications toolbox). Written per `cleanroom/PROTOCOL.md`
(Spec Author role).

> **Scope.** Functional equivalence to MATLAB R2025b `dpcmopt`. The
> predictor is the autocorrelation-method linear predictor (Yule-Walker
> normal equations solved by the Levinson-Durbin recursion); the
> quantiser is the Lloyd-Max design on the prediction residual. The
> design is fully determined by this spec — including the one
> MATLAB-specific numerical convention (the per-lag autocorrelation
> denominator) — so the rewrite is expected to be bit-compatible with
> MATLAB.

## Public references

- N. S. Jayant & P. Noll, *Digital Coding of Waveforms: Principles and
  Applications to Speech and Video*, Prentice-Hall, 1984 — DPCM and
  predictive coding; design of the optimal linear predictor from the
  signal statistics; quantisation of the prediction residual.
- J. Makhoul, "Linear Prediction: A Tutorial Review", *Proc. IEEE*
  63(4):561–580, 1975 — the autocorrelation method of linear
  prediction and the Levinson-Durbin recursion that solves the
  Yule-Walker normal equations.
- J. G. Proakis & D. G. Manolakis, *Digital Signal Processing*,
  4th ed., Pearson, 2007 — Levinson-Durbin algorithm.
- S. P. Lloyd, "Least Squares Quantization in PCM", *IEEE Trans.
  Information Theory* 28(2):129–137, 1982 — the optimal (Lloyd-Max)
  scalar quantiser (used here via numkit's own `lloyds`).

## 1. Background

A DPCM coder predicts each sample from previous samples and quantises
only the prediction *residual*. `dpcmopt` chooses, from a training
signal:

1. the **predictor** — an FIR linear predictor of order `ord`, designed
   by the autocorrelation method so that it minimises the mean-square
   prediction error on the training signal;
2. the **quantiser** (optional) — an optimal Lloyd-Max scalar
   quantiser for the resulting residual.

## 2. Interface

```
predictor                        = dpcmopt(training_set, ord)
[predictor, codebook, partition] = dpcmopt(training_set, ord, ini_cb)
[predictor, codebook, partition] = dpcmopt(training_set, ord, len)
```

- **`training_set`** — a real vector of training samples, length `N`.
- **`ord`** — predictor order, a positive integer. `N` must be at
  least `ord + 3`; otherwise raise an error ("The size of the training
  set is not large enough …", id `m:dpcmopt:InvalidInput`). `ord < 1`
  → error id `m:dpcmopt:InvalidOrd`.
- **third argument** (optional) — either an integer `len` (number of
  codebook levels) or a vector `ini_cb` (an initial codebook). It is
  passed straight through to numkit's `lloyds`, which already accepts
  both forms; `dpcmopt` need not distinguish them.

Outputs:
- **`predictor`** — a length-`(ord+1)` **row vector**
  `[0, p1, …, p_ord]`.
- **`codebook`**, **`partition`** — produced only when the third
  argument is supplied (else empty).

## 3. Algorithm

### 3.1 Autocorrelation estimate

For lags `k = 0 … ord`, estimate the autocorrelation of the (length-`N`,
zero-based) training signal `x`:

```
r[k] = ( sum_{n=0}^{N-1-k} x[n] * x[n+k] ) / (N - 1 - k)
```

The lag-`k` sum has `N-k` terms; the denominator is `N-1-k` (one less
than the term count — equivalently `(N-1) - k`). This is the unbiased
autocorrelation estimator with a sample-variance-style `N-1`
correction, and is the convention MATLAB's `dpcmopt` uses; reproduce
it exactly for numerical agreement.

### 3.2 Levinson-Durbin recursion

Solve the order-`ord` Yule-Walker normal equations for the AR
coefficients `a[0 … ord]` by the standard Levinson-Durbin recursion.
`a` is initialised to `[1, 0, …, 0]` and `D ← r[0]`; then for
`m = 0 … ord-1`:

```
beta = sum_{j=0}^{m}  a[j] * r[m+1-j]
K    = -beta / D
a[1 … m+1] += K * reverse( a[0 … m] )       # update in place
D    = (1 - K*K) * D
```

After the recursion, `a = [1, a1, …, a_ord]` is the prediction-error
filter `A(z)`.

### 3.3 Predictor

The DPCM predictor is the negated AR tail with a leading zero:

```
predictor = [ 0, -a1, -a2, …, -a_ord ]      # length ord+1
```

(The leading 0 occupies the "current sample" slot; predictor[k] is the
weight on `x[n-k]`.)

### 3.4 Quantiser (optional)

When the third argument is supplied, design the residual quantiser:

1. **Prediction residual.** For `i = ord … N-1`:
   ```
   e[i-ord] = x[i] - sum_{k=1}^{ord} predictor[k] * x[i-k]
   ```
   giving a residual vector of length `N - ord`.
2. **Lloyd-Max quantiser.** Run numkit's `lloyds(residual, arg3, tol)`
   with `tol = 1e-7`. `lloyds` returns `(partition, codebook, distor,
   rel)`; keep `partition` and `codebook`.

## 4. numkit interface

Unchanged from the existing header
`numkit/comm/source/dpcmopt.hpp`:

```cpp
struct DpcmOptResult { Value predictor, codebook, partition; };

DpcmOptResult dpcmopt(const Value &training_set, int ord,
                      const Value &ini_codebook = Value::Empty,
                      std::pmr::memory_resource *mr = nullptr);
```

- Keep error ids `m:dpcmopt:InvalidOrd`, `m:dpcmopt:InvalidInput`
  (and `m:dpcmopt:nargin`, `m:dpcmopt:NeedIniCodebook` in the
  registration layer).
- `codebook` / `partition` stay empty (`Value::Empty`) when
  `ini_codebook.isEmpty()`.
- PMR HARD RULE: scratch via `ScratchArena` / `ScratchVec<T>`; the
  returned `Value`s are allocated on `mr`.
- Call numkit's own `numkit::comm::lloyds` — first-party code — for the
  quantiser step.

## 5. Verification

- gtest `libs/comm/tests/dpcmopt_test.cpp` — predictor-only and
  predictor+codebook cases; re-baseline hardcoded values only where
  the clean-room result genuinely differs.
- Parity `tools/parity/specs/dpcmopt.json` — require `correctness = OK`
  vs MATLAB R2025b. The predictor and codebook/partition are expected
  to stay bit-compatible; if a corner falls outside `tol`, re-baseline
  it and record the reason in the spec comment.
- **MATLAB-independent correctness test** (mandatory): generate a
  training signal from a *known* AR process, e.g.
  `x[n] = a1·x[n-1] + a2·x[n-2] + noise`; `dpcmopt(x, 2)` must recover
  a predictor close to `[0, a1, a2]` (the predictor of an AR(p) signal
  is its AR coefficients). Also verify that the prediction residual
  has a substantially smaller variance than the training signal
  (the predictor actually reduces the signal energy).

## Constraints for the Implementer

- Do **not** open `libs/comm/src/source/dpcmopt.cpp`.
- Do **not** consult MATLAB `.m` source (`dpcmopt.m`) or any
  third-party reference implementation.
- Implement solely from this specification and the cited public
  references. You MAY call numkit's own `numkit::comm::lloyds`.
