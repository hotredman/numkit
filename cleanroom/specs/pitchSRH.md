# Clean-room specification — `pitchSRH`

Summation-of-Residual-Harmonics (SRH) fundamental-frequency estimation,
per analysis frame. Written per `cleanroom/PROTOCOL.md` (Spec Author
role).

> **Scope decision.** This is a faithful implementation of the
> *published* SRH method. It does NOT attempt to bit-replicate
> MATLAB's `pitch(...,'SRH')`, whose internal pipeline diverges from
> the paper in undocumented ways. numkit's SRH output is therefore
> expected to differ from MATLAB R2025b; the SRH parity fingerprint
> and gtest are re-baselined to this implementation.

## Public reference

T. Drugman and A. Alwan, "Joint Robust Voicing Detection and Pitch
Estimation Based on Residual Harmonics", *Interspeech 2011*,
pp. 1973–1976.

## 1. Algorithm

SRH analyses the **LPC residual** of the signal: inverse filtering
whitens the spectral envelope, leaving the harmonic structure of the
voiced excitation. Summing the residual amplitude spectrum over the
harmonics of a candidate fundamental yields a score that peaks at f0.

Per analysis frame:

1. Apply a periodic Hann window to the frame:
   `w[n] = 0.5·(1 − cos(2π·n / N))`, n = 0 .. N−1.
2. **LPC residual.** Fit an order-`P` linear-prediction model to the
   windowed frame and inverse-filter it to obtain the residual frame
   `r[n] = filter(a, 1, windowedFrame)`, where `a` is the LPC
   polynomial `[1, a₁, …, a_P]`. (Paper §2: residual obtained by LPC
   inverse filtering; §3.2: order `P = 12`, non-critical in 10–18.)
3. **Amplitude spectrum.** `E[k] = |DFT_NFFT(r)|`, with
   `NFFT = round(fs)` so each bin spans 1 Hz and `E` is indexed
   directly in Hz.
4. **SRH criterion** (paper Eq. 1) — for an integer candidate
   fundamental `f` (Hz):
   `SRH(f) = E[f] + Σ_{k=2..Nharm} ( E[k·f] − E[round((k−½)·f)] )`
   with `Nharm = 5`. The `E[k·f]` terms sum the harmonics; the
   `E[(k−½)·f]` subtraction suppresses spurious maxima at even
   harmonics / at f0/2 (paper §2).
5. `f0(frame)` = the `f` maximising `SRH(f)` over the candidate range.

**Two-step range refinement** (paper §2):

- Pass 1 — candidate range `[ceil(minF), floor(maxF)]`; compute
  `f0` for every frame; `F0mean` = mean of all pass-1 estimates.
- Pass 2 — repeat steps 4–5 for every frame with the candidate range
  narrowed to
  `[ max(ceil(minF), round(0.5·F0mean)), min(floor(maxF), round(2·F0mean)) ]`.
  (If `F0mean` is not finite/positive, skip pass 2 and keep pass-1.)

Finally clip each `f0` to `[minF, maxF]`.

**Indexing:** all arrays 0-based. A candidate fundamental of `f` Hz
reads `E` at 0-based index `f` (1-Hz bins). `SRH(f)` is evaluated for
integer `f`; the reported `f0` is that integer `f` directly (Hz).

## 2. Interface (numkit)

```cpp
Value pitchSRH(const Value &x, double fs, double minF, double maxF,
               std::pmr::memory_resource *mr);
```

- `x` — real audio `Value` vector; sample `i` via `x.elemAsDouble(i)`.
- `fs`, `minF`, `maxF` as for the other pitch methods; `mr` PMR
  resource. **PMR HARD RULE:** scratch via `ScratchArena` /
  `ScratchVec<T>`; the returned Value uses `mr`.
- Returns `Value::matrix(numFrames, 1, ValueType::DOUBLE, mr)` — one
  `f0` per frame; `numFrames == 0` → `Value::matrix(0, 0, …)`.

Framing — use the standard pitch framing so the frame count matches
the other methods:
- `FrameSpec fr = frameSpec(N, fs, 0.052, 0.042);`
  → `winLen`, `hop`, `numFrames`.

File-local helper (already defined — call, don't reimplement):
- `struct FrameSpec { size_t winLen, overlap, hop, numFrames; };`
  `FrameSpec frameSpec(size_t N, double fs, double winSec, double ovSec);`

numkit signal API (read the headers for exact signatures):
- `numkit/signal/spectral_analysis/signal_modeling.hpp` —
  `std::tuple<Value,double> signal::lpc(const Value &x, int order, std::pmr::memory_resource *mr);`
  returns the LPC polynomial `a` (length `order+1`, leading 1) and the
  prediction gain.
- `numkit/signal/digital_filtering/filter.hpp` —
  `Value signal::filter(const Value &b, const Value &a, const Value &x, std::pmr::memory_resource *mr);`
  For inverse filtering use `b = a_lpc`, `a = Value::scalar(1.0, mr)`.
- `numkit/signal/transforms/fft.hpp` —
  `Value signal::fft(const Value &m, int nfft, int dim, std::pmr::memory_resource *mr);`
  forward result COMPLEX, via `Value::complexData()`.

The periodic Hann window is a trivial closed-form (formula in §1.1);
compute it inline.

## 3. Method parameters

- LPC order `P = 12` (paper §3.2).
- Harmonic count `Nharm = 5` (paper §3.2).
- DFT length `NFFT = round(fs)` (1-Hz bins).
- `minF` / `maxF` from the caller (MATLAB default range `[50, 400] Hz`).
- Highest bin touched: `Nharm · floor(maxF)`. If that exceeds `NFFT`,
  the range cannot be served — return the empty/zero result.

This implementation is paper-faithful; no MATLAB-internal pipeline
detail (residual re-framing, overlap-add, alternative windows) is
reproduced.

## 4. Verification

- gtest: `PitchHarmonicsTest` in `libs/audio/tests/pitch_harmonics_test.cpp`.
  The hardcoded SRH expectation will be **re-baselined** to this
  implementation's output.
- Parity: `tools/parity/specs/audio_pitch_harmonics.json` — `srh_first`,
  `srh_n`. `srh_n` (frame count) is unchanged; `srh_first` is
  re-baselined. The spec comment is updated to record that SRH is a
  paper-faithful implementation, intentionally not bit-matched to
  MATLAB.

## Constraints for the Implementer

- Do **not** open `libs/audio/src/features/pitch_harmonics.cpp`, nor
  `libs/signal/src/spectral_analysis/signal_modeling.cpp`.
- Do **not** consult MATLAB `.m` source or any third-party reference
  implementation (in particular not the COVAREP `pitch_srh.m`).
- Implement solely from this specification and the cited public paper.
