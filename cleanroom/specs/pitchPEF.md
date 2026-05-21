# Clean-room specification — `pitchPEF`

Pitch-Estimation-Filter ("PEF") fundamental-frequency estimation, per
analysis frame. Written per `cleanroom/PROTOCOL.md` (Spec Author role).

> **Scope decision.** This implements the published PEFAC method
> *without the amplitude-compression stage* — the paper itself calls
> that variant "PEF" (Fig. 6: "the algorithm without the amplitude
> compression stage"), which is the method MATLAB exposes as
> `Method="PEF"`. It is a faithful implementation of the *paper's*
> filter (Eq. 4). MATLAB's `pitch(...,'PEF')` is expected to use a
> different comb-filter formula; numkit's PEF is therefore NOT
> guaranteed to bit-match MATLAB. If parity (5 % tol) does not hold,
> the PEF fingerprints / gtest are re-baselined, exactly as was done
> for SRH.

## Public reference

S. Gonzalez and M. Brookes, "A Pitch Estimation Filter robust to high
levels of noise (PEFAC)", *Proc. EUSIPCO 2011*, pp. 451–455.

## 1. Algorithm

In the log-frequency domain (`q = ln f`) the harmonics of a signal of
fundamental `f0` sit at `q = ln f0 + ln k`, k = 1..K — a spacing
independent of `f0`. Convolving the log-frequency power spectrum with a
comb filter `h(q)` whose teeth sit at `q = ln k` therefore sums the
harmonic energy and peaks at `q = ln f0`.

**Filter** (paper Eq. 4). On a natural-log axis, for
`ln(0.5) < q < ln(K + 0.5)`:

```
g(q) = ln( γ − cos(2π · e^q) )
h(q) = β − g(q)
```

with `h(q) = 0` outside that interval. `β` is set so the filter has
zero mean (`Σ h = 0`, i.e. `β = mean of g over the support`); this
makes `h` reject white and smoothly-varying noise. `γ` controls the
tooth width. Parameters: `γ = 1.5`, `K = 10` (paper §2.1, §3).

**Per analysis frame:**

1. Apply a periodic Hamming window to the frame.
2. Zero-pad to `NFFT` and take the DFT; power spectrum
   `P[k] = |Y[k]|²` for k = 0 .. NFFT/2, at linear frequency
   `f_k = k · fs / NFFT`. Zero-pad generously for low-frequency
   resolution — `NFFT = nextPow2(4 · winLen)` (the paper zero-pads a
   90 ms window to 360 ms).
3. For a candidate fundamental `f0`, the convolution
   `(Y * h)(ln f0)` is evaluated directly as a correlation:

   ```
   score(f0) = Σ_j  h_j · P_interp( f0 · e^{q'_j} )
   ```

   where `q'_j = j · Δ` are the filter-support samples (see §2),
   `h_j = β − g(q'_j)`, and `P_interp(f)` is `P` linearly
   interpolated at linear frequency `f` (0 outside `[0, fs/2]`).
   `f0 · e^{q'_j}` is the linear frequency of the j-th filter tap for
   that candidate.
4. `f0(frame)` = the candidate maximising `score`, searched over
   `q0 = ln f0 ∈ [ln(minF), ln(maxF)]` on the same `Δ` log-grid.
5. Clip to `[minF, maxF]`.

**Indexing:** all 0-based. Frequencies/log-frequencies are physical
quantities (Hz / nat-log-Hz); no MATLAB index convention applies.

## 2. Grid and filter construction

- Log-grid step `Δ = ln(1.0058)` — the paper's 0.58 % frequency
  resolution.
- Filter-support sample indices: `j` from `ceil(ln(0.5)/Δ)` to
  `floor(ln(K + 0.5)/Δ)`; `q'_j = j·Δ`.
- `g_j = ln(γ − cos(2π · e^{q'_j}))`; `β = (Σ g_j) / count`;
  `h_j = β − g_j`.
- Candidate log-frequencies: `q0 = m·Δ` for every integer `m` with
  `q0 ∈ [ln(minF), ln(maxF)]`; `f0 = e^{q0}`.

## 3. Interface (numkit)

```cpp
Value pitchPEF(const Value &x, double fs, double minF, double maxF,
               std::pmr::memory_resource *mr);
```

- `x` — real audio `Value` vector; sample `i` via `x.elemAsDouble(i)`.
- `fs`, `minF`, `maxF`, `mr` as for the other pitch methods.
  **PMR HARD RULE:** scratch via `ScratchArena` / `ScratchVec<T>`.
- Returns `Value::matrix(numFrames, 1, ValueType::DOUBLE, mr)`;
  `numFrames == 0` → `Value::matrix(0, 0, …)`.

File-local helpers — **call, do not reimplement**:
- `void hammingPeriodic(double *w, size_t N)` — periodic Hamming.
- `struct FrameSpec { size_t winLen, overlap, hop, numFrames; };`
  `FrameSpec frameSpec(size_t N, double fs, double winSec, double ovSec);`
- `size_t nextPow2(size_t x);`

numkit signal API:
- `Value signal::fft(const Value &m, int nfft, int dim, std::pmr::memory_resource *mr);`
  forward result COMPLEX, via `Value::complexData()`. For a column
  `Value` use `dim = 0` (first non-singleton) or build an
  `NFFT × numFrames` matrix and `dim = 1`.

For a writable double buffer use `Value::doubleDataMut()`.

## 4. Compatibility parameters

- Framing: `frameSpec(N, fs, 0.052, 0.042)` (numkit's default pitch
  framing — one f0 per frame, matching the other methods).
- `minF` / `maxF` from the caller (MATLAB default `[50, 400] Hz`).

This is a paper-faithful PEF; no MATLAB-internal pipeline detail is
reproduced. Expect the output to differ from MATLAB's `pitch(…,'PEF')`.

## 5. Verification

- gtest: `PitchHarmonicsTest` in `libs/audio/tests/pitch_harmonics_test.cpp`.
  The PEF expectations are re-baselined to this implementation if they
  differ from MATLAB.
- Parity: `tools/parity/specs/audio_pitch_harmonics.json` — `pef_first`,
  `pef_mean`, `pef_r_first`. If numkit's paper-PEF falls outside the
  5 % tolerance vs MATLAB, those fingerprints are re-baselined (dropped
  from the cross-engine comparison with a spec-comment note), exactly
  as was done for SRH.

## Constraints for the Implementer

- Do **not** open `libs/audio/src/features/pitch_harmonics.cpp`.
- Do **not** consult MATLAB `.m` source or any third-party reference
  implementation.
- Implement solely from this specification and the cited public paper.
