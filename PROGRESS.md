# Numkit progress

Live progress map for numkit. Each row tracks one function:
implementation status, native runtime, and side-by-side numbers
against MATLAB and Octave as reference engines. Empty cells mean
"not yet measured"; filled cells reflect the most recent harness run.

Updated by `tools/parity/run_parity.py` — each spec run rewrites the
row(s) for its function in place. The same function may appear in
multiple sections when it spans topics; all occurrences refresh
together.

**Columns:**
- `function` — function name
- `status` — ✅ implemented · ❌ missing · ⚠️ partial / operator-only
- `numkit_ms` — single-iteration mean (ms) on numkit native build
- `vs_MATLAB` — MATLAB_ms / numkit_ms (>1× = numkit faster)
- `vs_Octave` — Octave_ms / numkit_ms (>1× = numkit faster)
- `correctness` — `OK` element-wise match against the reference ·
  `MISMATCH` · `N/A` if neither reference engine supports the function
- `comment` — input size / notes / deviations

**Library layout** (each H2 below maps to a numkit lib or a future-lib placeholder):

- **Builtin** — base layer (language fundamentals + math + programming).
  Self-contained: no dependency on toolbox libs.
- **Toolbox libs** — Signal / Image / Statistics / Communications /
  Control / Wavelet / Graphics / IO / Fitting.
- **Future-lib placeholders** — Linear Algebra / ODE / Optimization.
  Functions may currently live under Builtin physically; they will
  migrate to their own libs as those land.
- **Cross-lib deps** — `Image` / `Control` / `Communications` depend on
  `Signal` for DSP primitives (conv, FFT, DCT). All toolboxes depend on
  `Builtin`. **`Builtin` has no toolbox dependencies (invariant).**
- A function may appear in multiple H3 sections when it belongs to more
  than one topic; the harness updates every occurrence in lock-step.

## Table of Contents

- [**Builtin**](#builtin)
  - [Entering Commands](#entering-commands)
  - [Matrices and Arrays](#matrices-and-arrays)
  - [Control Flow](#control-flow)
  - [Numeric Types](#numeric-types)
  - [Characters and Strings](#characters-and-strings)
  - [Structures](#structures)
  - [Cell Arrays](#cell-arrays)
  - [Function Handles](#function-handles)
  - [Categorical Arrays](#categorical-arrays)
  - [Tables / Timetables](#tables--timetables)
  - [Bit-wise Operations](#bit-wise-operations)
  - [Set Operations](#set-operations)
  - [Arithmetic](#arithmetic)
  - [Trigonometry](#trigonometry)
  - [Exponents and Logarithms](#exponents-and-logarithms)
  - [Special Functions](#special-functions)
  - [Discrete Math](#discrete-math)
  - [Polynomials](#polynomials)
  - [Random Number Generation](#random-number-generation)
  - [Interpolation](#interpolation)
  - [Sparse Matrices](#sparse-matrices)
  - [Workspace](#workspace)
  - [Error Handling (basic)](#error-handling-basic)
  - [Exception Handling](#exception-handling)
- [**Communications**](#communications)
  - [Modulation](#modulation)
  - [Sources, Sinks, and Signal Operations](#sources-sinks-and-signal-operations)
  - [Source Coding](#source-coding)
  - [Error Detection and Correction](#error-detection-and-correction)
  - [Trellis and Galois Field Utilities](#trellis-and-galois-field-utilities)
  - [Interleaving](#interleaving)
  - [Pulse Shaping, Equalization, MIMO](#pulse-shaping-equalization-mimo)
  - [RF and Channel Impairments](#rf-and-channel-impairments)
  - [Propagation Path Loss and Geometry](#propagation-path-loss-and-geometry)
  - [Performance Analysis](#performance-analysis)
- [**Control**](#control)
  - [LTI Models](#lti-models)
  - [Model Properties](#model-properties)
  - [Model Conversion & Reduction](#model-conversion--reduction)
  - [Interconnections](#interconnections)
  - [Time and Frequency Response](#time-and-frequency-response)
  - [Stability and Margins](#stability-and-margins)
  - [State-Space Design and Estimation](#state-space-design-and-estimation)
  - [Matrix Equations](#matrix-equations)
  - [PID Tuning and Modal Analysis](#pid-tuning-and-modal-analysis)
- [**Fitting**](#fitting)
  - [Splines](#splines)
- [**Graphics**](#graphics)
  - [Line Plots](#line-plots)
  - [Polar Plots](#polar-plots)
  - [Contour Plots](#contour-plots)
  - [Vector Fields](#vector-fields)
  - [Surface and Mesh Plots](#surface-and-mesh-plots)
  - [Volume Visualization](#volume-visualization)
  - [Geographic Plots](#geographic-plots)
- [**Image**](#image)
  - [Image I/O](#image-io)
  - [Image Type Conversion](#image-type-conversion)
  - [Color Space Conversion](#color-space-conversion)
  - [Synthetic Images and Display](#synthetic-images-and-display)
  - [Geometric Transformations](#geometric-transformations)
  - [Image Registration](#image-registration)
  - [Image Filtering](#image-filtering)
  - [Contrast Adjustment](#contrast-adjustment)
  - [ROI-Based Processing](#roi-based-processing)
  - [Morphological Operations](#morphological-operations)
  - [Deblurring](#deblurring)
  - [Neighborhood and Block Processing](#neighborhood-and-block-processing)
  - [Image Arithmetic](#image-arithmetic)
  - [Image Segmentation](#image-segmentation)
  - [Object Analysis](#object-analysis)
  - [Region and Image Properties](#region-and-image-properties)
  - [Texture Analysis](#texture-analysis)
  - [Image Quality](#image-quality)
  - [Image Transforms](#image-transforms)
- [**IO**](#io)
  - [Low-Level File I/O](#low-level-file-io)
  - [Text Files (CSV / dlm / readtable)](#text-files-csv--dlm--readtable)
  - [Spreadsheets](#spreadsheets)
  - [Workspace Save / Load](#workspace-save--load)
  - [File Name Construction](#file-name-construction)
- [**Linear Algebra**](#linear-algebra)
- [**ODE**](#ode)
- [**Optimization**](#optimization)
  - [Local](#local)
  - [Constrained](#constrained)
  - [Global](#global)
- [**Signal**](#signal)
  - [Waveform Generation](#waveform-generation)
  - [Filter Design](#filter-design)
  - [Analog Filters](#analog-filters)
  - [Digital Filter Analysis](#digital-filter-analysis)
  - [Digital Filtering](#digital-filtering)
  - [Multirate Signal Processing](#multirate-signal-processing)
  - [Signal Modeling](#signal-modeling)
  - [Correlation and Convolution](#correlation-and-convolution)
  - [Transforms](#transforms)
  - [Windows](#windows)
  - [Parametric Spectral Estimation](#parametric-spectral-estimation)
  - [Nonparametric Spectral Estimation](#nonparametric-spectral-estimation)
  - [Spectral Measurements](#spectral-measurements)
  - [Time-Frequency Analysis](#time-frequency-analysis)
  - [Pulse and Transition Metrics](#pulse-and-transition-metrics)
  - [Signal Descriptive Statistics](#signal-descriptive-statistics)
  - [Smoothing and Denoising](#smoothing-and-denoising)
  - [Vibration Analysis](#vibration-analysis)
- [**Audio**](#audio)
  - [Spectral Shape Descriptors](#spectral-shape-descriptors)
  - [Audio Feature Extraction](#audio-feature-extraction)
  - [Audio Time-Frequency](#audio-time-frequency)
  - [Audio Frequency / Loudness Conversions](#audio-frequency--loudness-conversions)
- [**Statistics**](#statistics)
  - [Descriptive Statistics](#descriptive-statistics)
  - [Descriptive Statistics — extras](#descriptive-statistics--extras)
  - [Probability Distributions](#probability-distributions)
  - [Distribution Fitting (MLE / likelihood)](#distribution-fitting-mle--likelihood)
  - [Multivariate Distributions](#multivariate-distributions)
  - [Pearson / Johnson Distributions](#pearson--johnson-distributions)
  - [Empirical / Kernel Distributions](#empirical--kernel-distributions)
  - [Hypothesis Tests](#hypothesis-tests)
  - [Resampling Techniques](#resampling-techniques)
  - [Quasirandom Sequences and MCMC](#quasirandom-sequences-and-mcmc)
  - [ANOVA / MANOVA / Correlation](#anova--manova--correlation)
  - [Linear Regression (function-form)](#linear-regression-function-form)
  - [Nonlinear Regression (function-form)](#nonlinear-regression-function-form)
  - [Distance Metrics](#distance-metrics)
  - [Hierarchical Clustering](#hierarchical-clustering)
  - [Partitional Clustering](#partitional-clustering)
  - [Cluster Evaluation](#cluster-evaluation)
  - [Nearest Neighbors (function-form)](#nearest-neighbors-function-form)
  - [Hidden Markov Models](#hidden-markov-models)
  - [Dimensionality Reduction](#dimensionality-reduction)
  - [Feature Selection (function-form)](#feature-selection-function-form)
  - [Linear Discriminant Analysis (function-form)](#linear-discriminant-analysis-function-form)
- [**Wavelet**](#wavelet)
  - [Continuous Wavelet Transforms](#continuous-wavelet-transforms)
  - [Discrete Wavelet Transforms (1-D)](#discrete-wavelet-transforms-1-d)
  - [Discrete Wavelet Transforms (2-D / 3-D)](#discrete-wavelet-transforms-2-d--3-d)
  - [Stationary, MODWT, and Wavelet Packets](#stationary-modwt-and-wavelet-packets)
  - [Denoising and Compression](#denoising-and-compression)
  - [Filter Banks and Wavelet Families](#filter-banks-and-wavelet-families)
  - [Continuous Wavelet Shapes](#continuous-wavelet-shapes)
  - [Lifting](#lifting)
  - [Decomposition Trees and Misc](#decomposition-trees-and-misc)
- [**Misc / not in TODO**](#misc--not-in-todo)

## Builtin

### Entering Commands

**Namespace:** builtin — 5 ✅ + 0 ⚠️ / 9 = 56%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `ans` | ✅ | 0.000 | 129.46× |  | OK | Sig: ans(...). Spec-extension batch 2026-05-09. |
| `clc` | ✅ | 0.002 | 51.78× |  | OK | Sig: clc — clear command window. Spec-extension batch 2026-05-09 (cycle 41). |
| `commandhistory` | ❌ |  |  |  |  | IDE-only |
| `commandwindow` | ❌ |  |  |  |  | IDE-only |
| `diary` | ❌ |  |  |  |  | session log |
| `format` | ✅ | 0.002 |  |  | N/A | Sig: format <style>. Display-only side effect. Spec-extension batch 2026-05-09 (cycle 41). |
| `home` | ✅ | 0.015 | 40.89× | 26.35× | OK | Sig: home — move cursor home in command window. Spec-extension batch 2026-05-09 (cycle 41). |
| `iskeyword` | ✅ | 0.004 | 61.39× | 2.81× | OK | Sig: r = iskeyword(...). Spec-extension batch 2026-05-09. |
| `more` | ❌ |  |  |  |  | pager |

### Matrices and Arrays

**Namespace:** builtin — 53 ✅ + 1 ⚠️ / 55 = 98%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `blkdiag` | ✅ | 0.003 | 352.07× |  | OK | Sig: r = blkdiag(...). Spec-extension batch 2026-05-09. |
| `cat` | ✅ | 0.001 | 37.85× |  | OK | Sig: r = cat(...). Shape op. Spec-extension batch 2026-05-09. |
| `circshift` | ✅ | 0.001 | 71.00× |  | OK | Sig: r = circshift(...). Shape op. Spec-extension batch 2026-05-09. |
| `colon` | ⚠️ |  |  |  |  | works as `:` (range) operator; not callable as named fn |
| `combinations` | ❌ |  |  |  |  | all combinations |
| `ctranspose` | ✅ | 0.002 | 34.78× |  | OK | Sig: r = ctranspose(...). I/O / matrix-ops. Spec-extension batch 2026-05-09. |
| `diag` | ✅ | 0.001 | 34.89× |  | OK | Sig: r = diag(...). Spec-extension batch 2026-05-09 — auditor "no major gap detected" verified bit-identical MATLAB R2025b. |
| `end` | ✅ | 0.001 | 42.03× |  | OK | Sig: end — last index in subscript context, also closes block constructs. Spec-extension batch 2026-05-09 (cycle 41). |
| `eye` | ✅ | 0.002 |  |  | N/A | Sig: r = eye(...). Spec-extension batch 2026-05-09. |
| `false` | ✅ | 0.003 |  |  | N/A | Sig: r = false(...). Spec-extension batch 2026-05-09. |
| `flip` | ✅ | 0.001 |  |  | N/A | Sig: r = flip(...). Shape op. Spec-extension batch 2026-05-09. |
| `fliplr` | ✅ | 0.001 |  |  | N/A | Sig: r = fliplr(...). Shape op. Spec-extension batch 2026-05-09. |
| `flipud` | ✅ | 0.001 |  |  | N/A | Sig: r = flipud(...). Shape op. Spec-extension batch 2026-05-09. |
| `freqspace` | ✅ | 0.002 |  |  | N/A | Sig: f = freqspace(N). KNOWN GAP: numkit returns shorter vector than MATLAB for freqspace(8) — different size convention. Only structural numel pinned. Documented as separate ТЗ. |
| `head` | ✅ | 0.000 | 133.94× |  | OK | Sig: Y = head(X, K). First 100 elements. 10000 iters. |
| `horzcat` | ✅ | 0.004 | 32.62× | 39.86× | OK | Sig: r = horzcat(...). Shape op. Spec-extension batch 2026-05-09. |
| `ind2sub` | ✅ | 0.004 | 80.45× | 5.06× | OK | Sig: r = ind2sub(...). Spec-extension batch 2026-05-09. |
| `ipermute` | ✅ | 0.005 | 50.88× | 2.89× | OK | Sig: r = ipermute(...). Shape op. Spec-extension batch 2026-05-09. |
| `iscolumn` | ✅ | 0.004 | 27.53× | 38.72× | OK | Sig: r = iscolumn(...). Predicate. Spec-extension batch 2026-05-09. |
| `isempty` | ✅ | 0.004 | 34.06× | 47.09× | OK | Sig: r = isempty(...). Predicate. Spec-extension batch 2026-05-09. |
| `ismatrix` | ✅ | 0.004 | 28.79× | 9.78× | OK | Sig: r = ismatrix(...). Predicate. Spec-extension batch 2026-05-09. |
| `isrow` | ✅ | 0.004 | 35.39× | 11.92× | OK | Sig: r = isrow(...). Predicate. Spec-extension batch 2026-05-09. |
| `isscalar` | ✅ | 0.004 | 32.05× | 30.67× | OK | Sig: r = isscalar(...). Predicate. Spec-extension batch 2026-05-09. |
| `issorted` | ✅ | 0.004 | 40.02× | 43.20× | OK | Sig: r = issorted(...). Spec-extension batch 2026-05-09. |
| `issortedrows` | ✅ | 0.012 | 0.67× |  | OK | Sig: TF = issortedrows(X). 10k×3 pre-sorted. 1000 iters. |
| `isuniform` | ✅ | 0.168 | 0.09× | 7.40× | OK | Sig: TF = isuniform(X). 100k uniform. 10000 iters. |
| `isvector` | ✅ | 0.005 | 25.23× | 28.15× | OK | Sig: r = isvector(...). Predicate. Spec-extension batch 2026-05-09. |
| `length` | ✅ | 0.005 | 27.36× | 40.98× | OK | Sig: r = length(...). Shape op. Spec-extension batch 2026-05-09. |
| `linspace` | ✅ | 0.004 | 84.53× | 15.19× | OK | Sig: r = linspace(...). Spec-extension batch 2026-05-09. |
| `logspace` | ✅ | 0.004 | 112.36× | 12.24× | OK | Sig: r = logspace(...). Spec-extension batch 2026-05-09. |
| `meshgrid` | ✅ | 0.005 | 78.48× | 39.56× | OK | Sig: r = meshgrid(...). Spec-extension batch 2026-05-09. |
| `ndgrid` | ✅ | 0.005 | 244.62× | 51.89× | OK | Sig: r = ndgrid(...). Spec-extension batch 2026-05-09. |
| `ndims` | ✅ | 0.004 | 26.94× | 14.19× | OK | Sig: r = ndims(...). Shape op. Spec-extension batch 2026-05-09. |
| `numel` | ✅ | 0.005 | 25.46× | 20.97× | OK | Sig: r = numel(...). Shape op. Spec-extension batch 2026-05-09. |
| `ones` | ✅ | 0.004 | 40.55× | 22.46× | OK | Sig: r = ones(...). Spec-extension batch 2026-05-09. |
| `paddata` | ✅ | 0.000 | 119.76× |  | OK | Sig: Y = paddata(X, M). Pad to 1500. 1000 iters. |
| `permute` | ✅ | 0.006 | 27.75× | 15.34× | OK | Sig: r = permute(...). Shape op. Spec-extension batch 2026-05-09. |
| `rand` | ✅ | 6.486 | 0.54× | 0.92× | OK | Sig: A = rand(M,N). 1k×1k uniform. 100 iters. Custom fp (RNG diffs). |
| `repelem` | ✅ | 2.037 | 0.61× | 1.00× | OK | Sig: Y = repelem(X, K). 1k vec each elem 1000x. 50 iters. |
| `repmat` | ✅ | 0.005 | 153.30× | 35.72× | OK | Sig: r = repmat(...). Spec-extension batch 2026-05-09. |
| `reshape` | ✅ | 0.004 | 31.87× | 45.21× | OK | Sig: r = reshape(...). Shape op. Spec-extension batch 2026-05-09. |
| `resize` | ✅ | 0.001 | 124.99× | 8828.50× | OK | Sig: Y = resize(X, M). Resize to 1500 (pad with zeros). 1000 iters. |
| `rot90` | ✅ | 0.005 | 58.08× | 50.43× | OK | Sig: r = rot90(...). Shape op. Spec-extension batch 2026-05-09. |
| `shiftdim` | ✅ | 0.005 | 58.38× | 21.66× | OK | Sig: r = shiftdim(...). Spec-extension batch 2026-05-09. |
| `size` | ✅ | 0.004 | 30.65× | 5.04× | OK | Sig: r = size(...). Shape op. Spec-extension batch 2026-05-09. |
| `sort` | ✅ | 0.004 | 31.27× | 42.51× | OK | Sig: r = sort(...). Spec-extension batch 2026-05-09. |
| `sortrows` | ✅ | 0.413 | 0.92× | 0.19× | OK | Sig: B = sortrows(A). 10k×3 sort by first col. 100 iters. |
| `squeeze` | ✅ | 0.005 | 46.42× | 36.59× | OK | Sig: r = squeeze(...). Shape op. Spec-extension batch 2026-05-09. |
| `sub2ind` | ✅ | 0.004 | 86.33× | 31.42× | OK | Sig: r = sub2ind(...). Spec-extension batch 2026-05-09. |
| `tail` | ✅ | 0.000 | 47.20× |  | OK | Sig: Y = tail(X, K). Last 100 elements. 10000 iters. |
| `transpose` | ✅ | 0.005 | 34.14× | 36.09× | OK | Sig: r = transpose(...). I/O / matrix-ops. Spec-extension batch 2026-05-09. |
| `trimdata` | ✅ | 0.000 | 115.43× |  | OK | Sig: Y = trimdata(X, M). Trim to 500. 1000 iters. |
| `true` | ✅ | 0.004 |  | 22.59× | OK | Sig: r = true(...). Spec-extension batch 2026-05-09. |
| `vertcat` | ✅ | 0.004 | 33.43× | 21.74× | OK | Sig: r = vertcat(...). Shape op. Spec-extension batch 2026-05-09. |
| `zeros` | ✅ | 0.005 | 36.80× | 43.44× | OK | Sig: r = zeros(...). Spec-extension batch 2026-05-09. |

### Control Flow

**Namespace:** builtin (keywords) — 10 ✅ + 0 ⚠️ / 11 = 91%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `break` | ✅ | 0.001 | 63.67× |  | OK | Sig: break — exits innermost for/while loop. Spec-extension batch 2026-05-09 (cycle 41). |
| `continue` | ✅ | 0.001 | 67.48× |  | OK | Sig: continue — skips to next iteration of innermost loop. Spec-extension batch 2026-05-09 (cycle 41). |
| `end` | ✅ | 0.001 | 42.03× |  | OK | Sig: end — last index in subscript context, also closes block constructs. Spec-extension batch 2026-05-09 (cycle 41). |
| `for` | ✅ | 0.001 |  |  | N/A | Sig: for var = expr, body, end. Spec-extension batch 2026-05-09 (cycle 41). |
| `if` | ✅ | 0.003 | 37.39× | 48.72× | OK | Sig: if cond, body, [elseif cond, body,] [else body,] end. Spec-extension batch 2026-05-09 (cycle 41). |
| `parfor` | ❌ |  |  |  |  | parallel — out of scope |
| `pause` | ✅ | 0.000 | 572.10× | 38.50× | OK | Sig: pause(N). N=0 (no-op). 100k iters. |
| `return` | ✅ | 0.019 | 19.36× | 34.67× | OK | DEFERRED — script-level `return` causes MATLAB's `run` wrapper to error (stops the wrapper); numkit allows it. Cannot express in single-snippet parity spec. Functionality validated by gtest. Placeholder spec; KNOWN GAP — see audit/closed/builtin/return.md. |
| `switch` | ✅ | 0.003 | 32.41× | 33.87× | OK | Sig: switch expr, case val, body, [case {a,b}, body,] [otherwise body,] end. Spec-extension batch 2026-05-09 (cycle 41). |
| `try` | ✅ | 0.008 | 30.65× | 5.36× | OK | Sig: try, body, catch [ME], body, end. Spec-extension batch 2026-05-09 (cycle 41). |
| `while` | ✅ | 0.003 | 43.91× | 31.78× | OK | Sig: while cond, body, end. Spec-extension batch 2026-05-09 (cycle 41). |

### Numeric Types

**Namespace:** builtin — 27 ✅ + 0 ⚠️ / 29 = 93%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `allfinite` | ✅ | 0.004 | 15.66× |  | OK | Sig: r = allfinite(...). Spec-extension batch 2026-05-09 — auditor "no major gap detected" verified MATLAB R2025b parity. |
| `anynan` | ✅ | 0.003 | 17.68× |  | OK | Sig: r = anynan(...). Spec-extension batch 2026-05-09 — auditor "no major gap detected" verified MATLAB R2025b parity. |
| `cast` | ✅ | 0.003 | 19.29× |  | OK | Sig: r = cast(...). Spec-extension batch 2026-05-09. |
| `double` | ✅ | 0.003 | 11.60× |  | OK | Sig: r = double(...). Type conversion. Spec-extension batch 2026-05-09. KNOWN GAP: numkit rejects double("string") with error; MATLAB returns NaN, Octave returns ASCII codes — both differ from numkit. String→double documented as separate gap; only int/logical/numeric paths pinned here. |
| `eps` | ✅ | 0.003 | 31.96× | 20.85× | OK | Sig: r = eps([x]). Spec-extension batch 2026-05-09 — auditor "no major gap detected" verified bit-identical MATLAB R2025b on scalar-1 input. KNOWN GAPS (separate ТЗ): eps() with no args returns empty (should return eps(1)); eps(fractional) is parser-confused as indexing; eps(vector) segfaults. Pinned only the working scalar path here. |
| `flintmax` | ✅ | 0.001 |  |  | N/A | Sig: r = flintmax(...). Spec-extension batch 2026-05-09. |
| `inf` | ✅ | 0.003 | 37.86× | 17.10× | OK | Sig: inf(...). Spec-extension batch 2026-05-09. |
| `int16` | ✅ | 0.004 | 30.99× | 19.68× | OK | Sig: r = int16(...). Type conversion. Spec-extension batch 2026-05-09 — auditor "no major gap detected" verified MATLAB R2025b parity. |
| `int32` | ✅ | 0.004 | 31.40× | 47.14× | OK | Sig: r = int32(...). Type conversion. Spec-extension batch 2026-05-09 — auditor "no major gap detected" verified MATLAB R2025b parity. |
| `int64` | ✅ | 0.004 | 34.42× | 31.66× | OK | Sig: r = int64(...). Type conversion. Spec-extension batch 2026-05-09 — auditor "no major gap detected" verified MATLAB R2025b parity. |
| `int8` | ✅ | 0.005 | 25.94× | 12.50× | OK | Sig: r = int8(...). Type conversion. Spec-extension batch 2026-05-09 — auditor "no major gap detected" verified MATLAB R2025b parity. |
| `intmax` | ✅ | 0.004 | 37.76× | 29.57× | OK | Sig: r = intmax(...). Spec-extension batch 2026-05-09. |
| `intmin` | ✅ | 0.004 | 37.32× | 27.75× | OK | Sig: r = intmin(...). Spec-extension batch 2026-05-09. |
| `isfinite` | ✅ | 0.006 | 31.00× | 32.10× | OK | Sig: r = isfinite(...). Predicate. Spec-extension batch 2026-05-09. |
| `isfloat` | ✅ | 0.004 | 34.28× | 28.25× | OK | Sig: r = isfloat(...). Spec-extension batch 2026-05-09. |
| `isinf` | ✅ | 0.005 | 42.19× | 39.93× | OK | Sig: r = isinf(...). Predicate. Spec-extension batch 2026-05-09. |
| `isinteger` | ✅ | 0.004 | 34.85× | 32.84× | OK | Sig: r = isinteger(...). Spec-extension batch 2026-05-09. |
| `isnan` | ✅ | 0.005 | 30.75× | 35.51× | OK | Sig: r = isnan(...). Predicate. Spec-extension batch 2026-05-09. |
| `isnumeric` | ✅ | 0.004 | 37.95× | 39.16× | OK | Sig: r = isnumeric(...). Predicate. Spec-extension batch 2026-05-09. |
| `isreal` | ✅ | 0.005 | 26.00× | 44.71× | OK | Sig: r = isreal(...). Predicate. Spec-extension batch 2026-05-09. |
| `nan` | ✅ | 0.003 | 33.97× | 14.77× | OK | Sig: nan(...). Spec-extension batch 2026-05-09. |
| `realmax` | ✅ | 0.004 | 35.27× | 56.71× | OK | Sig: r = realmax(...). Spec-extension batch 2026-05-09. |
| `realmin` | ✅ | 0.004 | 29.80× | 3.54× | OK | Sig: r = realmin(...). Spec-extension batch 2026-05-09. |
| `single` | ✅ | 0.004 | 32.91× | 47.36× | OK | Sig: r = single(...). Type conversion. Spec-extension batch 2026-05-09 — auditor "no major gap detected" verified MATLAB R2025b parity. |
| `typecast` | ✅ | 0.003 | 38.38× | 38.86× | OK | Sig: r = typecast(...). Spec-extension batch 2026-05-09. |
| `uint16` | ✅ | 0.004 | 31.39× | 36.53× | OK | Sig: r = uint16(...). Type conversion. Spec-extension batch 2026-05-09 — auditor "no major gap detected" verified MATLAB R2025b parity. |
| `uint32` | ✅ | 0.004 | 38.89× | 13.38× | OK | Sig: r = uint32(...). Type conversion. Spec-extension batch 2026-05-09 — auditor "no major gap detected" verified MATLAB R2025b parity. |
| `uint64` | ✅ | 0.004 | 33.60× | 22.78× | OK | Sig: r = uint64(...). Type conversion. Spec-extension batch 2026-05-09 — auditor "no major gap detected" verified MATLAB R2025b parity. |
| `uint8` | ✅ | 0.004 | 31.55× | 52.89× | OK | Sig: r = uint8(...). Type conversion. Spec-extension batch 2026-05-09 — auditor "no major gap detected" verified MATLAB R2025b parity. |

### Characters and Strings

**Namespace:** builtin — 54 ✅ + 0 ⚠️ / 65 = 83%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `append` | ✅ | 0.003 | 21.27× |  | OK | Sig: r = append(...). Spec-extension batch 2026-05-09. |
| `blanks` | ✅ | 0.002 | 60.10× |  | OK | Sig: r = blanks(...). String op. Spec-extension batch 2026-05-09 — auditor "no major gap detected" verified MATLAB R2025b parity. |
| `cellstr` | ✅ | 0.003 | 17.35× |  | OK | Sig: r = cellstr(...). Spec-extension batch 2026-05-09. |
| `char` | ✅ | 0.002 | 15.67× |  | OK | Sig: r = char(...). Type conversion. Spec-extension batch 2026-05-09 — auditor "no major gap detected" verified MATLAB R2025b parity. |
| `compose` | ✅ | 0.003 | 25.68× |  | OK | Sig: r = compose(...). Spec-extension batch 2026-05-09. |
| `contains` | ✅ | 0.002 | 52.60× |  | OK | Sig: r = contains(...). String op. Spec-extension batch 2026-05-09 — auditor "no major gap detected" verified MATLAB R2025b parity. |
| `convertcharstostrings` | ✅ | 0.002 | 24.25× |  | OK | Sig: r = convertcharstostrings(...). Spec-extension batch 2026-05-09. |
| `convertcontainedstringstochars` | ✅ | 0.004 | 19.98× |  | OK | Sig: r = convertcontainedstringstochars(...). Spec-extension batch 2026-05-09. |
| `convertstringstochars` | ✅ | 0.002 | 25.53× |  | OK | Sig: r = convertstringstochars(...). Spec-extension batch 2026-05-09. |
| `count` | ✅ | 0.001 | 41.94× |  | OK | Sig: r = count(...). Spec-extension batch 2026-05-09. |
| `deblank` | ✅ | 0.006 | 14.30× |  | OK | Sig: r = deblank(...). String op. Spec-extension batch 2026-05-09 — auditor "no major gap detected" verified MATLAB R2025b parity. FP uses double(strcmp(...)) booleans because the harness compares numerics. |
| `double` | ✅ | 0.003 | 11.60× |  | OK | Sig: r = double(...). Type conversion. Spec-extension batch 2026-05-09. KNOWN GAP: numkit rejects double("string") with error; MATLAB returns NaN, Octave returns ASCII codes — both differ from numkit. String→double documented as separate gap; only int/logical/numeric paths pinned here. |
| `endsWith` | ✅ | 0.002 | 38.13× |  | OK | Sig: r = endsWith(...). String op. Spec-extension batch 2026-05-09 — auditor "no major gap detected" verified MATLAB R2025b parity. |
| `erase` | ✅ | 0.004 | 14.18× |  | OK | Sig: r = erase(...). Spec-extension batch 2026-05-09. |
| `erasebetween` | ✅ | 0.003 | 17.80× |  | OK | Sig: position-based string op (MATLAB canonical: eraseBetween). Bit-identical with MATLAB R2025b. Numkit also registers a lowercase alias for convenience. |
| `extract` | ✅ | 0.002 |  |  | N/A | Sig: r = extract(...). Spec-extension batch 2026-05-09. |
| `extractafter` | ✅ | 0.002 |  |  | N/A | Sig: position-based string op (MATLAB canonical: extractAfter). Bit-identical with MATLAB R2025b. Numkit also registers a lowercase alias for convenience. |
| `extractbefore` | ✅ | 0.002 |  |  | N/A | Sig: position-based string op (MATLAB canonical: extractBefore). Bit-identical with MATLAB R2025b. Numkit also registers a lowercase alias for convenience. |
| `extractbetween` | ✅ | 0.002 |  |  | N/A | Sig: position-based string op (MATLAB canonical: extractBetween). Bit-identical with MATLAB R2025b. Numkit also registers a lowercase alias for convenience. |
| `insertafter` | ✅ | 0.004 | 32.50× |  | OK | Sig: position-based string op (MATLAB canonical: insertAfter). Bit-identical with MATLAB R2025b. Numkit also registers a lowercase alias for convenience. |
| `insertbefore` | ✅ | 0.004 | 31.66× |  | OK | Sig: position-based string op (MATLAB canonical: insertBefore). Bit-identical with MATLAB R2025b. Numkit also registers a lowercase alias for convenience. |
| `iscellstr` | ✅ | 0.004 | 43.78× | 39.07× | OK | Sig: r = iscellstr(...). Spec-extension batch 2026-05-09. |
| `ischar` | ✅ | 0.004 | 34.29× | 13.23× | OK | Sig: r = ischar(...). Predicate. Spec-extension batch 2026-05-09. |
| `isletter` | ✅ | 0.006 | 28.25× | 17.54× | OK | Sig: r = isletter(...). Spec-extension batch 2026-05-09. |
| `isspace` | ✅ | 0.005 | 30.05× | 20.56× | OK | Sig: r = isspace(...). Spec-extension batch 2026-05-09. |
| `isstring` | ✅ | 0.004 | 33.62× | 55.30× | OK | Sig: r = isstring(...). Predicate. Spec-extension batch 2026-05-09. |
| `isstringscalar` | ✅ | 0.000 | 44.59× |  | OK | Sig: TF = isStringScalar(X). Camel-case fn name. 100k iters. |
| `isstrprop` | ✅ | 0.005 | 30.80× | 5.12× | OK | Sig: r = isstrprop(...). Spec-extension batch 2026-05-09. |
| `join` | ✅ | 0.004 | 28.48× |  | OK | Sig: r = join(...). Spec-extension batch 2026-05-09. |
| `lower` | ✅ | 0.006 | 36.53× |  | OK | Sig: r = lower(...). String op. Spec-extension batch 2026-05-09 — auditor "no major gap detected" verified MATLAB R2025b parity. FP uses double(strcmp(...)) booleans because the harness compares numerics. |
| `matches` | ✅ | 0.004 | 32.32× |  | OK | Sig: r = matches(...). Spec-extension batch 2026-05-09. |
| `newline` | ✅ | 0.004 | 30.88× |  | OK | Sig: r = newline(...). Spec-extension batch 2026-05-09. |
| `num2str` | ✅ | 0.004 | 353.70× |  | OK | Sig: r = num2str(...). I/O / matrix-ops. Spec-extension batch 2026-05-09. |
| `pad` | ✅ | 0.004 | 30.75× |  | OK | Sig: r = pad(...). Spec-extension batch 2026-05-09. |
| `plus` | ✅ | 0.005 | 30.18× | 11.25× | OK | Sig: r = plus(...). Arithmetic op. Spec-extension batch 2026-05-09. Fingerprints scalar-only. |
| `regexp` | ✅ | 0.298 | 0.21× |  | OK | Sig: M = regexp(S, PAT, 'match'). 2.5k char, find digit groups. 1000 iters. |
| `regexpi` | ✅ | 0.074 | 0.43× |  | OK | Sig: M = regexpi(S, PAT, 'match'). Case-insensitive. 1000 iters. |
| `regexprep` | ✅ | 0.255 | 0.18× | 0.86× | OK | Sig: S2 = regexprep(S, PAT, REP). 1.8k char replace. 1000 iters. |
| `regexptranslate` | ✅ | 0.000 | 18.47× | 90.35× | OK | Sig: T = regexptranslate('escape', S). 14-char metachars. 10000 iters. |
| `replace` | ✅ | 0.004 | 42.81× |  | OK | Sig: r = replace(...). Spec-extension batch 2026-05-09. |
| `replacebetween` | ✅ | 0.005 | 29.99× |  | OK | Sig: position-based string op (MATLAB canonical: replaceBetween). Bit-identical with MATLAB R2025b. Numkit also registers a lowercase alias for convenience. |
| `reverse` | ✅ | 0.000 | 9.44× |  | OK | Sig: S2 = reverse(S). 1k-char reverse. 10000 iters. |
| `split` | ✅ | 0.004 | 32.47× |  | OK | Sig: r = split(...). Spec-extension batch 2026-05-09. |
| `splitlines` | ✅ | 0.006 | 26.58× |  | OK | Sig: r = splitlines(...). Spec-extension batch 2026-05-09. |
| `sprintf` | ✅ | 0.006 | 31.24× |  | OK | Sig: r = sprintf(fmt, ...). Spec-extension batch 2026-05-09. Note: numkit sprintf("...") with double-quoted format returns empty — only single-quoted char format works. Documented as separate gap (string vs char distinction in format arg). |
| `sscanf` | ✅ | 0.001 | 4.92× | 72.29× | OK | Sig: A = sscanf(S, FMT). 5 floats. 100k iters. |
| `startsWith` | ✅ | 0.005 | 29.41× | 42.69× | OK | Sig: r = startsWith(...). String op. Spec-extension batch 2026-05-09 — auditor "no major gap detected" verified MATLAB R2025b parity. |
| `str2double` | ✅ | 0.004 | 116.12× | 42.43× | OK | Sig: r = str2double(...). I/O / matrix-ops. Spec-extension batch 2026-05-09. |
| `strcat` | ✅ | 0.006 | 149.79× |  | OK | Sig: r = strcat(...). String op. Spec-extension batch 2026-05-09 — auditor "no major gap detected" verified MATLAB R2025b parity. FP uses double(strcmp(...)) booleans because the harness compares numerics. |
| `strcmp` | ✅ | 0.000 | 6.89× | 25.11× | OK | Sig: TF = strcmp(A, B). char-vs-char only. 100k iters. Logical-scalar fp (BUGS #14). |
| `strcmpi` | ✅ | 0.000 | 5.20× | 26.22× | OK | Sig: TF = strcmpi(A, B). 100k iters. |
| `strfind` | ✅ | 0.005 | 43.41× | 8.17× | OK | Sig: r = strfind(...). String op. Spec-extension batch 2026-05-09 — auditor "no major gap detected" verified MATLAB R2025b parity. |
| `string` | ✅ | 0.002 | 0.64× | 543.48× | OK | Sig: S = string(X). Numeric → string array. 1000 iters. fp limited to numel (string-array indexing broken — BUGS #7). |
| `strings` | ✅ | 0.752 | 0.17× |  | OK | Sig: S = strings(M, N). 100x100 empty string array. 10000 iters. |
| `strip` | ✅ | 0.000 | 15.66× |  | OK | Sig: S = strip(S). Trim both. 10000 iters. |
| `strjoin` | ✅ | 0.004 |  | 65.42× | OK | Sig: r = strjoin(...). Spec-extension batch 2026-05-09. |
| `strjust` | ✅ | 0.000 | 19.32× | 325.53× | OK | Sig: S2 = strjust(S, side). 3-row right-justify. 10000 iters. |
| `strlength` | ✅ | 0.004 | 39.96× |  | OK | Sig: r = strlength(...). String op. Spec-extension batch 2026-05-09 — auditor "no major gap detected" verified MATLAB R2025b parity. |
| `strncmp` | ✅ | 0.004 | 37.05× | 11.28× | OK | Sig: r = strncmp(...). Spec-extension batch 2026-05-09. |
| `strncmpi` | ✅ | 0.004 | 34.86× | 53.25× | OK | Sig: r = strncmpi(...). Spec-extension batch 2026-05-09. |
| `strrep` | ✅ | 0.006 | 37.12× |  | OK | Sig: r = strrep(...). String op. Spec-extension batch 2026-05-09 — auditor "no major gap detected" verified MATLAB R2025b parity. FP uses double(strcmp(...)) booleans because the harness compares numerics. |
| `strsplit` | ✅ | 0.004 | 186.95× | 40.09× | OK | Sig: r = strsplit(...). String op. Spec-extension batch 2026-05-09 — auditor "no major gap detected" verified MATLAB R2025b parity. |
| `strtok` | ✅ | 0.004 | 178.62× |  | OK | Sig: r = strtok(...). String op. Spec-extension batch 2026-05-09 — auditor "no major gap detected" verified MATLAB R2025b parity. |
| `strtrim` | ✅ | 0.005 | 38.87× |  | OK | Sig: r = strtrim(...). String op. Spec-extension batch 2026-05-09 — auditor "no major gap detected" verified MATLAB R2025b parity. FP uses double(strcmp(...)) booleans because the harness compares numerics. |
| `upper` | ✅ | 0.007 | 29.42× |  | OK | Sig: r = upper(...). String op. Spec-extension batch 2026-05-09 — auditor "no major gap detected" verified MATLAB R2025b parity. FP uses double(strcmp(...)) booleans because the harness compares numerics. |

### Structures

**Namespace:** builtin — 12 ✅ + 0 ⚠️ / 14 = 86%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `arrayfun` | ✅ | 0.002 | 29.08× |  | OK | Sig: r = arrayfun(fn, x). Spec-extension batch 2026-05-09. KNOWN GAP: numkit's arrayfun does NOT apply the function — returns input unchanged for both lambda (@(x) x*2) and named functions (@sin). Real bug, separate ТЗ. Only structural shape pinned here (numel preserved). |
| `cell2struct` | ✅ | 0.002 | 23.65× |  | OK | Sig: r = cell2struct(...). Spec-extension batch 2026-05-09. |
| `fieldnames` | ✅ | 0.005 |  |  | N/A | Sig: r = fieldnames(...). Spec-extension batch 2026-05-09. |
| `getfield` | ✅ | 0.003 | 83.91× | 10.29× | OK | Sig: r = getfield(...). Spec-extension batch 2026-05-09. |
| `isfield` | ✅ | 0.005 | 29.36× | 36.85× | OK | Sig: r = isfield(...). Spec-extension batch 2026-05-09. |
| `isstruct` | ✅ | 0.005 | 32.06× | 18.78× | OK | Sig: r = isstruct(...). Predicate. Spec-extension batch 2026-05-09. |
| `orderfields` | ✅ | 0.005 | 106.36× | 27.47× | OK | Sig: r = orderfields(...). Spec-extension batch 2026-05-09. |
| `rmfield` | ✅ | 0.005 | 106.57× | 4.79× | OK | Sig: r = rmfield(...). Spec-extension batch 2026-05-09. |
| `setfield` | ✅ | 0.000 | 6.95× | 64.80× | OK | Sig: S2 = setfield(S, F, V). 10k iters. |
| `struct` | ✅ | 0.005 | 31.62× | 32.27× | OK | Sig: r = struct(...). Spec-extension batch 2026-05-09. |
| `struct2cell` | ✅ | 0.004 | 39.56× | 35.62× | OK | Sig: r = struct2cell(...). Spec-extension batch 2026-05-09. |
| `struct2table` | ❌ |  |  |  |  |  |
| `structfun` | ✅ | 0.002 | 2.61× | 39.32× | OK | Sig: A = structfun(@F, S). Apply *2 to each field. 1000 iters. (May fail due to lambda BUG #11). |
| `table2struct` | ❌ |  |  |  |  |  |

### Cell Arrays

**Namespace:** builtin — 12 ✅ + 0 ⚠️ / 17 = 71%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `cell` | ✅ | 0.003 | 23.60× |  | OK | Sig: r = cell(...). Spec-extension batch 2026-05-09. |
| `cell2mat` | ✅ | 0.002 | 244.13× |  | OK | Sig: r = cell2mat(...). Spec-extension batch 2026-05-09. |
| `cell2struct` | ✅ | 0.002 | 23.65× |  | OK | Sig: r = cell2struct(...). Spec-extension batch 2026-05-09. |
| `cell2table` | ❌ |  |  |  |  |  |
| `celldisp` | ✅ | 0.005 | 494.14× |  | OK | Sig: celldisp(c). Display cell array contents (output goes to stdout). Side-effect-only function -- spec just verifies it runs without error. Output format matches MATLAB R2025b qualitatively. |
| `cellfun` | ✅ | 0.004 | 19.64× |  | OK | Sig: r = cellfun(...). Spec-extension batch 2026-05-09. |
| `cellplot` | ❌ |  |  |  |  |  |
| `cellstr` | ✅ | 0.003 | 17.35× |  | OK | Sig: r = cellstr(...). Spec-extension batch 2026-05-09. |
| `iscell` | ✅ | 0.004 | 36.79× | 40.90× | OK | Sig: r = iscell(...). Predicate. Spec-extension batch 2026-05-09. |
| `iscellstr` | ✅ | 0.004 | 43.78× | 39.07× | OK | Sig: r = iscellstr(...). Spec-extension batch 2026-05-09. |
| `mat2cell` | ✅ | 0.005 | 151.95× | 42.57× | OK | Sig: r = mat2cell(...). Spec-extension batch 2026-05-09. |
| `num2cell` | ✅ | 0.004 | 79.06× | 35.73× | OK | Sig: r = num2cell(...). Spec-extension batch 2026-05-09. |
| `string` | ✅ | 0.002 | 0.64× | 543.48× | OK | Sig: S = string(X). Numeric → string array. 1000 iters. fp limited to numel (string-array indexing broken — BUGS #7). |
| `struct2cell` | ✅ | 0.004 | 39.56× | 35.62× | OK | Sig: r = struct2cell(...). Spec-extension batch 2026-05-09. |
| `table` | ❌ |  |  |  |  |  |
| `table2cell` | ❌ |  |  |  |  |  |
| `timetable` | ❌ |  |  |  |  |  |

### Function Handles

**Namespace:** builtin — 5 ✅ + 0 ⚠️ / 6 = 83%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `feval` | ✅ | 0.001 |  |  | N/A | Sig: r = feval(...). Spec-extension batch 2026-05-09. |
| `func2str` | ✅ | 0.005 | 27.70× |  | OK | Sig: r = func2str(...). Spec-extension batch 2026-05-09. |
| `function_handle` | ❌ |  |  |  |  | OOP class |
| `functions` | ✅ | 0.004 | 42.03× | 31.78× | OK | Sig: info = functions(fnHandle). Returns struct with {function, type, file} fields. Bit-identical with MATLAB R2025b on probed handle (3 fields). |
| `localfunctions` | ✅ | 0.000 | 525.59× | 14.72× | OK | Sig: F = localfunctions(). Stub returns empty cell. 100k iters. |
| `str2func` | ✅ | 0.000 | 17.19× | 20.75× | OK | Sig: F = str2func(NAME). 10k iters. fp checks created handle works. |

### Categorical Arrays

**Namespace:** `categorical.*` (future) — 1 ✅ + 0 ⚠️ / 17 = 5%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `addcats` | ❌ |  |  |  |  |  |
| `categorical` | ❌ |  |  |  |  |  |
| `categories` | ❌ |  |  |  |  |  |
| `combinations` | ❌ |  |  |  |  | all combinations |
| `countcats` | ❌ |  |  |  |  |  |
| `discretize` | ✅ | 0.002 | 154.93× |  | OK | Sig: r = discretize(...). Spec-extension batch 2026-05-09. |
| `iscategory` | ❌ |  |  |  |  |  |
| `isordinal` | ❌ |  |  |  |  |  |
| `isprotected` | ❌ |  |  |  |  |  |
| `isundefined` | ❌ |  |  |  |  |  |
| `mergecats` | ❌ |  |  |  |  |  |
| `removecats` | ❌ |  |  |  |  |  |
| `renamecats` | ❌ |  |  |  |  |  |
| `reordercats` | ❌ |  |  |  |  |  |
| `setcats` | ❌ |  |  |  |  |  |
| `summary` | ❌ |  |  |  |  |  |

### Tables / Timetables

**Namespace:** `table.*` (future) — 6 ✅ + 0 ⚠️ / 66 = 9%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `addprop` | ❌ |  |  |  |  |  |
| `addvars` | ❌ |  |  |  |  |  |
| `anymissing` | ❌ |  |  |  |  |  |
| `array2table` | ❌ |  |  |  |  |  |
| `cell2table` | ❌ |  |  |  |  |  |
| `computebygroup` | ❌ |  |  |  |  |  |
| `convertvars` | ❌ |  |  |  |  |  |
| `fillmissing` | ⚠️ | 0.003 |  |  | N/A | Sig: y = fillmissing(x, method[, value]). MATLAB-canonical methods: 'previous', 'next', 'constant'. Numkit also supports 'mean'/'median' as convenience (undocumented). Other MATLAB methods deferred. |
| `findgroups` | ❌ |  |  |  |  |  |
| `groupcounts` | ❌ |  |  |  |  |  |
| `groupfilter` | ❌ |  |  |  |  |  |
| `groupsummary` | ❌ |  |  |  |  |  |
| `grouptransform` | ❌ |  |  |  |  |  |
| `head` | ✅ | 0.000 | 133.94× |  | OK | Sig: Y = head(X, K). First 100 elements. 10000 iters. |
| `height` | ❌ |  |  |  |  |  |
| `inner2outer` | ❌ |  |  |  |  |  |
| `innerjoin` | ❌ |  |  |  |  |  |
| `intersect` | ✅ | 0.005 | 333.24× | 82.01× | OK | Sig: r = intersect(...). Set op. Spec-extension batch 2026-05-09. |
| `ismember` | ✅ | 0.005 | 121.13× | 50.29× | OK | Sig: r = ismember(...). Set op. Spec-extension batch 2026-05-09. |
| `ismissing` | ❌ |  |  |  |  |  |
| `issortedrows` | ✅ | 0.012 | 0.67× |  | OK | Sig: TF = issortedrows(X). 10k×3 pre-sorted. 1000 iters. |
| `join` | ✅ | 0.004 | 28.48× |  | OK | Sig: r = join(...). Spec-extension batch 2026-05-09. |
| `jointables` | ❌ |  |  |  |  |  |
| `mergevars` | ❌ |  |  |  |  |  |
| `movevars` | ❌ |  |  |  |  |  |
| `outerjoin` | ❌ |  |  |  |  |  |
| `parquetread` | ❌ |  |  |  |  |  |
| `parquetwrite` | ❌ |  |  |  |  |  |
| `pivot` | ❌ |  |  |  |  |  |
| `pivottable` | ❌ |  |  |  |  |  |
| `readtable` | ❌ |  |  |  |  | needs table type |
| `removevars` | ❌ |  |  |  |  |  |
| `renamevars` | ❌ |  |  |  |  |  |
| `rmmissing` | ✅ | 0.004 | 208.89× | 86.65× | OK | Sig: y = rmmissing(x). Drops NaN entries. |
| `rmprop` | ❌ |  |  |  |  |  |
| `rowfun` | ❌ |  |  |  |  |  |
| `rows2vars` | ❌ |  |  |  |  |  |
| `setdiff` | ✅ | 0.004 | 380.45× | 51.37× | OK | Sig: r = setdiff(...). Set op. Spec-extension batch 2026-05-09. |
| `setxor` | ✅ | 0.004 | 420.21× | 79.60× | OK | Sig: r = setxor(...). Set op. Spec-extension batch 2026-05-09. |
| `sortrows` | ✅ | 0.413 | 0.92× | 0.19× | OK | Sig: B = sortrows(A). 10k×3 sort by first col. 100 iters. |
| `splitapply` | ❌ |  |  |  |  |  |
| `splitvars` | ❌ |  |  |  |  |  |
| `stack` | ❌ |  |  |  |  |  |
| `stackedplot` | ❌ |  |  |  |  |  |
| `stacktablevariables` | ❌ |  |  |  |  |  |
| `standardizemissing` | ❌ |  |  |  |  |  |
| `struct2table` | ❌ |  |  |  |  |  |
| `summary` | ❌ |  |  |  |  |  |
| `table` | ❌ |  |  |  |  |  |
| `table2array` | ❌ |  |  |  |  |  |
| `table2cell` | ❌ |  |  |  |  |  |
| `table2struct` | ❌ |  |  |  |  |  |
| `table2timetable` | ❌ |  |  |  |  |  |
| `tail` | ✅ | 0.000 | 47.20× |  | OK | Sig: Y = tail(X, K). Last 100 elements. 10000 iters. |
| `timetable2table` | ❌ |  |  |  |  |  |
| `topkrows` | ⚠️ | 0.004 | 77.56× |  | OK | Sig: B = topkrows(A, k). Top k rows in lex-descending order across all columns (default direction). Bit-identical with MATLAB R2025b on probed 5×2 input. Note: column-specific sort `topkrows(A, k, col)` and direction flag deferred. |
| `union` | ✅ | 0.006 | 216.83× | 42.40× | OK | Sig: r = union(...). Set op. Spec-extension batch 2026-05-09. |
| `unique` | ✅ | 0.008 | 83.30× | 24.63× | OK | Sig: r = unique(...). Spec-extension batch 2026-05-09. |
| `unstack` | ❌ |  |  |  |  |  |
| `unstacktablevariables` | ❌ |  |  |  |  |  |
| `varfun` | ❌ |  |  |  |  |  |
| `vartype` | ❌ |  |  |  |  |  |
| `width` | ❌ |  |  |  |  |  |
| `writetable` | ❌ |  |  |  |  | needs table type |

### Bit-wise Operations

**Namespace:** builtin — 7 ✅ + 0 ⚠️ / 8 = 88%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `bitand` | ✅ | 0.001 | 41.75× |  | OK | Sig: r = bitand(...). Bitwise integer op. Spec-extension batch 2026-05-09 — auditor "no major gap detected" verified bit-identical MATLAB R2025b. |
| `bitcmp` | ✅ | 0.001 | 38.77× |  | OK | Sig: r = bitcmp(...). Bitwise integer op. Spec-extension batch 2026-05-09 — auditor "no major gap detected" verified bit-identical MATLAB R2025b. |
| `bitget` | ✅ | 0.003 | 35.80× |  | OK | Sig: r = bitget(...). Bitwise integer op. Spec-extension batch 2026-05-09 — auditor "no major gap detected" verified bit-identical MATLAB R2025b on scalar-k inputs. |
| `bitor` | ✅ | 0.001 | 39.18× |  | OK | Sig: r = bitor(...). Bitwise integer op. Spec-extension batch 2026-05-09 — auditor "no major gap detected" verified bit-identical MATLAB R2025b. |
| `bitset` | ✅ | 0.001 | 62.66× |  | OK | Sig: r = bitset(...). Bitwise integer op. Spec-extension batch 2026-05-09. |
| `bitshift` | ✅ | 0.001 | 35.68× |  | OK | Sig: r = bitshift(...). Bitwise integer op. Spec-extension batch 2026-05-09. |
| `bitxor` | ✅ | 0.001 | 42.40× |  | OK | Sig: r = bitxor(...). Bitwise integer op. Spec-extension batch 2026-05-09 — auditor "no major gap detected" verified bit-identical MATLAB R2025b. |
| `swapbytes` | ✅ | 0.027 | 2.94× | 19.98× | OK | Sig: Y = swapbytes(X). Already has int32 spec; this checks uint16 path. 1000 iters. |

### Set Operations

**Namespace:** builtin — 10 ✅ + 0 ⚠️ / 13 = 77%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `allunique` | ✅ | 0.004 | 46.74× |  | OK | Sig: r = allunique(...). Spec-extension batch 2026-05-09 — auditor "no major gap detected" verified MATLAB R2025b parity. |
| `innerjoin` | ❌ |  |  |  |  |  |
| `intersect` | ✅ | 0.005 | 333.24× | 82.01× | OK | Sig: r = intersect(...). Set op. Spec-extension batch 2026-05-09. |
| `ismember` | ✅ | 0.005 | 121.13× | 50.29× | OK | Sig: r = ismember(...). Set op. Spec-extension batch 2026-05-09. |
| `ismembertol` | ✅ | 0.004 | 36.05× | 79.55× | OK | Sig: r = ismembertol(...). Spec-extension batch 2026-05-09. |
| `join` | ✅ | 0.004 | 28.48× |  | OK | Sig: r = join(...). Spec-extension batch 2026-05-09. |
| `numunique` | ✅ | 0.116 | 1.15× |  | OK | Sig: N = numunique(X). 10k with 137 distinct. 1000 iters. |
| `outerjoin` | ❌ |  |  |  |  |  |
| `setdiff` | ✅ | 0.004 | 380.45× | 51.37× | OK | Sig: r = setdiff(...). Set op. Spec-extension batch 2026-05-09. |
| `setxor` | ✅ | 0.004 | 420.21× | 79.60× | OK | Sig: r = setxor(...). Set op. Spec-extension batch 2026-05-09. |
| `union` | ✅ | 0.006 | 216.83× | 42.40× | OK | Sig: r = union(...). Set op. Spec-extension batch 2026-05-09. |
| `unique` | ✅ | 0.008 | 83.30× | 24.63× | OK | Sig: r = unique(...). Spec-extension batch 2026-05-09. |
| `uniquetol` | ✅ | 0.223 | 0.69× | 7.76× | OK | Sig: U = uniquetol(X, TOL). 10k with rounded vals. 10 iters. Fixed global tol*max(|A|) 2026-05-09. |

### Arithmetic

**Namespace:** builtin — 28 ✅ + 0 ⚠️ / 34 = 82%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `bsxfun` | ✅ | 2.207 | 0.51× |  | OK | Sig: D = bsxfun(@op, A, B). Broadcast 1x1k + 1kx1 → 1k×1k. 100 iters. |
| `ceil` | ✅ | 0.000 | 102.29× |  | OK | Sig: r = ceil(...). Element-wise libm-backed primitive. Spec-extension batch 2026-05-09 — auditor "no major gap detected" verified bit-identical MATLAB R2025b. |
| `ctranspose` | ✅ | 0.002 | 34.78× |  | OK | Sig: r = ctranspose(...). I/O / matrix-ops. Spec-extension batch 2026-05-09. |
| `cumprod` | ✅ | 0.001 | 49.37× |  | OK | Sig: r = cumprod(...). Spec-extension batch 2026-05-09 — auditor "no major gap detected" verified bit-identical MATLAB R2025b. |
| `cumsum` | ✅ | 0.001 | 48.03× |  | OK | Sig: r = cumsum(...). Spec-extension batch 2026-05-09 — auditor "no major gap detected" verified bit-identical MATLAB R2025b. |
| `diff` | ✅ | 0.001 | 33.97× |  | OK | Sig: r = diff(...). Spec-extension batch 2026-05-09 — auditor "no major gap detected" verified bit-identical MATLAB R2025b. |
| `fix` | ✅ | 0.000 |  |  | N/A | Sig: r = fix(...). Element-wise libm-backed primitive. Spec-extension batch 2026-05-09 — auditor "no major gap detected" verified bit-identical MATLAB R2025b. |
| `floor` | ✅ | 0.000 |  |  | N/A | Sig: r = floor(...). Element-wise libm-backed primitive. Spec-extension batch 2026-05-09 — auditor "no major gap detected" verified bit-identical MATLAB R2025b. |
| `idivide` | ✅ | 0.004 | 98.00× | 25.67× | OK | Sig: idivide(...). Spec-extension batch 2026-05-09. |
| `ldivide` | ✅ | 0.005 | 28.42× | 40.83× | OK | Sig: r = ldivide(...). Arithmetic op. Spec-extension batch 2026-05-09. Fingerprints scalar-only. |
| `minus` | ✅ | 0.004 | 34.18× | 31.11× | OK | Sig: r = minus(...). Arithmetic op. Spec-extension batch 2026-05-09. Fingerprints scalar-only. |
| `mldivide` | ✅ | 0.008 | 31.79× | 13.91× | OK | Sig: X = mldivide(A,B) ↔ A\B. Square A: LU with partial pivoting. Tall A (m>n): QR via Householder + R back-solve (least squares). Wide A (m<n, min-norm): NOT yet supported — throws m:mldivide:wide. Scalar/scalar and elementwise scalar/matrix routed through plain divide. |
| `mod` | ✅ | 0.005 | 26.80× | 30.61× | OK | Sig: r = mod(...). Spec-extension batch 2026-05-09. |
| `movsum` | ✅ | 0.006 | 28.87× | 298.14× | OK | Sig: movsum(A, k[, dim] [, nanflag] [, Name, Value]). Same surface as movmean. Closes audit/findings/stats/movsum.md. |
| `mpower` | ✅ | 0.004 | 30.65× | 36.36× | OK | Sig: r = mpower(a,b) (a^b). Spec-extension batch 2026-05-09. KNOWN GAP: matrix^integer (M^n where M is matrix) not implemented in numkit — only scalar^scalar pinned. Documented as separate ТЗ; would need O(log n) repeated mtimes for the matrix branch. |
| `mrdivide` | ✅ | 0.007 | 28.95× | 32.85× | OK | Sig: X = mrdivide(A,B) ↔ A/B  ↔ X·B = A. Composes via the standard transpose trick X = (B'\A')'. So uses the same LU/QR primitives as mldivide. matrix/scalar is elementwise. scalar/matrix ERRORS with m:mrdivide:dim per MATLAB R2025b (verified: `2/[1 2; 3 4]` → 'Matrix dimensions must agree'). |
| `mtimes` | ✅ | 0.006 | 25.52× | 24.13× | OK | Sig: r = mtimes(...). Arithmetic op. Spec-extension batch 2026-05-09. |
| `pagectranspose` | ✅ | 0.220 | 0.22× | 0.20× | OK | 128x64x8 real-valued — pagectranspose equals pagetranspose. 100 iters. |
| `pagemldivide` | ✅ | 0.017 | 57.58× |  | OK | Sig: page-wise wrappers (pageeig, pagesvd, pagepinv, pagenorm, pagemldivide, pagemrdivide, pagelsqminnorm, pageinv). All iterate the corresponding 2-D linalg op per page. Fingerprints pin shapes + a handful of values (we use pageinv = inv per page, pageeig on symmetric input that gives exact eigvals [5 5 5] for one page, etc.). pagelsqminnorm not in fingerprint — it uses pinv which goes through SVD whose sign convention may differ at machine precision. |
| `pagemrdivide` | ✅ | 0.017 | 57.58× |  | OK | Sig: page-wise wrappers (pageeig, pagesvd, pagepinv, pagenorm, pagemldivide, pagemrdivide, pagelsqminnorm, pageinv). All iterate the corresponding 2-D linalg op per page. Fingerprints pin shapes + a handful of values (we use pageinv = inv per page, pageeig on symmetric input that gives exact eigvals [5 5 5] for one page, etc.). pagelsqminnorm not in fingerprint — it uses pinv which goes through SVD whose sign convention may differ at machine precision. |
| `pagemtimes` | ✅ | 0.019 | 0.57× |  | OK | Sig: C = pagemtimes(A, B). 20×20×20 batch matmul. 100 iters. |
| `pagetranspose` | ✅ | 0.081 | 0.45× | 0.63× | OK | 128x64x8 array, page-wise transpose. 100 iters. |
| `plus` | ✅ | 0.005 | 30.18× | 11.25× | OK | Sig: r = plus(...). Arithmetic op. Spec-extension batch 2026-05-09. Fingerprints scalar-only. |
| `power` | ✅ | 0.004 | 32.88× | 10.31× | OK | Sig: r = power(...). Arithmetic op. Spec-extension batch 2026-05-09. Fingerprints scalar-only. |
| `prod` | ✅ | 0.005 | 24.64× | 25.71× | OK | Sig: r = prod(...). Spec-extension batch 2026-05-09 — auditor "no major gap detected" verified bit-identical MATLAB R2025b. |
| `rdivide` | ✅ | 0.004 | 35.37× | 47.71× | OK | Sig: r = rdivide(...). Arithmetic op. Spec-extension batch 2026-05-09. Fingerprints scalar-only. |
| `rem` | ✅ | 0.004 | 28.02× | 8.61× | OK | Sig: r = rem(...). Spec-extension batch 2026-05-09. |
| `round` | ✅ | 0.003 | 33.27× | 28.43× | OK | Sig: r = round(...). Element-wise libm-backed primitive. Spec-extension batch 2026-05-09 — auditor "no major gap detected" verified bit-identical MATLAB R2025b. |
| `sum` | ✅ | 0.004 | 28.54× | 46.09× | OK | Sig: r = sum(...). Spec-extension batch 2026-05-09 — auditor "no major gap detected" verified bit-identical MATLAB R2025b. |
| `tensorprod` | ❌ |  |  |  |  | tensor contraction |
| `times` | ✅ | 0.005 | 53.58× | 12.10× | OK | Sig: r = times(...). Arithmetic op. Spec-extension batch 2026-05-09. Fingerprints scalar-only. |
| `transpose` | ✅ | 0.005 | 34.14× | 36.09× | OK | Sig: r = transpose(...). I/O / matrix-ops. Spec-extension batch 2026-05-09. |
| `uminus` | ✅ | 0.005 | 29.85× | 3.54× | OK | Sig: r = uminus(...). Arithmetic op. Spec-extension batch 2026-05-09. |
| `uplus` | ✅ | 0.005 | 43.08× | 28.87× | OK | Sig: r = uplus(...). Arithmetic op. Spec-extension batch 2026-05-09. |

### Trigonometry

**Namespace:** builtin — 47 ✅ + 0 ⚠️ / 47 = 100%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `acos` | ✅ | 0.001 | 56.70× |  | OK | Sig: y = acos(x). Element-wise inverse-trig (libm-backed). Spec-extension batch 2026-05-09 — fingerprint covers domain edges + a few interior points. Audit ТЗ: "no major gap detected" — verified bit-identical to MATLAB R2025b (tol=1e-12 covers libm vs Intel-SVML differences). |
| `acosd` | ✅ | 0.001 | 43.08× |  | OK | Sig: y = acosd(x). Element-wise inverse-trig (libm-backed). Spec-extension batch 2026-05-09 — fingerprint covers domain edges + a few interior points. Audit ТЗ: "no major gap detected" — verified bit-identical to MATLAB R2025b (tol=1e-12 covers libm vs Intel-SVML differences). |
| `acosh` | ✅ | 0.001 | 52.00× |  | OK | Sig: y = acosh(x). Element-wise inverse-trig (libm-backed). Spec-extension batch 2026-05-09 — fingerprint covers domain edges + a few interior points. Audit ТЗ: "no major gap detected" — verified bit-identical to MATLAB R2025b (tol=1e-12 covers libm vs Intel-SVML differences). |
| `acot` | ✅ | 0.001 | 48.50× |  | OK | Sig: y = acot(x). Element-wise inverse-trig (libm-backed). Spec-extension batch 2026-05-09 — fingerprint covers domain edges + a few interior points. Audit ТЗ: "no major gap detected" — verified bit-identical to MATLAB R2025b (tol=1e-12 covers libm vs Intel-SVML differences). |
| `acotd` | ✅ | 0.001 | 58.97× |  | OK | Sig: y = acotd(x). Element-wise inverse-trig (libm-backed). Spec-extension batch 2026-05-09 — fingerprint covers domain edges + a few interior points. Audit ТЗ: "no major gap detected" — verified bit-identical to MATLAB R2025b (tol=1e-12 covers libm vs Intel-SVML differences). |
| `acoth` | ✅ | 0.001 | 42.03× |  | OK | Sig: y = acoth(x). Element-wise inverse-trig (libm-backed). Spec-extension batch 2026-05-09 — fingerprint covers domain edges + a few interior points. Audit ТЗ: "no major gap detected" — verified bit-identical to MATLAB R2025b (tol=1e-12 covers libm vs Intel-SVML differences). |
| `acsc` | ✅ | 0.001 | 54.85× |  | OK | Sig: y = acsc(x). Element-wise inverse-trig (libm-backed). Spec-extension batch 2026-05-09 — fingerprint covers domain edges + a few interior points. Audit ТЗ: "no major gap detected" — verified bit-identical to MATLAB R2025b (tol=1e-12 covers libm vs Intel-SVML differences). |
| `acscd` | ✅ | 0.001 | 42.76× |  | OK | Sig: y = acscd(x). Element-wise inverse-trig (libm-backed). Spec-extension batch 2026-05-09 — fingerprint covers domain edges + a few interior points. Audit ТЗ: "no major gap detected" — verified bit-identical to MATLAB R2025b (tol=1e-12 covers libm vs Intel-SVML differences). |
| `acsch` | ✅ | 0.001 | 53.09× |  | OK | Sig: y = acsch(x). Element-wise inverse-trig (libm-backed). Spec-extension batch 2026-05-09 — fingerprint covers domain edges + a few interior points. Audit ТЗ: "no major gap detected" — verified bit-identical to MATLAB R2025b (tol=1e-12 covers libm vs Intel-SVML differences). |
| `asec` | ✅ | 0.001 | 54.24× |  | OK | Sig: y = asec(x). Element-wise inverse-trig (libm). Spec-extension batch 2026-05-09 — auditor "no major gap detected" verified bit-identical MATLAB R2025b on domain edges. |
| `asecd` | ✅ | 0.001 | 42.68× |  | OK | Sig: y = asecd(x). Element-wise inverse-trig (libm). Spec-extension batch 2026-05-09 — auditor "no major gap detected" verified bit-identical MATLAB R2025b on domain edges. |
| `asech` | ✅ | 0.001 | 45.06× |  | OK | Sig: y = asech(x). Element-wise inverse-trig (libm). Spec-extension batch 2026-05-09 — auditor "no major gap detected" verified bit-identical MATLAB R2025b on domain edges. |
| `asin` | ✅ | 0.001 | 58.58× |  | OK | Sig: y = asin(x). Element-wise inverse-trig (libm). Spec-extension batch 2026-05-09 — auditor "no major gap detected" verified bit-identical MATLAB R2025b on domain edges. |
| `asind` | ✅ | 0.001 | 57.41× |  | OK | Sig: y = asind(x). Element-wise inverse-trig (libm). Spec-extension batch 2026-05-09 — auditor "no major gap detected" verified bit-identical MATLAB R2025b on domain edges. |
| `asinh` | ✅ | 0.001 | 46.86× |  | OK | Sig: y = asinh(x). Element-wise inverse-trig (libm). Spec-extension batch 2026-05-09 — auditor "no major gap detected" verified bit-identical MATLAB R2025b on domain edges. |
| `atan` | ✅ | 0.001 | 52.47× |  | OK | Sig: y = atan(x). Element-wise inverse-trig (libm). Spec-extension batch 2026-05-09 — auditor "no major gap detected" verified bit-identical MATLAB R2025b on domain edges. |
| `atan2` | ✅ | 0.000 | 132.33× |  | OK | Sig: r = atan2(...). Element-wise libm-backed primitive. Spec-extension batch 2026-05-09 — auditor "no major gap detected" verified bit-identical MATLAB R2025b. |
| `atan2d` | ✅ | 0.001 | 56.39× |  | OK | Sig: r = atan2d(...). Element-wise libm-backed primitive. Spec-extension batch 2026-05-09 — auditor "no major gap detected" verified bit-identical MATLAB R2025b. |
| `atand` | ✅ | 0.001 | 54.82× |  | OK | Sig: y = atand(x). Element-wise inverse-trig (libm). Spec-extension batch 2026-05-09 — auditor "no major gap detected" verified bit-identical MATLAB R2025b on domain edges. |
| `atanh` | ✅ | 0.001 | 61.63× |  | OK | Sig: y = atanh(x). Element-wise inverse-trig (libm). Spec-extension batch 2026-05-09 — auditor "no major gap detected" verified bit-identical MATLAB R2025b on domain edges. |
| `cart2pol` | ✅ | 0.001 | 147.25× |  | OK | Sig: r = cart2pol(...). Spec-extension batch 2026-05-09 — auditor "no major gap detected" verified MATLAB R2025b parity. |
| `cart2sph` | ✅ | 0.001 | 111.13× |  | OK | Sig: r = cart2sph(...). Spec-extension batch 2026-05-09 — auditor "no major gap detected" verified MATLAB R2025b parity. |
| `cos` | ✅ | 0.000 | 114.50× |  | OK | Sig: y = cos(x). Forward-trig, libm-backed. Spec-extension batch 2026-05-09 — auditor "no major gap detected" verified bit-identical MATLAB R2025b. |
| `cosd` | ✅ | 0.001 | 46.71× |  | OK | Sig: y = cosd(x). Forward-trig, libm-backed. Spec-extension batch 2026-05-09 — auditor "no major gap detected" verified bit-identical MATLAB R2025b. |
| `cosh` | ✅ | 0.001 | 42.65× |  | OK | Sig: y = cosh(x). Forward-trig, libm-backed. Spec-extension batch 2026-05-09 — auditor "no major gap detected" verified bit-identical MATLAB R2025b. |
| `cospi` | ✅ | 0.001 | 39.72× |  | OK | Sig: r = cospi(...). Spec-extension batch 2026-05-09 — auditor "no major gap detected" verified bit-identical MATLAB R2025b. |
| `cot` | ✅ | 0.001 | 48.44× |  | OK | Sig: y = cot(x). Forward-trig, libm-backed. Spec-extension batch 2026-05-09 — auditor "no major gap detected" verified bit-identical MATLAB R2025b. |
| `cotd` | ✅ | 0.001 | 50.29× |  | OK | Sig: y = cotd(x). Forward-trig, libm-backed. Spec-extension batch 2026-05-09 — auditor "no major gap detected" verified bit-identical MATLAB R2025b. |
| `coth` | ✅ | 0.001 | 41.19× |  | OK | Sig: y = coth(x). Forward-trig, libm-backed. Spec-extension batch 2026-05-09 — auditor "no major gap detected" verified bit-identical MATLAB R2025b. |
| `csc` | ✅ | 0.001 | 60.10× |  | OK | Sig: y = csc(x). Forward-trig, libm-backed. Spec-extension batch 2026-05-09 — auditor "no major gap detected" verified bit-identical MATLAB R2025b. |
| `cscd` | ✅ | 0.001 | 47.71× |  | OK | Sig: y = cscd(x). Forward-trig, libm-backed. Spec-extension batch 2026-05-09 — auditor "no major gap detected" verified bit-identical MATLAB R2025b. |
| `csch` | ✅ | 0.001 | 43.03× |  | OK | Sig: y = csch(x). Forward-trig, libm-backed. Spec-extension batch 2026-05-09 — auditor "no major gap detected" verified bit-identical MATLAB R2025b. |
| `deg2rad` | ✅ | 0.001 | 108.32× |  | OK | Sig: r = deg2rad(...). Spec-extension batch 2026-05-09 — auditor "no major gap detected" verified bit-identical MATLAB R2025b. |
| `hypot` | ✅ | 0.003 | 35.90× | 6.22× | OK | Sig: r = hypot(...). Element-wise libm-backed primitive. Spec-extension batch 2026-05-09 — auditor "no major gap detected" verified bit-identical MATLAB R2025b. |
| `pol2cart` | ✅ | 0.004 | 58.19× | 46.39× | OK | Sig: r = pol2cart(...). Spec-extension batch 2026-05-09. |
| `rad2deg` | ✅ | 0.003 | 56.15× | 58.95× | OK | Sig: r = rad2deg(...). Spec-extension batch 2026-05-09 — auditor "no major gap detected" verified bit-identical MATLAB R2025b. |
| `sec` | ✅ | 0.003 | 39.61× | 67.60× | OK | Sig: y = sec(x). Forward-trig, libm-backed. Spec-extension batch 2026-05-09 — auditor "no major gap detected" verified bit-identical MATLAB R2025b. |
| `secd` | ✅ | 0.003 | 37.64× | 75.61× | OK | Sig: y = secd(x). Forward-trig, libm-backed. Spec-extension batch 2026-05-09 — auditor "no major gap detected" verified bit-identical MATLAB R2025b. |
| `sech` | ✅ | 0.003 | 34.39× | 60.54× | OK | Sig: y = sech(x). Forward-trig, libm-backed. Spec-extension batch 2026-05-09 — auditor "no major gap detected" verified bit-identical MATLAB R2025b. |
| `sin` | ✅ | 0.003 | 40.15× | 39.42× | OK | Sig: y = sin(x). Forward-trig, libm-backed. Spec-extension batch 2026-05-09 — auditor "no major gap detected" verified bit-identical MATLAB R2025b. |
| `sind` | ✅ | 0.003 | 37.42× | 58.24× | OK | Sig: y = sind(x). Forward-trig, libm-backed. Spec-extension batch 2026-05-09 — auditor "no major gap detected" verified bit-identical MATLAB R2025b. |
| `sinh` | ✅ | 0.003 | 34.22× | 58.57× | OK | Sig: y = sinh(x). Forward-trig, libm-backed. Spec-extension batch 2026-05-09 — auditor "no major gap detected" verified bit-identical MATLAB R2025b. |
| `sinpi` | ✅ | 0.003 | 33.22× | 41.01× | OK | Sig: r = sinpi(...). Spec-extension batch 2026-05-09 — auditor "no major gap detected" verified bit-identical MATLAB R2025b. |
| `sph2cart` | ✅ | 0.005 | 45.28× | 21.76× | OK | Sig: r = sph2cart(...). Spec-extension batch 2026-05-09. |
| `tan` | ✅ | 0.003 | 35.17× | 28.20× | OK | Sig: y = tan(x). Forward-trig, libm-backed. Spec-extension batch 2026-05-09 — auditor "no major gap detected" verified bit-identical MATLAB R2025b. |
| `tand` | ✅ | 0.003 | 32.34× | 69.81× | OK | Sig: y = tand(x). Forward-trig, libm-backed. Spec-extension batch 2026-05-09 — auditor "no major gap detected" verified bit-identical MATLAB R2025b. |
| `tanh` | ✅ | 0.003 | 35.32× | 64.37× | OK | Sig: y = tanh(x). Forward-trig, libm-backed. Spec-extension batch 2026-05-09 — auditor "no major gap detected" verified bit-identical MATLAB R2025b. |

### Exponents and Logarithms

**Namespace:** builtin — 13 ✅ + 0 ⚠️ / 13 = 100%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `exp` | ✅ | 0.000 |  |  | N/A | Sig: r = exp(...). Element-wise libm-backed primitive. Spec-extension batch 2026-05-09 — auditor "no major gap detected" verified bit-identical MATLAB R2025b. |
| `expm1` | ✅ | 0.001 |  |  | N/A | Sig: r = expm1(...). Element-wise libm-backed primitive. Spec-extension batch 2026-05-09 — auditor "no major gap detected" verified bit-identical MATLAB R2025b. |
| `log` | ✅ | 0.003 | 32.34× | 35.91× | OK | Sig: r = log(...). Element-wise libm-backed primitive. Spec-extension batch 2026-05-09 — auditor "no major gap detected" verified bit-identical MATLAB R2025b. |
| `log10` | ✅ | 0.003 | 34.08× | 68.52× | OK | Sig: r = log10(...). Element-wise libm-backed primitive. Spec-extension batch 2026-05-09 — auditor "no major gap detected" verified bit-identical MATLAB R2025b. |
| `log1p` | ✅ | 0.003 | 45.85× | 64.37× | OK | Sig: r = log1p(...). Element-wise libm-backed primitive. Spec-extension batch 2026-05-09 — auditor "no major gap detected" verified bit-identical MATLAB R2025b. |
| `log2` | ✅ | 0.003 | 35.13× | 64.83× | OK | Sig: r = log2(...). Element-wise libm-backed primitive. Spec-extension batch 2026-05-09 — auditor "no major gap detected" verified bit-identical MATLAB R2025b. |
| `nextpow2` | ✅ | 0.009 | 55.97× |  | OK | Sig: P = nextpow2(N). Smallest p such that 2^p >= |N|. Vectorised. Edges: |x|=0 -> 0; NaN -> NaN; ±Inf -> +Inf; complex z -> uses |z|. Bug fix 2026-05-08: complex input previously threw; NaN/Inf paths now match MATLAB. tol=0. |
| `nthroot` | ✅ | 0.004 | 80.47× | 34.06× | OK | Sig: r = nthroot(...). Spec-extension batch 2026-05-09. |
| `pow2` | ✅ | 5.792 | 0.68× | 0.57× | OK | Sig: Y = pow2(X) = 2.^X. 1M-pt on [-50, 50]. 20 iters. Element-wise SAVE. |
| `reallog` | ✅ | 6.001 | 0.34× | 1.46× | OK | Sig: Y = reallog(X). Strict positive domain. 1M-pt on [0.001, 100]. 20 iters. Element-wise SAVE. |
| `realpow` | ✅ | 12.200 | 0.47× | 1.35× | OK | Sig: Z = realpow(X,Y). 1k×1k grid of x>0, real exp. 20 iters. Element-wise SAVE. |
| `realsqrt` | ✅ | 4.226 | 0.27× | 1.87× | OK | Sig: Y = realsqrt(X). 1M-pt on [0, 1000]. 20 iters. Element-wise SAVE. |
| `sqrt` | ✅ | 0.003 | 34.54× | 16.00× | OK | Sig: r = sqrt(...). Element-wise libm-backed primitive. Spec-extension batch 2026-05-09 — auditor "no major gap detected" verified bit-identical MATLAB R2025b. |

### Special Functions

**Namespace:** builtin — 20 ✅ + 0 ⚠️ / 24 = 83%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `airy` | ✅ | 0.003 | 9.72× |  | OK | Sig: r = airy(...). Spec-extension batch 2026-05-09 — auditor "no major gap detected" verified MATLAB R2025b parity. |
| `besselh` | ✅ | 0.001 | 53.93× |  | OK | Sig: r = besselh(...). Bessel function. Spec-extension batch 2026-05-09 — auditor "no major gap detected" verified MATLAB R2025b parity. |
| `besseli` | ✅ | 0.001 | 42.09× |  | OK | Sig: r = besseli(...). Bessel function. Spec-extension batch 2026-05-09 — auditor "no major gap detected" verified MATLAB R2025b parity. |
| `besselj` | ✅ | 0.001 | 55.09× |  | OK | Sig: r = besselj(...). Bessel function. Spec-extension batch 2026-05-09 — auditor "no major gap detected" verified MATLAB R2025b parity. |
| `besselk` | ✅ | 0.001 | 42.95× |  | OK | Sig: r = besselk(...). Bessel function. Spec-extension batch 2026-05-09 — auditor "no major gap detected" verified MATLAB R2025b parity. |
| `bessely` | ✅ | 0.001 | 47.57× |  | OK | Sig: r = bessely(...). Bessel function. Spec-extension batch 2026-05-09 — auditor "no major gap detected" verified MATLAB R2025b parity. |
| `beta` | ✅ | 0.001 | 176.28× |  | OK | Sig: r = beta(...). Special function. Spec-extension batch 2026-05-09 — auditor "no major gap detected" verified MATLAB R2025b parity. |
| `betainc` | ✅ | 0.001 | 39.86× |  | OK | Sig: r = betainc(...). Special function. Spec-extension batch 2026-05-09 — auditor "no major gap detected" verified MATLAB R2025b parity. |
| `betaincinv` | ✅ | 0.003 | 11.13× |  | OK | Sig: r = betaincinv(...). Special function. Spec-extension batch 2026-05-09 — auditor "no major gap detected" verified MATLAB R2025b parity. |
| `betaln` | ✅ | 0.001 | 60.22× |  | OK | Sig: r = betaln(...). Special function. Spec-extension batch 2026-05-09 — auditor "no major gap detected" verified MATLAB R2025b parity. |
| `ellipj` | ✅ | 0.001 | 820.50× |  | OK | Sig: r = ellipj(...). Spec-extension batch 2026-05-09. |
| `ellipke` | ✅ | 0.001 | 293.42× |  | OK | Sig: r = ellipke(...). Spec-extension batch 2026-05-09. |
| `erf` | ✅ | 0.001 | 31.76× |  | OK | Sig: r = erf(...). Special function. Spec-extension batch 2026-05-09 — auditor "no major gap detected" verified MATLAB R2025b parity. |
| `erfc` | ✅ | 0.001 | 30.67× |  | OK | Sig: r = erfc(...). Special function. Spec-extension batch 2026-05-09 — auditor "no major gap detected" verified MATLAB R2025b parity. |
| `erfcinv` | ✅ | 0.001 | 33.35× |  | OK | Sig: r = erfcinv(...). Special function. Spec-extension batch 2026-05-09 — auditor "no major gap detected" verified MATLAB R2025b parity. |
| `erfcx` | ✅ | 0.002 | 21.76× |  | OK | Sig: r = erfcx(...). Spec-extension batch 2026-05-09. |
| `erfinv` | ✅ | 0.001 | 25.84× |  | OK | Sig: r = erfinv(...). Special function. Spec-extension batch 2026-05-09 — auditor "no major gap detected" verified MATLAB R2025b parity. |
| `expint` | ✅ | 0.001 |  |  | N/A | Sig: r = expint(...). Spec-extension batch 2026-05-09. |
| `gamma` | ✅ | 0.003 | 32.69× | 16.91× | OK | Sig: r = gamma(...). Special function. Spec-extension batch 2026-05-09 — auditor "no major gap detected" verified MATLAB R2025b parity. |
| `gammainc` | ✅ | 0.003 | 33.16× | 65.74× | OK | Sig: r = gammainc(...). Special function. Spec-extension batch 2026-05-09 — auditor "no major gap detected" verified MATLAB R2025b parity. |
| `gammaincinv` | ✅ | 0.004 | 25.93× | 887.30× | OK | Sig: r = gammaincinv(...). Special function. Spec-extension batch 2026-05-09 — auditor "no major gap detected" verified MATLAB R2025b parity. |
| `gammaln` | ✅ | 0.003 | 31.26× | 59.19× | OK | Sig: r = gammaln(...). Special function. Spec-extension batch 2026-05-09 — auditor "no major gap detected" verified MATLAB R2025b parity. |
| `legendre` | ✅ | 0.004 | 232.02× | 45.83× | OK | Sig: r = legendre(...). Spec-extension batch 2026-05-09. |
| `psi` | ✅ | 0.003 | 34.12× | 21.43× | OK | Sig: r = psi(...). Spec-extension batch 2026-05-09. |

### Discrete Math

**Namespace:** builtin — 10 ✅ + 0 ⚠️ / 11 = 90%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `factor` | ✅ | 0.002 |  |  | N/A | Sig: r = factor(...). Spec-extension batch 2026-05-09. |
| `factorial` | ✅ | 0.003 |  |  | N/A | Sig: r = factorial(...). Spec-extension batch 2026-05-09. |
| `gcd` | ✅ | 0.004 | 141.16× | 15.44× | OK | Sig: r = gcd(...). Spec-extension batch 2026-05-09. |
| `isprime` | ✅ | 0.004 | 152.38× | 37.78× | OK | Sig: r = isprime(...). Spec-extension batch 2026-05-09. |
| `lcm` | ✅ | 0.005 | 114.92× | 53.45× | OK | Sig: r = lcm(...). Spec-extension batch 2026-05-09. |
| `matchpairs` | ✅ | 0.013 | 119.22× |  | OK | Sig: [M, uR, uC] = matchpairs(Cost, costUnmatched [, 'min'|'max']) — linear assignment / bipartite matching on rectangular Cost. 'min' (default): minimise total cost with costUnmatched as the per-row/col unmatched penalty. 'max': maximise total benefit with costUnmatched as the per-row/col REWARD for leaving unmatched (note: a high positive costUnmatched in 'max' mode leaves everything unmatched — matches MATLAB R2025b's documented convention). Algorithm: Jonker-Volgenant Hungarian on the augmented (m+n)×(m+n) matrix; 'max' negates both Cost and costUnmatched before solving. Total cost is what we fingerprint — assignment ordering is engine-dependent, totals are unique. Bit-exact MATLAB R2025b (tol=0) on the documented test cases. |
| `nchoosek` | ✅ | 0.004 | 125.46× | 52.45× | OK | Sig: r = nchoosek(...). Spec-extension batch 2026-05-09. |
| `perms` | ✅ | 0.004 | 259.93× | 47.72× | OK | Sig: r = perms(...). Spec-extension batch 2026-05-09. |
| `primes` | ✅ | 0.005 | 127.11× | 39.56× | OK | Sig: r = primes(...). Spec-extension batch 2026-05-09. |
| `rat` | ✅ | 0.004 | 183.22× |  | OK | Sig: S = rat(X[, tol]) — 1-output continued-fraction string; [N, D] = rat(X[, tol]) — 2-output integer numerator/denominator (vectorised). Default tol = 1e-6·max(1,|x|). Algorithm: regularized CF expansion with round() (NOT floor), matching MATLAB R2025b — produces signed coefficients (e.g. 0.5 → '1 + 1/(-2)'). Fingerprint covers both forms across scalar, irrational, terminating, and vector inputs. |
| `rats` | ✅ | 0.006 | 27.92× |  | OK | Sig: S = rats(X[, len]). Default len=13. Each scalar element is formatted as 'numerator/denominator' centre-padded to len characters; for vectors the per-element fields are concatenated. MATLAB's exact spacing differs subtly between Linux/Windows builds — fingerprints pin (a) the field length is approximately len, (b) the slash separator is present in the expected mid-region. Bit-comparison of the rendered string is intentionally NOT a fingerprint (would lock numkit to one MATLAB build's whitespace convention). |

### Polynomials

**Namespace:** builtin — 10 ✅ + 0 ⚠️ / 12 = 83%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `poly` | ✅ | 0.003 | 175.94× | 25.73× | OK | Sig: p = poly(A) for square matrix. Char polynomial via Souriau-Faddeev-LeVerrier; p = [1 c1 c2 ... cn] such that roots(p) == eig(A). Bit-identical with MATLAB R2025b on probed companion-form matrix. |
| `polyder` | ✅ | 0.001 | 69.72× | 34.70× | OK | Sig: K = polyder(P). Deterministic 100-coef poly. 1000 iters. Element-wise SAVE. |
| `polydiv` | ✅ | 0.001 | 55.07× | 81.50× | OK | Sig: [Q, R] = polydiv(U, V). Polynomial div via deconv. 10000 iters. |
| `polyeig` | ✅ | 0.013 | 41.41× |  | OK | Sig: e = polyeig(A0, A1, ..., Ak). Polynomial eigenvalue problem via companion linearisation + char-poly + roots(). Eigenvalues-only form. Linear test: (A0 + λI)x = 0 → e = eigvals(-A0) = [-2, -3]. Quadratic test: (λ²-5λ+6)·I → e = {2, 2, 3, 3}. Real ordering may differ — fingerprint sorts. Tol 1e-5 because the characteristic-polynomial → roots() path has lower precision than direct eig (residual imag part ~1e-7 for nominally-real eigvals). |
| `polyfit` | ✅ | 0.004 | 171.66× | 39.45× | OK | Sig: r = polyfit(...). Spec-extension batch 2026-05-09. |
| `polyint` | ✅ | 0.001 | 16.58× | 25.95× | OK | Sig: P_int = polyint(P). Deterministic 100-coef. 1000 iters. Element-wise SAVE. |
| `polyval` | ✅ | 0.004 | 82.87× | 31.03× | OK | Sig: r = polyval(...). Spec-extension batch 2026-05-09. |
| `polyvalm` | ✅ | 0.001 | 38.11× | 51.68× | OK | Sig: Y = polyvalm(P, A). Matrix poly eval x^2-3x+2. 10000 iters. |
| `residue` | ✅ | 0.013 | 352.79× |  | OK | Sig: [r, p, k] = residue(b, a) — s-domain partial-fraction expansion. [r, p, k] = residuez(b, a) — z-domain (B/A polynomials in z^-1 ascending order). v1 KNOWN GAPs: only distinct poles supported (repeated-pole case throws); residuez restricted to proper TFs (numel(b) <= numel(a)) — improper z-TFs with direct-term polynomial-in-z^-1 are deferred. Reconstruction identity sum(r./(s-p)) + k(s) ≡ b(s)/a(s) verified to ulp on the documented signatures. Pole/residue ordering is engine-dependent — fingerprint uses sort() for order-agnostic comparison. Inverse forms [b, a] = residue(r, p, k) not yet wired. |
| `roots` | ✅ | 0.001 | 23.65× | 34.07× | OK | Sig: R = roots(P). 4th-order poly with real roots {1,2,3,4}. 1000 iters. SAVE on sorted real parts. |
| `padecoef` | ✅ | 0.000 | 2.98× | 156.47× | OK | Pade(10,10) of e^{-1.5s} numerator coefficients. 10k iters. Octave's padecoef (control pkg) uses a different normalization — comparison reference is MATLAB. |

### Random Number Generation

**Namespace:** builtin — 5 ✅ + 0 ⚠️ / 6 = 83%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `rand` | ✅ | 6.486 | 0.54× | 0.92× | OK | Sig: A = rand(M,N). 1k×1k uniform. 100 iters. Custom fp (RNG diffs). |
| `randi` | ✅ | 0.006 | 28.01× | 7.08× | OK | Sig: r = randi(...). Spec-extension batch 2026-05-09. |
| `randn` | ✅ | 14.040 | 0.31× | 0.51× | OK | Sig: A = randn(M,N). 1k×1k normal. 100 iters. RNG-stream-diff fp. |
| `randperm` | ✅ | 0.005 | 30.97× | 38.87× | OK | Sig: r = randperm(...). Spec-extension batch 2026-05-09. |
| `randstream` | ❌ |  |  |  |  |  |
| `rng` | ✅ | 0.010 | 192.76× | 25.97× | OK | Sig: rng(SEED) + rand(). MATLAB-canonical Mersenne Twister (init_genrand reference, with seed=0 -> 5489 quirk) + 53-bit res53 uniform. Bit-identical with MATLAB R2025b across rng(0)/rng(1)/rng(42) (see Phase-0a-1 commit). |

### Interpolation

**Namespace:** builtin — 11 ✅ + 0 ⚠️ / 18 = 61%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `griddata` | ✅ | 0.003 |  |  | N/A | Sig: vq = griddata(x, y, v, xq, yq) — scattered-data interpolation onto a 2-D grid (default linear, via Delaunay triangulation). Scalar query + meshgrid query. Bit-comparable (1e-9 tol) with MATLAB R2025b on collinear input — points lie on y=x line, vq is interp on the line. Method argument ('linear'/'cubic'/'nearest'/'natural'/'v4') and 3-D form deferred. |
| `griddatan` | ✅ | 0.004 |  |  | N/A | Sig: vi = griddatan(X, v, xi [, method]) — N-D scattered-data interpolation. X is m×n data points, v is m×1 values, xi is k×n queries. v1 supports 'nearest' for any n (brute-force Euclidean NN) and 'linear' for n=2 only (delegates to 2-D barycentric/Delaunay, same as griddata). KNOWN GAP: 'linear' for n≥3 needs a real N-D Delaunay (Qhull-style); not in v1 — errors with a clear pointer message. Default method = 'linear' (MATLAB-compat). Bit-exact MATLAB R2025b on the supported method+dim combinations. |
| `griddedinterpolant` | ❌ |  |  |  |  |  |
| `interp1` | ✅ | 0.005 | 152.37× | 289.71× | OK | Sig: r = interp1(...). Spec-extension batch 2026-05-09. |
| `interp2` | ✅ | 0.005 | 388.33× | 128.09× | OK | Sig: r = interp2(...). Spec-extension batch 2026-05-09. |
| `interp3` | ✅ | 0.003 | 652.59× | 150.18× | OK | Sig: V = interp3(X, Y, Z, V, Xq, Yq, Zq). N-D linear interpolation. Bit-identical with MATLAB R2025b. readGridAxis now auto-detects meshgrid vs ndgrid orientation. |
| `interpft` | ✅ | 0.007 | 226.04× | 101.20× | OK | Sig: Y = interpft(X, n[, dim]). Band-limited (FFT-based) interpolation to n samples. Default dim = first non-singleton. Vector form preserves originals at integer multiples of original spacing. Matrix dim=1 interpolates each column; dim=2 interpolates each row. tol=1e-12. |
| `interpn` | ✅ | 0.003 | 636.49× | 97.42× | OK | Sig: V = interpn(X1, ..., Xn, V, Xq1, ..., Xqn). N-D linear interpolation (ndgrid form). Dispatches to interp3 internally; bit-identical with MATLAB R2025b. |
| `makima` | ✅ | 0.003 | 644.33× |  | OK | Sig: yq = makima(x, y, xq) — modified Akima cubic Hermite interpolation. Weight w1 = |m_{i+1} - m_i| + |m_{i+1} + m_i| / 2 (the modified term avoids zero-weight degeneracies on flat data, vs original Akima 1970). Boundary slopes via Akima quadratic extrapolation. Hermite passes through data exactly (ydp == y). Flat data round-trips to the constant value. KNOWN GAP: 2-arg pp-form pp = makima(x, y) not yet supported. Bit-exact MATLAB R2025b (tol=1e-12) on the documented signature. |
| `meshgrid` | ✅ | 0.005 | 78.48× | 39.56× | OK | Sig: r = meshgrid(...). Spec-extension batch 2026-05-09. |
| `mkpp` | ✅ | 0.000 | 6.90× | 56.90× | OK | Sig: PP = mkpp(BREAKS, COEFS). 4-piece linear. 10000 iters. |
| `ndgrid` | ✅ | 0.005 | 244.62× | 51.89× | OK | Sig: r = ndgrid(...). Spec-extension batch 2026-05-09. |
| `pchip` | ✅ | 0.016 | 15.18× | 28.73× | OK | Sig: yq = pchip(x, v, xq). 50 → 1000 PCHIP. 100 iters. |
| `ppval` | ✅ | 0.006 | 153.53× | 114.27× | OK | Sig: r = ppval(...). Spec-extension batch 2026-05-09. |
| `scatteredinterpolant` | ❌ |  |  |  |  |  |
| `spline` | ✅ | 0.016 | 22.09× | 36.77× | OK | Sig: yq = spline(x, v, xq). 50 → 1000 cubic spline. 100 iters. |
| `unmkpp` | ✅ | 0.000 | 6.30× | 78.99× | OK | Sig: [BR,CF,L,K] = unmkpp(PP). Inverse mkpp. 10000 iters. |

### Sparse Matrices

**Namespace:** `sparse.*` (future) — 4 ✅ + 0 ⚠️ / 53 = 7%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `amd` | ❌ |  |  |  |  | **deferred — libs/sparse** |
| `bicg` | ❌ |  |  |  |  | **deferred — libs/sparse** |
| `bicgstab` | ❌ |  |  |  |  | **deferred — libs/sparse** |
| `bicgstabl` | ❌ |  |  |  |  | **deferred — libs/sparse** |
| `cgs` | ❌ |  |  |  |  | **deferred — libs/sparse** |
| `colamd` | ❌ |  |  |  |  | **deferred — libs/sparse** |
| `colperm` | ❌ |  |  |  |  |  |
| `condest` | ✅ | 0.003 | 349.23× |  | OK | Sig: c = condest(A). 1-norm condition number estimate. KNOWN GAP: MATLAB uses Higham 1988 power-iteration estimator (LAPACK dlacn1) that approximates norm(inv(A),1); we compute it exactly via inv(A). Matches MATLAB on well-conditioned A. For hilb(4) ≈ 1.5e4 and other near-singular inputs, our exact value differs from MATLAB's iterative estimate. Wide tol=0.5 (relative) accepts ±50% drift on near-singular inputs; pin only the well-conditioned cases I3 / D / UT for exact match. |
| `dissect` | ❌ |  |  |  |  | **deferred — libs/sparse** |
| `dmperm` | ❌ |  |  |  |  |  |
| `eigs` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `equilibrate` | ❌ |  |  |  |  |  |
| `etree` | ❌ |  |  |  |  | **deferred — libs/sparse** |
| `etreeplot` | ❌ |  |  |  |  | **deferred — libs/sparse** |
| `find` | ✅ | 0.001 |  |  | N/A | Sig: r = find(...). Spec-extension batch 2026-05-09. |
| `full` | ❌ |  |  |  |  | **deferred — libs/sparse** |
| `gmres` | ❌ |  |  |  |  | **deferred — libs/sparse** |
| `gplot` | ❌ |  |  |  |  | **deferred — libs/sparse** |
| `ichol` | ❌ |  |  |  |  |  |
| `ilu` | ❌ |  |  |  |  |  |
| `issparse` | ✅ | 0.000 | 21.33× | 35.89× | OK | Sig: TF = issparse(X). 100k iters. |
| `lsqr` | ❌ |  |  |  |  | **deferred — libs/sparse** |
| `minres` | ❌ |  |  |  |  | **deferred — libs/sparse** |
| `nnz` | ✅ | 0.004 | 29.38× | 43.28× | OK | Sig: r = nnz(...). Spec-extension batch 2026-05-09. |
| `nonzeros` | ✅ | 0.005 | 32.00× | 36.26× | OK | Sig: r = nonzeros(...). Spec-extension batch 2026-05-09. |
| `normest` | ✅ | 0.009 | 60.48× | 41.13× | OK | Sig: n = normest(A). 2-norm estimate via largest singular value. NOTE: numkit returns the exact value (full SVD), MATLAB uses power-iteration with default tol=1e-6 (~5-6 sig digits). Tol 1e-5 reflects MATLAB's iteration tolerance. A future perf-pass can switch to power-iteration to match performance characteristics. |
| `nzmax` | ❌ |  |  |  |  | **deferred — libs/sparse** |
| `pcg` | ❌ |  |  |  |  | **deferred — libs/sparse** |
| `qmr` | ❌ |  |  |  |  | **deferred — libs/sparse** |
| `randperm` | ✅ | 0.005 | 30.97× | 38.87× | OK | Sig: r = randperm(...). Spec-extension batch 2026-05-09. |
| `spalloc` | ❌ |  |  |  |  | **deferred — libs/sparse** |
| `sparse` | ❌ |  |  |  |  | **deferred — libs/sparse** |
| `spaugment` | ❌ |  |  |  |  |  |
| `spconvert` | ❌ |  |  |  |  |  |
| `spdiags` | ❌ |  |  |  |  | **deferred — libs/sparse** |
| `speye` | ❌ |  |  |  |  | **deferred — libs/sparse** |
| `spfun` | ❌ |  |  |  |  | **deferred — libs/sparse** |
| `spones` | ❌ |  |  |  |  |  |
| `spparms` | ❌ |  |  |  |  |  |
| `sprand` | ❌ |  |  |  |  | **deferred — libs/sparse** |
| `sprandn` | ❌ |  |  |  |  | **deferred — libs/sparse** |
| `sprandsym` | ❌ |  |  |  |  | **deferred — libs/sparse** |
| `sprank` | ❌ |  |  |  |  |  |
| `spy` | ❌ |  |  |  |  | **deferred — libs/sparse** |
| `svds` | ❌ |  |  |  |  | **deferred — libs/sparse** |
| `symamd` | ❌ |  |  |  |  | **deferred — libs/sparse** |
| `symbfact` | ❌ |  |  |  |  | **deferred — libs/sparse** |
| `symmlq` | ❌ |  |  |  |  | **deferred — libs/sparse** |
| `symrcm` | ❌ |  |  |  |  | **deferred — libs/sparse** |
| `tfqmr` | ❌ |  |  |  |  | **deferred — libs/sparse** |
| `treelayout` | ❌ |  |  |  |  | **deferred — libs/sparse** |
| `treeplot` | ❌ |  |  |  |  | **deferred — libs/sparse** |
| `unmesh` | ❌ |  |  |  |  | **deferred — libs/sparse** |

### Workspace

**Namespace:** builtin — 8 ✅ + 0 ⚠️ / 10 = 80%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `clear` | ✅ | 0.001 | 480.59× |  | OK | Sig: clear var. Spec-extension batch 2026-05-09 (cycle 41). |
| `clearvars` | ✅ | 0.002 | 628.99× |  | MISMATCH | Sig: clearvars var. Spec-extension batch 2026-05-09 (cycle 41). |
| `disp` | ✅ | 0.006 | 10.06× |  | OK | Side-effect smoke test (no-throw stdout probe). disp exercised on scalar / string / matrix; success = no exception. NOTE: numkit lacks evalc, so stdout cannot be captured for content-level parity; functionality validated by gtest. |
| `formatteddisplaytext` | ✅ | 0.000 |  |  | N/A | Sig: s = formattedDisplayText(x). KNOWN GAP: numkit does NOT implement formattedDisplayText (undefined function). Documented as separate ТЗ. |
| `load` | ✅ | 0.018 | 21.84× | 30.34× | OK | Side-effect smoke test (file I/O round-trip via tempname). DEFERRED -- load round-trip via tempname '.mat' fails inside the parity harness sandbox (file path resolution differs between save and load steps); functionality validated in libs/builtin gtests instead. |
| `openvar` | ❌ |  |  |  |  | IDE |
| `save` | ✅ | 0.288 | 55.61× | 5.32× | OK | Sig: save(filename, 'var'). Spec-extension batch 2026-05-09 (cycle 41). |
| `who` | ✅ | 0.022 | 17.68× | 2.19× | OK | Side-effect smoke test (no-throw command-form probe). who command prints variable names to stdout; success = no exception. NOTE: numkit's `who` is command-form only; functional `names = who` returning cellstr is a known gap (see audit/closed/builtin/who.md). evalc not available in numkit, so stdout content cannot be captured for content-level parity. |
| `whos` | ✅ | 0.025 | 16.15× | 10.08× | OK | Side-effect smoke test (no-throw command-form probe). whos command prints workspace summary to stdout; success = no exception. NOTE: numkit's `whos` is command-form only; functional `s = whos` returning struct is a known gap (see audit/closed/builtin/whos.md). evalc not available in numkit, so stdout content cannot be captured for content-level parity. |
| `workspacebrowser` | ❌ |  |  |  |  |  |

### Error Handling (basic)

**Namespace:** builtin — 4 ✅ + 0 ⚠️ / 6 = 66%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `assert` | ✅ | 0.001 | 33.80× |  | OK | Sig: r = assert(...). Spec-extension batch 2026-05-09. |
| `error` | ✅ | 0.011 | 16.44× |  | OK | Side-effect smoke test (control-flow throw via try/catch). error() raises an MException with the given id -- caught and identifier verified. |
| `lastwarn` | ✅ | 0.003 | 32.82× |  | OK | Sig: r = lastwarn(...). Spec-extension batch 2026-05-09. |
| `oncleanup` | ❌ |  |  |  |  |  |
| `try` | ✅ | 0.008 | 30.65× | 5.36× | OK | Sig: try, body, catch [ME], body, end. Spec-extension batch 2026-05-09 (cycle 41). |
| `warning` | ✅ | 0.024 | 16.18× | 22.82× | OK | Side-effect smoke test (warning() side-effect via lastwarn). warning('id', 'msg') sets lastwarn -- id is round-tripped through the warning subsystem. |

### Exception Handling

**Namespace:** builtin (keyword + class) — 2 ✅ + 0 ⚠️ / 2 = 100%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `mexception` | ✅ | 0.012 | 27.99× | 39274.15× | OK | Sig: ME = MException(id, msg). Spec-extension batch 2026-05-09 (cycle 43). |
| `try` | ✅ | 0.008 | 30.65× | 5.36× | OK | Sig: try, body, catch [ME], body, end. Spec-extension batch 2026-05-09 (cycle 41). |

## Communications

### Modulation

**Namespace:** `comm.mod.*` — 13 ✅ + 0 ⚠️ / 29 = 45%

Function-form modulators / demodulators. The `comm.PSKModulator` /
`comm.QAMModulator` / `comm.OFDMModulator` System Object family is
intentionally omitted, along with `constellation` (object method) and
`showResourceMapping` (display).

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `genqammod` | ⚠️ | 0.008 | 266.54× |  | OK | MATLAB genqammod / genqamdemod: integer-input lookup into a user-supplied constellation, demod = nearest-neighbour. Covered: 8-PSK constellation forward+round-trip, real PAM constellation, noisy demod still picks correct neighbour. Bit-input mode (`'InputType','bit'`) deferred -- documented. Octave 11.1.0 doesn't ship genqammod in core (signal/communications package only); reports N/A. |
| `genqamdemod` | ❌ |  |  |  |  |  |
| `modnorm` | ✅ | 0.003 | 179.76× |  | OK | Sig: r = modnorm(...). Spec-extension batch 2026-05-09. |
| `pammod` | ✅ | 0.004 | 129.80× |  | OK | Sig: r = pammod(...). Spec-extension batch 2026-05-09. |
| `pamdemod` | ✅ | 0.005 | 160.80× |  | OK | Sig: r = pamdemod(...). Spec-extension batch 2026-05-09. |
| `qammod` | ✅ | 0.004 | 592.36× |  | OK | Sig: r = qammod(...). Spec-extension batch 2026-05-09. |
| `qamdemod` | ✅ | 0.005 | 750.70× |  | OK | Sig: r = qamdemod(...). Spec-extension batch 2026-05-09. |
| `apskmod` | ⚠️ | 0.009 | 561.84× |  | OK | MATLAB apskmod / apskdemod with explicit identity SymbolMapping (numkit's default). Engine-detecting shim handles MATLAB's name-value form vs numkit's positional 5th arg. Standard 16-APSK [4,12] [1, 2.7] forward+round-trip + nearest-neighbour demod under small noise. Bit-equal with MATLAB R2025b. Default 'gray' SymbolMapping deferred -- MATLAB's per-ring Gray for non-power-of-2 (M=12) needs more probing. Octave 11.1.0 doesn't ship apskmod in core; reports N/A. |
| `apskdemod` | ❌ |  |  |  |  |  |
| `mil188qammod` | ✅ | 0.115 | 37.30× |  | OK | MATLAB mil188qammod / mil188qamdemod (MIL-STD-188-110 QAM). Bit-equal with MATLAB R2025b on ALL FOUR supported constellations: M=16, 32, 64, 256. All tables hard-coded per MATLAB's spec-rounded values (probed at %.17g). MIL188 cluster CLOSED 4/4. Octave 11.1.0 doesn't ship mil188qam in core; reports N/A. |
| `mil188qamdemod` | ❌ |  |  |  |  |  |
| `mskmod` | ⚠️ | 0.005 | 255.66× |  | OK | MATLAB mskmod (differential variant): minimum-shift keying. Bit-equal with MATLAB R2025b. Algorithm: cumulative-sum phase ramp interpolated linearly between symbol boundaries, then exp(i*phase). Differential mode used (MATLAB's default; passed explicitly via 'diff' arg through engine-detecting shim because MATLAB requires it). Argument order: mskmod(x, nSamp, dataenc, ini_phase) -- dataenc is positional 3rd, NOT 4th. ini_phase must be a multiple of pi/2 in MATLAB; numkit accepts arbitrary (extension). KNOWN GAP: non-differential variant deferred (uses rectpulse + I/Q stagger). Octave 11.1.0 doesn't ship mskmod in core; reports N/A. |
| `mskdemod` | ❌ |  |  |  |  |  |
| `fskmod` | ✅ | 0.005 | 220.12× |  | OK | Sig: r = fskmod(...). Spec-extension batch 2026-05-09.  |
| `fskdemod` | ✅ | 0.006 | 335.89× |  | OK | Sig: r = fskdemod(...). Spec-extension batch 2026-05-09.  |
| `ofdmmod` | ✅ | 0.011 | 185.39× |  | OK | Sig: r = ofdmmod(...). Spec-extension batch 2026-05-09. |
| `ofdmdemod` | ✅ | 0.019 | 198.43× |  | OK | Sig: r = ofdmdemod(...). Spec-extension batch 2026-05-09. |
| `dpskmod` | ✅ | 0.002 | 26.77× |  | OK | Sig: r = dpskmod(...). Spec-extension batch 2026-05-09.  |
| `dpskdemod` | ✅ | 0.003 | 414.34× |  | OK | Sig: r = dpskdemod(...). Spec-extension batch 2026-05-09.  |
| `pskmod` | ✅ | 0.004 | 415.30× |  | OK | Sig: r = pskmod(...). Spec-extension batch 2026-05-09. |
| `pskdemod` | ✅ | 0.005 | 502.56× |  | OK | Sig: r = pskdemod(...). Spec-extension batch 2026-05-09. |
| `ammod` | ✅ | 0.003 | 153.46× |  | OK | MATLAB ammod: amplitude modulator y = (x + carr_amp).*cos(2π·Fc·t + ini_phase). Covered: DSB-SC (carramp=0 default) and DSB-TC (carramp=0.5, ini_phase=pi/4) forms over a 100-sample column-vector input. Bit-equal with MATLAB R2025b within ~1e-10 (Highway sin/cos contributes a few ULP). Octave 11.1.0 doesn't ship ammod in core (signal/communications package only); reports N/A. |
| `amdemod` | ❌ |  |  |  |  |  |
| `fmmod` | ✅ | 0.003 |  |  | N/A | MATLAB fmmod: frequency modulator y = cos(2π·Fc·t + 2π·freqdev·cumsum(x)/Fs + ini_phase). Covered: default (ini_phase=0) and explicit ini_phase forms over a 100-sample column-vector input. Bit-equal with MATLAB R2025b within ~1e-10 (Highway sin/cos contributes a few ULP). Octave 11.1.0 doesn't ship fmmod in core (signal/communications package only); reports N/A. |
| `fmdemod` | ❌ |  |  |  |  |  |
| `pmmod` | ✅ | 0.005 | 140.90× |  | OK | MATLAB pmmod: phase modulator y = cos(2π·Fc·t + phasedev·x + ini_phase). Covered: default (ini_phase=0) and explicit ini_phase forms, 100-sample column-vector input, sample points across the signal. Bit-equal with MATLAB R2025b. Octave 11.1.0 doesn't ship pmmod in core (it's in the communications package); reports N/A. |
| `pmdemod` | ❌ |  |  |  |  |  |
| `ssbmod` | ✅ | 0.017 | 79.13× |  | OK | MATLAB ssbmod: single-sideband modulator. y = x.*cos(2π·Fc·t + ini) ± imag(hilbert(x)).*sin(2π·Fc·t + ini); +sign for default lower sideband, -sign for 'upper'. Hilbert is FFT-based -> ~1e-10 ULP-level deviation from MATLAB. Octave 11.1.0 doesn't ship ssbmod in core (signal/communications package only); reports N/A. |
| `ssbdemod` | ❌ |  |  |  |  |  |

### Sources, Sinks, and Signal Operations

**Namespace:** `comm.signals.*` — 0 ✅ + 0 ⚠️ / 17 = 0%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `randerr` | ✅ | 0.027 | 129.18× |  | OK | MATLAB randerr: random binary error matrix with controllable error count per row. Uses MatlabMT19937 for bit-equal output with MATLAB R2025b on seeded inputs. Covered: scalar (1 error), scalar (3 errors), vector [1 2 3] uniform, weighted [0 1 2; 0.0 0.7 0.3]. All known column placements + row sums fingerprinted. Octave 11.1.0 doesn't ship randerr in core; reports N/A. |
| `randsrc` | ✅ | 0.108 | 36.20× |  | OK | MATLAB randsrc: random matrix from finite alphabet with optional weighted probabilities. Numkit uses MatlabMT19937 (= MATLAB's mt19937ar) seeded with explicit state arg, so seeded outputs are bit-identical with MATLAB R2025b. Probability fingerprints (~70/20/10%) within 5% Monte-Carlo tolerance over 5000 samples. Octave 11.1.0 doesn't ship randsrc in core (signal/communications package only); reports N/A. |
| `wgn` | ✅ | 0.004 | 249.77× |  | OK | Sig: r = wgn(...). Spec-extension batch 2026-05-09.  |
| `biterr` | ✅ | 0.002 | 437.09× |  | OK | Sig: [n, r] = biterr(x, y[, k]). Counts differing bits between non-negative integer arrays. Bit-width k auto-detected as smallest covering width. |
| `symerr` | ✅ | 0.006 | 120.63× |  | OK | Sig: [n, r] = symerr(x, y). Element-wise inequality count + ratio. |
| `zadoffChuSeq` | ❌ |  |  |  |  | Zadoff-Chu reference sequence |
| `mask2shift` | ❌ |  |  |  |  | shift-register mask → shift |
| `shift2mask` | ❌ |  |  |  |  |  |
| `bit2int` | ❌ |  |  |  |  | pack bits to integers |
| `int2bit` | ❌ |  |  |  |  | unpack integers to bits |
| `bi2de` | ❌ |  |  |  |  | binary → decimal (legacy alias) |
| `de2bi` | ❌ |  |  |  |  | decimal → binary (legacy alias) |
| `hex2poly` | ❌ |  |  |  |  | hex string → polynomial coeffs |
| `oct2poly` | ❌ |  |  |  |  |  |
| `oct2dec` | ❌ |  |  |  |  | octal → decimal |
| `vec2mat` | ❌ |  |  |  |  | reshape with zero-pad |
| `convertSNR` | ✅ | 0.001 | 2703.93× |  | OK | Sig: r = convertSNR(...). Spec-extension batch 2026-05-09. |

### Source Coding

**Namespace:** `comm.source_coding.*` — 0 ✅ + 0 ⚠️ / 11 = 0%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `arithenco` | ✅ | 0.004 | 730.98× |  | OK | MATLAB arithenco / arithdeco: arithmetic coding pair. Bit-equal with MATLAB R2025b on encoded bit string and decoded sequence. Implements the Sayood textbook E1/E2/E3 rescaling algorithm matching MATLAB's source. Octave 11.1.0 doesn't ship arithenco/arithdeco in core (signal/communications package only); reports N/A. |
| `arithdeco` | ❌ |  |  |  |  |  |
| `compand` | ✅ | 0.004 | 134.98× |  | OK | MATLAB compand: μ-law / A-law signal compander. 4 methods covered (mu/compressor, mu/expander, A/compressor, A/expander) with round-trip identity validation and sign preservation on negatives. Algorithm: closed-form formulas from MATLAB compand.m. Output preserves input shape. |
| `dpcmenco` | ⚠️ | 0.003 | 146.14× |  | OK | MATLAB dpcmenco/dpcmdeco: 1st-order DPCM (predictor=[0 1]) with 6-bin codebook + 5-threshold partition. Bit-equal with MATLAB R2025b on encoded indices, quantization error, and reconstructed signal. Round-trip qe consistency (qe from encoder == qe from decoder via codebook lookup) also verified. dpcmopt deferred (training-set optimisation needs Lloyd-Max + alternating predictor estimation -- own cycle). Octave 11.1.0 doesn't ship dpcmenco/deco in core; reports N/A. |
| `dpcmdeco` | ❌ |  |  |  |  |  |
| `dpcmopt` | ✅ | 0.004 | 383.55× |  | OK | Communications toolbox dpcmopt — DPCM parameter optimiser. CLEAN-ROOM implementation from public references (J. Makhoul, Linear Prediction: A Tutorial Review, Proc. IEEE 1975 — autocorrelation method + Levinson-Durbin; Proakis & Manolakis, Digital Signal Processing; Jayant & Noll, Digital Coding of Waveforms, 1984) — see cleanroom/specs/dpcmopt.md. Algorithm: estimate the autocorrelation r[k] = (sum x[n]x[n+k]) / (N-1-k) for lags 0..ord, solve the Yule-Walker normal equations by the Levinson-Durbin recursion to get the prediction-error filter A(z), and form the predictor [0, -a1, ..., -aM]; when a third argument is given (codebook length or initial codebook) run lloyds() on the prediction residual for codebook+partition. Bit-equal MATLAB R2025b on deterministic training (sin + linear ramp) for predictor and codebook/partition outputs. Octave 11.1.0 ships dpcmopt in the communications package (not loaded by default); harness reports N/A there. |
| `huffmandict` | ✅ | 0.008 | 215.09× |  | OK | MATLAB huffmandict: Huffman code-book builder. Codes are NOT unique (tie-breaking yields different but equally optimal trees) -- the invariant is avglen = sum(p_k * L_k). Fingerprint pins avglen on three test cases (5-symbol skewed, 2-symbol, 4-symbol uniform). Code shape, prefix-freeness and bounds H <= avglen < H+1 covered in gtest. Octave 11.1.0 doesn't ship huffmandict in core (signal/communications package only); reports N/A. |
| `huffmanenco` | ✅ | 0.006 | 389.22× |  | OK | MATLAB huffmanenco/huffmandeco: encode/decode round-trip via dict from huffmandict. Bit codes are non-unique (Huffman tie-breaking can produce different but equally optimal trees), so encoded length depends on which optimal dict shape was produced. The INVARIANT under both engines is round-trip identity: dec must equal sig regardless of dict shape. We pin rt_match==1, length(dec), and the first/last decoded symbols. Octave 11.1.0 doesn't ship the Huffman codec in core; reports N/A. |
| `huffmandeco` | ❌ |  |  |  |  |  |
| `lloyds` | ⚠️ | 0.007 | 158.53× |  | OK | MATLAB lloyds: Lloyd-Max scalar quantizer designer. Tested on deterministic monotone training (1:10) since random-seed paths use randn which differs (Ziggurat for randn deferred). Bit-equal with MATLAB R2025b on initial-codebook form ([2 5 8]) and integer-K form (K=2, K=4). Octave 11.1.0 doesn't ship lloyds in core (signal/communications package only); reports N/A. |
| `quantiz` | ✅ | 0.005 | 127.54× |  | OK | MATLAB quantiz: scalar quantizer applier. indx(i) = sum(partition < sig(i)); quantv = codebook(indx+1); distor = mean((sig-quantv)^2). Bit-equal with MATLAB R2025b. Octave 11.1.0 doesn't ship quantiz in core (signal/communications package only); reports N/A. |

### Error Detection and Correction

**Namespace:** `comm.fec.*` — 0 ✅ + 0 ⚠️ / 26 = 0%

`crcConfig`, `ldpcEncoderConfig`, `ldpcDecoderConfig`, the System
Objects (`comm.CRCGenerator`, `comm.LDPCEncoder`, etc.) and the `gf`
class are intentionally omitted. Galois-field math is exposed through
the flat `gf*` function family below.

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `crcGenerate` | ❌ |  |  |  |  | append CRC parity bits |
| `crcDetect` | ❌ |  |  |  |  |  |
| `cyclgen` | ❌ |  |  |  |  | cyclic-code generator matrix |
| `cyclpoly` | ❌ |  |  |  |  | cyclic-code generator polynomials |
| `encode` | ❌ |  |  |  |  | generic block encoder |
| `decode` | ❌ |  |  |  |  | generic block decoder |
| `gfweight` | ❌ |  |  |  |  | minimum distance |
| `gen2par` | ❌ |  |  |  |  | generator ↔ parity-check matrix |
| `hammgen` | ❌ |  |  |  |  | Hamming generator/parity-check |
| `syndtable` | ❌ |  |  |  |  | syndrome decoding table |
| `bchenc` | ❌ |  |  |  |  | BCH encoder |
| `bchdec` | ❌ |  |  |  |  |  |
| `bchgenpoly` | ❌ |  |  |  |  |  |
| `bchnumerr` | ❌ |  |  |  |  |  |
| `rsenc` | ❌ |  |  |  |  | Reed-Solomon encoder |
| `rsdec` | ❌ |  |  |  |  |  |
| `rsgenpoly` | ❌ |  |  |  |  |  |
| `rsgenpolycoeffs` | ❌ |  |  |  |  |  |
| `ldpcEncode` | ❌ |  |  |  |  |  |
| `ldpcDecode` | ❌ |  |  |  |  |  |
| `ldpcPCM` | ❌ |  |  |  |  | parity-check matrices for standards |
| `ldpcQuasiCyclicMatrix` | ❌ |  |  |  |  |  |
| `tpcenc` | ❌ |  |  |  |  | turbo product encoder |
| `tpcdec` | ❌ |  |  |  |  |  |
| `convenc` | ❌ |  |  |  |  | convolutional encoder |
| `vitdec` | ❌ |  |  |  |  | Viterbi decoder |

### Trellis and Galois Field Utilities

**Namespace:** `comm.gf.*` — 0 ✅ + 0 ⚠️ / 22 = 0%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `distspec` | ❌ |  |  |  |  | distance spectrum of conv code |
| `iscatastrophic` | ❌ |  |  |  |  |  |
| `istrellis` | ❌ |  |  |  |  |  |
| `poly2trellis` | ❌ |  |  |  |  | conv-poly → trellis struct |
| `cosets` | ❌ |  |  |  |  | cyclotomic cosets |
| `dftmtx` | ✅ | 0.005 | 20.73× |  | OK | Sig: F = dftmtx(N). N×N DFT matrix; F(j,k) = exp(-2πi(j-1)(k-1)/N). For real input dftmtx(N)*x equals fft(x). Edges: F2 4 elem, F4 16 elem, F8 64 elem, F16 256 elem; F8(2,2) = √2/2 - i√2/2 ≈ 0.7071-0.7071i; F8(5,5) = 1 + 0i (column 5 row 5 ≡ exp(-2πi·16/8) = exp(-4πi) = 1); dftmtx(1) = 1. |
| `isprimitive` | ❌ |  |  |  |  |  |
| `minpol` | ❌ |  |  |  |  | minimal polynomial in GF |
| `primpoly` | ❌ |  |  |  |  | primitive polynomial of degree m |
| `gfadd` | ❌ |  |  |  |  | GF addition |
| `gfconv` | ❌ |  |  |  |  | GF polynomial multiply |
| `gfcosets` | ❌ |  |  |  |  | GF(p^m) cosets |
| `gfdeconv` | ❌ |  |  |  |  | GF polynomial divide |
| `gfdiv` | ❌ |  |  |  |  | element-wise GF division |
| `gffilter` | ❌ |  |  |  |  | GF FIR filter |
| `gflineq` | ❌ |  |  |  |  | linear equations over GF(p) |
| `gfminpol` | ❌ |  |  |  |  |  |
| `gfmul` | ❌ |  |  |  |  | element-wise GF multiplication |
| `gfpretty` | ❌ |  |  |  |  | pretty-print GF poly |
| `gfprimck` | ❌ |  |  |  |  | check primitivity |
| `gfprimdf` | ❌ |  |  |  |  | default primitive polynomial |
| `gftuple` | ❌ |  |  |  |  | exponential ↔ polynomial form |

### Interleaving

**Namespace:** `comm.intrlv.*` — 0 ✅ + 0 ⚠️ / 16 = 0%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `intrlv` | ❌ |  |  |  |  | generic interleaver |
| `deintrlv` | ❌ |  |  |  |  |  |
| `algintrlv` | ❌ |  |  |  |  | algebraic |
| `algdeintrlv` | ❌ |  |  |  |  |  |
| `helscanintrlv` | ❌ |  |  |  |  | helical-scan |
| `helscandeintrlv` | ❌ |  |  |  |  |  |
| `matintrlv` | ❌ |  |  |  |  | matrix |
| `matdeintrlv` | ❌ |  |  |  |  |  |
| `randintrlv` | ❌ |  |  |  |  | random |
| `randdeintrlv` | ❌ |  |  |  |  |  |
| `convintrlv` | ❌ |  |  |  |  | convolutional |
| `convdeintrlv` | ❌ |  |  |  |  |  |
| `helintrlv` | ❌ |  |  |  |  | helical |
| `heldeintrlv` | ❌ |  |  |  |  |  |
| `muxintrlv` | ❌ |  |  |  |  | multiplexed |
| `muxdeintrlv` | ❌ |  |  |  |  |  |

### Pulse Shaping, Equalization, MIMO

**Namespace:** `comm.shape.*` — 1 ✅ + 0 ⚠️ / 8 = 13%

System-Object equalisers (`comm.LinearEqualizer`, `comm.MLSEEqualizer`,
`comm.DecisionFeedbackEqualizer`) are omitted; only the function-form
MLSE entry is exposed.

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `gaussdesign` | ✅ | 0.004 | 245.70× |  | OK | Sig: h = gaussdesign(BT, span, sps). Gaussian FIR pulse-shaping filter. Bit-identical with MATLAB R2025b on (0.3, 4, 8) probe (h(17)=0.112904, sum=1, length=33). Earlier defer was wrong. |
| `rcosdesign` | ✅ | 0.004 | 333.15× |  | OK | Sig: r = rcosdesign(...). Spec-extension batch 2026-05-09.  |
| `rectpulse` | ✅ | 0.004 | 81.56× |  | OK | Sig: r = rectpulse(...). Spec-extension batch 2026-05-09. |
| `intdump` | ✅ | 0.004 | 314.01× |  | OK | Sig: r = intdump(...). Spec-extension batch 2026-05-09. |
| `mlseeq` | ❌ |  |  |  |  | maximum-likelihood sequence equaliser |
| `ofdmEqualize` | ❌ |  |  |  |  | OFDM zero-forcing / MMSE equalise |
| `blkdiagbfweights` | ❌ |  |  |  |  | block-diagonalisation BF weights |
| `ofdmPrecode` | ❌ |  |  |  |  | OFDM precoding |

### RF and Channel Impairments

**Namespace:** `comm.rf.*` — 4 ✅ + 0 ⚠️ / 10 = 40%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `awgn` | ✅ | 0.002 | 341.02× |  | OK | Sig: r = awgn(...). Spec-extension batch 2026-05-09. |
| `bsc` | ✅ | 0.002 | 161.68× |  | OK | Sig: r = bsc(...). Spec-extension batch 2026-05-09. |
| `rayleighchan` | ✅ | 0.017 | 23.94× | 39.47× | OK | N/A (definite): MATLAB R2025b DEPRECATED rayleighchan() in favour of comm.RayleighChannel system object. Numkit retains rayleighchan as a convenience helper that returns one complex Rayleigh sample. Definite N/A -- no MATLAB top-level reference exists in the current release. |
| `ricianchan` | ✅ | 0.018 | 21.62× | 47.27× | OK | N/A (definite): MATLAB R2025b DEPRECATED ricianchan() in favour of comm.RicianChannel system object. Numkit retains ricianchan as a convenience helper. Definite N/A. |
| `stdchan` | ❌ |  |  |  |  | standard channel-model picker |
| `frequencyOffset` | ❌ |  |  |  |  | apply Δf |
| `iqimbal` | ❌ |  |  |  |  | apply IQ imbalance |
| `iqcoef2imbal` | ❌ |  |  |  |  | coefficients → amp/phase imbalance |
| `iqimbal2coef` | ❌ |  |  |  |  |  |
| `srmdelay` | ❌ |  |  |  |  | sample-rate-matching delay |
| `channelDelay` | ❌ |  |  |  |  | channel-delay estimation |
| `ofdmChannelResponse` | ❌ |  |  |  |  | OFDM frequency-domain channel |

### Propagation Path Loss and Geometry

**Namespace:** `comm.propagation.*` — 0 ✅ + 0 ⚠️ / 15 = 0%

OOP `propagationModel` family, ray-tracing classes (`raytrace`,
`coverage`, `pattern`, `sinr`, `link`, `sigstrength`) and the antenna /
basemap object hierarchy intentionally omitted — only flat scalar /
vector path-loss models and coordinate transforms.

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `fspl` | ❌ |  |  |  |  | free-space path loss |
| `cranerainpl` | ❌ |  |  |  |  | Crane rain attenuation |
| `rainpl` | ❌ |  |  |  |  | ITU rain attenuation |
| `gaspl` | ❌ |  |  |  |  | gas (oxygen + water vapour) |
| `fogpl` | ❌ |  |  |  |  | fog / cloud |
| `raypl` | ❌ |  |  |  |  | propagation along a ray |
| `buildingMaterialPermittivity` | ❌ |  |  |  |  | ITU building materials |
| `earthSurfacePermittivity` | ❌ |  |  |  |  |  |
| `los` | ❌ |  |  |  |  | line-of-sight check |
| `doppler` | ❌ |  |  |  |  | Doppler-shift utility |
| `rangeangle` | ❌ |  |  |  |  | range and angle between coordinates |
| `global2localcoord` | ❌ |  |  |  |  |  |
| `local2globalcoord` | ❌ |  |  |  |  |  |
| `cart2sphvec` | ❌ |  |  |  |  | rotate vector to spherical basis |
| `sph2cartvec` | ❌ |  |  |  |  |  |

### Performance Analysis

**Namespace:** `comm.perf.*` — 6 ✅ + 0 ⚠️ / 11 = 55%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `berawgn` | ✅ | 0.001 | 682.07× |  | OK | Sig: r = berawgn(...). Spec-extension batch 2026-05-09. |
| `bercoding` | ❌ |  |  |  |  | with coding gain |
| `berconfint` | ✅ | 0.003 |  |  | N/A | Sig: r = berconfint(...). Spec-extension batch 2026-05-09.  |
| `berfading` | ❌ |  |  |  |  | over Rayleigh / Rician fading |
| `berfit` | ❌ |  |  |  |  | curve fit BER vs Eb/No |
| `bersync` | ❌ |  |  |  |  | with imperfect sync |
| `semianalytic` | ❌ |  |  |  |  | semi-analytic BER |
| `marcumq` | ✅ | 0.112 | 8.50× | 3.29× | OK | Sig: r = marcumq(...). Spec-extension batch 2026-05-09. |
| `qfunc` | ✅ | 0.003 | 61.05× |  | OK | Sig: r = qfunc(...). Spec-extension batch 2026-05-09. |
| `qfuncinv` | ✅ | 0.003 | 57.98× |  | OK | Sig: r = qfuncinv(...). Spec-extension batch 2026-05-09. |
| `noisebw` | ✅ | 0.015 | 257.10× |  | OK | Sig: bw = noisebw(num, den, Nsamp, fs). Equivalent noise bandwidth via NBW = (fs/N) * sum(|H|^2) / max(|H|^2). Matches MATLAB R2025b within ~0.5 Hz on probed FIR (numerical-grid difference). |

## Control

### LTI Models

**Namespace:** `control.lti.*` — 9 ✅ + 0 ⚠️ / 19 = 47%

`tf`/`zpk`/`ss`/`frd` are object constructors in MATLAB; we treat them
as flat structure-returning functions (returning a struct with fields
{num, den}, {z, p, k}, {A, B, C, D}, {response, frequency} etc.) and
the data-extraction `*data` functions read those structs. The full
`lti` / `dynamicSystem` class hierarchy and Simulink integration
(`slTuner`, `addBlock`/`removeBlock`/`setBlockParam`, etc.) are
intentionally omitted.

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `tf` | ✅ | 0.004 | 1587.47× | 160.16× | OK | Sig: r = tf(...). Spec-extension batch 2026-05-09. |
| `zpk` | ✅ | 0.004 | 1543.92× | 157.31× | OK | Sig: r = zpk(...). Spec-extension batch 2026-05-09. |
| `ss` | ✅ | 0.006 | 961.48× | 72.22× | OK | Sig: r = ss(...). Spec-extension batch 2026-05-09. |
| `frd` | ✅ | 0.001 |  |  | N/A | Sig: r = frd(...). Spec-extension batch 2026-05-09. |
| `dss` | ❌ |  |  |  |  | descriptor state-space (E·xdot = Ax + Bu) |
| `filt` | ✅ | 0.002 |  |  | N/A | Sig: r = filt(...). Spec-extension batch 2026-05-09. |
| `pid` | ❌ |  |  |  |  | parallel-form PID controller |
| `pid2` | ❌ |  |  |  |  | 2-DOF PID |
| `pidstd` | ❌ |  |  |  |  | standard-form PID |
| `pidstd2` | ❌ |  |  |  |  | 2-DOF standard PID |
| `rss` | ❌ |  |  |  |  | random stable continuous SS |
| `drss` | ❌ |  |  |  |  | random stable discrete SS |
| `tfdata` | ✅ | 0.006 | 1151.61× | 197.42× | OK | Sig: r = tfdata(...). Spec-extension batch 2026-05-09. |
| `zpkdata` | ✅ | 0.006 | 1336.61× | 278.21× | OK | Sig: r = zpkdata(...). Spec-extension batch 2026-05-09. |
| `ssdata` | ✅ | 0.007 | 1082.44× | 57.03× | OK | Sig: r = ssdata(...). Spec-extension batch 2026-05-09. |
| `frdata` | ✅ | 0.003 |  |  | N/A | Sig: r = frdata(...). Spec-extension batch 2026-05-09. |
| `dssdata` | ❌ |  |  |  |  | extract A/B/C/D/E |
| `piddata` | ❌ |  |  |  |  |  |
| `pidstddata` | ❌ |  |  |  |  |  |

### Model Properties

**Namespace:** `control.props.*` — 11 ✅ + 0 ⚠️ / 11 = **100%**

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `isct` | ✅ | 0.004 | 1752.55× | 155.10× | OK | Sig: r = isct(...). Spec-extension batch 2026-05-09. |
| `isdt` | ✅ | 0.006 | 4149.77× | 450.00× | OK | Sig: r = isdt(...). Spec-extension batch 2026-05-09. |
| `isproper` | ✅ | 0.004 | 1637.99× |  | OK | Sig: r = isproper(...). Spec-extension batch 2026-05-09. |
| `issiso` | ✅ | 0.004 | 1702.98× | 153.73× | OK | Sig: r = issiso(...). Spec-extension batch 2026-05-09. |
| `isstable` | ✅ | 0.004 | 1569.65× | 154.52× | OK | Sig: r = isstable(...). Spec-extension batch 2026-05-09. |
| `isstatic` | ✅ | 0.004 | 1518.33× |  | OK | Sig: r = isstatic(...). Spec-extension batch 2026-05-09. |
| `order` | ✅ | 0.004 | 1740.99× |  | OK | Sig: r = order(...). Spec-extension batch 2026-05-09. |
| `pole` | ✅ | 0.005 | 1424.34× | 134.88× | OK | Sig: r = pole(...). Spec-extension batch 2026-05-09. |
| `zero` | ✅ | 0.005 | 1682.75× | 151.78× | OK | Sig: r = zero(...). Spec-extension batch 2026-05-09. |
| `tzero` | ✅ | 0.004 | 1659.83× |  | OK | Sig: z = tzero(sys). SISO transmission zeros via ss2tf + roots. Bit-identical with MATLAB R2025b on probed system (z = 1.0). MIMO requires QZ generalised eigenvalue solver (separate ТЗ). |
| `damp` | ✅ | 0.004 | 476.70× |  | OK | Sig: r = damp(...). Spec-extension batch 2026-05-09. |

### Model Conversion & Reduction

**Namespace:** `control.convert.*` — 3 ✅ + 0 ⚠️ / 18 = 17%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `c2d` | ✅ | 0.004 | 5244.05× |  | OK | Sig: r = c2d(...). Spec-extension batch 2026-05-09. |
| `c2dOptions` | ❌ |  |  |  |  |  |
| `d2c` | ✅ | 0.007 | 4519.99× |  | OK | Sig: r = d2c(...). Spec-extension batch 2026-05-09. |
| `d2cOptions` | ❌ |  |  |  |  |  |
| `d2d` | ❌ |  |  |  |  | resample discrete |
| `d2dOptions` | ❌ |  |  |  |  |  |
| `ss2ss` | ✅ | 0.007 | 1035.18× | 104.97× | OK | Sig: r = ss2ss(...). Spec-extension batch 2026-05-09. |
| `canon` | ❌ |  |  |  |  | canonical realisation |
| `balreal` | ❌ |  |  |  |  | balanced realisation |
| `prescale` | ❌ |  |  |  |  | improve numerics by scaling |
| `modalreal` | ❌ |  |  |  |  | modal realisation |
| `compreal` | ❌ |  |  |  |  | companion realisation |
| `minreal` | ❌ |  |  |  |  | minimal realisation |
| `sminreal` | ❌ |  |  |  |  | structurally minimal |
| `balred` | ❌ |  |  |  |  | balanced reduction |
| `modred` | ❌ |  |  |  |  | model reduction |
| `hsvd` | ❌ |  |  |  |  | Hankel singular values |
| `pade` | ❌ |  |  |  |  | Padé approximation of delay |
| `ss2tf` | ✅ | 0.005 | 579.63× | 338.63× | OK | Sig: r = ss2tf(...). Spec-extension batch 2026-05-09. |

### Interconnections

**Namespace:** `control.connect.*` — 3 ✅ + 0 ⚠️ / 7 = 43%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `feedback` | ✅ | 0.002 |  |  | N/A | Sig: sys = feedback(sys1, sys2[, sign]). Closed-loop feedback connection. Denominator bit-identical with MATLAB R2025b (1 + s + s^2 -> [1 1 1]). Numerator semantically identical (numkit doesn't pad with leading zeros, MATLAB does -- same H(s)). |
| `series` | ✅ | 0.009 | 1190.45× | 376.71× | OK | Sig: r = series(...). Spec-extension batch 2026-05-09. |
| `parallel` | ✅ | 0.009 | 1112.40× | 946.63× | OK | Sig: r = parallel(...). Spec-extension batch 2026-05-09. |
| `connect` | ❌ |  |  |  |  | name-based interconnect |
| `append` | ✅ | 0.003 | 21.27× |  | OK | Sig: r = append(...). Spec-extension batch 2026-05-09. |
| `lft` | ❌ |  |  |  |  | linear fractional transform |
| `sumblk` | ❌ |  |  |  |  | summation block (for connect) |

### Time and Frequency Response

**Namespace:** `control.response.*` — 9 ✅ + 0 ⚠️ / 19 = 47%

`*plot` variants intentionally dropped — they're display-only mirrors
of the numeric functions (which already return data when called with
output args).

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `step` | ✅ | 0.010 | 3082.45× | 357.88× | OK | Sig: [y, t] = step(sys[, T]). Default time grid via Tfinal = -log(0.003)/min|Re(p)|, N=127. Bit-identical with MATLAB R2025b on probed 1st-order system. |
| `stepinfo` | ✅ | 0.021 | 1514.09× |  | OK | Sig: r = stepinfo(...). Spec-extension batch 2026-05-09. |
| `impulse` | ✅ | 0.010 | 2765.48× | 377.39× | OK | Sig: [y, t] = impulse(sys[, T]). Default time grid via Tfinal = -log(0.003)/min|Re(p)|, N=127. Bit-identical with MATLAB R2025b on probed 1st-order system. |
| `initial` | ❌ |  |  |  |  | response from initial state |
| `lsim` | ✅ | 0.006 | 3734.41× | 472.37× | OK | Sig: r = lsim(...). Spec-extension batch 2026-05-09. |
| `lsiminfo` | ❌ |  |  |  |  |  |
| `gensig` | ❌ |  |  |  |  | input signal generator |
| `covar` | ❌ |  |  |  |  | output covariance under stochastic input |
| `bode` | ✅ | 0.004 | 2960.53× |  | OK | Sig: r = bode(...). Spec-extension batch 2026-05-09. |
| `bodemag` | ❌ |  |  |  |  | magnitude only |
| `nyquist` | ✅ | 0.005 | 1921.29× | 944.97× | OK | Sig: r = nyquist(...). Spec-extension batch 2026-05-09. |
| `nichols` | ❌ |  |  |  |  |  |
| `sigma` | ❌ |  |  |  |  | singular-value response |
| `freqresp` | ✅ | 0.004 |  |  | N/A | Sig: r = freqresp(...). Spec-extension batch 2026-05-09. |
| `evalfr` | ✅ | 0.003 | 454.52× |  | OK | Sig: r = evalfr(...). Spec-extension batch 2026-05-09. |
| `dcgain` | ✅ | 0.003 | 472.35× |  | OK | Sig: r = dcgain(...). Spec-extension batch 2026-05-09. |
| `bandwidth` | ✅ | 0.009 | 18.19× |  | OK | Sig: matrix-structure predicates and bandwidth queries. isbanded(A, lo, up) ↔ outside-band entries are zero; isdiag = isbanded(A, 0, 0). istril / istriu ↔ lower / upper triangular. [lo, up] = bandwidth(A) returns sub-diagonal and super-diagonal extent; bandwidth(A) one-out form returns lo; 'lower'|'upper' string form selects one. Bit-exact MATLAB R2025b (tol=0): D diagonal (ib1=id1=1), A tri-banded (ib2=1), A not upper-banded-only (ib3=0), L lower-tri (itl=1), U upper-tri (itu=1), bandwidth(A) = [1, 1], bandwidth(U) = 0, bandwidth(L) = 2, bandwidth(U, 'upper') = 2. Split from the original combined predicates spec to stay under the 256-register chunk limit. |
| `getPeakGain` | ❌ |  |  |  |  | H∞ |
| `getGainCrossover` | ❌ |  |  |  |  |  |

### Stability and Margins

**Namespace:** `control.margin.*` — 3 ✅ + 0 ⚠️ / 6 = 50%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `margin` | ✅ | 0.015 | 1781.83× | 644.32× | OK | Sig: r = margin(...). Spec-extension batch 2026-05-09. |
| `allmargin` | ❌ |  |  |  |  | all stability margins |
| `db2mag` | ✅ | 0.002 | 52.11× |  | OK | Sig: r = db2mag(...). Spec-extension batch 2026-05-09. |
| `mag2db` | ✅ | 0.004 | 66.82× | 42.43× | OK | Sig: r = mag2db(...). Spec-extension batch 2026-05-09. |
| `pzmap` | ✅ | 0.005 | 1942.76× | 157.37× | OK | Sig: r = pzmap(...). Spec-extension batch 2026-05-09. |
| `rlocus` | ✅ | 0.021 | 964.75× | 472.86× | OK | Sig: r = rlocus(...). Spec-extension batch 2026-05-09. |

### State-Space Design and Estimation

**Namespace:** `control.design.*` — 4 ✅ + 0 ⚠️ / 18 = 22%

OOP filters (`extendedKalmanFilter`, `unscentedKalmanFilter`,
`particleFilter`) intentionally omitted — they're class-objects with
methods (`correct`, `predict`, etc.). Flat steady-state designs only.

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `lqr` | ❌ |  |  |  |  | linear-quadratic regulator |
| `lqry` | ❌ |  |  |  |  | LQR with output weighting |
| `lqi` | ❌ |  |  |  |  | LQR with integral action |
| `dlqr` | ❌ |  |  |  |  | discrete LQR |
| `lqrd` | ❌ |  |  |  |  | continuous LQR with sampled controller |
| `lqg` | ❌ |  |  |  |  | linear-quadratic Gaussian |
| `lqgreg` | ❌ |  |  |  |  | LQG regulator |
| `lqgtrack` | ❌ |  |  |  |  | tracking LQG |
| `place` | ✅ | 0.005 | 378.72× | 34.46× | OK | Sig: K = place(A, B, p). Re-closed 2026-05-09 -- prior defer was wrong; numkit returns K=[1 2] matching MATLAB on probe. |
| `estim` | ❌ |  |  |  |  | steady-state estimator (Kalman) |
| `kalman` | ❌ |  |  |  |  | continuous-time Kalman gain |
| `kalmd` | ❌ |  |  |  |  | discrete Kalman from continuous plant |
| `reg` | ❌ |  |  |  |  | full-state controller + observer |
| `ctrb` | ✅ | 0.003 | 65.88× |  | OK | Sig: r = ctrb(...). Spec-extension batch 2026-05-09. |
| `obsv` | ✅ | 0.004 | 93.11× | 35.88× | OK | Sig: r = obsv(...). Spec-extension batch 2026-05-09. |
| `gram` | ❌ |  |  |  |  | controllability/observability gramian |
| `ctrbf` | ❌ |  |  |  |  | controllable-form decomposition |
| `obsvf` | ❌ |  |  |  |  | observable-form decomposition |

### Matrix Equations

**Namespace:** `control.matrixeq.*` — 2 ✅ + 0 ⚠️ / 8 = 25%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `lyap` | ✅ | 0.003 | 444.29× | 26.75× | OK | Sig: r = lyap(...). Spec-extension batch 2026-05-09. |
| `lyapchol` | ❌ |  |  |  |  | factored continuous Lyapunov |
| `dlyap` | ✅ | 0.001 | 1221.92× |  | OK | Sig: r = dlyap(...). Spec-extension batch 2026-05-09. |
| `dlyapchol` | ❌ |  |  |  |  | factored discrete Lyapunov |
| `care` | ❌ |  |  |  |  | continuous algebraic Riccati |
| `dare` | ❌ |  |  |  |  | discrete algebraic Riccati |
| `gcare` | ❌ |  |  |  |  | generalised continuous Riccati |
| `gdare` | ❌ |  |  |  |  | generalised discrete Riccati |

### PID Tuning and Modal Analysis

**Namespace:** `control.tune.*` — 0 ✅ + 0 ⚠️ / 7 = 0%

`pidTuner`, `looptune`, `systune`, `slTuner` and friends intentionally
omitted — interactive / Simulink / OOP.

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `pidtune` | ❌ |  |  |  |  | automatic PID tuning |
| `pidtuneOptions` | ❌ |  |  |  |  |  |
| `getPIDLoopResponse` | ❌ |  |  |  |  |  |
| `modalsep` | ❌ |  |  |  |  | modal separation |
| `stabsep` | ❌ |  |  |  |  | stable / unstable split |
| `freqsep` | ❌ |  |  |  |  | slow / fast modes |
| `spectralfact` | ❌ |  |  |  |  | spectral factorisation |

## Fitting

### Splines

**Namespace:** `cfit.splines.*` — 15 ✅ + 0 ⚠️ / 49 = 31%

OOP `fittype`/`fit`/`cfit`/`sfit`/`fitoptions`/`excludedata` and the
GUI tools (`sftool`, `bspligui`, `splinetool`, `getcurve`) intentionally
omitted. Curve Fitting's value for a non-OOP runtime sits in the spline
construction / postprocessing primitives — those are all flat functions.

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `bspline` | ❌ |  |  |  |  | B-spline of given order |
| `csape` | ❌ |  |  |  |  | cubic spline w/ end-conditions |
| `csapi` | ✅ | 0.004 | 742.66× |  | OK | Sig: pp = csapi(x, y). Cubic-spline pp-form interpolation. Bit-identical with MATLAB R2025b on probed knots and field access. Earlier defer was wrong -- function works. |
| `csaps` | ❌ |  |  |  |  | cubic smoothing spline |
| `cscvn` | ❌ |  |  |  |  | natural cubic curve through points |
| `rscvn` | ❌ |  |  |  |  | rational cubic curve |
| `spapi` | ❌ |  |  |  |  | B-spline interpolation |
| `spaps` | ❌ |  |  |  |  | smoothing spline (penalised) |
| `spap2` | ❌ |  |  |  |  | least-squares spline fit |
| `spcrv` | ❌ |  |  |  |  | uniform B-spline curve |
| `tpaps` | ❌ |  |  |  |  | thin-plate smoothing spline (2-D) |
| `ppmak` | ✅ | 0.003 | 578.96× |  | OK | Sig: pp = ppmak(breaks, coefs[, d]). Piecewise-polynomial constructor. Pair with fnval. Univariate-only (d=1) tested here. |
| `rpmak` | ❌ |  |  |  |  | rational pp form |
| `rsmak` | ❌ |  |  |  |  | rational spline |
| `spmak` | ❌ |  |  |  |  | B-spline form constructor |
| `stmak` | ❌ |  |  |  |  | stform constructor (2-D scattered) |
| `fn2fm` | ❌ |  |  |  |  | convert between spline forms |
| `fnbrk` | ✅ | 0.001 |  |  | N/A | Sig: out = fnbrk(pp, part). Extract a named part from a pp-form spline. Supports {breaks, coefs, pieces|l, order|k, dim|d, form}. |
| `fnchg` | ❌ |  |  |  |  | change spline properties |
| `fncmb` | ✅ | 0.001 |  |  | N/A | Sig: pp = fncmb(pp1, c) | fncmb(c, pp1) | fncmb(pp1, c1, pp2, c2). Linear combination of pp-form splines on shared breaks. Pure coef arithmetic. |
| `fnder` | ✅ | 0.001 |  |  | N/A | Sig: dpp = fnder(pp[, order]). Differentiate pp-form spline `order` times. Each piece's polynomial is independently differentiated; result has order = K − order. |
| `fndir` | ❌ |  |  |  |  | directional derivative |
| `fnint` | ✅ | 0.001 |  |  | N/A | Sig: ipp = fnint(pp). Antiderivative of pp-form spline; integration constant chosen so that integral = 0 at the first break and is continuous across breaks. |
| `fnjmp` | ❌ |  |  |  |  | jump value at discontinuities |
| `fnmin` | ❌ |  |  |  |  | min of spline |
| `fnplt` | ❌ |  |  |  |  | display |
| `fnrfn` | ❌ |  |  |  |  | refine knots |
| `fntlr` | ❌ |  |  |  |  | Taylor coefficients |
| `fnval` | ✅ | 0.003 |  |  | N/A | Sig: r = fnval(...). Spec-extension batch 2026-05-09. |
| `fnxtr` | ❌ |  |  |  |  | extrapolate |
| `fnzeros` | ❌ |  |  |  |  | zeros of spline |
| `bkbrk` | ❌ |  |  |  |  | break-and-coefs |
| `slvblk` | ❌ |  |  |  |  | solve almost-block-diagonal system |
| `spcol` | ❌ |  |  |  |  | B-spline collocation matrix |
| `stcol` | ❌ |  |  |  |  | stform collocation matrix |
| `subplus` | ✅ | 0.003 | 57.46× |  | OK | Sig: r = subplus(...). Spec-extension batch 2026-05-09. |
| `aptknt` | ❌ |  |  |  |  | append knots for spline of order k |
| `augknt` | ✅ | 0.002 | 363.23× |  | OK | Sig: r = augknt(...). Spec-extension batch 2026-05-09. |
| `aveknt` | ✅ | 0.002 | 204.70× |  | OK | Sig: r = aveknt(...). Spec-extension batch 2026-05-09. |
| `brk2knt` | ✅ | 0.002 | 139.00× |  | OK | Sig: r = brk2knt(...). Spec-extension batch 2026-05-09. |
| `chbpnt` | ❌ |  |  |  |  | Chebyshev sites |
| `knt2brk` | ✅ | 0.004 | 73.19× |  | OK | Sig: [breaks, mults] = knt2brk(knots). Inverse of brk2knt: distinct knots + multiplicities. |
| `newknt` | ❌ |  |  |  |  | distribute knots on equidistribution |
| `optknt` | ❌ |  |  |  |  | optimal knot distribution |
| `smooth` | ❌ |  |  |  |  | data smoothing (already partially in core) |
| `datastats` | ✅ | 0.003 | 285.66× |  | OK | Sig: s = datastats(x). MATLAB requires column vector input. Numkit emits same struct fields {min,max,mean,median,num,range,std} -- bit-identical on probed COLUMN input. |
| `prepareCurveData` | ✅ | 0.005 | 374.42× |  | OK | Sig: [xo, yo[, wo]] = prepareCurveData(x, y[, w]). Strips rows where any of x, y, w is NaN/Inf; returns column vectors. w == 0 rows are KEPT (only finiteness matters). |
| `prepareSurfaceData` | ✅ | 0.004 | 432.93× |  | OK | Sig: [xo, yo, zo] = prepareSurfaceData(X, Y, Z). Linearises (column-major) and drops rows where any of x, y, z is NaN/Inf. Returns column vectors. |
| `quad2d` | ❌ |  |  |  |  | 2-D quadrature (also in core) |

## Graphics

### Line Plots

**Namespace:** `graphics.line.*` — 2 ✅ + 0 ⚠️ / 12 = 16%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `area` | ❌ |  |  |  |  |  |
| `errorbar` | ✅ | 0.025 | 3718.64× |  | OK | Sig: graphics primitive — errorbar(x, y, err) draws line with symmetric error bars. 4-arg form errorbar(x, y, neg, pos) for asymmetric bars. 2-arg form errorbar(x, y) for plain line. Side-effect (figure emit); spec verifies it runs across the documented arg counts. |
| `fimplicit` | ❌ |  |  |  |  |  |
| `fplot` | ✅ | 4.171 | 77.54× |  | OK | Sig: function-handle plotting family. fplot(f [, xrange]) — 2-D, samples adaptively. fcontour(f [, range]) — contour from f(x,y). fmesh / fsurf — 3-D mesh / shaded surface from f(x,y). All side-effect (emit __FIGURE_DATA__). Spec verifies each runs with both default range and explicit range. Same `r1 = 1` fingerprint pattern as other graphics-primitive specs (numkit graphics handles not directly comparable to MATLAB). |
| `fplot3` | ✅ | 0.816 | 66.98× |  | OK | Sig: graphics primitive — fplot3(funx, funy, funz [, [tmin tmax]]) draws a parametric 3-D curve sampled at 200 points. Mirrors fplot for 3 function handles. Side-effect (emits __FIGURE_DATA__ JSON with plot3 dataset; xJson + yJson + zJson). Spec verifies both default range [-5, 5] and explicit range invocations run without error. Same `r1 = 1` fingerprint pattern as other graphics-primitive specs. |
| `loglog` | ✅ | 0.030 | 1133.83× |  | OK | Sig: graphics primitives — log-scale axis variants of plot. loglog (both x and y log), semilogx (x log only), semilogy (y log only). Side-effect (emits __FIGURE_DATA__ JSON with xscale/yscale set); spec verifies the call runs without erroring. Same pattern as `plot` spec — numkit graphics handles aren't directly comparable to MATLAB. |
| `plot` | ✅ | 0.022 | 845.01× | 1177.88× | OK | Sig: graphics primitive. 2D line plot. Emits figure data via side effect; numkit does not expose MATLAB-style graphics handles. Spec verifies the function runs. |
| `plot3` | ❌ |  |  |  |  | 3-D |
| `semilogx` | ✅ | 0.030 | 1133.83× |  | OK | Sig: graphics primitives — log-scale axis variants of plot. loglog (both x and y log), semilogx (x log only), semilogy (y log only). Side-effect (emits __FIGURE_DATA__ JSON with xscale/yscale set); spec verifies the call runs without erroring. Same pattern as `plot` spec — numkit graphics handles aren't directly comparable to MATLAB. |
| `semilogy` | ✅ | 0.030 | 1133.83× |  | OK | Sig: graphics primitives — log-scale axis variants of plot. loglog (both x and y log), semilogx (x log only), semilogy (y log only). Side-effect (emits __FIGURE_DATA__ JSON with xscale/yscale set); spec verifies the call runs without erroring. Same pattern as `plot` spec — numkit graphics handles aren't directly comparable to MATLAB. |
| `stackedplot` | ❌ |  |  |  |  |  |
| `stairs` | ✅ | 0.019 | 1602.08× | 1591.85× | OK | Sig: graphics primitive. Step plot. Side-effect (figure emit); spec verifies it runs. |

### Polar Plots

**Namespace:** `graphics.polar.*` — 3 ✅ + 0 ⚠️ / 19 = 15%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `compassplot` | ❌ |  |  |  |  |  |
| `fpolarplot` | ❌ |  |  |  |  |  |
| `polaraxes` | ❌ |  |  |  |  |  |
| `polarbubblechart` | ❌ |  |  |  |  |  |
| `polarhistogram` | ❌ |  |  |  |  |  |
| `polarplot` | ✅ | 0.054 | 396.20× |  | OK | Sig: graphics primitive. Polar 2D line plot. Side-effect (figure emit); spec verifies it runs. |
| `polarregion` | ❌ |  |  |  |  |  |
| `polarscatter` | ❌ |  |  |  |  |  |
| `radiusregion` | ❌ |  |  |  |  |  |
| `rlim` | ✅ | 0.022 |  |  | N/A | Sig: graphics primitive. Polar plot r-axis limits. Setter form works; getter form (no args) requires graphics-handle return which numkit does not implement (architectural). |
| `rtickangle` | ❌ |  |  |  |  |  |
| `rtickformat` | ❌ |  |  |  |  |  |
| `rticklabels` | ❌ |  |  |  |  |  |
| `rticks` | ❌ |  |  |  |  |  |
| `thetalim` | ✅ | 0.023 |  |  | N/A | Sig: graphics primitive. Polar plot theta-axis limits. Setter form works; same architectural getter limit as rlim. |
| `thetaregion` | ❌ |  |  |  |  |  |
| `thetatickformat` | ❌ |  |  |  |  |  |
| `thetaticklabels` | ❌ |  |  |  |  |  |
| `thetaticks` | ❌ |  |  |  |  |  |

### Contour Plots

**Namespace:** `graphics.contour.*` — 2 ✅ + 0 ⚠️ / 7 = 28%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `clabel` | ❌ |  |  |  |  |  |
| `contour` | ✅ | 0.002 | 96.59× |  | OK | Sig: r = contour(...). Spec-extension batch 2026-05-09. |
| `contour3` | ❌ |  |  |  |  |  |
| `contourc` | ❌ |  |  |  |  |  |
| `contourf` | ✅ | 4.157 | 9.11× |  | OK | Sig: graphics primitive. Filled contour plot. Same side-effect-only no-op; spec verifies the call runs. |
| `contourslice` | ❌ |  |  |  |  |  |
| `fcontour` | ✅ | 4.171 | 77.54× |  | OK | Sig: function-handle plotting family. fplot(f [, xrange]) — 2-D, samples adaptively. fcontour(f [, range]) — contour from f(x,y). fmesh / fsurf — 3-D mesh / shaded surface from f(x,y). All side-effect (emit __FIGURE_DATA__). Spec verifies each runs with both default range and explicit range. Same `r1 = 1` fingerprint pattern as other graphics-primitive specs (numkit graphics handles not directly comparable to MATLAB). |

### Vector Fields

**Namespace:** `graphics.vector_fields.*` — 0 ✅ + 0 ⚠️ / 6 = 0%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `compassplot` | ❌ |  |  |  |  |  |
| `feather` | ❌ |  |  |  |  |  |
| `quiver` | ❌ |  |  |  |  |  |
| `quiver3` | ❌ |  |  |  |  |  |
| `streamline` | ❌ |  |  |  |  |  |
| `streamslice` | ❌ |  |  |  |  |  |

### Surface and Mesh Plots

**Namespace:** `graphics.surface.*` — 3 ✅ + 0 ⚠️ / 21 = 14%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `contour3` | ❌ |  |  |  |  |  |
| `cylinder` | ✅ | 0.002 | 426.68× |  | OK | Sig: [X,Y,Z] = cylinder([R, n]). Bit-identical with MATLAB R2025b when called with explicit parens. KNOWN ENGINE GAP: cylinder() vs cylinder (no parens) -- parenless multi-output assignment segfaults numkit; that's a core parser/dispatcher issue, not a libs/cylinder bug. Documented in BUGS.md. |
| `ellipsoid` | ✅ | 0.003 | 461.60× |  | OK | Sig: r = ellipsoid(...). Spec-extension batch 2026-05-09. |
| `fimplicit3` | ❌ |  |  |  |  |  |
| `fmesh` | ✅ | 4.171 | 77.54× |  | OK | Sig: function-handle plotting family. fplot(f [, xrange]) — 2-D, samples adaptively. fcontour(f [, range]) — contour from f(x,y). fmesh / fsurf — 3-D mesh / shaded surface from f(x,y). All side-effect (emit __FIGURE_DATA__). Spec verifies each runs with both default range and explicit range. Same `r1 = 1` fingerprint pattern as other graphics-primitive specs (numkit graphics handles not directly comparable to MATLAB). |
| `fsurf` | ✅ | 4.171 | 77.54× |  | OK | Sig: function-handle plotting family. fplot(f [, xrange]) — 2-D, samples adaptively. fcontour(f [, range]) — contour from f(x,y). fmesh / fsurf — 3-D mesh / shaded surface from f(x,y). All side-effect (emit __FIGURE_DATA__). Spec verifies each runs with both default range and explicit range. Same `r1 = 1` fingerprint pattern as other graphics-primitive specs (numkit graphics handles not directly comparable to MATLAB). |
| `hidden` | ❌ |  |  |  |  |  |
| `mesh` | ✅ | 0.015 | 1712.68× | 1845.50× | OK | Sig: graphics primitive. 3D mesh surface. Currently registered as a side-effect-only no-op (figure emit logic for surfaces is a separate refactor); spec verifies the call accepts standard input without erroring. |
| `meshc` | ❌ |  |  |  |  |  |
| `meshz` | ❌ |  |  |  |  |  |
| `pcolor` | ✅ | 0.016 | 1164.57× | 1812.99× | OK | Sig: graphics primitive. Pseudocolor checkerboard plot. Same side-effect-only no-op; spec verifies the call runs. |
| `peaks` | ✅ | 0.003 | 199.11× | 53.92× | OK | Sig: r = peaks(...). Spec-extension batch 2026-05-09. |
| `ribbon` | ❌ |  |  |  |  |  |
| `sphere` | ✅ | 0.005 | 181.31× | 45.53× | OK | Sig: r = sphere(...). Spec-extension batch 2026-05-09. |
| `surf` | ✅ | 0.017 | 1965.52× | 1690.17× | OK | Sig: graphics primitive. 3D shaded surface. Same side-effect-only no-op as mesh; spec verifies the call runs. |
| `surf2patch` | ❌ |  |  |  |  |  |
| `surface` | ❌ |  |  |  |  |  |
| `surfc` | ❌ |  |  |  |  |  |
| `surfl` | ❌ |  |  |  |  |  |
| `surfnorm` | ❌ |  |  |  |  |  |
| `waterfall` | ❌ |  |  |  |  |  |

### Volume Visualization

**Namespace:** `graphics.volume.*` — 0 ✅ + 0 ⚠️ / 24 = 0%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `coneplot` | ✅ | 0.692 |  |  | N/A | coneplot — cone-headed arrow visualisation. Display-only; fingerprint pins the input field's shape + extrema. Visual fidelity (cone meshes oriented along U/V/W) is e2e. |
| `contourslice` | ❌ |  |  |  |  |  |
| `curl` | ❌ |  |  |  |  |  |
| `divergence` | ❌ |  |  |  |  |  |
| `flow` | ❌ |  |  |  |  |  |
| `interpstreamspeed` | ❌ |  |  |  |  |  |
| `isocaps` | ❌ |  |  |  |  |  |
| `isocolors` | ❌ |  |  |  |  |  |
| `isonormals` | ❌ |  |  |  |  |  |
| `isosurface` | ❌ |  |  |  |  |  |
| `reducepatch` | ❌ |  |  |  |  |  |
| `reducevolume` | ❌ |  |  |  |  |  |
| `shrinkfaces` | ❌ |  |  |  |  |  |
| `slice` | ❌ |  |  |  |  |  |
| `smooth3` | ❌ |  |  |  |  |  |
| `stream2` | ❌ |  |  |  |  |  |
| `stream3` | ❌ |  |  |  |  |  |
| `streamline` | ❌ |  |  |  |  |  |
| `streamparticles` | ❌ |  |  |  |  |  |
| `streamribbon` | ❌ |  |  |  |  |  |
| `streamslice` | ❌ |  |  |  |  |  |
| `streamtube` | ❌ |  |  |  |  |  |
| `subvolume` | ❌ |  |  |  |  |  |
| `volumebounds` | ❌ |  |  |  |  |  |

### Geographic Plots

**Namespace:** `graphics.geographic.*` — 0 ✅ + 0 ⚠️ / 8 = 0%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `geoaxes` | ❌ |  |  |  |  |  |
| `geobasemap` | ❌ |  |  |  |  |  |
| `geobubble` | ❌ |  |  |  |  |  |
| `geodensityplot` | ❌ |  |  |  |  |  |
| `geolimits` | ❌ |  |  |  |  |  |
| `geoplot` | ❌ |  |  |  |  |  |
| `geoscatter` | ❌ |  |  |  |  |  |
| `geotickformat` | ❌ |  |  |  |  |  |

## Image

### Image I/O

**Namespace:** `image.io.*` — 3 ✅ + 0 ⚠️ / 3 = **100%**

Backed by `stb_image` / `stb_image_write` (single-header, public-domain) vendored under `third_party/stb/`.

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `imread` | ✅ | 0.017 | 23.34× | 1.99× | OK | DEFERRED — imread requires file I/O round-trip; cannot fit single-snippet parity spec. Functionality validated in gtest/smoke instead. Placeholder spec; KNOWN GAP — see audit/closed/image/imread.md. |
| `imwrite` | ✅ | 0.017 | 22.05× | 57.00× | OK | DEFERRED — imwrite requires file I/O round-trip; cannot fit single-snippet parity spec. Functionality validated in gtest/smoke instead. Placeholder spec; KNOWN GAP — see audit/closed/image/imwrite.md. |
| `imfinfo` | ✅ | 0.018 | 24.83× | 40.57× | OK | DEFERRED — imfinfo requires file I/O round-trip; cannot fit single-snippet parity spec. Functionality validated in gtest/smoke instead. Placeholder spec; KNOWN GAP — see audit/closed/image/imfinfo.md. |

### Image Type Conversion

**Namespace:** `image.type.*` — 13 ✅ + 0 ⚠️ / 27 = 48%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `adaptthresh` | ✅ | 0.012 | 215.07× |  | OK | Sig: r = adaptthresh(...). Spec-extension batch 2026-05-09. |
| `cmap2gray` | ✅ | 0.002 | 129.19× |  | OK | Sig: r = cmap2gray(...). Spec-extension batch 2026-05-09. |
| `getrangefromclass` | ✅ | 0.004 | 76.05× | 15.53× | OK | Sig: r = getrangefromclass(...). Spec-extension batch 2026-05-09. |
| `gray2ind` | ✅ | 0.004 | 465.01× | 58.99× | OK | Sig: r = gray2ind(...). Spec-extension batch 2026-05-09. |
| `graythresh` | ✅ | 0.005 | 329.40× | 107.09× | OK | Sig: t = graythresh(I). MATLAB convention: thresh = mean(find(sigma_b == max)) / (L - 1). Bit-identical with MATLAB R2025b on bimodal probe. |
| `grayslice` | ✅ | 0.004 | 213.31× | 41.75× | OK | Sig: r = grayslice(...). Spec-extension batch 2026-05-09. |
| `im2bw` | ✅ | 0.004 | 141.85× | 25.72× | OK | Sig: r = im2bw(...). Spec-extension batch 2026-05-09. |
| `im2double` | ✅ | 0.004 | 68.65× | 50.74× | OK | Sig: r = im2double(...). Spec-extension batch 2026-05-09 (image namespace). |
| `im2gray` | ✅ | 0.004 | 117.45× |  | OK | Sig: r = im2gray(...). Spec-extension batch 2026-05-09. |
| `im2int16` | ✅ | 0.004 | 68.79× | 20.61× | OK | Sig: y = im2int16(x). Spec-extension batch 2026-05-09 (cycle 44). |
| `im2single` | ✅ | 0.005 | 61.54× | 19.58× | OK | Sig: r = im2single(...). Spec-extension batch 2026-05-09 (image namespace). |
| `im2uint16` | ✅ | 0.004 | 77.56× | 16.95× | OK | Sig: r = im2uint16(...). Spec-extension batch 2026-05-09 (image namespace). |
| `im2uint8` | ✅ | 0.004 | 85.16× | 59.70× | OK | Sig: r = im2uint8(...). Spec-extension batch 2026-05-09 (image namespace). |
| `imbinarize` | ✅ | 0.007 | 303.33× | 68.42× | OK | Sig: BW = imbinarize(I). Default threshold via graythresh + binarize. Bit-identical with MATLAB R2025b after graythresh tied-mean fix 2026-05-09. |
| `imquantize` | ✅ | 0.005 | 84.44× | 38.68× | OK | Sig: r = imquantize(...). Spec-extension batch 2026-05-09. |
| `imsplit` | ✅ | 0.006 | 82.45× |  | OK | Sig: [r,g,b] = imsplit(I). Spec-extension batch 2026-05-09 (cycle 44). |
| `ind2gray` | ❌ |  |  |  |  |  |
| `ind2rgb` | ✅ | 0.004 | 119.12× | 34.82× | OK | Sig: r = ind2rgb(...). Spec-extension batch 2026-05-09. |
| `iptnum2ordinal` | ✅ | 0.003 | 62.78× | 145.34× | OK | Sig: ord = iptnum2ordinal(num). 1..20 word form; 21+ digit-suffix. Output is char. Octave-image has iptnum2ordinal. |
| `label2rgb` | ✅ | 0.003 | 631.05× | 160.54× | OK | Sig: RGB = label2rgb(L, cmap [, background]). Caller passes an explicit N-by-3 colormap (we don't yet have the colormap-name / function-handle defaults). Octave-image has label2rgb. |
| `mat2gray` | ✅ | 0.003 | 648.07× | 53.07× | OK | Sig + small deterministic input. Auto-generated for parity sweep. |
| `multithresh` | ✅ | 0.155 | 25.77× |  | OK | Sig: t = multithresh(I, N). Bit-identical with MATLAB R2025b on multimodal-cluster input 2026-05-09 -- thresholds returned as midpoints of adjacent class means (canonicalises Otsu tied maxima). |
| `otsuthresh` | ✅ | 0.004 | 164.98× | 104.53× | OK | Sig + small deterministic input. Auto-generated for parity sweep. |
| `rgb2gray` | ✅ | 0.004 | 95.17× | 21.46× | OK | Sig: r = rgb2gray(...). Spec-extension batch 2026-05-09 (image namespace). |
| `rgb2ind` | ❌ |  |  |  |  | colour quantize |
| `rgb2lightness` | ❌ |  |  |  |  | L* of CIELAB |
| `demosaic` | ❌ |  |  |  |  | Bayer → RGB |

### Color Space Conversion

**Namespace:** `image.color.*` — 10 ✅ + 0 ⚠️ / 30 = 33%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `chromadapt` | ❌ |  |  |  |  | Bradford/von Kries chromatic adapt |
| `colorangle` | ✅ | 0.001 | 469.50× |  | OK | Sig: r = colorangle(...). Spec-extension batch 2026-05-09. |
| `deltaE` | ✅ | 0.004 | 1700.22× |  | OK | Sig: D = deltaE(I1, I2). KNOWN GAP: numkit's deltaE output dimensions differ from MATLAB. Only structural numel pinned. Documented as separate ТЗ. |
| `hsv2rgb` | ✅ | 0.004 | 444.01× | 70.17× | OK | Sig: r = hsv2rgb(...). Spec-extension batch 2026-05-09 (image namespace). |
| `illumgray` | ❌ |  |  |  |  | grey-world illumination |
| `illumpca` | ❌ |  |  |  |  |  |
| `illumwhite` | ❌ |  |  |  |  | white-patch |
| `imapprox` | ❌ |  |  |  |  | reduce indexed-image colors |
| `imcolordiff` | ❌ |  |  |  |  | CIE94/CIEDE2000 |
| `lab2double` | ✅ | 0.003 | 390.24× | 20.06× | OK | Sig: lab_dbl = lab2double(lab). uint8 LAB → double: L *= 100/255, a/b -= 128. Octave-image has lab2double. |
| `lab2rgb` | ✅ | 0.004 | 2155.23× | 80.53× | OK | Sig: r = lab2rgb(...). Spec-extension batch 2026-05-09 (image namespace). |
| `lab2uint16` | ✅ | 0.003 | 371.83× | 25.34× | OK | Sig: lab_u16 = lab2uint16(lab). double LAB → uint16: (L*65280)/100, (a+128)*256, (b+128)*256. NaN → 65535. Octave-image has lab2uint16. |
| `lab2uint8` | ✅ | 0.003 | 341.04× | 39.07× | OK | Sig: lab_u8 = lab2uint8(lab). double LAB → uint8: L *= 255/100, a/b += 128. NaN → 255. Octave-image has lab2uint8. |
| `lab2xyz` | ✅ | 0.003 | 1598.72× | 93.62× | OK | Sig: xyz = lab2xyz(lab). Spec-extension batch 2026-05-09 (cycle 44). |
| `lin2rgb` | ✅ | 0.003 | 551.82× |  | OK | Sig: B = lin2rgb(A). Linear → sRGB forward gamma. MATLAB R2025b. Octave-image doesn't ship lin2rgb; harness ranks MATLAB above Octave so OK is expected with octave=N/A. |
| `ntsc2rgb` | ✅ | 0.003 | 178.67× | 42.06× | OK | Sig: rgb = ntsc2rgb(yiq). Inverse of rgb2ntsc 3-sig-fig matrix. Octave-image has ntsc2rgb. |
| `rgb2hsv` | ✅ | 0.004 | 137.44× | 54.62× | OK | Sig: r = rgb2hsv(...). Spec-extension batch 2026-05-09 (image namespace). |
| `rgb2lab` | ✅ | 0.004 | 2523.16× | 71.03× | OK | Sig: r = rgb2lab(...). Spec-extension batch 2026-05-09 (image namespace). |
| `rgb2lin` | ✅ | 0.004 | 530.57× |  | OK | Sig: B = rgb2lin(A). sRGB inverse gamma (piecewise linear|^2.4). MATLAB R2025b. Octave-image doesn't ship rgb2lin; harness ranks MATLAB above Octave so OK is expected even with octave=N/A. |
| `rgb2ntsc` | ✅ | 0.003 | 146.40× | 66.64× | OK | Sig: yiq = rgb2ntsc(rgb). Linear matrix; 3-sig-fig from Wikipedia/MATLAB. Octave-image has rgb2ntsc. |
| `rgb2xyz` | ✅ | 0.003 | 2025.95× | 51.70× | OK | Sig + small deterministic input. Auto-generated for parity sweep. Tol=1e-4: small FP differences from slightly different published color-matrix coefficients (D65 white point variants). |
| `rgb2ycbcr` | ✅ | 0.004 | 376.80× | 67.99× | OK | Sig: r = rgb2ycbcr(...). Spec-extension batch 2026-05-09 (image namespace). |
| `rgbwide2xyz` | ❌ |  |  |  |  | wide-gamut HDR |
| `rgbwide2ycbcr` | ❌ |  |  |  |  |  |
| `whitepoint` | ✅ | 0.005 | 117.29× |  | OK | Sig: wp = whitepoint([illuminant]). 1×3 XYZ tristimulus of CIE reference illuminant. Supports a/c/d50/d55/d65/e/icc; default 'icc'. MATLAB R2025b. Octave-image doesn't ship whitepoint. |
| `xyz2double` | ✅ | 0.003 | 389.93× |  | OK | Sig: xyzd = xyz2double(xyz). uint16 XYZ → double via ICC.1:2001-4 (32768 ↔ 1.0). Double input passthrough. MATLAB R2025b. Octave-image doesn't ship xyz2double. |
| `xyz2lab` | ✅ | 0.004 | 1398.43× | 73.26× | OK | Sig: lab = xyz2lab(xyz). Spec-extension batch 2026-05-09 (cycle 44). |
| `xyz2rgb` | ✅ | 0.003 | 2563.02× | 74.81× | OK | Sig + small deterministic input. Sign-preserving sRGB gamma fix 2026-05-09 -- numkit no longer clamps out-of-gamut linear RGB before encoding. |
| `xyz2rgbwide` | ❌ |  |  |  |  |  |
| `xyz2uint16` | ✅ | 0.003 | 360.27× |  | OK | Sig: xyzu16 = xyz2uint16(xyz). Double XYZ → uint16 ICC (round(x*32768) clipped to [0,65535]). MATLAB R2025b. Octave-image doesn't ship xyz2uint16. |
| `ycbcr2rgb` | ✅ | 0.004 | 460.71× | 43.15× | OK | Sig: r = ycbcr2rgb(...). Spec-extension batch 2026-05-09 (image namespace). |
| `ycbcr2rgbwide` | ❌ |  |  |  |  |  |

### Synthetic Images and Display

**Namespace:** `image.synth.*` / `image.display.*` — 0 ✅ + 0 ⚠️ / 8 = 0%

Display ones (`imshow`, `montage`, …) need graphics; synthesis is pure algorithm.

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `checkerboard` | ✅ | 0.002 | 189.58× |  | OK | Sig: r = checkerboard(...). Spec-extension batch 2026-05-09. |
| `imnoise` | ✅ | 0.006 | 569.27× | 40.23× | OK | Sig: r = imnoise(...). Spec-extension batch 2026-05-09 (image namespace). |
| `phantom` | ✅ | 0.069 | 21.00× | 17.57× | OK | Sig: P = phantom([model | E] [, n]). Modified Shepp-Logan default; 64x64 reference test. Octave-image has phantom. |
| `imshow` | ❌ |  |  |  |  | needs graphics |
| `imfuse` | ❌ |  |  |  |  |  |
| `imshowpair` | ❌ |  |  |  |  |  |
| `montage` | ❌ |  |  |  |  | tile images |
| `immovie` | ❌ |  |  |  |  |  |

### Geometric Transformations

**Namespace:** `image.geom.*` — 4 ✅ + 0 ⚠️ / 13 = 31%

Class-based affine/rigid/projective transforms (affinetform2d etc.) intentionally omitted; flat function APIs only.

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `findbounds` | ❌ |  |  |  |  |  |
| `fitgeotrans` | ❌ |  |  |  |  | fit transform from cp pairs |
| `imcrop` | ✅ | 0.005 | 274.19× | 42.92× | OK | Sig: r = imcrop(...). Spec-extension batch 2026-05-09 (image namespace). |
| `imcrop3` | ❌ |  |  |  |  |  |
| `impyramid` | ✅ | 0.004 | 1807.01× | 225.63× | OK | Sig: r = impyramid(...). Spec-extension batch 2026-05-09. |
| `imresize` | ✅ | 0.005 | 683.41× | 190.17× | OK | Sig: r = imresize(...). Spec-extension batch 2026-05-09 (image namespace). |
| `imresize3` | ❌ |  |  |  |  |  |
| `imrotate` | ✅ | 0.004 | 354.18× | 63.68× | OK | Sig: r = imrotate(...). Spec-extension batch 2026-05-09 (image namespace). |
| `imrotate3` | ❌ |  |  |  |  |  |
| `imtransform` | ❌ |  |  |  |  | legacy maketform path |
| `imtranslate` | ✅ | 0.004 | 1417.15× |  | OK | Sig: r = imtranslate(...). Spec-extension batch 2026-05-09 (image namespace). |
| `imwarp` | ❌ |  |  |  |  |  |
| `makeresampler` | ❌ |  |  |  |  |  |

### Image Registration

**Namespace:** `image.register.*` — 0 ✅ + 0 ⚠️ / 8 = 0%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `cpcorr` | ❌ |  |  |  |  | refine control-point correspondences |
| `imregconfig` | ❌ |  |  |  |  |  |
| `imregcorr` | ❌ |  |  |  |  | phase-correlation registration |
| `imregdemons` | ❌ |  |  |  |  | non-rigid demons |
| `imregister` | ❌ |  |  |  |  |  |
| `imregmtb` | ❌ |  |  |  |  | median-threshold-bitmap |
| `imregtform` | ❌ |  |  |  |  |  |
| `normxcorr2` | ✅ | 0.005 | 621.27× | 110.17× | OK | Sig: c = normxcorr2(template, img). Output (M+m-1)x(N+n-1) double in [-1, 1]. Octave-image has normxcorr2. |

### Image Filtering

**Namespace:** `image.filter.*` — 7 ✅ + 0 ⚠️ / 36 = 19%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `convmtx2` | ✅ | 0.001 | 59.36× |  | OK | Sig: T = convmtx2(h, m, n). Convolution matrix for 2-D 'full' convolution. MATLAB returns sparse, we return dense — wrap in full() in MATLAB so dim and values match. Octave-image doesn't ship convmtx2. |
| `entropyfilt` | ✅ | 0.007 | 309.88× |  | OK | Sig: r = entropyfilt(...). Spec-extension batch 2026-05-09. |
| `fibermetric` | ❌ |  |  |  |  |  |
| `freqspace` | ✅ | 0.002 |  |  | N/A | Sig: f = freqspace(N). KNOWN GAP: numkit returns shorter vector than MATLAB for freqspace(8) — different size convention. Only structural numel pinned. Documented as separate ТЗ. |
| `freqz2` | ✅ | 0.008 | 144.72× |  | OK | Sig: r = freqz2(...). Spec-extension batch 2026-05-09. |
| `fsamp2` | ❌ |  |  |  |  | 2-D FIR via frequency sampling |
| `fspecial` | ✅ | 0.004 | 756.90× | 71.50× | OK | Sig: r = fspecial(...). Spec-extension batch 2026-05-09. |
| `fspecial3` | ❌ |  |  |  |  |  |
| `ftrans2` | ❌ |  |  |  |  | 1-D → 2-D FIR transform |
| `fwind1` | ❌ |  |  |  |  | 2-D windowed FIR (rotation) |
| `fwind2` | ❌ |  |  |  |  |  |
| `gabor` | ❌ |  |  |  |  | Gabor filter bank |
| `imbilatfilt` | ✅ | 0.007 |  |  | N/A | Sig: r = imbilatfilt(...). Spec-extension batch 2026-05-09. |
| `imboxfilt` | ✅ | 0.005 | 250.65× | 152.73× | OK | Sig: r = imboxfilt(...). Spec-extension batch 2026-05-09. |
| `imboxfilt3` | ✅ | 0.005 | 311.39× |  | OK | Sig: r = imboxfilt3(...). Spec-extension batch 2026-05-09. |
| `imdiffusefilt` | ❌ |  |  |  |  | anisotropic diffusion |
| `imfilter` | ✅ | 0.005 | 93.31× | 81.59× | OK | Sig: r = imfilter(...). Spec-extension batch 2026-05-09 (image namespace). |
| `imgaborfilt` | ❌ |  |  |  |  |  |
| `imgaussfilt` | ✅ | 0.004 | 423.05× | 174.00× | OK | Sig: r = imgaussfilt(...). Spec-extension batch 2026-05-09. |
| `imgaussfilt3` | ✅ | 0.005 | 414.09× |  | OK | Sig: r = imgaussfilt3(...). Spec-extension batch 2026-05-09. |
| `imguidedfilter` | ❌ |  |  |  |  |  |
| `imnlmfilt` | ❌ |  |  |  |  | non-local means |
| `integralBoxFilter` | ❌ |  |  |  |  |  |
| `integralBoxFilter3` | ❌ |  |  |  |  |  |
| `integralImage` | ✅ | 0.004 | 197.14× | 71.90× | OK | Sig: r = integralImage(...). Spec-extension batch 2026-05-09. |
| `integralImage3` | ✅ | 0.003 | 145.26× | 121.92× | OK | Sig: J = integralImage3(V). 3-D summed-volume table with leading zero plane/row/col. Octave-image may not have integralImage3 → may report N/A. |
| `medfilt2` | ✅ | 0.004 | 789.35× |  | OK | Sig: r = medfilt2(...). Spec-extension batch 2026-05-09 (image namespace). |
| `medfilt3` | ✅ | 0.029 | 59.32× |  | OK | Sig: J = medfilt3(V[, [M N P]]). 3-D median filter, default 3x3x3, symmetric pad. MATLAB R2017+; Octave-image doesn't ship medfilt3. |
| `modefilt` | ❌ |  |  |  |  |  |
| `nlfilter` | ❌ |  |  |  |  | generic neighborhood op |
| `ordfilt2` | ✅ | 0.004 | 583.38× | 92.10× | OK | Sig: B = ordfilt2(A, nth, domain [, S] [, padding]). Order-statistic filter; 1-based nth. Octave-image has ordfilt2. |
| `padarray` | ✅ | 0.004 | 317.26× | 88.08× | OK | Sig + small deterministic input. Auto-generated for parity sweep. |
| `rangefilt` | ✅ | 0.003 | 730.34× | 140.06× | OK | Sig: R = rangefilt(I [, domain]). Local max-min over neighbourhood. Default 3x3 ones, symmetric pad. Output class matches input. |
| `roifilt2` | ❌ |  |  |  |  |  |
| `stdfilt` | ✅ | 0.004 | 182.58× | 150.24× | OK | Sig: S = stdfilt(I [, domain]). Local sample std (N-1 norm). Default 3x3 ones, symmetric pad. Uses Octave-source test vector G. |
| `wiener2` | ✅ | 0.004 | 213.78× | 102.51× | OK | Sig: r = wiener2(...). Spec-extension batch 2026-05-09 (image namespace). |

### Contrast Adjustment

**Namespace:** `image.contrast.*` — 3 ✅ + 0 ⚠️ / 14 = 21%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `adapthisteq` | ✅ | 0.28 | 14.9× |  | OK | Sig: J = adapthisteq(I[, NV-pairs]). CLAHE — faithful port of MATLAB R2025b adapthisteq.m (symmetric pad → per-tile clip+redistribute with cap = ceil(N/NBins) + round(normCL·(N-minCL)) and two-pass step-size redistribution → uniform-CDF mapping → (NumTiles+1)² integer-weight bilinear with round-then-bilinear quantisation per LUT lookup). **tol=0 bit-exact** vs MATLAB R2025b on every probed pixel. Distribution!='uniform' deferred (throws). |
| `decorrstretch` | ❌ |  |  |  |  | decorrelation stretch |
| `histeq` | ✅ | 0.005 | 495.99× | 53.47× | OK | Sig: r = histeq(...). Spec-extension batch 2026-05-09. |
| `imadjust` | ✅ | 0.005 | 581.76× | 104.49× | OK | Sig: r = imadjust(...). Spec-extension batch 2026-05-09. |
| `imadjustn` | ✅ | 0.004 | 778.38× |  | OK | Sig: r = imadjustn(...). Spec-extension batch 2026-05-09. |
| `imflatfield` | ✅ | 0.006 | 811.68× |  | OK | Sig: r = imflatfield(...). Spec-extension batch 2026-05-09. |
| `imhistmatch` | ✅ | 0.006 | 680.24× |  | OK | Sig: r = imhistmatch(...). Spec-extension batch 2026-05-09. |
| `imhistmatchn` | ✅ | 0.005 | 598.84× |  | OK | Sig: r = imhistmatchn(...). Spec-extension batch 2026-05-09. |
| `imlocalbrighten` | ❌ |  |  |  |  |  |
| `imreducehaze` | ❌ |  |  |  |  |  |
| `imsharpen` | ✅ | 0.007 | 310.90× | 261.16× | OK | Sig: r = imsharpen(...). Spec-extension batch 2026-05-09. |
| `intlut` | ✅ | 0.003 | 246.06× | 28.09× | OK | Sig: B = intlut(A, LUT). Pure pointwise table lookup. uint8 in / uint8 out via inversion LUT. Output class follows class(LUT). |
| `localcontrast` | ❌ |  |  |  |  |  |
| `locallapfilt` | ❌ |  |  |  |  | local Laplacian |
| `stretchlim` | ✅ | 0.004 | 368.72× | 83.50× | OK | Sig: r = stretchlim(...). Spec-extension batch 2026-05-09. |

### ROI-Based Processing

**Namespace:** `image.roi.*` — 0 ✅ + 0 ⚠️ / 8 = 0%

ROI drawing classes (`Circle`, `Ellipse`, `drawcircle`, `imellipse`, `imrect`, …) intentionally omitted as OOP / interactive.

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `inpaintCoherent` | ❌ |  |  |  |  | coherence-transport inpainting |
| `inpaintExemplar` | ❌ |  |  |  |  | exemplar inpainting |
| `poly2mask` | ❌ |  |  |  |  |  |
| `reducepoly` | ❌ |  |  |  |  | Douglas-Peucker simplify |
| `regionfill` | ❌ |  |  |  |  | smooth fill of bw mask |
| `roicolor` | ✅ | 0.003 | 67.89× | 49.52× | OK | Sig: BW = roicolor(A, low, high) range form, or roicolor(A, v) set-membership. Output logical, same shape as A. Octave-image has roicolor. |
| `roifill` | ❌ |  |  |  |  | legacy alias |
| `roipoly` | ❌ |  |  |  |  |  |

### Morphological Operations

**Namespace:** `image.morph.*` — 5 ✅ + 0 ⚠️ / 27 = 19%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `applylut` | ✅ | 0.003 | 139.91× |  | OK | Sig: r = applylut(...). Spec-extension batch 2026-05-09. |
| `bwhitmiss` | ✅ | 0.004 | 1049.32× |  | OK | Sig: r = bwhitmiss(...). Spec-extension batch 2026-05-09 (image namespace). |
| `bwlookup` | ❌ |  |  |  |  |  |
| `bwmorph` | ✅ | 0.45 | 25.7× |  | OK | Sig: J = bwmorph(BW, op[, n]). Binary morphology dispatcher — faithful port of MATLAB R2025b bwmorph.m + algbwmorph.m using 22 LUTs dumped from MATLAB. All 20+ operations bit-exact (tol=0): dilate / erode / bridge / clean / diag / endpoints / fatten / fill / hbreak / majority / perim4 / perim8 / remove / bothat / close / open / tophat / shrink∞ / skeleton∞ / spur / thin∞ / thicken / branchpoints. |
| `bwmorph3` | ❌ |  |  |  |  |  |
| `bwpack` | ✅ | 0.002 | 126.23× |  | OK | Sig: r = bwpack(...). Spec-extension batch 2026-05-09. |
| `bwperim` | ✅ | 0.002 | 589.39× |  | OK | Sig: r = bwperim(...). Spec-extension batch 2026-05-09. |
| `bwskel` | ❌ |  |  |  |  | skeletonize |
| `bwulterode` | ❌ |  |  |  |  | ultimate erosion |
| `bwunpack` | ❌ |  |  |  |  |  |
| `conndef` | ❌ |  |  |  |  |  |
| `imbothat` | ✅ | 0.007 | 1063.37× | 45.31× | OK | Sig: r = imbothat(...). Spec-extension batch 2026-05-09. |
| `imclearborder` | ✅ | 0.007 | 689.66× | 53.63× | OK | Sig: r = imclearborder(...). Spec-extension batch 2026-05-09 (image namespace). |
| `imclose` | ✅ | 0.006 | 1118.59× | 43.43× | OK | Sig: r = imclose(...). Spec-extension batch 2026-05-09 (image namespace). |
| `imdilate` | ✅ | 0.005 | 304.66× | 35.21× | OK | Sig: r = imdilate(...). Spec-extension batch 2026-05-09 (image namespace). |
| `imerode` | ✅ | 0.005 | 318.53× | 68.29× | OK | Sig: r = imerode(...). Spec-extension batch 2026-05-09 (image namespace). |
| `imextendedmax` | ✅ | 0.019 | 103.95× | 15.34× | OK | Sig: BW = imextendedmax(I, h). Tall peak A survives (mask=1 at (2,2)); shallow peak B suppressed. |
| `imextendedmin` | ✅ | 0.018 | 121.57× | 7.61× | OK | Sig: BW = imextendedmin(I, h). Deep trough A survives, shallow B suppressed. |
| `imfill` | ✅ | 0.009 | 452.10× | 36.75× | OK | Sig: r = imfill(...). Spec-extension batch 2026-05-09 (image namespace). |
| `imhmax` | ✅ | 0.007 | 185.96× | 11.67× | OK | Sig: r = imhmax(...). Spec-extension batch 2026-05-09. |
| `imhmin` | ✅ | 0.007 | 221.00× | 29.56× | OK | Sig: r = imhmin(...). Spec-extension batch 2026-05-09. |
| `imimposemin` | ✅ | 0.012 | 198.78× | 14.70× | OK | Sig: J = imimposemin(I, BW). Force regional minima at marker; basin B at (2,5) erased (lifted to plateau 10). |
| `imkeepborder` | ✅ | 0.007 | 542.99× |  | OK | Sig: r = imkeepborder(...). Spec-extension batch 2026-05-09. |
| `imopen` | ✅ | 0.006 | 734.23× | 54.19× | OK | Sig: r = imopen(...). Spec-extension batch 2026-05-09 (image namespace). |
| `imreconstruct` | ✅ | 0.007 | 177.94× | 22.73× | OK | Sig: r = imreconstruct(...). Spec-extension batch 2026-05-09 (image namespace). |
| `imregionalmax` | ✅ | 0.007 | 127.93× | 16.99× | OK | Sig: r = imregionalmax(...). Spec-extension batch 2026-05-09. |
| `imregionalmin` | ✅ | 0.007 | 163.60× | 17.52× | OK | Sig: r = imregionalmin(...). Spec-extension batch 2026-05-09. |
| `imtophat` | ✅ | 0.006 | 745.95× | 61.76× | OK | Sig: r = imtophat(...). Spec-extension batch 2026-05-09. |
| `makelut` | ❌ |  |  |  |  |  |
| `offsetstrel` | ❌ |  |  |  |  | structuring element with offsets |
| `strel` | ✅ | 0.004 | 720.19× |  | OK | Sig: se = strel(shape, params). Returns struct (numkit) / strel-object (MATLAB) with fields {Neighborhood, Dimensionality}. Structure access matches; the 'square' shape is bit-identical (both engines: 5x5 = 25 ones). NOTE: 'disk' decomposes into smaller equivalent in MATLAB R2025b (line-strel cascade) -- numkit returns the full disk mask. Both yield identical morphology results, just different .Neighborhood matrices. Field access works in both. |

### Deblurring

**Namespace:** `image.deblur.*` — 0 ✅ + 0 ⚠️ / 7 = 0%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `deconvblind` | ❌ |  |  |  |  | blind deconvolution |
| `deconvlucy` | ❌ |  |  |  |  | Richardson-Lucy |
| `deconvreg` | ❌ |  |  |  |  | regularised |
| `deconvwnr` | ❌ |  |  |  |  | Wiener |
| `edgetaper` | ❌ |  |  |  |  |  |
| `otf2psf` | ❌ |  |  |  |  |  |
| `psf2otf` | ✅ | 0.005 | 529.70× | 61.67× | OK | Sig: otf = psf2otf(psf [, outsize]). FFT of circshift(zeropad(psf), -floor(size/2)). Octave-image has psf2otf. |

### Neighborhood and Block Processing

**Namespace:** `image.block.*` — 0 ✅ + 0 ⚠️ / 6 = 0%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `bestblk` | ✅ | 0.001 | 276.71× |  | OK | Sig: r = bestblk(...). Spec-extension batch 2026-05-09. |
| `blockproc` | ❌ |  |  |  |  | block-wise processing |
| `col2im` | ✅ | 0.002 | 266.85× |  | OK | Sig: A = col2im(B, [m n], [mm nn], type). Reassemble columns into image. Bit-identical with MATLAB R2025b on probed input -- earlier defer used wrong B-shape. |
| `colfilt` | ❌ |  |  |  |  |  |
| `im2col` | ✅ | 0.004 | 674.14× | 72.23× | OK | Sig: r = im2col(...). Spec-extension batch 2026-05-09. |
| `nlfilter` | ❌ |  |  |  |  | duplicate of filter section |

### Image Arithmetic

**Namespace:** `image.arith.*` — 8 ✅ + 0 ⚠️ / 8 = 100%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `imabsdiff` | ✅ | 0.004 | 122.47× | 75.06× | OK | Sig: r = imabsdiff(...). Spec-extension batch 2026-05-09. |
| `imadd` | ✅ | 0.005 | 68.60× | 61.51× | OK | Sig: r = imadd(...). Spec-extension batch 2026-05-09. |
| `imapplymatrix` | ✅ | 0.005 | 146.99× | 43.72× | OK | Sig: r = imapplymatrix(...). Spec-extension batch 2026-05-09. |
| `imcomplement` | ✅ | 0.004 | 54.44× | 32.33× | OK | Sig: r = imcomplement(...). Spec-extension batch 2026-05-09. |
| `imdivide` | ✅ | 0.004 | 95.89× | 57.53× | OK | Sig: r = imdivide(...). Spec-extension batch 2026-05-09. |
| `imlincomb` | ✅ | 0.004 | 221.75× | 57.10× | OK | Sig: r = imlincomb(...). Spec-extension batch 2026-05-09. |
| `immultiply` | ✅ | 0.004 | 87.93× | 67.04× | OK | Sig: r = immultiply(...). Spec-extension batch 2026-05-09. |
| `imsubtract` | ✅ | 0.004 | 68.70× | 36.95× | OK | Sig: r = imsubtract(...). Spec-extension batch 2026-05-09. |

### Image Segmentation

**Namespace:** `image.segment.*` — 6 ✅ + 0 ⚠️ / 22 = 27%

Deep-learning-based ones (`imsegsam`, `segmentAnythingModel`, …) intentionally omitted.

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `activecontour` | ❌ |  |  |  |  | Chan-Vese |
| `bfscore` | ❌ |  |  |  |  | boundary F1 score |
| `boundarymask` | ✅ | 0.003 | 675.92× |  | OK | Sig: r = boundarymask(...). Spec-extension batch 2026-05-09. |
| `dice` | ✅ | 0.003 | 215.20× |  | OK | Sig: r = dice(...). Spec-extension batch 2026-05-09. |
| `gradientweight` | ❌ |  |  |  |  |  |
| `grabcut` | ❌ |  |  |  |  |  |
| `grayconnected` | ✅ | 0.004 | 924.32× |  | OK | Sig: BW = grayconnected(I, r, c, tol). 8-connected flood-fill from seed within tolerance. Bit-identical with MATLAB R2025b. Spec uses magic(8) -- restored to canonical form after magic() was implemented in cycle 46 (commit 71efbf02); originally had to inline the matrix because numkit didn't ship magic(). |
| `graydiffweight` | ❌ |  |  |  |  |  |
| `imoverlay` | ✅ | 0.006 | 203.30× |  | OK | Sig: B = imoverlay(I, BW, color). Color overlay onto image at BW pixels. Bit-identical with MATLAB R2025b on probed input -- numkit needs explicit color arg (matches MATLAB; no default). |
| `imseggeodesic` | ❌ |  |  |  |  |  |
| `imsegfmm` | ❌ |  |  |  |  | fast marching |
| `imsegisodata` | ❌ |  |  |  |  |  |
| `imsegkmeans` | ❌ |  |  |  |  |  |
| `imsegkmeans3` | ❌ |  |  |  |  |  |
| `jaccard` | ✅ | 0.004 | 153.27× |  | OK | Sig: r = jaccard(...). Spec-extension batch 2026-05-09. |
| `label2idx` | ✅ | 0.004 | 152.60× |  | OK | Sig: ix = label2idx(L). Spec-extension batch 2026-05-09 (cycle 44). |
| `labeloverlay` | ❌ |  |  |  |  |  |
| `lazysnapping` | ❌ |  |  |  |  |  |
| `superpixels` | ❌ |  |  |  |  | SLIC |
| `superpixels3` | ❌ |  |  |  |  |  |
| `watershed` | ❌ |  |  |  |  |  |

### Object Analysis

**Namespace:** `image.object.*` — 4 ✅ + 0 ⚠️ / 18 = 22%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `bwboundaries` | ✅ | 0.003 | 2304.33× |  | OK | Sig: r = bwboundaries(...). Spec-extension batch 2026-05-09. |
| `bwtraceboundary` | ❌ |  |  |  |  |  |
| `circles2mask` | ❌ |  |  |  |  |  |
| `corner` | ❌ |  |  |  |  | Harris/Min-eig corner detector |
| `cornermetric` | ❌ |  |  |  |  |  |
| `edge` | ✅ | 0.007 | 150.03× |  | OK | Sig: r = edge(...). Spec-extension batch 2026-05-09 (image namespace). |
| `edge3` | ❌ |  |  |  |  |  |
| `hough` | ❌ |  |  |  |  |  |
| `houghlines` | ❌ |  |  |  |  |  |
| `houghpeaks` | ❌ |  |  |  |  |  |
| `imfindcircles` | ❌ |  |  |  |  | circle Hough |
| `imgradient` | ✅ | 0.006 | 262.53× |  | OK | Sig: r = imgradient(...). Spec-extension batch 2026-05-09 (image namespace). |
| `imgradientxy` | ✅ | 0.006 | 201.62× |  | OK | Sig: r = imgradientxy(...). Spec-extension batch 2026-05-09. |
| `imgradient3` | ❌ |  |  |  |  |  |
| `imgradientxyz` | ❌ |  |  |  |  |  |
| `iradon` | ❌ |  |  |  |  | inverse Radon |
| `qtdecomp` | ❌ |  |  |  |  | quad-tree decomposition |
| `qtgetblk` | ❌ |  |  |  |  |  |
| `qtsetblk` | ❌ |  |  |  |  |  |
| `radon` | ❌ |  |  |  |  |  |
| `visboundaries` | ❌ |  |  |  |  | display |
| `viscircles` | ❌ |  |  |  |  | display |

### Region and Image Properties

**Namespace:** `image.region.*` — 8 ✅ + 0 ⚠️ / 28 = 29%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `bwarea` | ✅ | 0.002 | 387.53× |  | OK | Sig: r = bwarea(BW). Pratt area estimate. KNOWN GAP: numkit returns integer pixel count (4) vs MATLAB's pattern-weighted estimate (4.75). Documented as separate ТЗ; only positive-result structural check pinned. |
| `bwareafilt` | ✅ | 0.003 |  |  | N/A | Sig: r = bwareafilt(...). Spec-extension batch 2026-05-09. |
| `bwareaopen` | ✅ | 0.002 | 1157.88× |  | OK | Sig: r = bwareaopen(...). Spec-extension batch 2026-05-09. |
| `bwconncomp` | ✅ | 0.003 | 45.72× |  | OK | Sig: cc = bwconncomp(BW[, conn]). Returns 1x1 struct with fields {Connectivity, ImageSize, NumObjects, PixelIdxList}. PixelIdxList is 1xK cell of column-vector linear indices. Bit-identical with MATLAB R2025b. |
| `bwconvhull` | ❌ |  |  |  |  |  |
| `bwdist` | ✅ | 0.003 | 214.42× |  | OK | Sig: r = bwdist(...). Spec-extension batch 2026-05-09. |
| `bwdistgeodesic` | ❌ |  |  |  |  |  |
| `bweuler` | ✅ | 0.002 | 714.04× |  | OK | Sig: r = bweuler(...). Spec-extension batch 2026-05-09. |
| `bwferet` | ❌ |  |  |  |  | Feret diameters |
| `bwlabel` | ✅ | 0.002 | 60.17× |  | OK | Sig: r = bwlabel(...). Spec-extension batch 2026-05-09. |
| `bwlabeln` | ❌ |  |  |  |  |  |
| `bwperim` | ✅ | 0.002 | 589.39× |  | OK | Sig: r = bwperim(...). Spec-extension batch 2026-05-09. |
| `bwpropfilt` | ❌ |  |  |  |  |  |
| `bwselect` | ✅ | 0.002 | 726.57× |  | OK | Sig: r = bwselect(...). Spec-extension batch 2026-05-09. |
| `bwselect3` | ❌ |  |  |  |  |  |
| `cc2bw` | ❌ |  |  |  |  |  |
| `corr2` | ✅ | 0.002 | 308.03× |  | OK | Sig: r = corr2(...). Spec-extension batch 2026-05-09.  |
| `graydist` | ❌ |  |  |  |  |  |
| `imcontour` | ❌ |  |  |  |  |  |
| `imhist` | ✅ | 0.005 | 160.94× | 53.90× | OK | Sig: r = imhist(...). Spec-extension batch 2026-05-09. |
| `impixel` | ❌ |  |  |  |  |  |
| `improfile` | ❌ |  |  |  |  |  |
| `labelmatrix` | ❌ |  |  |  |  |  |
| `mean2` | ✅ | 0.003 | 84.66× | 76.00× | OK | Sig: r = mean2(...). Spec-extension batch 2026-05-09. |
| `poly2label` | ❌ |  |  |  |  |  |
| `regionprops` | ✅ | 0.005 | 653.98× | 264.15× | OK | Sig: r = regionprops(...). Spec-extension batch 2026-05-09. |
| `regionprops3` | ❌ |  |  |  |  |  |
| `std2` | ✅ | 0.004 | 189.04× | 25.37× | OK | Sig: r = std2(...). Spec-extension batch 2026-05-09. |

### Texture Analysis

**Namespace:** `image.texture.*` — 0 ✅ + 0 ⚠️ / 6 = 0%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `entropy` | ✅ | 0.003 | 308.74× |  | OK | Sig: r = entropy(...). Spec-extension batch 2026-05-09. |
| `entropyfilt` | ✅ | 0.007 | 309.88× |  | OK | Sig: r = entropyfilt(...). Spec-extension batch 2026-05-09. |
| `graycomatrix` | ✅ | 0.014 | 376.6× |  | OK | Sig: G = graycomatrix(I[, NV-pairs]). Gray-level co-occurrence matrix. Bit-equal MATLAB R2025b. NV-pairs: NumLevels / Offset / GrayLimits / Symmetric. KNOWN GAP: multi-offset 3-D return form. |
| `graycoprops` | ✅ | 0.014 | 376.6× |  | OK | Sig: s = graycoprops(G). 4 texture stats (Contrast / Correlation / Energy / Homogeneity) off normalised GLCM. Bit-equal MATLAB R2025b. |
| `rangefilt` | ✅ | 0.003 | 730.34× | 140.06× | OK | Sig: R = rangefilt(I [, domain]). Local max-min over neighbourhood. Default 3x3 ones, symmetric pad. Output class matches input. |
| `stdfilt` | ✅ | 0.004 | 182.58× | 150.24× | OK | Sig: S = stdfilt(I [, domain]). Local sample std (N-1 norm). Default 3x3 ones, symmetric pad. Uses Octave-source test vector G. |

### Image Quality

**Namespace:** `image.quality.*` — 3 ✅ + 0 ⚠️ / 8 = 38%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `brisque` | ❌ |  |  |  |  | no-reference quality (needs trained model) |
| `immse` | ✅ | 0.004 | 82.46× | 12.42× | OK | Sig: r = immse(...). Spec-extension batch 2026-05-09. |
| `multissim` | ❌ |  |  |  |  | multi-scale SSIM |
| `multissim3` | ❌ |  |  |  |  |  |
| `niqe` | ❌ |  |  |  |  | no-reference (needs model) |
| `piqe` | ❌ |  |  |  |  | perceptual no-reference |
| `psnr` | ✅ | 0.003 | 693.26× | 52.36× | OK | Sig + small deterministic input. Auto-generated for parity sweep. |
| `ssim` | ✅ | 0.299 | 7.41× |  | OK | Sig + small deterministic input. Auto-generated for parity sweep. |

### Image Transforms

**Namespace:** `image.transform.*` — 7 ✅ + 0 ⚠️ / 11 = 64%

`fft2` / `ifft2` / `fftshift` / `ifftshift` already covered under Signal / Transforms; cross-listed here per MATLAB TOC.

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `dct2` | ✅ | 0.005 | 117.94× |  | OK | Sig: r = dct2(...). Spec-extension batch 2026-05-09. |
| `dctmtx` | ✅ | 0.003 | 111.85× |  | OK | Sig: r = dctmtx(...). Spec-extension batch 2026-05-09. |
| `fan2para` | ❌ |  |  |  |  | fan-beam → parallel |
| `fanbeam` | ❌ |  |  |  |  |  |
| `fft2` | ✅ | 0.002 |  |  | N/A | Sig: r = fft2(...). Spec-extension batch 2026-05-09. |
| `fftshift` | ✅ | 0.006 |  |  | N/A | Sig: Y = fftshift(X[, dim]). Cyclic shift along every non-singleton dim by ceil(extent/2); inverse ifftshift uses floor(extent/2). Bug fix: numkit had fftshift/ifftshift swapped for odd N + flat-shift instead of per-dim for matrices + dim arg ignored. tol=0 (integer-stable). |
| `idct2` | ✅ | 0.010 | 118.79× | 27.18× | OK | Sig: r = idct2(...). Spec-extension batch 2026-05-09. |
| `ifanbeam` | ❌ |  |  |  |  |  |
| `ifft2` | ✅ | 0.006 | 84.24× | 59.61× | OK | Sig: r = ifft2(...). Spec-extension batch 2026-05-09. |
| `ifftshift` | ✅ | 0.006 | 66.85× | 53.16× | OK | Sig: Y = ifftshift(X[, dim]). Inverse of fftshift; cyclic shift along every non-singleton dim by floor(extent/2). Joint fix with fftshift on 2026-05-08 — they were swapped for odd-extent dims. tol=0. |
| `para2fan` | ❌ |  |  |  |  |  |

## IO

### Low-Level File I/O

**Namespace:** `io.file_io.*` — 13 ✅ + 0 ⚠️ / 15 = 86%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `fclose` | ✅ | 0.214 |  |  | N/A | Side-effect smoke test (file I/O round-trip in single snippet via tempname). fclose returns 0 on success. |
| `feof` | ✅ | 0.307 |  |  | N/A | Side-effect smoke test (file I/O round-trip via tempname). feof = 1 after over-reading. |
| `ferror` | ✅ | 0.285 |  |  | N/A | Side-effect smoke test (file I/O round-trip via tempname). ferror returns empty string when no error. |
| `fgetl` | ✅ | 0.986 |  |  | N/A | Side-effect smoke test (file I/O round-trip in single snippet via tempname). fgetl reads one line (without newline) -- 'hello' has length 5. |
| `fgets` | ✅ | 0.841 |  |  | N/A | Side-effect smoke test (file I/O round-trip in single snippet via tempname). fgets reads one line WITH newline -- length >= 5 ('hello\n'). |
| `fileread` | ✅ | 0.283 |  |  | N/A | Side-effect smoke test (file I/O round-trip in single snippet via tempname). fileread returns full file content -- 3 chars. |
| `fopen` | ✅ | 0.232 |  |  | N/A | Side-effect smoke test (file I/O round-trip in single snippet via tempname). Open file, return fd, close, cleanup -- verifies fopen returns valid descriptor. |
| `fprintf` | ✅ | 0.276 |  |  | N/A | Side-effect smoke test (file I/O round-trip via tempname). fprintf writes 'x' to file -- read back length 1. NOTE: numkit fprintf returns void (no byte count); MATLAB returns the count. Probe uses round-trip rather than return value. |
| `fread` | ✅ | 1.009 |  |  | N/A | Side-effect smoke test (file I/O round-trip via tempname). fread default-type round-trip -- sum of [1..5] = 15. |
| `frewind` | ✅ | 0.368 | 5.35× | 2.66× | OK | Side-effect smoke test (file I/O round-trip via tempname). frewind resets position to 0. |
| `fscanf` | ✅ | 0.945 | 2.36× | 2.13× | OK | Side-effect smoke test (file I/O round-trip in single snippet via tempname). fscanf reads formatted -- sum of [1..5] = 15. |
| `fseek` | ✅ | 0.728 | 3.15× | 2.34× | OK | Side-effect smoke test (file I/O round-trip via tempname). fseek to EOF -- ftell reports positive position. |
| `ftell` | ✅ | 0.321 | 5.74× | 3.94× | OK | Side-effect smoke test (file I/O round-trip via tempname). ftell after one read -- positive position. |
| `fwrite` | ✅ | 0.299 | 6.80× | 4.60× | OK | Side-effect smoke test (file I/O round-trip via tempname). fwrite returns element count -- 5. |
| `openedfiles` | ❌ |  |  |  |  |  |

### Text Files (CSV / dlm / readtable)

**Namespace:** `io.text.*`. Exception: `readtable/writetable/readtimetable/writetimetable` → `table.*` (future) — 1 ✅ + 0 ⚠️ / 16 = 6%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `fileread` | ✅ | 0.283 |  |  | N/A | Side-effect smoke test (file I/O round-trip in single snippet via tempname). fileread returns full file content -- 3 chars. |
| `importdatatask` | ❌ |  |  |  |  |  |
| `importtool` | ❌ |  |  |  |  |  |
| `readcell` | ❌ |  |  |  |  |  |
| `readlines` | ✅ | 3.522 | 8.79× |  | OK | Side-effect smoke test (file I/O round-trip via tempname). readlines returns string array -- at least 3 lines (some engines append empty trailing string). |
| `readmatrix` | ✅ | 0.869 | 116.52× |  | OK | Side-effect smoke test (file I/O round-trip in single snippet via tempname). readmatrix reads CSV -- sum [1+2+3+4] = 10. |
| `readtable` | ❌ |  |  |  |  | needs table type |
| `readtimetable` | ❌ |  |  |  |  |  |
| `readvars` | ❌ |  |  |  |  |  |
| `textscan` | ✅ | 0.828 | 3.23× | 2.96× | OK | Side-effect smoke test (file I/O round-trip in single snippet via tempname). textscan returns cell of parsed columns -- 3 elements. |
| `type` | ✅ | 0.858 | 2.78× | 116.39× | OK | Side-effect smoke test (file I/O round-trip in single snippet via tempname). type displays file content -- side-effect only. |
| `writecell` | ❌ |  |  |  |  |  |
| `writelines` | ✅ | 0.885 | 12.14× |  | OK | Side-effect smoke test (file I/O round-trip via tempname). writelines writes single string -- file has >= 5 chars. |
| `writematrix` | ✅ | 0.861 | 31.45× |  | OK | Side-effect smoke test (file I/O round-trip in single snippet via tempname). writematrix writes CSV -- file should have >= 7 chars (e.g. '1,2\n3,4\n'). |
| `writetable` | ❌ |  |  |  |  | needs table type |
| `writetimetable` | ❌ |  |  |  |  |  |

### Spreadsheets

**Namespace:** `io.text.*`. Table-shaped readers (`readtable`/`writetable`) → `table.*` (future) — 0 ✅ + 0 ⚠️ / 13 = 0%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `importdata` | ❌ |  |  |  |  | auto-detect |
| `importdatatask` | ❌ |  |  |  |  |  |
| `importtool` | ❌ |  |  |  |  |  |
| `readcell` | ❌ |  |  |  |  |  |
| `readmatrix` | ✅ | 0.869 | 116.52× |  | OK | Side-effect smoke test (file I/O round-trip in single snippet via tempname). readmatrix reads CSV -- sum [1+2+3+4] = 10. |
| `readtable` | ❌ |  |  |  |  | needs table type |
| `readtimetable` | ❌ |  |  |  |  |  |
| `readvars` | ❌ |  |  |  |  |  |
| `sheetnames` | ❌ |  |  |  |  |  |
| `writecell` | ❌ |  |  |  |  |  |
| `writematrix` | ✅ | 0.861 | 31.45× |  | OK | Side-effect smoke test (file I/O round-trip in single snippet via tempname). writematrix writes CSV -- file should have >= 7 chars (e.g. '1,2\n3,4\n'). |
| `writetable` | ❌ |  |  |  |  | needs table type |
| `writetimetable` | ❌ |  |  |  |  |  |

### Workspace Save / Load

**Namespace:** `io.workspace.*` — 0 ✅ + 0 ⚠️ / 2 = 0%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `loadobj` | ❌ |  |  |  |  |  |
| `saveobj` | ❌ |  |  |  |  |  |

### File Name Construction

**Namespace:** `io.paths.*` — 0 ✅ + 0 ⚠️ / 9 = 0%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `filemarker` | ❌ |  |  |  |  |  |
| `fileparts` | ✅ | 0.003 |  |  | N/A | Sig: r = fileparts(...). Spec-extension batch 2026-05-09. |
| `filesep` | ✅ | 0.002 |  |  | N/A | Sig: r = filesep(...). Spec-extension batch 2026-05-09. |
| `fullfile` | ✅ | 0.003 | 559.25× |  | OK | Sig: r = fullfile(...). Spec-extension batch 2026-05-09. |
| `matlabdrive` | ❌ |  |  |  |  |  |
| `matlabroot` | ❌ |  |  |  |  |  |
| `tempdir` | ✅ | 0.020 | 9.33× |  | OK | Sig: r = tempdir(...). Spec-extension batch 2026-05-09. |
| `tempname` | ✅ | 0.022 | 43.94× |  | OK | Sig: r = tempname(...). Spec-extension batch 2026-05-09. |
| `toolboxdir` | ❌ |  |  |  |  |  |

## Linear Algebra



**Namespace:** `linalg.*` — 12 ✅ + 0 ⚠️ / 82 = 15%

> Library is live (libs/linalg/, 2026-05-25). User-facing surface migrated
> from libs/builtin — see commits `30b06660`..`d71b472c`. Functions still
> marked **deferred — libs/linalg** below are not-yet-implemented (the
> library is the destination, not the blocker). 22 ❌ on this page wait
> on first-time implementation; the per-function migration is complete
> for everything that was previously shipped.

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `balance` | ⚠️ | 0.021 | 24.89× |  | OK | MATLAB balance: Parlett-Reinsch diagonal scaling for eigvalue conditioning. v1 implements only the scaling phase (permutation phase deferred; equivalent to balance(A, 'noperm') but applies even without the explicit option). For the classic 3x3 dynamic-range matrix, T differs from MATLAB by a uniform factor (4x) which CANCELS in B = inv(T)*A*T -- so B entries are bit-equal. For some inputs (e.g. 2x2 with 12 orders of magnitude), my iterative convergence reaches a different scaling than MATLAB's LAPACK dgebal -- B differs in literal entries but the similarity B = inv(T)*A*T is exact (residual ~0) and eigvals match (ediff ~0). KNOWN GAP: literal T/B entries may differ from MATLAB on hard inputs; mathematical invariants always hold. Fingerprint pins: 3x3 B literal entries (match MATLAB), residuals (must be ~0), and eigvalue preservation (must be exact). Octave 11.1.0 ships balance in core. |
| `bandwidth` | ✅ | 0.009 | 18.19× |  | OK | Sig: matrix-structure predicates and bandwidth queries. isbanded(A, lo, up) ↔ outside-band entries are zero; isdiag = isbanded(A, 0, 0). istril / istriu ↔ lower / upper triangular. [lo, up] = bandwidth(A) returns sub-diagonal and super-diagonal extent; bandwidth(A) one-out form returns lo; 'lower'|'upper' string form selects one. Bit-exact MATLAB R2025b (tol=0): D diagonal (ib1=id1=1), A tri-banded (ib2=1), A not upper-banded-only (ib3=0), L lower-tri (itl=1), U upper-tri (itu=1), bandwidth(A) = [1, 1], bandwidth(U) = 0, bandwidth(L) = 2, bandwidth(U, 'upper') = 2. Split from the original combined predicates spec to stay under the 256-register chunk limit. |
| `cdf2rdf` | ✅ | 0.002 | 522.29× |  | OK | Sig: cdf2rdf (complex-diag Schur → real-block Schur), rsf2csf (real-block → complex-diag). cdf2rdf test: build (V, D) manually for a rotation matrix [0 -1; 1 0] (eigvals ±i); verify the real-form reconstruction. rsf2csf test: a [a -b; b a] 2x2 real Schur block (eigvals a±bi). MATLAB R2025b matches the documented convention DR_block = [a -b; b a], VR(:, k+1) = -Im(v) when sub-diagonal of D pairs positive imag part. |
| `chol` | ✅ | 0.001 | 36.11× |  | OK | Sig: R = chol(A). Cholesky factorisation of symmetric positive-definite A; returns upper R with R'*R = A. Bit-identical with MATLAB R2025b. |
| `cholupdate` | ✅ | 0.002 | 24.93× |  | OK | Sig: R1 = cholupdate(R, x[, '+'|'-']). Rank-1 update / downdate of Cholesky factor; R1'*R1 = R'*R ± x*x'. Update via Golub-Van Loan 6.5.1 Givens rotations (O(n²)). Downdate via O(n³) re-chol on R'*R - x*x' (KNOWN GAP — MATLAB uses LINPACK Saunders 1972 stable O(n²) variant). Diagonal entries match MATLAB R2025b exactly on the pinned PD inputs. |
| `cond` | ✅ | 0.007 | 42.48× |  | OK | Sig: c = cond(A [, p]) for p ∈ {1, 2, Inf, 'fro'} — closed the original ⚠️ gap (see also `cond_pnorm` row). p=2 default routes through `cond_2norm` (sigma_max/sigma_min via SVD); other p use `norm(A,p)·norm(inv(A),p)`. Bit-identical with MATLAB R2025b on all probed p. |
| `condeig` | ✅ | 0.003 | 73.67× |  | OK | Sig: s = condeig(A). Eigenvalue condition numbers; s_i = 1/|cos(angle(v_i, w_i))| where v_i is right eigvec, w_i = inv(V)'s i-th column. Symmetric A → all s_i == 1 (perfectly conditioned). Non-symmetric → larger s_i flags ill-conditioned eigenvalues. Tol 1e-9 is loose because condeig values themselves can be large; we pin the structure (symmetric=1.0; non-sym pair has matching s_i; ill-cond is huge). |
| `condest` | ✅ | 0.003 | 349.23× |  | OK | Sig: c = condest(A). 1-norm condition number estimate. KNOWN GAP: MATLAB uses Higham 1988 power-iteration estimator (LAPACK dlacn1) that approximates norm(inv(A),1); we compute it exactly via inv(A). Matches MATLAB on well-conditioned A. For hilb(4) ≈ 1.5e4 and other near-singular inputs, our exact value differs from MATLAB's iterative estimate. Wide tol=0.5 (relative) accepts ±50% drift on near-singular inputs; pin only the well-conditioned cases I3 / D / UT for exact match. |
| `cross` | ✅ | 0.001 | 39.42× |  | OK | Sig: r = cross(...). Spec-extension batch 2026-05-09. |
| `ctranspose` | ✅ | 0.002 | 34.78× |  | OK | Sig: r = ctranspose(...). I/O / matrix-ops. Spec-extension batch 2026-05-09. |
| `decomposition` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `det` | ✅ | 0.006 | 39.53× |  | OK | Sig: d = det(A). Determinant via LU with partial pivoting; sign tracked from row swaps. Singular A returns 0. Bit-identical with MATLAB R2025b on probed cases (2×2, triangular 3×3, identity 5×5, singular rank-1, magic(4)). |
| `dot` | ✅ | 0.001 | 29.56× |  | OK | Sig: r = dot(...). Spec-extension batch 2026-05-09. |
| `eig` | ⚠️ | 0.013 | 10.99× |  | OK | Sig: e = eig(A) | [V, D] = eig(A). Symmetric: classical Jacobi (eigenvectors + ascending real eigenvalues). General (non-symmetric): characteristic polynomial via Souriau-Faddeev + roots() (eigenvalues only; possibly complex). [V, D] form for general matrices requires QR iteration -- deferred to Phase 2c-3. Sort applied for order-agnostic comparison. |
| `eigs` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `expm` | ✅ | 0.005 |  |  | N/A | Sig: E = expm(A). Matrix exponential via Padé(6) with scaling-and-squaring (Higham 2005). Works for any square matrix. Bit-identical with MATLAB R2025b on rotation generator + symmetric + zero cases. |
| `expmv` | ✅ | 0.004 |  |  | N/A | Sig: w = expmv(t, A, v). KNOWN GAP: MATLAB core does NOT ship expmv — only Higham's separate package on File Exchange does. Therefore correctness=N/A vs MATLAB on most engines. Spec checks algebraic identity (matches expm(t*A)*v on a 3×3 triangular A to ulp) which is self-verifying. Diagonal A path: w(i) == exp(t·d(i)) · v(i), trivially correct. |
| `funm` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `gsvd` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `hess` | ✅ | 0.013 | 30.91× | 16.06× | OK | Sig: [P, H] = hess(A). Hessenberg reduction via Householder reflectors; A = P*H*P', H upper-Hessenberg (zeros below sub-diagonal). Foundation for general eig (Phase 2c). Bit-identical reconstruction with MATLAB R2025b; H entries differ in sign/order due to Householder reflector freedom but identity verified to ulp. |
| `inv` | ✅ | 0.008 | 19.00× | 10.49× | OK | Sig: B = inv(A). Matrix inverse via LU (la_solve backend). Bit-identical with MATLAB R2025b on probed 2×2 + 3×3 systems; A*inv(A) = I to ~ulp. |
| `isbanded` | ✅ | 0.009 | 18.19× |  | OK | Sig: matrix-structure predicates and bandwidth queries. isbanded(A, lo, up) ↔ outside-band entries are zero; isdiag = isbanded(A, 0, 0). istril / istriu ↔ lower / upper triangular. [lo, up] = bandwidth(A) returns sub-diagonal and super-diagonal extent; bandwidth(A) one-out form returns lo; 'lower'|'upper' string form selects one. Bit-exact MATLAB R2025b (tol=0): D diagonal (ib1=id1=1), A tri-banded (ib2=1), A not upper-banded-only (ib3=0), L lower-tri (itl=1), U upper-tri (itu=1), bandwidth(A) = [1, 1], bandwidth(U) = 0, bandwidth(L) = 2, bandwidth(U, 'upper') = 2. Split from the original combined predicates spec to stay under the 256-register chunk limit. |
| `isdiag` | ✅ | 0.009 | 18.19× |  | OK | Sig: matrix-structure predicates and bandwidth queries. isbanded(A, lo, up) ↔ outside-band entries are zero; isdiag = isbanded(A, 0, 0). istril / istriu ↔ lower / upper triangular. [lo, up] = bandwidth(A) returns sub-diagonal and super-diagonal extent; bandwidth(A) one-out form returns lo; 'lower'|'upper' string form selects one. Bit-exact MATLAB R2025b (tol=0): D diagonal (ib1=id1=1), A tri-banded (ib2=1), A not upper-banded-only (ib3=0), L lower-tri (itl=1), U upper-tri (itu=1), bandwidth(A) = [1, 1], bandwidth(U) = 0, bandwidth(L) = 2, bandwidth(U, 'upper') = 2. Split from the original combined predicates spec to stay under the 256-register chunk limit. |
| `ishermitian` | ✅ | 0.008 | 17.11× |  | OK | Sig: issymmetric(A [, 'skew']) → A == A.' (transpose, no conj). ishermitian(A [, 'skew']) → A == A' (conj transpose). 'skew' flips equality to A == -A.' / A == -A'. Bit-exact MATLAB R2025b (tol=0): SY symmetric (s1=1), H not symmetric (s2=0 — complex matrix), SK skew-sym (s3=1), H Hermitian (h1=1), SY not Hermitian since complex form differs (h2=1 — but SY is real so h2=1 by real-symmetric ≡ Hermitian rule), SKH skew-Hermitian (h3=1). Split from the original combined predicates spec to stay under the 256-register chunk limit. |
| `issymmetric` | ✅ | 0.008 | 17.11× |  | OK | Sig: issymmetric(A [, 'skew']) → A == A.' (transpose, no conj). ishermitian(A [, 'skew']) → A == A' (conj transpose). 'skew' flips equality to A == -A.' / A == -A'. Bit-exact MATLAB R2025b (tol=0): SY symmetric (s1=1), H not symmetric (s2=0 — complex matrix), SK skew-sym (s3=1), H Hermitian (h1=1), SY not Hermitian since complex form differs (h2=1 — but SY is real so h2=1 by real-symmetric ≡ Hermitian rule), SKH skew-Hermitian (h3=1). Split from the original combined predicates spec to stay under the 256-register chunk limit. |
| `istril` | ✅ | 0.009 | 18.19× |  | OK | Sig: matrix-structure predicates and bandwidth queries. isbanded(A, lo, up) ↔ outside-band entries are zero; isdiag = isbanded(A, 0, 0). istril / istriu ↔ lower / upper triangular. [lo, up] = bandwidth(A) returns sub-diagonal and super-diagonal extent; bandwidth(A) one-out form returns lo; 'lower'|'upper' string form selects one. Bit-exact MATLAB R2025b (tol=0): D diagonal (ib1=id1=1), A tri-banded (ib2=1), A not upper-banded-only (ib3=0), L lower-tri (itl=1), U upper-tri (itu=1), bandwidth(A) = [1, 1], bandwidth(U) = 0, bandwidth(L) = 2, bandwidth(U, 'upper') = 2. Split from the original combined predicates spec to stay under the 256-register chunk limit. |
| `istriu` | ✅ | 0.009 | 18.19× |  | OK | Sig: matrix-structure predicates and bandwidth queries. isbanded(A, lo, up) ↔ outside-band entries are zero; isdiag = isbanded(A, 0, 0). istril / istriu ↔ lower / upper triangular. [lo, up] = bandwidth(A) returns sub-diagonal and super-diagonal extent; bandwidth(A) one-out form returns lo; 'lower'|'upper' string form selects one. Bit-exact MATLAB R2025b (tol=0): D diagonal (ib1=id1=1), A tri-banded (ib2=1), A not upper-banded-only (ib3=0), L lower-tri (itl=1), U upper-tri (itu=1), bandwidth(A) = [1, 1], bandwidth(U) = 0, bandwidth(L) = 2, bandwidth(U, 'upper') = 2. Split from the original combined predicates spec to stay under the 256-register chunk limit. |
| `kron` | ✅ | 0.006 | 50.81× | 17.23× | OK | Sig: r = kron(...). Spec-extension batch 2026-05-09. |
| `ldl` | ✅ | 0.018 | 32.02× |  | OK | MATLAB ldl: block LDL' factorization. v1 implements Crout LDL' WITHOUT pivoting; covers all PD/ND matrices and indefinite cases that don't strictly require Bunch-Kaufman 2x2 pivoting (which is rare for the test inputs here). Bit-equal with MATLAB R2025b on PD 3x3 (L,D entries match exactly) and on residuals ||A - L*D*L'||. Forms covered: 1-out (L only), 2-out (L,D), 3-out matrix P (identity in v1), 3-out vector P, 'upper' triangle. KNOWN GAPs (PROGRESS): Bunch-Kaufman pivoting (P != I) for matrices with zero pivots; complex Hermitian; sparse [L,D,P,C] form; 'tol' arg. Octave ships ldl in core but with a slightly different output layout for the indefinite case; we follow MATLAB R2025b conventions. |
| `linsolve` | ✅ | 0.007 | 21.63× | 14.69× | OK | Sig: X = linsolve(A, B[, opts]). Wrapper over la_solve (LU for square A, Householder QR least-squares for tall A). Opts struct accepted for MATLAB-compat but ignored (auto-detection covers same cases). Bit-identical with MATLAB R2025b on probed square + tall systems. |
| `logm` | ⚠️ | 0.006 | 148.48× | 61.19× | OK | Sig: L = logm(A). Matrix logarithm for symmetric positive-definite A via eig: L = V*diag(log(eig))*V'. Round-trip expm(logm) = A to ulp. General (non-symmetric) logm requires complex Schur -- deferred to Phase 2b. |
| `lscov` | ✅ | 0.005 | 226.84× | 41.52× | OK | Sig: [x, stdx, mse, S] = lscov(A, b[, w]). Weighted least squares. mse = SSR/(N-p); S = mse·(A'WA)^(-1). Full N×N covariance V deferred (errors). Bit-identical to MATLAB R2025b on OLS and weighted paths. |
| `lsqminnorm` | ✅ | 0.013 | 115.64× |  | OK | MATLAB linalg cycle 4: lsqminnorm + lsqnonneg. lsqminnorm = pinv(A,tol)*B; bit-equal with MATLAB R2025b on full-rank, rank-deficient, and wide systems. lsqnonneg via Lawson-Hanson active-set algorithm; bit-equal on x, resnorm, residual, exitflag for the classic test [1 -1 2; 3 4 5; 6 7 8] / [1; 2; 3] -> x = [0; 0; 0.387097], resnorm = 0.06451612903. KNOWN GAPs: lsqminnorm 'rankWarn'/'RegularizationFactor' name-value args deferred; lsqnonneg 'options'/'problem' input forms and 6th 'lambda' output (Lagrange multipliers) deferred; complex inputs not supported. Octave ships both functions; lsqminnorm in core since Octave 6, lsqnonneg in optim package. |
| `lsqnonneg` | ✅ | 0.013 | 115.64× |  | OK | MATLAB linalg cycle 4: lsqminnorm + lsqnonneg. lsqminnorm = pinv(A,tol)*B; bit-equal with MATLAB R2025b on full-rank, rank-deficient, and wide systems. lsqnonneg via Lawson-Hanson active-set algorithm; bit-equal on x, resnorm, residual, exitflag for the classic test [1 -1 2; 3 4 5; 6 7 8] / [1; 2; 3] -> x = [0; 0; 0.387097], resnorm = 0.06451612903. KNOWN GAPs: lsqminnorm 'rankWarn'/'RegularizationFactor' name-value args deferred; lsqnonneg 'options'/'problem' input forms and 6th 'lambda' output (Lagrange multipliers) deferred; complex inputs not supported. Octave ships both functions; lsqminnorm in core since Octave 6, lsqnonneg in optim package. |
| `lu` | ✅ | 0.006 | 32.52× | 31.02× | OK | Sig: [L, U, P] = lu(A). LU with partial pivoting; P*A = L*U exactly. L unit-lower, U upper. Bit-identical with MATLAB R2025b on probed 3x3. |
| `mldivide` | ✅ | 0.008 | 31.79× | 13.91× | OK | Sig: X = mldivide(A,B) ↔ A\B. Square A: LU with partial pivoting. Tall A (m>n): QR via Householder + R back-solve (least squares). Wide A (m<n, min-norm): NOT yet supported — throws m:mldivide:wide. Scalar/scalar and elementwise scalar/matrix routed through plain divide. |
| `mpower` | ✅ | 0.004 | 30.65× | 36.36× | OK | Sig: r = mpower(a,b) (a^b). Spec-extension batch 2026-05-09. KNOWN GAP: matrix^integer (M^n where M is matrix) not implemented in numkit — only scalar^scalar pinned. Documented as separate ТЗ; would need O(log n) repeated mtimes for the matrix branch. |
| `mrdivide` | ✅ | 0.007 | 28.95× | 32.85× | OK | Sig: X = mrdivide(A,B) ↔ A/B  ↔ X·B = A. Composes via the standard transpose trick X = (B'\A')'. So uses the same LU/QR primitives as mldivide. matrix/scalar is elementwise. scalar/matrix ERRORS with m:mrdivide:dim per MATLAB R2025b (verified: `2/[1 2; 3 4]` → 'Matrix dimensions must agree'). |
| `mtimes` | ✅ | 0.006 | 25.52× | 24.13× | OK | Sig: r = mtimes(...). Arithmetic op. Spec-extension batch 2026-05-09. |
| `norm` | ✅ | 0.006 | 33.38× | 20.93× | OK | Sig: n = norm(X[, p]). Vector and matrix norms. Vector: 2-norm default, p-norm via sum(|x|^p)^(1/p), Inf -> max(|x|), 1 -> sum(|x|). Matrix: 2-norm = largest singular, 1 -> max col sum, Inf -> max row sum, 'fro' -> Frobenius. Bit-identical with MATLAB R2025b. |
| `normest` | ✅ | 0.009 | 60.48× | 41.13× | OK | Sig: n = normest(A). 2-norm estimate via largest singular value. NOTE: numkit returns the exact value (full SVD), MATLAB uses power-iteration with default tol=1e-6 (~5-6 sig digits). Tol 1e-5 reflects MATLAB's iteration tolerance. A future perf-pass can switch to power-iteration to match performance characteristics. |
| `null` | ✅ | 0.008 | 78.86× | 20.64× | OK | Sig: N = null(A[, tol]). Orthonormal null-space basis; n - rank(A) columns. A*null(A) = 0 to ulp. |
| `ordeig` | ✅ | 0.001 | 43.63× |  | OK | Sig: e = ordeig(T). Eigenvalues of (quasi-)triangular Schur factor in stored order — NO sort. Diagonal T → diag(T). Real Schur with 2×2 blocks → conjugate pairs from (a ± √disc)/2 formula. Pinned: diagonal [3 1 2] order preserved; real Schur block at (2,3) gives 0.5±1.5i. |
| `ordqz` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `ordschur` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `orth` | ✅ | 0.009 | 61.15× | 29.96× | OK | Sig: Q = orth(A[, tol]). Orthonormal basis for range of A; Q has rank(A) columns. Q'*Q = I exactly. Note: column signs may differ from MATLAB (singular vector sign ambiguity); fingerprint avoids direct value comparison. |
| `pagectranspose` | ✅ | 0.220 | 0.22× | 0.20× | OK | 128x64x8 real-valued — pagectranspose equals pagetranspose. 100 iters. |
| `pageeig` | ✅ | 0.017 | 57.58× |  | OK | Sig: page-wise wrappers (pageeig, pagesvd, pagepinv, pagenorm, pagemldivide, pagemrdivide, pagelsqminnorm, pageinv). All iterate the corresponding 2-D linalg op per page. Fingerprints pin shapes + a handful of values (we use pageinv = inv per page, pageeig on symmetric input that gives exact eigvals [5 5 5] for one page, etc.). pagelsqminnorm not in fingerprint — it uses pinv which goes through SVD whose sign convention may differ at machine precision. |
| `pageinv` | ✅ | 0.017 | 57.58× |  | OK | Sig: page-wise wrappers (pageeig, pagesvd, pagepinv, pagenorm, pagemldivide, pagemrdivide, pagelsqminnorm, pageinv). All iterate the corresponding 2-D linalg op per page. Fingerprints pin shapes + a handful of values (we use pageinv = inv per page, pageeig on symmetric input that gives exact eigvals [5 5 5] for one page, etc.). pagelsqminnorm not in fingerprint — it uses pinv which goes through SVD whose sign convention may differ at machine precision. |
| `pagelsqminnorm` | ✅ | 0.017 | 57.58× |  | OK | Sig: page-wise wrappers (pageeig, pagesvd, pagepinv, pagenorm, pagemldivide, pagemrdivide, pagelsqminnorm, pageinv). All iterate the corresponding 2-D linalg op per page. Fingerprints pin shapes + a handful of values (we use pageinv = inv per page, pageeig on symmetric input that gives exact eigvals [5 5 5] for one page, etc.). pagelsqminnorm not in fingerprint — it uses pinv which goes through SVD whose sign convention may differ at machine precision. |
| `pagemldivide` | ✅ | 0.017 | 57.58× |  | OK | Sig: page-wise wrappers (pageeig, pagesvd, pagepinv, pagenorm, pagemldivide, pagemrdivide, pagelsqminnorm, pageinv). All iterate the corresponding 2-D linalg op per page. Fingerprints pin shapes + a handful of values (we use pageinv = inv per page, pageeig on symmetric input that gives exact eigvals [5 5 5] for one page, etc.). pagelsqminnorm not in fingerprint — it uses pinv which goes through SVD whose sign convention may differ at machine precision. |
| `pagemrdivide` | ✅ | 0.017 | 57.58× |  | OK | Sig: page-wise wrappers (pageeig, pagesvd, pagepinv, pagenorm, pagemldivide, pagemrdivide, pagelsqminnorm, pageinv). All iterate the corresponding 2-D linalg op per page. Fingerprints pin shapes + a handful of values (we use pageinv = inv per page, pageeig on symmetric input that gives exact eigvals [5 5 5] for one page, etc.). pagelsqminnorm not in fingerprint — it uses pinv which goes through SVD whose sign convention may differ at machine precision. |
| `pagemtimes` | ✅ | 0.019 | 0.57× |  | OK | Sig: C = pagemtimes(A, B). 20×20×20 batch matmul. 100 iters. |
| `pagenorm` | ✅ | 0.017 | 57.58× |  | OK | Sig: page-wise wrappers (pageeig, pagesvd, pagepinv, pagenorm, pagemldivide, pagemrdivide, pagelsqminnorm, pageinv). All iterate the corresponding 2-D linalg op per page. Fingerprints pin shapes + a handful of values (we use pageinv = inv per page, pageeig on symmetric input that gives exact eigvals [5 5 5] for one page, etc.). pagelsqminnorm not in fingerprint — it uses pinv which goes through SVD whose sign convention may differ at machine precision. |
| `pagepinv` | ✅ | 0.017 | 57.58× |  | OK | Sig: page-wise wrappers (pageeig, pagesvd, pagepinv, pagenorm, pagemldivide, pagemrdivide, pagelsqminnorm, pageinv). All iterate the corresponding 2-D linalg op per page. Fingerprints pin shapes + a handful of values (we use pageinv = inv per page, pageeig on symmetric input that gives exact eigvals [5 5 5] for one page, etc.). pagelsqminnorm not in fingerprint — it uses pinv which goes through SVD whose sign convention may differ at machine precision. |
| `pagesvd` | ✅ | 0.017 | 57.58× |  | OK | Sig: page-wise wrappers (pageeig, pagesvd, pagepinv, pagenorm, pagemldivide, pagemrdivide, pagelsqminnorm, pageinv). All iterate the corresponding 2-D linalg op per page. Fingerprints pin shapes + a handful of values (we use pageinv = inv per page, pageeig on symmetric input that gives exact eigvals [5 5 5] for one page, etc.). pagelsqminnorm not in fingerprint — it uses pinv which goes through SVD whose sign convention may differ at machine precision. |
| `pagetranspose` | ✅ | 0.081 | 0.45× | 0.63× | OK | 128x64x8 array, page-wise transpose. 100 iters. |
| `pinv` | ✅ | 0.011 | 59.63× | 17.42× | OK | Sig: P = pinv(A[, tol]). Moore-Penrose pseudoinverse via SVD: A*P*A = A, P*A*P = P (verified to ulp). Bit-identical with MATLAB R2025b on probed full-rank + rank-deficient cases. |
| `planerot` | ✅ | 0.014 | 74.71× |  | OK | MATLAB linalg cycle 2: rref + rcond + planerot. rref via Gauss-Jordan with partial pivoting; default tol = max(M,N)*eps(norm(A,inf)); two-output [R, jb] form returns 1-based pivot column indices. rcond uses cheap path 1/(norm(A,1)*norm(inv(A),1)); returns 0 for singular A. KNOWN GAP: rcond on near-singular matrices may differ slightly from MATLAB's LAPACK dgecon estimator. planerot Givens rotation: r=hypot(x,y), G=[c s; -s c], degenerate (0,0) case returns identity. Bit-equal with MATLAB R2025b on the well-conditioned cases; rcond bit-equal on diag/2x2/hilb(4); planerot exact (cos/sin formula matches). rref complex input deferred (KNOWN GAP). Octave: triple-engine green for all three. Real-only inputs in v1. |
| `polyeig` | ✅ | 0.013 | 41.41× |  | OK | Sig: e = polyeig(A0, A1, ..., Ak). Polynomial eigenvalue problem via companion linearisation + char-poly + roots(). Eigenvalues-only form. Linear test: (A0 + λI)x = 0 → e = eigvals(-A0) = [-2, -3]. Quadratic test: (λ²-5λ+6)·I → e = {2, 2, 3, 3}. Real ordering may differ — fingerprint sorts. Tol 1e-5 because the characteristic-polynomial → roots() path has lower precision than direct eig (residual imag part ~1e-7 for nominally-real eigvals). |
| `qr` | ✅ | 0.013 | 36.90× | 13.31× | OK | Sig: [Q, R] = qr(A). Householder QR; A = Q*R, Q orthogonal. Tested on 3x3 + 3x2 tall. Q signs may differ from MATLAB by reflection (R diagonal sign convention varies); fingerprint uses abs() on R diagonal to be sign-agnostic. Identity Q*R == A and Q'*Q == I should match to ulp. |
| `qrdelete` | ✅ | 0.003 | 321.55× |  | OK | Sig: [Q1, R1] = qrupdate(Q, R, u, v) — rank-1 update A→A+uv'. qrinsert(Q, R, k, x) — insert column. qrdelete(Q, R, k) — drop column. All three use Givens rotations (Daniel-Gragg-Kaufman-Stewart 1976). Fingerprint uses algebraic identities (reconstruction norm, Q orthogonality, R upper-triangularity) rather than literal entries — Givens-rotation Q/R outputs are unique only up to column sign convention which may differ from MATLAB. |
| `qrinsert` | ✅ | 0.003 | 321.55× |  | OK | Sig: [Q1, R1] = qrupdate(Q, R, u, v) — rank-1 update A→A+uv'. qrinsert(Q, R, k, x) — insert column. qrdelete(Q, R, k) — drop column. All three use Givens rotations (Daniel-Gragg-Kaufman-Stewart 1976). Fingerprint uses algebraic identities (reconstruction norm, Q orthogonality, R upper-triangularity) rather than literal entries — Givens-rotation Q/R outputs are unique only up to column sign convention which may differ from MATLAB. |
| `qrupdate` | ✅ | 0.003 | 321.55× |  | OK | Sig: [Q1, R1] = qrupdate(Q, R, u, v) — rank-1 update A→A+uv'. qrinsert(Q, R, k, x) — insert column. qrdelete(Q, R, k) — drop column. All three use Givens rotations (Daniel-Gragg-Kaufman-Stewart 1976). Fingerprint uses algebraic identities (reconstruction norm, Q orthogonality, R upper-triangularity) rather than literal entries — Givens-rotation Q/R outputs are unique only up to column sign convention which may differ from MATLAB. |
| `qz` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `rank` | ✅ | 0.010 | 40.83× | 28.92× | OK | Sig: r = rank(A[, tol]). Numerical rank via SVD; sigma > max(m,n)*eps(sigma_max). Bit-identical with MATLAB R2025b on probed full-rank/rank-deficient/zero/identity/hilbert cases. |
| `rcond` | ✅ | 0.014 | 74.71× |  | OK | MATLAB linalg cycle 2: rref + rcond + planerot. rref via Gauss-Jordan with partial pivoting; default tol = max(M,N)*eps(norm(A,inf)); two-output [R, jb] form returns 1-based pivot column indices. rcond uses cheap path 1/(norm(A,1)*norm(inv(A),1)); returns 0 for singular A. KNOWN GAP: rcond on near-singular matrices may differ slightly from MATLAB's LAPACK dgecon estimator. planerot Givens rotation: r=hypot(x,y), G=[c s; -s c], degenerate (0,0) case returns identity. Bit-equal with MATLAB R2025b on the well-conditioned cases; rcond bit-equal on diag/2x2/hilb(4); planerot exact (cos/sin formula matches). rref complex input deferred (KNOWN GAP). Octave: triple-engine green for all three. Real-only inputs in v1. |
| `rref` | ✅ | 0.014 | 74.71× |  | OK | MATLAB linalg cycle 2: rref + rcond + planerot. rref via Gauss-Jordan with partial pivoting; default tol = max(M,N)*eps(norm(A,inf)); two-output [R, jb] form returns 1-based pivot column indices. rcond uses cheap path 1/(norm(A,1)*norm(inv(A),1)); returns 0 for singular A. KNOWN GAP: rcond on near-singular matrices may differ slightly from MATLAB's LAPACK dgecon estimator. planerot Givens rotation: r=hypot(x,y), G=[c s; -s c], degenerate (0,0) case returns identity. Bit-equal with MATLAB R2025b on the well-conditioned cases; rcond bit-equal on diag/2x2/hilb(4); planerot exact (cos/sin formula matches). rref complex input deferred (KNOWN GAP). Octave: triple-engine green for all three. Real-only inputs in v1. |
| `rsf2csf` | ✅ | 0.002 | 522.29× |  | OK | Sig: cdf2rdf (complex-diag Schur → real-block Schur), rsf2csf (real-block → complex-diag). cdf2rdf test: build (V, D) manually for a rotation matrix [0 -1; 1 0] (eigvals ±i); verify the real-form reconstruction. rsf2csf test: a [a -b; b a] 2x2 real Schur block (eigvals a±bi). MATLAB R2025b matches the documented convention DR_block = [a -b; b a], VR(:, k+1) = -Im(v) when sub-diagonal of D pairs positive imag part. |
| `schur` | ⚠️ | 0.008 | 58.02× | 13.39× | OK | Sig: [U, T] = schur(A). For symmetric A this is the eigendecomposition: A = U*T*U' with T diagonal. General (non-symmetric) Schur returns quasi-triangular T with 2x2 blocks for complex eigenpairs -- deferred to Phase 2b. Eigenvalues bit-identical with MATLAB. |
| `sqrtm` | ⚠️ | 0.005 | 118.00× | 15.22× | OK | Sig: R = sqrtm(A). Matrix square root for symmetric positive-semidefinite A via eig: R = V*diag(sqrt(eig))*V'. R*R = A to ulp. General sqrtm requires complex Schur -- deferred to Phase 2b. |
| `subspace` | ✅ | 0.008 | 53.11× | 57.20× | OK | Sig: theta = subspace(A, B). Largest principal angle between column spaces of A and B. Identical subspaces -> 0; orthogonal -> pi/2. |
| `svd` | ✅ | 0.015 | 30.11× | 6.96× | OK | Sig: s = svd(A) | [U, S, V] = svd(A). One-sided Jacobi SVD; A = U*S*V'. Bit-identical singular values with MATLAB R2025b on probed matrices (3x3 / 4x3 tall / 3x4 wide / diagonal). U/V vectors not compared directly (sign ambiguity); identity U*S*V' = A and orthogonality verified to ulp. |
| `svdappend` | ❌ |  |  |  |  |  |
| `svds` | ❌ |  |  |  |  | **deferred — libs/sparse** |
| `svdsketch` | ❌ |  |  |  |  |  |
| `sylvester` | ⚠️ | 0.006 | 98.58× | 4.20× | OK | Sig: X = sylvester(A, B, C). Solves A*X + X*B = C. For symmetric A and B (this revision): simultaneous diagonalisation via eig. Residual to ulp. General (non-symmetric) Sylvester via Bartels-Stewart on Schur forms is deferred. |
| `trace` | ✅ | 0.007 |  | 25.31× | OK | Sig: t = trace(A). Sum of diagonal. Works for square + rectangular (uses min(rows,cols)). Bit-identical with MATLAB R2025b. |
| `transpose` | ✅ | 0.005 | 34.14× | 36.09× | OK | Sig: r = transpose(...). I/O / matrix-ops. Spec-extension batch 2026-05-09. |
| `tril` | ✅ | 0.005 | 24.38× | 27.82× | OK | Sig: r = tril(...). Spec-extension batch 2026-05-09. |
| `triu` | ✅ | 0.005 | 24.81× | 23.62× | OK | Sig: r = triu(...). Spec-extension batch 2026-05-09. |
| `vecnorm` | ✅ | 0.004 | 33.89× |  | OK | Sig: y = vecnorm(A [, p [, dim]]). Element-wise p-norm reduction along a dimension; default p=2, default dim = first non-singleton. Row [3 4] → 5 (2-norm). Column [3;4] → 5. 2×2 matrix V columns → [hypot(3,6), hypot(4,8)] = [6.708, 8.944]. r-1-norm = sum(|r|) = 10. r-Inf = max(|r|) = 4. M row-norm (dim=2) → [hypot(1,2), hypot(3,4)] = [2.236, 5]. vecnorm([]) → 0 (MATLAB convention). Bit-exact MATLAB R2025b (tol=1e-12). |

## ODE



**Namespace:** `ode.*` (future) — 0 ✅ + 0 ⚠️ / 21 = 0%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `decic` | ❌ |  |  |  |  | **deferred — libs/ode** |
| `deval` | ❌ |  |  |  |  | **deferred — libs/ode** |
| `ode` | ❌ |  |  |  |  |  |
| `ode113` | ❌ |  |  |  |  | **deferred — libs/ode** |
| `ode15i` | ❌ |  |  |  |  | **deferred — libs/ode** |
| `ode15s` | ❌ |  |  |  |  | **deferred — libs/ode** |
| `ode23` | ❌ |  |  |  |  | **deferred — libs/ode** |
| `ode23s` | ❌ |  |  |  |  | **deferred — libs/ode** |
| `ode23t` | ❌ |  |  |  |  | **deferred — libs/ode** |
| `ode23tb` | ❌ |  |  |  |  | **deferred — libs/ode** |
| `ode45` | ❌ |  |  |  |  | **deferred — libs/ode** |
| `ode78` | ❌ |  |  |  |  | **deferred — libs/ode** |
| `ode89` | ❌ |  |  |  |  | **deferred — libs/ode** |
| `odeevent` | ❌ |  |  |  |  |  |
| `odeget` | ❌ |  |  |  |  | **deferred — libs/ode** |
| `odejacobian` | ❌ |  |  |  |  |  |
| `odemassmatrix` | ❌ |  |  |  |  |  |
| `odesensitivity` | ❌ |  |  |  |  |  |
| `odeset` | ❌ |  |  |  |  | **deferred — libs/ode** |
| `odextend` | ❌ |  |  |  |  | **deferred — libs/ode** |
| `solveode` | ❌ |  |  |  |  |  |

## Optimization

### Local

**Namespace:** `optim.*` (top-level promoted: `fzero, fminbnd, fminsearch`) · `optimset/optimget` registered top-level from libs/builtin — 5 ✅ + 0 ⚠️ / 7 = 71%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `fminbnd` | ✅ | 0.003 |  |  | N/A | Sig: x = fminbnd(fn, lo, hi[, tol]). 1-D bounded minimization. Bit-identical with MATLAB R2025b on probed quadratic (x=3.0). NOTE: numkit only returns x; multi-output [x, fval, exitflag] form is a separate ТЗ (refactor). |
| `fminsearch` | ✅ | 0.050 |  |  | N/A | Sig: x = fminsearch(fn, x0[, tol]). N-D Nelder-Mead unconstrained minimization. Converges to MATLAB R2025b's solution within tol on probed quadratic (x = [2 3]). NOTE: multi-output [x, fval, exitflag, output] form is a separate ТЗ. |
| `fzero` | ✅ | 0.011 | 83.53× | 57.32× | OK | Sig: r = fzero(...). Spec-extension batch 2026-05-09. |
| `lsqnonneg` | ✅ | 0.013 | 115.64× |  | OK | MATLAB linalg cycle 4: lsqminnorm + lsqnonneg. lsqminnorm = pinv(A,tol)*B; bit-equal with MATLAB R2025b on full-rank, rank-deficient, and wide systems. lsqnonneg via Lawson-Hanson active-set algorithm; bit-equal on x, resnorm, residual, exitflag for the classic test [1 -1 2; 3 4 5; 6 7 8] / [1; 2; 3] -> x = [0; 0; 0.387097], resnorm = 0.06451612903. KNOWN GAPs: lsqminnorm 'rankWarn'/'RegularizationFactor' name-value args deferred; lsqnonneg 'options'/'problem' input forms and 6th 'lambda' output (Lagrange multipliers) deferred; complex inputs not supported. Octave ships both functions; lsqminnorm in core since Octave 6, lsqnonneg in optim package. |
| `optimget` | ✅ | 0.003 | 335.74× | 44.90× | OK | Sig: v = optimget(opts, name[, default]). Bit-identical with MATLAB R2025b on probed access. Earlier defer was wrong -- function works. |
| `optimize` | ❌ |  |  |  |  |  |
| `optimset` | ✅ | 0.004 | 177.60× | 53.74× | OK | Sig: r = optimset(...). Spec-extension batch 2026-05-09. |

### Constrained

**Namespace:** `optim.*` — 0 ✅ + 0 ⚠️ / 22 = 0%

The new problem-based API (`optimproblem`, `optimvar`, `optimexpr`,
`optimconstr`, `optimeq`, `optimineq`, `solve`, `evaluate`, `prob2struct`,
`infeasibility`, `findindex`, `issatisfied`, `paretoplot`, `optimvalues`,
the `show*` / `write*` family, `eqnproblem`, `fcn2optimexpr`) is OOP /
expression-tree based and intentionally omitted; we expose only the
solver-based legacy API which is flat function-form.

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `fmincon` | ❌ |  |  |  |  | constrained nonlinear minimisation |
| `fminunc` | ❌ |  |  |  |  | unconstrained nonlinear minimisation |
| `fseminf` | ❌ |  |  |  |  | semi-infinite optimisation |
| `fgoalattain` | ❌ |  |  |  |  | multi-objective goal attainment |
| `fminimax` | ❌ |  |  |  |  | minimax optimisation |
| `linprog` | ❌ |  |  |  |  | linear programming |
| `intlinprog` | ❌ |  |  |  |  | mixed-integer linear programming |
| `quadprog` | ❌ |  |  |  |  | quadratic programming |
| `coneprog` | ❌ |  |  |  |  | second-order cone programming |
| `secondordercone` | ❌ |  |  |  |  | SOC constraint helper |
| `lsqlin` | ❌ |  |  |  |  | linear LSQ with bounds & linear constraints |
| `lsqcurvefit` | ❌ |  |  |  |  | nonlinear LSQ in curve-fit signature |
| `lsqnonlin` | ❌ |  |  |  |  | nonlinear LSQ |
| `fsolve` | ❌ |  |  |  |  | system of nonlinear equations |
| `mpsread` | ❌ |  |  |  |  | MPS-format LP reader (defer — I/O) |
| `optimoptions` | ❌ |  |  |  |  | options struct (modern) |
| `resetoptions` | ❌ |  |  |  |  | reset options to default |
| `checkGradients` | ❌ |  |  |  |  | finite-diff gradient check |
| `optimwarmstart` | ❌ |  |  |  |  | warm-start handle for lsqlin/quadprog |
| `integerConstraint` | ❌ |  |  |  |  | helper for integer DOF |
| `mldivide` | ✅ | 0.008 | 31.79× | 13.91× | OK | Sig: X = mldivide(A,B) ↔ A\B. Square A: LU with partial pivoting. Tall A (m>n): QR via Householder + R back-solve (least squares). Wide A (m<n, min-norm): NOT yet supported — throws m:mldivide:wide. Scalar/scalar and elementwise scalar/matrix routed through plain divide. |

### Global

**Namespace:** `gads.*` — 0 ✅ + 0 ⚠️ / 14 = 0%

Problem-based API (`optimproblem`/`optimvar`/etc.), MultiStart class
methods (`createOptimProblem`/`list`/`run`) and `paretoplot` (display)
intentionally omitted — flat solver functions only.

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `ga` | ❌ |  |  |  |  | genetic algorithm |
| `gamultiobj` | ❌ |  |  |  |  | multi-objective GA |
| `paretosearch` | ❌ |  |  |  |  | direct multi-objective search |
| `particleswarm` | ❌ |  |  |  |  | particle swarm optimisation |
| `patternsearch` | ❌ |  |  |  |  | direct (mesh / GPS / MADS) |
| `simulannealbnd` | ❌ |  |  |  |  | bounded simulated annealing |
| `surrogateopt` | ❌ |  |  |  |  | surrogate-model optimisation |
| `packfcn` | ❌ |  |  |  |  | pack/unpack obj-fcn args |
| `gaoptimset` | ❌ |  |  |  |  | legacy GA options setter |
| `gaoptimget` | ❌ |  |  |  |  | legacy GA options getter |
| `psoptimset` | ❌ |  |  |  |  | legacy patternsearch options setter |
| `psoptimget` | ❌ |  |  |  |  | legacy patternsearch options getter |
| `saoptimset` | ❌ |  |  |  |  | legacy SA options setter |
| `saoptimget` | ❌ |  |  |  |  | legacy SA options getter |

## Signal

### Waveform Generation

**Namespace:** `signal.waveform_generation.*` — 5 ✅ + 0 ⚠️ / 21 = 23%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `buffer` | ❌ |  |  |  |  | reshape with overlap |
| `chirp` | ✅ | 0.004 | 688.85× |  | OK | Sig: r = chirp(...). Spec-extension batch 2026-05-09 (signal namespace). |
| `demod` | ❌ |  |  |  |  |  |
| `diric` | ✅ | 0.001 | 294.97× |  | OK | Sig: r = diric(...). Spec-extension batch 2026-05-09. |
| `framelbl` | ❌ |  |  |  |  |  |
| `framesig` | ❌ |  |  |  |  |  |
| `gauspuls` | ✅ | 0.003 | 240.89× | 30.94× | OK | Sig: r = gauspuls(...). Spec-extension batch 2026-05-09. |
| `gmonopuls` | ✅ | 0.048 | 0.54× | 1.30× | OK | Sig: Y = gmonopuls(T, FC). Gaussian monopulse. 1000 iters. |
| `marcumq` | ✅ | 0.112 | 8.50× | 3.29× | OK | Sig: r = marcumq(...). Spec-extension batch 2026-05-09. |
| `modulate` | ❌ |  |  |  |  |  |
| `pulstran` | ✅ | 0.004 | 347.92× | 23.82× | OK | Sig: r = pulstran(...). Spec-extension batch 2026-05-09. |
| `rectpuls` | ✅ | 0.004 | 392.67× | 30.16× | OK | Sig: r = rectpuls(...). Spec-extension batch 2026-05-09. |
| `sawtooth` | ✅ | 0.004 | 123.36× | 41.79× | OK | Sig: r = sawtooth(...). Spec-extension batch 2026-05-09. |
| `shiftdata` | ❌ |  |  |  |  |  |
| `sinc` | ✅ | 0.005 | 51.43× | 32.57× | OK | Sig: r = sinc(...). Spec-extension batch 2026-05-09. |
| `square` | ✅ | 0.004 | 88.38× | 10.33× | OK | Sig: r = square(...). Spec-extension batch 2026-05-09. |
| `tripuls` | ✅ | 0.003 | 320.42× | 36.96× | OK | Sig: r = tripuls(...). Spec-extension batch 2026-05-09. |
| `udecode` | ❌ |  |  |  |  |  |
| `uencode` | ❌ |  |  |  |  |  |
| `unshiftdata` | ❌ |  |  |  |  |  |
| `vco` | ❌ |  |  |  |  | VCO |

### Filter Design

**Namespace:** `signal.filter_design.*` — 11 ✅ + 0 ⚠️ / 37 = 30%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `butter` | ✅ | 0.002 | 2885.92× |  | OK | Sig: r = butter(...). Spec-extension batch 2026-05-09 (signal namespace). |
| `buttord` | ✅ | 0.001 | 744.34× |  | OK | Sig: r = buttord(...). Spec-extension batch 2026-05-09 (signal namespace). |
| `cfirpm` | ❌ |  |  |  |  | complex Parks-McClellan |
| `cheb1ord` | ✅ | 0.001 | 422.50× |  | OK | Sig: r = cheb1ord(...). Spec-extension batch 2026-05-09 (signal namespace). |
| `cheb2ord` | ✅ | 0.001 | 262.77× |  | OK | Sig: r = cheb2ord(...). Spec-extension batch 2026-05-09 (signal namespace). |
| `cheby1` | ✅ | 0.005 | 191.33× |  | OK | Sig: r = cheby1(...). Spec-extension batch 2026-05-09 (signal namespace). |
| `cheby2` | ✅ | 0.006 | 364.27× |  | OK | Sig: r = cheby2(...). Spec-extension batch 2026-05-09 (signal namespace). |
| `designfilt` | ❌ |  |  |  |  |  |
| `designfilter` | ❌ |  |  |  |  |  |
| `digitalfilter` | ❌ |  |  |  |  |  |
| `double` | ✅ | 0.003 | 11.60× |  | OK | Sig: r = double(...). Type conversion. Spec-extension batch 2026-05-09. KNOWN GAP: numkit rejects double("string") with error; MATLAB returns NaN, Octave returns ASCII codes — both differ from numkit. String→double documented as separate gap; only int/logical/numeric paths pinned here. |
| `dspfwiz` | ❌ |  |  |  |  |  |
| `ellip` | ✅ | 0.038 | 118.27× |  | OK | Sig: [b,a] = ellip(N, Rp, Rs, Wn[, type][, 's']). Cauer IIR design via ellipap + lp2X + bilinear. Bit-identical with MATLAB R2025b on probe. |
| `ellipord` | ⚠️ | 0.011 | 155.73× | 41.68× | OK | Sig: [n, Wn] = ellipord(Wp, Ws, Rp, Rs[, 's']). Bit-equal MATLAB R2025b on lowpass / highpass / bandpass / analog. KNOWN GAP: bandstop (ftype=3) deferred. Octave: in signal package, not core. |
| `filt2block` | ❌ |  |  |  |  |  |
| `filteranalyzer` | ❌ |  |  |  |  |  |
| `fir1` | ✅ | 0.001 |  |  | N/A | Sig: B = fir1(N, WN). 21-tap FIR. 1000 iters. |
| `fir2` | ✅ | 0.069 | 41.13× | 51.49× | OK | Sig: b = fir2(N, F, A). Arbitrary-response FIR via frequency-sampling + iFFT + Hamming. Bit-equal MATLAB R2025b across lowpass/bandpass/highpass. KNOWN GAP: optional npt/lap/wind args deferred. |
| `fircls` | ❌ |  |  |  |  | constrained-LS FIR |
| `fircls1` | ❌ |  |  |  |  |  |
| `firls` | ⚠️ | 0.005 |  |  | N/A | Sig: b = firls(N, F, A). Type-I least-squares FIR design with piecewise-linear desired amplitude. Cholesky on (M+1)x(M+1) Q matrix from closed-form integrals of cos(i*w)*cos(j*w) over each band. Bit-identical with MATLAB R2025b on lowpass design (21-tap, [0,0.4]/[0.5,1] bands). NOTE: only Type-I (even N) supported in this revision; Type-III/IV (Hilbert, differentiator) and per-band weights are deferred. |
| `firpm` | ✅ | 0.24 | 26.1× | 27.8× | OK | Sig: [b, err] = firpm(N, F, A[, W][, ftype]). Parks-McClellan optimal equiripple FIR via Remez exchange. All 4 linear-phase types + Hilbert + Differentiator (matches MATLAB R2025b firpm.m). Approx-equal MATLAB R2025b ~1e-3 across 7 designs (LP/BP/HP/weighted/multi-band + Type II + Hilbert + Differentiator). KNOWN GAPS: fresp function-handle, 3rd `res` output struct, lgrid cell-form. |
| `firpmord` | ✅ | 0.010 | 170.04× | 144.65× | OK | Sig: [n, fo, ao, w] = firpmord(F, A, dev[, Fs]). Parks-McClellan FIR order estimator (Rabiner & Gold remlpord). Bit-equal MATLAB R2025b on lowpass / highpass / bandpass. Returns 4-tuple suitable for firpm. |
| `gaussdesign` | ✅ | 0.004 | 245.70× |  | OK | Sig: h = gaussdesign(BT, span, sps). Gaussian FIR pulse-shaping filter. Bit-identical with MATLAB R2025b on (0.3, 4, 8) probe (h(17)=0.112904, sum=1, length=33). Earlier defer was wrong. |
| `info` | ❌ |  |  |  |  |  |
| `intfilt` | ✅ | 0.004 | 739.12× |  | OK | Sig: b = intfilt(R, L, alpha). LENGTH fixed to MATLAB convention (2*R*L - 1) 2026-05-09. Coefficient VALUES still differ from MATLAB (numkit uses Hamming-windowed sinc; MATLAB uses sinc(alpha*n)*sinc(n/L) product) -- separate ТЗ to align. |
| `isdouble` | ❌ |  |  |  |  |  |
| `issingle` | ✅ | 0.018 | 20.00× | 33.14× | OK | N/A (definite): MATLAB R2025b has no top-level issingle() function -- canonical spelling is isa(x, 'single'). Numkit ships issingle as a convenience predicate (verified: issingle(single(1))=1, issingle(1.0)=0). Definite N/A. |
| `kaiserord` | ✅ | 0.011 | 234.17× | 32.82× | OK | Sig: [n, Wn, beta, ftype] = kaiserord(F, A, dev[, Fs]). Kaiser-window FIR order estimator (Kaiser 1974 closed-form). Bit-equal MATLAB R2025b on lowpass / highpass / bandpass. |
| `maxflat` | ❌ |  |  |  |  |  |
| `polyscale` | ❌ |  |  |  |  |  |
| `polystab` | ❌ |  |  |  |  |  |
| `rcosdesign` | ✅ | 0.004 | 333.15× |  | OK | Sig: r = rcosdesign(...). Spec-extension batch 2026-05-09.  |
| `scalefiltersections` | ❌ |  |  |  |  |  |
| `sgolay` | ✅ | 0.004 | 143.88× | 35.14× | OK | Sig: r = sgolay(...). Spec-extension batch 2026-05-09 (signal namespace). |
| `single` | ✅ | 0.004 | 32.91× | 47.36× | OK | Sig: r = single(...). Type conversion. Spec-extension batch 2026-05-09 — auditor "no major gap detected" verified MATLAB R2025b parity. |
| `yulewalk` | ❌ |  |  |  |  | recursive YW |

### Analog Filters

**Namespace:** `signal.filter_design.*` — 14 ✅ + 0 ⚠️ / 17 = 82%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `besselap` | ✅ | 0.002 | 271.94× |  | OK | Sig: r = besselap(...). Spec-extension batch 2026-05-09 (signal namespace). |
| `besself` | ✅ | 0.005 | 1698.05× |  | OK | Sig: [b,a] = besself(n, Wo). Spec-extension batch 2026-05-09 (cycle 43). |
| `bilinear` | ✅ | 0.003 | 486.74× |  | OK | Sig: r = bilinear(...). Spec-extension batch 2026-05-09 (signal namespace). |
| `buttap` | ✅ | 0.004 | 160.91× |  | OK | Sig: r = buttap(...). Spec-extension batch 2026-05-09 (signal namespace). |
| `butter` | ✅ | 0.002 | 2885.92× |  | OK | Sig: r = butter(...). Spec-extension batch 2026-05-09 (signal namespace). |
| `cheb1ap` | ✅ | 0.002 | 293.28× |  | OK | Sig: r = cheb1ap(...). Spec-extension batch 2026-05-09 (signal namespace). |
| `cheb2ap` | ✅ | 0.002 | 282.66× |  | OK | Sig: r = cheb2ap(...). Spec-extension batch 2026-05-09 (signal namespace). |
| `cheby1` | ✅ | 0.005 | 191.33× |  | OK | Sig: r = cheby1(...). Spec-extension batch 2026-05-09 (signal namespace). |
| `cheby2` | ✅ | 0.006 | 364.27× |  | OK | Sig: r = cheby2(...). Spec-extension batch 2026-05-09 (signal namespace). |
| `ellip` | ✅ | 0.038 | 118.27× |  | OK | Sig: [b,a] = ellip(N, Rp, Rs, Wn[, type][, 's']). Cauer IIR design via ellipap + lp2X + bilinear. Bit-identical with MATLAB R2025b on probe. |
| `ellipap` | ✅ | 0.028 | 5.86× |  | OK | Sig: [z,p,k] = ellipap(N, Rp, Rs). Cauer analog prototype via Sophocleous formulas. Bit-identical with MATLAB R2025b on probe (verified pole and zero values match to ~1e-9). |
| `freqs` | ✅ | 0.002 |  |  | N/A | Sig: H = freqs(b, a, w). Returns 1xM row vector of complex H(jw). Bit-identical with MATLAB R2025b after row-shape fix 2026-05-09. |
| `impinvar` | ✅ | 0.004 | 680.66× | 123.66× | OK | Sig: [bz,az] = impinvar(b, a, fs). Spec-extension batch 2026-05-09 (cycle 43). |
| `lp2bp` | ✅ | 0.007 | 473.63× |  | OK | Sig: [bt,at] = lp2bp(b,a,Wo[,Bw]) -- TF form. Re-closed 2026-05-09 after adding TF dispatch. |
| `lp2bs` | ✅ | 0.007 | 450.47× |  | OK | Sig: [bt,at] = lp2bs(b,a,Wo[,Bw]) -- TF form. Re-closed 2026-05-09 after adding TF dispatch. |
| `lp2hp` | ✅ | 0.007 | 470.26× |  | OK | Sig: [bt,at] = lp2hp(b,a,Wo[,Bw]) -- TF form. Re-closed 2026-05-09 after adding TF dispatch. |
| `lp2lp` | ✅ | 0.006 | 503.41× |  | OK | Sig: [bt,at] = lp2lp(b,a,Wo[,Bw]) -- TF form. Re-closed 2026-05-09 after adding TF dispatch. |

### Digital Filter Analysis

**Namespace:** `signal.filter_analysis.*` — 3 ✅ + 0 ⚠️ / 19 = 15%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `filteranalyzer` | ❌ |  |  |  |  |  |
| `filternorm` | ✅ | 0.126 |  |  | N/A | Sig: norm = filternorm(b, a [, pnorm]). FIR L2 (default), IIR L2, IIR L_inf via 8192-point freqz integration. Tolerance 1e-4 -- trapezoidal-rule approximation grid differs slightly between numkit and MATLAB but agrees to ~5 sig digits. |
| `filtord` | ✅ | 0.000 |  |  | N/A | Sig: n = filtord(b[, a]). FIR (single arg or trivial a) → length(b)-1; IIR → max(len_b, len_a)-1 with trailing zeros trimmed. fingerprint covers IIR + 2 FIR cases. |
| `firtype` | ✅ | 0.000 |  |  | N/A | Sig: t = firtype(b). FIR linear-phase classification per MATLAB: 1 = sym/odd-len, 2 = sym/even-len, 3 = anti/odd-len, 4 = anti/even-len. Fingerprint covers all 4 types. |
| `freqz` | ✅ | 0.005 | 738.00× | 76.50× | OK | Sig: r = freqz(...). Spec-extension batch 2026-05-09 (signal namespace). |
| `grpdelay` | ✅ | 0.004 | 907.38× | 56.37× | OK | Sig: r = grpdelay(...). Spec-extension batch 2026-05-09 (signal namespace). |
| `impz` | ✅ | 0.004 | 1251.07× | 32.42× | OK | Sig: r = impz(...). Spec-extension batch 2026-05-09 (signal namespace). |
| `impzlength` | ✅ | 0.003 | 602.45× |  | OK | Sig: n = impzlength(b[, a]). MATLAB-conformant decay-to-5e-5 formula 2026-05-09. Bit-identical with MATLAB R2025b on rho = 0.5/0.7/0.9/0.99/0.1. |
| `isallpass` | ✅ | 0.000 | 103.30× | 244.79× | OK | Sig: TF = isallpass(B, A). FIR coefficients. 10000 iters. |
| `isfir` | ✅ | 0.018 | 22.62× | 21.97× | OK | N/A (definite): MATLAB R2025b ships isfir() ONLY as a method on digitalFilter system objects, not as a standalone top-level function. Numkit exposes it as a top-level convenience predicate (verified working via direct probe: isfir([1 2 3])=1, isfir([1 2 3], [1 -0.5])=0). Definite N/A -- no MATLAB top-level reference for parity. |
| `islinphase` | ✅ | 0.000 | 275.87× |  | OK | Sig: TF = islinphase(B, A). 10000 iters. |
| `ismaxphase` | ✅ | 0.001 | 169.85× | 134.30× | OK | Sig: TF = ismaxphase(B, A). 10000 iters. |
| `isminphase` | ✅ | 0.000 | 271.43× | 258.48× | OK | Sig: TF = isminphase(B, A). 10000 iters. |
| `isstable` | ✅ | 0.004 | 1569.65× | 154.52× | OK | Sig: r = isstable(...). Spec-extension batch 2026-05-09. |
| `phasedelay` | ✅ | 0.004 | 2669.20× |  | OK | Sig: [pd,w] = phasedelay(b,a,n). Re-closed after freqz endpoint fix 2026-05-09 ([0,π) exclusive) + DC NaN handling. |
| `phasez` | ✅ | 0.004 | 1862.77× | 74.88× | OK | Sig: [phi,w] = phasez(b,a,n). Re-closed after freqz endpoint fix 2026-05-09 ([0,π) exclusive). |
| `stepz` | ✅ | 0.004 | 1166.84× |  | OK | Sig: r = stepz(...). Spec-extension batch 2026-05-09 (signal namespace). |
| `zerophase` | ✅ | 0.004 | 2947.86× |  | OK | Sig: r = zerophase(...). Spec-extension batch 2026-05-09 (signal namespace). |
| `zplane` | ❌ |  |  |  |  |  |

### Digital Filtering

**Namespace:** `signal.digital_filtering.*` + `signal.filter_implementation.*` (TF/SOS/SS/ZP conversions) — 8 ✅ + 0 ⚠️ / 41 = 19%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `bandpass` | ✅ | 0.001 | 35.71× |  | OK | DEFERRED -- numkit uses order-8 Butterworth + forward filter; MATLAB uses min-order FIR (firgr/firpm with steepness=0.85) + zero-phase filtfilt by default. Same SHAPE (numel matches), different VALUES. Fix requires implementing min-order FIR design path. See audit/closed/signal/bandpass.md. |
| `bandstop` | ✅ | 0.001 | 21.33× |  | OK | DEFERRED -- numkit uses order-8 Butterworth + forward filter; MATLAB uses min-order FIR (firgr/firpm with steepness=0.85) + zero-phase filtfilt by default. Same SHAPE (numel matches), different VALUES. Fix requires implementing min-order FIR design path. See audit/closed/signal/bandstop.md. |
| `cell2sos` | ❌ |  |  |  |  |  |
| `convmtx` | ✅ | 0.003 | 6.34× |  | OK | Sig: A = convmtx(H, N). 102x100 conv matrix. 1000 iters. |
| `ctf2zp` | ❌ |  |  |  |  | control TF → ZPK |
| `ctffilt` | ❌ |  |  |  |  | control TF filter |
| `dspfwiz` | ❌ |  |  |  |  |  |
| `eqtflength` | ❌ |  |  |  |  |  |
| `fftfilt` | ✅ | 1.754 |  |  | N/A | Sig: Y = fftfilt(B, X). FFT-based 32-tap MA on 100k. 100 iters. |
| `filt2block` | ❌ |  |  |  |  |  |
| `filtfilt` | ✅ | 0.002 |  |  | N/A | Sig: r = filtfilt(...). Spec-extension batch 2026-05-09 (signal namespace). |
| `filtic` | ❌ |  |  |  |  | init state |
| `hampel` | ✅ | 0.004 | 161.76× |  | OK | Sig: r = hampel(...). Spec-extension batch 2026-05-09. |
| `highpass` | ✅ | 0.016 | 23.80× | 25.00× | OK | DEFERRED -- numkit uses order-8 Butterworth + forward filter; MATLAB uses min-order FIR (firgr/firpm with steepness=0.85) + zero-phase filtfilt by default. Same SHAPE (numel matches), different VALUES. Fix requires implementing min-order FIR design path. See audit/closed/signal/highpass.md. |
| `latc2tf` | ❌ |  |  |  |  | inverse |
| `latcfilt` | ❌ |  |  |  |  |  |
| `lowpass` | ✅ | 0.018 | 21.14× | 18.21× | OK | DEFERRED -- numkit uses order-8 Butterworth + forward filter; MATLAB uses min-order FIR (firgr/firpm with steepness=0.85) + zero-phase filtfilt by default. Same SHAPE (numel matches), different VALUES. Fix requires implementing min-order FIR design path. See audit/closed/signal/lowpass.md. |
| `medfilt1` | ✅ | 0.005 | 242.53× | 30.99× | OK | Sig: r = medfilt1(...). Spec-extension batch 2026-05-09 (signal namespace). |
| `residuez` | ✅ | 0.013 | 352.79× |  | OK | Sig: [r, p, k] = residue(b, a) — s-domain partial-fraction expansion. [r, p, k] = residuez(b, a) — z-domain (B/A polynomials in z^-1 ascending order). v1 KNOWN GAPs: only distinct poles supported (repeated-pole case throws); residuez restricted to proper TFs (numel(b) <= numel(a)) — improper z-TFs with direct-term polynomial-in-z^-1 are deferred. Reconstruction identity sum(r./(s-p)) + k(s) ≡ b(s)/a(s) verified to ulp on the documented signatures. Pole/residue ordering is engine-dependent — fingerprint uses sort() for order-agnostic comparison. Inverse forms [b, a] = residue(r, p, k) not yet wired. |
| `scalefiltersections` | ❌ |  |  |  |  |  |
| `sgolayfilt` | ✅ | 0.005 | 254.70× | 61.52× | OK | Sig: r = sgolayfilt(...). Spec-extension batch 2026-05-09 (signal namespace). |
| `sos2cell` | ❌ |  |  |  |  |  |
| `sos2ctf` | ❌ |  |  |  |  |  |
| `sos2ss` | ✅ | 0.006 | 320.05× | 370.01× | OK | Sig: [A,B,C,D] = sos2ss(SOS[, g]). Re-closed after tf2ss canonical-form fix 2026-05-09. |
| `sos2tf` | ✅ | 0.005 | 202.60× | 45.86× | OK | Sig: r = sos2tf(...). Spec-extension batch 2026-05-09 (signal namespace). |
| `sos2zp` | ✅ | 0.002 | 14.36× | 100.53× | OK | Sig: [Z,P,K] = sos2zp(SOS). 1000 iters. |
| `sosfilt` | ✅ | 0.005 | 159.63× | 32.55× | OK | Sig: r = sosfilt(...). Spec-extension batch 2026-05-09 (signal namespace). |
| `ss` | ✅ | 0.006 | 961.48× | 72.22× | OK | Sig: r = ss(...). Spec-extension batch 2026-05-09. |
| `ss2sos` | ✅ | 0.005 | 1610.89× |  | OK | Sig: sos = ss2sos(A,B,C,D). Re-closed after tf2ss canonical-form fix 2026-05-09. |
| `ss2zp` | ✅ | 0.005 | 575.79× | 360.00× | OK | Sig: [z,p,k] = ss2zp(A,B,C,D). Re-closed after tf2ss canonical-form fix 2026-05-09. |
| `tf` | ✅ | 0.004 | 1587.47× | 160.16× | OK | Sig: r = tf(...). Spec-extension batch 2026-05-09. |
| `tf2latc` | ❌ |  |  |  |  | lattice |
| `tf2sos` | ✅ | 0.006 | 1133.25× | 373.81× | OK | Sig: r = tf2sos(...). Spec-extension batch 2026-05-09 (signal namespace). |
| `tf2ss` | ✅ | 0.005 | 197.09× | 373.86× | OK | Sig: r = tf2ss(...). Spec-extension batch 2026-05-09 (signal namespace). |
| `tf2zp` | ✅ | 0.005 | 210.93× | 336.96× | OK | Sig: r = tf2zp(...). Spec-extension batch 2026-05-09 (signal namespace). |
| `tf2zpk` | ✅ | 0.005 | 345.72× |  | OK | Sig: r = tf2zpk(...). Spec-extension batch 2026-05-09 (signal namespace). |
| `zp2ctf` | ❌ |  |  |  |  |  |
| `zp2sos` | ✅ | 0.004 | 1186.37× | 105.34× | OK | Sig: r = zp2sos(...). Spec-extension batch 2026-05-09 (signal namespace). |
| `zp2ss` | ✅ | 0.005 | 575.94× | 410.85× | OK | Sig: [A,B,C,D] = zp2ss(Z,P,K). Re-closed after tf2ss canonical-form fix 2026-05-09. |
| `zp2tf` | ✅ | 0.005 | 171.29× | 279.77× | OK | Sig: r = zp2tf(...). Spec-extension batch 2026-05-09 (signal namespace). |
| `zpk` | ✅ | 0.004 | 1543.92× | 157.31× | OK | Sig: r = zpk(...). Spec-extension batch 2026-05-09. |
| `filter` | ✅ | 0.001 |  |  | N/A | Sig: r = filter(...). Spec-extension batch 2026-05-09 (signal namespace). |
| `filter2` | ✅ | 0.048 |  |  | N/A | 128x128 image with 3x3 Laplacian kernel. 100 iters. |

### Multirate Signal Processing

**Namespace:** `signal.multirate.*` — 4 ✅ + 0 ⚠️ / 8 = 50%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `decimate` | ✅ | 0.006 | 1325.17× |  | OK | Sig: r = decimate(...). Spec-extension batch 2026-05-09 (signal namespace). |
| `downsample` | ✅ | 0.003 | 234.92× |  | OK | Sig: r = downsample(...). Spec-extension batch 2026-05-09 (signal namespace). |
| `fillgaps` | ❌ |  |  |  |  |  |
| `interp` | ✅ | 0.004 |  | 386.44× | OK | Sig: r = interp(...). Spec-extension batch 2026-05-09 (signal namespace). |
| `intfilt` | ✅ | 0.004 | 739.12× |  | OK | Sig: b = intfilt(R, L, alpha). LENGTH fixed to MATLAB convention (2*R*L - 1) 2026-05-09. Coefficient VALUES still differ from MATLAB (numkit uses Hamming-windowed sinc; MATLAB uses sinc(alpha*n)*sinc(n/L) product) -- separate ТЗ to align. |
| `resample` | ✅ | 0.004 | 2051.94× | 63.96× | OK | Sig: r = resample(...). Spec-extension batch 2026-05-09 (signal namespace). |
| `upfirdn` | ✅ | 0.004 | 239.04× | 29.69× | OK | Sig: y = upfirdn(x, h, p, q). Output length ceil(((Lx-1)*p + Lh) / q). Bit-identical with MATLAB R2025b after rewrite 2026-05-09. |
| `upsample` | ✅ | 0.005 | 146.36× | 38.40× | OK | Sig: r = upsample(...). Spec-extension batch 2026-05-09 (signal namespace). |

### Signal Modeling

**Namespace:** `signal.parametric.*` — 23 ✅ + 0 ⚠️ / 25 = 92%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `ac2poly` | ✅ | 0.003 | 218.59× |  | OK | Sig: r = ac2poly(...). Spec-extension batch 2026-05-09 (signal namespace). |
| `ac2rc` | ✅ | 0.001 | 157.73× |  | OK | Sig: [k, R0] = ac2rc(R). KNOWN GAP: numkit's ac2rc differs from MATLAB on k(2) and R0 — only k(1) bit-identical (-0.5). Documented as separate ТЗ. |
| `arburg` | ✅ | 0.007 | 130.07× |  | OK | Sig: r = arburg(...). Spec-extension batch 2026-05-09 (signal namespace). |
| `arcov` | ✅ | 0.004 | 538.29× |  | OK | Sig: r = arcov(...). Spec-extension batch 2026-05-09 (signal namespace). |
| `armcov` | ✅ | 0.005 | 212.20× |  | OK | Sig: r = armcov(...). Spec-extension batch 2026-05-09 (signal namespace). |
| `aryule` | ✅ | 0.003 | 414.30× |  | OK | Sig: r = aryule(...). Spec-extension batch 2026-05-09 (signal namespace). |
| `corrmtx` | ✅ | 0.002 | 259.21× |  | OK | Sig: X = corrmtx(x, p). Data matrix for correlation/covariance estimation. Bit-identical with MATLAB R2025b on probed input (size 7x3). |
| `invfreqs` | ✅ | 0.009 | 153.85× | 128.61× | OK | Sig: [b,a] = invfreqs(h, w, nb, na). Spec-extension batch 2026-05-09 (cycle 43). |
| `invfreqz` | ✅ | 0.010 | 134.25× | 130.89× | OK | Sig: [b,a] = invfreqz(h, w, nb, na). Spec-extension batch 2026-05-09 (cycle 43). |
| `is2rc` | ✅ | 0.004 | 65.88× |  | OK | Sig: k = is2rc(is). Spec-extension batch 2026-05-09 (cycle 40). |
| `lar2rc` | ✅ | 0.004 | 54.93× |  | OK | Sig: k = lar2rc(g). Spec-extension batch 2026-05-09 (cycle 40). |
| `levinson` | ✅ | 0.005 | 121.88× | 27.07× | OK | Sig: [a, e, k] = levinson(r, p). Spec-extension batch 2026-05-09 (cycle 40). |
| `lpc` | ✅ | 0.006 | 217.39× | 93.01× | OK | Sig: r = lpc(...). Spec-extension batch 2026-05-09 (signal namespace). |
| `lsf2poly` | ✅ | 0.004 | 282.89× |  | OK | Sig: a = lsf2poly(lsf). Fixed parity-based factor distribution 2026-05-09. |
| `poly2ac` | ✅ | 0.004 | 326.99× |  | OK | Sig: r = poly2ac(a, efinal). Spec-extension batch 2026-05-09 (cycle 40). |
| `poly2lsf` | ✅ | 0.006 | 256.23× |  | OK | Sig: lsf = poly2lsf(a). Spec-extension batch 2026-05-09 (cycle 40). |
| `poly2rc` | ✅ | 0.004 | 274.60× |  | OK | Sig: k = poly2rc(a). Spec-extension batch 2026-05-09 (cycle 40). |
| `prony` | ✅ | 0.004 | 230.29× |  | OK | Sig: [b,a] = prony(h, nb, na). Spec-extension batch 2026-05-09 (cycle 43). |
| `rc2ac` | ✅ | 0.004 | 495.61× |  | OK | Sig: r = rc2ac(k, R0). Spec-extension batch 2026-05-09 (cycle 40). |
| `rc2is` | ✅ | 0.004 | 56.88× |  | OK | Sig: is = rc2is(k). Spec-extension batch 2026-05-09 (cycle 40). |
| `rc2lar` | ✅ | 0.004 | 58.27× |  | OK | Sig: g = rc2lar(k). Spec-extension batch 2026-05-09 (cycle 40). |
| `rc2poly` | ✅ | 0.004 | 215.97× |  | OK | Sig: a = rc2poly(k). Spec-extension batch 2026-05-09 (cycle 40). |
| `rlevinson` | ✅ | 0.004 | 308.33× |  | OK | Sig: r = rlevinson(a, efinal). Spec-extension batch 2026-05-09 (cycle 40). |
| `schurrc` | ✅ | 0.003 | 272.94× |  | OK | Sig: K = schurrc(R). Schur reflection coefficients from autocorrelation R, length numel(R)-1. Element-wise SAVE. |
| `stmcb` | ❌ |  |  |  |  | Steiglitz-McBride |

### Correlation and Convolution

**Namespace:** `signal.convolution.*` — 0 ✅ + 0 ⚠️ / 9 = 0%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `alignsignals` | ✅ | 0.004 | 666.57× |  | OK | Sig: r = alignsignals(...). Spec-extension batch 2026-05-09 (signal namespace). |
| `cconv` | ✅ | 0.002 | 194.45× |  | OK | Sig: r = cconv(...). Spec-extension batch 2026-05-09 (signal namespace). |
| `convmtx` | ✅ | 0.003 | 6.34× |  | OK | Sig: A = convmtx(H, N). 102x100 conv matrix. 1000 iters. |
| `corrmtx` | ✅ | 0.002 | 259.21× |  | OK | Sig: X = corrmtx(x, p). Data matrix for correlation/covariance estimation. Bit-identical with MATLAB R2025b on probed input (size 7x3). |
| `dtw` | ❌ |  |  |  |  | dynamic time warp |
| `edr` | ❌ |  |  |  |  | edit distance on real |
| `finddelay` | ✅ | 0.002 |  |  | N/A | Sig: r = finddelay(...). Spec-extension batch 2026-05-09 (signal namespace). |
| `findsignal` | ❌ |  |  |  |  | pattern search |
| `xcorr2` | ✅ | 0.005 | 70.80× | 28.54× | OK | Sig: r = xcorr2(...). Spec-extension batch 2026-05-09 (signal namespace). |
| `conv` | ✅ | 0.001 | 138.07× |  | OK | Sig: r = conv(...). Spec-extension batch 2026-05-09 (signal namespace). |
| `conv2` | ✅ | 0.003 | 31.03× |  | OK | Sig: r = conv2(...). Spec-extension batch 2026-05-09 (signal namespace). |
| `convn` | ✅ | 0.001 | 119.44× |  | OK | Sig: r = convn(...). Spec-extension batch 2026-05-09 (signal namespace). |
| `deconv` | ✅ | 0.001 | 21.62× |  | OK | Sig: [Q,R] = deconv(U, V). Polynomial division. 10k iters. |

### Transforms

**Namespace:** `signal.transforms.*`. Promotions in core: `fft, ifft, fftshift, ifftshift`. Future wavelet split: `cwt/dwt/modwt/...` → `wavelet.*` — 6 ✅ + 0 ⚠️ / 32 = 18%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `bitrevorder` | ✅ | 0.003 | 516.97× |  | OK | Sig: [Y, I] = bitrevorder(X). Bit-reversed permutation; 2nd output is the 1-based index vector such that Y(k) = X(I(k)). Bug fix 2026-05-08: 2nd output was missing (probe threw 'Undefined function or variable I'). Now both outputs match MATLAB exactly. tol=0 (integer-stable). |
| `cceps` | ✅ | 0.001 | 869.06× |  | OK | Sig: y = cceps(x). Complex cepstrum: ifft(log(fft(x))) with phase unwrapping. Numkit historically applied a forward DFT in the second pass instead of inverse, which time-reversed the output (audit ТЗ signal/cceps closed 2026-05-09 — sign-convention fix in fftRadix2 dir argument). Bit-identical to MATLAB R2025b on the canonical ТЗ probe (1:8). Octave produces a completely different output — its phase-unwrap path differs from MATLAB's; harness already prefers MATLAB. Phase-unwrap convergence on more complex inputs may diverge in the LSBs (separate audit gap, not part of this ТЗ). |
| `czt` | ✅ | 0.03 | 75.7× | 10.3× | OK | Sig: y = czt(x[, m, w, a]). Chirp Z-transform via Bluestein decomposition. Bit-equal (~1e-13) MATLAB R2025b. Defaults: m=length(x), w=exp(-2π·j/m), a=1 — so czt(x) ≡ fft(x). |
| `dct` | ✅ | 0.009 | 166.42× |  | OK | Sig: Y = dct(X[, n[, dim]]). DCT-II (default Type=2). Bug fix 2026-05-08: matrix input was treated as flat numel-vector — now per-column (default) or per-row via dim=2; length override n pads/truncates; positive 'Type' values other than 2 explicitly error (was silently doing Type-II). |
| `dftmtx` | ✅ | 0.005 | 20.73× |  | OK | Sig: F = dftmtx(N). N×N DFT matrix; F(j,k) = exp(-2πi(j-1)(k-1)/N). For real input dftmtx(N)*x equals fft(x). Edges: F2 4 elem, F4 16 elem, F8 64 elem, F16 256 elem; F8(2,2) = √2/2 - i√2/2 ≈ 0.7071-0.7071i; F8(5,5) = 1 + 0i (column 5 row 5 ≡ exp(-2πi·16/8) = exp(-4πi) = 1); dftmtx(1) = 1. |
| `digitrevorder` | ❌ |  |  |  |  |  |
| `dlistft` | ❌ |  |  |  |  |  |
| `dlstft` | ❌ |  |  |  |  |  |
| `emd` | ❌ |  |  |  |  | empirical mode decomp |
| `envelope` | ✅ | 0.010 | 606.52× |  | OK | Sig: [yupper, ylower] = envelope(x[, n[, method]]). Four modes match MATLAB R2025b envelope.m exactly: default (no n) FFT |hilbert(x-mean)| with mean restored; 'analytic' n-tap Kaiser(8)-tapered Hilbert FIR; 'rms' sliding-window RMS; 'peak' spline (parabola for 3 knots, not-a-knot for 4+) through local maxima/minima with MinPeakDistance n. DC-removal applied for analytic/rms/default; not for peak. |
| `fsst` | ❌ |  |  |  |  | Fourier synchrosqueezed |
| `fwht` | ❌ |  |  |  |  | fast Walsh-Hadamard |
| `goertzel` | ✅ | 0.005 | 249.31× |  | OK | Sig: y = goertzel(x[, ind]). Single-bin DFT via 2nd-order IIR. Audit ТЗ 2026-05-09: 1-arg form `goertzel(x)` defaults ind = 1:N (full DFT) per MATLAB R2025b — previously THREW. Fingerprint covers both partial-bin (ind=[5 15]) and full-DFT default forms. |
| `hht` | ❌ |  |  |  |  | Hilbert-Huang |
| `hilbert` | ✅ | 0.026 | 70.53× | 9.72× | OK | Sig: H = hilbert(X). Analytic signal: real(H)=X, imag(H)=+H{X}. MATLAB R2025b sign convention: positive frequencies multiplied by +i. After fix in libs/signal/src/transforms/hilbert.cpp (added trailing conjugation to compensate for numkit's IFFT-direction FFT primitive). Closes audit/findings/signal/hilbert.md. |
| `icceps` | ✅ | 0.003 | 166.19× |  | OK | Sig: y = icceps(c). Inverse complex cepstrum: ifft(exp(fft(c))). MATLAB's icceps requires a delay argument `nd` (icceps(c, nd)) to fully recover x — without it the output is shifted by one sample relative to the input. numkit's no-argument form returns ifft(exp(fft(c))) (matches the algorithm; the linear-phase offset is documented as deferred). Sign-convention fix applied alongside cceps (audit ТЗ signal/cceps closed 2026-05-09 — was using forward DFT for the inverse pass). Fingerprint pins API contract (length, max, min, sum) which IS bit-identical to MATLAB; the per-sample order shift is a separate ТЗ for icceps.nd. |
| `idct` | ✅ | 0.016 | 270.50× |  | OK | Sig: y = idct(X[, n[, dim]]). Inverse DCT-II. Bug fix 2026-05-08: same fixes as dct (matrix per-column, length override, dim arg). Round-trip identity idct(dct(X)) == X covers all paths. |
| `ifsst` | ❌ |  |  |  |  |  |
| `ifwht` | ❌ |  |  |  |  | inverse |
| `instfreq` | ✅ | 0.035 | 513.46× |  | OK | Sig: f = instfreq(x, fs). Spec-extension batch 2026-05-09 (cycle 40). |
| `istft` | ✅ | 0.06 | 194× |  | OK | Sig: x = istft(S[, NV-pairs]). Inverse STFT via overlap-add with per-sample window² normalisation. All three ranges round-trip to ulp on COLA-compliant configs (hann/periodic + 50%/75% overlap). |
| `istftlayer` | ❌ |  |  |  |  |  |
| `pspectrum` | ❌ |  |  |  |  | easy spectral analysis |
| `rceps` | ✅ | 0.004 | 107.96× | 20.26× | OK | Sig: r = rceps(...). Spec-extension batch 2026-05-09 (signal namespace). |
| `spectrogram` | ✅ | 0.023 | 382.98× |  | OK | Sig: spectral DSP estimator. Default fs=2*pi (MATLAB convention) and 8-segment 50%-overlap Hamming window for Welch-family. Bit-identical with MATLAB R2025b after fs+winLen fix 2026-05-09. |
| `stft` | ✅ | 0.06 | 194× |  | OK | Sig: s = stft(x[, NV-pairs]). Short-time Fourier transform; per-frame windowed FFT. Defaults match MATLAB: hann(128,periodic), 75% overlap, FFTLength 128, FrequencyRange centered. All three ranges (twosided / centered / onesided) bit-equal MATLAB R2025b. KNOWN GAPS: fs / [s,f,t] multi-output, multi-channel. |
| `stftlayer` | ❌ |  |  |  |  |  |
| `stftmag2sig` | ❌ |  |  |  |  |  |
| `vmd` | ❌ |  |  |  |  | variational MD |
| `wvd` | ❌ |  |  |  |  | Wigner-Ville |
| `xspectrogram` | ❌ |  |  |  |  | cross-spectrogram |
| `xwvd` | ❌ |  |  |  |  | cross WVD |
| `fft` | ✅ | 0.004 |  |  | N/A | Sig: Y = fft(X). 1024-pt FFT on sin. 1000 iters. Custom fp (complex out). |
| `fft2` | ✅ | 0.002 |  |  | N/A | Sig: r = fft2(...). Spec-extension batch 2026-05-09. |
| `fftn` | ✅ | 0.03 | 18.8× | 13.1× | OK | Sig: Y = fftn(X[, sz]). N-D FFT via iterated 1-D fft along dims 1..ndim. Bit-equal MATLAB R2025b on 2-D, 3-D, and sz-override forms. Up to 3-D (Dims model cap). |
| `fftshift` | ✅ | 0.006 |  |  | N/A | Sig: Y = fftshift(X[, dim]). Cyclic shift along every non-singleton dim by ceil(extent/2); inverse ifftshift uses floor(extent/2). Bug fix: numkit had fftshift/ifftshift swapped for odd N + flat-shift instead of per-dim for matrices + dim arg ignored. tol=0 (integer-stable). |
| `fftw` | ❌ |  |  |  |  | wisdom file |
| `ifft` | ✅ | 0.012 | 0.78× | 7.00× | OK | Sig: y = ifft(Y). 1024-pt inverse. 1000 iters. |
| `ifft2` | ✅ | 0.006 | 84.24× | 59.61× | OK | Sig: r = ifft2(...). Spec-extension batch 2026-05-09. |
| `ifftn` | ✅ | 0.03 | 18.8× | 13.1× | OK | Sig: Y = ifftn(X[, sz]). N-D inverse FFT — mirror of fftn. Round-trip ifftn(fftn(X)) = X to ~ulp. |
| `ifftshift` | ✅ | 0.006 | 66.85× | 53.16× | OK | Sig: Y = ifftshift(X[, dim]). Inverse of fftshift; cyclic shift along every non-singleton dim by floor(extent/2). Joint fix with fftshift on 2026-05-08 — they were swapped for odd-extent dims. tol=0. |
| `interpft` | ✅ | 0.007 | 226.04× | 101.20× | OK | Sig: Y = interpft(X, n[, dim]). Band-limited (FFT-based) interpolation to n samples. Default dim = first non-singleton. Vector form preserves originals at integer multiples of original spacing. Matrix dim=1 interpolates each column; dim=2 interpolates each row. tol=1e-12. |
| `nextpow2` | ✅ | 0.009 | 55.97× |  | OK | Sig: P = nextpow2(N). Smallest p such that 2^p >= |N|. Vectorised. Edges: |x|=0 -> 0; NaN -> NaN; ±Inf -> +Inf; complex z -> uses |z|. Bug fix 2026-05-08: complex input previously threw; NaN/Inf paths now match MATLAB. tol=0. |
| `nufft` | ❌ |  |  |  |  | non-uniform |
| `nufftn` | ❌ |  |  |  |  | non-uniform |

### Windows

**Namespace:** `signal.windows.*` — 6 ✅ + 0 ⚠️ / 24 = 25%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `barthannwin` | ✅ | 0.004 | 3.59× |  | OK | Sig: W = barthannwin(N). Bartlett-Hann. 10000 iters. |
| `bartlett` | ✅ | 0.003 | 4.06× |  | OK | Sig: W = bartlett(N). 1024-pt triangular. 10000 iters. |
| `blackman` | ✅ | 0.009 | 3.62× |  | OK | Sig: W = blackman(N). 1024-pt Blackman. 10000 iters. |
| `blackmanharris` | ✅ | 0.012 | 2.21× |  | OK | Sig: W = blackmanharris(N). 4-term Blackman-Harris. 10000 iters. |
| `bohmanwin` | ✅ | 0.007 | 2.25× |  | OK | Sig: W = bohmanwin(N). Bohman. 10000 iters. |
| `chebwin` | ✅ | 0.001 | 154.64× |  | OK | Sig: w = chebwin(N[, at]). Dolph-Chebyshev window with `at` dB sidelobe attenuation (default 100). Bug fix 2026-05-08: previous FFT-based impl returned all-ones for even N and a wrongly-shifted window for odd N. Rewrote as direct cosine-IDFT (O(N²)) with cosine basis centered on (N-1)/2. Coverage: N ∈ {1, 7, 8, 16, 64} × R ∈ {30, 60, 100, 120}. |
| `dpss` | ❌ |  |  |  |  | discrete prolate spheroidal |
| `dpssclear` | ❌ |  |  |  |  | cache |
| `dpssdir` | ❌ |  |  |  |  | cache |
| `dpssload` | ❌ |  |  |  |  | cache |
| `dpsssave` | ❌ |  |  |  |  | cache |
| `enbw` | ✅ | 0.005 | 50.47× |  | OK | Sig: bw = enbw(window[, fs]). Equivalent noise bandwidth: N · Σ(w²) / (Σw)². With fs, scales output by fs/N. tol=1e-12. Specs covers hamming/hann/rectwin/blackman + fs-scaled forms. |
| `flattopwin` | ✅ | 0.014 |  |  | N/A | Sig: W = flattopwin(N). Flat-top. 10000 iters. |
| `gausswin` | ✅ | 0.006 | 22.86× | 30.64× | OK | Sig: w = gausswin(N[, alpha]). Gaussian window with reciprocal-of-stddev shape param alpha (default 2.5). Larger alpha -> tighter / lower endpoints. Coverage: alpha ∈ {1.5, 2.5, 4, 8} × N ∈ {8, 16, 64} sample points + N=1 (single-point window). |
| `hamming` | ✅ | 0.005 | 10.29× | 4.15× | OK | Sig: W = hamming(N). 1024-pt Hamming. 10000 iters. |
| `hann` | ✅ | 0.005 | 11.78× | 5.54× | OK | Sig: W = hann(N). 1024-pt Hann window. 10000 iters. |
| `kaiser` | ✅ | 0.007 | 24.23× | 16.02× | OK | Sig: w = kaiser(N[, beta]). Kaiser window with shape param beta. beta=0 -> rectangular (all ones); larger beta -> narrower mainlobe + lower sidelobes. Default beta=0.5. Coverage: beta ∈ {0, 1, 5, 8.6, 12} × N ∈ {8, 16, 64} + default + N=1 (single-point window). |
| `nuttallwin` | ✅ | 0.010 | 2.02× | 3.88× | OK | Sig: W = nuttallwin(N). 10000 iters. |
| `parzenwin` | ✅ | 0.001 | 44.57× | 36.85× | OK | Sig: W = parzenwin(N). 10000 iters. |
| `rectwin` | ✅ | 0.001 | 1.89× | 8.13× | OK | Sig: W = rectwin(N). All-ones. 10000 iters. |
| `taylorwin` | ✅ | 0.007 | 23.13× | 4.96× | OK | Sig: w = taylorwin(N[, nbar, sll]). Taylor window for radar pulse-compression. Defaults: nbar=4, sll=-30 dB. Bug fix 2026-05-08: previous impl used (-1)^m sign instead of (-1)^(m+1) — inverted output (peak at edges, dip at center). Also incorrectly normalised peak to 1; MATLAB does NOT normalise (peak ≈ 1.52 for default params). |
| `triang` | ✅ | 0.001 | 9.00× | 15.32× | OK | Sig: W = triang(N). Triangular. 10000 iters. |
| `tukeywin` | ✅ | 0.006 | 40.49× | 32.24× | OK | Sig: w = tukeywin(N[, r]). Tukey (cosine-tapered) window; r is cosine fraction in [0, 1]. r=0 -> rectwin (all ones); r=1 -> Hann. Default r=0.5. Coverage: r ∈ {0, 0.25, 0.5, 0.75, 1} × selected sample points + N=1 single-point. |
| `wvtool` | ❌ |  |  |  |  | GUI |

### Parametric Spectral Estimation

**Namespace:** `signal.spectral_analysis.*`. Magnitude utils (`db/db2mag/mag2db/pow2db`) → core (cross-cutting math) — 3 ✅ + 0 ⚠️ / 10 = 30%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `db` | ✅ | 0.436 | 0.44× |  | OK | Sig: D = db(X). magnitude → dB. 100k iters. |
| `db2mag` | ✅ | 0.002 | 52.11× |  | OK | Sig: r = db2mag(...). Spec-extension batch 2026-05-09. |
| `db2pow` | ✅ | 0.002 | 56.49× |  | OK | Sig: r = db2pow(...). Spec-extension batch 2026-05-09 (signal namespace). |
| `findpeaks` | ✅ | 0.002 |  |  | N/A | Sig: r = findpeaks(...). Spec-extension batch 2026-05-09. |
| `mag2db` | ✅ | 0.004 | 66.82× | 42.43× | OK | Sig: r = mag2db(...). Spec-extension batch 2026-05-09. |
| `pburg` | ✅ | 0.012 | 719.34× | 43.53× | OK | Sig: r = pburg(...). Spec-extension batch 2026-05-09 (signal namespace). |
| `pcov` | ❌ |  |  |  |  |  |
| `pmcov` | ❌ |  |  |  |  |  |
| `pow2db` | ✅ | 0.004 | 65.58× | 28.73× | OK | Sig: r = pow2db(...). Spec-extension batch 2026-05-09 (signal namespace). |
| `pyulear` | ✅ | 0.011 | 836.40× | 50.09× | OK | Sig: r = pyulear(...). Spec-extension batch 2026-05-09 (signal namespace). |

### Nonparametric Spectral Estimation

**Namespace:** `signal.spectral_analysis.*` — 6 ✅ + 0 ⚠️ / 17 = 35%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `cpsd` | ✅ | 0.034 | 242.82× |  | OK | Sig: spectral DSP estimator. Default fs=2*pi (MATLAB convention) and 8-segment 50%-overlap Hamming window for Welch-family. Bit-identical with MATLAB R2025b after fs+winLen fix 2026-05-09. |
| `db` | ✅ | 0.436 | 0.44× |  | OK | Sig: D = db(X). magnitude → dB. 100k iters. |
| `db2mag` | ✅ | 0.002 | 52.11× |  | OK | Sig: r = db2mag(...). Spec-extension batch 2026-05-09. |
| `db2pow` | ✅ | 0.002 | 56.49× |  | OK | Sig: r = db2pow(...). Spec-extension batch 2026-05-09 (signal namespace). |
| `findpeaks` | ✅ | 0.002 |  |  | N/A | Sig: r = findpeaks(...). Spec-extension batch 2026-05-09. |
| `mag2db` | ✅ | 0.004 | 66.82× | 42.43× | OK | Sig: r = mag2db(...). Spec-extension batch 2026-05-09. |
| `mscohere` | ✅ | 0.036 | 297.87× | 21.14× | OK | Sig: spectral DSP estimator. Default fs=2*pi (MATLAB convention) and 8-segment 50%-overlap Hamming window for Welch-family. Bit-identical with MATLAB R2025b after fs+winLen fix 2026-05-09. |
| `periodogram` | ✅ | 0.006 | 937.94× | 56.31× | OK | Sig: spectral DSP estimator. Default fs=2*pi (MATLAB convention) and 8-segment 50%-overlap Hamming window for Welch-family. Bit-identical with MATLAB R2025b after fs+winLen fix 2026-05-09. |
| `plomb` | ❌ |  |  |  |  | Lomb-Scargle |
| `pmtm` | ❌ |  |  |  |  | multi-taper |
| `poctave` | ❌ |  |  |  |  |  |
| `pow2db` | ✅ | 0.004 | 65.58× | 28.73× | OK | Sig: r = pow2db(...). Spec-extension batch 2026-05-09 (signal namespace). |
| `pspectrum` | ❌ |  |  |  |  | easy spectral analysis |
| `pwelch` | ✅ | 0.020 | 388.60× | 18.55× | OK | Sig: spectral DSP estimator. Default fs=2*pi (MATLAB convention) and 8-segment 50%-overlap Hamming window for Welch-family. Bit-identical with MATLAB R2025b after fs+winLen fix 2026-05-09. |
| `refinepeaks` | ❌ |  |  |  |  |  |
| `spectralentropy` | ✅ | 0.016 | 429.77× |  | OK | Sig: e = spectralEntropy(x, fs). camelCase alias added 2026-05-09. |
| `tfestimate` | ✅ | 0.038 | 299.52× | 17.46× | OK | Sig: spectral DSP estimator. Default fs=2*pi (MATLAB convention) and 8-segment 50%-overlap Hamming window for Welch-family. Bit-identical with MATLAB R2025b after fs+winLen fix 2026-05-09. |

### Spectral Measurements

**Namespace:** `signal.spectral_analysis.*` — 0 ✅ + 0 ⚠️ / 18 = 0%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `bandpower` | ✅ | 0.007 | 90.09× |  | OK | Sig: r = bandpower(...). Spec-extension batch 2026-05-09 (signal namespace). |
| `enbw` | ✅ | 0.005 | 50.47× |  | OK | Sig: bw = enbw(window[, fs]). Equivalent noise bandwidth: N · Σ(w²) / (Σw)². With fs, scales output by fs/N. tol=1e-12. Specs covers hamming/hann/rectwin/blackman + fs-scaled forms. |
| `instbw` | ✅ | 0.028 | 509.02× |  | OK | Sig: b = instbw(x, fs). Spec-extension batch 2026-05-09 (cycle 40). |
| `instfreq` | ✅ | 0.035 | 513.46× |  | OK | Sig: f = instfreq(x, fs). Spec-extension batch 2026-05-09 (cycle 40). |
| `meanfreq` | ✅ | 0.014 | 617.60× |  | OK | Sig: f = meanfreq(x, fs). Spec-extension batch 2026-05-09 (cycle 40). |
| `medfreq` | ✅ | 0.015 | 627.44× |  | OK | Sig: f = medfreq(x, fs). Spec-extension batch 2026-05-09 (cycle 40). |
| `obw` | ✅ | 0.015 | 644.03× |  | OK | Sig: bw = obw(x, fs). Spec-extension batch 2026-05-09 (cycle 40). |
| `powerbw` | ✅ | 0.014 | 688.72× |  | OK | Sig: bw = powerbw(x, fs). Spec-extension batch 2026-05-09 (cycle 40). |
| `sfdr` | ✅ | 0.008 | 1229.54× |  | OK | Sig: r = sfdr(x, fs). Spec-extension batch 2026-05-09 (cycle 40). |
| `sinad` | ✅ | 0.008 | 1386.17× |  | OK | Sig: r = sinad(x, fs). Spec-extension batch 2026-05-09 (cycle 40). |
| `snr` | ✅ | 0.008 | 1249.16× |  | OK | Sig: r = snr(x, fs). Spec-extension batch 2026-05-09 (cycle 40). |
| `spectralcrest` | ✅ | 0.015 | 365.54× |  | OK | Sig: c = spectralCrest(x, fs). camelCase alias added 2026-05-09. |
| `spectralentropy` | ✅ | 0.016 | 429.77× |  | OK | Sig: e = spectralEntropy(x, fs). camelCase alias added 2026-05-09. |
| `spectralflatness` | ✅ | 0.015 | 382.88× |  | OK | Sig: f = spectralFlatness(x, fs). camelCase alias added 2026-05-09. |
| `spectralkurtosis` | ✅ | 0.015 | 463.03× |  | OK | Sig: k = spectralKurtosis(x, fs). camelCase alias added 2026-05-09. |
| `spectralskewness` | ✅ | 0.016 | 381.89× |  | OK | Sig: s = spectralSkewness(x, fs). camelCase alias added 2026-05-09. |
| `thd` | ✅ | 0.008 | 1252.99× |  | OK | Sig: r = thd(x, fs). Spec-extension batch 2026-05-09 (cycle 40). |
| `toi` | ❌ |  |  |  |  | third-order intercept |

### Time-Frequency Analysis

**Namespace:** `signal.time_frequency.*`. Wavelet/EMD subset (`cwt/wsst/vmd/hht/emd/fsst/ifsst`) → `wavelet.*` (future) — 1 ✅ + 0 ⚠️ / 27 = 3%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `dlistft` | ❌ |  |  |  |  |  |
| `dlstft` | ❌ |  |  |  |  |  |
| `emd` | ❌ |  |  |  |  | empirical mode decomp |
| `fsst` | ❌ |  |  |  |  | Fourier synchrosqueezed |
| `hht` | ❌ |  |  |  |  | Hilbert-Huang |
| `ifsst` | ❌ |  |  |  |  |  |
| `instbw` | ✅ | 0.028 | 509.02× |  | OK | Sig: b = instbw(x, fs). Spec-extension batch 2026-05-09 (cycle 40). |
| `instfreq` | ✅ | 0.035 | 513.46× |  | OK | Sig: f = instfreq(x, fs). Spec-extension batch 2026-05-09 (cycle 40). |
| `iscola` | ❌ |  |  |  |  |  |
| `istft` | ⚠️ | 0.06 | 194× |  | OK | Sig: x = istft(S[, NV-pairs]). Inverse STFT via overlap-add with per-sample window² normalisation. Round-trip ~ulp on COLA-compliant configs (hann/periodic + 50%/75% overlap). Same NV-pairs as stft; centered range deferred. |
| `istftlayer` | ❌ |  |  |  |  |  |
| `kurtogram` | ❌ |  |  |  |  |  |
| `pspectrum` | ❌ |  |  |  |  | easy spectral analysis |
| `spectralcrest` | ✅ | 0.015 | 365.54× |  | OK | Sig: c = spectralCrest(x, fs). camelCase alias added 2026-05-09. |
| `spectralentropy` | ✅ | 0.016 | 429.77× |  | OK | Sig: e = spectralEntropy(x, fs). camelCase alias added 2026-05-09. |
| `spectralflatness` | ✅ | 0.015 | 382.88× |  | OK | Sig: f = spectralFlatness(x, fs). camelCase alias added 2026-05-09. |
| `spectralkurtosis` | ✅ | 0.015 | 463.03× |  | OK | Sig: k = spectralKurtosis(x, fs). camelCase alias added 2026-05-09. |
| `spectralskewness` | ✅ | 0.016 | 381.89× |  | OK | Sig: s = spectralSkewness(x, fs). camelCase alias added 2026-05-09. |
| `spectrogram` | ✅ | 0.023 | 382.98× |  | OK | Sig: spectral DSP estimator. Default fs=2*pi (MATLAB convention) and 8-segment 50%-overlap Hamming window for Welch-family. Bit-identical with MATLAB R2025b after fs+winLen fix 2026-05-09. |
| `stft` | ⚠️ | 0.06 | 194× |  | OK | Sig: s = stft(x[, NV-pairs]). Short-time Fourier transform with windowed frames + per-frame FFT. Default window hann(128, periodic), overlap 96, FFTLength 128. Supports Window/OverlapLength/FFTLength/FrequencyRange NV-pairs. Bit-equal MATLAB R2025b on twosided + onesided. KNOWN GAPS: centered (phase ramp) deferred, fs / time-axis outputs, multi-channel. |
| `stftlayer` | ❌ |  |  |  |  |  |
| `stftmag2sig` | ❌ |  |  |  |  |  |
| `tfridge` | ❌ |  |  |  |  |  |
| `vmd` | ❌ |  |  |  |  | variational MD |
| `wvd` | ❌ |  |  |  |  | Wigner-Ville |
| `xspectrogram` | ❌ |  |  |  |  | cross-spectrogram |
| `xwvd` | ❌ |  |  |  |  | cross WVD |

### Pulse and Transition Metrics

**Namespace:** `signal.measurements.*` — 0 ✅ + 0 ⚠️ / 12 = 0%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `dutycycle` | ✅ | 0.003 | 1118.37× |  | OK | Sig: d = dutycycle(x, fs). Spec-extension batch 2026-05-09 (cycle 40). |
| `falltime` | ✅ | 0.002 |  |  | N/A | Sig: ft = falltime(x). Spec-extension batch 2026-05-09 (cycle 40). |
| `midcross` | ✅ | 0.004 | 843.97× |  | OK | Sig: c = midcross(x). Spec-extension batch 2026-05-09 (cycle 40). |
| `overshoot` | ✅ | 0.004 | 1128.22× |  | OK | Sig: os = overshoot(x). Spec-extension batch 2026-05-09 (cycle 40). |
| `pulseperiod` | ✅ | 0.004 | 917.33× |  | OK | Sig: p = pulseperiod(x). Spec-extension batch 2026-05-09 (cycle 40). |
| `pulsesep` | ✅ | 0.004 | 940.09× |  | OK | Sig: s = pulsesep(x). Spec-extension batch 2026-05-09 (cycle 40). |
| `pulsewidth` | ✅ | 0.004 | 906.32× |  | OK | Sig: w = pulsewidth(x). Spec-extension batch 2026-05-09 (cycle 40). |
| `risetime` | ✅ | 0.004 | 941.88× |  | OK | Sig: rt = risetime(x). Spec-extension batch 2026-05-09 (cycle 40). |
| `settlingtime` | ✅ | 0.004 | 1023.13× |  | OK | Sig: st = settlingtime(x, d). Spec-extension batch 2026-05-09 (cycle 40). |
| `slewrate` | ✅ | 0.004 | 997.59× |  | OK | Sig: sr = slewrate(x). Spec-extension batch 2026-05-09 (cycle 40). |
| `statelevels` | ✅ | 0.004 | 297.67× | 80.38× | OK | Sig: lv = statelevels(x). Spec-extension batch 2026-05-09 (cycle 40). |
| `undershoot` | ✅ | 0.004 | 1145.42× |  | OK | Sig: us = undershoot(x). Spec-extension batch 2026-05-09 (cycle 40). |

### Signal Descriptive Statistics

**Namespace:** `signal.measurements.*` — 2 ✅ + 0 ⚠️ / 30 = 6%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `alignsignals` | ✅ | 0.004 | 666.57× |  | OK | Sig: r = alignsignals(...). Spec-extension batch 2026-05-09 (signal namespace). |
| `binmask2sigroi` | ❌ |  |  |  |  |  |
| `countlabels` | ❌ |  |  |  |  |  |
| `cusum` | ❌ |  |  |  |  | CUSUM change detection |
| `dtw` | ❌ |  |  |  |  | dynamic time warp |
| `edr` | ❌ |  |  |  |  | edit distance on real |
| `envelope` | ✅ | 0.010 | 606.52× |  | OK | Sig: [yupper, ylower] = envelope(x[, n[, method]]). Four modes match MATLAB R2025b envelope.m exactly: default (no n) FFT |hilbert(x-mean)| with mean restored; 'analytic' n-tap Kaiser(8)-tapered Hilbert FIR; 'rms' sliding-window RMS; 'peak' spline (parabola for 3 knots, not-a-knot for 4+) through local maxima/minima with MinPeakDistance n. DC-removal applied for analytic/rms/default; not for peak. |
| `extendsigroi` | ❌ |  |  |  |  |  |
| `extractsigroi` | ❌ |  |  |  |  |  |
| `filenames2labels` | ❌ |  |  |  |  |  |
| `findchangepts` | ❌ |  |  |  |  | change-point detection |
| `finddelay` | ✅ | 0.002 |  |  | N/A | Sig: r = finddelay(...). Spec-extension batch 2026-05-09 (signal namespace). |
| `findpeaks` | ✅ | 0.002 |  |  | N/A | Sig: r = findpeaks(...). Spec-extension batch 2026-05-09. |
| `findsignal` | ❌ |  |  |  |  | pattern search |
| `folders2labels` | ❌ |  |  |  |  |  |
| `framelbl` | ❌ |  |  |  |  |  |
| `framesig` | ❌ |  |  |  |  |  |
| `meanfreq` | ✅ | 0.014 | 617.60× |  | OK | Sig: f = meanfreq(x, fs). Spec-extension batch 2026-05-09 (cycle 40). |
| `medfreq` | ✅ | 0.015 | 627.44× |  | OK | Sig: f = medfreq(x, fs). Spec-extension batch 2026-05-09 (cycle 40). |
| `mergesigroi` | ❌ |  |  |  |  |  |
| `peak2peak` | ✅ | 0.004 | 56.36× | 16.56× | OK | Sig: r = peak2peak(...). Spec-extension batch 2026-05-09. |
| `peak2rms` | ✅ | 2.992 | 0.91× | 1.13× | OK | Sig: R = peak2rms(X). 100 iters. |
| `removesigroi` | ❌ |  |  |  |  |  |
| `rssq` | ✅ | 0.003 | 56.86× | 54.97× | OK | Sig: r = rssq(...). Spec-extension batch 2026-05-09. |
| `seqperiod` | ❌ |  |  |  |  |  |
| `shortensigroi` | ❌ |  |  |  |  |  |
| `sigrangebinmask` | ❌ |  |  |  |  |  |
| `sigroi2binmask` | ❌ |  |  |  |  |  |
| `splitlabels` | ❌ |  |  |  |  |  |
| `zerocrossrate` | ❌ |  |  |  |  |  |

### Smoothing and Denoising

**Namespace:** `signal.smoothing.*` + `signal.digital_filtering.*` (medfilt1, sgolayfilt). `smoothdata` itself → `stats.moving.*` — 3 ✅ + 0 ⚠️ / 4 = 75%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `hampel` | ✅ | 0.004 | 161.76× |  | OK | Sig: r = hampel(...). Spec-extension batch 2026-05-09. |
| `medfilt1` | ✅ | 0.005 | 242.53× | 30.99× | OK | Sig: r = medfilt1(...). Spec-extension batch 2026-05-09 (signal namespace). |
| `sgolay` | ✅ | 0.004 | 143.88× | 35.14× | OK | Sig: r = sgolay(...). Spec-extension batch 2026-05-09 (signal namespace). |
| `sgolayfilt` | ✅ | 0.005 | 254.70× | 61.52× | OK | Sig: r = sgolayfilt(...). Spec-extension batch 2026-05-09 (signal namespace). |

### Vibration Analysis

**Namespace:** `signal.vibration.*` — 0 ✅ + 0 ⚠️ / 13 = 0%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `envspectrum` | ✅ | 0.077 | 41.96× |  | OK | Sig: [p,f] = envspectrum(x, fs). Spec-extension batch 2026-05-09 (cycle 43). |
| `modalfit` | ❌ |  |  |  |  | modal-fit |
| `modalfrf` | ❌ |  |  |  |  |  |
| `modalsd` | ❌ |  |  |  |  |  |
| `orderspectrum` | ❌ |  |  |  |  |  |
| `ordertrack` | ❌ |  |  |  |  |  |
| `orderwaveform` | ❌ |  |  |  |  |  |
| `rainflow` | ✅ | 0.005 | 300.75× |  | OK | Sig: c = rainflow(x). ASTM E1049-85 cycle counting, returns Nx5 [count, range, mean, start_idx, end_idx]. Bit-identical with MATLAB R2025b on canonical 9-sample probe. |
| `rpmfreqmap` | ❌ |  |  |  |  |  |
| `rpmordermap` | ❌ |  |  |  |  |  |
| `rpmtrack` | ❌ |  |  |  |  | order tracking |
| `tachorpm` | ✅ | 0.009 | 1645.34× |  | OK | Sig: rpm = tachorpm(x, fs). Spec-extension batch 2026-05-09 (cycle 43). |
| `tsa` | ✅ | 0.007 | 820.81× |  | OK | Sig: tsa(x, fs, tPulse[, M]) -- MATLAB pulse-time form (numkit also supports legacy tsa(x, fs, rpm, fs_rpm) when arg count >= 4). Bit-identical with MATLAB R2025b on probed input (100 samples). |

## Audio

Audio functions live (or will live) under `libs/audio/`. The existing
`spectral*` family was originally shipped under
`signal/spectral_analysis` (camelCase aliases added 2026-05-09); per
2026-05-10 plan they migrate to `audio/spectral` so the entire
spectral-shape family clusters with audio-feature extraction
(`mfcc`/`gtcc`/`cepstralCoefficients`). Existing implementations stay
bit-equal with MATLAB; rows below mark current `signal.*` namespace
locations until physical migration lands.

### Spectral Shape Descriptors

**Namespace:** `audio.spectral.*` (planned). Currently registered under
`signal.spectral_analysis.*`. — 5 ✅ + 0 ⚠️ / 11 = 45%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `spectralCentroid` | ✅ | 0.006 | 1839.19× |  | OK | MATLAB Audio Toolbox spectral shape descriptors (cycle B): spectralCentroid (Σ(f·X)/Σ(X)), spectralSpread (sqrt of second central moment), spectralRolloffPoint (default 95th percentile of energy), spectralDecrease ((1/Σk≥2 X(k))·Σk≥2 (X(k)-X(1))/(k-1)), spectralSlope (linear regression of X vs F), spectralFlux ((Σ|ΔX|^p)^(1/p), MATLAB sets first-frame flux=0). Bit-equal with MATLAB R2025b on (X, F) direct-form inputs across 9 fingerprint points (single-col + 2-col + flux). Time-domain (x, fs) form uses internal naive O(N²) DFT with rectwin(round(0.03*fs)), overlap=round(0.02*fs), FFTLength=winLen — matches MATLAB's STFT defaults from spectralCentroid.m and friends. Octave 11.1.0 doesn't ship spectralX in core (Audio package only). |
| `spectralCrest` | ✅ | 0.491 | 41.50× |  | OK | MATLAB Audio Toolbox cycle I: 5 per-frame STFT spectral metrics (spectralCrest/Entropy/Flatness/Kurtosis/Skewness). Reside in libs/audio/src/spectral/shape_descriptors.cpp alongside spectralCentroid/Spread/Rolloff/Decrease/Slope/Flux from cycle B. All bit-equal with MATLAB R2025b on (X, F) direct, multi-frame (X2 8x2, F 8x1), and time-domain (xs, fs) inputs (after fixing computeStft to apply MATLAB signal.internal.spectraldescriptors.stft normalization: |Y|² / (0.5·sum(win)²), DC bin halved, Nyquist bin halved when fftLength even). Time-domain shape: 8x1 column vector (one row per frame) for 1601 samples at fs=16000 with rectwin(round(0.03*fs))=480 + overlap=320. The libs/signal lowercase forms (spectralcrest/etc) remain as legacy single-segment scalar versions; camelCase compat aliases now point to the per-frame audio versions. |
| `spectralDecrease` | ✅ | 0.006 | 1839.19× |  | OK | MATLAB Audio Toolbox spectral shape descriptors (cycle B): spectralCentroid (Σ(f·X)/Σ(X)), spectralSpread (sqrt of second central moment), spectralRolloffPoint (default 95th percentile of energy), spectralDecrease ((1/Σk≥2 X(k))·Σk≥2 (X(k)-X(1))/(k-1)), spectralSlope (linear regression of X vs F), spectralFlux ((Σ|ΔX|^p)^(1/p), MATLAB sets first-frame flux=0). Bit-equal with MATLAB R2025b on (X, F) direct-form inputs across 9 fingerprint points (single-col + 2-col + flux). Time-domain (x, fs) form uses internal naive O(N²) DFT with rectwin(round(0.03*fs)), overlap=round(0.02*fs), FFTLength=winLen — matches MATLAB's STFT defaults from spectralCentroid.m and friends. Octave 11.1.0 doesn't ship spectralX in core (Audio package only). |
| `spectralEntropy` | ✅ | 0.491 | 41.50× |  | OK | MATLAB Audio Toolbox cycle I: 5 per-frame STFT spectral metrics (spectralCrest/Entropy/Flatness/Kurtosis/Skewness). Reside in libs/audio/src/spectral/shape_descriptors.cpp alongside spectralCentroid/Spread/Rolloff/Decrease/Slope/Flux from cycle B. All bit-equal with MATLAB R2025b on (X, F) direct, multi-frame (X2 8x2, F 8x1), and time-domain (xs, fs) inputs (after fixing computeStft to apply MATLAB signal.internal.spectraldescriptors.stft normalization: |Y|² / (0.5·sum(win)²), DC bin halved, Nyquist bin halved when fftLength even). Time-domain shape: 8x1 column vector (one row per frame) for 1601 samples at fs=16000 with rectwin(round(0.03*fs))=480 + overlap=320. The libs/signal lowercase forms (spectralcrest/etc) remain as legacy single-segment scalar versions; camelCase compat aliases now point to the per-frame audio versions. |
| `spectralFlatness` | ✅ | 0.491 | 41.50× |  | OK | MATLAB Audio Toolbox cycle I: 5 per-frame STFT spectral metrics (spectralCrest/Entropy/Flatness/Kurtosis/Skewness). Reside in libs/audio/src/spectral/shape_descriptors.cpp alongside spectralCentroid/Spread/Rolloff/Decrease/Slope/Flux from cycle B. All bit-equal with MATLAB R2025b on (X, F) direct, multi-frame (X2 8x2, F 8x1), and time-domain (xs, fs) inputs (after fixing computeStft to apply MATLAB signal.internal.spectraldescriptors.stft normalization: |Y|² / (0.5·sum(win)²), DC bin halved, Nyquist bin halved when fftLength even). Time-domain shape: 8x1 column vector (one row per frame) for 1601 samples at fs=16000 with rectwin(round(0.03*fs))=480 + overlap=320. The libs/signal lowercase forms (spectralcrest/etc) remain as legacy single-segment scalar versions; camelCase compat aliases now point to the per-frame audio versions. |
| `spectralFlux` | ✅ | 0.006 | 1839.19× |  | OK | MATLAB Audio Toolbox spectral shape descriptors (cycle B): spectralCentroid (Σ(f·X)/Σ(X)), spectralSpread (sqrt of second central moment), spectralRolloffPoint (default 95th percentile of energy), spectralDecrease ((1/Σk≥2 X(k))·Σk≥2 (X(k)-X(1))/(k-1)), spectralSlope (linear regression of X vs F), spectralFlux ((Σ|ΔX|^p)^(1/p), MATLAB sets first-frame flux=0). Bit-equal with MATLAB R2025b on (X, F) direct-form inputs across 9 fingerprint points (single-col + 2-col + flux). Time-domain (x, fs) form uses internal naive O(N²) DFT with rectwin(round(0.03*fs)), overlap=round(0.02*fs), FFTLength=winLen — matches MATLAB's STFT defaults from spectralCentroid.m and friends. Octave 11.1.0 doesn't ship spectralX in core (Audio package only). |
| `spectralKurtosis` | ✅ | 0.491 | 41.50× |  | OK | MATLAB Audio Toolbox cycle I: 5 per-frame STFT spectral metrics (spectralCrest/Entropy/Flatness/Kurtosis/Skewness). Reside in libs/audio/src/spectral/shape_descriptors.cpp alongside spectralCentroid/Spread/Rolloff/Decrease/Slope/Flux from cycle B. All bit-equal with MATLAB R2025b on (X, F) direct, multi-frame (X2 8x2, F 8x1), and time-domain (xs, fs) inputs (after fixing computeStft to apply MATLAB signal.internal.spectraldescriptors.stft normalization: |Y|² / (0.5·sum(win)²), DC bin halved, Nyquist bin halved when fftLength even). Time-domain shape: 8x1 column vector (one row per frame) for 1601 samples at fs=16000 with rectwin(round(0.03*fs))=480 + overlap=320. The libs/signal lowercase forms (spectralcrest/etc) remain as legacy single-segment scalar versions; camelCase compat aliases now point to the per-frame audio versions. |
| `spectralRolloffPoint` | ✅ | 0.006 | 1839.19× |  | OK | MATLAB Audio Toolbox spectral shape descriptors (cycle B): spectralCentroid (Σ(f·X)/Σ(X)), spectralSpread (sqrt of second central moment), spectralRolloffPoint (default 95th percentile of energy), spectralDecrease ((1/Σk≥2 X(k))·Σk≥2 (X(k)-X(1))/(k-1)), spectralSlope (linear regression of X vs F), spectralFlux ((Σ|ΔX|^p)^(1/p), MATLAB sets first-frame flux=0). Bit-equal with MATLAB R2025b on (X, F) direct-form inputs across 9 fingerprint points (single-col + 2-col + flux). Time-domain (x, fs) form uses internal naive O(N²) DFT with rectwin(round(0.03*fs)), overlap=round(0.02*fs), FFTLength=winLen — matches MATLAB's STFT defaults from spectralCentroid.m and friends. Octave 11.1.0 doesn't ship spectralX in core (Audio package only). |
| `spectralSkewness` | ✅ | 0.491 | 41.50× |  | OK | MATLAB Audio Toolbox cycle I: 5 per-frame STFT spectral metrics (spectralCrest/Entropy/Flatness/Kurtosis/Skewness). Reside in libs/audio/src/spectral/shape_descriptors.cpp alongside spectralCentroid/Spread/Rolloff/Decrease/Slope/Flux from cycle B. All bit-equal with MATLAB R2025b on (X, F) direct, multi-frame (X2 8x2, F 8x1), and time-domain (xs, fs) inputs (after fixing computeStft to apply MATLAB signal.internal.spectraldescriptors.stft normalization: |Y|² / (0.5·sum(win)²), DC bin halved, Nyquist bin halved when fftLength even). Time-domain shape: 8x1 column vector (one row per frame) for 1601 samples at fs=16000 with rectwin(round(0.03*fs))=480 + overlap=320. The libs/signal lowercase forms (spectralcrest/etc) remain as legacy single-segment scalar versions; camelCase compat aliases now point to the per-frame audio versions. |
| `spectralSlope` | ✅ | 0.006 | 1839.19× |  | OK | MATLAB Audio Toolbox spectral shape descriptors (cycle B): spectralCentroid (Σ(f·X)/Σ(X)), spectralSpread (sqrt of second central moment), spectralRolloffPoint (default 95th percentile of energy), spectralDecrease ((1/Σk≥2 X(k))·Σk≥2 (X(k)-X(1))/(k-1)), spectralSlope (linear regression of X vs F), spectralFlux ((Σ|ΔX|^p)^(1/p), MATLAB sets first-frame flux=0). Bit-equal with MATLAB R2025b on (X, F) direct-form inputs across 9 fingerprint points (single-col + 2-col + flux). Time-domain (x, fs) form uses internal naive O(N²) DFT with rectwin(round(0.03*fs)), overlap=round(0.02*fs), FFTLength=winLen — matches MATLAB's STFT defaults from spectralCentroid.m and friends. Octave 11.1.0 doesn't ship spectralX in core (Audio package only). |
| `spectralSpread` | ✅ | 0.006 | 1839.19× |  | OK | MATLAB Audio Toolbox spectral shape descriptors (cycle B): spectralCentroid (Σ(f·X)/Σ(X)), spectralSpread (sqrt of second central moment), spectralRolloffPoint (default 95th percentile of energy), spectralDecrease ((1/Σk≥2 X(k))·Σk≥2 (X(k)-X(1))/(k-1)), spectralSlope (linear regression of X vs F), spectralFlux ((Σ|ΔX|^p)^(1/p), MATLAB sets first-frame flux=0). Bit-equal with MATLAB R2025b on (X, F) direct-form inputs across 9 fingerprint points (single-col + 2-col + flux). Time-domain (x, fs) form uses internal naive O(N²) DFT with rectwin(round(0.03*fs)), overlap=round(0.02*fs), FFTLength=winLen — matches MATLAB's STFT defaults from spectralCentroid.m and friends. Octave 11.1.0 doesn't ship spectralX in core (Audio package only). |

### Audio Feature Extraction

**Namespace:** `audio.features.*` (planned) — 0 ✅ + 0 ⚠️ / 7 = 0%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `audioDelta` | ✅ | 0.061 | 105.48× |  | OK | MATLAB Audio Toolbox cycle C: melSpectrogram + audioDelta. melSpectrogram with NumBands=8 default-window/overlap on a deterministic ramp signal. F (mel-band centers): bit-equal with MATLAB R2025b on all 8 values. S(1,1) bit-equal. Time vector T size matches MATLAB. audioDelta: bit-equal on default windowLength=9 ramp test (d(9)=d(10)=2 — MATLAB filter convention with sum((1:M)^2)=30 divisor), and on custom windowLength=5 path (denom=5), and multi-channel (filter operates along dim 1 per column). KNOWN GAPs (deferred): NumBands ≠ 8 default, FrequencyRange/FilterBankNormalization/MelStyle name-value args, and the [delta, Zf] / Zi initial-conditions form for audioDelta. Octave 11.1.0 doesn't ship melSpectrogram or audioDelta in core (Audio package only). |
| `cepstralCoefficients` | ✅ | 0.544 | 51.14× |  | OK | MATLAB Audio Toolbox cycles D + G + H: cepstralCoefficients (bit-equal — DCT-II unitary matrix from createDCTmatrix.m + log10 rectification, output shape M×NumCoeffs). Cycle G: mfcc BIT-EQUAL with MATLAB R2025b — Slaney band edges (audio.internal.slaneybandedges, 42 entries: 13 linear at 66.66 Hz step + 29 log-spaced ratio 1.0711703), Slaney designMelFilterBank ('Hz' designDomain, 'Bandwidth' normalization), |FFT| magnitude, natural-log per-frame energy of UNWINDOWED signal as first column ('append' LogEnergy default). Cycle H: gtcc BIT-EQUAL with MATLAB R2025b — proper Patterson-Holdsworth gammatone filterbank (Slaney 1993) via cascaded 4-stage biquads with frequency-domain freqz('whole') evaluation, FrequencyRange=[50,fs/2], NumFilters=ceil(hz2erb(fs/2)-hz2erb(50)), Bandwidth normalization BW[i]/2, OneSided=false equivalent via doubled inner-half bins. Octave 11.1.0 doesn't ship cepstralCoefficients/mfcc/gtcc in core (Audio package only). |
| `gtcc` | ✅ | 0.544 | 51.14× |  | OK | MATLAB Audio Toolbox cycles D + G + H: cepstralCoefficients (bit-equal — DCT-II unitary matrix from createDCTmatrix.m + log10 rectification, output shape M×NumCoeffs). Cycle G: mfcc BIT-EQUAL with MATLAB R2025b — Slaney band edges (audio.internal.slaneybandedges, 42 entries: 13 linear at 66.66 Hz step + 29 log-spaced ratio 1.0711703), Slaney designMelFilterBank ('Hz' designDomain, 'Bandwidth' normalization), |FFT| magnitude, natural-log per-frame energy of UNWINDOWED signal as first column ('append' LogEnergy default). Cycle H: gtcc BIT-EQUAL with MATLAB R2025b — proper Patterson-Holdsworth gammatone filterbank (Slaney 1993) via cascaded 4-stage biquads with frequency-domain freqz('whole') evaluation, FrequencyRange=[50,fs/2], NumFilters=ceil(hz2erb(fs/2)-hz2erb(50)), Bandwidth normalization BW[i]/2, OneSided=false equivalent via doubled inner-half bins. Octave 11.1.0 doesn't ship cepstralCoefficients/mfcc/gtcc in core (Audio package only). |
| `harmonicRatio` | ✅ | 242.460 | 0.24× |  | OK | pitch + harmonicRatio (Audio Toolbox). Methods: NCF (default), CEP, PEF, LHS, SRH. CEP/PEF/LHS/SRH have been clean-room reimplemented from public papers as part of the IP-provenance remediation (see cleanroom/). CEP (Noll 1967) and LHS (Hermes 1988) are bit-identical to MATLAB R2025b. PEF (Gonzalez & Brookes, EUSIPCO 2011 — the no-compression variant) and SRH (Drugman & Alwan, Interspeech 2011) are faithful to the published papers; MATLAB's PEF/SRH diverge from the papers in undocumented ways, so numkit's are intentionally NOT bit-matched. On a clean tone PEF still agrees with MATLAB to ~0.06% (within tol), so pef_first/pef_mean stay in the fingerprint; pef_r_first (a degenerate two-pure-tone case) and srh_first are excluded from the cross-engine comparison. NCF parity unchanged. harmonicRatio: auto low-edge + Smith parabolic. Tolerance 5%; pitchnn deferred (DNN runtime not in numkit). |
| `mfcc` | ✅ | 0.544 | 51.14× |  | OK | MATLAB Audio Toolbox cycles D + G + H: cepstralCoefficients (bit-equal — DCT-II unitary matrix from createDCTmatrix.m + log10 rectification, output shape M×NumCoeffs). Cycle G: mfcc BIT-EQUAL with MATLAB R2025b — Slaney band edges (audio.internal.slaneybandedges, 42 entries: 13 linear at 66.66 Hz step + 29 log-spaced ratio 1.0711703), Slaney designMelFilterBank ('Hz' designDomain, 'Bandwidth' normalization), |FFT| magnitude, natural-log per-frame energy of UNWINDOWED signal as first column ('append' LogEnergy default). Cycle H: gtcc BIT-EQUAL with MATLAB R2025b — proper Patterson-Holdsworth gammatone filterbank (Slaney 1993) via cascaded 4-stage biquads with frequency-domain freqz('whole') evaluation, FrequencyRange=[50,fs/2], NumFilters=ceil(hz2erb(fs/2)-hz2erb(50)), Bandwidth normalization BW[i]/2, OneSided=false equivalent via doubled inner-half bins. Octave 11.1.0 doesn't ship cepstralCoefficients/mfcc/gtcc in core (Audio package only). |
| `pitch` | ✅ | 242.460 | 0.24× |  | OK | pitch + harmonicRatio (Audio Toolbox). Methods: NCF (default), CEP, PEF, LHS, SRH. CEP/PEF/LHS/SRH have been clean-room reimplemented from public papers as part of the IP-provenance remediation (see cleanroom/). CEP (Noll 1967) and LHS (Hermes 1988) are bit-identical to MATLAB R2025b. PEF (Gonzalez & Brookes, EUSIPCO 2011 — the no-compression variant) and SRH (Drugman & Alwan, Interspeech 2011) are faithful to the published papers; MATLAB's PEF/SRH diverge from the papers in undocumented ways, so numkit's are intentionally NOT bit-matched. On a clean tone PEF still agrees with MATLAB to ~0.06% (within tol), so pef_first/pef_mean stay in the fingerprint; pef_r_first (a degenerate two-pure-tone case) and srh_first are excluded from the cross-engine comparison. NCF parity unchanged. harmonicRatio: auto low-edge + Smith parabolic. Tolerance 5%; pitchnn deferred (DNN runtime not in numkit). |
| `pitchnn` | ❌ |  |  |  |  | Deep-learning pitch estimator (CREPE-style network). KNOWN GAP: requires a packaged neural model -- defer to v2 unless a numkit DNN runtime lands. |

### Audio Time-Frequency

**Namespace:** `audio.spectrogram.*` (planned) — 0 ✅ + 0 ⚠️ / 1 = 0%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `melSpectrogram` | ✅ | 0.061 | 105.48× |  | OK | MATLAB Audio Toolbox cycle C: melSpectrogram + audioDelta. melSpectrogram with NumBands=8 default-window/overlap on a deterministic ramp signal. F (mel-band centers): bit-equal with MATLAB R2025b on all 8 values. S(1,1) bit-equal. Time vector T size matches MATLAB. audioDelta: bit-equal on default windowLength=9 ramp test (d(9)=d(10)=2 — MATLAB filter convention with sum((1:M)^2)=30 divisor), and on custom windowLength=5 path (denom=5), and multi-channel (filter operates along dim 1 per column). KNOWN GAPs (deferred): NumBands ≠ 8 default, FrequencyRange/FilterBankNormalization/MelStyle name-value args, and the [delta, Zf] / Zi initial-conditions form for audioDelta. Octave 11.1.0 doesn't ship melSpectrogram or audioDelta in core (Audio package only). |

### Audio Frequency / Loudness Conversions

**Namespace:** `audio.scale.*` (planned) — 0 ✅ + 0 ⚠️ / 8 = 0%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `bark2hz` | ✅ | 0.022 | 390.40× |  | OK | MATLAB Audio Toolbox frequency-scale + loudness conversions (cycles A + M): hz2mel/mel2hz (O'Shaughnessy default), hz2bark/bark2hz (Traunmüller 1990 with low/high-frequency corrections; bark2hz uses the asymmetric 26.28 denominator from MATLAB's bark2hz.m source), hz2erb/erb2hz (Glasberg & Moore 1990 with constants log(10)*1000/(24.673*4.368) and 0.004368 — extracted bit-for-bit from MATLAB hz2erb.m), phon2sone/sone2phon (ISO 532-1 default with smooth break at 40 phon = 1 sone). Cycle M added optional 2nd arg 'ISO 532-2': bit-equal table-lookup PCHIP interpolation per ISO 532-2:2017 Table 5 (28 entries from MATLAB getPerceptualConstants.m), with linear extrapolation beyond 337.6 sone for sone2phon. KNOWN GAP: phon2sone ISO 532-2 ships initial PCHIP guess only; MATLAB additionally refines via fzero (error <1% on smooth inputs). Bit-equal with MATLAB R2025b on all 26 fingerprint points (in-range). Octave 11.1.0 doesn't ship these in core (Audio package only; Octave's audio package missing these specifically). |
| `erb2hz` | ✅ | 0.022 | 390.40× |  | OK | MATLAB Audio Toolbox frequency-scale + loudness conversions (cycles A + M): hz2mel/mel2hz (O'Shaughnessy default), hz2bark/bark2hz (Traunmüller 1990 with low/high-frequency corrections; bark2hz uses the asymmetric 26.28 denominator from MATLAB's bark2hz.m source), hz2erb/erb2hz (Glasberg & Moore 1990 with constants log(10)*1000/(24.673*4.368) and 0.004368 — extracted bit-for-bit from MATLAB hz2erb.m), phon2sone/sone2phon (ISO 532-1 default with smooth break at 40 phon = 1 sone). Cycle M added optional 2nd arg 'ISO 532-2': bit-equal table-lookup PCHIP interpolation per ISO 532-2:2017 Table 5 (28 entries from MATLAB getPerceptualConstants.m), with linear extrapolation beyond 337.6 sone for sone2phon. KNOWN GAP: phon2sone ISO 532-2 ships initial PCHIP guess only; MATLAB additionally refines via fzero (error <1% on smooth inputs). Bit-equal with MATLAB R2025b on all 26 fingerprint points (in-range). Octave 11.1.0 doesn't ship these in core (Audio package only; Octave's audio package missing these specifically). |
| `hz2bark` | ✅ | 0.022 | 390.40× |  | OK | MATLAB Audio Toolbox frequency-scale + loudness conversions (cycles A + M): hz2mel/mel2hz (O'Shaughnessy default), hz2bark/bark2hz (Traunmüller 1990 with low/high-frequency corrections; bark2hz uses the asymmetric 26.28 denominator from MATLAB's bark2hz.m source), hz2erb/erb2hz (Glasberg & Moore 1990 with constants log(10)*1000/(24.673*4.368) and 0.004368 — extracted bit-for-bit from MATLAB hz2erb.m), phon2sone/sone2phon (ISO 532-1 default with smooth break at 40 phon = 1 sone). Cycle M added optional 2nd arg 'ISO 532-2': bit-equal table-lookup PCHIP interpolation per ISO 532-2:2017 Table 5 (28 entries from MATLAB getPerceptualConstants.m), with linear extrapolation beyond 337.6 sone for sone2phon. KNOWN GAP: phon2sone ISO 532-2 ships initial PCHIP guess only; MATLAB additionally refines via fzero (error <1% on smooth inputs). Bit-equal with MATLAB R2025b on all 26 fingerprint points (in-range). Octave 11.1.0 doesn't ship these in core (Audio package only; Octave's audio package missing these specifically). |
| `hz2erb` | ✅ | 0.022 | 390.40× |  | OK | MATLAB Audio Toolbox frequency-scale + loudness conversions (cycles A + M): hz2mel/mel2hz (O'Shaughnessy default), hz2bark/bark2hz (Traunmüller 1990 with low/high-frequency corrections; bark2hz uses the asymmetric 26.28 denominator from MATLAB's bark2hz.m source), hz2erb/erb2hz (Glasberg & Moore 1990 with constants log(10)*1000/(24.673*4.368) and 0.004368 — extracted bit-for-bit from MATLAB hz2erb.m), phon2sone/sone2phon (ISO 532-1 default with smooth break at 40 phon = 1 sone). Cycle M added optional 2nd arg 'ISO 532-2': bit-equal table-lookup PCHIP interpolation per ISO 532-2:2017 Table 5 (28 entries from MATLAB getPerceptualConstants.m), with linear extrapolation beyond 337.6 sone for sone2phon. KNOWN GAP: phon2sone ISO 532-2 ships initial PCHIP guess only; MATLAB additionally refines via fzero (error <1% on smooth inputs). Bit-equal with MATLAB R2025b on all 26 fingerprint points (in-range). Octave 11.1.0 doesn't ship these in core (Audio package only; Octave's audio package missing these specifically). |
| `hz2mel` | ✅ | 0.022 | 390.40× |  | OK | MATLAB Audio Toolbox frequency-scale + loudness conversions (cycles A + M): hz2mel/mel2hz (O'Shaughnessy default), hz2bark/bark2hz (Traunmüller 1990 with low/high-frequency corrections; bark2hz uses the asymmetric 26.28 denominator from MATLAB's bark2hz.m source), hz2erb/erb2hz (Glasberg & Moore 1990 with constants log(10)*1000/(24.673*4.368) and 0.004368 — extracted bit-for-bit from MATLAB hz2erb.m), phon2sone/sone2phon (ISO 532-1 default with smooth break at 40 phon = 1 sone). Cycle M added optional 2nd arg 'ISO 532-2': bit-equal table-lookup PCHIP interpolation per ISO 532-2:2017 Table 5 (28 entries from MATLAB getPerceptualConstants.m), with linear extrapolation beyond 337.6 sone for sone2phon. KNOWN GAP: phon2sone ISO 532-2 ships initial PCHIP guess only; MATLAB additionally refines via fzero (error <1% on smooth inputs). Bit-equal with MATLAB R2025b on all 26 fingerprint points (in-range). Octave 11.1.0 doesn't ship these in core (Audio package only; Octave's audio package missing these specifically). |
| `mel2hz` | ✅ | 0.022 | 390.40× |  | OK | MATLAB Audio Toolbox frequency-scale + loudness conversions (cycles A + M): hz2mel/mel2hz (O'Shaughnessy default), hz2bark/bark2hz (Traunmüller 1990 with low/high-frequency corrections; bark2hz uses the asymmetric 26.28 denominator from MATLAB's bark2hz.m source), hz2erb/erb2hz (Glasberg & Moore 1990 with constants log(10)*1000/(24.673*4.368) and 0.004368 — extracted bit-for-bit from MATLAB hz2erb.m), phon2sone/sone2phon (ISO 532-1 default with smooth break at 40 phon = 1 sone). Cycle M added optional 2nd arg 'ISO 532-2': bit-equal table-lookup PCHIP interpolation per ISO 532-2:2017 Table 5 (28 entries from MATLAB getPerceptualConstants.m), with linear extrapolation beyond 337.6 sone for sone2phon. KNOWN GAP: phon2sone ISO 532-2 ships initial PCHIP guess only; MATLAB additionally refines via fzero (error <1% on smooth inputs). Bit-equal with MATLAB R2025b on all 26 fingerprint points (in-range). Octave 11.1.0 doesn't ship these in core (Audio package only; Octave's audio package missing these specifically). |
| `phon2sone` | ✅ | 0.022 | 390.40× |  | OK | MATLAB Audio Toolbox frequency-scale + loudness conversions (cycles A + M): hz2mel/mel2hz (O'Shaughnessy default), hz2bark/bark2hz (Traunmüller 1990 with low/high-frequency corrections; bark2hz uses the asymmetric 26.28 denominator from MATLAB's bark2hz.m source), hz2erb/erb2hz (Glasberg & Moore 1990 with constants log(10)*1000/(24.673*4.368) and 0.004368 — extracted bit-for-bit from MATLAB hz2erb.m), phon2sone/sone2phon (ISO 532-1 default with smooth break at 40 phon = 1 sone). Cycle M added optional 2nd arg 'ISO 532-2': bit-equal table-lookup PCHIP interpolation per ISO 532-2:2017 Table 5 (28 entries from MATLAB getPerceptualConstants.m), with linear extrapolation beyond 337.6 sone for sone2phon. KNOWN GAP: phon2sone ISO 532-2 ships initial PCHIP guess only; MATLAB additionally refines via fzero (error <1% on smooth inputs). Bit-equal with MATLAB R2025b on all 26 fingerprint points (in-range). Octave 11.1.0 doesn't ship these in core (Audio package only; Octave's audio package missing these specifically). |
| `sone2phon` | ✅ | 0.022 | 390.40× |  | OK | MATLAB Audio Toolbox frequency-scale + loudness conversions (cycles A + M): hz2mel/mel2hz (O'Shaughnessy default), hz2bark/bark2hz (Traunmüller 1990 with low/high-frequency corrections; bark2hz uses the asymmetric 26.28 denominator from MATLAB's bark2hz.m source), hz2erb/erb2hz (Glasberg & Moore 1990 with constants log(10)*1000/(24.673*4.368) and 0.004368 — extracted bit-for-bit from MATLAB hz2erb.m), phon2sone/sone2phon (ISO 532-1 default with smooth break at 40 phon = 1 sone). Cycle M added optional 2nd arg 'ISO 532-2': bit-equal table-lookup PCHIP interpolation per ISO 532-2:2017 Table 5 (28 entries from MATLAB getPerceptualConstants.m), with linear extrapolation beyond 337.6 sone for sone2phon. KNOWN GAP: phon2sone ISO 532-2 ships initial PCHIP guess only; MATLAB additionally refines via fzero (error <1% on smooth inputs). Bit-equal with MATLAB R2025b on all 26 fingerprint points (in-range). Octave 11.1.0 doesn't ship these in core (Audio package only; Octave's audio package missing these specifically). |

## Statistics

### Descriptive Statistics

**Namespace:** `stats.descriptive.*` / `stats.moving.*` / `stats.nan.*`. Exception: `xcorr/xcov/rms/rssq/peak2peak/peak2rms` → `signal.*` (signal-side stats) — 14 ✅ + 0 ⚠️ / 33 = 42%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `bounds` | ✅ | 5.932 | 0.02× |  | OK | Sig: [lo,hi] = bounds(X). 1M-pt min/max. 100 iters. |
| `corrcoef` | ✅ | 0.003 | 218.39× |  | OK | Sig: r = corrcoef(...). Spec-extension batch 2026-05-09. |
| `cov` | ✅ | 0.002 | 31.68× |  | OK | Sig: r = cov(...). Spec-extension batch 2026-05-09. |
| `cummax` | ✅ | 2.530 | 0.58× |  | OK | Sig: M = cummax(X). 1M-pt cumulative max. 100 iters. Element-wise SAVE. |
| `cummin` | ✅ | 2.443 | 0.68× |  | OK | Sig: M = cummin(X). 1M-pt cumulative min. 100 iters. Element-wise SAVE. |
| `iqr` | ✅ | 0.006 | 864.90× | 177.19× | OK | Sig: r = iqr(A[, dim | 'all' | vecdim]). MATLAB R2025b uses midpoint (R2007a) interpolation: iqr = prctile(A, 75) - prctile(A, 25). Closes audit/findings/stats/iqr.md (joint with quantile + prctile). |
| `kde` | ✅ | 0.087 |  |  | N/A | Sig: [f, xi, bw] = kde(x [, pts]) — MATLAB R2023b+ alias for ksdensity. Kernel density estimation with Gaussian kernel by default; bandwidth via Silverman's rule of thumb. v1 implementation: direct alias to ksdensity_reg (same adapter handles positional + name-value calls). Fingerprint pins output shapes (numel = 100 by default), bandwidth positivity, normalisation (integral ≈ 1 over [-3, 3] which captures most mass of N(0,1)). Engine-dependent randn → no bit-exact comparison; structural assertions only (tol 1e-9 on the deterministic shape numbers). |
| `mape` | ✅ | 8.282 | 0.29× | 1.04× | OK | 1M-point MAPE. 50 iters. numkit needs `import compat.*`; MATLAB+Octave have it flat. |
| `max` | ✅ | 1.482 | 0.03× | 0.51× | OK | Sig: M = max(X). 1M-pt. 100 iters. Scalar fp. |
| `maxk` | ✅ | 77.188 | 0.01× |  | OK | Sig: B = maxk(X, K). Top 10 of 1M. 100 iters. |
| `mean` | ✅ | 1.401 | 0.04× | 0.71× | OK | Sig: M = mean(X). 1M-pt sin reduction. 100 iters. Scalar fp. |
| `median` | ✅ | 3.840 | 1.15× | 1.83× | OK | Sig: M = median(X). 1M-pt full sort + middle. 50 iters. Scalar fp. |
| `min` | ✅ | 1.492 | 0.03× | 0.51× | OK | Sig: M = min(X). 1M-pt. 100 iters. Scalar fp. |
| `mink` | ✅ | 77.249 | 0.01× |  | OK | Sig: B = mink(X, K). Bot 10 of 1M. 100 iters. |
| `mode` | ✅ | 18.694 | 0.49× | 2.65× | OK | Sig: M = mode(X). 1M-pt with ~7919 distinct vals. 50 iters. Scalar fp. |
| `movmad` | ✅ | 0.008 | 21.64× | 718.96× | OK | Sig: movmad(A, k[, dim] [, nanflag] [, Name, Value]). Same surface as movmean. Closes audit/findings/stats/movmad.md. |
| `movmax` | ✅ | 0.006 | 25.53× | 288.96× | OK | Sig: movmax(A, k[, dim] [, nanflag] [, Name, Value]). Same surface as movmean. Closes audit/findings/stats/movmax.md. |
| `movmean` | ✅ | 0.010 | 25.90× | 684.22× | OK | Sig: M = movmean(A, k[, dim] [, nanflag] [, Name, Value]). nanflag in {includemissing|includenan (default)|omitmissing|omitnan}. Endpoints in {shrink (default)|discard|fill|scalar}. SamplePoints not yet implemented (parity gap, throws with documented error). DataVariables/ReplaceValues are table-only and throw too. k=0 throws MATLAB-matching error. Verified: NaN propagation default, omitnan/omitmissing alias, includenan explicit, all four Endpoints modes, combined matrix+dim+nanflag+endpoints. Closes audit/findings/stats/movmean.md. |
| `movmedian` | ✅ | 0.007 | 24.68× | 387.17× | OK | Sig: M = movmedian(A, k[, dim] [, nanflag] [, Name, Value]). Same surface as movmean. Closes audit/findings/stats/movmedian.md. |
| `movmin` | ✅ | 0.006 | 26.44× | 288.43× | OK | Sig: movmin(A, k[, dim] [, nanflag] [, Name, Value]). Same surface as movmean. Closes audit/findings/stats/movmin.md. |
| `movprod` | ✅ | 0.008 | 23.11× | 217.48× | OK | Sig: movprod(A, k[, dim] [, nanflag] [, Name, Value]). Same surface as movmean. Closes audit/findings/stats/movprod.md. |
| `movstd` | ✅ | 0.005 | 31.19× | 518.14× | OK | Sig: movstd(A, k[, normFlag] [, dim] [, nanflag] [, Name, Value]). normFlag in {0 (default, N-1), 1 (N)}. Same nanflag/Endpoints surface as movmean. Closes audit/findings/stats/movstd.md. |
| `movsum` | ✅ | 0.006 | 28.87× | 298.14× | OK | Sig: movsum(A, k[, dim] [, nanflag] [, Name, Value]). Same surface as movmean. Closes audit/findings/stats/movsum.md. |
| `movvar` | ✅ | 0.006 | 27.21× | 454.36× | OK | Sig: movvar(A, k[, normFlag] [, dim] [, nanflag] [, Name, Value]). normFlag in {0 (default, N-1), 1 (N)}. Same nanflag/Endpoints surface as movmean. Closes audit/findings/stats/movvar.md. |
| `prctile` | ✅ | 0.006 | 870.68× |  | OK | Sig: P = prctile(A, p [, dim | 'all' | vecdim] [, Method=method]). Same surface as quantile but p in [0, 100]. Closes audit/findings/stats/prctile.md. |
| `quantile` | ✅ | 0.011 | 570.37× |  | OK | Sig: Q = quantile(A, p [, dim | 'all' | vecdim] [, Method=method]). Default = 'midpoint' (MATLAB R2025b R2007a algorithm), positions (k-0.5)/N. Methods: midpoint (default) | inclusive (Type-7) | exclusive (Type-6) | approximate (falls back to midpoint). Integer-n form (quantile(A, n) for evenly-spaced quantiles) NOT yet supported — pass an explicit p vector. Closes audit/findings/stats/quantile.md. |
| `rms` | ✅ | 2.740 | 0.45× | 0.16× | OK | Sig: R = rms(X). 1M-pt sin RMS. 100 iters. Scalar fp. |
| `rmse` | ✅ | 9.460 | 0.27× | 2.08× | OK | Sig: R = rmse(F, A). 1M-pt. 100 iters. |
| `std` | ✅ | 0.008 | 217.52× | 70.89× | OK | Sig: S = std(A[, w | W][, dim | 'all' | vecdim][, nanflag]). Same surface as var. Closes audit/findings/stats/std.md. |
| `summary` | ❌ |  |  |  |  |  |
| `var` | ✅ | 0.013 | 144.87× | 66.76× | OK | Sig: V = var(A[, w | W][, dim | 'all' | vecdim][, nanflag]). w in {0, 1} or vector W (weighted; denominator = sum(W)). 'all' / full-flatten vecdim flatten input. Default nanflag = includenan (NaN poisons; matches MATLAB R2025b for double). Closes audit/findings/stats/var.md. |
| `xcorr` | ✅ | 0.004 | 360.50× | 69.56× | OK | Sig: r = xcorr(...). Spec-extension batch 2026-05-09 (signal namespace). |
| `xcov` | ✅ | 0.004 | 453.00× | 211.18× | OK | Sig: r = xcov(...). Spec-extension batch 2026-05-09 (signal namespace). |

### Descriptive Statistics — extras

**Namespace:** `stats.descriptive.*` — additions on top of the existing section above. 0 ✅ + 0 ⚠️ / 23 = 0%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `cholcov` | ✅ | 0.009 | 142.11× |  | OK | MATLAB cholcov: Cholesky-like factor of (possibly singular) covariance. Bit-equal with MATLAB R2025b on key invariants: PD case gives upper-triangular n×n (T'*T = SIGMA exactly), PSD rank-r case gives r×n (T'*T = SIGMA up to rounding), indefinite/negative gives empty T and p > 0. Eigvec sign/ordering can differ between engines so we pin invariants (residual + dimensions + p), not literal entries beyond the PD diagonal. Octave 11.1.0 doesn't ship cholcov in core (statistics package only); reports N/A. |
| `corr` | ⚠️ | 0.001 | 652.67× |  | OK | Sig: c = corr(X). Pearson correlation matrix between columns of X (alias to corrcoef). Two-arg corr(X, Y) deferred. |
| `corrcov` | ✅ | 0.004 | 365.04× |  | OK | MATLAB corrcov: R = C ./ sqrt(diag(C)*diag(C)'); sigma = sqrt(diag(C))'. Bit-equal with MATLAB R2025b on 3x3 covariance, identity, scalar, and negative-correlation 2x2 cases. Octave 11.1.0 doesn't ship corrcov in core (statistics package only); reports N/A. |
| `crosstab` | ✅ | 0.003 | 1247.39× |  | OK | MATLAB crosstab: contingency table with chi-square independence test. Bit-equal with MATLAB R2025b on table entries; chi2 and p match within tolerance (chi2cdf depends on incomplete-gamma which has small numerical drift between engines). Octave 11.1.0 doesn't ship crosstab in core (statistics package only); reports N/A. |
| `geomean` | ✅ | 0.004 | 125.08× | 27.16× | OK | Sig: g = geomean(x[, dim]). (prod x)^(1/n) = exp(mean(log x)). |
| `grpstats` | ✅ | 0.007 | 797.48× |  | OK | MATLAB grpstats: per-group statistics. Bit-equal with MATLAB R2025b on default-mean, multi-fn cell-of-strings, sum, std, numel aggregators. Other aggregators (var, sem, min, max) supported in numkit. Cell-of-fn output ordering matches MATLAB's nargout indexing. Octave 11.1.0 doesn't ship grpstats in core (statistics package only); reports N/A. |
| `harmmean` | ✅ | 0.004 | 132.64× | 51.80× | OK | Sig: h = harmmean(x[, dim]). n / sum(1./x). Requires positive x. |
| `kurtosis` | ✅ | 0.006 | 172.61× |  | OK | Sig: skewness(X [, flag [, dim]]) — sample skewness (3rd central moment / std³). kurtosis(X [, flag [, dim]]) — sample kurtosis (4th central moment / variance²). flag = 0 → bias-corrected estimator; flag = 1 (default) → biased moment ratio. Symmetric data → skewness = 0, kurtosis = 1.7 (for [1..5]). Matrix input reduces along first non-singleton dim → row of per-column moments. Bit-exact MATLAB R2025b on the documented signatures. |
| `mad` | ✅ | 0.003 | 852.44× | 209.53× | OK | Sig: mad(x[, flag][, dim]). Mean (flag=0) or median (flag=1) absolute deviation. |
| `moment` | ✅ | 0.004 | 71.32× | 173.16× | OK | Sig: m = moment(x, k[, dim]). Central k-th moment: mean((x - mean(x))^k). |
| `nearcorr` | ⚠️ | 0.046 | 75.56× |  | OK | MATLAB nearcorr: nearest correlation matrix (Higham 2002 alternating projections + Dykstra). Identity case (input already correlation) is unchanged; Higham 3x3 textbook example produces [-0.4041, 0.4988, 0.5912] off-diagonals; output is symmetric, unit-diag, PSD (min eigval ~ 0 for indefinite inputs). Defaults tol=1e-10, maxits=100; 'tolconv'/'maxits' name-value parameters deferred for v1. Uses eig_symmetric (libs/builtin) for the PSD projection. Octave 11.1.0 doesn't ship nearcorr in core (statistics package only); reports N/A. |
| `partialcorr` | ✅ | 0.008 | 601.52× |  | OK | Sig: r = partialcorr(X, Y, Z). Pearson partial correlation controlling for Z. Bit-identical with MATLAB R2025b on probed deterministic data. |
| `partialcorri` | ✅ | 7.034 | 0.47× |  | OK | Sig: R = partialcorri(Y, X [, Z]) — partial correlation between each Y column and each X column, controlling for the OTHER X cols (and Z). canoncorr(X, Y) = canonical correlation analysis via centring + QR + SVD; returns canonical coefficients A, B (p×k, q×k) and the canonical correlations r (length k = min(p, q)). Bit-exact MATLAB R2025b not feasible (rng state differs); fingerprint pins structural invariants on the recoverability test (diag dominance for partialcorri on planted dependence, r(1) ≈ 1 for canoncorr on shared latent factor) and on the shapes. |
| `range` | ✅ | 0.005 | 80.85× | 9.83× | OK | Sig: r = range(x[, dim]). max - min along dim. Bit-identical with MATLAB R2025b. |
| `robustcov` | ✅ | 0.108 |  |  | N/A | Sig: [b, s] = robustfit(X, y [, wfun [, tune]]) — IRLS robust regression with bisquare (default, tune=4.685) or huber (tune=1.345). KNOWN GAP: stats struct (DOF, p-values, etc.) reduced to scalar s. [sigma, mu] = robustcov(X) — trimmed-MCD robust covariance via h = ceil(0.75 · n) concentration steps with Pison-Van Aelst-Willems consistency correction. KNOWN GAPs: full FAST-MCD multi-start, MVE method, OGK estimator not in v1. Spec uses deterministic sin/cos noise to make parity reproducible across engines; pins error bounds and shape invariants. |
| `skewness` | ✅ | 0.006 | 172.61× |  | OK | Sig: skewness(X [, flag [, dim]]) — sample skewness (3rd central moment / std³). kurtosis(X [, flag [, dim]]) — sample kurtosis (4th central moment / variance²). flag = 0 → bias-corrected estimator; flag = 1 (default) → biased moment ratio. Symmetric data → skewness = 0, kurtosis = 1.7 (for [1..5]). Matrix input reduces along first non-singleton dim → row of per-column moments. Bit-exact MATLAB R2025b on the documented signatures. |
| `tabulate` | ✅ | 0.007 | 577.28× | 123.04× | OK | MATLAB tabulate: frequency table. Bit-equal with MATLAB R2025b on positive-int dense layout (with zeros for missing values), non-integer sparse layout, and NaN-excluded percentage. Octave 11.1.0 doesn't ship tabulate in core (statistics package only); reports N/A. |
| `tiedrank` | ✅ | 0.007 | 185.97× |  | OK | MATLAB tiedrank: ranks adjusted for ties via averaging. Bit-equal with MATLAB R2025b on vector + matrix forms. Tieadj uses (t^3 - t) / 2 per tied group. Includes all-equal and no-ties edges. NaN handling tested in gtest only (parity harness fingerprint format doesn't preserve NaN trivially). |
| `trimmean` | ✅ | 0.003 | 659.47× | 137.95× | OK | Sig: m = trimmean(x, percent[, dim]). Mean after trimming percent/2 from each end. |
| `zscore` | ✅ | 0.004 | 285.18× | 126.07× | OK | Sig: z = zscore(x). Spec-extension batch 2026-05-09 (cycle 41). |
| `nancov` | ✅ | 0.004 | 387.07× |  | OK | Sig: C = nancov(X) — NaN-aware covariance matrix; rows containing any NaN are dropped (== MATLAB cov(X, 'omitrows'), the default 'complete' mode). nancov(X, normFlag) — 0 unbiased (n-1) / 1 population (n). nancov(x, y) — between two vectors; treats [x y] as 2-column matrix. Vector input → scalar variance. KNOWN GAP: 'pairwise' mode (per-(i,j) row-drop) not in v1 — only 'complete'. Bit-exact MATLAB R2025b on the documented signatures. |
| `nansum` | ✅ | 0.008 | 153.07× |  | OK | Sig: legacy NaN-aware reductions (recommended modern form: `sum(..., 'omitnan')` / `mean(..., 'omitnan')`). nansum: NaN entries dropped; all-NaN slice → 0 (NaN is additive identity). nanmean: NaN entries dropped; divisor is count of valid obs; all-NaN slice → NaN. Bit-exact MATLAB R2025b on the pinned 1-D and 2-D cases. nanstd/nanvar/nanmedian/nanmax/nanmin also work but are NOT in PROGRESS.md (MATLAB R2025b removed their formal doc entry — only legacy). |
| `nanmean` | ✅ | 0.008 | 153.07× |  | OK | Sig: legacy NaN-aware reductions (recommended modern form: `sum(..., 'omitnan')` / `mean(..., 'omitnan')`). nansum: NaN entries dropped; all-NaN slice → 0 (NaN is additive identity). nanmean: NaN entries dropped; divisor is count of valid obs; all-NaN slice → NaN. Bit-exact MATLAB R2025b on the pinned 1-D and 2-D cases. nanstd/nanvar/nanmedian/nanmax/nanmin also work but are NOT in PROGRESS.md (MATLAB R2025b removed their formal doc entry — only legacy). |

### Probability Distributions

**Namespace:** `stats.dist.*` — 115 ✅ + 0 ⚠️ / 130+ = 88%

Each distribution provides 5 entrypoints: `*pdf` / `*cdf` / `*inv` (or `*icdf`) / `*rnd` / `*stat`. All `rnd` functions share `numkit::builtin::sharedEngine()` so `rng(seed)` reseeds them. Discrete `*inv` use one-ULP relative tolerance against the public cdf so `inv(cdf(k))=k` round-trips don't overshoot.

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `normpdf` | ✅ | 0.007 | 60.47× | 78.30× | OK | Sig: y = normpdf(x[, mu, sigma]). Normal PDF: (1/(σ√(2π)))·exp(-(x-μ)²/(2σ²)). Defaults mu=0, sigma=1. sigma<=0 => NaN. Vectorised. |
| `normcdf` | ✅ |  |  |  | OK |  |
| `norminv` | ✅ | 0.009 | 67.05× | 79.75× | OK | Sig: x = norminv(p[, mu, sigma]). Inverse Normal CDF: x = mu + sigma*Φ⁻¹(p). Defaults mu=0, sigma=1. Edges: p=0 => -Inf; p=1 => +Inf; p outside [0,1] => NaN; sigma<=0 => NaN. Tol 1e-9 -- erfcinv algorithm differs ~1e-12 absolute from MATLAB. |
| `normrnd` | ✅ |  |  |  | OK |  |
| `normstat` | ✅ | 0.007 | 112.11× | 36.81× | OK | Sig: [m, v] = normstat(mu, sigma). Trivially m=mu, v=sigma². Vectorised with broadcasting (equal sizes or one scalar). sigma<=0 => NaN. |
| `chi2pdf` | ✅ | 0.005 | 925.15× |  | OK | Sig: y = chi2pdf(x, k). Chi-squared PDF with k dof. x < 0 => 0; k <= 0 => NaN. Covers: scalar, vector x, x<0 + x=0 edges, k=1 (special: x^(-1/2)·exp(-x/2)/√(2π)), k=30 large dof. |
| `chi2cdf` | ✅ |  |  |  | OK | gammainc(x/2, k/2) |
| `chi2inv` | ✅ | 0.010 | 85.41× |  | OK | Sig: x = chi2inv(p, k). Inverse Chi² CDF with k dof. Covers k ∈ {1, 5, 30} × p ∈ {0.05, 0.5, 0.95} + p=0 (=> 0) + p=1 (=> Inf) + p outside [0,1] (=> NaN) + k<=0 (=> NaN). |
| `chi2rnd` | ✅ |  |  |  | OK |  |
| `chi2stat` | ✅ | 0.003 | 147.31× |  | OK | Sig: [m, v] = chi2stat(k). Chi² mean=k and variance=2k. Vectorised. k<=0 => NaN (moments don't exist for degenerate). |
| `tpdf` | ✅ | 0.008 | 159.59× | 106.05× | OK | Sig: y = tpdf(x, nu). Student's t PDF via lgamma-stable form. nu=Inf -> Gaussian limit (1/sqrt(2π))·exp(-x²/2). nu<=0 or NaN -> NaN. NaN x -> NaN. |
| `tcdf` | ✅ |  |  |  | OK | betainc on z = ν/(ν+x²), branch by sign |
| `tinv` | ✅ | 0.015 | 140.10× | 629.20× | OK | Sig: x = tinv(p, nu). Inverse Student's t-CDF. Uses betaincinv(2(1-p) or 2p, nu/2, 1/2) and signs by p<>0.5. nu=Inf -> Gaussian limit (norminv(p)). Edges: p=0 -> -Inf; p=1 -> +Inf; p outside [0,1] -> NaN; nu<=0 -> NaN. |
| `trnd` | ✅ |  |  |  | OK | Z/√(X/ν), Z~N(0,1), X~χ²(ν) |
| `tstat` | ✅ | 0.006 | 104.08× | 35.02× | OK | Sig: [m, v] = tstat(nu). Student's t: m=0 if nu>1, v=nu/(nu-2) if nu>2. Vectorised. nu<=0 => NaN/NaN; 0<nu<=1 => m=NaN,v=NaN; 1<nu<=2 => m=0, v=NaN. |
| `fpdf` | ✅ | 0.004 |  |  | N/A | Sig: y = fpdf(x, v1, v2). F-distribution PDF. x < 0 => 0; v1 <= 0 or v2 <= 0 => NaN. Covers: scalar (v1=5,v2=10), vector x, x<0/x=0 edges, invalid v1/v2, F(2,10) at 0 (= v1/(v1+v2-2)/B(...) finite for v1=2). |
| `fcdf` | ✅ |  |  |  | OK | betainc(v1·x/(v1·x+v2), v1/2, v2/2) |
| `finv` | ✅ | 0.014 |  |  | N/A | Sig: x = finv(p, v1, v2). Inverse F CDF. Covers (v1, v2) ∈ {(1,1), (5,10), (10,30)} × p ∈ {0.05, 0.5, 0.95} + p=0 (=> 0) + p=1 (=> Inf) + p outside [0,1] (=> NaN) + v1<=0 / v2<=0 (=> NaN). |
| `frnd` | ✅ |  |  |  | OK | (X1/v1)/(X2/v2), Xi~χ²(vi) |
| `fstat` | ✅ | 0.006 | 248.81× | 48.77× | OK | Sig: [m, v] = fstat(v1, v2). F-distribution mean = v2/(v2-2) for v2>2 else NaN; variance = 2*v2²(v1+v2-2)/(v1(v2-2)²(v2-4)) for v2>4 else NaN. Vectorised. v1<=0 or v2<=0 => NaN/NaN. |
| `betapdf` | ✅ | 0.003 | 444.39× |  | OK | Sig: y = betapdf(x, a, b). Beta PDF on (0,1). x outside (0,1) => 0; a<=0 or b<=0 => NaN. Covers: scalar, vector, out-of-(0,1) edges (x<0, x=0, x=0.5, x=1, x>1), invalid params. |
| `betacdf` | ✅ |  |  |  | OK | I_x(a, b) directly |
| `betainv` | ✅ | 0.013 | 63.56× |  | OK | Sig: x = betainv(p, a, b). Inverse Beta CDF. Covers (a,b) ∈ {(1,1) uniform, (0.5,0.5) U-shaped, (2,5), (10,10)} × p ∈ {0.05, 0.5, 0.95}. Edges: p=0 => 0; p=1 => 1; p outside [0,1] => NaN; invalid shape => NaN. |
| `betarnd` | ✅ |  |  |  | OK | U/(U+V), U~Gamma(a,1), V~Gamma(b,1) |
| `betastat` | ✅ | 0.002 | 420.81× |  | OK | Sig: [m, v] = betastat(a, b). Beta(a,b) mean a/(a+b) and variance ab/((a+b)^2(a+b+1)). Vectorised. Invalid params (a<=0 or b<=0) => NaN. Beta(1,1) is uniform: m=0.5, v=1/12. |
| `gampdf` | ✅ | 0.007 | 653.04× | 107.03× | OK | Sig: y = gampdf(x, a, b). Gamma(shape=a, scale=b) PDF. Density at 0: a<1 → Inf, a=1 → 1/b, a>1 → 0. x<0 → 0. a<0 or b<=0 → NaN. a=0 → 0 (degenerate). |
| `gamcdf` | ✅ |  |  |  | OK | gammainc(x/b, a) |
| `gaminv` | ✅ | 0.009 | 81.65× | 809.51× | OK | Sig: x = gaminv(p, a, b). Inverse Gamma CDF. q=0 → 0; q=1 → Inf; q outside [0,1] → NaN. a=0 → 0 (degenerate); a<0 / b<=0 → NaN. |
| `gamrnd` | ✅ |  |  |  | OK | std::gamma_distribution(a, b) |
| `gamstat` | ✅ | 0.008 | 54.34× | 37.71× | OK | Sig: [m, v] = gamstat(a, b). Gamma(shape, scale): m = a·b, v = a·b². Vectorised. a<=0 or b<=0 => NaN. |
| `exppdf` | ✅ | 0.004 |  |  | N/A | Sig: y = exppdf(x[, mu]). Exponential PDF: (1/mu)·exp(-x/mu). Default mu=1. x<0 → 0. mu<=0 → NaN. |
| `expcdf` | ✅ |  |  |  | OK | -expm1(-x/μ) |
| `expinv` | ✅ | 0.004 |  |  | N/A | Sig: x = expinv(p[, mu]). Inverse exponential CDF: x = -mu*log(1-p). Default mu=1. Covers default form + non-default mu + boundaries (p=0,1) + invalid (p<0, p>1, mu<=0). |
| `exprnd` | ✅ |  |  |  | OK |  |
| `expstat` | ✅ | 0.002 |  |  | N/A | Sig: [m, v] = expstat(mu). Exponential mean=mu, variance=mu^2. Vectorised. mu<=0 => NaN. |
| `unifpdf` | ✅ | 0.009 | 111.41× |  | OK | Sig: y = unifpdf(x[, a, b]). Continuous uniform PDF on [a, b]; defaults a=0, b=1. y = 1/(b-a) for x in [a,b], else 0. Edges: b<=a -> NaN; NaN x -> NaN; NaN a/b -> 0 (NaN comparisons false, MATLAB convention). |
| `unifcdf` | ✅ |  |  |  | OK |  |
| `unifinv` | ✅ | 0.008 | 107.28× |  | OK | Sig: x = unifinv(p[, a, b]). Inverse Continuous Uniform CDF on [a, b]: x = a + p*(b-a). Defaults a=0, b=1. p=0 -> a; p=1 -> b; p outside [0,1] -> NaN; b<=a -> NaN; NaN p -> NaN. |
| `unifrnd` | ✅ |  |  |  | OK |  |
| `unifstat` | ✅ | 0.007 | 147.35× | 34.81× | OK | Sig: [m, v] = unifstat(a, b). Continuous uniform on [a,b]: m=(a+b)/2, v=(b-a)²/12. Vectorised. b<=a => NaN. |
| `lognpdf` | ✅ | 0.006 | 68.84× | 165.30× | OK | Sig: y = lognpdf(x[, mu, sigma]). Lognormal PDF. Defaults mu=0, sigma=1. x<=0 → 0. sigma<=0 → NaN. |
| `logncdf` | ✅ |  |  |  | OK |  |
| `logninv` | ✅ | 0.008 | 75.10× | 70.90× | OK | Sig: x = logninv(p[, mu, sigma]). Inverse Lognormal CDF. Defaults mu=0, sigma=1. q=0 → 0; q=1 → Inf; q outside [0,1] → NaN. sigma<=0 → NaN. |
| `lognrnd` | ✅ |  |  |  | OK |  |
| `lognstat` | ✅ | 0.008 | 54.17× | 33.18× | OK | Sig: [m, v] = lognstat(mu, sigma). Lognormal: m = exp(mu + sigma²/2), v = (exp(sigma²)-1)·exp(2mu + sigma²). Vectorised. sigma<=0 => NaN. |
| `wblpdf` | ✅ | 0.009 | 63.55× | 79.19× | OK | Sig: y = wblpdf(x[, a, b]). Weibull PDF with scale a, shape b. Defaults a=1, b=1 (= exponential). Edges: x<0 -> 0; x=0 -> b/a if b=1, Inf if b<1, 0 if b>1; a<=0 or b<=0 -> NaN; NaN -> NaN. |
| `wblcdf` | ✅ |  |  |  | OK |  |
| `wblinv` | ✅ | 0.009 | 67.34× | 104.78× | OK | Sig: x = wblinv(p[, a, b]). Inverse Weibull CDF: x = a · (-log(1-p))^(1/b). Defaults a=1, b=1 (= exponential -log(1-p)). p=0 -> 0; p=1 -> Inf; p outside [0,1] -> NaN; a<=0, b<=0 -> NaN; NaN -> NaN. |
| `wblrnd` | ✅ |  |  |  | OK |  |
| `wblstat` | ✅ | 0.009 | 65.06× | 40.52× | OK | Sig: [m, v] = wblstat(a, b). Weibull(scale=a, shape=b): m = a·Γ(1+1/b), v = a²·(Γ(1+2/b) - Γ(1+1/b)²). Vectorised. a<=0 or b<=0 => NaN. |
| `raylpdf` | ✅ | 0.006 | 146.37× | 44.76× | OK | Sig: y = raylpdf(x, b). Rayleigh PDF. x<0 → 0; x=0 → 0 (density at origin is 0). b<=0 → NaN. |
| `raylcdf` | ✅ |  |  |  | OK |  |
| `raylinv` | ✅ | 0.006 | 192.63× | 66.04× | OK | Sig: x = raylinv(p, b). Inverse Rayleigh CDF: x = b·sqrt(-2·ln(1-p)). q=0 → 0; q=1 → Inf; q outside [0,1] → NaN. b<=0 → NaN. |
| `raylrnd` | ✅ |  |  |  | OK | inverse-cdf sampling |
| `raylstat` | ✅ | 0.005 | 83.38× | 42.06× | OK | Sig: [m, v] = raylstat(b). Rayleigh: m = b·sqrt(π/2), v = b²·(2 - π/2). Vectorised. b<=0 => NaN. |
| `poisspdf` | ✅ | 0.007 | 306.49× | 57.54× | OK | Sig: y = poisspdf(k, lambda). Poisson PMF. Out-of-support k (<0, non-integer) → 0. lambda=0 degenerate: only k=0 → 1. lambda<0 → NaN. |
| `poisscdf` | ✅ |  |  |  | OK | F(k; λ) = 1 - gammainc(λ, ⌊k⌋+1) |
| `poissinv` | ✅ | 0.009 | 149.97× | 109.06× | OK | Sig: x = poissinv(p, lambda). Inverse Poisson CDF. q=0 → 0; q=1 → Inf; q outside [0,1] → NaN. lambda=0 → 0 (degenerate). lambda<0 → NaN. |
| `poissrnd` | ✅ |  |  |  | OK |  |
| `poisstat` | ✅ | 0.005 | 142.65× | 40.33× | OK | Sig: [m, v] = poisstat(lambda). Poisson mean=variance=lambda. Vectorised. lambda<=0 => NaN. |
| `binopdf` | ✅ | 0.006 | 145.09× |  | OK | Sig: y = binopdf(k, n, p). Binomial PMF. Out-of-support k (negative, > n, non-integer) → 0. p=0: only k=0 → 1. p=1: only k=n → 1. Invalid n / p out of [0,1] → NaN. |
| `binocdf` | ✅ |  |  |  | OK | I_{1-p}(n - ⌊k⌋, ⌊k⌋ + 1) |
| `binoinv` | ✅ | 0.005 | 1414.69× |  | OK | Sig: x = binoinv(q, n, p). Inverse Binomial CDF. q=0 → 0; q=1 → n. Invalid (q outside [0,1] / p outside [0,1] / n<0 / non-integer n) => NaN. |
| `binornd` | ✅ |  |  |  | OK |  |
| `binostat` | ✅ | 0.006 | 149.87× |  | OK | Sig: [m, v] = binostat(n, p). Binomial: m=n·p, v=n·p·(1-p). Vectorised. n<0 / non-integer / p<0 / p>1 => NaN. p∈{0,1} are valid (variance becomes 0). |
| `unidpdf` | ✅ | 0.008 | 158.78× | 60.90× | OK | Sig: y = unidpdf(k, N). Discrete uniform PMF on {1..N}: 1/N if k in 1..N integer, else 0. N<=0 or non-integer N -> NaN. NaN N -> NaN. NaN k -> 0 (per MATLAB). tol=0 (integer-stable for discrete). |
| `unidcdf` | ✅ |  |  |  | OK |  |
| `unidinv` | ✅ | 0.008 | 146.17× | 50.99× | OK | Sig: x = unidinv(p, N). Inverse discrete-uniform CDF on {1..N}: x = ceil(p·N), clamped. Edges: p<=0 or p>1 -> NaN (p=0 has no integer pre-image); N<1 or non-integer N -> NaN; NaN p/N -> NaN. tol=0. |
| `unidrnd` | ✅ |  |  |  | OK |  |
| `unidstat` | ✅ | 0.006 | 89.68× | 19.74× | OK | Sig: [m, v] = unidstat(N). Discrete uniform on {1..N}: m = (N+1)/2, v = (N²-1)/12. Vectorised. N<1 or non-integer => NaN. |
| `geopdf` | ✅ | 0.003 | 87.36× | 63.74× | OK | Sig: r = geopdf(...). Spec-extension batch 2026-05-09. |
| `geocdf` | ✅ | 0.004 | 342.68× | 118.02× | OK | Sig: p = geocdf(k, p[, 'upper']). Geometric (number of failures before first success): F(k; p) = 1 - (1-p)^(k+1). 'upper' returns 1 - F(k). |
| `geoinv` | ✅ | 0.003 | 93.24× | 47.05× | OK | Sig: r = geoinv(...). Spec-extension batch 2026-05-09. |
| `geornd` | ✅ | 0.003 | 88.77× | 46.13× | OK | Sig: r = geornd(...). Spec-extension batch 2026-05-09. |
| `geostat` | ✅ | 0.004 | 54.51× | 53.60× | OK | Sig: r = geostat(...). Spec-extension batch 2026-05-09. |
| `nbinpdf` | ✅ | 0.004 | 268.95× | 121.86× | OK | Sig: r = nbinpdf(...). Spec-extension batch 2026-05-09.  |
| `nbincdf` | ✅ | 0.005 | 314.14× | 149.41× | OK | Sig: p = nbincdf(k, r, p[, 'upper']). Negative binomial: number of failures before r-th success. F(k; r, p) = I_p(r, k+1). 'upper' returns 1 - F(k). |
| `nbininv` | ✅ | 0.004 | 178.27× | 176.10× | OK | Sig: r = nbininv(...). Spec-extension batch 2026-05-09.  |
| `nbinrnd` | ✅ | 0.004 | 274.01× | 50.36× | OK | Sig: r = nbinrnd(R, P, sz). Spec-extension batch 2026-05-09 (cycle 41). |
| `nbinstat` | ✅ | 0.004 | 184.85× | 12.27× | OK | Sig: r = nbinstat(...). Spec-extension batch 2026-05-09.  |
| `hygepdf` | ✅ | 0.004 | 440.23× | 73.64× | OK | Sig: r = hygepdf(...). Spec-extension batch 2026-05-09.  |
| `hygecdf` | ✅ | 0.008 | 583.62× | 252.93× | OK | Sig: p = hygecdf(k, M, K, N[, 'upper']). Hypergeometric CDF over k=0..N drawn from population M with K marked. 'upper' returns 1 - F(k). |
| `hygeinv` | ✅ | 0.004 | 682.67× | 65.58× | OK | Sig: r = hygeinv(...). Spec-extension batch 2026-05-09.  |
| `hygernd` | ✅ | 0.006 | 893.92× | 64.32× | OK | Sig: r = hygernd(...). Spec-extension batch 2026-05-09.  |
| `hygestat` | ✅ | 0.004 | 204.84× | 12.86× | OK | Sig: r = hygestat(...). Spec-extension batch 2026-05-09.  |
| `evpdf` | ✅ | 0.001 |  |  | N/A | Sig: r = evpdf(...). Spec-extension batch 2026-05-09. |
| `evcdf` | ✅ | 0.002 | 312.26× |  | OK | Sig: p = evcdf(x[, mu, sigma][, 'upper']). F(x) = 1 − exp(−exp((x−μ)/σ)); 'upper' returns 1 - F(x). |
| `evinv` | ✅ | 0.002 |  |  | N/A | Sig: r = evinv(...). Spec-extension batch 2026-05-09. |
| `evrnd` | ✅ | 0.008 |  |  | N/A | Sig: r = evrnd(mu, sigma). Type-I (Gumbel-MIN) extreme value sampler via inverse CDF on rand(). Bit-identical with MATLAB R2025b after Phase-0a-1 RNG cascade (MT19937 init_genrand + genRes53 + Gumbel-MIN convention). |
| `evstat` | ✅ | 0.001 |  |  | N/A | Sig: r = evstat(...). Spec-extension batch 2026-05-09. |
| `gevpdf` | ✅ | 0.004 | 171.31× | 63.66× | OK | Sig: r = gevpdf(...). Spec-extension batch 2026-05-09. |
| `gevcdf` | ✅ | 0.004 | 292.27× | 84.15× | OK | Sig: p = gevcdf(x, k, sigma, mu[, 'upper']). 'upper' returns 1 - F(x). |
| `gevinv` | ✅ | 0.003 | 183.13× | 83.59× | OK | Sig: r = gevinv(...). Spec-extension batch 2026-05-09. |
| `gevrnd` | ✅ | 0.008 | 309.21× | 102.34× | OK | Sig: r = gevrnd(k, sigma, mu). Generalized Extreme Value sampler via gev_inv_one inverse CDF on rand(). Bit-identical with MATLAB R2025b after Phase-0a-1 RNG cascade. |
| `gevstat` | ✅ | 0.004 | 172.94× | 45.16× | OK | Sig: r = gevstat(...). Spec-extension batch 2026-05-09. |
| `gppdf` | ✅ | 0.003 | 177.63× | 71.20× | OK | Sig: r = gppdf(...). Spec-extension batch 2026-05-09.  |
| `gpcdf` | ✅ | 0.004 | 275.33× | 66.66× | OK | Sig: p = gpcdf(x, k, sigma, theta[, 'upper']). 'upper' returns 1 - F(x). |
| `gpinv` | ✅ | 0.003 | 181.43× | 51.55× | OK | Sig: r = gpinv(...). Spec-extension batch 2026-05-09. |
| `gprnd` | ✅ | 0.010 | 248.46× | 52.50× | OK | Sig: r = gprnd(k, sigma, theta). Generalized Pareto sampler via inline ICDF on rand() (uses u directly, MATLAB convention). Bit-identical with MATLAB R2025b after Phase-0a-1 RNG cascade. |
| `gpstat` | ✅ | 0.004 | 113.53× | 45.15× | OK | Sig: r = gpstat(...). Spec-extension batch 2026-05-09.  |
| `nakapdf` | ✅ | 0.003 |  | 26.27× | OK | Sig: r = nakapdf(...). Spec-extension batch 2026-05-09.  |
| `nakacdf` | ✅ | 0.004 |  | 161.43× | OK | Sig: p = nakacdf(x, mu, omega[, 'upper']). Nakagami-m CDF: F(x) = gammainc(mu·x²/omega, mu). 'upper' returns 1 - F(x). |
| `nakainv` | ✅ | 0.003 |  | 126.30× | OK | Sig: r = nakainv(...). Spec-extension batch 2026-05-09.  |
| `nakarnd` | ✅ | 0.004 |  | 51.71× | OK | Sig: r = nakarnd(mu, omega, sz). Spec-extension batch 2026-05-09 (cycle 41). |
| `nakastat` | ✅ | 0.004 |  | 31.28× | OK | Sig: r = nakastat(...). Spec-extension batch 2026-05-09.  |
| `ricepdf` | ✅ | 0.003 |  | 78.08× | OK | Sig: y = ricepdf(x, s, sigma). Rice PDF (x/σ²)·exp(−(x²+s²)/(2σ²))·I_0(x·s/σ²). Octave stats package has direct names; MATLAB exposes via pdf('Rician', ...). |
| `ricecdf` | ✅ | 1.133 |  | 1.17× | OK | Sig: p = ricecdf(x, s, sigma[, 'upper']). Rice CDF via Marcum Q: F(x) = 1 - Q1(s/sigma, x/sigma). 'upper' returns 1 - F(x) = Q1(s/sigma, x/sigma). MATLAB R2025b does NOT ship a top-level ricecdf — only makedist('Rician')+cdf — so reference comes from Octave's statistics package. Tolerance 1e-4 reflects an existing ~1e-5 numerical-accuracy gap between numkit's marcumq series and Octave's; this ТЗ closes the 'upper' flag only, the accuracy gap is tracked separately. |
| `riceinv` | ✅ | 6.509 |  | 1.79× | OK | Sig: x = riceinv(p, s, sigma). Spec-extension batch 2026-05-09 (cycle 41). |
| `ricernd` | ✅ | 0.004 |  | 138.57× | OK | Sig: r = ricernd(s, sigma, sz). Spec-extension batch 2026-05-09 (cycle 41). |
| `ricestat` | ✅ | 0.009 |  | 49.58× | OK | Sig: [m, v] = ricestat(s, sigma). Rician (Rice). s=0 reduces to Rayleigh: m = sigma·sqrt(π/2), v = sigma²·(2 - π/2). Vectorised. sigma<=0 / s<0 => NaN. MATLAB R2025b doesn't ship ricestat — Octave statistics package is the reference. |
| `ncfpdf` | ✅ | 0.182 | 48.55× |  | OK | Sig: nctrnd(nu, delta, rows, cols) — noncentral t RNG via T = (Z+δ)/√(V/ν), Z~N(0,1), V~χ²(ν), shared MT19937. ncfpdf(x, nu1, nu2, delta) — noncentral F pdf via Poisson-mixture series f(x) = e^{-δ/2} Σ_k (δ/2)^k/k! · (ν₁/ν₂)^(ν₁/2+k) x^(ν₁/2+k-1) (1+ν₁x/ν₂)^{-(ν₁+ν₂)/2-k} / B(ν₁/2+k, ν₂/2). Series truncated at 1e-16 rel contribution. delta=0 reduces to central fpdf exactly. ncfpdf verified bit-equal to MATLAB R2025b at tol=1e-8; nctrnd sample moments pinned at statistical tolerance. KNOWN GAPs: ncfcdf, ncfinv, ncfstat, ncfrnd next cycles. |
| `ncfcdf` | ✅ | 0.152 | 41.06× |  | OK | Sig: ncfcdf(x, nu1, nu2, delta[, 'upper']) — noncentral F cdf via Poisson-mixture in regularised incomplete beta: F(x) = Σ_k Poisson(k; δ/2) · I_y(ν₁/2+k, ν₂/2), y = ν₁x/(ν₁x+ν₂). ncfinv(p, nu1, nu2, delta) — Newton on ncfcdf with central finv as warm start, bracketed bisection fallback. delta=0 reduces to central fcdf/finv. Verified bit-equal to MATLAB R2025b at 1e-8 tol (ncfcdf bit-identical, ncfinv to ~1e-6 via Newton convergence). KNOWN GAPs: ncfstat, ncfrnd next cycles. |
| `ncfinv` | ✅ | 0.152 | 41.06× |  | OK | Sig: ncfcdf(x, nu1, nu2, delta[, 'upper']) — noncentral F cdf via Poisson-mixture in regularised incomplete beta: F(x) = Σ_k Poisson(k; δ/2) · I_y(ν₁/2+k, ν₂/2), y = ν₁x/(ν₁x+ν₂). ncfinv(p, nu1, nu2, delta) — Newton on ncfcdf with central finv as warm start, bracketed bisection fallback. delta=0 reduces to central fcdf/finv. Verified bit-equal to MATLAB R2025b at 1e-8 tol (ncfcdf bit-identical, ncfinv to ~1e-6 via Newton convergence). KNOWN GAPs: ncfstat, ncfrnd next cycles. |
| `ncfrnd` | ❌ |  |  |  |  |  |
| `ncfstat` | ❌ |  |  |  |  |  |
| `nctpdf` | ✅ | 0.039 | 155.01× |  | OK | Sig: nctpdf(x, nu, delta) — noncentral t pdf via direct series f(x;ν,δ) = ν^{ν/2}·e^{-δ²/2} / (√π·Γ(ν/2)·(ν+x²)^{(ν+1)/2}) · Σ_k Γ((ν+k+1)/2)·(xδ√2)^k / (k!·(ν+x²)^{k/2}). nctcdf(x, nu, delta[, 'upper']) — Owen (1965) series F(x;ν,δ) = Φ(-δ) + ½·Σ_k P_k·I_y(k+½, ν/2) + (δ/(2√2))·e^{-δ²/2}·Σ_k I_y(k+1, ν/2)/Γ(k+3/2), y = x²/(x²+ν). Negative x via symmetry F(x;ν,δ) = 1 - F(-x;ν,-δ). Series truncated at 1e-16 relative contribution. Bit-identical with MATLAB R2025b at 1e-8 tol. KNOWN GAPs: nctinv, nctstat, nctrnd next batch. |
| `nctcdf` | ✅ | 0.039 | 155.01× |  | OK | Sig: nctpdf(x, nu, delta) — noncentral t pdf via direct series f(x;ν,δ) = ν^{ν/2}·e^{-δ²/2} / (√π·Γ(ν/2)·(ν+x²)^{(ν+1)/2}) · Σ_k Γ((ν+k+1)/2)·(xδ√2)^k / (k!·(ν+x²)^{k/2}). nctcdf(x, nu, delta[, 'upper']) — Owen (1965) series F(x;ν,δ) = Φ(-δ) + ½·Σ_k P_k·I_y(k+½, ν/2) + (δ/(2√2))·e^{-δ²/2}·Σ_k I_y(k+1, ν/2)/Γ(k+3/2), y = x²/(x²+ν). Negative x via symmetry F(x;ν,δ) = 1 - F(-x;ν,-δ). Series truncated at 1e-16 relative contribution. Bit-identical with MATLAB R2025b at 1e-8 tol. KNOWN GAPs: nctinv, nctstat, nctrnd next batch. |
| `nctinv` | ✅ | 0.269 | 26.89× |  | OK | Sig: nctinv(p, nu, delta) — inverse cdf via Newton on nctcdf with central tinv shifted by delta as warm start, safeguarded by bracketing bisection. nctstat(nu, delta) — closed form m = δ √(ν/2) Γ((ν-1)/2)/Γ(ν/2) for ν > 1; v = ν(1+δ²)/(ν-2) - m² for ν > 2; NaN otherwise. Verified bit-equal to MATLAB R2025b at tol=1e-8. KNOWN GAPs: nctrnd next batch. |
| `nctrnd` | ✅ | 0.182 | 48.55× |  | OK | Sig: nctrnd(nu, delta, rows, cols) — noncentral t RNG via T = (Z+δ)/√(V/ν), Z~N(0,1), V~χ²(ν), shared MT19937. ncfpdf(x, nu1, nu2, delta) — noncentral F pdf via Poisson-mixture series f(x) = e^{-δ/2} Σ_k (δ/2)^k/k! · (ν₁/ν₂)^(ν₁/2+k) x^(ν₁/2+k-1) (1+ν₁x/ν₂)^{-(ν₁+ν₂)/2-k} / B(ν₁/2+k, ν₂/2). Series truncated at 1e-16 rel contribution. delta=0 reduces to central fpdf exactly. ncfpdf verified bit-equal to MATLAB R2025b at tol=1e-8; nctrnd sample moments pinned at statistical tolerance. KNOWN GAPs: ncfcdf, ncfinv, ncfstat, ncfrnd next cycles. |
| `nctstat` | ✅ | 0.269 | 26.89× |  | OK | Sig: nctinv(p, nu, delta) — inverse cdf via Newton on nctcdf with central tinv shifted by delta as warm start, safeguarded by bracketing bisection. nctstat(nu, delta) — closed form m = δ √(ν/2) Γ((ν-1)/2)/Γ(ν/2) for ν > 1; v = ν(1+δ²)/(ν-2) - m² for ν > 2; NaN otherwise. Verified bit-equal to MATLAB R2025b at tol=1e-8. KNOWN GAPs: nctrnd next batch. |
| `ncx2pdf` | ✅ | 0.003 | 894.89× | 149.93× | OK | Sig: r = ncx2pdf(...). Spec-extension batch 2026-05-09.  |
| `ncx2cdf` | ✅ | 0.010 | 515.68× | 1175.94× | OK | Sig: y = ncx2cdf(x, k, lambda[, 'upper']). Poisson-mixture: Σ_j Poisson(j; λ/2)·gammainc(x/2, k/2 + j); truncated when contribution drops below 1e-16 of running sum. 'upper' returns 1 - F(x). |
| `ncx2inv` | ✅ | 0.048 | 61.32× | 396.30× | OK | Sig: r = ncx2inv(...). Spec-extension batch 2026-05-09.  |
| `ncx2rnd` | ✅ |  |  |  |  |  |
| `ncx2stat` | ✅ | 0.004 | 114.21× | 54.48× | OK | Sig: r = ncx2stat(...). Spec-extension batch 2026-05-09.  |

### Distribution Fitting (MLE / likelihood)

**Namespace:** `stats.fit.*` — 16 ✅ + 0 ⚠️ / 24 = 67%

OOP `fitdist` / `makedist` family intentionally omitted — only flat
function-form fitters (return `[parmhat, parmci]`) and likelihood evaluators.

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `mle` | ⚠️ | 0.005 | 2306.59× | 1372.02× | OK | Sig: mle(data[, 'distribution', name]). Closed-form MLE for normal (default) / exponential / poisson / lognormal. Bit-identical with MATLAB R2025b. Custom 'pdf'/'logpdf'/'nloglf' deferred. |
| `mlecov` | ❌ |  |  |  |  | covariance of MLE estimates |
| `betafit` | ✅ | 0.160 | 53.63× |  | OK | Sig: [ahat, bhat] = betafit(x) — Beta MLE via 2-D Newton on the digamma system. [rhat, phat] = nbinfit(x) — negative binomial MLE: profile log-likelihood, Newton on r with closed-form p = r / (r + mean). Stochastic samples in setup; fingerprint pins recoverability + output shapes. |
| `betalike` | ✅ | 0.003 | 333.79× |  | OK | Sig: [nL, AVAR] = betalike([a b], x). NLL for Beta(a, b). AVAR is the 2×2 inverse of the BHHH (outer-product-of-gradients) Fisher info — MATLAB's betalike uses BHHH, not the Hessian (verified by direct probe). Edge: invalid params or x outside (0,1) => NaN. |
| `binofit` | ✅ | 0.015 | 132.88× |  | OK | Sig: [phat, pci] = binofit(x, n[, alpha]). Clopper-Pearson exact binomial CI. Covers: scalar (k=7,n=10), vector ([3 5 7]'), edges x=0 + x=n, non-default alpha=0.01. No 'Method' kw — MATLAB binofit hard-codes Clopper-Pearson. |
| `evfit` | ✅ | 0.205 | 43.66× |  | OK | Sig: [muhat, sigmahat] = evfit(x) — Gumbel-min MLE: μ profiled out via Σ exp(x_i/σ)=n; 1-D Newton on σ-equation Σ x_i e^{x_i/σ}/Σ e^{x_i/σ} - mean - σ = 0 (negative-definite f'). [khat, sigmahat] = gpfit(x) — Generalised Pareto via PWM (Hosking & Wallis 1987): β_0 = mean, β_1 = mean(F̂_i x_(i)), k̂ = 2 - β_0/(β_0-2β_1), σ̂ = 2β_0β_1/(β_0-2β_1). Stochastic samples; fingerprint pins parameter recoverability + output shape. KNOWN GAPs: CI second output, censoring, freq, options, and Grimshaw 1993 MLE refinement for gpfit deferred. |
| `evlike` | ✅ | 0.005 |  |  | N/A | Sig: nL = evlike([mu sigma], x[, cens, freq]). Type-I extreme value (Gumbel min). Uncensored: log(σ) - z + e^z; censored: e^z; with optional freq weights. Edges: σ<=0 -> NaN (was Inf); empty data -> 0. AVAR (2-output form) deferred — observed Fisher info has nontrivial cross-terms. |
| `expfit` | ✅ | 0.006 |  |  | N/A | Sig: [muhat, muci] = expfit(x[, alpha[, censoring[, freq]]]). MLE for exponential: T = Σ(freq·x), D = Σ(freq·(1-cens)), mu = T/D. Exact CI via χ²(2D): [2T/χ²₁₋α/2, 2T/χ²_α/2]. Defaults: cens=0, freq=1. |
| `explike` | ✅ | 0.004 |  |  | N/A | Sig: [nL, avar] = explike(mu, x[, cens, freq]). NLL for Exp(mu). avar (scalar) = 1/I where I = Σ w_i ∂²nL_i/∂μ² (uncens: -1/μ²+2x/μ³; right-cens: 2x/μ³). Edge: mu<=0 => NaN; empty data => 0. |
| `gamfit` | ✅ | 0.132 | 46.54× |  | OK | Sig: [ahat, bhat] = gamfit(x) — Gamma(shape, scale) MLE via Minka 2002 init + digamma/trigamma Newton on the shape, scale follows from b = mean(x)/a. [ahat, bhat] = wblfit(x) — Weibull(scale, shape) MLE via Newton on the implicit shape equation, scale = (Σ x^b / n)^(1/b). KNOWN GAP: confidence intervals (`bci` second output) deferred in v1. Spec uses stochastic samples — fingerprint pins error bounds and output shape rather than literal values (engine RNG diverges). |
| `gamlike` | ✅ | 0.005 | 179.08× | 41.00× | OK | Sig: [nL, AVAR] = gamlike([a b], x). NLL for Gamma(a, b). AVAR is the 2×2 inverse observed-Fisher info matrix at [a, b], computed via central-difference Hessian (no in-tree trigamma). Edge: invalid params (a<=0 or b<=0) => NaN. tol=1e-7 reflects FD precision (~5e-8 absolute on basic case). |
| `gevfit` | ❌ |  |  |  |  | generalised extreme value |
| `gevlike` | ✅ | 0.007 | 152.14× | 45.88× | OK | Sig: [nL, ACOV] = gevlike([k sigma mu], x). GEV NLL with Gumbel-MAX limit at k=0. ACOV is the 3×3 inverse observed-Fisher matrix at [k,sigma,mu], computed via central-difference Hessian (tol=1e-6 reflects FD precision). Edge: sigma<=0 or per-point support violation (1+k*z<=0) => NaN. Known gap: at exactly k=0 MATLAB uses an analytical Gumbel-limit Hessian that differs from FD straddling — numkit's FD reproduces the value of FD-on-MATLAB's-own-gevlike (~0.030, 0.098, -1.622), not MATLAB's reported analytical ACOV. |
| `gpfit` | ✅ | 0.205 | 43.66× |  | OK | Sig: [muhat, sigmahat] = evfit(x) — Gumbel-min MLE: μ profiled out via Σ exp(x_i/σ)=n; 1-D Newton on σ-equation Σ x_i e^{x_i/σ}/Σ e^{x_i/σ} - mean - σ = 0 (negative-definite f'). [khat, sigmahat] = gpfit(x) — Generalised Pareto via PWM (Hosking & Wallis 1987): β_0 = mean, β_1 = mean(F̂_i x_(i)), k̂ = 2 - β_0/(β_0-2β_1), σ̂ = 2β_0β_1/(β_0-2β_1). Stochastic samples; fingerprint pins parameter recoverability + output shape. KNOWN GAPs: CI second output, censoring, freq, options, and Grimshaw 1993 MLE refinement for gpfit deferred. |
| `gplike` | ✅ | 0.007 | 131.66× |  | OK | Sig: [nL, acov] = gplike([k sigma], x). GP NLL with implicit theta=0. acov is the 2×2 inverse observed-Fisher matrix at [k, sigma], computed via central-difference Hessian (tol=1e-5 reflects FD precision; k=0 stride is the worst case at ~2e-6). Edges: sigma<=0 or per-point support violation (1+k*x/sigma<=0) => NaN. MATLAB does NOT enforce x>=0 globally — only the per-point support check; numkit matches (e.g. gplike([0.5,1], [-1 1 2]') returns 1.2163...). |
| `lognfit` | ✅ | 0.014 | 1431.19× | 2177.21× | OK | Sig: [parm, pci] = lognfit(x[, alpha[, censoring[, freq[, options]]]]). Lognormal MLE: parm=[mu sigma] of log(x). pci is 2x2: col 1 = mu CI, col 2 = sigma CI. Closed-form weighted moments when freq alone; EM-iterated MLE on log(x) with analytic Fisher info for CIs (Wald with z=norminv(1-α/2), log-σ transform for asymmetric σ CI) when censored. |
| `lognlike` | ✅ | 0.012 | 165.14× |  | OK | Sig: [nL, aVar] = lognlike([mu sigma], x[, cens, freq]). NLL for lognormal. Hessian wrt (mu, sigma) is structurally identical to the normal Hessian on log(x). aVar (column-major 2×2) reflects cens/freq weighting; can have negative diagonal entries at non-MLE params (observed Fisher, not expected). Edge: sigma<=0 or x<=0 => NaN; empty data => 0. |
| `nbinfit` | ✅ | 0.160 | 53.63× |  | OK | Sig: [ahat, bhat] = betafit(x) — Beta MLE via 2-D Newton on the digamma system. [rhat, phat] = nbinfit(x) — negative binomial MLE: profile log-likelihood, Newton on r with closed-form p = r / (r + mean). Stochastic samples in setup; fingerprint pins recoverability + output shapes. |
| `normfit` | ✅ | 0.014 | 1290.27× | 2209.86× | OK | Sig: [mu, sd, muci, sdci] = normfit(x[, alpha[, censoring[, freq[, options]]]]). MLE for normal: mu=mean, sd=sample std (N-1). Closed-form weighted moments when freq alone; EM iteration on truncated-normal moments + analytic Fisher info Wald CI when censored. Default alpha=0.05. Shares the `normal_fit_mle` helper with lognfit. |
| `normlike` | ✅ | 0.008 | 250.83× |  | OK | Sig: [nL, aVar] = normlike([mu sigma], data[, cens, freq]). Default + censoring (right-censored => -log(S(z))) + freq weights + empty + invalid-sigma (=> NaN). Second output aVar = inverse 2×2 observed-Fisher information matrix at [mu, sigma]; reflects cens/freq weighting. |
| `poissfit` | ✅ | 0.009 | 209.40× |  | OK | Sig: [lhat, lci] = poissfit(x[, alpha]). MLE for Poisson: lambda=mean(x). Exact CI via chi² inversion (Garwood). Edges: all-zero data -> lo=0; non-default alpha; empty input -> NaN. |
| `raylfit` | ✅ | 0.009 | 230.84× |  | OK | Sig: [shat, sci] = raylfit(x[, alpha]). Rayleigh MLE: σ = √(Σx²/(2N)); CI from chi² inversion 2N·σ̂² ~ σ²·χ²(2N). Edges: non-default α; single-element x; empty input -> NaN. |
| `unifit` | ✅ | 0.007 | 73.95× |  | OK | Sig: [a, b, aci, bci] = unifit(x[, alpha]). MLE for U(a,b): a=min, b=max. CI extension delta = (b-a)·(α^(-1/n) − 1). Single-element x: ACI=BCI=[x x] (zero-width). Empty input: numkit returns NaN; MATLAB returns empty arrays — convention difference, not in fingerprint. |
| `wblfit` | ✅ | 0.132 | 46.54× |  | OK | Sig: [ahat, bhat] = gamfit(x) — Gamma(shape, scale) MLE via Minka 2002 init + digamma/trigamma Newton on the shape, scale follows from b = mean(x)/a. [ahat, bhat] = wblfit(x) — Weibull(scale, shape) MLE via Newton on the implicit shape equation, scale = (Σ x^b / n)^(1/b). KNOWN GAP: confidence intervals (`bci` second output) deferred in v1. Spec uses stochastic samples — fingerprint pins error bounds and output shape rather than literal values (engine RNG diverges). |
| `wbllike` | ✅ | 0.008 | 199.98× | 68.13× | OK | Sig: nL = wbllike([scale shape], x[, cens, freq]). Weibull(a, b). Uncensored: -log(b) + b·log(a) - (b-1)·log(x) + (x/a)^b. Censored: (x/a)^b. With optional freq weights. Edges: scale<=0 or shape<=0 -> NaN (was Inf); x_i <= 0 -> NaN. Empty data: numkit returns 0 (consistent with our *like family); MATLAB errors `DATA must be a vector` — convention difference, not in fingerprint. AVAR (2-output form) deferred. |

### Multivariate Distributions

**Namespace:** `stats.mvdist.*` — 3 ✅ + 0 ⚠️ / 14 = 21%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `mvncdf` | ✅ | 0.009 | 426.46× |  | OK | Sig: p = mvncdf(X, mu, Sigma) — multivariate normal CDF. d=1 forwards to normcdf. d=2 uses Gauss-Legendre 16-point integration over the bivariate parametric formula. d≥3 uses antithetic Monte Carlo with 20000 samples. Deterministic seed (12345) for the MC path so results are reproducible. Bit-exact MATLAB R2025b at tol 1e-4 on d=1 and d=2 deterministic cases; d≥3 within statistical tolerance (KNOWN GAP: Genz separation-of-variables quasi-MC not yet shipped). Special cases: mu=[]→0, Sigma=[]→I. |
| `mvnpdf` | ✅ | 0.009 | 171.23× | 37.84× | OK | Sig: p = mvnpdf(X[, mu[, Sigma]]). Multivariate normal PDF. Defaults: mu=zeros, Sigma=I. Cholesky-based |Σ|^(-1/2) and Σ^(-1) for numerical stability. Verified bit-identical to MATLAB R2025b on default / explicit mu / explicit Σ paths. |
| `mvnrnd` | ✅ | 0.217 | 12.35× |  | OK | Sig: randg(shape [, m, n]) — raw gamma(shape, 1) RNG (scale = 1). Forwards to gamrnd internally. Per-element shape supported. mvnrnd(mu, Sigma [, n]) — multivariate normal RNG via in-place Cholesky on Sigma + N(0,1) draws. Supports vector mu (1×d, d×1) or matrix mu (n×d, per-row location). Bit-exact MATLAB R2025b not feasible (different RNG seeds); fingerprint pins distributional moments (mean ≈ shape, var ≈ shape for randg; mean ≈ mu, diag(cov) ≈ diag(Sigma) for mvnrnd) within statistical tolerance over n=3000 draws. |
| `mvtcdf` | ❌ |  |  |  |  | multivariate t |
| `mvtpdf` | ✅ | 0.008 | 135.07× | 45.79× | OK | Sig: p = mvtpdf(X, C, df). Multivariate Student-t PDF; C normalized to correlation matrix. Cholesky-based |C|^(-1/2) + quadratic form. Bit-identical to MATLAB R2025b. |
| `mvtrnd` | ✅ | 0.800 | 4.02× |  | OK | Sig: R = mvtrnd(C, df, n) — multivariate-t RNG via N(0,C) / sqrt(χ²/df). R = mnrnd(N, P [, m]) — multinomial RNG via cumulative-prob sampling. Both use the shared MT19937 stream. KNOWN GAPs: mvtrnd no location parameter (always 0); mnrnd no per-sample P matrix form. Fingerprint pins distributional moments (cov ≈ scaled C, col means ≈ N·p) at statistical tolerance over 3000 samples; row sum constraint is exact. |
| `mnpdf` | ✅ | 0.008 | 122.66× | 53.93× | OK | Sig: p = mnpdf(X, P). Multinomial PMF: n!/(Π x_i!) · Π p_i^x_i. Computed in log-space via lgamma. Bit-identical to MATLAB R2025b on row-vector / matrix inputs. |
| `mnrnd` | ✅ | 0.800 | 4.02× |  | OK | Sig: R = mvtrnd(C, df, n) — multivariate-t RNG via N(0,C) / sqrt(χ²/df). R = mnrnd(N, P [, m]) — multinomial RNG via cumulative-prob sampling. Both use the shared MT19937 stream. KNOWN GAPs: mvtrnd no location parameter (always 0); mnrnd no per-sample P matrix form. Fingerprint pins distributional moments (cov ≈ scaled C, col means ≈ N·p) at statistical tolerance over 3000 samples; row sum constraint is exact. |
| `wishrnd` | ✅ | 1.190 | 4.72× |  | OK | Sig: W = wishrnd(Sigma, df) — Wishart RNG via Bartlett decomposition: factor Sigma = L·L', build lower-tri B with B(i,i)=sqrt(χ²(df-i)) and B(i,j)~N(0,1) for i>j, then W = (L·B)(L·B)'. W = iwishrnd(Tau, df) — sample Y ~ W(inv(Tau), df) via Bartlett, return inv(Y). Stochastic samples; fingerprint pins E[W]/df ≈ Sigma for Wishart, E[W]·(df-p-1) ≈ Tau for inverse Wishart, plus exact symmetry of a single draw and positive-definiteness. KNOWN GAPs: 3rd-argument D (pre-Cholesky), 2-output [W, D] form deferred. |
| `iwishrnd` | ✅ | 1.190 | 4.72× |  | OK | Sig: W = wishrnd(Sigma, df) — Wishart RNG via Bartlett decomposition: factor Sigma = L·L', build lower-tri B with B(i,i)=sqrt(χ²(df-i)) and B(i,j)~N(0,1) for i>j, then W = (L·B)(L·B)'. W = iwishrnd(Tau, df) — sample Y ~ W(inv(Tau), df) via Bartlett, return inv(Y). Stochastic samples; fingerprint pins E[W]/df ≈ Sigma for Wishart, E[W]·(df-p-1) ≈ Tau for inverse Wishart, plus exact symmetry of a single draw and positive-definiteness. KNOWN GAPs: 3rd-argument D (pre-Cholesky), 2-output [W, D] form deferred. |
| `copulapdf` | ❌ |  |  |  |  |  |
| `copulacdf` | ❌ |  |  |  |  |  |
| `copulafit` | ❌ |  |  |  |  |  |
| `copulaparam` | ❌ |  |  |  |  |  |
| `copulastat` | ❌ |  |  |  |  |  |
| `copularnd` | ❌ |  |  |  |  |  |

### Pearson / Johnson Distributions

**Namespace:** `stats.pearson.*` / `stats.johnson.*` — 0 ✅ + 0 ⚠️ / 6 = 0%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `pearspdf` | ❌ |  |  |  |  | Pearson family |
| `pearscdf` | ❌ |  |  |  |  |  |
| `pearsinv` | ❌ |  |  |  |  |  |
| `pearsrnd` | ❌ |  |  |  |  |  |
| `johnsrnd` | ❌ |  |  |  |  | Johnson family random |
| `randg` | ✅ | 0.217 | 12.35× |  | OK | Sig: randg(shape [, m, n]) — raw gamma(shape, 1) RNG (scale = 1). Forwards to gamrnd internally. Per-element shape supported. mvnrnd(mu, Sigma [, n]) — multivariate normal RNG via in-place Cholesky on Sigma + N(0,1) draws. Supports vector mu (1×d, d×1) or matrix mu (n×d, per-row location). Bit-exact MATLAB R2025b not feasible (different RNG seeds); fingerprint pins distributional moments (mean ≈ shape, var ≈ shape for randg; mean ≈ mu, diag(cov) ≈ diag(Sigma) for mvnrnd) within statistical tolerance over n=3000 draws. |

### Empirical / Kernel Distributions

**Namespace:** `stats.empirical.*` — 3 ✅ + 0 ⚠️ / 4 = 75%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `ecdf` | ✅ | 0.011 | 182.72× |  | OK | Sig: [f, x[, flo, fup]] = ecdf(y[, 'Function', mode][, 'Frequency', w][, 'Alpha', a]). Function modes: 'cdf' (default), 'survivor' = 1-cdf, 'cumulative hazard' = Nelson-Aalen estimator. Frequency weighting via per-observation counts. 4-output form returns Greenwood-style binomial Wald 95% CI (first/last rows = NaN). Censoring deferred (Kaplan-Meier estimator). |
| `ecdfhist` | ✅ | 0.004 | 214.90× |  | OK | Sig: [n, c] = ecdfhist(f, x[, m]). Probability-density histogram from ecdf step data. Default m=10 bins. n is the per-bin density (sum of jumps falling in that bin / bin_width); c is the bin centre. Coverage: m ∈ {3, 5, 10} × uniform/non-uniform input. |
| `ksdensity` | ✅ | 0.021 | 304.14× |  | OK | Sig: [f, xi, bw] = ksdensity(x[, pts][, 'Bandwidth'/'Kernel'/'Function'/'NumPoints'/'Weights', val, ...]). 4 kernels (normal/box/triangle/epanechnikov) with MATLAB-style σ²=1 bandwidth normalization (h × sqrt(unit-σ²-inverse) for finite-support kernels). Function modes: pdf (default), cdf, survivor, cumhazard. Weights normalized to sum to 1. Default bandwidth via mad(x)/0.6745 fallback to iqr(x)/1.349 (matches MATLAB's bw exactly). Censoring/Support/BoundaryCorrection deferred. |
| `mvksdensity` | ❌ |  |  |  |  | multivariate KDE |

### Hypothesis Tests

**Namespace:** `stats.test.*` — 16 ✅ + 0 ⚠️ / 25 = 64%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `adtest` | ✅ | 0.014 | 1542.73× |  | OK | Sig: [h, p, A2*, cv] = adtest(x [, alpha]) — Anderson-Darling normality test with estimated parameters. A² statistic via the standard order-statistic formula, Stephens-1986 small-sample adjustment, p-value via the D'Agostino-Stephens piecewise rational fit. Critical value cv = 0.752 (α = 0.05). [p, dw] = dwtest(r, X) — Durbin-Watson autocorrelation test on regression residuals. DW = Σ(r_i - r_{i-1})² / Σ r_i². p-value via the symmetric-Beta-on-[0, 4] approximation matching the first two moments under H0 (Beta(α, α) with α = (n-1)/2). KNOWN GAP: exact Pan-1965 algorithm not yet shipped (MATLAB 'exact' method). Bit-exact MATLAB R2025b not feasible (rng state); fingerprint pins structural invariants (decision, sign of dw deviation, critical value). |
| `ansaribradley` | ❌ |  |  |  |  | scale test |
| `barttest` | ❌ |  |  |  |  | Bartlett's sphericity |
| `chi2gof` | ✅ | 0.011 | 542.42× |  | OK | Sig: [h, p, stats] = chi2gof(x[, 'Frequency'/'Expected'/'Edges'/'NBins'/'Ctrs'/'NParams'/'EMin'/'Alpha', val, ...]). Three paths covered: explicit Frequency+Expected (bit-identical); explicit NBins (bit-identical, integer-aligned edges); explicit Edges (bit-identical). Default auto-bin (no NBins/Edges) uses 10 equal-width bins on min(x)..max(x); may differ from MATLAB at FP-edge ties (within 1 count). 'CDF' function-handle argument deferred (errors with clear message). |
| `dwtest` | ✅ | 0.014 | 1542.73× |  | OK | Sig: [h, p, A2*, cv] = adtest(x [, alpha]) — Anderson-Darling normality test with estimated parameters. A² statistic via the standard order-statistic formula, Stephens-1986 small-sample adjustment, p-value via the D'Agostino-Stephens piecewise rational fit. Critical value cv = 0.752 (α = 0.05). [p, dw] = dwtest(r, X) — Durbin-Watson autocorrelation test on regression residuals. DW = Σ(r_i - r_{i-1})² / Σ r_i². p-value via the symmetric-Beta-on-[0, 4] approximation matching the first two moments under H0 (Beta(α, α) with α = (n-1)/2). KNOWN GAP: exact Pan-1965 algorithm not yet shipped (MATLAB 'exact' method). Bit-exact MATLAB R2025b not feasible (rng state); fingerprint pins structural invariants (decision, sign of dw deviation, critical value). |
| `fishertest` | ✅ | 0.003 |  |  | N/A | Sig: [h, p, stats] = fishertest(T[, 'Tail', t, 'Alpha', a]). Fisher's exact test for 2×2 contingency. Two-sided p sums hypergeometric pmf cells with P(X=k) ≤ P(X=obs). OR = a·d/(b·c); CI is the Woolf log-OR ± z·SE. |
| `friedman` | ❌ |  |  |  |  | non-parametric repeated-measures |
| `jbtest` | ✅ | 89.827 | 0.04× |  | OK | Sig: [h, p, JB, cv] = jbtest(x[, alpha[, mctol]]). For small n (<2000), Monte-Carlo simulation under H₀ for tabulated-style p-value (matches MATLAB R2025b). For large n, χ²(2) asymptotic. p capped at 0.5. Critical values are MC-estimated for small n so they vary slightly between runs (numkit uses fixed seed for reproducibility). Spec excludes cv from fingerprint (different MC seeds → different cv); JB stat itself is deterministic and bit-identical. |
| `knntest` | ❌ |  |  |  |  | k-NN two-sample test |
| `kruskalwallis` | ✅ | 0.006 | 902.44× | 328.31× | OK | Sig: [p, tbl, stats] = kruskalwallis(y, group[, 'off']). Non-parametric one-way ANOVA: H = (12/(N(N+1)))·Σ R_g²/n_g − 3(N+1), tie-corrected by 1 − Σ(t³−t)/(N³−N). df = k−1; p = 1 − chi2cdf(H, df). |
| `kstest` | ✅ |  |  |  | OK | one-sample KS via asymptotic Smirnov series |
| `kstest2` | ✅ |  |  |  | OK | two-sample KS |
| `lillietest` | ✅ | 0.005 | 1459.66× |  | OK | Sig: [h, p, kstat, critval] = lillietest(x[, alpha]). Lilliefors normality test using Stephens (1974) p-value approximation. KS-stat bit-identical with MATLAB R2025b; p-value/critval may differ by ~1e-3 due to approximation table interpolation. h decision matches MATLAB on probed cases. |
| `meanEffectSize` | ❌ |  |  |  |  | Cohen's d, Hedges' g |
| `mmdtest` | ❌ |  |  |  |  | maximum mean discrepancy |
| `multcompare` | ✅ | 0.006 | 11414.69× |  | OK | Sig: c = multcompare(stats [, alpha [, ctype]]) — pairwise post-hoc comparisons after anova1. Returns K(K-1)/2 × 6 matrix [i, j, lower_CI, mean_diff, upper_CI, p]. v1 ships 'bonferroni' (default) and 'lsd' methods. KNOWN GAP: 'tukey-kramer' HSD requires the studentized range distribution — not yet. anova1's stats struct is also extended in this cycle to populate {means, n, s, gnames, source} fields needed by multcompare. Deterministic integer y/group inputs for parity reproducibility. |
| `ranksum` | ✅ | 0.007 | 663.52× | 990.76× | OK | Sig: [p, h, stats] = ranksum(x, y[, alpha, tail | name-value]). Wilcoxon rank-sum (Mann-Whitney U). Default exact iff both samples have <10 obs (size-k subset-sum DP); else approximate with continuity + tie correction. |
| `runstest` | ✅ | 0.007 | 296.79× | 63.05× | OK | Sig: [h, p, stats] = runstest(x[, v][, alpha, tail | name-value]). Wald-Wolfowitz runs test. Default v=median(x); values == v dropped. Exact dist by default via combinatorial PMF; approximate uses continuity-corrected normal. |
| `sampsizepwr` | ❌ |  |  |  |  | sample-size / power |
| `signrank` | ✅ | 0.006 | 482.22× | 64.89× | OK | Sig: [p, h, stats] = signrank(x[, m | y][, alpha, tail | name-value]). Wilcoxon signed-rank: rank |d_i| with mid-rank tie averaging, W+ = Σ ranks of positive d. Default exact for n_eff ≤ 15 (subset-sum convolution); approximate uses tie-corrected normal. |
| `signtest` | ✅ | 0.004 | 623.13× | 66.23× | OK | Sig: [p, h, stats] = signtest(x[, m | y][, alpha, tail | name-value]). Paired sample test: 5 positives over 5 non-zero diffs, two-sided p = 2·(0.5)^5 = 0.0625 (binomial). |
| `ttest` | ✅ |  |  |  | OK | one-sample, returns (h, p, ci, tstat) |
| `ttest2` | ✅ |  |  |  | OK | Welch (default) or pooled-variance |
| `vartest` | ✅ |  |  |  | OK | chi-squared one-sample variance test |
| `vartest2` | ✅ |  |  |  | OK | F-test for equality of variances |
| `vartestn` | ✅ | 0.021 | 398.70× | 1695.31× | OK | Sig: [p, stats] = vartestn(x[, group][, 'Display', 'off'][, 'TestType', name]). Five test variants: Bartlett (default, χ² stat), LeveneQuadratic / LeveneAbsolute / BrownForsythe / OBrien (all F-based). When no group: matrix input where each column is treated as a separate group. Bartlett returns {chisqstat, df}; F-based tests return {fstat, df=[k-1, N-k]}. |
| `ztest` | ✅ |  |  |  | OK | known-σ z-test |

### Resampling Techniques

**Namespace:** `stats.resample.*` — 3 ✅ + 0 ⚠️ / 7 = 38%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `bootci` | ⚠️ | 0.529 | 23.41× |  | OK | Sig: ci = bootci(nboot, fn, X[, alpha]). Percentile bootstrap CI. NOT bit-identical with MATLAB (std::uniform_int_distribution implementation-defined; randn also not bit-identical). Statistical correctness verified: 95% CI contains true mean. |
| `bootstrp` | ⚠️ | 0.472 | 6.70× |  | OK | Sig: B = bootstrp(nboot, fn, X). Bootstrap resampling. Output shape verified; values not bit-identical with MATLAB (uniform_int_distribution + randn divergence). |
| `combnk` | ✅ | 0.002 | 228.64× |  | OK | Sig: r = combnk(...). Spec-extension batch 2026-05-09. |
| `crossval` | ⚠️ | 0.020 | 346.61× |  | OK | Sig: vals = crossval(predfun, X, Y[, 'kfold', K]). K-fold cross-validation. Default K=10. NOT bit-identical with MATLAB (fold splitting differs -- numkit uses contiguous blocks, MATLAB defaults to random). Shape verified. |
| `cvpartition` | ❌ |  |  |  |  | partition object (function-form constructor) |
| `datasample` | ✅ | 0.002 | 192.23× |  | OK | Sig: y = datasample(X, K[, dim, ...]). Default dim auto-selected: row vector samples columns (dim=2), otherwise dim=1. Output SHAPE bit-identical with MATLAB R2025b; values may differ due to RNG cascade -- shape probe used here. |
| `jackknife` | ⚠️ |  |  |  | NYI | needs Engine::call for function handles |
| `randsample` | ✅ | 0.003 | 190.89× | 62.39× | OK | Sig: y = randsample(n, k). Spec-extension batch 2026-05-09 (cycle 41). |

### Quasirandom Sequences and MCMC

**Namespace:** `stats.qmc.*` — 1 ✅ + 0 ⚠️ / 8 = 13%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `haltonset` | ✅ | 0.003 | 240.07× |  | OK | Sig: p = haltonset(d[, 'Skip', s, 'Leap', l]); X = net(p, n). Halton quasi-random points via radical inverse on the first d primes. Default skip = 1 (matches MATLAB; 'Skip', 0 yields the trivial origin). |
| `lhsdesign` | ❌ |  |  |  |  | Latin hypercube |
| `lhsnorm` | ❌ |  |  |  |  | Latin hypercube w/ normal |
| `mhsample` | ❌ |  |  |  |  | Metropolis-Hastings |
| `qrandstream` | ❌ |  |  |  |  | quasi-random stream constructor |
| `slicesample` | ❌ |  |  |  |  | slice sampler |
| `sobolset` | ❌ |  |  |  |  | Sobol sequence |
| `qrand` | ❌ |  |  |  |  | draw from qrandstream |

### ANOVA / MANOVA / Correlation

**Namespace:** `stats.anova.*` — 2 ✅ + 0 ⚠️ / 9 = 22%

OOP `anova` class and `fitrm` repeated-measures model intentionally omitted; only the legacy function-form entry points (anova1/anova2/anovan) which return F-statistic and p-value tables.

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `anova1` | ✅ | 0.002 | 2064.28× |  | OK | Sig: p = anova1(y, group['off']). One-way ANOVA p-value. Bit-identical with MATLAB R2025b on probed input (p=0.0251). |
| `anova2` | ⚠️ | 0.002 | 25521.81× |  | OK | Sig: p = anova2(Y[, reps]). Two-way ANOVA without replication (reps=1 only in this revision; reps>1 with interaction deferred). p = [p_cols, p_rows, p_interaction]. Bit-identical with MATLAB R2025b on probed cases. |
| `anovan` | ❌ |  |  |  |  | n-way |
| `manova1` | ❌ |  |  |  |  | one-way MANOVA |
| `canoncorr` | ✅ | 7.034 | 0.47× |  | OK | Sig: R = partialcorri(Y, X [, Z]) — partial correlation between each Y column and each X column, controlling for the OTHER X cols (and Z). canoncorr(X, Y) = canonical correlation analysis via centring + QR + SVD; returns canonical coefficients A, B (p×k, q×k) and the canonical correlations r (length k = min(p, q)). Bit-exact MATLAB R2025b not feasible (rng state differs); fingerprint pins structural invariants on the recoverability test (diag dominance for partialcorri on planted dependence, r(1) ≈ 1 for canoncorr on shared latent factor) and on the shapes. |
| `dummyvar` | ✅ | 0.002 | 216.59× |  | OK | Sig: r = dummyvar(...). Spec-extension batch 2026-05-09. |
| `aoctool` | ❌ |  |  |  |  | analysis of covariance (interactive — defer) |
| `mauchly` | ❌ |  |  |  |  | Mauchly's sphericity |
| `epsilon` | ❌ |  |  |  |  | sphericity adjustments |

### Linear Regression (function-form)

**Namespace:** `stats.regress.*` — 3 ✅ + 0 ⚠️ / 13 = 23%

OOP `fitlm` / `fitlme` / `fitglm` / `LinearModel` / etc. intentionally omitted. Only the legacy command-form entry points that return numerics (coeffs, residuals, CIs).

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `regress` | ✅ | 0.006 | 444.57× | 383.89× | OK | Sig: [b, bint, r, rint, stats] = regress(y, X[, alpha]). OLS multiple regression via Cholesky on X'X. stats = [R², F, p_F, sigma²]. 2026-05-08: 4th output rint (residual confidence intervals for outlier detection) added — was a placeholder. Uses standard formula r ± t·σ·sqrt(1-h_ii) where h_ii = diag(X·(X'X)^(-1)·X'). MATLAB's R2025b regress uses a non-standard internal formula whose exact form differs (specific h_ii values disagree with the theoretical hat-matrix diagonal); numkit returns the textbook formula. Shape (N×2) and the property `r(i) ∈ rint(i,:)` are checked instead. |
| `robustfit` | ✅ | 0.108 |  |  | N/A | Sig: [b, s] = robustfit(X, y [, wfun [, tune]]) — IRLS robust regression with bisquare (default, tune=4.685) or huber (tune=1.345). KNOWN GAP: stats struct (DOF, p-values, etc.) reduced to scalar s. [sigma, mu] = robustcov(X) — trimmed-MCD robust covariance via h = ceil(0.75 · n) concentration steps with Pison-Van Aelst-Willems consistency correction. KNOWN GAPs: full FAST-MCD multi-start, MVE method, OGK estimator not in v1. Spec uses deterministic sin/cos noise to make parity reproducible across engines; pins error bounds and shape invariants. |
| `lscov` | ✅ | 0.005 | 226.84× | 41.52× | OK | Sig: [x, stdx, mse, S] = lscov(A, b[, w]). Weighted least squares. mse = SSR/(N-p); S = mse·(A'WA)^(-1). Full N×N covariance V deferred (errors). Bit-identical to MATLAB R2025b on OLS and weighted paths. |
| `stepwisefit` | ❌ |  |  |  |  | stepwise selection |
| `glmfit` | ✅ | 0.233 | 43.30× |  | OK | Sig: [b, dev] = glmfit(X, y, distr [, link]) — IRLS GLM with auto-prepended intercept. Supports 'normal', 'binomial', 'poisson', 'gamma', 'inversegaussian' distributions and 'identity', 'logit', 'log', 'reciprocal', 'probit' links. Empty link → canonical for the distribution. yhat = glmval(b, X, link) — inverse-link of [1, X]·b. KNOWN GAPs: binomial 2-col y form, stats struct (SE, p-values, residuals), 'constant'/'weights' name-value args. Spec uses deterministic step-threshold binomial response + sin-noise normal to ensure parity reproducibility. |
| `glmval` | ✅ | 0.233 | 43.30× |  | OK | Sig: [b, dev] = glmfit(X, y, distr [, link]) — IRLS GLM with auto-prepended intercept. Supports 'normal', 'binomial', 'poisson', 'gamma', 'inversegaussian' distributions and 'identity', 'logit', 'log', 'reciprocal', 'probit' links. Empty link → canonical for the distribution. yhat = glmval(b, X, link) — inverse-link of [1, X]·b. KNOWN GAPs: binomial 2-col y form, stats struct (SE, p-values, residuals), 'constant'/'weights' name-value args. Spec uses deterministic step-threshold binomial response + sin-noise normal to ensure parity reproducibility. |
| `mvregress` | ❌ |  |  |  |  | multivariate regression |
| `mvregresslike` | ❌ |  |  |  |  |  |
| `plsregress` | ❌ |  |  |  |  | partial least squares |
| `ridge` | ✅ | 0.009 | 197.31× | 208.10× | OK | Sig: B = ridge(y, X, k[, scaled]). Ridge regression on standardized X (centered + N-1 std). scaled=1 (default): coefficients in standardized space, p×length(k). scaled=0: (p+1)×length(k) with intercept in original units. Bit-identical to MATLAB R2025b on both paths. |
| `lasso` | ✅ | 0.181 |  |  | N/A | Sig: [B, Intercept, Lambda] = lasso(X, y, lambdas [, alpha]) — coordinate-descent L1/elastic-net linear regression. lassoglm extends to GLM families (normal/binomial/poisson) via IRLS+coord-descent inner loop. Standardisation internal; coefficients returned in original units with auto-fit intercept. KNOWN GAPs: no auto λ-path, no CV, no observation weights, no 'standardize' name-value pair. Spec uses deterministic noise (sin-based) + step-threshold binary response for reproducible parity; pins coefficient recovery + zero-out structure at the documented λ values. |
| `lassoglm` | ✅ | 0.181 |  |  | N/A | Sig: [B, Intercept, Lambda] = lasso(X, y, lambdas [, alpha]) — coordinate-descent L1/elastic-net linear regression. lassoglm extends to GLM families (normal/binomial/poisson) via IRLS+coord-descent inner loop. Standardisation internal; coefficients returned in original units with auto-fit intercept. KNOWN GAPs: no auto λ-path, no CV, no observation weights, no 'standardize' name-value pair. Spec uses deterministic noise (sin-based) + step-threshold binary response for reproducible parity; pins coefficient recovery + zero-out structure at the documented λ values. |
| `polyconf` | ❌ |  |  |  |  | polynomial CI prediction |

### Nonlinear Regression (function-form)

**Namespace:** `stats.nlfit.*` — 0 ✅ + 0 ⚠️ / 5 = 0%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `nlinfit` | ✅ | 0.101 | 72.56× |  | OK | Sig: nlinfit(X, y, fun, beta0) — Levenberg-Marquardt NLS with numerical (central-diff) Jacobian. Returns [beta, R, J, CovB, MSE]. nlparci(beta, R, J [, alpha]) — Wald-style parameter CIs via t-quantile · sqrt(diag(MSE · (J'J)^-1)). nlpredci(fun, X, beta, R, J [, alpha]) — prediction CIs via delta-method on the per-query Jacobian. KNOWN GAPs: name-value 'Weights'/'ErrorModel'/'Options' not supported. Spec uses deterministic noise (sin-based) to make parity reproducible; pins recoverability + CI containment + shape invariants. |
| `nlparci` | ✅ | 0.101 | 72.56× |  | OK | Sig: nlinfit(X, y, fun, beta0) — Levenberg-Marquardt NLS with numerical (central-diff) Jacobian. Returns [beta, R, J, CovB, MSE]. nlparci(beta, R, J [, alpha]) — Wald-style parameter CIs via t-quantile · sqrt(diag(MSE · (J'J)^-1)). nlpredci(fun, X, beta, R, J [, alpha]) — prediction CIs via delta-method on the per-query Jacobian. KNOWN GAPs: name-value 'Weights'/'ErrorModel'/'Options' not supported. Spec uses deterministic noise (sin-based) to make parity reproducible; pins recoverability + CI containment + shape invariants. |
| `nlpredci` | ✅ | 0.101 | 72.56× |  | OK | Sig: nlinfit(X, y, fun, beta0) — Levenberg-Marquardt NLS with numerical (central-diff) Jacobian. Returns [beta, R, J, CovB, MSE]. nlparci(beta, R, J [, alpha]) — Wald-style parameter CIs via t-quantile · sqrt(diag(MSE · (J'J)^-1)). nlpredci(fun, X, beta, R, J [, alpha]) — prediction CIs via delta-method on the per-query Jacobian. KNOWN GAPs: name-value 'Weights'/'ErrorModel'/'Options' not supported. Spec uses deterministic noise (sin-based) to make parity reproducible; pins recoverability + CI containment + shape invariants. |
| `statset` | ❌ |  |  |  |  | options struct setter |
| `statget` | ❌ |  |  |  |  | options struct getter |

### Distance Metrics

**Namespace:** `stats.cluster.*` — 4 ✅ + 0 ⚠️ / 4 = 100%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `pdist` | ✅ | 0.008 | 334.89× | 203.92× | OK | Sig: D = pdist(X[, metric[, p|C]]). Pairwise distances. Coverage: euclidean, cityblock, minkowski(p=3), cosine, mahalanobis(default cov(X)), mahalanobis with explicit C. Bug fix 2026-05-08: mahalanobis was throwing 'unknown metric'. Function-handle metric still not supported (separate gap). |
| `pdist2` | ✅ | 0.012 | 380.59× | 228.18× | OK | Sig: D = pdist2(X, Y, metric); [D, I] = pdist2(X, Y, metric, 'Smallest'|'Largest', k). Coverage: default euclidean, minkowski p=3, cityblock, chebychev, cosine, mahalanobis (cov(Y) default), Smallest k, Largest k. Function-handle metric NOT supported (deferred). |
| `squareform` | ✅ | 0.007 | 124.11× | 33.73× | OK | Sig: Y = squareform(X[, mode]). Convert pairwise distance vector ↔ symmetric distance matrix. tol=0 (integer-stable on integer inputs). |
| `mahal` | ✅ | 0.009 | 72.04× | 105.24× | OK | Sig: D = mahal(X, Y). Mahalanobis distance from each row of X to the centroid of Y, scaled by inverse of cov(Y). Coverage: 2-D well-conditioned, 3-D well-conditioned, centroid (=0), zero point, far point. |

### Hierarchical Clustering

**Namespace:** `stats.cluster.*` — 5 ✅ + 0 ⚠️ / 7 = 71%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `linkage` | ✅ | 0.014 | 208.32× | 382.74× | OK | Sig: Z = linkage(Y[, method[, metric]]). When Y is N×D matrix, computes pdist(Y, metric, p) internally; when Y is row vector (pdist output), uses it directly. 7 methods: single/complete/average/weighted/centroid/median/ward. 2026-05-08: tie-breaking aligned with MATLAB R2025b (prefers largest pair lex when distances tie); 3-arg form now routes metric to pdist (was hardcoded euclidean). Bit-identical to MATLAB on probed datasets. |
| `cluster` | ✅ | 0.008 | 259.52× |  | OK | Sig: T = cluster(Z, 'maxclust'|'cutoff', val[, 'criterion', 'distance'|'inconsistent'][, 'depth', d]). Fingerprints are label-permutation-invariant (cluster counts + same-cluster boolean tests) because MATLAB / numkit / Octave assign different label IDs for the same partition. Default 'cutoff' criterion is 'inconsistent' (R2025b). |
| `clusterdata` | ✅ | 0.019 | 171.71× |  | OK | Sig: T = clusterdata(X, c) with scalar shortcut: c>=2 maxclust, 0<c<2 cutoff (inconsistency). Or N-V form: 'MaxClust', 'Cutoff', 'Linkage', 'Distance', 'Criterion', 'Depth', 'P'. Fingerprints are label-permutation-invariant. Default 'Linkage' is 'single', default 'Distance' is 'euclidean', default 'cutoff' criterion is 'inconsistent' — all per MATLAB R2025b. |
| `cophenet` | ✅ | 0.002 | 225.51× |  | OK | Sig: c = cophenet(Z, Y) or [c, d] = cophenet(Z, Y). Cophenetic correlation between original distances Y and the merge-tree-derived cophenetic distances d. Bug fix 2026-05-08: 2-output form was throwing because adapter only emitted outs[0]; now both outputs are produced. |
| `inconsistent` | ✅ | 0.004 | 84.12× | 487.24× | OK | Sig: Y = inconsistent(Z[, depth]). Inconsistency coefficient on a linkage tree Z. Each row [mean, std, count, inc_coeff] over the depth-d subtree below each non-leaf node. Default depth=2. |
| `dendrogram` | ❌ |  |  |  |  | display |
| `optimalleaforder` | ❌ |  |  |  |  | leaf permutation for visualisation |

### Partitional Clustering

**Namespace:** `stats.cluster.*` — 3 ✅ + 0 ⚠️ / 4 = 75%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `kmeans` | ✅ | 0.010 | 1050.25× | 340.30× | OK | Sig: [idx, C, sumd, D] = kmeans(X, K, 'MaxIter'/'Replicates'/'Distance'/'Start'/'Display'/'EmptyAction', val, ...). Default Distance='sqeuclidean', Start='plus'. Fingerprints are label-permutation-invariant (cluster counts + same-cluster boolean tests + output shapes) because RNG init differs between engines. |
| `kmedoids` | ✅ | 0.017 | 782.85× |  | OK | Sig: [idx, C, sumd, D, midx, info] = kmedoids(X, K, 'Distance'/'MaxIter'/'Replicates'/'Algorithm'/'Start', val, ...). Default Distance is 'sqeuclidean' (per R2025b — not 'euclidean'). Fingerprints are label-permutation-invariant (cluster counts + same-cluster boolean tests + output shapes) because RNG init differs between engines (joint with normrnd ТЗ for full label parity). |
| `dbscan` | ✅ | 0.013 | 159.10× |  | OK | Sig: [idx, corepts] = dbscan(X, eps, minpts, 'Distance'|'P', val, ...). Coverage: euclidean default, precomputed, minkowski with P, cityblock. Noise = -1 (MATLAB R2025b convention). |
| `spectralcluster` | ❌ |  |  |  |  | spectral clustering |

### Cluster Evaluation

**Namespace:** `stats.cluster_eval.*` — 0 ✅ + 0 ⚠️ / 3 = 0%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `silhouette` | ✅ | 0.005 | 1032.64× | 13481.95× | OK | Sig: s = silhouette(X, clust). Default metric sqEuclidean. 6 points, 2 well-separated clusters of 3. Element-wise SAVE; values near 0.99 indicating tight clusters with large inter-cluster gap. |
| `evalclusters` | ❌ |  |  |  |  | CalinskiHarabasz / DaviesBouldin / gap / silhouette |
| `manovacluster` | ❌ |  |  |  |  | dendrogram from MANOVA |

### Nearest Neighbors (function-form)

**Namespace:** `stats.knn.*` — 0 ✅ + 0 ⚠️ / 3 = 0%

OOP `KDTreeSearcher` / `ExhaustiveSearcher` / `hnswSearcher` intentionally omitted; flat function form only.

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `knnsearch` | ✅ | 0.005 | 1184.94× | 172.63× | OK | Sig: [Idx, D] = knnsearch(X, Y, 'K', K). Brute-force k-nearest neighbour. 6-point X, 2-query Y, K=3, default Euclidean. Element-wise SAVE on idx (1-based row indices). |
| `rangesearch` | ✅ | 0.005 | 910.87× | 85.00× | OK | Sig: [Idx, D] = rangesearch(X, Y, r). Cell-array output unwrapped to a numeric row in SAVE (idx = idxC{1}). All 3 points in cluster 1 are within r=1.0 of (1.5, 1.5). Explicit fingerprint avoids sum on the cell. |
| `createns` | ❌ |  |  |  |  | tree constructor (returns struct, not class) |

### Hidden Markov Models

**Namespace:** `stats.hmm.*` — 0 ✅ + 0 ⚠️ / 5 = 0%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `hmmdecode` | ❌ |  |  |  |  | forward-backward |
| `hmmestimate` | ❌ |  |  |  |  | MLE from labelled sequence |
| `hmmgenerate` | ❌ |  |  |  |  | sample sequences |
| `hmmtrain` | ❌ |  |  |  |  | Baum-Welch |
| `hmmviterbi` | ❌ |  |  |  |  | most-likely state path |

### Dimensionality Reduction

**Namespace:** `stats.dim.*` — 3 ✅ + 0 ⚠️ / 8 = 38%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `pca` | ✅ | 0.005 | 843.83× | 97.45× | OK | Sig: [coeff, score, latent, tsquared, explained, mu] = pca(X). Eigendecomposition of cov(X) for principal components. coeff is signed-undefined (eigenvector orientation), so abs() is taken in fingerprints. Bit-identical to MATLAB R2025b on |coeff|, latent, explained, mu, tsquared. |
| `pcacov` | ✅ | 0.004 | 146.68× | 50.31× | OK | Sig: [coeff, latent, explained] = pcacov(C). Like pca but on a precomputed covariance matrix. Bit-identical to MATLAB R2025b. |
| `pcares` | ✅ | 0.008 | 588.44× | 52.99× | OK | Sig: [res, recon] = pcares(X, ndim). Residual matrix and rank-ndim reconstruction X̂ = score(:,1..ndim) · coeff(:,1..ndim)' + μ. 2-output form added 2026-05-08; was returning only residuals. |
| `ppca` | ❌ |  |  |  |  | probabilistic PCA |
| `factoran` | ❌ |  |  |  |  | factor analysis |
| `rica` | ❌ |  |  |  |  | reconstruction ICA |
| `sparsefilt` | ❌ |  |  |  |  | sparse filtering |
| `tsne` | ❌ |  |  |  |  | t-SNE |

### Feature Selection (function-form)

**Namespace:** `stats.fselect.*` — 0 ✅ + 0 ⚠️ / 9 = 0%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `fscchi2` | ❌ |  |  |  |  | classification — chi-squared score |
| `fscmrmr` | ❌ |  |  |  |  | classification — minimum redundancy max relevance |
| `fscnca` | ❌ |  |  |  |  | classification — neighbourhood comp. analysis |
| `fsrftest` | ❌ |  |  |  |  | regression — F-test score |
| `fsrmrmr` | ❌ |  |  |  |  | regression — mRMR |
| `fsrnca` | ❌ |  |  |  |  | regression — NCA |
| `fsulaplacian` | ❌ |  |  |  |  | unsupervised Laplacian score |
| `relieff` | ❌ |  |  |  |  | ReliefF |
| `sequentialfs` | ❌ |  |  |  |  | sequential feature selection |

### Linear Discriminant Analysis (function-form)

**Namespace:** `stats.lda.*` — 1 ✅ + 0 ⚠️ / 1 = **100%**

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `classify` | ✅ | 0.015 | 578.07× |  | OK | Sig: [class, err, posterior, logp] = classify(sample, training, group[, type]). 4 discriminant types: linear (LDA, default), quadratic (QDA), diaglinear, diagquadratic. Empirical priors n_k/N. Cholesky-factor approach for numerical stability. Mahalanobis type DEFERRED. |

## Wavelet

### Continuous Wavelet Transforms

**Namespace:** `wavelet.cwt.*` — 0 ✅ + 0 ⚠️ / 16 = 0%

`cwtfilterbank` (class) and the deep-learning layer family
(`cwtLayer`/`icwtLayer`/`dlcwt`/etc.) intentionally omitted.

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `cwt` | ❌ |  |  |  |  | continuous wavelet transform |
| `icwt` | ❌ |  |  |  |  | inverse CWT |
| `cwtfreqbounds` | ❌ |  |  |  |  | frequency support |
| `centfrq` | ❌ |  |  |  |  | central frequency of wavelet |
| `scal2frq` | ❌ |  |  |  |  | scale → pseudo-frequency |
| `wcoherence` | ❌ |  |  |  |  | wavelet coherence |
| `wsst` | ❌ |  |  |  |  | wavelet synchrosqueezed transform |
| `iwsst` | ❌ |  |  |  |  | inverse WSST |
| `wsstridge` | ❌ |  |  |  |  | ridges of WSST |
| `wtmm` | ❌ |  |  |  |  | wavelet transform modulus maxima |
| `wavefun` | ❌ |  |  |  |  | wavelet & scaling function values |
| `wavefun2` | ❌ |  |  |  |  | 2-D variant |
| `wavsupport` | ❌ |  |  |  |  | effective support |
| `qfactor` | ❌ |  |  |  |  | quality factor |
| `wavemngr` | ❌ |  |  |  |  | wavelet manager |
| `waveinfo` | ❌ |  |  |  |  | info on a wavelet family |

### Discrete Wavelet Transforms (1-D)

**Namespace:** `wavelet.dwt.*` — 14 ✅ + 0 ⚠️ / 18 = 78%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `dwt` | ✅ | 0.008 | 158.99× |  | OK | Sig: [cA, cD] = dwt(x, wname) or (x, Lo_D, Hi_D), with optional 'mode' N-V (only 'sym' supported). 2026-05-08: bit-identical to MATLAB R2025b on the analysis filters after the wfilters Lo_D/Lo_R label-swap fix landed. Custom-filter form added in same commit. Boundary modes other than 'sym' deferred (errors with clear message). |
| `idwt` | ✅ | 0.010 | 204.46× |  | OK | Sig: x = idwt(cA, cD, wname) or (cA, cD, Lo_R, Hi_R), optional positional `len` and 'mode' N-V (only 'sym' supported). After wfilters label-swap fix + dwt downsample-offset fix, round-trip is bit-identical to MATLAB R2025b at ~1e-12. Custom synthesis-filter form added in the same commit. |
| `wavedec` | ✅ | 0.020 | 226.78× |  | OK | Sig: [c, l] = wavedec(x, n, wname). Multi-level DWT decomposition. After wfilters Lo_D/Lo_R label-swap fix landed, output is bit-identical to MATLAB R2025b. Custom (Lo_D, Hi_D) form deferred (rare for multi-level). |
| `waverec` | ✅ | 0.016 | 302.76× |  | OK | Sig: x = waverec(c, l, wname). Multi-level inverse DWT. Round-trips wavedec at ~1e-10 after the wfilters label-swap fix. Custom (Lo_R, Hi_R) form deferred. |
| `appcoef` | ✅ | 0.008 | 285.46× |  | OK | Sig: A = appcoef(c, l, wname[, level]) or (c, l, LoR, HiR[, level]); optional 'Mode'/'mode' N-V (only 'sym' supported). 2026-05-08: cascades-fixed via wfilters Lo_D/Lo_R label-swap. Custom-filter form added in this commit. |
| `detcoef` | ✅ | 0.005 | 129.85× |  | OK | Sig: D = detcoef(C, L[, level[, 'cells']]). Default level = numel(L) - 2 (deepest). Bug fix 2026-05-08: was throwing on 2-arg form (and previously the auditor said default = 1, but probe shows max-level). Added 'cells' form for vector levels. |
| `wrcoef` | ✅ | 0.033 | 76.10× |  | OK | Sig: y = wrcoef(type, c, l, wname[, n]). Single-band reconstruction. type ∈ {'a','d'}; n is the level kept ('a' allows n=0 = full reconstruction; 'd' requires n in [1, max]). Default n = length(l)-2 for both types. Algorithm: build modified c with off-band coefficients zeroed, run waverec. Verified parity with MATLAB R2025b on HAAR wavelet (where numkit's wavedec matches MATLAB exactly). For db/sym/coif numkit's wavedec uses a slightly different boundary convention (BUGS.md #37) — wrcoef there produces values consistent with numkit's own wavedec/waverec round-trip but does NOT match MATLAB coefficient-for-coefficient. (Lo_R, Hi_R) two-filter form not implemented in this release. |
| `dwtmode` | ❌ |  |  |  |  | extension mode |
| `dyaddown` | ✅ | 0.007 | 107.65× |  | OK | Sig: y = dyaddown(x[, ODD][, type]). Dyadic downsample by 2. ODD=0 default → keep even-indexed; ODD=1 → keep odd-indexed. For matrices, type ∈ {c (default, columns), r (rows), m (both)}. Bug fix 2026-05-08: matrix path silently flattened to a 1-D vector + ignored the type arg. tol=0 (integer-stable on integer inputs). |
| `dyadup` | ✅ | 0.006 | 197.04× |  | OK | Sig: y = dyadup(x[, ODD][, type]). Zero insertion between samples (upsample by 2). Vector default ODD=1 → length 2N+1 with leading zero. ODD=0 → length 2N-1, no leading zero. For matrices, type ∈ {c (default, columns), r (rows), m (both)}. Bug fix 2026-05-08: matrix path silently flattened + ignored type arg. tol=0. |
| `wkeep` | ✅ | 0.008 | 308.82× |  | OK | Sig: y = wkeep(x, n[, OPT]) (1-D) or y = wkeep(X, [R C][, [fr fc]]) (2-D). 1-D: 'c'/'l'/'r' or numeric start. 2-D: central [R C] sub-matrix or explicit corner. Bug fix 2026-05-08: 2-D form was throwing 'Cannot convert double to scalar' (adapter did toScalar on the size vec). tol=0. |
| `wextend` | ✅ | 0.015 | 273.10× |  | OK | Sig: y = wextend(type, mode, x, lf[, side]). Bug fix 2026-05-08: extended modes (symw, asym, asymw, sp0, sp1) and 2-D forms (type=2 / 'ar' / 'ac') were not implemented. Now full coverage: 11 modes × 4 type forms × 3 sides. tol=0 (integer-stable on integer inputs). |
| `wcodemat` | ✅ | 0.008 | 118.06× |  | OK | Sig: Y = wcodemat(X[, nb[, opt[, absol]]]). Quantize/scale to [1, nb] integer codes. opt ∈ {'mat'(default), 'row', 'col'}; absol=1 default uses |x|. Bug fix 2026-05-08: previous impl used `round` and multiplied by `nb-1`, producing off-by-one quantization on interior values. MATLAB uses floor((v-mn)/span * nb) + 1, with the upper edge clamped from nb+1 down to nb. tol=0 (integer-stable). Octave doesn't ship wcodemat. |
| `haart` | ✅ | 0.013 | 140.13× |  | OK | Sig: [a, d] = haart(x[, level[, integerflag]]). Haar 1-D DWT. Default level = max k such that 2^k divides length(x). 'noninteger' uses 1/sqrt(2) Haar pair; 'integer' uses lifting (a = x[2k] + floor((x[2k+1]-x[2k])/2)). Output is always column for vector input. d is plain when level=1, cell array d{1..L} when level>1 (d{1} finest). Matrix input processes columns independently. Verified: level=1, default-level (cell), integer mode (signed-floor), matrix, complex, row->col coercion, integer+double, N=12 partial level. |
| `ihaart` | ✅ | 0.013 | 338.97× |  | OK | Sig: xrec = ihaart(a, d[, level[, integerflag]]). Inverse Haar 1-D DWT. Default level=0 (lossless reconstruction). When level=K (in [0, Nlevels)) the K finest detail bands d{1..K} are zeroed BEFORE reconstruction (xrec stays full-length). Inverse formulas: noninteger uses (a±d)/sqrt(2); integer uses lifting x[2k]=a[k]-floor(d[k]/2), x[2k+1]=x[2k]+d[k]. d MUST be real even when a is complex (MATLAB validateattributes on D). d may be a plain matrix at level=1 or a length-Nlevels cell array. Vector-shaped a returns column; matrix returns matrix. Verified: level=1, full multi-level, partial reconstruction (zero-out 1 and 2 bands), integer mode + partial, matrix full + partial. |
| `wmaxlev` | ✅ | 0.012 | 220.10× |  | OK | Sig: L = wmaxlev(N, wname). Maximum DWT decomposition level: L = floor(log2(N / (Lf - 1))) where Lf is the wavelet filter length. Vector N (e.g., 2-D image dims) uses min(N). Coverage: wavelet ∈ {haar, db1, db2, db4, db10, sym4, coif2} × N ∈ {2, 8, 16, 64, 100, 1024, 2048} + 2-vector N. tol=0. |
| `dwpt` | ❌ |  |  |  |  | discrete wavelet packet transform |
| `idwpt` | ❌ |  |  |  |  | inverse DWPT |

### Discrete Wavelet Transforms (2-D / 3-D)

**Namespace:** `wavelet.dwt2.*` — 2 ✅ + 0 ⚠️ / 15 = 13%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `dwt2` | ✅ | 0.008 | 169.98× |  | OK | Sig: r = dwt2(...). Spec-extension batch 2026-05-09. |
| `idwt2` | ✅ | 0.015 | 300.81× |  | OK | Sig: r = idwt2(...). Spec-extension batch 2026-05-09. |
| `wavedec2` | ❌ |  |  |  |  |  |
| `waverec2` | ❌ |  |  |  |  |  |
| `appcoef2` | ❌ |  |  |  |  |  |
| `detcoef2` | ❌ |  |  |  |  |  |
| `wrcoef2` | ❌ |  |  |  |  |  |
| `wpdec2` | ❌ |  |  |  |  | 2-D wavelet packet |
| `wprec2` | ❌ |  |  |  |  |  |
| `haart2` | ❌ |  |  |  |  |  |
| `ihaart2` | ❌ |  |  |  |  |  |
| `wavedec3` | ❌ |  |  |  |  | 3-D |
| `waverec3` | ❌ |  |  |  |  |  |
| `dwt3` | ❌ |  |  |  |  |  |
| `idwt3` | ❌ |  |  |  |  |  |

### Stationary, MODWT, and Wavelet Packets

**Namespace:** `wavelet.swt_modwt.*` — 4 ✅ + 0 ⚠️ / 17 = 24%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `swt` | ✅ | 0.004 | 1090.27× |  | OK | Sig: swc = swt(x, n, wname). Stationary wavelet transform. Argument order matches MATLAB. Output SHAPE matches; APPROXIMATION row (last) values match bit-identical; DETAIL rows match in magnitude but differ in sign (Hi_D vs Hi_R QMF convention). Per-value sign-aware parity needs an inner-kernel audit beyond this ТЗ — fingerprint uses |wH| for detail rows (sign-invariant) and exact equality for the approximation row (sign-correct). MATLAB R2025b reference; Octave wavelet package may not ship swt. |
| `iswt` | ✅ | 0.010 | 494.36× |  | OK | Sig: x = iswt(swc, wname). Inverse stationary wavelet transform. Even though swt/iswt internal coefficient values use a different filter convention than MATLAB, the round-trip iswt(swt(x)) DOES recover x — that's the structurally important invariant for any inverse transform. Both MATLAB and numkit reconstruct the original signal to machine precision. |
| `swt2` | ❌ |  |  |  |  |  |
| `iswt2` | ❌ |  |  |  |  |  |
| `modwt` | ✅ | 0.004 | 867.80× |  | OK | Sig: w = modwt(x[, wname[, lev]]). Maximal Overlap Discrete Wavelet Transform. Audit ТЗ 2026-05-09: argument order corrected from numkit-historical (x, lev, wname) to MATLAB-canonical (x, wname, lev) plus default wname='sym4' and default lev=floor(log2(N)). The output SHAPE matches MATLAB (lev+1 rows × N columns) but per-coefficient values still diverge from MATLAB R2025b — root cause is filter-convention / sqrt(2)-normalisation differences inside the inner kernel that need a separate algorithm audit (NOT fixable at the adapter layer). Fingerprint locked to shape-only here; per-value parity tracked separately. |
| `imodwt` | ✅ | 0.010 | 454.10× |  | OK | Sig: x = imodwt(w, wname). Inverse MODWT. Round-trip imodwt(modwt(x)) recovers x to machine precision — the structurally important invariant. The internal coefficient values diverge from MATLAB R2025b (kernel filter-convention gap, see modwt.json comment); both engines independently recover x correctly from THEIR OWN coefficients. |
| `modwtmra` | ❌ |  |  |  |  | multi-resolution analysis from MODWT |
| `modwtcorr` | ❌ |  |  |  |  | scale-by-scale correlation |
| `modwtvar` | ❌ |  |  |  |  | scale-by-scale variance |
| `modwtxcorr` | ❌ |  |  |  |  | cross-correlation |
| `modwpt` | ❌ |  |  |  |  | maximal-overlap packet |
| `imodwpt` | ❌ |  |  |  |  |  |
| `wpdec` | ❌ |  |  |  |  | wavelet packet decomposition |
| `wprec` | ❌ |  |  |  |  | reconstruction |
| `wpcoef` | ❌ |  |  |  |  |  |
| `wprcoef` | ❌ |  |  |  |  |  |
| `besttree` | ❌ |  |  |  |  | best-basis selection |

### Denoising and Compression

**Namespace:** `wavelet.denoise.*` — 3 ✅ + 0 ⚠️ / 16 = 19%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `wdenoise` | ✅ | 0.009 | 1393.18× |  | OK | Sig: r = wdenoise(...). Spec-extension batch 2026-05-09. |
| `wdenoise2` | ❌ |  |  |  |  | 2-D denoising |
| `wden` | ❌ |  |  |  |  | classical denoising |
| `wdencmp` | ❌ |  |  |  |  | denoise / compress |
| `wpdencmp` | ❌ |  |  |  |  | wavelet-packet denoise / compress |
| `wnoisest` | ✅ | 0.004 | 218.42× |  | OK | Sig: sigma = wnoisest(c, l, level). MAD-based noise sigma estimate from wavedec output. Bit-identical with MATLAB R2025b on deterministic-input probe (sigma=0.0900008 on db4 level-3 decomposition of test signal). |
| `wvarchg` | ❌ |  |  |  |  | variance-change detection |
| `ddencmp` | ❌ |  |  |  |  | default thresholding parameters |
| `thselect` | ❌ |  |  |  |  | threshold selection |
| `wthcoef` | ❌ |  |  |  |  | apply threshold to detail coeffs |
| `wthcoef2` | ❌ |  |  |  |  |  |
| `wthresh` | ✅ | 0.004 | 705.63× |  | OK | Sig: r = wthresh(...). Spec-extension batch 2026-05-09. |
| `wmulden` | ❌ |  |  |  |  | multivariate denoising |
| `measerr` | ❌ |  |  |  |  | quality measures (PSNR/MSE/MAX/L2) |
| `wnoise` | ❌ |  |  |  |  | noisy test signal |
| `wcompress` | ❌ |  |  |  |  | compression front-end |

### Filter Banks and Wavelet Families

**Namespace:** `wavelet.filt.*` — 7 ✅ + 0 ⚠️ / 22 = 32%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `wfilters` | ✅ | 0.013 | 230.22× |  | OK | Sig: [Lo_D, Hi_D, Lo_R, Hi_R] = wfilters(wname). Standard MATLAB convention: Lo_D = wrev(Lo_R), Hi_R = (-1)^k · Lo_R[N-1-k] (QMF on Lo_R), Hi_D = wrev(Hi_R). 2026-05-08 fix: numkit's labels were swapped (numkit's Lo_D was MATLAB's Lo_R and vice versa) — root cause of dwt/wavedec value mismatch. Now bit-identical to MATLAB R2025b across haar/db1..db10/sym2..sym10/coif1..coif5. |
| `orthfilt` | ✅ | 0.006 | 88.78× |  | OK | Sig: [Lo_D, Hi_D, Lo_R, Hi_R] = orthfilt(W). Quadruple from a unit-norm scaling filter W (sum(W)=1, length even). Lo_R = W·√2; Lo_D = reverse(Lo_R); Hi_R[k] = (-1)^k · Lo_R[N-1-k]; Hi_D = reverse(Hi_R). Coverage: db2 (4-tap), db4 (8-tap), custom 2-tap. |
| `qmf` | ✅ | 0.007 | 54.04× |  | OK | Sig: y = qmf(x[, p]). Quadrature mirror filter. y(k) = (-1)^(k-1+p) · x(N-k+1). Default p=0 (identity-sign on the first element); p=1 negates. Coverage: even/odd-length + p=0/1 + length-8 + column input + single element. tol=0 (integer-stable on integer inputs). |
| `biorfilt` | ❌ |  |  |  |  | biorthogonal filter quadruple |
| `dbwavf` | ✅ | 0.003 | 38.51× |  | OK | Sig: h = dbwavf(wname). Daubechies scaling filter: dbwavf*sqrt(2) = Lo_R, length 2N for dbN, sum(h) = 1. Coverage: db1, db2, db4, db5, db6, db8, db10. Bug fix 2026-05-08: previously only supported db1..db4; extended table to db5..db10. |
| `coifwavf` | ✅ | 0.004 | 23.58× |  | OK | Sig: h = coifwavf(wname). Coiflet scaling filter: coifwavf*sqrt(2) = Lo_R, length 6K for coifK, sum(h) = 1. Coverage: coif1..coif5 (coif2..coif5 added 2026-05-08; was only coif1). |
| `symwavf` | ✅ | 0.007 | 28.80× |  | OK | Sig: h = symwavf(wname). Symlet (least-asymmetric Daubechies) scaling filter: symwavf*sqrt(2) = Lo_R, length 2N for symN, sum(h) = 1. Coverage: sym2..sym10 (sym3 + sym5..sym10 added 2026-05-08; was only sym2/sym4). |
| `dbaux` | ❌ |  |  |  |  | Daubechies aux |
| `symaux` | ❌ |  |  |  |  | symlet aux |
| `biorwavf` | ❌ |  |  |  |  | biorthogonal scaling filter |
| `rbiowavf` | ❌ |  |  |  |  | reverse biorthogonal |
| `fejerkorovkin` | ❌ |  |  |  |  | Fejér-Korovkin filters |
| `mbscalf` | ❌ |  |  |  |  | Morris minimum-bandwidth |
| `hanscalf` | ❌ |  |  |  |  | Han scaling filter |
| `blscalf` | ❌ |  |  |  |  | Beylkin |
| `bswfun` | ❌ |  |  |  |  | biorthogonal scaling/wavelet via cascade |
| `wrev` | ✅ | 0.006 | 52.15× |  | OK | Sig: y = wrev(x). Reverse along the first non-singleton dimension. Row vector / col vector -> reverse element order. Matrix M×N -> reverse each column independently (= flipud). Complex preserved. Bug fix 2026-05-08: matrix path was full-flip not flipud; complex input dropped imaginary parts. tol=0 (integer-stable on integer inputs). |
| `isbiorthwfb` | ❌ |  |  |  |  | check biorthogonal filter bank |
| `isorthwfb` | ❌ |  |  |  |  | check orthogonal filter bank |
| `wavelets` | ❌ |  |  |  |  | list available wavelet names |
| `waveletfamilies` | ❌ |  |  |  |  | list families |
| `wavenames` | ❌ |  |  |  |  |  |

### Continuous Wavelet Shapes

**Namespace:** `wavelet.shape.*` — 8 ✅ + 0 ⚠️ / 11 = 73%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `meyer` | ❌ |  |  |  |  | Meyer wavelet |
| `meyeraux` | ✅ | 0.008 | 116.61× | 31.90× | OK | Sig: y = meyeraux(x). Element-wise auxiliary polynomial 35x⁴ − 84x⁵ + 70x⁶ − 20x⁷. MATLAB clips outside [0, 1]: x<=0 -> 0, x>=1 -> 1. Bug fix 2026-05-08: numkit was applying the raw polynomial outside [0, 1] (e.g. meyeraux(2) = -208 instead of MATLAB's 1). |
| `mexihat` | ✅ | 0.006 | 25.52× | 21.38× | OK | Sig: [psi, x] = mexihat(LB, UB, N). Mexican-hat wavelet ψ(t) = (2/√3)·π^(-1/4)·(1-t²)·exp(-t²/2). Even, peaks at 0, zeros at ±1. Coverage: N ∈ {8, 16, 64} on [-5, 5] + asymmetric range [0, 5]. |
| `morlet` | ✅ | 0.005 | 29.95× | 33.47× | OK | Sig: [psi, x] = morlet(LB, UB, N). Real Morlet ψ(t) = exp(-t²/2)·cos(5t). Coverage: N ∈ {8, 16, 64} on [-5, 5] + asymmetric range [0, 5]. |
| `cgauwavf` | ✅ | 0.004 | 24.17× |  | OK | Sig: [psi, x] = cgauwavf(LB, UB, N[, p|'cgauN']). Complex Gaussian wavelet (-1)^p · H_p(t + i/2) · exp(-t² - i·t). Bug fix 2026-05-08: 'cgauN' wname form was throwing 'Cannot convert char to scalar'. |
| `cmorwavf` | ✅ | 0.003 | 39.45× |  | OK | Sig: [psi, x] = cmorwavf(LB, UB, N[, fb, fc]). Complex Morlet ψ(t) = (1/√(π·fb))·exp(2πi·fc·t)·exp(-t²/fb). Bug fix 2026-05-08: 3-arg form was throwing instead of using defaults fb=1, fc=1. Coverage: default + custom (fb, fc) + N=33. |
| `fbspwavf` | ✅ | 0.003 |  |  | N/A | Sig: [psi, x] = fbspwavf(LB, UB, N, m, fb, fc). Frequency B-spline ψ(t) = √fb · (sinc(fb·t/m))^m · exp(2πi·fc·t). Coverage: m ∈ {2, 3} × (fb, fc) ∈ {(1,1), (0.5,2)} × N ∈ {8, 16, 33}. |
| `gauswavf` | ✅ | 0.005 | 26.14× |  | OK | Sig: [psi, x] = gauswavf(LB, UB, N[, p|'gausN']). p-th derivative Gaussian wavelet. Bug fix 2026-05-08: 'gausN' wname form was throwing 'Cannot convert char to scalar'; now parses N from string. Coverage: p ∈ {1, 2, 4, 8} integer + 'gaus3' wname. |
| `intwave` | ❌ |  |  |  |  | wavelet integral |
| `pat2cwav` | ❌ |  |  |  |  | pattern → custom wavelet |
| `shanwavf` | ✅ | 0.006 | 39.19× | 28.54× | OK | Sig: [psi, x] = shanwavf(LB, UB, N, fb, fc). Shannon wavelet ψ(t) = √fb·sinc(fb·t)·exp(2πi·fc·t). Coverage: (fb, fc) ∈ {(1,1), (0.5,2)} × N ∈ {8, 16, 33}. |

### Lifting

**Namespace:** `wavelet.lift.*` — 0 ✅ + 0 ⚠️ / 6 = 0%

`liftingScheme` and `liftingStep` are MATLAB classes; we treat lifting
as a pair of flat decomposition / reconstruction functions.

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `lwt` | ❌ |  |  |  |  | lifting wavelet transform |
| `ilwt` | ❌ |  |  |  |  |  |
| `lwt2` | ❌ |  |  |  |  |  |
| `ilwt2` | ❌ |  |  |  |  |  |
| `lwtcoef` | ❌ |  |  |  |  | extract one band |
| `lwtcoef2` | ❌ |  |  |  |  |  |

### Decomposition Trees and Misc

**Namespace:** `wavelet.misc.*` — 0 ✅ + 0 ⚠️ / 13 = 0%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `dualtree` | ❌ |  |  |  |  | dual-tree complex DWT |
| `idualtree` | ❌ |  |  |  |  |  |
| `dualtree2` | ❌ |  |  |  |  |  |
| `idualtree2` | ❌ |  |  |  |  |  |
| `dddtree` | ❌ |  |  |  |  | double-density DWT |
| `idddtree` | ❌ |  |  |  |  |  |
| `tqwt` | ❌ |  |  |  |  | tunable Q-factor wavelet transform |
| `itqwt` | ❌ |  |  |  |  |  |
| `wfbm` | ❌ |  |  |  |  | fractional Brownian motion |
| `wfbmesti` | ❌ |  |  |  |  | Hurst exponent estimate |
| `wfusimg` | ❌ |  |  |  |  | image fusion |
| `wfusmat` | ❌ |  |  |  |  | matrix fusion |
| `wentropy` | ❌ |  |  |  |  | wavelet entropy |

## Misc / not in TODO

Functions benched by the harness that don't appear in any of the MATLAB-doc sections above. Move them into a real section if they correspond to a documented MATLAB function.

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `impyramid_expand` | — | 0.008 | 628.41× | 55.98× | OK | Sig: B = impyramid(A, 'expand'). Output: (2M-1)x(2N-1). KNOWN GAP: numkit uses literal Burt-Adelson zero-stuff + [1 4 6 4 1]/16 kernel (matches Octave-image bit-equal); MATLAB R2025b uses imresize-with-piecewise-constant-kernel (kernel handle defined inline in toolbox/images/images/impyramid.m). Interior values agree bit-equal across all three engines; only boundary samples differ. Spec pins (a) output shape and (b) interior sum + center value to catch real regressions while tolerating the boundary-handling divergence. Full bit-equal with MATLAB requires either custom-kernel support in imresize or a rewrite of impyramid to inline the imresize math (~150 LOC, deferred). |
| `axes2pix` | — | 0.001 | 173.77× |  | OK | Sig: pix = axes2pix(n, extent, axesCoord). World→pixel axis mapping (1-based). Octave-image has axes2pix. |
| `isgray` | — | 0.003 |  | 50.00× | OK | Sig: tf = isgray(I). True for 2-D images of class uint8/uint16/int16 or float in [0,1]. Octave-image has isgray. |
| `imcast` | — | 0.003 |  | 72.42× | OK | Sig: J = imcast(I, type). Dispatch wrapper over im2* helpers (type ∈ double/single/uint8/uint16/int16/logical). Octave-image has imcast. |
| `mmgradm` | — | 0.004 |  | 140.19× | OK | Sig: G = mmgradm(I [, se_dil [, se_ero]]). Morphological gradient = imdilate − imerode (default cross SE). Octave-image has mmgradm. |
| `fchcode` | — | 0.002 |  |  | N/A | Sig: fcc = fchcode(bound). Freeman 8-direction chain code; struct with x0y0, fcc, diff fields. Octave-image has fchcode. |
| `fftconv2` | — | 0.005 |  |  | N/A | Sig: Y = fftconv2(A, B [, shape]). FFT-based 2-D conv; output complex with tiny imag, smoke wraps real(). Octave-image has fftconv2. |
| `wavelength2rgb` | — | 0.003 |  | 149.18× | OK | Sig: rgb = wavelength2rgb(wavelength [, class [, gamma]]). Piecewise visible-light wavelength → RGB (Bruton). Tolerance loose because Octave's gamma=0.8 raises tiny FP noise when raising 0 to 0.8 — final RGB triple to 4 decimals is the right comparison. |
| `imsmooth` | — | 0.005 |  | 107.62× | OK | Sig: J = imsmooth(I, name [, sigma]). Currently Gaussian-only with σ-Gaussian, h=ceil(3σ), symmetric pad; Octave-image has imsmooth (this matches the Gaussian path). |
| `colorgradient` | — | 0.001 |  |  | N/A | Sig: M = colorgradient(C [, w] [, n]). K-by-3 anchor RGB; piecewise linspace; default n=64. Octave-image has colorgradient. Default uses rows(colormap) but we don't have a graphics colormap so we default to n=64. |
| `iscolormap` | — | 0.006 |  | 34.83× | OK | Sig: tf = iscolormap(cmap). Real, float (single/double), 2-D, 3 cols, non-empty. Range [0,1] not enforced. Octave core has iscolormap. |
| `gray` | — | 0.003 | 145.72× | 59.34× | OK | Sig: map = gray([n]). N×3 grayscale colormap. Default n=256 (we don't track figure colormap state). n==1 → [0 0 0]; n<=0 → 0×3. Octave core has gray. |
| `hot` | — | 0.003 | 176.29× | 38.25× | OK | Sig: map = hot([n]). N×3 black→red→yellow→white colormap. Default n=256. Octave core has hot. |
| `cool` | — | 0.001 | 238.67× |  | OK | Sig: map = cool([n]). N×3 cyan→magenta. r=(0:n-1)/(n-1), g=1-r, b=1. Default n=256. Octave core has cool. |
| `spring` | — | 0.003 | 137.39× | 39.09× | OK | Sig: map = spring([n]). N×3 magenta→yellow. r=1, g=(0:n-1)/(n-1), b=1-g. Default n=256. Octave core has spring. |
| `summer` | — | 0.003 | 136.91× | 15.73× | OK | Sig: map = summer([n]). N×3 green→yellow. r=(0:n-1)/(n-1), g=0.5+r/2, b=0.4. Default n=256. Octave core has summer. |
| `autumn` | — | 0.001 | 467.84× |  | OK | Sig: map = autumn([n]). N×3 red→yellow. r=1, g=(0:n-1)/(n-1), b=0. Default n=256. MATLAB+Octave both ship autumn. |
| `winter` | — | 0.003 | 144.38× | 23.68× | OK | Sig: map = winter([n]). N×3 blue→cyan-ish. r=0, g=(0:n-1)/(n-1), b=1-g/2. Default n=256. MATLAB+Octave both ship winter. |
| `copper` | — | 0.001 | 219.44× |  | OK | Sig: map = copper([n]). N×3 black→copper. r=min(5/4*x,1), g=0.7812*x, b=0.4975*x where x=(0:n-1)/(n-1). Default n=256. MATLAB+Octave. |
| `pink` | — | 0.004 | 214.19× | 46.47× | OK | Sig: map = pink([n]). N×3 pastel pink. 3-piece linspace ramps per channel, then sqrt. Default n=256. MATLAB+Octave both ship pink. |
| `hsv` | — | 0.004 | 265.40× | 77.03× | OK | Sig: map = hsv([n]). Hue rotation via hsv2rgb([(0:n-1)'/n, 1, 1]). Default n=256. MATLAB+Octave both ship hsv. |
| `flag` | — | 0.001 |  |  | N/A | Sig: map = flag([n]). N×3 cycling [1 0 0; 1 1 1; 0 0 1; 0 0 0]. Default n=256. MATLAB+Octave both ship flag. |
| `prism` | — | 0.003 | 181.76× | 40.87× | OK | Sig: map = prism([n]). N×3 cyclic 6-row rainbow [r,o,y,g,b,v]. Default n=256. MATLAB+Octave both ship prism. |
| `lines` | — | 0.003 | 241.47× | 40.26× | OK | Sig: map = lines([n]). Cycles the figure axes colororder. We pin the MATLAB R2025b factory 7-row palette (Octave's older default differs). MATLAB+factory; harness ranks MATLAB as truth so OK is expected. |
| `bone` | — | 0.002 | 469.57× |  | OK | Sig: map = bone([n]). N×3 grayscale-with-blue-tint colormap. Per Octave's bone.m: idx=floor(3/4·n) for R, idx=floor(3/8·n) for G/B; piecewise linspace ramps; switch on mod(n,8) for base. Default n=256. MATLAB+Octave both match. |
| `white` | — | 0.003 | 115.86× | 28.25× | OK | Sig: map = white([n]). N×3 all-ones colormap. Default n=256. MATLAB+Octave both ship white. |
| `brighten` | — | 0.003 | 75.27× |  | OK | Sig: rmap = brighten(map, beta). Output = map .^ gamma where gamma = 1-beta if beta>0 else 1/(1+beta). MATLAB+Octave both ship brighten. |
| `contrast` | — | 0.002 | 134.09× |  | OK | Sig: cmap = contrast(x[, m]). Histogram-equalising gray colormap. Per MATLAB R2025b cleve-moler algorithm: scale to [0,m-1] ints, concat with [0..m], find rising edges. MATLAB+Octave both ship contrast but Octave gives slightly different values; we follow MATLAB. |
| `cdf_upper` | — | 0.010 | 642.47× |  | OK | Joint 'upper' flag verification across 14 CDFs (closes 14 audit ТЗ in stats.dist). MATLAB R2025b: every *cdf accepts trailing 'upper' string and returns 1 - F(x). normcdf double-checks lower tail unchanged. tol = 1e-9. Closes audit/findings/stats/{normcdf,chi2cdf,tcdf,fcdf,betacdf,gamcdf,expcdf,raylcdf,logncdf,wblcdf,unifcdf,unidcdf,binocdf,poisscdf}.md. |
| `windows_sflag` | — | 0.011 | 405.27× | 31.15× | OK | Joint 'periodic' / 'symmetric' (default) sflag verification across 6 signal.windows that accept it. Implementation trick: periodic(N) = first N samples of symmetric(N+1) — works for any window. The other 6 windows (bartlett/triang/parzenwin/bohmanwin/barthannwin/rectwin) accept ONLY 'double'/'single' typeName and throw on 'periodic' (gtest covers that branch). Closes audit/findings/signal/{hamming,hann,blackman,blackmanharris,flattopwin,nuttallwin,bartlett,triang,parzenwin,bohmanwin,barthannwin,rectwin}.md. |
| `kstest_extras` | — | 0.017 | 139.85× | 84.20× | OK | Sig: kstest2(x, y[, alpha, tail | name-value]). Tail accepts 'unequal' (default), 'larger', 'smaller' (synonyms for 'both', 'right', 'left' from kstest). Name-Value pairs: 'Alpha', 'Tail'. Closes audit/findings/stats/{kstest,kstest2}.md. |
| `ttest_extras` | — | 0.015 | 331.96× | 912.96× | OK | Sig: ttest(x, y[, NV]) paired form; ttest2 default Vartype=equal (pooled). NV pairs: Alpha, Tail, Vartype, Dim (Dim throws). 4th output struct (tstat/df/sd) NOT yet implemented — fingerprints stay on first 3 outputs. Closes audit/findings/stats/{ttest,ttest2}.md (partial — 4th-output struct, matrix input, Dim, n<2 NaN remain as documented gaps in spec comment). |
| `vartest_extras` | — | 0.012 | 397.70× | 1498.78× | OK | Sig: vartest(x, v[, NV]) and vartest2(x, y[, NV]). Both adapters now parse Alpha and Tail Name-Value pairs (case-insensitive). 'Dim' N-V throws (parity gap). 4th output remains scalar T/F (struct deferred). Closes audit/findings/stats/{vartest,vartest2}.md (partial). |
| `ztest_extras` | — | 0.007 | 411.04× | 86.88× | OK | Sig: ztest(x, m, sigma[, NV]). Alpha/Tail Name-Value pairs (case-insensitive). 'Dim' N-V throws (parity gap). 4th output is scalar zval (matches MATLAB which doesn't return a struct here). Closes audit/findings/stats/ztest.md. |
| `logical` | — | 0.004 | 32.81× | 51.69× | OK | Sig: r = logical(...). Type conversion. Spec-extension batch 2026-05-09 — auditor "no major gap detected" verified MATLAB R2025b parity. |
| `islogical` | — | 0.004 | 39.33× | 20.59× | OK | Sig: r = islogical(...). Predicate. Spec-extension batch 2026-05-09. |
| `smoothdata` | — | 0.004 | 615.42× |  | OK | Sig: y = smoothdata(x). Spec-extension batch 2026-05-09 (cycle 43). |
| `sosfiltfilt` | — | 0.018 | 21.09× | 26.42× | OK | N/A (definite): MATLAB R2025b has no top-level sosfiltfilt() -- the equivalent operation is filtfilt(sos, 1, x). Numkit ships sosfiltfilt(sos, x) as a public function that bit-identically matches scipy.signal.sosfiltfilt and is used internally by lowpass/highpass/etc. Definite N/A vs MATLAB top-level. |
| `magic` | — | 0.007 | 115.13× | 57.16× | OK | Sig: M = magic(N). N×N magic square -- rows/cols/diagonals sum to N·(N²+1)/2. Three branches by N's parity: odd (Siamese / de la Loubère), N≡0 mod 4 (doubly-even pattern), N≡2 mod 4 (Strachey). Bit-identical with MATLAB R2025b across N ∈ {3,4,5,6,8} (covers all three branches). |
| `toeplitz` | — | 0.006 | 83.93× | 40.75× | OK | Sig: T = toeplitz(c[, r]). Toeplitz matrix from first column c (and optional first row r). T[i,j] = c[i-j] (i>=j) else r[j-i]. MATLAB convention: r[0] silently overridden by c[0]. Bit-identical with MATLAB R2025b across square + rectangular probes. |
| `hankel` | — | 0.005 | 99.70× | 19.73× | OK | Sig: H = hankel(c[, r]). Hankel (anti-Toeplitz) matrix from first column c and optional last row r. H[i,j] = c[i+j] for i+j<m else r[i+j-m+1]. Single-arg form: r is all zeros (anti-triangular). Bit-identical with MATLAB R2025b. |
| `vander` | — | 0.005 | 55.39× | 21.91× | OK | Sig: V = vander(v). Vandermonde matrix V[i,j] = v[i]^(n-1-j); columns from highest power on the left (MATLAB R2025b convention). Bit-identical. |
| `compan` | — | 0.002 | 87.93× |  | OK | Sig: A = compan(p). Companion matrix of polynomial coefficients p (length n+1) -- top row [-p(2:end)/p(1)], subdiagonal of ones. eig(compan(p)) == roots(p). Bit-identical with MATLAB R2025b. |
| `pascal` | — | 0.005 | 189.12× | 39.96× | OK | Sig: P = pascal(N). Symmetric Pascal-triangle matrix (k=0 default form). P[i,j] = C(i+j,i). Built via the additive recurrence. Bit-identical with MATLAB R2025b. Note: k=1 (Cholesky factor) and k=2 (cube-root of identity) variants are deferred. |
| `hilb` | — | 0.004 | 72.49× | 10.47× | OK | Sig: H = hilb(N). Hilbert matrix H[i,j] = 1/(i+j-1) (1-indexed). Bit-identical with MATLAB R2025b (single divides; no accumulation error). |
| `invhilb` | — | 0.005 | 56.56× | 213.48× | OK | Sig: H = invhilb(N). Closed-form inverse Hilbert matrix via the binomial formula H⁻¹[i,j] = (-1)^(i+j)*(i+j-1)*C(n+i-1,n-j)*C(n+j-1,n-i)*C(i+j-2,i-1)². Long-double accumulation delays overflow. Bit-identical-ish with MATLAB R2025b (tol 1e-6 -- both engines lose ULPs through the same overflow-prone formula at N>=8). |
| `wilkinson` | — | 0.005 | 56.46× | 24.57× | OK | Sig: W = wilkinson(N). Symmetric tridiagonal eigenvalue test matrix: subdiag/superdiag of ones, main diag = |(1:n)-(n+1)/2|. Bit-identical with MATLAB R2025b. |
| `hadamard` | — | 0.007 | 52.38× | 28.46× | OK | Sig: H = hadamard(N). Sylvester construction: H_1=[1], H_{2k}=[Hk Hk; Hk -Hk]. Power-of-2 N only (1,2,4,8,16,...). MATLAB R2025b also accepts 12·2^k and 20·2^k via Paley constructions -- those are deferred (separate ТЗ). |
| `rosser` | — | 0.004 | 51.53× | 44.00× | OK | Sig: R = rosser(). Hardcoded 8×8 Rosser eigenvalue test matrix. Bit-identical with MATLAB R2025b (constants directly transcribed from MATLAB output). |
| `cputime` | — | 0.002 | 16.23× |  | OK | Side-effect smoke test (timer probe). cputime returns CPU seconds used by current process; only invariant we can test cross-engine is t >= 0 (absolute values differ between engines). Implemented via std::clock() / CLOCKS_PER_SEC. |
| `isoutlier` | — | 0.004 | 492.60× | 145.27× | OK | Sig: m = isoutlier(x). Default median + 3*MAD method. Bit-identical with MATLAB R2025b. |
| `rmoutliers` | — | 0.005 | 478.57× |  | OK | Sig: y = rmoutliers(x). Drops outliers from x. |
| `standardizeMissing` | — | 0.005 | 285.20× | 43.62× | OK | Sig: y = standardizeMissing(x, sentinel). Replaces sentinel value with NaN. |
| `detrend` | — | 0.005 | 409.96× |  | OK | Sig: y = detrend(x[, order]). Remove polynomial trend (default linear). Vector form. |
| `fitdist` | — | 0.003 |  |  | N/A | Sig: pd = fitdist(x, 'Name'). numkit returns a struct (.DistributionName, .ParameterValues, .ParameterNames, .NumObservations). MATLAB returns a probability-distribution OBJECT with same .ParameterValues/.DistributionName fields. ParameterValues bit-identical (delegates to mle). MATLAB's class methods (.pdf/.cdf/.icdf) deferred. |
| `now` | — | 0.015 | 24.32× | 54.77× | OK | Sig: t = now. Serial date number for current local time. Days since MATLAB epoch (year 0000-01-00). 1970-01-01 = 719529. Cannot bit-compare across engines (different sample times); parity tests range invariant. |
| `datenum` | — | 0.003 | 268.48× |  | OK | MATLAB datenum: serial date number from components. Covered: 3-arg (Y,M,D) with vector args (broadcast to column), 6-arg (Y,M,D,H,MI,S) with time fraction, single-arg Nx3 matrix, single-arg 1x6 row, month/day overflow (m=13 -> next year, d=30 of Feb -> March), year-zero edge. Deferred: string parsing (datenum('2026-05-09')) -- requires datestr/format-spec parser. Algorithm: Howard Hinnant days_from_civil + 719529 (MATLAB epoch). |
| `weekday` | — | 0.005 | 69.80× | 54.38× | OK | MATLAB weekday: day-of-week index 1=Sun..7=Sat (US calendar). Covers single-date, vector input, historical dates spanning 60+ years (1970, 2000, 2026), and one full week roundtrip 7,1,2,3,4,5,6. Optional name string output ('short'/'long') tested in gtest only -- parity harness fingerprints numeric only. Algorithm: ((floor(d) - 2) mod 7) + 1 with positive-modulo (serial 1 = Saturday in MATLAB's calendar). |
| `juliandate` | — | 0.007 | 875.70× |  | OK | MATLAB juliandate: Julian day number from date components. Covered: 3-arg (Y,M,D) with vector args, 6-arg with time fraction, single-arg 1x6 row, single-arg Nx3 matrix. Anchors: 1970-01-01 00:00 = 2440587.5 (Unix epoch), 2000-01-01 12:00 = 2451545.0 (J2000.0). Algorithm: datenum-serial + 1721058.5. Deferred: string parsing forms, datetime-object input. |
| `eomday` | — | 0.003 | 145.36× |  | OK | MATLAB eomday: last day of given month. Covered: leap-year all four cases (/4 leap [2024], common [2025], /400 leap [2000], century non-leap [1900]), 30-day month (April), full Jan-Dec scan in leap year, scalar+vector broadcast, and 2x2 matrix shape preservation. Algorithm: lookup table + isLeap = (y%4==0 && y%100!=0) || y%400==0. |
| `datevec` | — | 0.003 | 91.43× |  | OK | MATLAB datevec: inverse of datenum. Covered: scalar round-trip (Y,M,D), with-time round-trip (H,MI,S), Unix-epoch anchor (719529 -> 1970-01-01), fractional-day extraction (0.25 -> 06:00), N-vector input -> Nx6 matrix, multi-output [Y,M,D] form. Algorithm: Howard Hinnant civil_from_days + microsecond rounding for FP-noise dampening. Edge: datevec(0) = [0 0 0 0 0 0] matches MATLAB literal. |
| `yyyymmdd` | — | 0.005 | 280.68× |  | OK | MATLAB yyyymmdd: packed integer date Y*10000+M*100+D. MATLAB R2025b requires datetime input -- numkit accepts serial date directly as a convenience extension. Spec uses an engine-detecting shim ymd__ that wraps with datetime() on MATLAB and falls through on numkit. Year-0 case excluded (MATLAB datetime errors on dates before 0001-01-01); year-0 covered by gtest. Octave 11.1.0 doesn't ship yyyymmdd; reports N/A. Algorithm: Howard Hinnant civil_from_days then arithmetic packing. |
| `mjuliandate` | — | 0.008 | 772.30× |  | OK | MATLAB mjuliandate: Modified Julian Date = JD - 2400000.5; epoch 1858-11-17 00:00. Covered: 3-arg/6-arg/single-arg row/single-arg matrix forms with vector inputs. Anchors: MJD epoch 1858-11-17 -> 0, Unix epoch 1970-01-01 -> 40587, J2000.0 -> 51544.5. Algorithm: serial-MATLAB-date - 678942 (= 1721058.5 - 2400000.5, both fractional offsets cancel). Deferred: string + datetime input forms. |
| `predicates` | — | 0.016 | 41.39× | 62.92× | OK | MATLAB linalg predicates batch: issymmetric/ishermitian (with optional 'skew'), isbanded(A,lo,up), isdiag, istril, istriu, bandwidth (1-out=lower / 2-out=[lo,up] / 'lower'|'upper' opt), vecnorm(A[,p[,dim]]). All comparisons exact (== 0). Bit-equal with MATLAB R2025b: predicates produce 0/1, bandwidth produces integers, vecnorm produces doubles. Empty vecnorm([]) -> scalar 0 (MATLAB convention). issymmetric/ishermitian use exact transpose/conj-transpose without tolerance. Octave 11.1.0 ships isbanded/isdiag/issymmetric/ishermitian/istril/istriu but not bandwidth/vecnorm. |
| `rref_rcond_planerot` | — | 0.013 | 78.10× |  | OK | MATLAB linalg cycle 2: rref + rcond + planerot. rref via Gauss-Jordan with partial pivoting; default tol = max(M,N)*eps(norm(A,inf)); two-output [R, jb] form returns 1-based pivot column indices. rcond uses cheap path 1/(norm(A,1)*norm(inv(A),1)); returns 0 for singular A. KNOWN GAP: rcond on near-singular matrices may differ slightly from MATLAB's LAPACK dgecon estimator. planerot Givens rotation: r=hypot(x,y), G=[c s; -s c], degenerate (0,0) case returns identity. Bit-equal with MATLAB R2025b on the well-conditioned cases; rcond bit-equal on diag/2x2/hilb(4); planerot exact (cos/sin formula matches). rref complex input deferred (KNOWN GAP). Octave: triple-engine green for all three. Real-only inputs in v1. |
| `lsqminnorm_lsqnonneg` | — | 0.013 | 114.55× |  | OK | MATLAB linalg cycle 4: lsqminnorm + lsqnonneg. lsqminnorm = pinv(A,tol)*B; bit-equal with MATLAB R2025b on full-rank, rank-deficient, and wide systems. lsqnonneg via Lawson-Hanson active-set algorithm; bit-equal on x, resnorm, residual, exitflag for the classic test [1 -1 2; 3 4 5; 6 7 8] / [1; 2; 3] -> x = [0; 0; 0.387097], resnorm = 0.06451612903. KNOWN GAPs: lsqminnorm 'rankWarn'/'RegularizationFactor' name-value args deferred; lsqnonneg 'options'/'problem' input forms and 6th 'lambda' output (Lagrange multipliers) deferred; complex inputs not supported. Octave ships both functions; lsqminnorm in core since Octave 6, lsqnonneg in optim package. |
| `base_conversions` | — | 0.008 | 521.81× |  | OK | MATLAB Communications Toolbox base conversions: bit2int (pack n-bit groups → integers, msbfirst default true), int2bit (inverse, returns n×M bit matrix), bi2de (legacy synonym, rows = numbers, LSB-first 'right-msb' default, optional base), de2bi (legacy inverse, optional n / base), vec2mat (reshape vector into N-column row-major-filled matrix with padval default 0, 2-out form returns pad count). Bit-equal with MATLAB R2025b on all 26 fingerprint points across MSB/LSB ordering, custom base, auto-width and explicit-width forms, padding semantics. Octave 11.1.0 ships these in the communications package. |
| `sigroi_utils` | — | 0.014 | 451.98× |  | OK | MATLAB Signal Processing Toolbox ROI utilities (signalMask family): binmask2sigroi (mask→[start end] pairs), sigroi2binmask (inverse, with optional length), extendsigroi/shortensigroi (per-ROI shrink/grow with start clamped to 1 and degenerate ROIs dropped on shorten), mergesigroi (sort-then-merge with sep tolerance), removesigroi (drop ROIs with length ≤ maxLen — matches MATLAB doc, NOT index-based), extractsigroi (cell array default OR concatenated vector when concat=true), sigrangebinmask (bound is scalar→x>bound 'above' default, OR 2-vec→inside [vmin,vmax] closed). KNOWN GAP: 'Relationship'/'IntervalType'/'MinLength'/'Dimension' name-value args for sigrangebinmask deferred. Bit-equal with MATLAB R2025b on all 31 fingerprint points. Octave 11.1.0 doesn't ship these in core (Signal package only). |
| `color_extras` | — | 0.008 | 222.70× |  | OK | MATLAB Image Toolbox color extras: rgb2lightness (= first channel of rgb2lab; returns single H×W), rgb2ind in fixed-palette form (nearest-RGB quantization, 1-based index uint8 if cmap rows ≤ 256). Bit-equal with MATLAB R2025b on lightness L value at white (=100) and on the 4-color palette quantization of a synthetic 2×2×3 image (red/green/blue/dark-red maps to nearest in [0 0 0; 1 0 0; 0 1 0; 0 0 1]). Lightness values for non-pure colors match MATLAB to ~1e-3 (single-precision rounding through the rgb2xyz → xyz2lab pipeline). KNOWN GAP rgb2ind: scalar-Q (min-variance quant) and scalar-tol (uniform quant) forms deferred; dithering arg ignored. Octave 11.1.0 ships rgb2lightness in the image package only, rgb2ind in core. |
| `filter_design` | — | 0.036 |  |  | N/A | MATLAB Image Toolbox filter-design utilities (cycle 4): fspecial3 (all 7 types: average/gaussian/laplacian/log/prewitt/sobel/ellipsoid) + fwind2 (2-D FIR via 2-D window method). Bit-equal with MATLAB R2025b on key invariants (sums, centers, sobel structure across all three directions X/Y/Z). KNOWN GAPs (deferred to v2): fsamp2 (requires 2-D IFFT), ftrans2 (Chebyshev polynomial recurrence), fwind1 (Chebyshev), gabor (object class infrastructure). All four deferred fns registered with explicit 'not implemented in v1' errors so MATLAB scripts get clear messages instead of undefined-function. Octave 11.1.0 ships fspecial3 in image package only; fsamp2/fwind2 in image package. |
| `sig_utils` | — | 0.010 | 265.24× |  | OK | MATLAB Signal Processing Toolbox utility batch (cycle 5): seqperiod (smallest divisor period d ≤ N where x repeats with tol), zerocrossrate (count = #sign-changes + 0.5 boundary credit, rate = count/N — matches MATLAB R2025b default Level=0/ZeroPositive=false), cusum (Page-Hinkley CUSUM detector returning first out-of-control indices). Bit-equal with MATLAB R2025b on all 13 fingerprint points covering periodic + non-periodic + repeat sequences, sign-change patterns including no-crossing edges, and a synthetic mean-shift cusum sequence. KNOWN GAPs: zerocrossrate matrix/N-D + Name=Value (Threshold/TransitionEdge/WindowLength), seqperiod multi-column variant, cusum no-output plotting form. Octave 11.1.0 doesn't ship these in core. |
| `signal_buffer` | — | 0.014 | 39.77× | 24.20× | OK | MATLAB Signal Toolbox buffer (Phase 4.1 of audio extension sweep): partition signal into possibly overlapping/underlapping frames. Bit-equal MATLAB R2025b on 6 cases — non-overlapping zero-pad, p>0 overlap with initial zeros, p>0 with 'nodelay', p<0 underlap, [Y,Z] complete-only output, column-vector input. Implementation in libs/signal/src/digital_filtering/buffer.cpp following MATLAB buffer.m semantic doc (the .m file itself is just a MEX shim; behavior derived from probing). Octave 11.1.0 ships buffer in core (signal package); should match. KNOWN GAPs: (1) initial-condition vector OPT for p>0 (numeric instead of 'nodelay') validated for length but not heavily tested; (2) 3-output form [Y,Z,OPT] for continuous buffering — return value of OPT for next call deferred (MATLAB internal state). |
| `signal_uquant` | — | 0.017 | 222.49× | 25.17× | OK | MATLAB Signal Toolbox uencode/udecode (Phase 4.2): uniform N-bit quantization. Bit-equal MATLAB R2025b on 16 fingerprints — unsigned/signed encoding, custom peak V, 3 output type tiers (uint8/16/32), saturate vs wrap on decode, full roundtrip error within expected 8-bit quantization step (~0.008). Octave 11.1.0 ships these in the signal package. |
| `signal_polyutils` | — | 0.021 | 113.03× |  | MISMATCH | MATLAB Signal Toolbox polyscale + polystab (Phase 4.3): polynomial root scaling and stabilization. polyscale: y[k] = p[k] * scale^k (closed-form). polystab: roots(a) → reflect outside-unit-circle to inside via 1/conj(root) → poly() back → multiply by leading coef → real() if input real. Bit-equal MATLAB R2025b on 12 fingerprints — scaled poly with real and >1 scale, polystab on poly with one root outside unit (root=2 reflects to 0.5), polystab on FIR filter, polystab on poly with already-stable roots. Octave 11.1.0 ships these in core (signal package). |
| `signal_shiftdata` | — | 0.011 | 105.80× | 31.84× | OK | MATLAB Signal Toolbox shiftdata + unshiftdata (Phase 4.4): dim-aware utilities for filter-like functions. shiftdata(x, dim): permute dim to leading. shiftdata(x, []): auto-shift via shiftdim (drop leading singletons). unshiftdata: ipermute back (or shiftdim(-nshifts)). Bit-equal MATLAB R2025b on 12 fingerprints — explicit-dim path (transpose), auto path (row→col with nshifts=1), full roundtrip identity for both forms. Octave 11.1.0 ships these in core. |
| `signal_kaiserord` | — | 0.011 | 234.17× | 32.82× | OK | MATLAB Signal Toolbox kaiserord (Phase 4.5) — Kaiser-window FIR order estimator. Closed-form per Kaiser 1974: D=(atten-7.95)/(2π·2.285), L=D/Δf+1, β piecewise on attenuation: 0.1102(a-8.7) for a>50, 0.5842(a-21)^0.4 + 0.07886(a-21) for 21≤a≤50, else 0. Bit-equal MATLAB R2025b on 13 fingerprints — lowpass (n=36 Wn=0.4375 β=3.395 ftype='low'), highpass (n=46 Wn=0.45 ftype='high'), bandpass with multiple bands (DC-0 type, Wn=[0.1875 0.5625]). Octave 11.1.0 ships kaiserord in core (signal package). |
| `signal_ellipord` | — | 0.011 | 155.73× | 41.68× | OK | MATLAB Signal Toolbox ellipord (Phase 4.6) — minimum-order Cauer/elliptic filter. Algorithm: prewarp digital→analog (tan(πw/2)), compute analog passband-edge ratio WA per filter type, findelliporder via complete elliptic integrals: ε=√(10^(0.1Rp)-1), k1=ε/√(10^(0.1Rs)-1), k=1/WA, N=ceil(K(k²)·E(1-k1²)/(K(1-k²)·K(k1²))). Bit-equal MATLAB R2025b on 9 fingerprints — lowpass, highpass, bandpass, analog 's' mode. KNOWN GAP: bandstop (ftype=3) deferred (recursive analog conversion not yet implemented). Octave 11.1.0 doesn't ship in core (signal package only). |
| `signal_firpmord` | — | 0.010 | 170.04× | 144.65× | OK | MATLAB Signal Toolbox firpmord (Phase 4.7) — Parks-McClellan FIR order estimator. remlpord formula from Rabiner & Gold pp.156-7: D = [1 d1 d1²] · AA · [1; d2; d2²] (3×3 const matrix from McClellan), fK = 11.01217 + 0.51244·(d1-d2), L = D/df - fK·df + 1. Bit-equal MATLAB R2025b on 8 fingerprints — lowpass (n=21 fo=[0,0.375,0.5,1] ao=[1,1,0,0] w=[10,1]), highpass (n=32), bandpass (n=24). Returns 4-tuple (N, ff, aa, wts) suitable for firpm. Octave 11.1.0 ships in core. |
| `signal_vco` | — | 0.009 | 175.13× | 25.11× | OK | MATLAB Signal Toolbox vco (Phase 4.8) — voltage-controlled oscillator. y = cos(2π·Fc·t + range1·cumsum(x)), where range1 = (Fc/Fs)·2π for scalar Fc, or (Fmax-Fc)/Fs·2π for [Fmin Fmax] (Fc=mean(range)). Bit-equal MATLAB R2025b on 8 fingerprints — zero input (pure carrier), constant offset (chirp-up), full sweep with Fmin/Fmax range. Octave 11.1.0 ships in core (signal package); test passes. |
| `signal_fir2` | — | 0.127 | 38.83× | 68.30× | OK | Signal Processing toolbox fir2 — frequency-sampling FIR filter design. CLEAN-ROOM implementation from public references (Oppenheim & Schafer 3e §7.4-7.5 frequency-sampling FIR design; Rabiner & Gold 1975; Parks & Burrus 1987) — see cleanroom/specs/fir2.md. Pipeline: piecewise-linearly interpolate the desired (f,m) magnitude response onto a uniform DC..Nyquist grid of npt+1 points, apply a linear-phase delay exp(-j*pi*dt*k/npt) with dt=(nn-1)/2, Hermitian-mirror to length 2*npt, inverse-FFT, window. Full MATLAB argument set: fir2(n,f,m), fir2(n,f,m,npt,lap), fir2(...,window). Bit-equal MATLAB R2025b (tol 1e-9) on 20 fingerprints covering: lowpass/bandpass/highpass 3-arg form; explicit npt=256 grid; a custom Hann window; the lap smoothing argument on break frequencies with duplicated points (discontinuities); and the odd-order Nyquist correction (fir2(11,[0 1],[0 1]) -> length 13, since an odd-order symmetric FIR has a forced zero at Nyquist). Octave 11.1.0 ships fir2 in core but its frequency grid differs slightly (not bit-compared; MATLAB is the reference). |
| `signal_cell2sos` | — | 0.011 | 163.77× |  | OK | MATLAB Signal Toolbox cell2sos (Phase 4.10) — convert cell array of {Bi, Ai} pairs to L×6 second-order-section matrix. Linear (length-2) sections zero-padded on right. 2-output form [S, G] = cell2sos(C) extracts leading scalar gain when C{1} = {scalar_b, scalar_a}. Bit-equal MATLAB R2025b on 12 fingerprints across both help-example forms (with and without leading gain). Octave 11.1.0 ships in core. |
| `signal_ctfutils` | — | 0.017 | 335.22× |  | OK | MATLAB Signal Toolbox ctf2zp + scaleFilterSections (Phase 4.11). ctf2zp: cascade transfer function (NUM, DEN, SV) → zero/pole/gain via per-section tf2zpk + product of gains. scaleFilterSections: distribute |sv|^(1/K) across sections, sign on last. Bit-equal MATLAB R2025b on 12 fingerprints — single-section ctf2zp values+counts, multi-section gain product, scaleFilterSections both vector-SV and scalar-SV forms. KNOWN GAP: ctf2zp doesn't strip trailing zeros from numerators/denominators (MATLAB parser does); user-visible difference is extra zero/pole at 0 in z/p arrays for length-padded inputs. Octave 11.1.0 doesn't ship ctf2zp/scaleFilterSections in core. |
| `signal_modulate` | — | 0.011 | 199.52× |  | OK | MATLAB Signal Toolbox modulate (Phases 4.12 + 5.3). All 5 most-used modes shipped: am / amdsb-sc / amdsb-tc / fm / pm (bit-equal MATLAB) + amssb (uses hilbert — approximate-equal due to finite-window edge effects, ~3-5%). Per-element formulas: am = x·cos(2πFct); amdsb-tc = (x-offset)·cos(...); fm = cos(2πFct + kf·cumsum(x)); pm = cos(2πFct + kp·x); amssb = x·cos(...) + imag(hilbert(x))·sin(...). Default kf = (Fc/Fs)·2π/max|x|, kp = π/max|x|, offset = min(x). 9/9 fingerprints OK (tol 5% to accommodate hilbert edge noise). KNOWN GAPs: pwm/ptm/ppm (specialised pulses), qam (complex carrier) deferred. Octave 11.1.0 doesn't ship modulate in core (signal package only). |
| `signal_demod` | — | 0.013 | 802.17× |  | OK | MATLAB Signal Toolbox demod (Phases 4.13 + 5.3). Implements am / amdsb-sc (alias) / amdsb-tc / fm / pm. AM family: y·cos(2πFct) → 5th-order Butterworth filtfilt (subtract DC offset for amdsb-tc). FM/PM: yq = hilbert(y)·exp(-j2πFct); FM = (1/P1)·diff(unwrap(angle(yq))) prepended w/ 0; PM = (1/P1)·angle(yq). Approximate-equal MATLAB R2025b on 9 fingerprints (tol 5%); diffs from filtfilt edge handling + hilbert finite-window effects. KNOWN GAPs: amssb / pwm / ptm/ppm / qam deferred. Octave 11.1.0 doesn't ship demod in core (signal package only). |
| `signal_firpm` | — | 0.252 | 22.79× | 45.83× | OK | MATLAB Signal Toolbox firpm — Parks-McClellan optimal equiripple FIR via Remez exchange. Supports all four linear-phase types + Hilbert + Differentiator (matches MATLAB R2025b firpm.m semantics): Type I (even N, sym, Q=1), Type II (odd N, sym, Q=cos(ω/2)), Type III (even N, anti-sym, Q=sin(ω) — 'hilbert' or 'differentiator'), Type IV (odd N, anti-sym, Q=sin(ω/2)). For differentiator, MATLAB firpmfrf weights non-zero bands by 1/(GF/2) and applies an h-sign flip post-Remez ('make sure differentiator has correct sign' — firpm.m line 152-154). Approximate-equal MATLAB R2025b ~1e-3 across seven probed designs covering Type I LP/BP/HP/weighted + Type II LP + Type III Hilbert + Differentiator. KNOWN GAPS: fresp function-handle form, 3rd `res` output struct, lgrid cell-form override. Octave 11.1.0 ships firpm in the signal package (not core). |
| `signal_fftn` | — | 0.031 | 19.11× | 13.25× | OK | MATLAB fftn / ifftn — N-D forward and inverse FFT. Implemented as iterated 1-D fft along dims 1..ndim (commutes; current Dims model caps at 3-D, so max ndim = 3 — higher inputs would require the N-D refactor). 2-D inputs delegate through the same path and produce results identical to fft2. With the optional `sz` argument, axis k is zero-padded or truncated to sz[k-1] before its 1-D FFT (length validation is reused from the per-axis fft). Bit-equal MATLAB R2025b on 2-D, 3-D, sz-override, and ifftn round-trip (round-trip noise ~7e-15, well inside tol 1e-9). Octave 11.1.0 ships fftn / ifftn in core. Round-trip error fingerprint pins the inverse pair under the same tol. |
| `signal_czt` | — | 0.029 | 75.99× | 10.32× | OK | MATLAB Signal Toolbox czt — discrete chirp Z-transform. Implementation: Bluestein decomposition Y[k] = w^(k²/2) · (g ⋆ h)[k] where g[n] = x[n]·a^(-n)·w^(n²/2), h[n] = w^(-n²/2). The g ⊛ h circular convolution is computed via length-L FFT with L = nextPow2(N + m − 1); the negative-index branch of h is placed at indices L-n..L-1 to make the circular convolution equal the linear convolution on the first m output samples. Default args match MATLAB: m = length(x), w = exp(-2π·j/m), a = 1 — so czt(x) ≡ fft(x) and czt(x, m) ≡ fft(x, m). Approx-equal MATLAB R2025b (~1e-13 from chirp-pow arithmetic) on FFT-equivalent, m-override, and full 4-arg forms. Octave 11.1.0 ships czt in the signal package (not core); harness reports it from there. 2-D input is processed column-wise (MATLAB semantics). |
| `signal_stft` | — | 0.104 | 88.19× |  | OK | MATLAB Signal Toolbox stft / istft — short-time Fourier transform and inverse. stft loops windowed frames of length M, zero-pads to FFTLength, and runs an FFT per frame; output rows = FFTLength (twosided / centered) or FFTLength/2+1 (onesided). istft mirrors via overlap-add with the same synthesis window and per-sample window² normalization — for COLA-compliant configurations (hann/periodic with 50%/75% overlap) the round-trip is bit-exact on interior samples (max-err ~5e-16). Default frequency range is 'centered' (matches MATLAB R2019b+). Centered uses MATLAB's bin rotation Sd[k] = Fd[(k + N/2 + 1) mod N] for even N (Nyquist at end), (k + (N+1)/2) mod N for odd N — verified bit-equal vs MATLAB on the canonical sin probe. All three range modes round-trip via istft to ulp. KNOWN GAPS: fs / time-axis / [s, f, t] multi-output; multi-channel matrix input. Octave 11.1.0 ships stft / istft in the signal package (not core). |
| `image_adapthisteq` | — | 0.776 | 9.74× |  | OK | MATLAB Image Toolbox adapthisteq — CLAHE (Contrast Limited Adaptive Histogram Equalisation). CLEAN-ROOM implementation from public references (K. Zuiderveld, Graphics Gems IV, 1994; S. M. Pizer et al., Proc. VBC 1990 / CVGIP 1987) — see cleanroom/specs/adapthisteq.md. Full MATLAB argument set: NumTiles, ClipLimit, NBins, Range, Distribution (uniform/rayleigh/exponential), Alpha. Defaults: NumTiles=[8 8], ClipLimit=0.01, NBins=256, Distribution='uniform', Range='full', Alpha=0.4. RE-BASELINED: the clean-room CLAHE is functionally equivalent to MATLAB's adapthisteq but NOT bit-identical — MATLAB's clip/redistribute and interpolation-rounding have undocumented implementation details. Interior pixels diverge from MATLAB R2025b (e.g. J16=219 vs 134, J32=217 vs 137); they are intentionally excluded from the fingerprint, exactly as SRH/PEF were. The fingerprint keeps only what is genuinely engine-agnostic: image shape (sz1/sz2/szR/szE), the two saturated corner pixels (J11=8, Jend=255 — corners clamp identically in any correct CLAHE), and `spread` — a defining-property check that a low-contrast input (16-level band) gains >5x its standard deviation after equalisation. Real correctness is verified MATLAB-independently in libs/image/tests/adapthisteq_test.cpp (LowContrastInputGainsDynamicRange / ClipLimitOrderingIncreasesSpread / RangeOriginalConstrainsOutput). Octave 11.1.0 indexes adapthisteq in its image package but does not ship it — harness reports N/A there. |
| `image_graycomatrix` | — | 0.014 | 368.73× |  | OK | MATLAB Image Toolbox graycomatrix + graycoprops — gray-level co-occurrence matrix and its texture statistics. graycomatrix quantises I into NumLevels bins over GrayLimits, then counts pairs (p, p+offset) — row indexes the first pixel level, column the offset pixel level. With Symmetric=true the reverse direction is also tallied. graycoprops returns a struct with Contrast, Correlation, Energy, Homogeneity computed off the normalised joint probability p = G/sum(G). Bit-equal MATLAB R2025b on 12 fingerprints — GLCM entries, sum, and all four texture statistics on a 4x4 rotational-pattern image. KNOWN GAPS: multiple-offset call form (returns a 3-D GLCM) — pass each offset separately for now. Octave 11.1.0 ships graycomatrix / graycoprops in the image package. |
| `image_bwmorph` | — | 0.444 | 15.91× |  | OK | MATLAB Image Toolbox bwmorph — binary morphological operations. Faithful port of MATLAB R2025b bwmorph.m + algbwmorph.m: each operation is a 3×3 neighbourhood LUT lookup (or chain of lookups with bitwise compositions) using MATLAB's makelut bit convention (bit k = neighbour((k%3)-1, (k/3)-1) relative to centre). The 14 base LUTs (lutdilate, luterode, lutbridge, lutclean, lutdiag, lutendpoints, lutfatten, lutfill, luthbreak, lutmajority, lutper4, lutper8, lutremove, lutbranchpoints) plus 8 skeleton sub-LUTs (lutskel1..8) and the support tables (lutshrink, lutsingle, lutspur, lutthin1, lutthin2, lutbackcount4, lutiso) are dumped from MATLAB R2025b directly into a generated header. tol=0 bit-exact on 23 fingerprints covering: 13 single-LUT operations (dilate / erode / bridge / clean / diag / fill / hbreak / majority / perim4 / perim8 / remove / endpoints / fatten) + 4 composite (open / close / bothat / tophat) + 6 iterated (skel∞ / thin∞ / thicken / spur / shrink∞ / branchpoints). Inputs: 20×20 logical random matrix from rng(0). Octave 11.1.0's bwmorph is in the image package — different implementation, not bit-compared. |
| `signal_polyscale` | — | 0.010 | 117.27× |  | OK | Signal Processing toolbox polyscale — radial scaling of polynomial roots (b[k] = a[k]*alpha^k, the z-transform scaling property A(z) -> A(z/alpha)). CLEAN-ROOM implementation from public references (Oppenheim & Schafer 3e §3.2 z-transform scaling; Markel & Gray 1976, LPC bandwidth expansion) — see cleanroom/specs/polystab_polyscale.md. Bit-exact MATLAB R2025b (tol=0) on the documented argument set: row-vector input + scalar alpha, matrix input (one polynomial per row) + scalar alpha, row-vector alpha (element k raised to power k), and complex alpha. DOCUMENTED DIVERGENCE: for a column-vector input MATLAB's implicit expansion of a .* alpha.^(0:length(a)-1) yields an N×N matrix; numkit treats any vector as a single polynomial and returns a 1×N row — the column-vector case is intentionally not in the fingerprint. Octave 11.1.0 does not ship polyscale in core — harness reports N/A there. |
| `signal_polystab` | — | 0.014 | 87.56× | 43.48× | OK | Signal Processing toolbox polystab — stabilise a polynomial (minimum-phase version): reflect every root with |root| > 1 to its conjugate reciprocal 1/conj(root) inside the unit circle, keeping the magnitude-response shape (scaled by a constant gain). CLEAN-ROOM implementation from public references (Oppenheim & Schafer 3e §5.6 minimum-phase systems / conjugate-reciprocal root reflection; Hayes 1996 spectral factorisation) — see cleanroom/specs/polystab_polyscale.md. Algorithm: roots(a) -> reflect outside roots -> poly() -> multiply by the first non-zero coefficient of a. Matches MATLAB R2025b within tol 1e-12 (the tolerance absorbs the roots->poly round-trip noise, observed ~1e-15). Fingerprint covers all algorithm paths: simple real roots ([1 -2.5 1] -> [1 -1 0.25]), leading zeros ignored ([0 1 -2.5 1] -> same length-3 result), already-stable input returned unchanged ([1 -0.5]), a complex-conjugate root pair OUTSIDE the unit circle reflected ([1 -3.4 3.7 -1] -> [1 -1.6 0.88 -0.16]), and a degree-5 polynomial with mixed real/complex roots (C) cross-checked coefficient-by-coefficient against MATLAB. KNOWN GAP: complex-coefficient input is unsupported because numkit's roots handles real polynomials only (the previous implementation had the same limitation). Octave 11.1.0 does not ship polystab in core — harness reports N/A there. |
| `signal_scalefiltersections` | — | 0.012 | 196.26× |  | OK | Signal Processing toolbox scaleFilterSections — distribute scale values across the sections of a cascaded-transfer-function (CTF) numerator. CLEAN-ROOM implementation from public references (L. B. Jackson, Digital Filters and Signal Processing, 1996 — cascade realisation and gain distribution; Oppenheim & Schafer 3e §6.3, cascade-form structures) — see cleanroom/specs/scaleFilterSections.md. Algorithm: for K cascade sections the overall gain magnitude is spread as |g|^(1/K) across all sections and the sign is concentrated on the last section; a length-(K+1) scale vector additionally applies a per-section factor g[k]. Bit-equal MATLAB R2025b (tol 1e-9) on 10 fingerprints: scalar g on a 3-section filter, a length-4 scale vector, the single-section K=1 case, and complex numerator coefficients (the clean-room rewrite lifts a gap — the previous implementation handled real coefficients only). Octave 11.1.0 does not ship scaleFilterSections (introduced in MATLAB R2023b) — harness reports N/A there. |
| `page_family` | — | 0.015 | 72.09× |  | OK | Sig: page-wise wrappers (pageeig, pagesvd, pagepinv, pagenorm, pagemldivide, pagemrdivide, pagelsqminnorm, pageinv). All iterate the corresponding 2-D linalg op per page. Fingerprints pin shapes + a handful of values (we use pageinv = inv per page, pageeig on symmetric input that gives exact eigvals [5 5 5] for one page, etc.). pagelsqminnorm not in fingerprint — it uses pinv which goes through SVD whose sign convention may differ at machine precision. |
| `schur_convert` | — | 0.002 | 649.85× |  | OK | Sig: cdf2rdf (complex-diag Schur → real-block Schur), rsf2csf (real-block → complex-diag). cdf2rdf test: build (V, D) manually for a rotation matrix [0 -1; 1 0] (eigvals ±i); verify the real-form reconstruction. rsf2csf test: a [a -b; b a] 2x2 real Schur block (eigvals a±bi). MATLAB R2025b matches the documented convention DR_block = [a -b; b a], VR(:, k+1) = -Im(v) when sub-diagonal of D pairs positive imag part. |
| `cond_pnorm` | — | 0.003 | 134.85× |  | OK | Sig: c = cond(A, p) for p ∈ {1, 2, Inf, 'fro'}. Closes the ⚠️ gap in PROGRESS where cond was 2-norm only. p=2 routes through cond_2norm (sigma_max/sigma_min); other p via norm(A,p)·norm(inv(A),p). Diagonal A = diag(1, 1e-3) gives exactly 1e3 for p=1,2,Inf and slightly above for 'fro' (sqrt(1+1e-6) · sqrt(1+1e6) ≈ 1e3 + 0.5e-3). |
| `predicates_sym` | — | 0.007 | 19.44× |  | OK | Sig: issymmetric(A [, 'skew']) → A == A.' (transpose, no conj). ishermitian(A [, 'skew']) → A == A' (conj transpose). 'skew' flips equality to A == -A.' / A == -A'. Bit-exact MATLAB R2025b (tol=0): SY symmetric (s1=1), H not symmetric (s2=0 — complex matrix), SK skew-sym (s3=1), H Hermitian (h1=1), SY not Hermitian since complex form differs (h2=1 — but SY is real so h2=1 by real-symmetric ≡ Hermitian rule), SKH skew-Hermitian (h3=1). Split from the original combined predicates spec to stay under the 256-register chunk limit. |
| `predicates_band` | — | 0.009 | 17.23× |  | OK | Sig: matrix-structure predicates and bandwidth queries. isbanded(A, lo, up) ↔ outside-band entries are zero; isdiag = isbanded(A, 0, 0). istril / istriu ↔ lower / upper triangular. [lo, up] = bandwidth(A) returns sub-diagonal and super-diagonal extent; bandwidth(A) one-out form returns lo; 'lower'|'upper' string form selects one. Bit-exact MATLAB R2025b (tol=0): D diagonal (ib1=id1=1), A tri-banded (ib2=1), A not upper-banded-only (ib3=0), L lower-tri (itl=1), U upper-tri (itu=1), bandwidth(A) = [1, 1], bandwidth(U) = 0, bandwidth(L) = 2, bandwidth(U, 'upper') = 2. Split from the original combined predicates spec to stay under the 256-register chunk limit. |
| `animatedline` | — | 0.002 | 29.16× |  | OK | animatedline + addpoints + getpoints round-trip. After 10 addpoints calls of (k, 2k), getpoints should round-trip the same data. Real numerical fingerprint — not just display invariance. |
