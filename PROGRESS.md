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
| `ans` | ✅ |  |  |  |  | implicit-assigned unsuppressed result |
| `clc` | ✅ |  |  |  |  |  |
| `commandhistory` | ❌ |  |  |  |  | IDE-only |
| `commandwindow` | ❌ |  |  |  |  | IDE-only |
| `diary` | ❌ |  |  |  |  | session log |
| `format` | ✅ |  |  |  |  | output format (no-op stub) |
| `home` | ✅ |  |  |  |  | terminal home |
| `iskeyword` | ✅ | 0.000 | 5.37× | 6.40× | OK | Sig: TF = iskeyword(NAME). Returns scalar logical. 100k iters. |
| `more` | ❌ |  |  |  |  | pager |

### Matrices and Arrays

**Namespace:** builtin — 53 ✅ + 1 ⚠️ / 55 = 98%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `blkdiag` | ✅ | 0.081 | 1.19× | 1.88× | OK | Sig: D = blkdiag(A,B,C). 50/80/60 deterministic mats. 100 iters. Element-wise SAVE. |
| `cat` | ✅ | 1.843 | 0.91× | 0.59× | OK | Sig: D = cat(DIM,A,B). 500x500 vert-cat. 100 iters. Element-wise SAVE. |
| `circshift` | ✅ | 3.830 | 0.33× | 0.64× | OK | Sig: B = circshift(A, K). 1000x1000 shift [3 5]. 100 iters. Element-wise SAVE. |
| `colon` | ⚠️ |  |  |  |  | works as `:` (range) operator; not callable as named fn |
| `combinations` | ❌ |  |  |  |  | all combinations |
| `ctranspose` | ✅ | 6.955 | 0.22× | 0.37× | OK | Sig: Y = ctranspose(A). 1k×1k Hermitian (real → same as transpose). 100 iters. |
| `diag` | ✅ | 0.008 | 1.22× | 1.97× | OK | Sig: V = diag(A). Diagonal of 2000x2000 deterministic. 100 iters. |
| `end` | ✅ |  |  |  |  | keyword + `A(end)` indexing form |
| `eye` | ✅ | 1.808 | 0.57× | 0.00× | OK | Sig: I = eye(N). 1000x1000 identity. 100 iters. |
| `false` | ✅ |  |  |  | N/A | Sig: F = false(M, N). 100x100 logical. 1000 iters. |
| `flip` | ✅ | 2.122 | 0.79× | 1.03× | OK | Sig: B = flip(A, DIM). 1000x1000 flip dim 2. 100 iters. Element-wise SAVE. |
| `fliplr` | ✅ | 2.144 | 0.80× | 1.02× | OK | Sig: B = fliplr(A). 1000x1000 left-right flip. 100 iters. Element-wise SAVE. |
| `flipud` | ✅ | 2.308 | 0.53× | 0.99× | OK | Sig: B = flipud(A). 1000x1000 up-down flip. 100 iters. Element-wise SAVE. |
| `freqspace` | ✅ | 0.003 | 31.49× |  | OK | Sig: [f1, f2] = freqspace(N|[N M]) or f = freqspace(N[, 'whole']). Now supports 2-output centered form and 2-vec [N M] input (via libs/builtin extension). |
| `head` | ✅ | 0.000 | 56.10× |  | OK | Sig: Y = head(X, K). First 100 elements. 10000 iters. |
| `horzcat` | ✅ | 1.842 | 0.62× | 0.57× | OK | Sig: D = horzcat(A, B). 500x500 || 500x500. 100 iters. |
| `ind2sub` | ✅ | 12.093 |  | 0.93× | OK | Sig: [I,J] = ind2sub(SZ, IND). 1M idx → row index. SAVE on row idx (y). 50 iters. |
| `ipermute` | ✅ | 5.008 | 0.66× | 1.16× | OK | Sig: Y = ipermute(X, ORDER). Round-trip via permute. 100 iters. |
| `iscolumn` | ✅ | 0.000 | 26.89× | 68.46× | OK | Sig: TF = iscolumn(X). 1k column. 100k iters. |
| `isempty` | ✅ | 0.000 | 25.72× | 34.68× | OK | Sig: TF = isempty(X). Empty []. 100k iters. |
| `ismatrix` | ✅ | 0.000 | 24.44× | 57.78× | OK | Sig: TF = ismatrix(X). 1k×1k mat. 100k iters. |
| `isrow` | ✅ | 0.000 | 29.38× | 17.67× | OK | Sig: TF = isrow(X). 1k row. 100k iters. |
| `isscalar` | ✅ | 0.000 | 41.65× | 43.61× | OK | Sig: TF = isscalar(X). 100k iters. |
| `issorted` | ✅ | 0.008 | 0.86× | 1.64× | OK | Sig: TF = issorted(X). 10k pre-sorted. 10k iters. |
| `issortedrows` | ✅ | 0.013 | 0.59× |  | OK | Sig: TF = issortedrows(X). 10k×3 pre-sorted. 1000 iters. |
| `isuniform` | ✅ | 0.174 | 0.10× | 5.40× | OK | Sig: TF = isuniform(X). 100k uniform. 10000 iters. |
| `isvector` | ✅ | 0.000 | 26.56× | 51.51× | OK | Sig: TF = isvector(X). 10k vec. 100k iters. |
| `length` | ✅ | 0.000 | 26.91× | 36.70× | OK | Sig: L = length(X). 100x600 → returns 600. 100k iters. |
| `linspace` | ✅ | 2.871 | 1.00× | 0.80× | OK | Sig: V = linspace(A,B,N). N=1M. 100 iters. Element-wise SAVE. |
| `logspace` | ✅ | 9.205 | 0.95× | 1.42× | OK | Sig: V = logspace(A,B,N). N=1M log-spaced. 100 iters. Element-wise SAVE. |
| `meshgrid` | ✅ | 11.413 | 0.21× | 0.40× | OK | Sig: [X,Y] = meshgrid(x,y). 1k×1k grid. 50 iters. SAVE on X. |
| `ndgrid` | ✅ |  |  |  | N/A | Sig: [X,Y] = ndgrid(x,y). 1k×1k grid. 100 iters. |
| `ndims` | ✅ | 0.000 | 27.42× | 25.81× | OK | Sig: N = ndims(X). 2D mat → 2. 100k iters. |
| `numel` | ✅ | 0.000 | 22.63× | 20.44× | OK | Sig: N = numel(X). 1M-elem mat. 100k iters. |
| `ones` | ✅ | 2.645 | 0.73× | 0.84× | OK | Sig: O = ones(M,N). 1000x1000. 100 iters. |
| `paddata` | ✅ | 0.001 | 110.39× |  | OK | Sig: Y = paddata(X, M). Pad to 1500. 1000 iters. |
| `permute` | ✅ | 2.322 | 0.54× | 1.11× | OK | Sig: Y = permute(X, ORDER). 100×100×100 → reordered. 100 iters. |
| `rand` | ✅ | 6.807 | 0.51× | 0.81× | OK | Sig: A = rand(M,N). 1k×1k uniform. 100 iters. Custom fp (RNG diffs). |
| `repelem` | ✅ | 2.189 | 0.55× | 1.01× | OK | Sig: Y = repelem(X, K). 1k vec each elem 1000x. 50 iters. |
| `repmat` | ✅ | 2.113 | 0.44× | 1.08× | OK | Sig: B = repmat(A,M,N). 50x50 → 1000x1000. 100 iters. |
| `reshape` | ✅ | 1.999 | 0.00× | 1.06× | OK | Sig: B = reshape(A,M,N). 1M vec → 1000x1000. 100 iters. |
| `resize` | ✅ | 0.001 | 132.27× | 9756.20× | OK | Sig: Y = resize(X, M). Resize to 1500 (pad with zeros). 1000 iters. |
| `rot90` | ✅ | 2.992 | 0.80× | 1.92× | OK | Sig: B = rot90(A). 1k×1k 90° rotate. 100 iters. |
| `shiftdim` | ✅ | 4.641 | 0.00× | 3.64× | OK | Sig: B = shiftdim(A). Drop leading singleton (numkit ndims=3, MATLAB=2 — see BUGS). 1000 iters. |
| `size` | ✅ | 0.000 | 18.70× | 36.28× | OK | Sig: S = size(X). 2D 100x600 → [100 600]. 100k iters. |
| `sort` | ✅ | 44.711 | 0.15× | 0.15× | OK | Sig: B = sort(A). 1M deterministic sin values. 100 iters. Element-wise SAVE. |
| `sortrows` | ✅ | 0.425 | 0.74× | 0.19× | OK | Sig: B = sortrows(A). 10k×3 sort by first col. 100 iters. |
| `squeeze` | ✅ | 2.034 | 0.01× | 0.00× | OK | Sig: Y = squeeze(X). 1×1k×1×1k → 1k×1k. 1000 iters. |
| `sub2ind` | ✅ | 7.505 | 0.23× | 0.47× | OK | Sig: IND = sub2ind(SZ, I, J). 1M (r,c) pairs. 50 iters. |
| `tail` | ✅ | 0.000 | 60.38× |  | OK | Sig: Y = tail(X, K). Last 100 elements. 10000 iters. |
| `transpose` | ✅ | 7.375 | 0.20× | 0.35× | OK | Sig: Y = transpose(X). 1k×1k transpose. 100 iters. Element-wise SAVE. |
| `trimdata` | ✅ | 0.000 | 139.09× |  | OK | Sig: Y = trimdata(X, M). Trim to 500. 1000 iters. |
| `true` | ✅ |  |  |  | N/A | Sig: T = true(M, N). 100x100 logical. 1000 iters. |
| `vertcat` | ✅ | 1.811 | 0.64× | 0.60× | OK | Sig: D = vertcat(A,B). 500x500 stack. 100 iters. |
| `zeros` | ✅ | 1.807 | 0.03× | 1.16× | OK | Sig: Z = zeros(M,N). 1000x1000. 100 iters. |

### Control Flow

**Namespace:** builtin (keywords) — 10 ✅ + 0 ⚠️ / 11 = 91%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `break` | ✅ |  |  |  |  | keyword |
| `continue` | ✅ |  |  |  |  | keyword |
| `end` | ✅ |  |  |  |  | keyword + `A(end)` indexing form |
| `for` | ✅ |  |  |  |  | keyword |
| `if` | ✅ |  |  |  |  | keyword |
| `parfor` | ❌ |  |  |  |  | parallel — out of scope |
| `pause` | ✅ | 0.000 | 744.48× | 32.63× | OK | Sig: pause(N). N=0 (no-op). 100k iters. |
| `return` | ✅ |  |  |  |  | keyword |
| `switch` | ✅ |  |  |  |  | keyword (`switch/case/otherwise`) |
| `try` | ✅ |  |  |  |  | keyword (`try/catch`) |
| `while` | ✅ |  |  |  |  | keyword |

### Numeric Types

**Namespace:** builtin — 27 ✅ + 0 ⚠️ / 29 = 93%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `allfinite` | ✅ | 0.490 | 0.09× |  | OK | Sig: TF = allfinite(X). Returns scalar (logical-scalar fp BUGS #14). 100k iters. |
| `anynan` | ✅ | 0.248 | 0.18× |  | OK | Sig: TF = anynan(X). Returns scalar. 100k iters. |
| `cast` | ✅ | 5.072 | 0.30× | 0.55× | OK | 1M doubles -> int32. 50 iters. |
| `double` | ✅ | 3.606 | 0.04× | 0.57× | OK | Sig: Y = double(X). 1M single → double. 50 iters. Element-wise SAVE. |
| `eps` | ✅ | 0.000 | 21.02× | 44.01× | OK | Sig: E = eps. Machine epsilon scalar. 1M iters. |
| `flintmax` | ✅ | 0.000 | 25.86× | 47.63× | OK | Sig: M = flintmax. Largest exact float-int. 1M iters. |
| `inf` | ✅ | 0.000 | 37.86× | 44.14× | OK | Sig: I = Inf. 1M iters. |
| `int16` | ✅ | 3.036 | 0.04× | 0.74× | OK | Sig: Y = int16(X). 1M doubles → int16. 50 iters. |
| `int32` | ✅ | 1.183 | 0.15× | 2.30× | OK | Sig: Y = int32(X). 1M doubles → int32. 50 iters. Element-wise SAVE. |
| `int64` | ✅ | 2.305 | 0.54× | 1.56× | OK | Sig: Y = int64(X). 1M doubles → int64. 50 iters. |
| `int8` | ✅ | 2.595 | 0.06× | 0.70× | OK | Sig: Y = int8(X). 1M doubles → int8. 50 iters. |
| `intmax` | ✅ | 0.000 | 11.60× | 16.41× | OK | Sig: M = intmax(TYPE). int32 max. 1M iters. |
| `intmin` | ✅ | 0.000 | 11.16× | 4.63× | OK | Sig: M = intmin(TYPE). int32 min. 1M iters. |
| `isfinite` | ✅ | 0.280 | 0.33× | 0.77× | OK | Sig: TF = isfinite(X). 1M-pt mixed. 50 iters. |
| `isfloat` | ✅ | 0.000 | 20.26× | 26.00× | OK | Sig: TF = isfloat(X). Returns scalar. 100k iters. |
| `isinf` | ✅ | 0.265 | 0.29× | 0.85× | OK | Sig: TF = isinf(X). 1M-pt with Inf/-Inf scattered. 50 iters. |
| `isinteger` | ✅ | 0.000 | 20.54× | 16.06× | OK | Sig: TF = isinteger(X). Returns scalar. 100k iters. |
| `isnan` | ✅ | 0.249 | 0.30× | 0.91× | OK | Sig: TF = isnan(X). 1M-pt with NaN every 3rd. 50 iters. Element-wise SAVE on logical. |
| `isnumeric` | ✅ | 0.000 | 23.28× | 24.81× | OK | Sig: TF = isnumeric(X). Returns scalar. 100k iters. |
| `isreal` | ✅ | 0.000 | 18.13× | 31.18× | OK | Sig: TF = isreal(X). Returns scalar. 100k iters. |
| `nan` | ✅ | 0.000 | 166.43× | 9.70× | OK | Sig: N = NaN. 1M iters. fp checks isnan since y itself is NaN. |
| `realmax` | ✅ | 0.000 | 30.11× | 45.22× | OK | Sig: M = realmax. Largest finite double. 1M iters. |
| `realmin` | ✅ | 0.000 | 31.22× | 26.61× | OK | Sig: M = realmin. Smallest normal double. 1M iters. |
| `single` | ✅ | 2.755 | 0.06× | 0.43× | OK | Sig: Y = single(X). 1M double → single. 50 iters. Element-wise SAVE. |
| `typecast` | ✅ | 1.059 | 0.01× | 0.97× | OK | 1M uint32 reinterpreted as 2M uint16 (LE byte order). 50 iters. |
| `uint16` | ✅ | 3.049 | 0.03× | 0.69× | OK | Sig: Y = uint16(X). 1M → uint16. 50 iters. |
| `uint32` | ✅ | 1.298 | 0.11× | 1.97× | OK | Sig: Y = uint32(X). 1M doubles → uint32. 50 iters. Element-wise SAVE. |
| `uint64` | ✅ | 4.673 | 0.27× | 0.76× | OK | Sig: Y = uint64(X). 1M → uint64. 50 iters. |
| `uint8` | ✅ | 2.576 | 0.03× | 0.63× | OK | Sig: Y = uint8(X). 1M → uint8. 50 iters. |

### Characters and Strings

**Namespace:** builtin — 54 ✅ + 0 ⚠️ / 65 = 83%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `append` | ✅ | 0.000 | 8.36× |  | OK | Sig: S = append(S1,S2). 3k char + 'bar'. 1000 iters. |
| `blanks` | ✅ | 0.000 | 3.40× | 30.21× | OK | Sig: S = blanks(N). N=1000. 10000 iters. |
| `cellstr` | ✅ | 0.001 | 4.28× |  | OK | Sig: C = cellstr(CHAR). 3-row char mat → cellstr. 10000 iters. |
| `char` | ✅ | 0.000 | 2.37× | 11.62× | OK | Sig: S = char(X). ASCII codes A-Z. 10000 iters. |
| `compose` | ✅ | 0.396 | 0.55× |  | OK | Format 1000 ints with single-spec template. 100 iters. |
| `contains` | ✅ | 0.000 | 3.75× |  | OK | Sig: TF = contains(S, PAT). 2k char single check (cellstr/string-array forms have parity issues). 1000 iters. Logical-scalar fp (BUGS #14). |
| `convertcharstostrings` | ✅ | 0.000 | 4.86× | 157.83× | OK | Sig: S = convertCharsToStrings(C). 100k iters. |
| `convertcontainedstringstochars` | ✅ | 0.001 | 1.76× |  | OK | Sig: C2 = convertContainedStringsToChars(C). 10000 iters. |
| `convertstringstochars` | ✅ | 0.000 | 3.26× | 69.36× | OK | Sig: C = convertStringsToChars(S). 100k iters. |
| `count` | ✅ | 0.005 | 1.06× |  | OK | Sig: N = count(S, PAT). 2.2k char string. 10k iters. |
| `deblank` | ✅ | 0.000 | 4.21× | 145.71× | OK | Sig: S = deblank(S). Trim trailing space. 10000 iters. |
| `double` | ✅ | 3.606 | 0.04× | 0.57× | OK | Sig: Y = double(X). 1M single → double. 50 iters. Element-wise SAVE. |
| `endsWith` | ✅ | 0.000 | 1.15× | 954.80× | OK | Sig: TF = endsWith(S, PATTERN). Char + string-scalar pattern forms. Logical-scalar fingerprint (BUGS #14). |
| `erase` | ✅ | 0.002 | 2.26× | 10.85× | OK | Sig: S2 = erase(S, PAT). 1.2k-char string remove 'bar '. 1000 iters. |
| `erasebetween` | ✅ | 0.000 | 6.05× |  | OK | Sig: S2 = eraseBetween(S, A, B). 10000 iters. |
| `extract` | ✅ | 0.104 | 0.82× |  | OK | Extract 'xyz' from 8000-char string with 1000 hits. 1000 iters. |
| `extractafter` | ✅ | 0.000 | 3.47× |  | OK | Sig: S2 = extractAfter(S, PAT). 10k iters. Function name camelCase. |
| `extractbefore` | ✅ | 0.000 | 3.12× |  | OK | Sig: S2 = extractBefore(S, PAT). 10k iters. |
| `extractbetween` | ✅ | 0.001 | 3.56× |  | OK | Sig: S2 = extractBetween(S, A, B). 3 matches. 10000 iters. |
| `insertafter` | ✅ | 0.000 | 6.71× |  | OK | Sig: S2 = insertAfter(S, PAT, ADD). 10000 iters. |
| `insertbefore` | ✅ | 0.000 | 6.82× |  | OK | Sig: S2 = insertBefore(S, PAT, ADD). 10000 iters. |
| `iscellstr` | ✅ | 0.000 | 7.36× | 23.33× | OK | Sig: TF = iscellstr(X). 100k iters. |
| `ischar` | ✅ | 0.000 | 8.05× | 34.28× | OK | Sig: TF = ischar(X). 100k iters. |
| `isletter` | ✅ | 0.034 | 0.77× | 2.18× | OK | Sig: TF = isletter(S). 14k char input. 1000 iters. Logical-array fp. |
| `isspace` | ✅ | 0.028 | 1.02× | 2.07× | OK | Sig: TF = isspace(S). 12k char input. 1000 iters. Logical-array fp. |
| `isstring` | ✅ | 0.000 | 23.96× | 66.39× | OK | Sig: TF = isstring(X). Returns scalar logical. 100k iters. |
| `isstringscalar` | ✅ |  |  |  | N/A | Sig: TF = isStringScalar(X). Camel-case fn name. 100k iters. |
| `isstrprop` | ✅ | 0.004 | 0.60× | 5.37× | OK | Sig: TF = isstrprop(S, prop). 1.6k char check digit. 1000 iters. |
| `join` | ✅ | 0.001 | 0.34× |  | OK | Join 24-element Greek-letter string array. 10k iters. |
| `lower` | ✅ | 0.046 | 1.59× | 3.67× | OK | Sig: Y = lower(S). 32k char string with mixed case. 1000 iters. Element-wise SAVE. |
| `matches` | ✅ | 0.000 | 3.85× |  | OK | Sig: TF = matches(S, PAT). Single string check. 10000 iters. |
| `newline` | ✅ | 0.000 | 3.15× | 7.71× | OK | Sig: NL = newline. ASCII LF=10. 100k iters. |
| `num2str` | ✅ | 0.000 | 32.25× | 604.47× | OK | Sig: S = num2str(X). 100k iters. |
| `pad` | ✅ | 0.000 | 14.90× |  | OK | Sig: S2 = pad(S, LEN). Pad 'foo' to length 20. 10000 iters. |
| `plus` | ✅ | 2.142 | 0.05× | 1.21× | OK | Sig: Y = plus(A, B). 1M-pt elementwise add via named fn. 50 iters. |
| `regexp` | ✅ | 0.300 | 0.21× |  | OK | Sig: M = regexp(S, PAT, 'match'). 2.5k char, find digit groups. 1000 iters. |
| `regexpi` | ✅ | 0.075 | 0.45× |  | OK | Sig: M = regexpi(S, PAT, 'match'). Case-insensitive. 1000 iters. |
| `regexprep` | ✅ | 0.248 | 0.19× | 0.91× | OK | Sig: S2 = regexprep(S, PAT, REP). 1.8k char replace. 1000 iters. |
| `regexptranslate` | ✅ | 0.000 | 18.05× | 86.59× | OK | Sig: T = regexptranslate('escape', S). 14-char metachars. 10000 iters. |
| `replace` | ✅ | 0.012 | 2.53× |  | OK | Sig: Y = replace(S, OLD, NEW). 16k string, 1k replacements. 1000 iters. |
| `replacebetween` | ✅ | 0.001 | 4.77× |  | OK | Sig: S2 = replaceBetween(S, A, B, REP). 10000 iters. |
| `reverse` | ✅ | 0.000 | 7.98× |  | OK | Sig: S2 = reverse(S). 1k-char reverse. 10000 iters. |
| `split` | ✅ | 0.103 | 0.84× |  | OK | Split CSV-like 4000-char string into 1000 tokens. 1000 iters. |
| `splitlines` | ✅ | 0.001 | 3.02× |  | OK | Sig: C = splitlines(S). 5-line input via sprintf '
| `sprintf` | ✅ | 0.001 | 5.38× | 5.43× | OK | Sig: S = sprintf(FMT, ...). Format scalar+int. 100k iters. |
| `sscanf` | ✅ | 0.000 | 5.16× | 80.00× | OK | Sig: A = sscanf(S, FMT). 5 floats. 100k iters. |
| `startsWith` | ✅ | 0.000 | 1.82× | 349.61× | OK | Sig: TF = startsWith(S, PATTERN). Char + string-scalar pattern forms. Logical-scalar fingerprint (BUGS #14). |
| `str2double` | ✅ | 0.000 | 24.98× | 16.03× | OK | Sig: V = str2double(S). 100k iters. |
| `strcat` | ✅ | 0.001 | 26.76× | 84.88× | OK | Sig: S = strcat(A, B). 5k + 6k char concat. 1000 iters. |
| `strcmp` | ✅ | 0.000 | 7.11× | 33.62× | OK | Sig: TF = strcmp(A, B). char-vs-char only. 100k iters. Logical-scalar fp (BUGS #14). |
| `strcmpi` | ✅ | 0.000 | 4.77× | 21.40× | OK | Sig: TF = strcmpi(A, B). 100k iters. |
| `strfind` | ✅ | 0.017 | 0.71× | 0.77× | OK | Sig: K = strfind(S, PAT). 15k string, 1k matches. 1000 iters. |
| `string` | ✅ | 0.002 | 0.59× | 504.99× | OK | Sig: S = string(X). Numeric → string array. 1000 iters. fp limited to numel (string-array indexing broken — BUGS #7). |
| `strings` | ✅ | 0.728 | 0.19× |  | OK | Sig: S = strings(M, N). 100x100 empty string array. 10000 iters. |
| `strip` | ✅ | 0.000 | 10.49× |  | OK | Sig: S = strip(S). Trim both. 10000 iters. |
| `strjoin` | ✅ | 0.009 | 12.80× | 89.14× | OK | Sig: S = strjoin(C, DELIM). 1k tokens via for-init (repmat rejects cell). 1000 iters. |
| `strjust` | ✅ | 0.000 | 18.35× | 320.71× | OK | Sig: S2 = strjust(S, side). 3-row right-justify. 10000 iters. |
| `strlength` | ✅ | 0.000 | 7.28× |  | OK | Sig: L = strlength(S). Single string (cellstr form differs). 100k iters. |
| `strncmp` | ✅ | 0.000 | 6.92× | 35.38× | OK | Sig: TF = strncmp(A, B, N). 100k iters. |
| `strncmpi` | ✅ | 0.000 | 5.67× | 25.72× | OK | Sig: TF = strncmpi(A, B, N). 100k iters. |
| `strrep` | ✅ | 0.012 | 1.59× | 1.21× | OK | Sig: Y = strrep(S, OLD, NEW). 16k string, 1k replacements. 1000 iters. |
| `strsplit` | ✅ | 0.076 | 1.23× |  | MISMATCH | Sig: C = strsplit(S, DELIM). 3.5k string, 500 splits → cell. 1000 iters. Custom fp (cell out). |
| `strtok` | ✅ | 0.000 |  | 85.38× | OK | Sig: [TOK, REM] = strtok(S). 10000 iters. |
| `strtrim` | ✅ | 0.000 | 3.09× | 135.74× | OK | Sig: S = strtrim(S). Trim leading+trailing. 10000 iters. |
| `upper` | ✅ | 0.068 | 1.10× | 2.51× | OK | Sig: Y = upper(S). 32k char string with mixed case. 1000 iters. Element-wise SAVE. |

### Structures

**Namespace:** builtin — 12 ✅ + 0 ⚠️ / 14 = 86%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `arrayfun` | ✅ |  |  |  |  |  |
| `cell2struct` | ✅ | 0.000 | 4.73× | 15.26× | OK | Sig: S = cell2struct(C, FIELDS, DIM). 10k iters. |
| `fieldnames` | ✅ | 0.001 | 1.82× |  | MISMATCH | Sig: C = fieldnames(S). 5-field struct. 10k iters. Cell-out fp. |
| `getfield` | ✅ | 0.000 | 15.25× | 107.10× | OK | Sig: V = getfield(S, F). 100k iters. |
| `isfield` | ✅ | 0.000 | 6.67× | 26.00× | OK | Sig: TF = isfield(S, F). 100k iters. |
| `isstruct` | ✅ | 0.000 | 8.76× | 29.48× | OK | Sig: TF = isstruct(S). Returns scalar logical. 100k iters. |
| `orderfields` | ✅ | 0.000 |  |  | N/A | Sig: S2 = orderfields(S). Alphabetical sort of fields. 10000 iters. |
| `rmfield` | ✅ | 0.000 | 13.56× | 11.47× | OK | Sig: S2 = rmfield(S, F). Remove 'c' from 5-field. 10k iters. |
| `setfield` | ✅ | 0.000 | 7.82× | 67.62× | OK | Sig: S2 = setfield(S, F, V). 10k iters. |
| `struct` | ✅ | 0.000 | 7.90× | 34.50× | OK | Sig: S = struct(name1,val1,...). 5 fields. 10k iters. Custom fp. |
| `struct2cell` | ✅ | 0.000 | 3.91× | 22.13× | OK | Sig: C = struct2cell(S). 5 fields. 10k iters. |
| `struct2table` | ❌ |  |  |  |  |  |
| `structfun` | ✅ | 0.002 | 3.03× | 38.01× | OK | Sig: A = structfun(@F, S). Apply *2 to each field. 1000 iters. (May fail due to lambda BUG #11). |
| `table2struct` | ❌ |  |  |  |  |  |

### Cell Arrays

**Namespace:** builtin — 12 ✅ + 0 ⚠️ / 17 = 71%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `cell` | ✅ | 0.068 | 0.08× | 2.60× | OK | Sig: C = cell(M, N). 100x100 empty cell. 1000 iters. |
| `cell2mat` | ✅ | 0.000 | 36.51× | 149.49× | OK | Sig: M = cell2mat(C). 3x3 cell of scalars. 10000 iters. |
| `cell2struct` | ✅ | 0.000 | 4.73× | 15.26× | OK | Sig: S = cell2struct(C, FIELDS, DIM). 10k iters. |
| `cell2table` | ❌ |  |  |  |  |  |
| `celldisp` | ✅ |  |  |  | N/A | Sig: celldisp(C). Captured via evalc. 10000 iters. |
| `cellfun` | ✅ | 0.002 | 2.64× | 20.17× | OK | Sig: A = cellfun(@F, C). Apply to cells. 1000 iters. |
| `cellplot` | ❌ |  |  |  |  |  |
| `cellstr` | ✅ | 0.001 | 4.28× |  | OK | Sig: C = cellstr(CHAR). 3-row char mat → cellstr. 10000 iters. |
| `iscell` | ✅ | 0.000 | 8.30× | 36.14× | OK | Sig: TF = iscell(X). 100k iters. |
| `iscellstr` | ✅ | 0.000 | 7.36× | 23.33× | OK | Sig: TF = iscellstr(X). 100k iters. |
| `mat2cell` | ✅ | 0.001 | 17.51× | 7.67× | OK | Sig: C = mat2cell(M, R, C). 6x6 → 2x2 cell of 3x3. 10000 iters. |
| `num2cell` | ✅ | 0.007 | 10.72× | 7.25× | OK | Sig: C = num2cell(A). 1k-vec wrap each. 1000 iters. |
| `string` | ✅ | 0.002 | 0.59× | 504.99× | OK | Sig: S = string(X). Numeric → string array. 1000 iters. fp limited to numel (string-array indexing broken — BUGS #7). |
| `struct2cell` | ✅ | 0.000 | 3.91× | 22.13× | OK | Sig: C = struct2cell(S). 5 fields. 10k iters. |
| `table` | ❌ |  |  |  |  |  |
| `table2cell` | ❌ |  |  |  |  |  |
| `timetable` | ❌ |  |  |  |  |  |

### Function Handles

**Namespace:** builtin — 5 ✅ + 0 ⚠️ / 6 = 83%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `feval` | ✅ | 0.000 | 4.85× | 26.99× | OK | Sig: V = feval(F, X). Call sin(pi/2) via feval. 100k iters. |
| `func2str` | ✅ | 0.000 | 4.83× |  | OK | Sig: S = func2str(F). 10k iters. |
| `function_handle` | ❌ |  |  |  |  | OOP class |
| `functions` | ✅ | 0.000 | 2.77× | 6.59× | OK | Sig: I = functions(F). Introspect handle. 10000 iters. |
| `localfunctions` | ✅ | 0.000 | 373.56× | 9.30× | OK | Sig: F = localfunctions(). Stub returns empty cell. 100k iters. |
| `str2func` | ✅ | 0.000 | 14.84× | 19.64× | OK | Sig: F = str2func(NAME). 10k iters. fp checks created handle works. |

### Categorical Arrays

**Namespace:** `categorical.*` (future) — 1 ✅ + 0 ⚠️ / 17 = 5%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `addcats` | ❌ |  |  |  |  |  |
| `categorical` | ❌ |  |  |  |  |  |
| `categories` | ❌ |  |  |  |  |  |
| `combinations` | ❌ |  |  |  |  | all combinations |
| `countcats` | ❌ |  |  |  |  |  |
| `discretize` | ✅ | 0.106 | 1.48× |  | OK | Sig: BIN = discretize(X, EDGES). 100k pts into 10 bins. 100 iters. |
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
| `fillmissing` | ❌ |  |  |  |  |  |
| `findgroups` | ❌ |  |  |  |  |  |
| `groupcounts` | ❌ |  |  |  |  |  |
| `groupfilter` | ❌ |  |  |  |  |  |
| `groupsummary` | ❌ |  |  |  |  |  |
| `grouptransform` | ❌ |  |  |  |  |  |
| `head` | ✅ | 0.000 | 56.10× |  | OK | Sig: Y = head(X, K). First 100 elements. 10000 iters. |
| `height` | ❌ |  |  |  |  |  |
| `inner2outer` | ❌ |  |  |  |  |  |
| `innerjoin` | ❌ |  |  |  |  |  |
| `intersect` | ✅ | 0.623 | 0.43× | 0.42× | OK | Sig: C = intersect(A, B). 10k vs 10k overlap. 100 iters. Element-wise SAVE. |
| `ismember` | ✅ | 1.359 | 0.30× | 0.55× | OK | Sig: TF = ismember(A, B). 100k vs 20k members. 50 iters. Element-wise SAVE on logical. |
| `ismissing` | ❌ |  |  |  |  |  |
| `issortedrows` | ✅ | 0.013 | 0.59× |  | OK | Sig: TF = issortedrows(X). 10k×3 pre-sorted. 1000 iters. |
| `join` | ✅ | 0.001 | 0.34× |  | OK | Join 24-element Greek-letter string array. 10k iters. |
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
| `rmmissing` | ❌ |  |  |  |  |  |
| `rmprop` | ❌ |  |  |  |  |  |
| `rowfun` | ❌ |  |  |  |  |  |
| `rows2vars` | ❌ |  |  |  |  |  |
| `setdiff` | ✅ | 0.591 | 0.52× | 0.50× | OK | Sig: C = setdiff(A, B). 10k minus 10k. 100 iters. Element-wise SAVE. |
| `setxor` | ✅ | 0.971 | 0.57× | 0.35× | OK | Sig: C = setxor(A, B). 10k symdiff 10k. 100 iters. Element-wise SAVE. |
| `sortrows` | ✅ | 0.425 | 0.74× | 0.19× | OK | Sig: B = sortrows(A). 10k×3 sort by first col. 100 iters. |
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
| `tail` | ✅ | 0.000 | 60.38× |  | OK | Sig: Y = tail(X, K). Last 100 elements. 10000 iters. |
| `timetable2table` | ❌ |  |  |  |  |  |
| `topkrows` | ❌ |  |  |  |  |  |
| `union` | ✅ | 1.183 | 0.33× | 0.15× | OK | Sig: C = union(A, B). 10k union 10k. 100 iters. Element-wise SAVE. |
| `unique` | ✅ | 0.931 | 1.08× | 0.35× | OK | Sig: C = unique(A). 100k with ~7919 distinct. 100 iters. Element-wise SAVE. |
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
| `bitand` | ✅ | 5.979 | 1.84× | 2.29× | OK | Sig: Y = bitand(A, B). 1M double (numkit rejects uint32 — see BUGS #13). 50 iters. |
| `bitcmp` | ✅ | 6.159 | 0.39× |  | OK | Sig: Y = bitcmp(A, type). 1M double + 'uint32' (numkit rejects uint32 array — see BUGS #13). 50 iters. |
| `bitget` | ✅ | 3.916 | 0.63× | 2.51× | OK | Sig: Y = bitget(A, K). 1M double, bit 3. 50 iters. |
| `bitor` | ✅ | 5.795 | 1.85× | 2.35× | OK | Sig: Y = bitor(A, B). 1M double. 50 iters. |
| `bitset` | ✅ | 4.155 | 0.61× | 8.75× | OK | Sig: Y = bitset(A, K). 1M double, set bit 5. 50 iters. |
| `bitshift` | ✅ | 4.415 | 0.56× | 1.79× | OK | Sig: Y = bitshift(A, K). 1M double << 3. 50 iters. |
| `bitxor` | ✅ | 5.769 | 1.84× | 2.37× | OK | Sig: Y = bitxor(A, B). 1M double. 50 iters. |
| `swapbytes` | ✅ | 1.070 | 0.95× | 8.06× | OK | Sig: Y = swapbytes(X). 1M uint32 endian-swap. 50 iters. (uint out — fp via double cast). |

### Set Operations

**Namespace:** builtin — 10 ✅ + 0 ⚠️ / 13 = 77%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `allunique` | ✅ | 0.101 | 1.03× |  | OK | Sig: TF = allunique(X). 10k unique values. 1000 iters. |
| `innerjoin` | ❌ |  |  |  |  |  |
| `intersect` | ✅ | 0.623 | 0.43× | 0.42× | OK | Sig: C = intersect(A, B). 10k vs 10k overlap. 100 iters. Element-wise SAVE. |
| `ismember` | ✅ | 1.359 | 0.30× | 0.55× | OK | Sig: TF = ismember(A, B). 100k vs 20k members. 50 iters. Element-wise SAVE on logical. |
| `ismembertol` | ✅ | 0.769 | 0.31× | 7.59× | OK | Sig: TF = ismembertol(A, B, TOL). 10k vs 100 with tol=0.005. 100 iters. |
| `join` | ✅ | 0.001 | 0.34× |  | OK | Join 24-element Greek-letter string array. 10k iters. |
| `numunique` | ✅ | 0.030 | 4.78× |  | OK | Sig: N = numunique(X). 10k with 137 distinct. 1000 iters. |
| `outerjoin` | ❌ |  |  |  |  |  |
| `setdiff` | ✅ | 0.591 | 0.52× | 0.50× | OK | Sig: C = setdiff(A, B). 10k minus 10k. 100 iters. Element-wise SAVE. |
| `setxor` | ✅ | 0.971 | 0.57× | 0.35× | OK | Sig: C = setxor(A, B). 10k symdiff 10k. 100 iters. Element-wise SAVE. |
| `union` | ✅ | 1.183 | 0.33× | 0.15× | OK | Sig: C = union(A, B). 10k union 10k. 100 iters. Element-wise SAVE. |
| `unique` | ✅ | 0.931 | 1.08× | 0.35× | OK | Sig: C = unique(A). 100k with ~7919 distinct. 100 iters. Element-wise SAVE. |
| `uniquetol` | ✅ | 0.234 | 0.49× | 6.98× | MISMATCH | Sig: U = uniquetol(X, TOL). 10k with rounded vals. 100 iters. |

### Arithmetic

**Namespace:** builtin — 28 ✅ + 0 ⚠️ / 34 = 82%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `bsxfun` | ✅ | 2.169 | 0.55× | 0.99× | OK | Sig: D = bsxfun(@op, A, B). Broadcast 1x1k + 1kx1 → 1k×1k. 100 iters. |
| `ceil` | ✅ | 2.232 | 0.20× | 1.59× | OK | Sig: Y = ceil(X). 1M-pt sweep with non-integer offset. 20 iters. Element-wise SAVE. |
| `ctranspose` | ✅ | 6.955 | 0.22× | 0.37× | OK | Sig: Y = ctranspose(A). 1k×1k Hermitian (real → same as transpose). 100 iters. |
| `cumprod` | ✅ | 0.002 | 13.53× | 24.37× | OK | Sig: Y = cumprod(X). 1k-pt cumprod near 1 (avoid overflow). 20 iters. |
| `cumsum` | ✅ | 2.579 | 1.13× | 1.01× | OK | Sig: Y = cumsum(X). 1M-pt cumulative sum (default dim). 20 iters. |
| `diff` | ✅ | 4.714 | 0.31× | 0.50× | OK | Sig: Y = diff(X). 1M-pt adjacent differences. 20 iters. Element-wise SAVE. |
| `fix` | ✅ | 2.145 | 0.24× | 1.63× | OK | Sig: Y = fix(X). 1M-pt sweep with non-integer offset. 20 iters. Element-wise SAVE. |
| `floor` | ✅ | 2.106 | 0.20× | 1.68× | OK | Sig: Y = floor(X). 1M-pt sweep with non-integer offset. 20 iters. Element-wise SAVE. |
| `idivide` | ✅ | 10.146 | 0.10× | 0.84× | OK | Sig: Y = idivide(A, B). int32 division. 50 iters. |
| `ldivide` | ✅ | 2.120 | 0.06× | 1.25× | MISMATCH | Sig: Y = ldivide(A, B). 1M-pt left-div = B/A. 50 iters. |
| `minus` | ✅ | 2.054 | 0.06× | 1.20× | OK | Sig: Y = minus(A, B). 1M-pt sub. 50 iters. |
| `mldivide` | ✅ |  |  |  | N/A | Sig: X = mldivide(A, B) = A\B. 100x100. 100 iters. |
| `mod` | ✅ | 3.384 | 0.30× | 1.45× | OK | Sig: Y = mod(X, D). 1M-pt with scalar divisor 7. 20 iters. Element-wise SAVE. |
| `movsum` | ✅ | 0.005 | 36.74× | 334.71× | OK | Sig: movsum(A, k[, dim] [, nanflag] [, Name, Value]). Same surface as movmean. Closes audit/findings/stats/movsum.md. |
| `mpower` | ✅ |  |  |  | N/A | Sig: Y = mpower(A, n). 20x20 matrix squared. 1000 iters. |
| `mrdivide` | ✅ |  |  |  | N/A | Sig: X = mrdivide(A, B) = A/B. 100x100. 100 iters. |
| `mtimes` | ✅ | 0.093 | 0.52× | 0.79× | OK | Sig: C = mtimes(A, B). 100x100 matmul. 100 iters. |
| `pagectranspose` | ✅ | 0.207 | 0.24× | 0.23× | OK | 128x64x8 real-valued — pagectranspose equals pagetranspose. 100 iters. |
| `pagemldivide` | ❌ |  |  |  |  |  |
| `pagemrdivide` | ❌ |  |  |  |  |  |
| `pagemtimes` | ✅ | 0.019 | 0.78× |  | OK | Sig: C = pagemtimes(A, B). 20×20×20 batch matmul. 100 iters. |
| `pagetranspose` | ✅ | 0.083 | 1.11× | 0.63× | OK | 128x64x8 array, page-wise transpose. 100 iters. |
| `plus` | ✅ | 2.142 | 0.05× | 1.21× | OK | Sig: Y = plus(A, B). 1M-pt elementwise add via named fn. 50 iters. |
| `power` | ✅ | 0.984 | 0.02× | 0.04× | OK | Sig: Y = power(A, B). 100k-pt squaring. 100 iters. |
| `prod` | ✅ | 0.002 | 11.52× | 20.71× | OK | Sig: Y = prod(X). 1k-pt reduction near 1 (avoid overflow). 20 iters. |
| `rdivide` | ✅ | 2.112 | 0.07× | 1.22× | MISMATCH | Sig: Y = rdivide(A, B). 1M-pt div. 50 iters. |
| `rem` | ✅ | 4.909 | 0.15× | 0.96× | OK | Sig: Y = rem(X, D). 1M-pt with scalar divisor 7. 20 iters. Element-wise SAVE. |
| `round` | ✅ | 2.209 | 0.17× | 1.59× | OK | Sig: Y = round(X). 1M-pt sweep with non-half offset. 20 iters. Element-wise SAVE. |
| `sum` | ✅ | 1.378 | 0.05× | 0.29× | OK | Sig: Y = sum(X). 1M-pt full reduction (default dim). 20 iters. |
| `tensorprod` | ❌ |  |  |  |  | tensor contraction |
| `times` | ✅ | 2.133 | 0.07× | 1.17× | OK | Sig: Y = times(A, B). 1M-pt elementwise mul. 50 iters. |
| `transpose` | ✅ | 7.375 | 0.20× | 0.35× | OK | Sig: Y = transpose(X). 1k×1k transpose. 100 iters. Element-wise SAVE. |
| `uminus` | ✅ | 3.806 | 0.03× | 0.58× | OK | Sig: Y = uminus(X). 1M-pt unary minus. 50 iters. |
| `uplus` | ✅ | 0.000 | 13.31× | 16.92× | OK | Sig: Y = uplus(X). 1M-pt unary plus (no-op). 50 iters. |

### Trigonometry

**Namespace:** builtin — 47 ✅ + 0 ⚠️ / 47 = 100%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `acos` | ✅ | 2.751 | 0.56× | 2.92× | OK | Sig: Y = acos(X). 1M-pt sweep on [-1, 1]. 20 iters. Element-wise SAVE. |
| `acosd` | ✅ | 2.681 | 0.68× | 4.89× | OK | Sig: Y = acosd(X). 1M-pt sweep on [-1,1]. Inverse trig (degrees). 20 iters. Element-wise SAVE. tol relaxed to 1e-10 because acos derivative diverges near x=±1 (1 elem rel diff 1.11e-12 at x≈0.99993, algorithmically correct). |
| `acosh` | ✅ | 3.507 | 0.60× | 2.74× | OK | Sig: Y = acosh(X). 1M-pt on [1,10] (domain X>=1). 20 iters. Element-wise SAVE. |
| `acot` | ✅ | 2.897 | 0.18× | 4.10× | OK | Sig: Y = acot(X). 1M-pt on [0.1,10] (avoid 0 singularity). 20 iters. Element-wise SAVE. |
| `acotd` | ✅ | 2.946 | 0.21× | 4.83× | OK | Sig: Y = acotd(X). 1M-pt (degrees). 20 iters. Element-wise SAVE. |
| `acoth` | ✅ | 3.065 | 0.83× | 5.46× | OK | Sig: Y = acoth(X). 1M-pt on (1,10] (domain |X|>1). 20 iters. Element-wise SAVE. |
| `acsc` | ✅ | 2.652 | 0.59× | 5.79× | OK | Sig: Y = acsc(X). 1M-pt domain |X|>=1. 20 iters. |
| `acscd` | ✅ | 2.728 | 0.60× | 7.48× | OK | Sig: Y = acscd(X). 1M-pt deg. 20 iters. |
| `acsch` | ✅ | 3.949 | 0.27× | 3.96× | OK | Sig: Y = acsch(X). 1M-pt avoid 0 (X != 0). 20 iters. |
| `asec` | ✅ | 2.909 | 0.47× | 5.26× | OK | Sig: Y = asec(X). 1M-pt domain |X|>=1. 20 iters. |
| `asecd` | ✅ | 2.777 | 0.54× | 7.35× | OK | Sig: Y = asecd(X). 1M-pt deg. 20 iters. |
| `asech` | ✅ | 3.849 | 0.50× | 4.53× | OK | Sig: Y = asech(X). 1M-pt domain (0,1]. 20 iters. |
| `asin` | ✅ | 2.495 | 0.70× | 3.55× | OK | Sig: Y = asin(X). 1M-pt sweep on [-1, 1]. 20 iters. Element-wise SAVE. |
| `asind` | ✅ | 2.571 | 0.64× | 5.14× | OK | Sig: Y = asind(X). 1M-pt on [-1,1]. Inverse (degrees). 20 iters. Element-wise SAVE. |
| `asinh` | ✅ | 3.620 | 0.34× | 2.27× | OK | Sig: Y = asinh(X). 1M-pt on [-10,10]. 20 iters. Element-wise SAVE. |
| `atan` | ✅ | 2.757 | 0.20× | 1.64× | OK | Sig: Y = atan(X). 1M-pt sweep on [-10, 10]. 20 iters. Element-wise SAVE. |
| `atan2` | ✅ | 3.513 | 0.22× | 2.25× | OK | Sig: P = atan2(Y, X). 1000x1000 quadrant grid. 20 iters. Element-wise SAVE. |
| `atan2d` | ✅ | 3.552 | 0.26× | 2.84× | OK | Sig: Z = atan2d(Y, X). 1k×1k quadrant grid (degrees). 20 iters. Element-wise SAVE. |
| `atand` | ✅ | 2.703 | 0.21× | 2.50× | OK | Sig: Y = atand(X). 1M-pt on [-10,10]. Inverse (degrees). 20 iters. Element-wise SAVE. |
| `atanh` | ✅ | 2.734 | 0.96× | 3.38× | OK | Sig: Y = atanh(X). 1M-pt on (-1,1) (avoid pole). 20 iters. Element-wise SAVE. |
| `cart2pol` | ✅ | 5.823 | 0.56× | 3.94× | OK | Sig: [TH,R] = cart2pol(X,Y) (2-D). 1000x1000 grid. 3-D form [TH,R,Z] = cart2pol(X,Y,Z) not benched yet. 20 iters. |
| `cart2sph` | ✅ |  |  |  | N/A | Sig: [TH,PHI,R] = cart2sph(X,Y,Z). 50³ grid. 50 iters. SAVE on TH (y). |
| `cos` | ✅ | 0.884 | 1.00× | 5.24× | OK | Sig: Y = cos(X). 1M-point sweep over 4π. 20 iters. Element-wise SAVE. |
| `cosd` | ✅ | 2.536 | 0.33× | 8.94× | OK | Sig: Y = cosd(X). 1M-pt sweep on [-720°, 720°]. degree variant. 20 iters. Element-wise SAVE. |
| `cosh` | ✅ | 3.354 | 0.27× | 1.70× | OK | Sig: Y = cosh(X). 1M-pt sweep on [-3, 3]. 20 iters. Element-wise SAVE. |
| `cospi` | ✅ | 2.926 | 0.29× | 6.19× | OK | Sig: Y = cospi(X) = cos(π·X). 1M-pt sweep on [-2, 2]. 20 iters. Element-wise SAVE. |
| `cot` | ✅ | 3.285 | 0.32× | 4.09× | OK | Sig: Y = cot(X). 1M-pt on (0, π) avoiding 0/π poles. 20 iters. |
| `cotd` | ✅ | 3.309 | 0.34× | 10.36× | OK | Sig: Y = cotd(X). 1M-pt deg, avoid 0/180. 20 iters. |
| `coth` | ✅ | 4.498 | 0.29× | 3.25× | OK | Sig: Y = coth(X). 1M-pt avoid 0 pole. 20 iters. |
| `csc` | ✅ | 2.635 | 0.34× | 4.65× | OK | Sig: Y = csc(X). 1M-pt on (0, π). 20 iters. |
| `cscd` | ✅ | 2.696 | 0.34× | 11.33× | OK | Sig: Y = cscd(X). 1M-pt deg. 20 iters. |
| `csch` | ✅ | 2.933 | 0.40× | 4.49× | OK | Sig: Y = csch(X). 1M-pt avoid 0 pole. 20 iters. |
| `deg2rad` | ✅ | 4.090 | 0.33× | 0.61× | OK | Sig: R = deg2rad(D). 1M-pt sweep. 20 iters. |
| `hypot` | ✅ | 2.464 | 0.45× | 2.03× | OK | Sig: Y = hypot(A, B). 1k×1k grid. 20 iters. Element-wise SAVE. |
| `pol2cart` | ✅ | 15.792 |  | 0.99× | OK | Sig: [X,Y]=pol2cart(TH,R). 1k×1k grid. 20 iters. SAVE on X. |
| `rad2deg` | ✅ | 3.942 | 0.36× | 0.60× | OK | Sig: D = rad2deg(R). 1M-pt sweep. 20 iters. |
| `sec` | ✅ | 2.690 | 0.33× | 4.47× | OK | Sig: Y = sec(X). 1M-pt on [-1.5, 1.5] (avoid π/2). 20 iters. Element-wise SAVE. |
| `secd` | ✅ | 2.798 | 0.29× | 10.67× | OK | Sig: Y = secd(X). 1M-pt on [-89°, 89°]. 20 iters. Element-wise SAVE. |
| `sech` | ✅ | 3.418 | 0.31× | 3.92× | OK | Sig: Y = sech(X). 1M-pt on [-5, 5]. 20 iters. Element-wise SAVE. |
| `sin` | ✅ | 0.836 |  |  | N/A | Sig: Y = sin(X). 1M-point sweep over 4π. 20 iters. Element-wise SAVE. |
| `sind` | ✅ | 2.597 | 0.32× | 7.95× | OK | Sig: Y = sind(X). 1M-pt sweep on [-720°, 720°]. degree variant. 20 iters. Element-wise SAVE. |
| `sinh` | ✅ | 3.105 | 0.35× | 1.87× | OK | Sig: Y = sinh(X). 1M-pt sweep on [-3, 3]. 20 iters. Element-wise SAVE. |
| `sinpi` | ✅ | 2.577 | 0.27× | 6.71× | OK | Sig: Y = sinpi(X) = sin(π·X). 1M-pt sweep on [-2, 2]. 20 iters. Element-wise SAVE. |
| `sph2cart` | ✅ |  |  |  | N/A | Sig: [X,Y,Z] = sph2cart(TH,PH,R). 50³ grid. 50 iters. SAVE on X (y). |
| `tan` | ✅ | 3.301 | 0.26× | 1.60× | OK | Sig: Y = tan(X). 1M-point sweep on [-1.5, 1.5] (avoid π/2 singularity). 20 iters. Element-wise SAVE. |
| `tand` | ✅ | 3.442 | 0.24× | 7.32× | OK | Sig: Y = tand(X). 1M-pt sweep on [-89°, 89°] (avoid 90° singularity). 20 iters. Element-wise SAVE. |
| `tanh` | ✅ | 3.078 | 0.41× | 2.26× | OK | Sig: Y = tanh(X). 1M-pt sweep on [-5, 5]. 20 iters. Element-wise SAVE. |

### Exponents and Logarithms

**Namespace:** builtin — 13 ✅ + 0 ⚠️ / 13 = 100%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `exp` | ✅ | 0.892 | 0.84× | 4.94× | OK | Sig: Y = exp(X). 1M-pt sweep on [-10,10]. 20 iters. Element-wise SAVE. |
| `expm1` | ✅ | 6.865 | 0.11× | 0.70× | OK | Sig: Y = expm1(X) = exp(X)-1. 1M-pt on [-2,2]. 20 iters. Element-wise SAVE. |
| `log` | ✅ | 0.762 | 2.60× | 10.62× | OK | Sig: Y = log(X). 1M-pt on [0.001, 100]. 20 iters. Element-wise SAVE. |
| `log10` | ✅ | 6.206 | 0.39× | 1.31× | OK | Sig: Y = log10(X). 1M-pt on [0.001, 1000]. 20 iters. Element-wise SAVE. |
| `log1p` | ✅ | 6.747 | 0.29× | 1.33× | OK | Sig: Y = log1p(X) = log(1+X). 1M-pt on [-0.5, 5] (avoid X=-1). 20 iters. Element-wise SAVE. |
| `log2` | ✅ | 8.248 | 0.30× | 1.90× | OK | Sig: Y = log2(X). 1M-pt on [0.001, 1024]. 20 iters. Element-wise SAVE. |
| `nextpow2` | ✅ | 0.007 | 67.71× |  | OK | Sig: P = nextpow2(N). Smallest p such that 2^p >= |N|. Vectorised. Edges: |x|=0 -> 0; NaN -> NaN; ±Inf -> +Inf; complex z -> uses |z|. Bug fix 2026-05-08: complex input previously threw; NaN/Inf paths now match MATLAB. tol=0. |
| `nthroot` | ✅ | 10.025 | 1.72× | 1.00× | OK | Sig: Y = nthroot(X, N). N=3, X on [0.001, 100]. 20 iters. Element-wise SAVE. |
| `pow2` | ✅ | 5.549 | 0.74× | 0.61× | OK | Sig: Y = pow2(X) = 2.^X. 1M-pt on [-50, 50]. 20 iters. Element-wise SAVE. |
| `reallog` | ✅ | 6.065 | 0.35× | 1.40× | OK | Sig: Y = reallog(X). Strict positive domain. 1M-pt on [0.001, 100]. 20 iters. Element-wise SAVE. |
| `realpow` | ✅ | 12.251 | 0.48× | 1.36× | OK | Sig: Z = realpow(X,Y). 1k×1k grid of x>0, real exp. 20 iters. Element-wise SAVE. |
| `realsqrt` | ✅ | 4.286 | 0.33× | 1.89× | OK | Sig: Y = realsqrt(X). 1M-pt on [0, 1000]. 20 iters. Element-wise SAVE. |
| `sqrt` | ✅ | 4.191 | 0.30× | 1.81× | OK | Sig: Y = sqrt(X). 1M-pt sqrt. 50 iters. |

### Special Functions

**Namespace:** builtin — 20 ✅ + 0 ⚠️ / 24 = 83%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `airy` | ✅ | 7.487 | 0.10× | 0.38× | OK | Ai over 10k pts on [-5,5]. 10 iters. Element-wise comparison. |
| `besselh` | ✅ | 0.317 | 1.16× | 7.20× | OK | Sig: H = besselh(NU, K, X). H1_0 on (0.1,10]. 50 iters. |
| `besseli` | ✅ | 0.096 | 2.53× | 22.58× | OK | Sig: I = besseli(NU, X). I_0 on (0.1,10]. 50 iters. |
| `besselj` | ✅ | 0.141 | 2.60× | 20.24× | OK | Sig: J = besselj(NU, X). J_0 on (0.1,30]. 50 iters. |
| `besselk` | ✅ | 0.139 | 1.72× | 12.34× | OK | Sig: K = besselk(NU, X). K_0 on (0.1,10]. 50 iters. |
| `bessely` | ✅ | 0.218 | 2.25× | 15.78× | OK | Sig: Y = bessely(NU, X). Y_0 on (0.1,30]. 50 iters. |
| `beta` | ✅ | 63.067 | 0.11× | 0.70× | OK | Sig: Y = beta(Z, W). 1000x1000 grid. 20 iters. Element-wise SAVE. |
| `betainc` | ✅ | 0.095 | 1.09× | 3.09× | OK | Sig: I = betainc(X, A, B). 1k-pt with scalar a=2.5 b=4. 20 iters. Element-wise SAVE. |
| `betaincinv` | ✅ | 1.119 | 1.04× | 4.43× | OK | Inverse regularized beta over 2k probability points, a=3 b=5. 20 iters, element-wise. |
| `betaln` | ✅ | 149.841 | 0.05× | 0.25× | OK | Sig: Y = betaln(Z, W). 1000x1000 grid. 20 iters. Element-wise SAVE. |
| `ellipj` | ✅ | 0.614 | 2.23× | 1.41× | OK | Jacobi sn over 5k pts at m=0.7. 50 iters, element-wise on sn. |
| `ellipke` | ✅ | 0.760 |  | 1.31× | OK | Sig: [K, E] = ellipke(M). Complete elliptic K, E. 50 iters. SAVE on K. |
| `erf` | ✅ | 9.174 | 0.28× | 0.78× | OK | smoke-test (already implemented). N=1e6, mean over 10 iters. |
| `erfc` | ✅ | 12.800 | 0.21× | 0.84× | OK | Sig: Y = erfc(X). 1M-pt sweep. 20 iters. Element-wise SAVE. |
| `erfcinv` | ✅ | 46.123 | 0.08× | 0.28× | OK | Sig: Y = erfcinv(X). 1M-pt sweep on (0,2). 20 iters. Element-wise SAVE. |
| `erfcx` | ✅ | 8.660 | 0.21× | 0.45× | OK | Sig: Y = erfcx(X) = exp(X^2)*erfc(X). 1M-pt. 20 iters. Element-wise SAVE. |
| `erfinv` | ✅ | 45.836 | 0.08× | 0.28× | OK | Sig: Y = erfinv(X). 1M-pt sweep avoiding singularities. 20 iters. Element-wise SAVE. |
| `expint` | ✅ | 4.783 | 3.12× | 8.57× | OK | Sig: Y = expint(X). 100k-pt on (0,50]. 20 iters. Element-wise SAVE. |
| `gamma` | ✅ | 1.306 | 0.28× | 0.84× | OK | Sig: Y = gamma(X). 100k-pt sweep on (0,10]. 20 iters. Element-wise SAVE. |
| `gammainc` | ✅ | 0.146 | 1.40× | 2.77× | OK | Sig: P = gammainc(X, A). Regularized lower gamma at X=2.5. 100 iters. |
| `gammaincinv` | ✅ | 1.725 | 1.18× | 23.31× | OK | Inverse regularized gamma over 5k probability points, a=2.5. 20 iters, element-wise. |
| `gammaln` | ✅ | 3.523 | 0.09× | 0.24× | OK | Sig: Y = gammaln(X). 100k-pt sweep on (0,100]. 20 iters. Element-wise SAVE. |
| `legendre` | ✅ | 0.039 | 12.03× | 6.21× | OK | Sig: P = legendre(N, X). N=4, 1k pts. 20 iters. SAVE on (n+1)x1000 matrix. |
| `psi` | ✅ | 0.689 | 0.81× | 1.05× | OK | Sig: Y = psi(X). 100k-pt sweep on positive domain. 20 iters. Element-wise SAVE. |

### Discrete Math

**Namespace:** builtin — 10 ✅ + 0 ⚠️ / 11 = 90%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `factor` | ✅ | 0.000 | 12455.93× | 893270.48× | MISMATCH | Sig: F = factor(N). Sum of #factors for 1..1000. 100 iters. |
| `factorial` | ✅ | 0.000 | 39.29× | 39.98× | OK | Sig: Y = factorial(N). N=0:20. 1k iters. Element-wise SAVE. |
| `gcd` | ✅ | 0.012 | 9.98× | 3.05× | OK | Sig: G = gcd(A, B). 1k-pt vector pair. 100 iters. Element-wise SAVE. |
| `isprime` | ✅ | 0.132 | 3.35× | 46.77× | OK | Sig: TF = isprime(X). 1..10000. 20 iters. Element-wise SAVE on logical. |
| `lcm` | ✅ | 0.014 | 11.69× | 6.69× | OK | Sig: L = lcm(A, B). 1k-pt vector pair. 100 iters. Element-wise SAVE. |
| `matchpairs` | ❌ |  |  |  | N/A | Sig: M = matchpairs(C, COST_NON). Hungarian-style 3×4. 1000 iters. |
| `nchoosek` | ✅ | 0.007 | 28.74× | 4.32× | OK | Sig: C = nchoosek(N, K). N=30, K=0:30 via for-loop (arrayfun-wrap broken in numkit, see BUGS.md #11). 1 iter. |
| `perms` | ✅ | 0.003 | 38.05× | 3.05× | OK | Sig: P = perms(V). 6! = 720 perms. 100 iters. |
| `primes` | ✅ | 0.286 | 1.00× | 2.30× | OK | Sig: P = primes(N). N=100000. 50 iters. Element-wise SAVE. |
| `rat` | ✅ | 0.001 | 96.24× |  | MISMATCH | Sig: S = rat(X, TOL). Continued frac of pi. 1000 iters. |
| `rats` | ✅ | 0.001 | 28.82× |  | MISMATCH | Sig: S = rats(X). Continued frac as char. 10000 iters. |

### Polynomials

**Namespace:** builtin — 10 ✅ + 0 ⚠️ / 12 = 83%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `poly` | ✅ | 0.000 | 84.89× | 164.01× | OK | Sig: P = poly(R). Roots → polynomial coefficients. 10000 iters. |
| `polyder` | ✅ | 0.001 | 79.15× | 34.66× | OK | Sig: K = polyder(P). Deterministic 100-coef poly. 1000 iters. Element-wise SAVE. |
| `polydiv` | ✅ | 0.000 |  | 73.27× | OK | Sig: [Q, R] = polydiv(U, V). Polynomial div via deconv. 10000 iters. |
| `polyeig` | ❌ |  |  |  |  | poly eig |
| `polyfit` | ✅ | 0.054 | 0.92× | 1.53× | OK | Sig: P = polyfit(X, Y, N). Deterministic 1k pts (sin), 5th-order fit. 100 iters. tol=1e-9 (LSQ residual noise). |
| `polyint` | ✅ | 0.001 | 15.55× | 28.71× | OK | Sig: P_int = polyint(P). Deterministic 100-coef. 1000 iters. Element-wise SAVE. |
| `polyval` | ✅ | 3.318 | 0.81× | 7.69× | OK | Sig: Y = polyval(P, X). 4th-order poly on 1M pts. 20 iters. Element-wise SAVE. |
| `polyvalm` | ✅ | 0.001 | 35.94× | 53.27× | OK | Sig: Y = polyvalm(P, A). Matrix poly eval x^2-3x+2. 10000 iters. |
| `residue` | ❌ |  |  |  |  | partial-fraction |
| `roots` | ✅ | 0.001 | 21.54× | 38.26× | OK | Sig: R = roots(P). 4th-order poly with real roots {1,2,3,4}. 1000 iters. SAVE on sorted real parts. |
| `padecoef` | ✅ | 0.000 | 3.03× | 158.05× | OK | Pade(10,10) of e^{-1.5s} numerator coefficients. 10k iters. Octave's padecoef (control pkg) uses a different normalization — comparison reference is MATLAB. |

### Random Number Generation

**Namespace:** builtin — 5 ✅ + 0 ⚠️ / 6 = 83%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `rand` | ✅ | 6.807 | 0.51× | 0.81× | OK | Sig: A = rand(M,N). 1k×1k uniform. 100 iters. Custom fp (RNG diffs). |
| `randi` | ✅ | 6.307 | 0.75× | 1.72× | OK | Sig: A = randi(IMAX, M, N). 1k×1k uniform-int. 100 iters. |
| `randn` | ✅ | 15.280 | 0.28× | 0.45× | OK | Sig: A = randn(M,N). 1k×1k normal. 100 iters. RNG-stream-diff fp. |
| `randperm` | ✅ | 0.707 | 2.73× | 1.08× | OK | Sig: P = randperm(N). 100k random permutation. 100 iters. |
| `randstream` | ❌ |  |  |  |  |  |
| `rng` | ✅ | 0.001 | 33.99× | 33.95× | MISMATCH | Sig: rng(SEED). After seeding, rand() should be deterministic. 1000 iters. |

### Interpolation

**Namespace:** builtin — 11 ✅ + 0 ⚠️ / 18 = 61%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `griddata` | ❌ |  |  |  |  |  |
| `griddatan` | ❌ |  |  |  |  |  |
| `griddedinterpolant` | ❌ |  |  |  |  |  |
| `interp1` | ✅ | 0.123 | 1.16× | 8.36× | OK | Sig: VQ = interp1(X, V, XQ). 100 → 10k linear interp. 100 iters. |
| `interp2` | ✅ |  |  |  | N/A | Sig: Vq = interp2(X,Y,V,Xq,Yq). 50x50 → 200x200 bilinear. 50 iters. |
| `interp3` | ✅ |  |  |  | N/A | Sig: Vq = interp3(X,Y,Z,V,Xq,Yq,Zq). 20³ → 50³ trilinear. 10 iters. |
| `interpft` | ✅ | 0.012 | 2.33× | 16.15× | OK | 256-pt band-limited signal interpolated to 1024 points. 200 iters, element-wise. |
| `interpn` | ✅ |  |  |  | N/A | Sig: Vq = interpn(...) N-D interp. 20³ → 50³. 10 iters. |
| `makima` | ❌ |  |  |  |  |  |
| `meshgrid` | ✅ | 11.413 | 0.21× | 0.40× | OK | Sig: [X,Y] = meshgrid(x,y). 1k×1k grid. 50 iters. SAVE on X. |
| `mkpp` | ✅ | 0.000 | 6.87× | 56.79× | OK | Sig: PP = mkpp(BREAKS, COEFS). 4-piece linear. 10000 iters. |
| `ndgrid` | ✅ |  |  |  | N/A | Sig: [X,Y] = ndgrid(x,y). 1k×1k grid. 100 iters. |
| `pchip` | ✅ | 0.016 | 15.97× | 29.07× | OK | Sig: yq = pchip(x, v, xq). 50 → 1000 PCHIP. 100 iters. |
| `ppval` | ✅ |  |  |  | N/A | Sig: V = ppval(PP, X). 50-knot spline → 10k pts. 100 iters. |
| `scatteredinterpolant` | ❌ |  |  |  |  |  |
| `spline` | ✅ | 0.017 | 22.81× | 37.93× | OK | Sig: yq = spline(x, v, xq). 50 → 1000 cubic spline. 100 iters. |
| `unmkpp` | ✅ | 0.000 | 3.90× | 46.01× | OK | Sig: [BR,CF,L,K] = unmkpp(PP). Inverse mkpp. 10000 iters. |

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
| `condest` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `dissect` | ❌ |  |  |  |  | **deferred — libs/sparse** |
| `dmperm` | ❌ |  |  |  |  |  |
| `eigs` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `equilibrate` | ❌ |  |  |  |  |  |
| `etree` | ❌ |  |  |  |  | **deferred — libs/sparse** |
| `etreeplot` | ❌ |  |  |  |  | **deferred — libs/sparse** |
| `find` | ✅ | 2.383 | 0.23× | 0.06× | OK | Sig: K = find(X). 1M-pt logical, ~77k matches. 100 iters. |
| `full` | ❌ |  |  |  |  | **deferred — libs/sparse** |
| `gmres` | ❌ |  |  |  |  | **deferred — libs/sparse** |
| `gplot` | ❌ |  |  |  |  | **deferred — libs/sparse** |
| `ichol` | ❌ |  |  |  |  |  |
| `ilu` | ❌ |  |  |  |  |  |
| `issparse` | ❌ |  |  |  | N/A | Sig: TF = issparse(X). 100k iters. |
| `lsqr` | ❌ |  |  |  |  | **deferred — libs/sparse** |
| `minres` | ❌ |  |  |  |  | **deferred — libs/sparse** |
| `nnz` | ✅ | 0.142 | 0.23× | 1.39× | OK | Sig: N = nnz(X). 1M-pt count. 1000 iters. |
| `nonzeros` | ✅ | 1.245 | 0.48× | 0.80× | OK | Sig: V = nonzeros(X). 1M-pt extract non-zero (logical→double cast for .*). 100 iters. |
| `normest` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `nzmax` | ❌ |  |  |  |  | **deferred — libs/sparse** |
| `pcg` | ❌ |  |  |  |  | **deferred — libs/sparse** |
| `qmr` | ❌ |  |  |  |  | **deferred — libs/sparse** |
| `randperm` | ✅ | 0.707 | 2.73× | 1.08× | OK | Sig: P = randperm(N). 100k random permutation. 100 iters. |
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
| `clear` | ✅ |  |  |  |  |  |
| `clearvars` | ✅ |  |  |  |  |  |
| `disp` | ✅ |  |  |  | N/A | Sig: disp(X) — captured via evalc. 1000 iters. |
| `formatteddisplaytext` | ✅ |  |  |  | N/A | Sig: S = formattedDisplayText(X). 1000 iters. |
| `load` | ✅ |  |  |  |  |  |
| `openvar` | ❌ |  |  |  |  | IDE |
| `save` | ✅ |  |  |  |  |  |
| `who` | ✅ |  |  |  |  |  |
| `whos` | ✅ |  |  |  |  |  |
| `workspacebrowser` | ❌ |  |  |  |  |  |

### Error Handling (basic)

**Namespace:** builtin — 4 ✅ + 0 ⚠️ / 6 = 66%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `assert` | ✅ | 0.000 | 2.92× |  | OK | Sig: assert(COND). Pass-case. 100k iters. |
| `error` | ✅ |  |  |  |  |  |
| `lastwarn` | ✅ | 0.000 | 3.42× |  | OK | Sig: msg = lastwarn. Read last warning. 100k iters. |
| `oncleanup` | ❌ |  |  |  |  |  |
| `try` | ✅ |  |  |  |  | keyword (`try/catch`) |
| `warning` | ✅ | 0.000 | 38.01× |  | OK | Sig: warning(ID, MSG). Side-effect tested via lastwarn. 10000 iters. |

### Exception Handling

**Namespace:** builtin (keyword + class) — 2 ✅ + 0 ⚠️ / 2 = 100%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `mexception` | ✅ |  |  |  |  | MATLAB exception class — registered as `MException` |
| `try` | ✅ |  |  |  |  | keyword (`try/catch`) |

## Communications

### Modulation

**Namespace:** `comm.mod.*` — 13 ✅ + 0 ⚠️ / 29 = 45%

Function-form modulators / demodulators. The `comm.PSKModulator` /
`comm.QAMModulator` / `comm.OFDMModulator` System Object family is
intentionally omitted, along with `constellation` (object method) and
`showResourceMapping` (display).

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `genqammod` | ❌ |  |  |  |  | generic QAM |
| `genqamdemod` | ❌ |  |  |  |  |  |
| `modnorm` | ✅ |  |  |  | OK | avpow / peakpow scaling |
| `pammod` | ✅ |  |  |  | OK | M-ary PAM, gray (default) / bin |
| `pamdemod` | ✅ |  |  |  | OK |  |
| `qammod` | ✅ |  |  |  | OK | rectangular Gray-coded QAM, optional UnitAveragePower |
| `qamdemod` | ✅ |  |  |  | OK |  |
| `apskmod` | ❌ |  |  |  |  | amplitude-phase-shift keying |
| `apskdemod` | ❌ |  |  |  |  |  |
| `mil188qammod` | ❌ |  |  |  |  | MIL-STD-188 QAM |
| `mil188qamdemod` | ❌ |  |  |  |  |  |
| `mskmod` | ❌ |  |  |  |  | minimum-shift keying |
| `mskdemod` | ❌ |  |  |  |  |  |
| `fskmod` | ✅ |  |  |  | OK | M-ary FSK; cont (default) and discont phase |
| `fskdemod` | ✅ |  |  |  | OK | per-symbol energy detection |
| `ofdmmod` | ✅ |  |  |  | OK | IFFT-based with cyclic prefix |
| `ofdmdemod` | ✅ |  |  |  | OK | drops CP then FFT |
| `dpskmod` | ✅ |  |  |  | OK | differential PSK |
| `dpskdemod` | ✅ |  |  |  | OK | phase-difference decoder |
| `pskmod` | ✅ |  |  |  | OK | M-ary PSK; gray (default) / bin orderings |
| `pskdemod` | ✅ |  |  |  | OK | nearest-phase decision |
| `ammod` | ❌ |  |  |  |  | amplitude modulation (analog) |
| `amdemod` | ❌ |  |  |  |  |  |
| `fmmod` | ❌ |  |  |  |  | frequency modulation |
| `fmdemod` | ❌ |  |  |  |  |  |
| `pmmod` | ❌ |  |  |  |  | phase modulation |
| `pmdemod` | ❌ |  |  |  |  |  |
| `ssbmod` | ❌ |  |  |  |  | single-sideband |
| `ssbdemod` | ❌ |  |  |  |  |  |

### Sources, Sinks, and Signal Operations

**Namespace:** `comm.signals.*` — 0 ✅ + 0 ⚠️ / 17 = 0%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `randerr` | ❌ |  |  |  |  | random binary error patterns |
| `randsrc` | ❌ |  |  |  |  | random matrix from given alphabet |
| `wgn` | ✅ |  |  |  | OK | dBW / dBm / linear power; real or complex |
| `biterr` | ❌ |  |  |  |  | bit-error count |
| `symerr` | ❌ |  |  |  |  | symbol-error count |
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
| `convertSNR` | ✅ |  |  |  | OK | Eb/No ↔ Es/No conversion via BitsPerSymbol |

### Source Coding

**Namespace:** `comm.source_coding.*` — 0 ✅ + 0 ⚠️ / 11 = 0%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `arithenco` | ❌ |  |  |  |  | arithmetic encoder |
| `arithdeco` | ❌ |  |  |  |  |  |
| `compand` | ❌ |  |  |  |  | μ-law / A-law companding |
| `dpcmenco` | ❌ |  |  |  |  | differential PCM encoder |
| `dpcmdeco` | ❌ |  |  |  |  |  |
| `dpcmopt` | ❌ |  |  |  |  | optimise predictor + partition |
| `huffmandict` | ❌ |  |  |  |  | build Huffman code table |
| `huffmanenco` | ❌ |  |  |  |  |  |
| `huffmandeco` | ❌ |  |  |  |  |  |
| `lloyds` | ❌ |  |  |  |  | Lloyd-Max scalar quantiser |
| `quantiz` | ❌ |  |  |  |  | apply quantisation table |

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
| `dftmtx` | ✅ |  |  |  |  | already in core / FFT |
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
| `gaussdesign` | ✅ | 0.004 | 250.45× |  | OK | Sig: h = gaussdesign(BT, span, sps). Gaussian FIR pulse-shaping filter, length span*sps+1, sum-normalized to 1. Element-wise SAVE. |
| `rcosdesign` | ✅ |  |  |  | OK | raised-cosine ('normal') and root-raised-cosine ('sqrt'); unit-energy normalised |
| `rectpulse` | ✅ | 0.003 | 110.13× |  | OK | Sig: y = rectpulse(x, n). Each sample of x repeats n times. 5x1 column → 20x1; n=4. Element-wise SAVE. |
| `intdump` | ✅ | 0.002 | 219.44× |  | OK | Sig: y = intdump(x, n). Inverse of rectpulse — averages each n consecutive samples along the leading non-singleton dim. 12x1 column → 4x1 averages [2;5;8;11]. Element-wise SAVE. |
| `mlseeq` | ❌ |  |  |  |  | maximum-likelihood sequence equaliser |
| `ofdmEqualize` | ❌ |  |  |  |  | OFDM zero-forcing / MMSE equalise |
| `blkdiagbfweights` | ❌ |  |  |  |  | block-diagonalisation BF weights |
| `ofdmPrecode` | ❌ |  |  |  |  | OFDM precoding |

### RF and Channel Impairments

**Namespace:** `comm.rf.*` — 4 ✅ + 0 ⚠️ / 10 = 40%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `awgn` | ✅ |  |  |  | OK | adds Gaussian noise at given SNR (real or complex) |
| `bsc` | ✅ |  |  |  | OK | binary symmetric channel; per-bit Bernoulli flip |
| `rayleighchan` | ✅ |  |  |  | OK | iid frequency-flat Rayleigh, E[\|h\|²]=1 |
| `ricianchan` | ✅ |  |  |  | OK | Rician with K-factor; E[\|h\|²]=1 regardless of K |
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
| `berawgn` | ✅ |  |  |  | OK | psk/qam/pam/fsk/dpsk; Gray-coded BER approximation |
| `bercoding` | ❌ |  |  |  |  | with coding gain |
| `berconfint` | ✅ | 0.005 | 258.02× |  | OK | Sig: [ber, ci] = berconfint(numErrs, numBits[, level]). Clopper-Pearson exact binomial CI via betaincinv. Edge cases: k=0 (lo=0), k=n (hi=1). |
| `berfading` | ❌ |  |  |  |  | over Rayleigh / Rician fading |
| `berfit` | ❌ |  |  |  |  | curve fit BER vs Eb/No |
| `bersync` | ❌ |  |  |  |  | with imperfect sync |
| `semianalytic` | ❌ |  |  |  |  | semi-analytic BER |
| `marcumq` | ✅ |  |  |  | OK | Marcum Q via integral form (m=1 closed-form) |
| `qfunc` | ✅ |  |  |  | OK | 0.5·erfc(x/√2) |
| `qfuncinv` | ✅ |  |  |  | OK | √2·erfcinv(2p) via Acklam approx |
| `noisebw` | ✅ |  |  |  | OK | numerical |H(jω)|² integration over 0..π |

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
| `tf` | ✅ |  |  |  | OK | transfer function — struct {kind='tf', num, den, Ts} |
| `zpk` | ✅ |  |  |  | OK | zero-pole-gain — struct {kind='zpk', z, p, k, Ts} |
| `ss` | ✅ |  |  |  | OK | state-space — struct {kind='ss', A, B, C, D, Ts} |
| `frd` | ✅ | 0.003 | 828.82× | 103.57× | OK | Sig: sys = frd(response, frequency[, Ts]). Frequency-response data model. Stores resp + freq as column vectors. Companion frdata(sys) extracts both. |
| `dss` | ❌ |  |  |  |  | descriptor state-space (E·xdot = Ax + Bu) |
| `filt` | ✅ | 0.003 | 353.66× | 256.47× | OK | Sig: f = filt(num, den[, Ts]). Discrete tf with z^-1 ordering convention. Default Ts = -1 (unspecified discrete). Internally same coefficients as tf(num, den, Ts) — the variable convention only affects display. |
| `pid` | ❌ |  |  |  |  | parallel-form PID controller |
| `pid2` | ❌ |  |  |  |  | 2-DOF PID |
| `pidstd` | ❌ |  |  |  |  | standard-form PID |
| `pidstd2` | ❌ |  |  |  |  | 2-DOF standard PID |
| `rss` | ❌ |  |  |  |  | random stable continuous SS |
| `drss` | ❌ |  |  |  |  | random stable discrete SS |
| `tfdata` | ✅ | 0.003 | 312.76× | 289.94× | OK | Sig: [num, den] = tfdata(sys[, 'v']). Extracts numerator/denominator coefficient vectors. With 'v' returns numeric row vectors; pads num with leading zeros so length matches den. Accepts tf / zpk / ss inputs. |
| `zpkdata` | ✅ | 0.003 | 472.67× | 393.13× | OK | Sig: [z, p, k] = zpkdata(sys[, 'v']). Extracts zeros / poles / gain. With 'v' returns column vectors (z, p) and scalar (k). Accepts tf / zpk / ss inputs. |
| `ssdata` | ✅ | 0.003 | 497.68× | 97.80× | OK | Sig: [A, B, C, D] = ssdata(sys). Extracts state-space matrices. Accepts tf / zpk / ss inputs (the former two get realised via tf2ss controllable canonical form). |
| `frdata` | ✅ |  |  |  | OK | column-vector form (`'v'` flag accepted; SISO 1×1×N tensor not modeled). Tested via `frd` spec. |
| `dssdata` | ❌ |  |  |  |  | extract A/B/C/D/E |
| `piddata` | ❌ |  |  |  |  |  |
| `pidstddata` | ❌ |  |  |  |  |  |

### Model Properties

**Namespace:** `control.props.*` — 11 ✅ + 0 ⚠️ / 11 = **100%**

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `isct` | ✅ |  |  |  | OK | true when Ts == 0 |
| `isdt` | ✅ |  |  |  | OK | true when Ts > 0 or Ts == -1 |
| `isproper` | ✅ |  |  |  | OK | tf: numel(num)≤numel(den); zpk: |z|≤|p|; ss: true |
| `issiso` | ✅ |  |  |  | OK | tf/zpk: true; ss: 1-col B and 1-row C |
| `isstable` | ✅ |  |  |  | OK | qualified-only (`control.props.isstable`) — `compat.isstable` is libs/signal coefficient form |
| `isstatic` | ✅ |  |  |  | OK | true when order(sys) == 0 (pure gain) |
| `order` | ✅ |  |  |  | OK | tf: max(deg); zpk: max(numel); ss: rows(A) |
| `pole` | ✅ |  |  |  | OK | tf: roots(den); ss: roots(charpoly via Faddeev) |
| `zero` | ✅ |  |  |  | OK | tf/zpk; ss form raises NYI |
| `tzero` | ✅ |  |  |  | OK | SISO alias for zero(sys); raises NYI on MIMO |
| `damp` | ✅ |  |  |  | OK | [wn, zeta, p]; discrete via s = ln(z)/Ts |

### Model Conversion & Reduction

**Namespace:** `control.convert.*` — 3 ✅ + 0 ⚠️ / 18 = 17%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `c2d` | ✅ |  |  |  | OK | ZOH (Van Loan expm) + Tustin; preserves tf/zpk/ss kind |
| `c2dOptions` | ❌ |  |  |  |  |  |
| `d2c` | ✅ |  |  |  | OK | Tustin only (ZOH would need matrix log) |
| `d2cOptions` | ❌ |  |  |  |  |  |
| `d2d` | ❌ |  |  |  |  | resample discrete |
| `d2dOptions` | ❌ |  |  |  |  |  |
| `ss2ss` | ✅ | 0.006 | 313.49× | 96.25× | OK | Sig: sys2 = ss2ss(sys, T). Similarity transform A' = T·A·T⁻¹, B' = T·B, C' = C·T⁻¹, D' = D. Inline LU (partial pivoting) for T⁻¹. |
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
| `ss2tf` | ✅ | 0.001 | 98.47× | 3045.82× | OK | Sig: [NUM,DEN] = ss2tf(A,B,C,D). State-space → transfer fn. 10000 iters. |

### Interconnections

**Namespace:** `control.connect.*` — 3 ✅ + 0 ⚠️ / 7 = 43%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `feedback` | ✅ |  |  |  | OK | T = G·d_H / (d_G·d_H − sign·n_G·n_H); default sign=−1 |
| `series` | ✅ |  |  |  | OK | tf form: num/den = conv(num1,num2)/conv(den1,den2) |
| `parallel` | ✅ |  |  |  | OK | tf form: (n1·d2 + n2·d1) / (d1·d2) |
| `connect` | ❌ |  |  |  |  | name-based interconnect |
| `append` | ✅ |  |  |  |  | block-diagonal stack |
| `lft` | ❌ |  |  |  |  | linear fractional transform |
| `sumblk` | ❌ |  |  |  |  | summation block (for connect) |

### Time and Frequency Response

**Namespace:** `control.response.*` — 9 ✅ + 0 ⚠️ / 19 = 47%

`*plot` variants intentionally dropped — they're display-only mirrors
of the numeric functions (which already return data when called with
output args).

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `step` | ✅ |  |  |  | OK | ZOH discretisation via Padé(6/6) expm + scaling/squaring |
| `stepinfo` | ✅ |  |  |  | OK | RiseTime / SettlingTime / Overshoot / Peak / etc. struct |
| `impulse` | ✅ |  |  |  | OK | continuous: x(0+) = B; discrete: u[0]=1 |
| `initial` | ❌ |  |  |  |  | response from initial state |
| `lsim` | ✅ |  |  |  | OK | uniform-grid one-shot expm; non-uniform per-step |
| `lsiminfo` | ❌ |  |  |  |  |  |
| `gensig` | ❌ |  |  |  |  | input signal generator |
| `covar` | ❌ |  |  |  |  | output covariance under stochastic input |
| `bode` | ✅ |  |  |  | OK | Horner H(jω) eval, phase unwrap |
| `bodemag` | ❌ |  |  |  |  | magnitude only |
| `nyquist` | ✅ |  |  |  | OK | re/im of H(jω) on grid |
| `nichols` | ❌ |  |  |  |  |  |
| `sigma` | ❌ |  |  |  |  | singular-value response |
| `freqresp` | ✅ |  |  |  | OK | complex H column on user grid; default log-spaced |
| `evalfr` | ✅ |  |  |  | OK | scalar H at one frequency, continuous + discrete |
| `dcgain` | ✅ |  |  |  | OK | continuous: H(0); discrete: H(z=1) |
| `bandwidth` | ❌ |  |  |  |  | -3 dB bandwidth |
| `getPeakGain` | ❌ |  |  |  |  | H∞ |
| `getGainCrossover` | ❌ |  |  |  |  |  |

### Stability and Margins

**Namespace:** `control.margin.*` — 3 ✅ + 0 ⚠️ / 6 = 50%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `margin` | ✅ |  |  |  | OK | linear interp on bode grid; returns Gm/Pm/Wcg/Wcp |
| `allmargin` | ❌ |  |  |  |  | all stability margins |
| `db2mag` | ✅ |  |  |  |  |  |
| `mag2db` | ✅ |  |  |  |  |  |
| `pzmap` | ✅ |  |  |  | OK | composes pole(sys) + zero(sys) into a 2-output |
| `rlocus` | ✅ |  |  |  | OK | sweep gain, roots(den + k·num); composes with feedback to 0 ULP |

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
| `place` | ✅ |  |  |  | OK | SISO Ackermann — also exposed as `acker` |
| `estim` | ❌ |  |  |  |  | steady-state estimator (Kalman) |
| `kalman` | ❌ |  |  |  |  | continuous-time Kalman gain |
| `kalmd` | ❌ |  |  |  |  | discrete Kalman from continuous plant |
| `reg` | ❌ |  |  |  |  | full-state controller + observer |
| `ctrb` | ✅ |  |  |  | OK | [B, AB, A²B, …, A^(n−1)B]; (A,B) or (sys) form |
| `obsv` | ✅ |  |  |  | OK | [C; CA; CA²; …; CA^(n−1)]; (A,C) or (sys) form |
| `gram` | ❌ |  |  |  |  | controllability/observability gramian |
| `ctrbf` | ❌ |  |  |  |  | controllable-form decomposition |
| `obsvf` | ❌ |  |  |  |  | observable-form decomposition |

### Matrix Equations

**Namespace:** `control.matrixeq.*` — 2 ✅ + 0 ⚠️ / 8 = 25%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `lyap` | ✅ |  |  |  | OK | A·X + X·Aᵀ + Q = 0 via Kronecker n²-system |
| `lyapchol` | ❌ |  |  |  |  | factored continuous Lyapunov |
| `dlyap` | ✅ |  |  |  | OK | A·X·Aᵀ − X + Q = 0 via Kronecker n²-system |
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
| `csapi` | ✅ | 0.004 | 480.48× |  | OK | Sig: pp = csapi(x, y). Not-a-knot cubic spline interpolation. Returns pp-form spline of order 4 that passes through all (x,y) and is C² with continuous third derivative at x(2) and x(end-1). |
| `csaps` | ❌ |  |  |  |  | cubic smoothing spline |
| `cscvn` | ❌ |  |  |  |  | natural cubic curve through points |
| `rscvn` | ❌ |  |  |  |  | rational cubic curve |
| `spapi` | ❌ |  |  |  |  | B-spline interpolation |
| `spaps` | ❌ |  |  |  |  | smoothing spline (penalised) |
| `spap2` | ❌ |  |  |  |  | least-squares spline fit |
| `spcrv` | ❌ |  |  |  |  | uniform B-spline curve |
| `tpaps` | ❌ |  |  |  |  | thin-plate smoothing spline (2-D) |
| `ppmak` | ✅ | 0.004 | 530.84× |  | OK | Sig: pp = ppmak(breaks, coefs[, d]). Piecewise-polynomial constructor. Pair with fnval. Univariate-only (d=1) tested here. |
| `rpmak` | ❌ |  |  |  |  | rational pp form |
| `rsmak` | ❌ |  |  |  |  | rational spline |
| `spmak` | ❌ |  |  |  |  | B-spline form constructor |
| `stmak` | ❌ |  |  |  |  | stform constructor (2-D scattered) |
| `fn2fm` | ❌ |  |  |  |  | convert between spline forms |
| `fnbrk` | ✅ | 0.003 | 209.81× |  | OK | Sig: out = fnbrk(pp, part). Extract a named part from a pp-form spline. Supports {breaks, coefs, pieces|l, order|k, dim|d, form}. |
| `fnchg` | ❌ |  |  |  |  | change spline properties |
| `fncmb` | ✅ | 0.003 | 363.28× |  | OK | Sig: pp = fncmb(pp1, c) | fncmb(c, pp1) | fncmb(pp1, c1, pp2, c2). Linear combination of pp-form splines on shared breaks. Pure coef arithmetic. |
| `fnder` | ✅ | 0.004 | 482.37× |  | OK | Sig: dpp = fnder(pp[, order]). Differentiate pp-form spline `order` times. Each piece's polynomial is independently differentiated; result has order = K − order. |
| `fndir` | ❌ |  |  |  |  | directional derivative |
| `fnint` | ✅ | 0.003 | 538.67× |  | OK | Sig: ipp = fnint(pp). Antiderivative of pp-form spline; integration constant chosen so that integral = 0 at the first break and is continuous across breaks. |
| `fnjmp` | ❌ |  |  |  |  | jump value at discontinuities |
| `fnmin` | ❌ |  |  |  |  | min of spline |
| `fnplt` | ❌ |  |  |  |  | display |
| `fnrfn` | ❌ |  |  |  |  | refine knots |
| `fntlr` | ❌ |  |  |  |  | Taylor coefficients |
| `fnval` | ✅ |  |  |  |  | evaluate at points |
| `fnxtr` | ❌ |  |  |  |  | extrapolate |
| `fnzeros` | ❌ |  |  |  |  | zeros of spline |
| `bkbrk` | ❌ |  |  |  |  | break-and-coefs |
| `slvblk` | ❌ |  |  |  |  | solve almost-block-diagonal system |
| `spcol` | ❌ |  |  |  |  | B-spline collocation matrix |
| `stcol` | ❌ |  |  |  |  | stform collocation matrix |
| `subplus` | ✅ | 0.002 | 70.25× |  | OK | Sig: y = subplus(x). Truncated power: max(x, 0) elementwise. NaN passes through. |
| `aptknt` | ❌ |  |  |  |  | append knots for spline of order k |
| `augknt` | ✅ | 0.003 | 189.77× |  | OK | Sig: out = augknt(knots, k). Endpoint multiplicity-k augmentation; size = N + 2(k-1). |
| `aveknt` | ✅ | 0.003 | 106.08× |  | OK | Sig: y = aveknt(t, k). Greville sites: y(i) = mean(t(i+1)..t(i+k-1)). Result length = length(t) - k. |
| `brk2knt` | ✅ | 0.006 | 58.62× |  | OK | Sig: knots = brk2knt(breaks, mults). Replicate each break by its multiplicity. |
| `chbpnt` | ❌ |  |  |  |  | Chebyshev sites |
| `knt2brk` | ✅ | 0.004 | 81.02× |  | OK | Sig: [breaks, mults] = knt2brk(knots). Inverse of brk2knt: distinct knots + multiplicities. |
| `newknt` | ❌ |  |  |  |  | distribute knots on equidistribution |
| `optknt` | ❌ |  |  |  |  | optimal knot distribution |
| `smooth` | ❌ |  |  |  |  | data smoothing (already partially in core) |
| `datastats` | ✅ | 0.003 | 493.04× |  | OK | Sig: s = datastats(x). Returns struct {num, max, min, mean, median, range, std} for the column-vector input. Sample std (N-1). |
| `prepareCurveData` | ✅ | 0.004 | 467.69× |  | OK | Sig: [xo, yo[, wo]] = prepareCurveData(x, y[, w]). Strips rows where any of x, y, w is NaN/Inf; returns column vectors. w == 0 rows are KEPT (only finiteness matters). |
| `prepareSurfaceData` | ✅ | 0.004 | 500.11× |  | OK | Sig: [xo, yo, zo] = prepareSurfaceData(X, Y, Z). Linearises (column-major) and drops rows where any of x, y, z is NaN/Inf. Returns column vectors. |
| `quad2d` | ❌ |  |  |  |  | 2-D quadrature (also in core) |

## Graphics

### Line Plots

**Namespace:** `graphics.line.*` — 2 ✅ + 0 ⚠️ / 12 = 16%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `area` | ❌ |  |  |  |  |  |
| `errorbar` | ❌ |  |  |  |  |  |
| `fimplicit` | ❌ |  |  |  |  |  |
| `fplot` | ❌ |  |  |  |  |  |
| `fplot3` | ❌ |  |  |  |  |  |
| `loglog` | ❌ |  |  |  |  |  |
| `plot` | ✅ |  |  |  |  |  |
| `plot3` | ❌ |  |  |  |  | 3-D |
| `semilogx` | ❌ |  |  |  |  |  |
| `semilogy` | ❌ |  |  |  |  |  |
| `stackedplot` | ❌ |  |  |  |  |  |
| `stairs` | ✅ |  |  |  |  |  |

### Polar Plots

**Namespace:** `graphics.polar.*` — 3 ✅ + 0 ⚠️ / 19 = 15%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `compassplot` | ❌ |  |  |  |  |  |
| `fpolarplot` | ❌ |  |  |  |  |  |
| `polaraxes` | ❌ |  |  |  |  |  |
| `polarbubblechart` | ❌ |  |  |  |  |  |
| `polarhistogram` | ❌ |  |  |  |  |  |
| `polarplot` | ✅ |  |  |  |  |  |
| `polarregion` | ❌ |  |  |  |  |  |
| `polarscatter` | ❌ |  |  |  |  |  |
| `radiusregion` | ❌ |  |  |  |  |  |
| `rlim` | ✅ |  |  |  |  |  |
| `rtickangle` | ❌ |  |  |  |  |  |
| `rtickformat` | ❌ |  |  |  |  |  |
| `rticklabels` | ❌ |  |  |  |  |  |
| `rticks` | ❌ |  |  |  |  |  |
| `thetalim` | ✅ |  |  |  |  |  |
| `thetaregion` | ❌ |  |  |  |  |  |
| `thetatickformat` | ❌ |  |  |  |  |  |
| `thetaticklabels` | ❌ |  |  |  |  |  |
| `thetaticks` | ❌ |  |  |  |  |  |

### Contour Plots

**Namespace:** `graphics.contour.*` — 2 ✅ + 0 ⚠️ / 7 = 28%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `clabel` | ❌ |  |  |  |  |  |
| `contour` | ✅ |  |  |  |  |  |
| `contour3` | ❌ |  |  |  |  |  |
| `contourc` | ❌ |  |  |  |  |  |
| `contourf` | ✅ |  |  |  |  |  |
| `contourslice` | ❌ |  |  |  |  |  |
| `fcontour` | ❌ |  |  |  |  |  |

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
| `cylinder` | ✅ | 0.125 | 0.48× | 2.75× | OK | 201-point sin-shaped profile, 50 angular samples. 200 iters. |
| `ellipsoid` | ✅ | 0.094 | 1.00× | 4.19× | OK | 101x101 ellipsoid (1,2,3) center, (4,5,6) semi-axes. 200 iters. |
| `fimplicit3` | ❌ |  |  |  |  |  |
| `fmesh` | ❌ |  |  |  |  |  |
| `fsurf` | ❌ |  |  |  |  |  |
| `hidden` | ❌ |  |  |  |  |  |
| `mesh` | ✅ |  |  |  |  |  |
| `meshc` | ❌ |  |  |  |  |  |
| `meshz` | ❌ |  |  |  |  |  |
| `pcolor` | ✅ |  |  |  |  |  |
| `peaks` | ✅ | 0.365 | 1.71× | 5.05× | OK | 200x200 peaks() surface. 50 iters, element-wise. |
| `ribbon` | ❌ |  |  |  |  |  |
| `sphere` | ✅ | 0.090 | 0.55× | 3.55× | OK | Unit sphere on 101x101 grid. 200 iters, element-wise on Z. |
| `surf` | ✅ |  |  |  |  |  |
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
| `coneplot` | ❌ |  |  |  |  |  |
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
| `imread` | ✅ |  |  |  | OK | PNG/JPG/BMP/TGA/PSD/GIF/HDR/PNM via stb_image |
| `imwrite` | ✅ |  |  |  | OK | PNG/JPG/BMP/TGA via stb_image_write; ext detected from path |
| `imfinfo` | ✅ |  |  |  | OK | stbi_info + magic-byte format sniff + filesystem size |

### Image Type Conversion

**Namespace:** `image.type.*` — 13 ✅ + 0 ⚠️ / 27 = 48%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `adaptthresh` | ✅ | 0.008 |  | 63.86× | MISMATCH | Sig + small deterministic input. Auto-generated for parity sweep. |
| `cmap2gray` | ✅ | 0.003 | 182.30× |  | OK | Sig: gmap = cmap2gray(cmap). N×3 RGB cmap → N×3 grayscale (replicated y across R/G/B), inv(YIQ→RGB) row weights (0.298936/0.587043/0.114021), clipped to [0,1]. MATLAB R2020b+; Octave-image 2.18.2 doesn't ship it. |
| `getrangefromclass` | ✅ | 0.003 | 94.05× | 56.02× | OK | Sig: r = getrangefromclass(I). Returns [intmin intmax] for integer classes; [0 1] for logical/single/double. Always double output. Octave-image has it. |
| `gray2ind` | ✅ | 0.003 | 578.25× | 35.72× | OK | Sig: [ind, map] = gray2ind(I [, n]). Default n=64 (or 2 for logical). uint8 if n<=256 else uint16. Octave-image has gray2ind. |
| `graythresh` | ✅ | 0.005 |  | 133.52× | OK | Sig + small deterministic input. Auto-generated for parity sweep. |
| `grayslice` | ✅ | 0.003 | 204.79× | 60.84× | OK | Sig: G = grayslice(I [, N|V]). Multilevel intensity thresholding. Default N=10. Output uint8 if levels < 256, else double + 1 (1-based). Octave-image has grayslice. |
| `im2bw` | ✅ | 0.003 |  | 52.74× | OK | Sig: BW = im2bw(I, level). Scalar threshold at 0.5 → [0 0 0 1 1 1]. |
| `im2double` | ✅ | 0.003 |  | 20.00× | OK | Sig + small deterministic input. Auto-generated for parity sweep. |
| `im2gray` | ✅ | 0.003 |  |  | N/A | Sig + small deterministic input. Auto-generated for parity sweep. |
| `im2int16` | ✅ |  |  |  | OK | round-then-shift convention |
| `im2single` | ✅ | 0.003 |  | 60.89× | OK | Sig + small deterministic input. Auto-generated for parity sweep. |
| `im2uint16` | ✅ | 0.003 |  | 34.85× | OK | Sig + small deterministic input. Auto-generated for parity sweep. |
| `im2uint8` | ✅ | 0.003 |  | 58.06× | OK | Sig + small deterministic input. Auto-generated for parity sweep. |
| `imbinarize` | ✅ | 0.003 |  | 32.70× | OK | Sig + small deterministic input. Auto-generated for parity sweep. |
| `imquantize` | ✅ | 0.003 |  | 85.18× | OK | Sig + small deterministic input. Auto-generated for parity sweep. |
| `imsplit` | ✅ |  |  |  | OK | split H×W×P volume into P planes (multi-output, byte-perfect copy) |
| `ind2gray` | ❌ |  |  |  |  |  |
| `ind2rgb` | ✅ | 0.003 | 188.96× | 61.49× | OK | Sig: rgb = ind2rgb(idx, map). Float idx 1-based, integer 0-based. Out-of-range clipped. Octave has ind2rgb. |
| `iptnum2ordinal` | ✅ | 0.003 | 73.82× | 193.87× | OK | Sig: ord = iptnum2ordinal(num). 1..20 word form; 21+ digit-suffix. Output is char. Octave-image has iptnum2ordinal. |
| `label2rgb` | ✅ | 0.003 | 714.04× | 168.46× | OK | Sig: RGB = label2rgb(L, cmap [, background]). Caller passes an explicit N-by-3 colormap (we don't yet have the colormap-name / function-handle defaults). Octave-image has label2rgb. |
| `mat2gray` | ✅ | 0.003 |  | 65.69× | OK | Sig + small deterministic input. Auto-generated for parity sweep. |
| `multithresh` | ✅ | 13.193 |  |  | N/A | Sig + small deterministic input. Auto-generated for parity sweep. |
| `otsuthresh` | ✅ | 0.003 |  | 96.59× | OK | Sig + small deterministic input. Auto-generated for parity sweep. |
| `rgb2gray` | ✅ | 0.003 |  | 61.95× | OK | Sig + small deterministic input. Auto-generated for parity sweep. |
| `rgb2ind` | ❌ |  |  |  |  | colour quantize |
| `rgb2lightness` | ❌ |  |  |  |  | L* of CIELAB |
| `demosaic` | ❌ |  |  |  |  | Bayer → RGB |

### Color Space Conversion

**Namespace:** `image.color.*` — 10 ✅ + 0 ⚠️ / 30 = 33%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `chromadapt` | ❌ |  |  |  |  | Bradford/von Kries chromatic adapt |
| `colorangle` | ✅ | 0.003 |  | 121.41× | OK | Sig: ang = colorangle(rgb1, rgb2). Angle in degrees between RGB colours; broadcasts N×3 vs 1×3. Octave-image has colorangle. |
| `deltaE` | ✅ | 0.003 | 435.72× |  | OK | Sig: delE = deltaE(I1, I2[, 'isInputLab', tf]). CIE76 distance in CIELAB. We test the Lab-input path (skips rgb2lab to avoid the known sRGB-vs-linear divergence between numkit and MATLAB). |
| `hsv2rgb` | ✅ | 0.003 |  | 104.82× | OK | Sig + small deterministic input. Auto-generated for parity sweep. |
| `illumgray` | ❌ |  |  |  |  | grey-world illumination |
| `illumpca` | ❌ |  |  |  |  |  |
| `illumwhite` | ❌ |  |  |  |  | white-patch |
| `imapprox` | ❌ |  |  |  |  | reduce indexed-image colors |
| `imcolordiff` | ❌ |  |  |  |  | CIE94/CIEDE2000 |
| `lab2double` | ✅ | 0.003 | 472.91× | 65.47× | OK | Sig: lab_dbl = lab2double(lab). uint8 LAB → double: L *= 100/255, a/b -= 128. Octave-image has lab2double. |
| `lab2rgb` | ✅ | 0.003 |  | 94.01× | MISMATCH | Sig + small deterministic input. Auto-generated for parity sweep. Tol=1e-4: small FP differences from slightly different published color-matrix coefficients (D65 white point variants). |
| `lab2uint16` | ✅ | 0.003 | 428.65× | 15.34× | OK | Sig: lab_u16 = lab2uint16(lab). double LAB → uint16: (L*65280)/100, (a+128)*256, (b+128)*256. NaN → 65535. Octave-image has lab2uint16. |
| `lab2uint8` | ✅ | 0.003 | 379.21× | 68.35× | OK | Sig: lab_u8 = lab2uint8(lab). double LAB → uint8: (L*255)/100, a/b += 128. NaN → 255. Octave-image has lab2uint8. |
| `lab2xyz` | ✅ |  |  |  | OK | CIELAB → XYZ (D65) |
| `lin2rgb` | ✅ | 0.003 | 601.81× |  | OK | Sig: B = lin2rgb(A). Linear → sRGB forward gamma. MATLAB R2025b. Octave-image doesn't ship lin2rgb; harness ranks MATLAB above Octave so OK is expected with octave=N/A. |
| `ntsc2rgb` | ✅ | 0.003 | 313.36× | 93.83× | OK | Sig: rgb = ntsc2rgb(yiq). Inverse of rgb2ntsc 3-sig-fig matrix. Octave-image has ntsc2rgb. |
| `rgb2hsv` | ✅ | 0.003 |  | 54.51× | OK | Sig + small deterministic input. Auto-generated for parity sweep. |
| `rgb2lab` | ✅ | 0.016 | 920.06× | 59.27× | MISMATCH | Verify our rgb2lab matches MATLAB. |
| `rgb2lin` | ✅ | 0.003 | 635.56× |  | OK | Sig: B = rgb2lin(A). sRGB inverse gamma (piecewise linear|^2.4). MATLAB R2025b. Octave-image doesn't ship rgb2lin; harness ranks MATLAB above Octave so OK is expected even with octave=N/A. |
| `rgb2ntsc` | ✅ | 0.003 | 133.74× | 83.13× | OK | Sig: yiq = rgb2ntsc(rgb). Linear matrix; 3-sig-fig from Wikipedia/MATLAB. Octave-image has rgb2ntsc. |
| `rgb2xyz` | ✅ | 0.003 |  | 28.55× | OK | Sig + small deterministic input. Auto-generated for parity sweep. Tol=1e-4: small FP differences from slightly different published color-matrix coefficients (D65 white point variants). |
| `rgb2ycbcr` | ✅ | 0.003 |  | 47.60× | OK | Sig + small deterministic input. Auto-generated for parity sweep. Tol=1e-4: small FP differences from slightly different published color-matrix coefficients (D65 white point variants). |
| `rgbwide2xyz` | ❌ |  |  |  |  | wide-gamut HDR |
| `rgbwide2ycbcr` | ❌ |  |  |  |  |  |
| `whitepoint` | ✅ | 0.005 | 134.32× |  | OK | Sig: wp = whitepoint([illuminant]). 1×3 XYZ tristimulus of CIE reference illuminant. Supports a/c/d50/d55/d65/e/icc; default 'icc'. MATLAB R2025b. Octave-image doesn't ship whitepoint. |
| `xyz2double` | ✅ | 0.002 | 465.36× |  | OK | Sig: xyzd = xyz2double(xyz). uint16 XYZ → double via ICC.1:2001-4 (32768 ↔ 1.0). Double input passthrough. MATLAB R2025b. Octave-image doesn't ship xyz2double. |
| `xyz2lab` | ✅ |  |  |  | OK |  |
| `xyz2rgb` | ✅ | 0.003 |  | 57.01× | MISMATCH | Sig + small deterministic input. Auto-generated for parity sweep. Tol=1e-4: small FP differences from slightly different published color-matrix coefficients (D65 white point variants). |
| `xyz2rgbwide` | ❌ |  |  |  |  |  |
| `xyz2uint16` | ✅ | 0.003 | 399.10× |  | OK | Sig: xyzu16 = xyz2uint16(xyz). Double XYZ → uint16 ICC (round(x*32768) clipped to [0,65535]). MATLAB R2025b. Octave-image doesn't ship xyz2uint16. |
| `ycbcr2rgb` | ✅ | 0.003 |  | 59.93× | MISMATCH | Sig + small deterministic input. Auto-generated for parity sweep. Tol=1e-4: small FP differences from slightly different published color-matrix coefficients (D65 white point variants). |
| `ycbcr2rgbwide` | ❌ |  |  |  |  |  |

### Synthetic Images and Display

**Namespace:** `image.synth.*` / `image.display.*` — 0 ✅ + 0 ⚠️ / 8 = 0%

Display ones (`imshow`, `montage`, …) need graphics; synthesis is pure algorithm.

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `checkerboard` | ✅ | 0.005 | 129.02× | 85.28× | OK | Sig: I = checkerboard(side [, M [, N]]). 2*M*side x 2*N*side double image; right half dimmed to 0.7. Octave-image has checkerboard. |
| `imnoise` | ✅ |  |  |  | OK | gaussian / localvar / salt&pepper / speckle / poisson; shares numkit::builtin RNG |
| `phantom` | ✅ | 0.068 | 17.70× | 19.71× | OK | Sig: P = phantom([model | E] [, n]). Modified Shepp-Logan default; 64x64 reference test. Octave-image has phantom. |
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
| `imcrop` | ✅ | 0.003 |  | 55.29× | OK | Sig + small deterministic input. Auto-generated for parity sweep. |
| `imcrop3` | ❌ |  |  |  |  |  |
| `impyramid` | ✅ | 0.004 | 1334.70× | 221.67× | OK | Sig: B = impyramid(A, type). type='reduce' or 'expand'. Burt-Adelson 5-tap separable kernel; replicate boundary. Output: ceil(M/2)xceil(N/2) for reduce, (2M-1)x(2N-1) for expand. Octave-image has impyramid; cross-check expected OK. |
| `imresize` | ✅ | 0.003 |  | 434.70× | MISMATCH | Sig + small deterministic input. Auto-generated for parity sweep. |
| `imresize3` | ❌ |  |  |  |  |  |
| `imrotate` | ✅ | 0.003 |  | 92.45× | OK | Sig + small deterministic input. Auto-generated for parity sweep. |
| `imrotate3` | ❌ |  |  |  |  |  |
| `imtransform` | ❌ |  |  |  |  | legacy maketform path |
| `imtranslate` | ✅ | 0.003 |  |  | N/A | Sig + small deterministic input. Auto-generated for parity sweep. |
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
| `normxcorr2` | ✅ | 0.005 | 630.83× | 92.43× | OK | Sig: c = normxcorr2(template, img). Output (M+m-1)x(N+n-1) double in [-1, 1]. Octave-image has normxcorr2. |

### Image Filtering

**Namespace:** `image.filter.*` — 7 ✅ + 0 ⚠️ / 36 = 19%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `convmtx2` | ✅ | 0.002 | 46.37× |  | OK | Sig: T = convmtx2(h, m, n). Convolution matrix for 2-D 'full' convolution. MATLAB returns sparse, we return dense — wrap in full() in MATLAB so dim and values match. Octave-image doesn't ship convmtx2. |
| `entropyfilt` | ✅ | 0.007 | 449.12× | 85.99× | OK | Sig: E = entropyfilt(I [, domain]). Local Shannon entropy in bits; default 9x9 ones, symmetric pad. Octave-image has entropyfilt. |
| `fibermetric` | ❌ |  |  |  |  |  |
| `freqspace` | ✅ | 0.003 | 31.49× |  | OK | Sig: [f1, f2] = freqspace(N|[N M]) or f = freqspace(N[, 'whole']). Now supports 2-output centered form and 2-vec [N M] input (via libs/builtin extension). |
| `freqz2` | ✅ | 0.006 | 260.88× |  | OK | Sig: [H, f1, f2] = freqz2(h[, M, N]). 2-D frequency response on freqspace M×N grid. Centred kernel: H[i,j] = Σ h[p,q]·exp(+iπ·(f1[i]·(p-cp) + f2[j]·(q-cq))) with cp = ⌊(P-1)/2⌋. |
| `fsamp2` | ❌ |  |  |  |  | 2-D FIR via frequency sampling |
| `fspecial` | ✅ | 0.004 |  | 91.07× | OK | Sig + small deterministic input. Auto-generated for parity sweep. |
| `fspecial3` | ❌ |  |  |  |  |  |
| `ftrans2` | ❌ |  |  |  |  | 1-D → 2-D FIR transform |
| `fwind1` | ❌ |  |  |  |  | 2-D windowed FIR (rotation) |
| `fwind2` | ❌ |  |  |  |  |  |
| `gabor` | ❌ |  |  |  |  | Gabor filter bank |
| `imbilatfilt` | ✅ | 0.021 |  | 80.24× | OK | Sig: B = imbilatfilt(I, dos, sigma). Step image, tight range Gaussian (preserves edge). Tol relaxed: kernel-window-size differences plus per-pixel exp-rounding propagate. |
| `imboxfilt` | ✅ | 0.004 |  | 193.49× | OK | Sig + small deterministic input. Auto-generated for parity sweep. |
| `imboxfilt3` | ✅ | 0.007 | 271.02× |  | OK | Sig: J = imboxfilt3(V [, FilterSize]). 3-D box (mean) filter over a volume, replicate boundary on all 3 axes. MATLAB R2020a+; Octave-image doesn't have it → correctness=N/A. Deterministic input (1:245 reshape) — `rng(0); rand` would diverge between engines. |
| `imdiffusefilt` | ❌ |  |  |  |  | anisotropic diffusion |
| `imfilter` | ✅ | 0.003 |  | 116.15× | OK | Sig + small deterministic input. Auto-generated for parity sweep. |
| `imgaborfilt` | ❌ |  |  |  |  |  |
| `imgaussfilt` | ✅ | 0.006 | 365.89× | 120.93× | OK | Sig + small deterministic input. Auto-generated for parity sweep. |
| `imgaussfilt3` | ✅ | 0.006 | 392.83× |  | OK | Sig: J = imgaussfilt3(V[, sigma]). 3-D Gaussian filter, separable, replicate boundary. Sigma scalar or 3-vec. Filter size = 2*ceil(2σ)+1 per axis. MATLAB R2017+; Octave-image doesn't ship imgaussfilt3. |
| `imguidedfilter` | ❌ |  |  |  |  |  |
| `imnlmfilt` | ❌ |  |  |  |  | non-local means |
| `integralBoxFilter` | ❌ |  |  |  |  |  |
| `integralBoxFilter3` | ❌ |  |  |  |  |  |
| `integralImage` | ✅ | 0.003 | 321.42× | 127.21× | OK | Sig: J = integralImage(I). Summed-area table with (M+1)x(N+1) zero-padded leading row/col. Octave-image has integralImage; cross-check expected OK. |
| `integralImage3` | ✅ | 0.003 | 173.37× | 131.83× | OK | Sig: J = integralImage3(V). 3-D summed-volume table with leading zero plane/row/col. Octave-image may not have integralImage3 → may report N/A. |
| `medfilt2` | ✅ | 0.004 |  | 80.09× | OK | Sig + small deterministic input. Auto-generated for parity sweep. |
| `medfilt3` | ✅ | 0.030 | 65.50× |  | OK | Sig: J = medfilt3(V[, [M N P]]). 3-D median filter, default 3x3x3, symmetric pad. MATLAB R2017+; Octave-image doesn't ship medfilt3. |
| `modefilt` | ❌ |  |  |  |  |  |
| `nlfilter` | ❌ |  |  |  |  | generic neighborhood op |
| `ordfilt2` | ✅ | 0.004 | 671.59× | 87.58× | OK | Sig: B = ordfilt2(A, nth, domain [, S] [, padding]). Order-statistic filter; 1-based nth. Octave-image has ordfilt2. |
| `padarray` | ✅ | 0.003 |  | 102.54× | OK | Sig + small deterministic input. Auto-generated for parity sweep. |
| `rangefilt` | ✅ | 0.003 | 961.97× | 167.28× | OK | Sig: R = rangefilt(I [, domain]). Local max-min over neighbourhood. Default 3x3 ones, symmetric pad. Output class matches input. |
| `roifilt2` | ❌ |  |  |  |  |  |
| `stdfilt` | ✅ | 0.004 | 211.15× | 169.60× | OK | Sig: S = stdfilt(I [, domain]). Local sample std (N-1 norm). Default 3x3 ones, symmetric pad. Uses Octave-source test vector G. |
| `wiener2` | ✅ | 0.003 | 346.38× | 136.93× | OK | Sig: J = wiener2(I [, nhood [, noise]]). Adaptive Wiener filter (Lim 1989, eq. 9.26-9.29). Default 3x3, zero-pad. Octave-image has wiener2. |

### Contrast Adjustment

**Namespace:** `image.contrast.*` — 3 ✅ + 0 ⚠️ / 14 = 21%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `adapthisteq` | ❌ |  |  |  |  | CLAHE |
| `decorrstretch` | ❌ |  |  |  |  | decorrelation stretch |
| `histeq` | ✅ | 0.004 | 495.84× |  | MISMATCH | Sig + small deterministic input. Auto-generated for parity sweep. |
| `imadjust` | ✅ | 0.005 | 467.60× | 107.88× | OK | Sig + small deterministic input. Auto-generated for parity sweep. KNOWN: depends on stretchlim — same fix. |
| `imadjustn` | ✅ | 0.005 | 522.22× |  | OK | Sig: imadjustn(I). N-D variant of imadjust; we alias since imadjust already handles 3-D. Octave-image does not have imadjustn so cross-check may report N/A — depends on stretchlim like the imadjust spec. |
| `imflatfield` | ✅ | 4.275 | 1.27× |  | OK | Sig: imflatfield(I, sigma [, mask]). Smooth synthetic shading on a 32x32 double image. Octave-image 11.1.0 does not have imflatfield so cross-check reports correctness=N/A. Algorithm: F = imgaussfilt(im2double(I), sigma); B = im2cls((I_double./F) * mean(F)). 3-D inputs are processed per-page. |
| `imhistmatch` | ✅ | 0.004 |  |  | N/A | Sig: J = imhistmatch(I, ref, nbins). [0,0.5] source CDF-matched to [0,1] reference. Tol relaxed: bin discretisation differs slightly across implementations. |
| `imhistmatchn` | ✅ | 0.005 | 582.59× |  | OK | Sig: imhistmatchn(I, ref [, nbins]). N-D histogram match (single histogram across volume). Aliased to imhistmatch which uses the same single-histogram semantics. Octave-image 11.1.0 has no imhistmatchn → correctness=N/A as documented. |
| `imlocalbrighten` | ❌ |  |  |  |  |  |
| `imreducehaze` | ❌ |  |  |  |  |  |
| `imsharpen` | ✅ | 0.021 |  | 80.53× | OK | Sig: B = imsharpen(I). Defaults Radius=1, Amount=0.8, Threshold=0. Step image. Tol relaxed: tiny boundary-condition diffs in the imgaussfilt convolution propagate. |
| `intlut` | ✅ | 0.002 | 295.79× | 11.76× | OK | Sig: B = intlut(A, LUT). Pure pointwise table lookup. uint8 in / uint8 out via inversion LUT. Output class follows class(LUT). |
| `localcontrast` | ❌ |  |  |  |  |  |
| `locallapfilt` | ❌ |  |  |  |  | local Laplacian |
| `stretchlim` | ✅ | 0.003 | 440.27× | 109.49× | OK | Sig + small deterministic input. Auto-generated for parity sweep. KNOWN: percentile interpolation differs (numkit uses bin counts, MATLAB uses linear interpolation between bin edges). |

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
| `roicolor` | ✅ | 0.002 | 79.31× | 38.22× | OK | Sig: BW = roicolor(A, low, high) range form, or roicolor(A, v) set-membership. Output logical, same shape as A. Octave-image has roicolor. |
| `roifill` | ❌ |  |  |  |  | legacy alias |
| `roipoly` | ❌ |  |  |  |  |  |

### Morphological Operations

**Namespace:** `image.morph.*` — 5 ✅ + 0 ⚠️ / 27 = 19%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `applylut` | ✅ | 0.003 | 169.55× | 42.24× | OK | Sig: A = applylut(BW, LUT). LUT length = 2^(n*n). Octave-image has applylut. |
| `bwhitmiss` | ✅ | 0.004 | 1353.97× | 92.25× | OK | Sig: J = bwhitmiss(BW, se1, se2) or bwhitmiss(BW, interval). Hit-or-miss: imerode(BW, se1) & imerode(~BW, se2). Octave-image has bwhitmiss. |
| `bwlookup` | ❌ |  |  |  |  |  |
| `bwmorph` | ❌ |  |  |  |  | 2-D morphology dispatch |
| `bwmorph3` | ❌ |  |  |  |  |  |
| `bwpack` | ✅ | 0.003 | 106.93× | 59.67× | OK | Sig: BWP = bwpack(BW). Pack 32 binary rows into one uint32 (LSB = row 0). Octave-image has bwpack. |
| `bwperim` | ✅ | 0.003 |  | 154.18× | OK | Sig + small deterministic input. Auto-generated for parity sweep. |
| `bwskel` | ❌ |  |  |  |  | skeletonize |
| `bwulterode` | ❌ |  |  |  |  | ultimate erosion |
| `bwunpack` | ❌ |  |  |  |  |  |
| `conndef` | ❌ |  |  |  |  |  |
| `imbothat` | ✅ | 0.005 |  | 38.27× | OK | Sig: J = imbothat(I, SE). Dark dot extracted (B(3,3)=9, others=0). |
| `imclearborder` | ✅ | 0.008 |  | 44.84× | OK | Sig: J = imclearborder(BW). 3 blobs (rim + interior); only interior dot survives. |
| `imclose` | ✅ | 0.004 |  | 66.54× | OK | Sig + small deterministic input. Auto-generated for parity sweep. |
| `imdilate` | ✅ | 0.003 |  | 74.03× | OK | Sig + small deterministic input. Auto-generated for parity sweep. |
| `imerode` | ✅ | 0.003 |  | 44.20× | OK | Sig + small deterministic input. Auto-generated for parity sweep. |
| `imextendedmax` | ✅ | 0.019 |  | 7.87× | OK | Sig: BW = imextendedmax(I, h). Tall peak A survives (mask=1 at (2,2)); shallow peak B suppressed. |
| `imextendedmin` | ✅ | 0.020 |  | 8.89× | OK | Sig: BW = imextendedmin(I, h). Deep trough A survives, shallow B suppressed. |
| `imfill` | ✅ | 0.006 |  | 53.54× | OK | Sig: J = imfill(BW, 'holes'). Hollow square ring → fully filled square. |
| `imhmax` | ✅ | 0.011 |  | 5.68× | OK | Sig: J = imhmax(I, h). 3x7 image, two peaks (40 at (2,2), 20 at (2,5)), background 10. h=15 must keep peak A (shaved to 25) and flatten peak B. |
| `imhmin` | ✅ | 0.011 |  | 16.30× | OK | Sig: J = imhmin(I, h). Two troughs depth 90 / 30; h=50 raises shallow (B) to background, keeps deep (A). |
| `imimposemin` | ✅ | 0.011 |  | 10.73× | OK | Sig: J = imimposemin(I, BW). Force regional minima at marker; basin B at (2,5) erased (lifted to plateau 10). |
| `imkeepborder` | ✅ | 0.008 |  |  | N/A | Sig: J = imkeepborder(BW). Inverse of imclearborder — keep components touching the rim. (NOTE: imkeepborder is a MATLAB R2025b addition; if Octave's image package lacks it, run with --no-octave.) |
| `imopen` | ✅ | 0.004 |  | 81.06× | OK | Sig + small deterministic input. Auto-generated for parity sweep. |
| `imreconstruct` | ✅ | 0.017 |  | 8.63× | OK | Sig: J = imreconstruct(marker, mask). Reconstruction by dilation; marker grows to fill the connected mask region. |
| `imregionalmax` | ✅ | 0.007 |  | 19.32× | OK | Sig: BW = imregionalmax(I). Two regional maxima at (2,2) and (2,5). |
| `imregionalmin` | ✅ | 0.008 |  | 24.93× | OK | Sig: BW = imregionalmin(I). Two regional minima at (2,2) and (2,5). |
| `imtophat` | ✅ | 0.005 |  | 68.45× | OK | Sig: J = imtophat(I, SE). Lone bright dot extracted (T(3,3)=9, others=0). |
| `makelut` | ❌ |  |  |  |  |  |
| `offsetstrel` | ❌ |  |  |  |  | structuring element with offsets |
| `strel` | ✅ |  |  |  | N/A | Sig + small deterministic input. Auto-generated for parity sweep. |

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
| `psf2otf` | ✅ | 0.005 | 751.25× | 56.86× | OK | Sig: otf = psf2otf(psf [, outsize]). FFT of circshift(zeropad(psf), -floor(size/2)). Octave-image has psf2otf. |

### Neighborhood and Block Processing

**Namespace:** `image.block.*` — 0 ✅ + 0 ⚠️ / 6 = 0%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `bestblk` | ✅ | 0.003 | 120.81× | 20.79× | OK | Sig: siz = bestblk(IMS [, k]). Best block size minimising mod-padding within [ceil(min(dim/10, k/2)), k]. Octave-image has bestblk. |
| `blockproc` | ❌ |  |  |  |  | block-wise processing |
| `col2im` | ✅ | 0.003 |  | 79.94× | OK | Sig: A = col2im(B, [m n], [mm nn], 'distinct'). Round-trip im2col→col2im rebuilds 4x4 (clean multiples). |
| `colfilt` | ❌ |  |  |  |  |  |
| `im2col` | ✅ | 0.003 |  | 69.65× | OK | Sig: B = im2col(A, [m n], 'sliding'). 4x4 lattice → 4x9 (3·3 sliding positions, column-major within block). |
| `nlfilter` | ❌ |  |  |  |  | duplicate of filter section |

### Image Arithmetic

**Namespace:** `image.arith.*` — 8 ✅ + 0 ⚠️ / 8 = 100%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `imabsdiff` | ✅ | 0.003 |  | 121.06× | OK | Sig + small deterministic input. Auto-generated for parity sweep. |
| `imadd` | ✅ | 0.003 |  | 101.93× | OK | Sig + small deterministic input. Auto-generated for parity sweep. |
| `imapplymatrix` | ✅ |  |  |  | OK | 3-D colour transform along page axis |
| `imcomplement` | ✅ | 0.002 |  | 52.55× | OK | Sig + small deterministic input. Auto-generated for parity sweep. |
| `imdivide` | ✅ | 0.003 |  | 54.92× | OK | Sig + small deterministic input. Auto-generated for parity sweep. |
| `imlincomb` | ✅ | 0.003 |  | 68.58× | OK | Sig + small deterministic input. Auto-generated for parity sweep. |
| `immultiply` | ✅ | 0.003 |  | 66.28× | OK | Sig + small deterministic input. Auto-generated for parity sweep. |
| `imsubtract` | ✅ | 0.003 |  | 54.12× | OK | Sig + small deterministic input. Auto-generated for parity sweep. |

### Image Segmentation

**Namespace:** `image.segment.*` — 6 ✅ + 0 ⚠️ / 22 = 27%

Deep-learning-based ones (`imsegsam`, `segmentAnythingModel`, …) intentionally omitted.

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `activecontour` | ❌ |  |  |  |  | Chan-Vese |
| `bfscore` | ❌ |  |  |  |  | boundary F1 score |
| `boundarymask` | ✅ |  |  |  | OK | conn=4/8; flags any pixel adjacent to a different label or image edge |
| `dice` | ✅ | 0.002 |  |  | N/A | Sig + small deterministic input. Auto-generated for parity sweep. |
| `gradientweight` | ❌ |  |  |  |  |  |
| `grabcut` | ❌ |  |  |  |  |  |
| `grayconnected` | ✅ |  |  |  | OK | 8-conn flood-fill from seed within tol; auto-tol per class |
| `graydiffweight` | ❌ |  |  |  |  |  |
| `imoverlay` | ✅ |  |  |  | OK | gray or RGB input → H×W×3 uint8; auto byte/float colour |
| `imseggeodesic` | ❌ |  |  |  |  |  |
| `imsegfmm` | ❌ |  |  |  |  | fast marching |
| `imsegisodata` | ❌ |  |  |  |  |  |
| `imsegkmeans` | ❌ |  |  |  |  |  |
| `imsegkmeans3` | ❌ |  |  |  |  |  |
| `jaccard` | ✅ | 0.003 |  |  | N/A | Sig + small deterministic input. Auto-generated for parity sweep. |
| `label2idx` | ✅ |  |  |  | OK | cell column of MATLAB-1-based linear indices per label |
| `labeloverlay` | ❌ |  |  |  |  |  |
| `lazysnapping` | ❌ |  |  |  |  |  |
| `superpixels` | ❌ |  |  |  |  | SLIC |
| `superpixels3` | ❌ |  |  |  |  |  |
| `watershed` | ❌ |  |  |  |  |  |

### Object Analysis

**Namespace:** `image.object.*` — 4 ✅ + 0 ⚠️ / 18 = 22%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `bwboundaries` | ✅ |  |  |  | OK | Moore-neighbour outer trace, conn=4/8; 'noholes' default |
| `bwtraceboundary` | ❌ |  |  |  |  |  |
| `circles2mask` | ❌ |  |  |  |  |  |
| `corner` | ❌ |  |  |  |  | Harris/Min-eig corner detector |
| `cornermetric` | ❌ |  |  |  |  |  |
| `edge` | ✅ | 0.014 |  |  | N/A | Sig + small deterministic input. Auto-generated for parity sweep. |
| `edge3` | ❌ |  |  |  |  |  |
| `hough` | ❌ |  |  |  |  |  |
| `houghlines` | ❌ |  |  |  |  |  |
| `houghpeaks` | ❌ |  |  |  |  |  |
| `imfindcircles` | ❌ |  |  |  |  | circle Hough |
| `imgradient` | ✅ | 0.006 |  |  | N/A | Sig + small deterministic input. Auto-generated for parity sweep. |
| `imgradientxy` | ✅ | 0.005 |  |  | N/A | Sig + small deterministic input. Auto-generated for parity sweep. |
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
| `bwarea` | ✅ | 0.002 |  | 38.03× | OK | Sig + small deterministic input. Auto-generated for parity sweep. |
| `bwareafilt` | ✅ | 0.004 |  | 231.24× | OK | Sig: J = bwareafilt(BW, range|n [, keep] [, conn]). Range [lo hi] or top-N selection ('largest' default). Octave-image has bwareafilt. |
| `bwareaopen` | ✅ | 0.003 |  | 61.61× | OK | Sig + small deterministic input. Auto-generated for parity sweep. |
| `bwconncomp` | ✅ |  |  |  | OK | connectivity / size / count / pixel-list |
| `bwconvhull` | ❌ |  |  |  |  |  |
| `bwdist` | ✅ | 0.004 |  | 19.43× | OK | Sig + small deterministic input. Auto-generated for parity sweep. Tol=1e-6: tiny FP precision delta on Euclidean sqrt. |
| `bwdistgeodesic` | ❌ |  |  |  |  |  |
| `bweuler` | ✅ | 0.004 | 589.22× | 71.04× | OK | Sig: e = bweuler(BW [, n]). Euler number (objects − holes) via Pratt bit-quad LUT. Octave-image has bweuler. |
| `bwferet` | ❌ |  |  |  |  | Feret diameters |
| `bwlabel` | ✅ | 0.003 |  | 43.65× | OK | Sig + small deterministic input. Auto-generated for parity sweep. |
| `bwlabeln` | ❌ |  |  |  |  |  |
| `bwperim` | ✅ | 0.003 |  | 154.18× | OK | Sig + small deterministic input. Auto-generated for parity sweep. |
| `bwpropfilt` | ❌ |  |  |  |  |  |
| `bwselect` | ✅ | 0.004 | 916.09× | 45.60× | OK | Sig: [BW2, idx] = bwselect(BW, cols, rows[, conn]). Keep all components that contain any seed pixel. Octave-image has bwselect. |
| `bwselect3` | ❌ |  |  |  |  |  |
| `cc2bw` | ❌ |  |  |  |  |  |
| `corr2` | ✅ | 0.003 | 254.34× | 160.16× | OK | Sig: r = corr2(A, B). Pearson correlation coefficient over all elements (flat). Octave-image has corr2. |
| `graydist` | ❌ |  |  |  |  |  |
| `imcontour` | ❌ |  |  |  |  |  |
| `imhist` | ✅ | 0.004 |  | 64.89× | OK | Sig + small deterministic input. Auto-generated for parity sweep. |
| `impixel` | ❌ |  |  |  |  |  |
| `improfile` | ❌ |  |  |  |  |  |
| `labelmatrix` | ❌ |  |  |  |  |  |
| `mean2` | ✅ | 0.002 | 115.99× | 63.66× | OK | Sig: m = mean2(A). Mean of all elements (flat). Octave-image has mean2. |
| `poly2label` | ❌ |  |  |  |  |  |
| `regionprops` | ✅ |  |  |  | OK | Area / Centroid / BoundingBox; struct array out, BW or label input |
| `regionprops3` | ❌ |  |  |  |  |  |
| `std2` | ✅ | 0.003 | 280.70× | 55.24× | OK | Sig: s = std2(A). Std of all elements normalized by N (population). Octave-image has std2. |

### Texture Analysis

**Namespace:** `image.texture.*` — 0 ✅ + 0 ⚠️ / 6 = 0%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `entropy` | ✅ | 0.004 | 338.82× | 62.73× | OK | Sig: E = entropy(I [, nbins]). Shannon entropy in bits over imhist of im2uint8(I) (256 bins by default). Octave-image has entropy. |
| `entropyfilt` | ✅ | 0.007 | 449.12× | 85.99× | OK | Sig: E = entropyfilt(I [, domain]). Local Shannon entropy in bits; default 9x9 ones, symmetric pad. Octave-image has entropyfilt. |
| `graycomatrix` | ❌ |  |  |  |  | GLCM |
| `graycoprops` | ❌ |  |  |  |  |  |
| `rangefilt` | ✅ | 0.003 | 961.97× | 167.28× | OK | Sig: R = rangefilt(I [, domain]). Local max-min over neighbourhood. Default 3x3 ones, symmetric pad. Output class matches input. |
| `stdfilt` | ✅ | 0.004 | 211.15× | 169.60× | OK | Sig: S = stdfilt(I [, domain]). Local sample std (N-1 norm). Default 3x3 ones, symmetric pad. Uses Octave-source test vector G. |

### Image Quality

**Namespace:** `image.quality.*` — 3 ✅ + 0 ⚠️ / 8 = 38%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `brisque` | ❌ |  |  |  |  | no-reference quality (needs trained model) |
| `immse` | ✅ | 0.003 |  | 47.28× | OK | Sig + small deterministic input. Auto-generated for parity sweep. |
| `multissim` | ❌ |  |  |  |  | multi-scale SSIM |
| `multissim3` | ❌ |  |  |  |  |  |
| `niqe` | ❌ |  |  |  |  | no-reference (needs model) |
| `piqe` | ❌ |  |  |  |  | perceptual no-reference |
| `psnr` | ✅ | 0.002 |  | 61.06× | OK | Sig + small deterministic input. Auto-generated for parity sweep. |
| `ssim` | ✅ | 0.317 |  |  | N/A | Sig + small deterministic input. Auto-generated for parity sweep. |

### Image Transforms

**Namespace:** `image.transform.*` — 7 ✅ + 0 ⚠️ / 11 = 64%

`fft2` / `ifft2` / `fftshift` / `ifftshift` already covered under Signal / Transforms; cross-listed here per MATLAB TOC.

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `dct2` | ✅ | 0.005 |  | 41.39× | OK | Sig + small deterministic input. Auto-generated for parity sweep. |
| `dctmtx` | ✅ | 0.003 |  | 37.54× | OK | Sig + small deterministic input. Auto-generated for parity sweep. |
| `fan2para` | ❌ |  |  |  |  | fan-beam → parallel |
| `fanbeam` | ❌ |  |  |  |  |  |
| `fft2` | ✅ |  |  |  | OK | already in Signal / Transforms |
| `fftshift` | ✅ | 0.008 | 69.39× | 52.90× | OK | Sig: Y = fftshift(X[, dim]). Cyclic shift along every non-singleton dim by ceil(extent/2); inverse ifftshift uses floor(extent/2). Bug fix: numkit had fftshift/ifftshift swapped for odd N + flat-shift instead of per-dim for matrices + dim arg ignored. tol=0 (integer-stable). |
| `idct2` | ✅ | 0.005 |  | 48.89× | OK | Sig + small deterministic input. Auto-generated for parity sweep. |
| `ifanbeam` | ❌ |  |  |  |  |  |
| `ifft2` | ✅ |  |  |  | OK |  |
| `ifftshift` | ✅ | 0.005 | 86.45× | 54.01× | OK | Sig: Y = ifftshift(X[, dim]). Inverse of fftshift; cyclic shift along every non-singleton dim by floor(extent/2). Joint fix with fftshift on 2026-05-08 — they were swapped for odd-extent dims. tol=0. |
| `para2fan` | ❌ |  |  |  |  |  |

## IO

### Low-Level File I/O

**Namespace:** `io.file_io.*` — 13 ✅ + 0 ⚠️ / 15 = 86%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `fclose` | ✅ | 0.025 | 0.77× | 0.94× | OK | Sig: STATUS = fclose(FID). 1000 iters. |
| `feof` | ✅ | 0.026 | 1.13× | 1.33× | OK | Sig: TF = feof(FID). 1000 iters. |
| `ferror` | ✅ | 0.026 | 0.71× |  | OK | Sig: MSG = ferror(FID). 1000 iters. |
| `fgetl` | ✅ | 0.026 | 1.01× |  | OK | Sig: LINE = fgetl(FID). 1000 iters. |
| `fgets` | ✅ | 0.025 | 1.01× |  | OK | Sig: LINE = fgets(FID). 1000 iters. |
| `fileread` | ✅ | 0.019 | 4.01× |  | OK | Sig: T = fileread(F). 1000 iters. |
| `fopen` | ✅ | 0.027 | 0.69× | 0.88× | OK | Sig: FID = fopen(F). 1000 iters. |
| `fprintf` | ✅ |  |  |  | N/A | Sig: COUNT = fprintf(FID, FMT, A). 100 iters. |
| `fread` | ✅ | 0.048 | 0.80× | 0.92× | OK | Sig: A = fread(FID, COUNT, PRECISION). 100 iters. |
| `frewind` | ✅ | 0.028 | 1.48× | 1.64× | OK | Sig: frewind(FID). 1000 iters. |
| `fscanf` | ✅ | 0.027 | 1.63× | 1.90× | OK | Sig: A = fscanf(FID, FMT). 1000 iters. |
| `fseek` | ✅ | 0.028 | 1.01× | 1.15× | OK | Sig: STATUS = fseek(FID, OFFSET, ORIGIN). 1000 iters. |
| `ftell` | ✅ | 0.028 | 1.03× | 1.23× | OK | Sig: POS = ftell(FID). 1000 iters. |
| `fwrite` | ✅ | 0.235 | 2.12× | 1.06× | OK | Sig: COUNT = fwrite(FID, A, PRECISION). 100 iters. |
| `openedfiles` | ❌ |  |  |  |  |  |

### Text Files (CSV / dlm / readtable)

**Namespace:** `io.text.*`. Exception: `readtable/writetable/readtimetable/writetimetable` → `table.*` (future) — 1 ✅ + 0 ⚠️ / 16 = 6%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `fileread` | ✅ | 0.019 | 4.01× |  | OK | Sig: T = fileread(F). 1000 iters. |
| `importdatatask` | ❌ |  |  |  |  |  |
| `importtool` | ❌ |  |  |  |  |  |
| `readcell` | ❌ |  |  |  |  |  |
| `readlines` | ✅ | 0.019 | 132.24× |  | MISMATCH | Sig: L = readlines(F). 4-line file. 1000 iters. |
| `readmatrix` | ✅ | 0.021 | 274.53× |  | OK | Sig: M = readmatrix(F). 100 iters. |
| `readtable` | ❌ |  |  |  |  | needs table type |
| `readtimetable` | ❌ |  |  |  |  |  |
| `readvars` | ❌ |  |  |  |  |  |
| `textscan` | ✅ | 0.028 | 4.16× | 1.97× | OK | Sig: C = textscan(FID, FMT). 100 iters. |
| `type` | ✅ |  |  |  | N/A | Sig: type(F). Captured via evalc. 1000 iters. |
| `writecell` | ❌ |  |  |  |  |  |
| `writelines` | ✅ |  |  |  | N/A | Sig: writelines(L, F). 100 iters. |
| `writematrix` | ✅ | 0.650 | 4.00× |  | MISMATCH | Sig: writematrix(M, F). 100 iters. |
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
| `readmatrix` | ✅ | 0.021 | 274.53× |  | OK | Sig: M = readmatrix(F). 100 iters. |
| `readtable` | ❌ |  |  |  |  | needs table type |
| `readtimetable` | ❌ |  |  |  |  |  |
| `readvars` | ❌ |  |  |  |  |  |
| `sheetnames` | ❌ |  |  |  |  |  |
| `writecell` | ❌ |  |  |  |  |  |
| `writematrix` | ✅ | 0.650 | 4.00× |  | MISMATCH | Sig: writematrix(M, F). 100 iters. |
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
| `fileparts` | ✅ | 0.000 | 8.91× |  | OK | Sig: [PATH,NAME,EXT] = fileparts(F). 10000 iters. |
| `filesep` | ✅ | 0.000 | 2.88× |  | OK | Sig: SEP = filesep. OS-specific separator. 100k iters. |
| `fullfile` | ✅ | 0.001 | 16.87× |  | OK | Sig: F = fullfile(PARTS). 10000 iters. |
| `matlabdrive` | ❌ |  |  |  |  |  |
| `matlabroot` | ❌ |  |  |  |  |  |
| `tempdir` | ✅ | 0.013 | 0.08× |  | OK | Sig: D = tempdir. 10000 iters. |
| `tempname` | ✅ | 0.014 | 1.08× |  | OK | Sig: F = tempname. 10000 iters. |
| `toolboxdir` | ❌ |  |  |  |  |  |

## Linear Algebra



**Namespace:** `linalg.*` (future) — 12 ✅ + 0 ⚠️ / 82 = 15%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `balance` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `bandwidth` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `cdf2rdf` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `chol` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `cholupdate` | ❌ |  |  |  |  |  |
| `cond` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `condeig` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `condest` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `cross` | ✅ | 0.000 | 18.24× | 177.90× | OK | Sig: C = cross(A, B). Single 3-vec pair (numkit batch unsupported — see BUGS). 100k iters. |
| `ctranspose` | ✅ | 6.955 | 0.22× | 0.37× | OK | Sig: Y = ctranspose(A). 1k×1k Hermitian (real → same as transpose). 100 iters. |
| `decomposition` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `det` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `dot` | ✅ | 2.036 | 0.02× | 0.07× | OK | Sig: D = dot(A, B). 1M-elem dot product. 100 iters. Scalar fp. |
| `eig` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `eigs` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `expm` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `expmv` | ❌ |  |  |  |  |  |
| `funm` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `gsvd` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `hess` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `inv` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `isbanded` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `isdiag` | ❌ |  |  |  |  |  |
| `ishermitian` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `issymmetric` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `istril` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `istriu` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `kron` | ✅ | 0.189 | 0.51× | 0.12× | OK | Sig: K = kron(A, B). 10x10 ⊗ 20x20 = 200x200. 100 iters. |
| `ldl` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `linsolve` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `logm` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `lscov` | ✅ | 0.004 | 201.24× | 10.25× | OK | Sig: [x, stdx, mse, S] = lscov(A, b[, w]). Weighted least squares. With identity weights = OLS. mse = SSR/(N-p); S = mse·(A'WA)^(-1). Full N×N covariance V intentionally not yet supported. |
| `lsqminnorm` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `lsqnonneg` | ❌ |  |  |  |  |  |
| `lu` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `mldivide` | ✅ |  |  |  | N/A | Sig: X = mldivide(A, B) = A\B. 100x100. 100 iters. |
| `mpower` | ✅ |  |  |  | N/A | Sig: Y = mpower(A, n). 20x20 matrix squared. 1000 iters. |
| `mrdivide` | ✅ |  |  |  | N/A | Sig: X = mrdivide(A, B) = A/B. 100x100. 100 iters. |
| `mtimes` | ✅ | 0.093 | 0.52× | 0.79× | OK | Sig: C = mtimes(A, B). 100x100 matmul. 100 iters. |
| `norm` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `normest` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `null` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `ordeig` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `ordqz` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `ordschur` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `orth` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `pagectranspose` | ✅ | 0.207 | 0.24× | 0.23× | OK | 128x64x8 real-valued — pagectranspose equals pagetranspose. 100 iters. |
| `pageeig` | ❌ |  |  |  |  |  |
| `pageinv` | ❌ |  |  |  |  |  |
| `pagelsqminnorm` | ❌ |  |  |  |  |  |
| `pagemldivide` | ❌ |  |  |  |  |  |
| `pagemrdivide` | ❌ |  |  |  |  |  |
| `pagemtimes` | ✅ | 0.019 | 0.78× |  | OK | Sig: C = pagemtimes(A, B). 20×20×20 batch matmul. 100 iters. |
| `pagenorm` | ❌ |  |  |  |  |  |
| `pagepinv` | ❌ |  |  |  |  |  |
| `pagesvd` | ❌ |  |  |  |  |  |
| `pagetranspose` | ✅ | 0.083 | 1.11× | 0.63× | OK | 128x64x8 array, page-wise transpose. 100 iters. |
| `pinv` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `planerot` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `polyeig` | ❌ |  |  |  |  | poly eig |
| `qr` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `qrdelete` | ❌ |  |  |  |  |  |
| `qrinsert` | ❌ |  |  |  |  |  |
| `qrupdate` | ❌ |  |  |  |  |  |
| `qz` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `rank` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `rcond` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `rref` | ❌ |  |  |  |  |  |
| `rsf2csf` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `schur` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `sqrtm` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `subspace` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `svd` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `svdappend` | ❌ |  |  |  |  |  |
| `svds` | ❌ |  |  |  |  | **deferred — libs/sparse** |
| `svdsketch` | ❌ |  |  |  |  |  |
| `sylvester` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `trace` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `transpose` | ✅ | 7.375 | 0.20× | 0.35× | OK | Sig: Y = transpose(X). 1k×1k transpose. 100 iters. Element-wise SAVE. |
| `tril` | ✅ | 2.260 | 0.88× | 0.94× | OK | Sig: L = tril(A). 1k×1k lower triangular. 100 iters. |
| `triu` | ✅ | 2.255 | 0.89× | 0.97× | OK | Sig: U = triu(A). 1k×1k upper triangular. 100 iters. |
| `vecnorm` | ❌ |  |  |  |  | **deferred — libs/linalg** |

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
| `fminbnd` | ✅ | 0.002 |  |  | N/A | Sig: X = fminbnd(F,A,B). 1-D quadratic min at x=3. 1000 iters. |
| `fminsearch` | ✅ | 0.041 |  |  | N/A | Sig: X = fminsearch(F, X0). 2-D quadratic at (1,2). 1000 iters. |
| `fzero` | ✅ | 0.005 |  |  | N/A | Sig: X = fzero(F, [A B]). Cubic root in [0,5]. 1000 iters. |
| `lsqnonneg` | ❌ |  |  |  |  |  |
| `optimget` | ✅ | 0.000 | 209.63× | 107.63× | OK | Sig: V = optimget(O, NAME). 10000 iters. |
| `optimize` | ❌ |  |  |  |  |  |
| `optimset` | ✅ | 0.001 | 116.44× | 68.66× | MISMATCH | Sig: O = optimset('NAME', VAL, ...). 10000 iters. |

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
| `mldivide` | ✅ |  |  |  | OK | already in core (operator `\`) |

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
| `chirp` | ✅ | 0.030 | 2.17× | 1.70× | OK | Sig: Y = chirp(T, F0, T1, F1). 4096-pt linear sweep. 1000 iters. |
| `demod` | ❌ |  |  |  |  |  |
| `diric` | ✅ | 0.116 | 0.96× | 1.87× | OK | Sig: Y = diric(X, N). Dirichlet kernel N=5. 1000 iters. |
| `framelbl` | ❌ |  |  |  |  |  |
| `framesig` | ❌ |  |  |  |  |  |
| `gauspuls` | ✅ | 0.107 | 0.44× | 1.00× | MISMATCH | Sig: Y = gauspuls(T, FC, BW). Gaussian pulse. 1000 iters. |
| `gmonopuls` | ✅ | 0.085 | 0.49× | 0.79× | OK | Sig: Y = gmonopuls(T, FC). Gaussian monopulse. 1000 iters. |
| `marcumq` | ✅ |  |  |  |  |  |
| `modulate` | ❌ |  |  |  |  |  |
| `pulstran` | ✅ | 0.009 | 5.05× | 17.41× | MISMATCH | Sig: Y = pulstran(T, D, FUNC, ARGS). Pulse train. 1000 iters. |
| `rectpuls` | ✅ | 0.020 | 1.23× | 1.42× | OK | Sig: Y = rectpuls(T). Rectangular pulse. 1000 iters. |
| `sawtooth` | ✅ | 0.063 | 0.80× | 1.31× | OK | Sig: Y = sawtooth(T). 1000 iters. |
| `shiftdata` | ❌ |  |  |  |  |  |
| `sinc` | ✅ | 0.731 | 0.28× | 1.75× | OK | Sig: Y = sinc(X). 100k-pt sin(πx)/(πx). 1000 iters. |
| `square` | ✅ | 0.061 | 0.66× | 0.82× | OK | Sig: Y = square(T). Square wave. 1000 iters. |
| `tripuls` | ✅ | 0.057 | 0.80× | 1.09× | OK | Sig: Y = tripuls(T). Triangular pulse. 1000 iters. |
| `udecode` | ❌ |  |  |  |  |  |
| `uencode` | ❌ |  |  |  |  |  |
| `unshiftdata` | ❌ |  |  |  |  |  |
| `vco` | ❌ |  |  |  |  | VCO |

### Filter Design

**Namespace:** `signal.filter_design.*` — 11 ✅ + 0 ⚠️ / 37 = 30%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `butter` | ✅ | 0.000 | 316.16× | 403.68× | OK | Sig: [B,A] = butter(N, WN). 4th-order LPF. 1000 iters. SAVE on B. |
| `buttord` | ✅ |  |  |  | OK | LP/HP match MATLAB exactly; band-edge refinement deferred. |
| `cfirpm` | ❌ |  |  |  |  | complex Parks-McClellan |
| `cheb1ord` | ✅ |  |  |  | OK | Wn = Wp (passband edge). |
| `cheb2ord` | ✅ |  |  |  | OK | Wn = Ws (stopband edge). |
| `cheby1` | ✅ |  |  |  | OK | LP/HP/BP/BS via cheb1ap+lp2X+zp2tf+bilinear. |
| `cheby2` | ✅ |  |  |  | OK | Cheb2ap zero formula was 1/sin → 1/cos; fixed in 6ec8a62. |
| `designfilt` | ❌ |  |  |  |  |  |
| `designfilter` | ❌ |  |  |  |  |  |
| `digitalfilter` | ❌ |  |  |  |  |  |
| `double` | ✅ | 3.606 | 0.04× | 0.57× | OK | Sig: Y = double(X). 1M single → double. 50 iters. Element-wise SAVE. |
| `dspfwiz` | ❌ |  |  |  |  |  |
| `ellip` | ❌ |  |  |  |  | IIR elliptic |
| `ellipord` | ❌ |  |  |  |  | order estimator |
| `filt2block` | ❌ |  |  |  |  |  |
| `filteranalyzer` | ❌ |  |  |  |  |  |
| `fir1` | ✅ | 0.000 | 152.85× | 3103.22× | OK | Sig: B = fir1(N, WN). 21-tap FIR. 1000 iters. |
| `fir2` | ❌ |  |  |  |  | arbitrary-response FIR |
| `fircls` | ❌ |  |  |  |  | constrained-LS FIR |
| `fircls1` | ❌ |  |  |  |  |  |
| `firls` | ❌ |  |  |  |  | least-squares FIR |
| `firpm` | ❌ |  |  |  |  | Parks-McClellan FIR |
| `firpmord` | ❌ |  |  |  |  | order estimator |
| `gaussdesign` | ✅ | 0.004 | 250.45× |  | OK | Sig: h = gaussdesign(BT, span, sps). Gaussian FIR pulse-shaping filter, length span*sps+1, sum-normalized to 1. Element-wise SAVE. |
| `info` | ❌ |  |  |  |  |  |
| `intfilt` | ✅ | 0.001 | 465.68× |  | MISMATCH | Sig: H = intfilt(R, L, ALPHA). FIR coeffs (alpha=0.5). 1000 iters. |
| `isdouble` | ❌ |  |  |  |  |  |
| `issingle` | ✅ | 0.000 |  |  | N/A | Sig: TF = issingle(X). 100k iters. |
| `kaiserord` | ❌ |  |  |  |  | Kaiser window order |
| `maxflat` | ❌ |  |  |  |  |  |
| `polyscale` | ❌ |  |  |  |  |  |
| `polystab` | ❌ |  |  |  |  |  |
| `rcosdesign` | ✅ |  |  |  | OK | shared with comm.shape; unit-energy 'normal' / 'sqrt' |
| `scalefiltersections` | ❌ |  |  |  |  |  |
| `sgolay` | ✅ | 0.001 | 16.08× | 214.22× | OK | Sig: B = sgolay(K, F). order=3 frame=11. 1000 iters. |
| `single` | ✅ | 2.755 | 0.06× | 0.43× | OK | Sig: Y = single(X). 1M double → single. 50 iters. Element-wise SAVE. |
| `yulewalk` | ❌ |  |  |  |  | recursive YW |

### Analog Filters

**Namespace:** `signal.filter_design.*` — 14 ✅ + 0 ⚠️ / 17 = 82%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `besselap` | ✅ |  |  |  |  | analog prototype |
| `besself` | ✅ |  |  |  | OK | a = [1, 2.4329, 2.4662, 1] for N=3 — matches Bessel polynomial. |
| `bilinear` | ✅ |  |  |  |  |  |
| `buttap` | ✅ |  |  |  |  | analog prototype |
| `butter` | ✅ | 0.000 | 316.16× | 403.68× | OK | Sig: [B,A] = butter(N, WN). 4th-order LPF. 1000 iters. SAVE on B. |
| `cheb1ap` | ✅ |  |  |  |  | analog prototype |
| `cheb2ap` | ✅ |  |  |  |  | analog prototype (zeros formula fixed in 6ec8a62) |
| `cheby1` | ✅ |  |  |  | OK | Matches MATLAB to 6+ decimals on (4, 0.5, 0.4) test. |
| `cheby2` | ✅ |  |  |  | OK | Matches MATLAB to 6+ decimals on (4, 30, 0.4) test. |
| `ellip` | ❌ |  |  |  |  | IIR elliptic — needs ellipap (Jacobi elliptic) |
| `ellipap` | ❌ |  |  |  |  | needs K(m) via AGM + Jacobi sn/cn/dn |
| `freqs` | ✅ |  |  |  |  | analog freq response |
| `impinvar` | ✅ |  |  |  | OK | Matches MATLAB to 8 decimals on simple-pole tests. Repeated poles not yet supported. |
| `lp2bp` | ✅ |  |  |  |  |  |
| `lp2bs` | ✅ |  |  |  |  |  |
| `lp2hp` | ✅ |  |  |  |  |  |
| `lp2lp` | ✅ |  |  |  |  |  |

### Digital Filter Analysis

**Namespace:** `signal.filter_analysis.*` — 3 ✅ + 0 ⚠️ / 19 = 15%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `filteranalyzer` | ❌ |  |  |  |  |  |
| `filternorm` | ✅ | 0.135 | 5.63× | 1.43× | OK | Sig: norm = filternorm(b, a [, pnorm]). FIR L2 (default), IIR L2, IIR L_inf via 8192-point freqz integration. Tolerance 1e-6 for the trapezoidal approximation. |
| `filtord` | ✅ | 0.000 | 97.29× | 92.24× | OK | Sig: n = filtord(b[, a]). FIR (single arg or trivial a) → length(b)-1; IIR → max(len_b, len_a)-1 with trailing zeros trimmed. fingerprint covers IIR + 2 FIR cases. |
| `firtype` | ✅ | 0.000 | 864.02× |  | OK | Sig: t = firtype(b). FIR linear-phase classification per MATLAB: 1 = sym/odd-len, 2 = sym/even-len, 3 = anti/odd-len, 4 = anti/even-len. Fingerprint covers all 4 types. |
| `freqz` | ✅ | 0.004 | 21.99× | 51.25× | MISMATCH | Sig: [H,W] = freqz(B,A,N). 256-pt freq response. 1000 iters. |
| `grpdelay` | ✅ | 0.006 | 29.69× | 26.74× | MISMATCH | Sig: [G,W] = grpdelay(B,A,N). Group delay. 1000 iters. |
| `impz` | ✅ | 0.002 | 38.13× | 13.46× | OK | Sig: [H,T] = impz(B,A,N). Impulse response. 1000 iters. |
| `impzlength` | ✅ | 0.000 | 316.67× |  | MISMATCH | Sig: L = impzlength(B, A). 10000 iters. |
| `isallpass` | ✅ | 0.000 | 107.10× | 241.60× | OK | Sig: TF = isallpass(B, A). FIR coefficients. 10000 iters. |
| `isfir` | ✅ | 0.000 |  |  | N/A | Sig: TF = isfir(B, A). 10000 iters. |
| `islinphase` | ✅ | 0.000 | 261.13× |  | OK | Sig: TF = islinphase(B, A). 10000 iters. |
| `ismaxphase` | ✅ | 0.001 | 178.17× | 154.70× | OK | Sig: TF = ismaxphase(B, A). 10000 iters. |
| `isminphase` | ✅ | 0.000 | 270.37× | 260.14× | OK | Sig: TF = isminphase(B, A). 10000 iters. |
| `isstable` | ✅ | 0.000 | 219.41× | 155.13× | OK | Sig: TF = isstable(B, A). 10000 iters. |
| `phasedelay` | ✅ | 0.006 | 152.47× |  | MISMATCH | Sig: [P,W] = phasedelay(B,A,N). Phase delay. 1000 iters. |
| `phasez` | ✅ | 0.005 | 82.46× | 45.85× | MISMATCH | Sig: [P,W] = phasez(B,A,N). 256-pt phase response. 1000 iters. |
| `stepz` | ✅ | 0.002 | 40.07× |  | OK | Sig: [H,T] = stepz(B,A,N). 256-pt step response. 1000 iters. |
| `zerophase` | ✅ | 0.005 | 173.47× |  | MISMATCH | Sig: [HZ,W] = zerophase(B,A,N). Zero-phase. 1000 iters. |
| `zplane` | ❌ |  |  |  |  |  |

### Digital Filtering

**Namespace:** `signal.digital_filtering.*` + `signal.filter_implementation.*` (TF/SOS/SS/ZP conversions) — 8 ✅ + 0 ⚠️ / 41 = 19%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `bandpass` | ✅ | 0.540 | 113.63× |  | MISMATCH | Sig: Y = bandpass(X, [LO HI], FS). 100 iters. |
| `bandstop` | ✅ | 0.603 | 95.72× |  | MISMATCH | Sig: Y = bandstop(X, [LO HI], FS). 100 iters. |
| `cell2sos` | ❌ |  |  |  |  |  |
| `convmtx` | ✅ | 0.004 | 11.81× | 31.10× | OK | Sig: A = convmtx(H, N). 102x100 conv matrix. 1000 iters. |
| `ctf2zp` | ❌ |  |  |  |  | control TF → ZPK |
| `ctffilt` | ❌ |  |  |  |  | control TF filter |
| `dspfwiz` | ❌ |  |  |  |  |  |
| `eqtflength` | ❌ |  |  |  |  |  |
| `fftfilt` | ✅ | 1.769 | 1.93× | 5.18× | OK | Sig: Y = fftfilt(B, X). FFT-based 32-tap MA on 100k. 100 iters. |
| `filt2block` | ❌ |  |  |  |  |  |
| `filtfilt` | ✅ | 0.261 | 1.41× | 1.86× | OK | Sig: Y = filtfilt(B, A, X). Zero-phase forward+back. 100 iters. |
| `filtic` | ❌ |  |  |  |  | init state |
| `hampel` | ✅ | 0.726 | 0.22× |  | OK | Sig: Y = hampel(X). Outlier-resistant smoother. 100 iters. |
| `highpass` | ✅ | 0.283 | 196.45× |  | MISMATCH | Sig: Y = highpass(X, FPASS, FS). 100 iters. |
| `latc2tf` | ❌ |  |  |  |  | inverse |
| `latcfilt` | ❌ |  |  |  |  |  |
| `lowpass` | ✅ | 0.291 | 184.11× |  | MISMATCH | Sig: Y = lowpass(X, FPASS, FS). 10k pts, 100 Hz cutoff at fs=1k. 100 iters. |
| `medfilt1` | ✅ | 1.813 | 0.19× | 0.28× | MISMATCH | Sig: Y = medfilt1(X, K). 100k window=5. 100 iters. |
| `residuez` | ❌ |  |  |  |  |  |
| `scalefiltersections` | ❌ |  |  |  |  |  |
| `sgolayfilt` | ✅ | 0.117 | 1.13× | 2.57× | OK | Sig: Y = sgolayfilt(X, K, F). order=3 frame=11. 100 iters. |
| `sos2cell` | ❌ |  |  |  |  |  |
| `sos2ctf` | ❌ |  |  |  |  |  |
| `sos2ss` | ✅ | 0.001 | 20.77× | 1990.57× | MISMATCH | Sig: [A,B,C,D] = sos2ss(SOS). 1000 iters. |
| `sos2tf` | ✅ | 0.001 | 28.06× | 211.88× | OK | Sig: [B,A] = sos2tf(SOS). 1000 iters. |
| `sos2zp` | ✅ | 0.002 | 14.99× | 95.44× | OK | Sig: [Z,P,K] = sos2zp(SOS). 1000 iters. |
| `sosfilt` | ✅ | 0.102 | 0.43× | 0.29× | OK | Sig: Y = sosfilt(SOS, X). 10k pts. 100 iters. |
| `ss` | ✅ |  |  |  |  |  |
| `ss2sos` | ✅ | 0.001 | 97.24× |  | MISMATCH | Sig: SOS = ss2sos(A,B,C,D). 1000 iters. |
| `ss2zp` | ✅ |  |  |  | N/A | Sig: [Z,P,K] = ss2zp(A,B,C,D). 1000 iters. |
| `tf` | ✅ |  |  |  |  |  |
| `tf2latc` | ❌ |  |  |  |  | lattice |
| `tf2sos` | ✅ | 0.001 | 97.22× | 1564.99× | MISMATCH | Sig: SOS = tf2sos(B,A). 1000 iters. |
| `tf2ss` | ✅ | 0.000 | 14.43× | 3654.83× | MISMATCH | Sig: [A,B,C,D] = tf2ss(BS,AS). 1000 iters. SAVE on A. |
| `tf2zp` | ✅ | 0.001 | 21.65× | 2076.90× | OK | Sig: [Z,P,K] = tf2zp(B,A). 10000 iters. SAVE on Z. |
| `tf2zpk` | ✅ | 0.001 | 27.29× |  | OK | Sig: [Z,P,K] = tf2zpk(B,A). 10000 iters. |
| `zp2ctf` | ❌ |  |  |  |  |  |
| `zp2sos` | ✅ | 0.000 | 264.82× | 1334.55× | OK | Sig: SOS = zp2sos(Z,P,K). 1000 iters. |
| `zp2ss` | ✅ | 0.001 | 51.12× | 2385.86× | MISMATCH | Sig: [A,B,C,D] = zp2ss(Z,P,K). 1000 iters. |
| `zp2tf` | ✅ | 0.000 | 43.39× | 4351.90× | OK | Sig: [B,A] = zp2tf(Z,P,K). 10000 iters. |
| `zpk` | ✅ |  |  |  |  |  |
| `filter` | ✅ | 1.154 | 0.05× | 0.11× | OK | Sig: Y = filter(B, A, X). FIR-1 [1 -0.5] on 100k. 100 iters. |
| `filter2` | ✅ | 0.141 | 0.51× | 0.34× | OK | 128x128 image with 3x3 Laplacian kernel. 100 iters. |

### Multirate Signal Processing

**Namespace:** `signal.multirate.*` — 4 ✅ + 0 ⚠️ / 8 = 50%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `decimate` | ✅ | 1.190 | 2.29× | 5.87× | MISMATCH | Sig: Y = decimate(X, M). M=4. 100 iters. |
| `downsample` | ✅ | 0.042 | 1.96× | 0.73× | OK | Sig: Y = downsample(X, N). N=4. 1000 iters. |
| `fillgaps` | ❌ |  |  |  |  |  |
| `interp` | ✅ | 3.443 | 0.21× | 3.66× | MISMATCH | Sig: Y = interp(X, L). Upsample×4 with FIR. 100 iters. |
| `intfilt` | ✅ | 0.001 | 465.68× |  | MISMATCH | Sig: H = intfilt(R, L, ALPHA). FIR coeffs (alpha=0.5). 1000 iters. |
| `resample` | ✅ | 0.496 | 1.82× | 5.42× | MISMATCH | Sig: Y = resample(X, P, Q). 3:2. 100 iters. |
| `upfirdn` | ✅ | 0.023 | 4.96× | 0.64× | MISMATCH | Sig: Y = upfirdn(X, H, P, Q). 100 iters. |
| `upsample` | ✅ | 0.133 | 0.52× | 0.47× | OK | Sig: Y = upsample(X, N). N=4. 1000 iters. |

### Signal Modeling

**Namespace:** `signal.parametric.*` — 23 ✅ + 0 ⚠️ / 25 = 92%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `ac2poly` | ✅ |  |  |  |  |  |
| `ac2rc` | ✅ |  |  |  |  |  |
| `arburg` | ✅ |  |  |  |  | Burg AR |
| `arcov` | ✅ |  |  |  |  | covariance AR |
| `armcov` | ✅ |  |  |  |  | modified cov AR |
| `aryule` | ✅ |  |  |  |  | Yule-Walker AR |
| `corrmtx` | ✅ |  |  |  |  | autocorr matrix |
| `invfreqs` | ✅ |  |  |  | OK | Levi LSQ; round-trip recovers source coefficients to machine precision. |
| `invfreqz` | ✅ |  |  |  | OK | Same, in z⁻¹ form. Iterative S-K refinement deferred. |
| `is2rc` | ✅ |  |  |  |  |  |
| `lar2rc` | ✅ |  |  |  |  |  |
| `levinson` | ✅ |  |  |  |  | Levinson-Durbin |
| `lpc` | ✅ |  |  |  |  | linear prediction |
| `lsf2poly` | ✅ |  |  |  |  |  |
| `poly2ac` | ✅ |  |  |  |  |  |
| `poly2lsf` | ✅ |  |  |  |  |  |
| `poly2rc` | ✅ |  |  |  |  |  |
| `prony` | ✅ |  |  |  |  | Prony method |
| `rc2ac` | ✅ |  |  |  |  |  |
| `rc2is` | ✅ |  |  |  |  |  |
| `rc2lar` | ✅ |  |  |  |  |  |
| `rc2poly` | ✅ |  |  |  |  |  |
| `rlevinson` | ✅ |  |  |  |  | reverse Levinson |
| `schurrc` | ✅ | 0.003 | 226.65× |  | OK | Sig: K = schurrc(R). Schur reflection coefficients from autocorrelation R, length numel(R)-1. Element-wise SAVE. |
| `stmcb` | ❌ |  |  |  |  | Steiglitz-McBride |

### Correlation and Convolution

**Namespace:** `signal.convolution.*` — 0 ✅ + 0 ⚠️ / 9 = 0%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `alignsignals` | ✅ | 0.099 | 2.19× |  | MISMATCH | Sig: [X1, X2] = alignsignals(A, B). 1000-pt signals. 100 iters. |
| `cconv` | ✅ | 10.545 | 0.02× | 0.03× | OK | Sig: C = cconv(A, B). Circular convolution. 100 iters. |
| `convmtx` | ✅ | 0.004 | 11.81× | 31.10× | OK | Sig: A = convmtx(H, N). 102x100 conv matrix. 1000 iters. |
| `corrmtx` | ✅ |  |  |  |  | autocorr matrix |
| `dtw` | ❌ |  |  |  |  | dynamic time warp |
| `edr` | ❌ |  |  |  |  | edit distance on real |
| `finddelay` | ✅ | 0.090 | 2.08× |  | OK | Sig: D = finddelay(A, B). 1000 iters. |
| `findsignal` | ❌ |  |  |  |  | pattern search |
| `xcorr2` | ✅ | 0.229 | 0.14× | 0.19× | OK | Sig: C = xcorr2(A, B). 32x32 vs 8x8. 1000 iters. |
| `conv` | ✅ | 0.025 | 0.78× | 1.24× | OK | Sig: C = conv(A, B). Deterministic 1k * 100 conv. 100 iters. Element-wise SAVE. |
| `conv2` | ✅ | 0.318 | 0.25× | 0.34× | OK | 128x128 image, 7x7 averaging kernel, 'same' shape. 100 iters. |
| `convn` | ✅ | 0.028 | 2.06× | 0.85× | OK | 64x64 2-D image / convn dispatch (delegates to conv2). 100 iters. |
| `deconv` | ✅ | 0.001 |  | 67.45× | OK | Sig: [Q,R] = deconv(U, V). Polynomial division. 10k iters. |

### Transforms

**Namespace:** `signal.transforms.*`. Promotions in core: `fft, ifft, fftshift, ifftshift`. Future wavelet split: `cwt/dwt/modwt/...` → `wavelet.*` — 6 ✅ + 0 ⚠️ / 32 = 18%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `bitrevorder` | ✅ | 0.003 | 133.74× | 204.18× | OK | Sig: Y = bitrevorder(X). Bit-reverse permutation. 10000 iters. |
| `cceps` | ✅ | 0.029 | 5.63× | 3.79× | OK | Sig: Y = cceps(X). Complex cepstrum. 100 iters. |
| `czt` | ❌ |  |  |  |  | chirp Z-transform |
| `dct` | ✅ | 3.978 | 0.02× | 0.02× | OK | Sig: Y = dct(X). 1024-pt DCT. 1000 iters. |
| `dftmtx` | ✅ | 0.034 | 1.55× | 1.24× | OK | Sig: F = dftmtx(N). 64x64 DFT matrix. 1000 iters. |
| `digitrevorder` | ❌ |  |  |  |  |  |
| `dlistft` | ❌ |  |  |  |  |  |
| `dlstft` | ❌ |  |  |  |  |  |
| `emd` | ❌ |  |  |  |  | empirical mode decomp |
| `envelope` | ✅ | 0.666 | 0.39× |  | MISMATCH | Sig: [UP, LO] = envelope(X). Hilbert envelope. 100 iters. SAVE on UP. |
| `fsst` | ❌ |  |  |  |  | Fourier synchrosqueezed |
| `fwht` | ❌ |  |  |  |  | fast Walsh-Hadamard |
| `goertzel` | ✅ | 0.066 | 2.56× |  | OK | Sig: Y = goertzel(X, F). 41 freq bins. 100 iters. |
| `hht` | ❌ |  |  |  |  | Hilbert-Huang |
| `hilbert` | ✅ | 0.004 | 179.00× | 26.58× | OK | Sig: H = hilbert(X). Analytic signal: real(H)=X, imag(H)=+H{X}. MATLAB R2025b sign convention: positive frequencies multiplied by +i. After fix in libs/signal/src/transforms/hilbert.cpp (added trailing conjugation to compensate for numkit's IFFT-direction FFT primitive). Closes audit/findings/signal/hilbert.md. |
| `icceps` | ✅ | 0.034 | 2.11× |  | OK | Sig: Y = icceps(C). Inverse complex cepstrum. 100 iters. |
| `idct` | ✅ | 3.917 | 0.02× | 0.03× | OK | Sig: y = idct(X). Inverse DCT 1024-pt. 1000 iters. |
| `ifsst` | ❌ |  |  |  |  |  |
| `ifwht` | ❌ |  |  |  |  | inverse |
| `instfreq` | ✅ |  |  |  |  | instantaneous frequency |
| `istft` | ❌ |  |  |  |  | inverse |
| `istftlayer` | ❌ |  |  |  |  |  |
| `pspectrum` | ❌ |  |  |  |  | easy spectral analysis |
| `rceps` | ✅ | 0.024 | 4.97× | 4.25× | OK | Sig: Y = rceps(X). Real cepstrum. 1000 iters. |
| `spectrogram` | ✅ | 0.102 | 7.88× |  | OK | Sig: [S, F, T] = spectrogram(X, NFFT). 100 iters. SAVE on S magnitude. |
| `stft` | ❌ |  |  |  |  | short-time FFT |
| `stftlayer` | ❌ |  |  |  |  |  |
| `stftmag2sig` | ❌ |  |  |  |  |  |
| `vmd` | ❌ |  |  |  |  | variational MD |
| `wvd` | ❌ |  |  |  |  | Wigner-Ville |
| `xspectrogram` | ❌ |  |  |  |  | cross-spectrogram |
| `xwvd` | ❌ |  |  |  |  | cross WVD |
| `fft` | ✅ | 0.004 |  |  | N/A | Sig: Y = fft(X). 1024-pt FFT on sin. 1000 iters. Custom fp (complex out). |
| `fft2` | ✅ | 1.127 | 0.60× | 0.58× | OK | 256x256 deterministic test signal, complex 2-D FFT. 50 iters. |
| `fftn` | ❌ |  |  |  |  | N-D FFT |
| `fftshift` | ✅ | 0.008 | 69.39× | 52.90× | OK | Sig: Y = fftshift(X[, dim]). Cyclic shift along every non-singleton dim by ceil(extent/2); inverse ifftshift uses floor(extent/2). Bug fix: numkit had fftshift/ifftshift swapped for odd N + flat-shift instead of per-dim for matrices + dim arg ignored. tol=0 (integer-stable). |
| `fftw` | ❌ |  |  |  |  | wisdom file |
| `ifft` | ✅ | 0.010 | 0.67× | 4.15× | OK | Sig: y = ifft(Y). 1024-pt inverse. 1000 iters. |
| `ifft2` | ✅ | 1.840 | 0.38× | 0.57× | OK | 256x256 inverse 2-D FFT (after fft2 of deterministic signal). 50 iters. |
| `ifftn` | ❌ |  |  |  |  | N-D FFT |
| `ifftshift` | ✅ | 0.005 | 86.45× | 54.01× | OK | Sig: Y = ifftshift(X[, dim]). Inverse of fftshift; cyclic shift along every non-singleton dim by floor(extent/2). Joint fix with fftshift on 2026-05-08 — they were swapped for odd-extent dims. tol=0. |
| `interpft` | ✅ | 0.012 | 2.33× | 16.15× | OK | 256-pt band-limited signal interpolated to 1024 points. 200 iters, element-wise. |
| `nextpow2` | ✅ | 0.007 | 67.71× |  | OK | Sig: P = nextpow2(N). Smallest p such that 2^p >= |N|. Vectorised. Edges: |x|=0 -> 0; NaN -> NaN; ±Inf -> +Inf; complex z -> uses |z|. Bug fix 2026-05-08: complex input previously threw; NaN/Inf paths now match MATLAB. tol=0. |
| `nufft` | ❌ |  |  |  |  | non-uniform |
| `nufftn` | ❌ |  |  |  |  | non-uniform |

### Windows

**Namespace:** `signal.windows.*` — 6 ✅ + 0 ⚠️ / 24 = 25%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `barthannwin` | ✅ | 0.004 | 4.96× | 6.35× | OK | Sig: W = barthannwin(N). Bartlett-Hann. 10000 iters. |
| `bartlett` | ✅ | 0.002 | 6.14× | 10.10× | OK | Sig: W = bartlett(N). 1024-pt triangular. 10000 iters. |
| `blackman` | ✅ | 0.007 | 4.61× | 3.81× | OK | Sig: W = blackman(N). 1024-pt Blackman. 10000 iters. |
| `blackmanharris` | ✅ | 0.010 | 2.84× | 3.91× | OK | Sig: W = blackmanharris(N). 4-term Blackman-Harris. 10000 iters. |
| `bohmanwin` | ✅ | 0.007 | 3.52× | 5.92× | OK | Sig: W = bohmanwin(N). Bohman. 10000 iters. |
| `chebwin` | ✅ | 0.024 | 0.83× | 7.41× | MISMATCH | Sig: W = chebwin(N, R). Dolph-Chebyshev. 1000 iters. |
| `dpss` | ❌ |  |  |  |  | discrete prolate spheroidal |
| `dpssclear` | ❌ |  |  |  |  | cache |
| `dpssdir` | ❌ |  |  |  |  | cache |
| `dpssload` | ❌ |  |  |  |  | cache |
| `dpsssave` | ❌ |  |  |  |  | cache |
| `enbw` | ✅ |  |  |  |  | equivalent noise BW |
| `flattopwin` | ✅ | 0.013 | 2.99× | 3.20× | OK | Sig: W = flattopwin(N). Flat-top. 10000 iters. |
| `gausswin` | ✅ | 0.004 | 5.68× | 5.12× | OK | Sig: W = gausswin(N). Gaussian. 10000 iters. |
| `hamming` | ✅ | 0.004 | 6.66× | 4.44× | OK | Sig: W = hamming(N). 1024-pt Hamming. 10000 iters. |
| `hann` | ✅ | 0.004 | 7.54× | 6.01× | OK | Sig: W = hann(N). 1024-pt Hann window. 10000 iters. |
| `kaiser` | ✅ | 0.019 | 1.63× | 13.62× | OK | Sig: W = kaiser(N, BETA). beta=5. 10000 iters. |
| `nuttallwin` | ✅ | 0.010 | 2.43× | 3.99× | OK | Sig: W = nuttallwin(N). 10000 iters. |
| `parzenwin` | ✅ | 0.001 | 43.75× | 39.21× | OK | Sig: W = parzenwin(N). 10000 iters. |
| `rectwin` | ✅ | 0.001 | 1.62× | 7.42× | OK | Sig: W = rectwin(N). All-ones. 10000 iters. |
| `taylorwin` | ✅ | 0.013 | 3.16× | 7.12× | MISMATCH | Sig: W = taylorwin(N). 1024-pt Taylor window. 1000 iters. |
| `triang` | ✅ | 0.001 | 8.97× | 15.16× | OK | Sig: W = triang(N). Triangular. 10000 iters. |
| `tukeywin` | ✅ | 0.002 | 9.45× | 26.31× | OK | Sig: W = tukeywin(N, R). r=0.5. 10000 iters. |
| `wvtool` | ❌ |  |  |  |  | GUI |

### Parametric Spectral Estimation

**Namespace:** `signal.spectral_analysis.*`. Magnitude utils (`db/db2mag/mag2db/pow2db`) → core (cross-cutting math) — 3 ✅ + 0 ⚠️ / 10 = 30%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `db` | ✅ | 0.246 | 1.04× |  | OK | Sig: D = db(X). magnitude → dB. 100k iters. |
| `db2mag` | ✅ | 0.861 | 0.70× | 1.37× | OK | Sig: M = db2mag(D). 100k iters. |
| `db2pow` | ✅ | 0.645 | 0.93× | 1.92× | OK | Sig: P = db2pow(D). 100k pts. 1000 iters. |
| `findpeaks` | ✅ | 0.018 | 32.81× |  | OK | Sig: [PKS, LOC] = findpeaks(X). 100 iters. |
| `mag2db` | ✅ | 0.451 | 0.53× | 2.53× | OK | Sig: D = mag2db(M). 100k iters. |
| `pburg` | ✅ |  |  |  | OK | AR PSD via Burg lattice; AR(2) peak recovery within 1 PSD bin |
| `pcov` | ❌ |  |  |  |  |  |
| `pmcov` | ❌ |  |  |  |  |  |
| `pow2db` | ✅ | 0.247 | 0.96× | 4.59× | OK | Sig: D = pow2db(P). 100k iters. |
| `pyulear` | ✅ |  |  |  | OK | AR PSD via Levinson-Durbin; agrees with pburg to 4 decimals |

### Nonparametric Spectral Estimation

**Namespace:** `signal.spectral_analysis.*` — 6 ✅ + 0 ⚠️ / 17 = 35%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `cpsd` | ✅ |  |  |  | OK | one-sided complex cross-PSD via Welch; Sxx-identity match to 0 |
| `db` | ✅ | 0.246 | 1.04× |  | OK | Sig: D = db(X). magnitude → dB. 100k iters. |
| `db2mag` | ✅ | 0.861 | 0.70× | 1.37× | OK | Sig: M = db2mag(D). 100k iters. |
| `db2pow` | ✅ | 0.645 | 0.93× | 1.92× | OK | Sig: P = db2pow(D). 100k pts. 1000 iters. |
| `findpeaks` | ✅ | 0.018 | 32.81× |  | OK | Sig: [PKS, LOC] = findpeaks(X). 100 iters. |
| `mag2db` | ✅ | 0.451 | 0.53× | 2.53× | OK | Sig: D = mag2db(M). 100k iters. |
| `mscohere` | ✅ |  |  |  | OK | \|Pxy\|² / (Pxx·Pyy); auto = 1 exactly, LTI ≈ 0.97 |
| `periodogram` | ✅ | 0.010 |  | 11.41× | MISMATCH | Sig: [PXX, F] = periodogram(X). 1024-pt PSD. 100 iters. SAVE on PXX. |
| `plomb` | ❌ |  |  |  |  | Lomb-Scargle |
| `pmtm` | ❌ |  |  |  |  | multi-taper |
| `poctave` | ❌ |  |  |  |  |  |
| `pow2db` | ✅ | 0.247 | 0.96× | 4.59× | OK | Sig: D = pow2db(P). 100k iters. |
| `pspectrum` | ❌ |  |  |  |  | easy spectral analysis |
| `pwelch` | ✅ | 0.063 | 19.59× | 14.55× | MISMATCH | Sig: [PXX, F] = pwelch(X). Welch PSD. 100 iters. |
| `refinepeaks` | ❌ |  |  |  |  |  |
| `spectralentropy` | ✅ |  |  |  |  |  |
| `tfestimate` | ✅ |  |  |  | OK | Pyx/Pxx; auto = 1 exactly, FIR LTI recovers \|H(f)\| within 0.018 |

### Spectral Measurements

**Namespace:** `signal.spectral_analysis.*` — 0 ✅ + 0 ⚠️ / 18 = 0%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `bandpower` | ✅ |  |  |  |  |  |
| `enbw` | ✅ |  |  |  |  | equivalent noise BW |
| `instbw` | ✅ |  |  |  |  |  |
| `instfreq` | ✅ |  |  |  |  | instantaneous frequency |
| `meanfreq` | ✅ |  |  |  |  | mean frequency |
| `medfreq` | ✅ |  |  |  |  | median frequency |
| `obw` | ✅ |  |  |  |  |  |
| `powerbw` | ✅ |  |  |  |  |  |
| `sfdr` | ✅ |  |  |  |  | spurious-free dynamic range |
| `sinad` | ✅ |  |  |  |  | signal-noise-distortion |
| `snr` | ✅ |  |  |  |  | signal-to-noise |
| `spectralcrest` | ✅ |  |  |  |  |  |
| `spectralentropy` | ✅ |  |  |  |  |  |
| `spectralflatness` | ✅ |  |  |  |  |  |
| `spectralkurtosis` | ✅ |  |  |  |  |  |
| `spectralskewness` | ✅ |  |  |  |  |  |
| `thd` | ✅ |  |  |  |  | total harmonic distortion |
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
| `instbw` | ✅ |  |  |  |  |  |
| `instfreq` | ✅ |  |  |  |  | instantaneous frequency |
| `iscola` | ❌ |  |  |  |  |  |
| `istft` | ❌ |  |  |  |  | inverse |
| `istftlayer` | ❌ |  |  |  |  |  |
| `kurtogram` | ❌ |  |  |  |  |  |
| `pspectrum` | ❌ |  |  |  |  | easy spectral analysis |
| `spectralcrest` | ✅ |  |  |  |  |  |
| `spectralentropy` | ✅ |  |  |  |  |  |
| `spectralflatness` | ✅ |  |  |  |  |  |
| `spectralkurtosis` | ✅ |  |  |  |  |  |
| `spectralskewness` | ✅ |  |  |  |  |  |
| `spectrogram` | ✅ | 0.102 | 7.88× |  | OK | Sig: [S, F, T] = spectrogram(X, NFFT). 100 iters. SAVE on S magnitude. |
| `stft` | ❌ |  |  |  |  | short-time FFT |
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
| `dutycycle` | ✅ |  |  |  |  | duty cycle |
| `falltime` | ✅ |  |  |  |  |  |
| `midcross` | ✅ |  |  |  |  | mid-ref crossings |
| `overshoot` | ✅ |  |  |  |  |  |
| `pulseperiod` | ✅ |  |  |  |  |  |
| `pulsesep` | ✅ |  |  |  |  |  |
| `pulsewidth` | ✅ |  |  |  |  |  |
| `risetime` | ✅ |  |  |  |  |  |
| `settlingtime` | ✅ |  |  |  |  |  |
| `slewrate` | ✅ |  |  |  |  |  |
| `statelevels` | ✅ |  |  |  |  |  |
| `undershoot` | ✅ |  |  |  |  |  |

### Signal Descriptive Statistics

**Namespace:** `signal.measurements.*` — 2 ✅ + 0 ⚠️ / 30 = 6%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `alignsignals` | ✅ | 0.099 | 2.19× |  | MISMATCH | Sig: [X1, X2] = alignsignals(A, B). 1000-pt signals. 100 iters. |
| `binmask2sigroi` | ❌ |  |  |  |  |  |
| `countlabels` | ❌ |  |  |  |  |  |
| `cusum` | ❌ |  |  |  |  | CUSUM change detection |
| `dtw` | ❌ |  |  |  |  | dynamic time warp |
| `edr` | ❌ |  |  |  |  | edit distance on real |
| `envelope` | ✅ | 0.666 | 0.39× |  | MISMATCH | Sig: [UP, LO] = envelope(X). Hilbert envelope. 100 iters. SAVE on UP. |
| `extendsigroi` | ❌ |  |  |  |  |  |
| `extractsigroi` | ❌ |  |  |  |  |  |
| `filenames2labels` | ❌ |  |  |  |  |  |
| `findchangepts` | ❌ |  |  |  |  | change-point detection |
| `finddelay` | ✅ | 0.090 | 2.08× |  | OK | Sig: D = finddelay(A, B). 1000 iters. |
| `findpeaks` | ✅ | 0.018 | 32.81× |  | OK | Sig: [PKS, LOC] = findpeaks(X). 100 iters. |
| `findsignal` | ❌ |  |  |  |  | pattern search |
| `folders2labels` | ❌ |  |  |  |  |  |
| `framelbl` | ❌ |  |  |  |  |  |
| `framesig` | ❌ |  |  |  |  |  |
| `meanfreq` | ✅ |  |  |  |  | mean frequency |
| `medfreq` | ✅ |  |  |  |  | median frequency |
| `mergesigroi` | ❌ |  |  |  |  |  |
| `peak2peak` | ✅ | 3.066 | 0.03× | 0.52× | OK | Sig: P = peak2peak(X). 1M-pt range. 100 iters. |
| `peak2rms` | ✅ | 3.127 | 0.87× | 1.16× | OK | Sig: R = peak2rms(X). 100 iters. |
| `removesigroi` | ❌ |  |  |  |  |  |
| `rssq` | ✅ | 2.638 | 0.10× | 0.16× | OK | Sig: R = rssq(X). 100 iters. |
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
| `hampel` | ✅ | 0.726 | 0.22× |  | OK | Sig: Y = hampel(X). Outlier-resistant smoother. 100 iters. |
| `medfilt1` | ✅ | 1.813 | 0.19× | 0.28× | MISMATCH | Sig: Y = medfilt1(X, K). 100k window=5. 100 iters. |
| `sgolay` | ✅ | 0.001 | 16.08× | 214.22× | OK | Sig: B = sgolay(K, F). order=3 frame=11. 1000 iters. |
| `sgolayfilt` | ✅ | 0.117 | 1.13× | 2.57× | OK | Sig: Y = sgolayfilt(X, K, F). order=3 frame=11. 100 iters. |

### Vibration Analysis

**Namespace:** `signal.vibration.*` — 0 ✅ + 0 ⚠️ / 13 = 0%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `envspectrum` | ✅ |  |  |  |  | envelope spectrum |
| `modalfit` | ❌ |  |  |  |  | modal-fit |
| `modalfrf` | ❌ |  |  |  |  |  |
| `modalsd` | ❌ |  |  |  |  |  |
| `orderspectrum` | ❌ |  |  |  |  |  |
| `ordertrack` | ❌ |  |  |  |  |  |
| `orderwaveform` | ❌ |  |  |  |  |  |
| `rainflow` | ✅ |  |  |  |  |  |
| `rpmfreqmap` | ❌ |  |  |  |  |  |
| `rpmordermap` | ❌ |  |  |  |  |  |
| `rpmtrack` | ❌ |  |  |  |  | order tracking |
| `tachorpm` | ✅ |  |  |  |  | tachometer→RPM |
| `tsa` | ✅ |  |  |  |  |  |

## Statistics

### Descriptive Statistics

**Namespace:** `stats.descriptive.*` / `stats.moving.*` / `stats.nan.*`. Exception: `xcorr/xcov/rms/rssq/peak2peak/peak2rms` → `signal.*` (signal-side stats) — 14 ✅ + 0 ⚠️ / 33 = 42%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `bounds` | ✅ | 6.271 | 0.02× | 0.25× | OK | Sig: [lo,hi] = bounds(X). 1M-pt min/max. 100 iters. |
| `corrcoef` | ✅ | 0.070 | 2.30× | 5.01× | OK | Sig: R = corrcoef(M). 2-col 10k matrix. 100 iters. |
| `cov` | ✅ | 0.030 | 1.02× | 1.75× | OK | Sig: C = cov(M). 2-col 10k cov matrix. 1000 iters. |
| `cummax` | ✅ | 2.385 | 1.08× | 1.17× | OK | Sig: M = cummax(X). 1M-pt cumulative max. 100 iters. Element-wise SAVE. |
| `cummin` | ✅ | 2.504 | 1.05× | 1.04× | OK | Sig: M = cummin(X). 1M-pt cumulative min. 100 iters. Element-wise SAVE. |
| `iqr` | ✅ | 0.006 | 1020.04× | 242.82× | OK | Sig: r = iqr(A[, dim | 'all' | vecdim]). MATLAB R2025b uses midpoint (R2007a) interpolation: iqr = prctile(A, 75) - prctile(A, 25). Closes audit/findings/stats/iqr.md (joint with quantile + prctile). |
| `kde` | ❌ |  |  |  |  |  |
| `mape` | ✅ | 9.431 | 0.28× | 0.98× | OK | 1M-point MAPE. 50 iters. numkit needs `import compat.*`; MATLAB+Octave have it flat. |
| `max` | ✅ | 1.462 | 0.04× | 0.54× | OK | Sig: M = max(X). 1M-pt. 100 iters. Scalar fp. |
| `maxk` | ✅ | 77.386 | 0.01× |  | OK | Sig: B = maxk(X, K). Top 10 of 1M. 100 iters. |
| `mean` | ✅ | 1.357 | 0.06× | 0.74× | OK | Sig: M = mean(X). 1M-pt sin reduction. 100 iters. Scalar fp. |
| `median` | ✅ | 3.330 | 1.47× | 2.30× | OK | Sig: M = median(X). 1M-pt full sort + middle. 50 iters. Scalar fp. |
| `min` | ✅ | 1.435 | 0.03× | 0.55× | OK | Sig: M = min(X). 1M-pt. 100 iters. Scalar fp. |
| `mink` | ✅ | 77.248 | 0.01× |  | OK | Sig: B = mink(X, K). Bot 10 of 1M. 100 iters. |
| `mode` | ✅ | 18.749 | 0.48× | 2.75× | OK | Sig: M = mode(X). 1M-pt with ~7919 distinct vals. 50 iters. Scalar fp. |
| `movmad` | ✅ | 0.008 | 22.17× | 727.46× | OK | Sig: movmad(A, k[, dim] [, nanflag] [, Name, Value]). Same surface as movmean. Closes audit/findings/stats/movmad.md. |
| `movmax` | ✅ | 0.007 | 24.13× | 264.51× | OK | Sig: movmax(A, k[, dim] [, nanflag] [, Name, Value]). Same surface as movmean. Closes audit/findings/stats/movmax.md. |
| `movmean` | ✅ | 0.010 | 25.75× | 681.58× | OK | Sig: M = movmean(A, k[, dim] [, nanflag] [, Name, Value]). nanflag in {includemissing|includenan (default)|omitmissing|omitnan}. Endpoints in {shrink (default)|discard|fill|scalar}. SamplePoints not yet implemented (parity gap, throws with documented error). DataVariables/ReplaceValues are table-only and throw too. k=0 throws MATLAB-matching error. Verified: NaN propagation default, omitnan/omitmissing alias, includenan explicit, all four Endpoints modes, combined matrix+dim+nanflag+endpoints. Closes audit/findings/stats/movmean.md. |
| `movmedian` | ✅ | 0.007 | 24.08× | 371.45× | OK | Sig: M = movmedian(A, k[, dim] [, nanflag] [, Name, Value]). Same surface as movmean. Closes audit/findings/stats/movmedian.md. |
| `movmin` | ✅ | 0.007 | 23.06× | 719.83× | OK | Sig: movmin(A, k[, dim] [, nanflag] [, Name, Value]). Same surface as movmean. Closes audit/findings/stats/movmin.md. |
| `movprod` | ✅ | 0.005 | 32.06× | 319.88× | OK | Sig: movprod(A, k[, dim] [, nanflag] [, Name, Value]). Same surface as movmean. Closes audit/findings/stats/movprod.md. |
| `movstd` | ✅ | 0.005 | 36.77× | 597.50× | OK | Sig: movstd(A, k[, normFlag] [, dim] [, nanflag] [, Name, Value]). normFlag in {0 (default, N-1), 1 (N)}. Same nanflag/Endpoints surface as movmean. Closes audit/findings/stats/movstd.md. |
| `movsum` | ✅ | 0.005 | 36.74× | 334.71× | OK | Sig: movsum(A, k[, dim] [, nanflag] [, Name, Value]). Same surface as movmean. Closes audit/findings/stats/movsum.md. |
| `movvar` | ✅ | 0.010 | 15.90× | 273.40× | OK | Sig: movvar(A, k[, normFlag] [, dim] [, nanflag] [, Name, Value]). normFlag in {0 (default, N-1), 1 (N)}. Same nanflag/Endpoints surface as movmean. Closes audit/findings/stats/movvar.md. |
| `prctile` | ✅ | 0.006 | 933.71× |  | OK | Sig: P = prctile(A, p [, dim | 'all' | vecdim] [, Method=method]). Same surface as quantile but p in [0, 100]. Closes audit/findings/stats/prctile.md. |
| `quantile` | ✅ | 0.011 | 524.51× |  | OK | Sig: Q = quantile(A, p [, dim | 'all' | vecdim] [, Method=method]). Default = 'midpoint' (MATLAB R2025b R2007a algorithm), positions (k-0.5)/N. Methods: midpoint (default) | inclusive (Type-7) | exclusive (Type-6) | approximate (falls back to midpoint). Integer-n form (quantile(A, n) for evenly-spaced quantiles) NOT yet supported — pass an explicit p vector. Closes audit/findings/stats/quantile.md. |
| `rms` | ✅ | 2.673 | 0.50× | 0.17× | OK | Sig: R = rms(X). 1M-pt sin RMS. 100 iters. Scalar fp. |
| `rmse` | ✅ | 8.994 | 0.26× | 2.19× | OK | Sig: R = rmse(F, A). 1M-pt. 100 iters. |
| `std` | ✅ | 0.008 | 256.96× | 81.61× | OK | Sig: S = std(A[, w | W][, dim | 'all' | vecdim][, nanflag]). Same surface as var. Closes audit/findings/stats/std.md. |
| `summary` | ❌ |  |  |  |  |  |
| `var` | ✅ | 0.012 | 171.62× | 73.92× | OK | Sig: V = var(A[, w | W][, dim | 'all' | vecdim][, nanflag]). w in {0, 1} or vector W (weighted; denominator = sum(W)). 'all' / full-flatten vecdim flatten input. Default nanflag = includenan (NaN poisons; matches MATLAB R2025b for double). Closes audit/findings/stats/var.md. |
| `xcorr` | ✅ | 0.959 | 0.20× | 1.08× | OK | Sig: C = xcorr(X). Auto-correlation 5k-pt. 100 iters. |
| `xcov` | ✅ | 1.011 | 0.36× | 0.99× | OK | Cross-cov of 5k-pt sine. 50 iters. |

### Descriptive Statistics — extras

**Namespace:** `stats.descriptive.*` — additions on top of the existing section above. 0 ✅ + 0 ⚠️ / 23 = 0%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `cholcov` | ❌ |  |  |  |  | Cholesky-of-cov, handles PSD |
| `corr` | ❌ |  |  |  |  | (with type='Spearman'/'Kendall' options) |
| `corrcov` | ❌ |  |  |  |  | covariance → correlation |
| `crosstab` | ❌ |  |  |  |  | cross-tabulation |
| `geomean` | ❌ |  |  |  |  | geometric mean |
| `grpstats` | ❌ |  |  |  |  | group-wise statistics |
| `harmmean` | ❌ |  |  |  |  | harmonic mean |
| `kurtosis` | ❌ |  |  |  |  | already partially via `stats.descriptive`; here MATLAB stats version |
| `mad` | ❌ |  |  |  |  | mean / median absolute deviation |
| `moment` | ❌ |  |  |  |  | central moment of order k |
| `nearcorr` | ❌ |  |  |  |  | nearest correlation matrix |
| `partialcorr` | ❌ |  |  |  |  |  |
| `partialcorri` | ❌ |  |  |  |  | with internal vars |
| `range` | ❌ |  |  |  |  | max - min |
| `robustcov` | ❌ |  |  |  |  | robust covariance estimator (FAST-MCD) |
| `skewness` | ❌ |  |  |  |  |  |
| `tabulate` | ❌ |  |  |  |  | frequency table |
| `tiedrank` | ❌ |  |  |  |  | ranks with tie correction |
| `trimmean` | ❌ |  |  |  |  | trimmed mean |
| `zscore` | ✅ |  |  |  | OK | alias for normalize(A,'zscore'); per-column on matrices |
| `nancov` | ❌ |  |  |  |  | NaN-aware covariance |
| `nansum` | ❌ |  |  |  |  | (legacy alias of stats.nan.nansum) |
| `nanmean` | ❌ |  |  |  |  | (legacy alias) |

### Probability Distributions

**Namespace:** `stats.dist.*` — 115 ✅ + 0 ⚠️ / 130+ = 88%

Each distribution provides 5 entrypoints: `*pdf` / `*cdf` / `*inv` (or `*icdf`) / `*rnd` / `*stat`. All `rnd` functions share `numkit::builtin::sharedEngine()` so `rng(seed)` reseeds them. Discrete `*inv` use one-ULP relative tolerance against the public cdf so `inv(cdf(k))=k` round-trips don't overshoot.

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `normpdf` | ✅ | 0.006 | 78.86× | 87.33× | OK | Sig: y = normpdf(x[, mu, sigma]). Normal PDF: (1/(σ√(2π)))·exp(-(x-μ)²/(2σ²)). Defaults mu=0, sigma=1. sigma<=0 => NaN. Vectorised. |
| `normcdf` | ✅ |  |  |  | OK |  |
| `norminv` | ✅ | 0.008 | 70.50× | 79.36× | MISMATCH | Sig: x = norminv(p[, mu, sigma]). Inverse Normal CDF: x = mu + sigma*Φ⁻¹(p). Defaults mu=0, sigma=1. Edges: p=0 => -Inf; p=1 => +Inf; p outside [0,1] => NaN; sigma<=0 => NaN. |
| `normrnd` | ✅ |  |  |  | OK |  |
| `normstat` | ✅ | 0.006 | 131.54× | 51.12× | OK | Sig: [m, v] = normstat(mu, sigma). Trivially m=mu, v=sigma². Vectorised with broadcasting (equal sizes or one scalar). sigma<=0 => NaN. |
| `chi2pdf` | ✅ | 0.008 | 498.87× | 110.44× | OK | Sig: y = chi2pdf(x, k). Chi-squared PDF with k dof. x < 0 => 0; k <= 0 => NaN. Covers: scalar, vector x, x<0 + x=0 edges, k=1 (special: x^(-1/2)·exp(-x/2)/√(2π)), k=30 large dof. |
| `chi2cdf` | ✅ |  |  |  | OK | gammainc(x/2, k/2) |
| `chi2inv` | ✅ | 0.012 | 73.76× | 1836.34× | OK | Sig: x = chi2inv(p, k). Inverse Chi² CDF with k dof. Covers k ∈ {1, 5, 30} × p ∈ {0.05, 0.5, 0.95} + p=0 (=> 0) + p=1 (=> Inf) + p outside [0,1] (=> NaN) + k<=0 (=> NaN). |
| `chi2rnd` | ✅ |  |  |  | OK |  |
| `chi2stat` | ✅ | 0.004 | 101.27× | 15.99× | OK | Sig: [m, v] = chi2stat(k). Chi² mean=k and variance=2k. Vectorised. k<=0 => NaN (moments don't exist for degenerate). |
| `tpdf` | ✅ | 0.008 | 164.21× | 94.15× | OK | Sig: y = tpdf(x, nu). Student's t PDF via lgamma-stable form. nu=Inf -> Gaussian limit (1/sqrt(2π))·exp(-x²/2). nu<=0 or NaN -> NaN. NaN x -> NaN. |
| `tcdf` | ✅ |  |  |  | OK | betainc on z = ν/(ν+x²), branch by sign |
| `tinv` | ✅ | 0.015 | 128.05× | 551.50× | OK | Sig: x = tinv(p, nu). Inverse Student's t-CDF. Uses betaincinv(2(1-p) or 2p, nu/2, 1/2) and signs by p<>0.5. nu=Inf -> Gaussian limit (norminv(p)). Edges: p=0 -> -Inf; p=1 -> +Inf; p outside [0,1] -> NaN; nu<=0 -> NaN. |
| `trnd` | ✅ |  |  |  | OK | Z/√(X/ν), Z~N(0,1), X~χ²(ν) |
| `tstat` | ✅ | 0.006 | 99.65× | 58.77× | OK | Sig: [m, v] = tstat(nu). Student's t: m=0 if nu>1, v=nu/(nu-2) if nu>2. Vectorised. nu<=0 => NaN/NaN; 0<nu<=1 => m=NaN,v=NaN; 1<nu<=2 => m=0, v=NaN. |
| `fpdf` | ✅ | 0.007 | 373.67× | 103.64× | OK | Sig: y = fpdf(x, v1, v2). F-distribution PDF. x < 0 => 0; v1 <= 0 or v2 <= 0 => NaN. Covers: scalar (v1=5,v2=10), vector x, x<0/x=0 edges, invalid v1/v2, F(2,10) at 0 (= v1/(v1+v2-2)/B(...) finite for v1=2). |
| `fcdf` | ✅ |  |  |  | OK | betainc(v1·x/(v1·x+v2), v1/2, v2/2) |
| `finv` | ✅ | 0.017 | 85.31× | 653.54× | OK | Sig: x = finv(p, v1, v2). Inverse F CDF. Covers (v1, v2) ∈ {(1,1), (5,10), (10,30)} × p ∈ {0.05, 0.5, 0.95} + p=0 (=> 0) + p=1 (=> Inf) + p outside [0,1] (=> NaN) + v1<=0 / v2<=0 (=> NaN). |
| `frnd` | ✅ |  |  |  | OK | (X1/v1)/(X2/v2), Xi~χ²(vi) |
| `fstat` | ✅ | 0.006 | 275.20× | 58.40× | OK | Sig: [m, v] = fstat(v1, v2). F-distribution mean = v2/(v2-2) for v2>2 else NaN; variance = 2*v2²(v1+v2-2)/(v1(v2-2)²(v2-4)) for v2>4 else NaN. Vectorised. v1<=0 or v2<=0 => NaN/NaN. |
| `betapdf` | ✅ | 0.007 | 415.31× | 86.47× | OK | Sig: y = betapdf(x, a, b). Beta PDF on (0,1). x outside (0,1) => 0; a<=0 or b<=0 => NaN. Covers: scalar, vector, out-of-(0,1) edges (x<0, x=0, x=0.5, x=1, x>1), invalid params. |
| `betacdf` | ✅ |  |  |  | OK | I_x(a, b) directly |
| `betainv` | ✅ | 0.015 | 72.93× | 583.32× | OK | Sig: x = betainv(p, a, b). Inverse Beta CDF. Covers (a,b) ∈ {(1,1) uniform, (0.5,0.5) U-shaped, (2,5), (10,10)} × p ∈ {0.05, 0.5, 0.95}. Edges: p=0 => 0; p=1 => 1; p outside [0,1] => NaN; invalid shape => NaN. |
| `betarnd` | ✅ |  |  |  | OK | U/(U+V), U~Gamma(a,1), V~Gamma(b,1) |
| `betastat` | ✅ | 0.006 | 202.07× | 24.93× | OK | Sig: [m, v] = betastat(a, b). Beta(a,b) mean a/(a+b) and variance ab/((a+b)^2(a+b+1)). Vectorised. Invalid params (a<=0 or b<=0) => NaN. Beta(1,1) is uniform: m=0.5, v=1/12. |
| `gampdf` | ✅ | 0.008 | 573.85× | 82.04× | OK | Sig: y = gampdf(x, a, b). Gamma(shape=a, scale=b) PDF. Density at 0: a<1 → Inf, a=1 → 1/b, a>1 → 0. x<0 → 0. a<0 or b<=0 → NaN. a=0 → 0 (degenerate). |
| `gamcdf` | ✅ |  |  |  | OK | gammainc(x/b, a) |
| `gaminv` | ✅ | 0.009 | 83.28× | 772.17× | OK | Sig: x = gaminv(p, a, b). Inverse Gamma CDF. q=0 → 0; q=1 → Inf; q outside [0,1] → NaN. a=0 → 0 (degenerate); a<0 / b<=0 → NaN. |
| `gamrnd` | ✅ |  |  |  | OK | std::gamma_distribution(a, b) |
| `gamstat` | ✅ | 0.006 | 73.72× | 65.48× | OK | Sig: [m, v] = gamstat(a, b). Gamma(shape, scale): m = a·b, v = a·b². Vectorised. a<=0 or b<=0 => NaN. |
| `exppdf` | ✅ | 0.007 | 116.61× | 56.99× | OK | Sig: y = exppdf(x[, mu]). Exponential PDF: (1/mu)·exp(-x/mu). Default mu=1. x<0 → 0. mu<=0 → NaN. |
| `expcdf` | ✅ |  |  |  | OK | -expm1(-x/μ) |
| `expinv` | ✅ | 0.007 | 112.71× | 110.87× | OK | Sig: x = expinv(p[, mu]). Inverse exponential CDF: x = -mu*log(1-p). Default mu=1. Covers default form + non-default mu + boundaries (p=0,1) + invalid (p<0, p>1, mu<=0). |
| `exprnd` | ✅ |  |  |  | OK |  |
| `expstat` | ✅ | 0.006 | 57.24× | 11.58× | OK | Sig: [m, v] = expstat(mu). Exponential mean=mu, variance=mu^2. Vectorised. mu<=0 => NaN. |
| `unifpdf` | ✅ | 0.009 | 107.97× |  | OK | Sig: y = unifpdf(x[, a, b]). Continuous uniform PDF on [a, b]; defaults a=0, b=1. y = 1/(b-a) for x in [a,b], else 0. Edges: b<=a -> NaN; NaN x -> NaN; NaN a/b -> 0 (NaN comparisons false, MATLAB convention). |
| `unifcdf` | ✅ |  |  |  | OK |  |
| `unifinv` | ✅ | 0.007 | 134.19× |  | OK | Sig: x = unifinv(p[, a, b]). Inverse Continuous Uniform CDF on [a, b]: x = a + p*(b-a). Defaults a=0, b=1. p=0 -> a; p=1 -> b; p outside [0,1] -> NaN; b<=a -> NaN; NaN p -> NaN. |
| `unifrnd` | ✅ |  |  |  | OK |  |
| `unifstat` | ✅ | 0.008 | 142.54× | 39.18× | OK | Sig: [m, v] = unifstat(a, b). Continuous uniform on [a,b]: m=(a+b)/2, v=(b-a)²/12. Vectorised. b<=a => NaN. |
| `lognpdf` | ✅ | 0.006 | 69.50× | 140.07× | OK | Sig: y = lognpdf(x[, mu, sigma]). Lognormal PDF. Defaults mu=0, sigma=1. x<=0 → 0. sigma<=0 → NaN. |
| `logncdf` | ✅ |  |  |  | OK |  |
| `logninv` | ✅ | 0.007 | 85.48× | 88.35× | OK | Sig: x = logninv(p[, mu, sigma]). Inverse Lognormal CDF. Defaults mu=0, sigma=1. q=0 → 0; q=1 → Inf; q outside [0,1] → NaN. sigma<=0 → NaN. |
| `lognrnd` | ✅ |  |  |  | OK |  |
| `lognstat` | ✅ | 0.009 | 46.35× | 31.11× | OK | Sig: [m, v] = lognstat(mu, sigma). Lognormal: m = exp(mu + sigma²/2), v = (exp(sigma²)-1)·exp(2mu + sigma²). Vectorised. sigma<=0 => NaN. |
| `wblpdf` | ✅ | 0.010 | 60.68× | 82.59× | OK | Sig: y = wblpdf(x[, a, b]). Weibull PDF with scale a, shape b. Defaults a=1, b=1 (= exponential). Edges: x<0 -> 0; x=0 -> b/a if b=1, Inf if b<1, 0 if b>1; a<=0 or b<=0 -> NaN; NaN -> NaN. |
| `wblcdf` | ✅ |  |  |  | OK |  |
| `wblinv` | ✅ | 0.009 | 78.92× | 110.55× | OK | Sig: x = wblinv(p[, a, b]). Inverse Weibull CDF: x = a · (-log(1-p))^(1/b). Defaults a=1, b=1 (= exponential -log(1-p)). p=0 -> 0; p=1 -> Inf; p outside [0,1] -> NaN; a<=0, b<=0 -> NaN; NaN -> NaN. |
| `wblrnd` | ✅ |  |  |  | OK |  |
| `wblstat` | ✅ | 0.007 | 99.87× | 55.78× | OK | Sig: [m, v] = wblstat(a, b). Weibull(scale=a, shape=b): m = a·Γ(1+1/b), v = a²·(Γ(1+2/b) - Γ(1+1/b)²). Vectorised. a<=0 or b<=0 => NaN. |
| `raylpdf` | ✅ | 0.005 | 184.23× | 47.40× | OK | Sig: y = raylpdf(x, b). Rayleigh PDF. x<0 → 0; x=0 → 0 (density at origin is 0). b<=0 → NaN. |
| `raylcdf` | ✅ |  |  |  | OK |  |
| `raylinv` | ✅ | 0.006 | 189.51× | 61.89× | OK | Sig: x = raylinv(p, b). Inverse Rayleigh CDF: x = b·sqrt(-2·ln(1-p)). q=0 → 0; q=1 → Inf; q outside [0,1] → NaN. b<=0 → NaN. |
| `raylrnd` | ✅ |  |  |  | OK | inverse-cdf sampling |
| `raylstat` | ✅ | 0.005 | 85.40× | 19.58× | OK | Sig: [m, v] = raylstat(b). Rayleigh: m = b·sqrt(π/2), v = b²·(2 - π/2). Vectorised. b<=0 => NaN. |
| `poisspdf` | ✅ | 0.008 | 282.39× | 55.51× | OK | Sig: y = poisspdf(k, lambda). Poisson PMF. Out-of-support k (<0, non-integer) → 0. lambda=0 degenerate: only k=0 → 1. lambda<0 → NaN. |
| `poisscdf` | ✅ |  |  |  | OK | F(k; λ) = 1 - gammainc(λ, ⌊k⌋+1) |
| `poissinv` | ✅ | 0.006 | 202.89× | 137.48× | OK | Sig: x = poissinv(p, lambda). Inverse Poisson CDF. q=0 → 0; q=1 → Inf; q outside [0,1] → NaN. lambda=0 → 0 (degenerate). lambda<0 → NaN. |
| `poissrnd` | ✅ |  |  |  | OK |  |
| `poisstat` | ✅ | 0.006 | 93.00× | 17.83× | OK | Sig: [m, v] = poisstat(lambda). Poisson mean=variance=lambda. Vectorised. lambda<=0 => NaN. |
| `binopdf` | ✅ | 0.009 | 318.21× | 168.79× | OK | Sig: y = binopdf(k, n, p). Binomial PMF. Out-of-support k (negative, > n, non-integer) → 0. p=0: only k=0 → 1. p=1: only k=n → 1. Invalid n / p out of [0,1] → NaN. |
| `binocdf` | ✅ |  |  |  | OK | I_{1-p}(n - ⌊k⌋, ⌊k⌋ + 1) |
| `binoinv` | ✅ | 0.008 | 845.30× | 377.14× | OK | Sig: x = binoinv(q, n, p). Inverse Binomial CDF. q=0 → 0; q=1 → n. Invalid (q outside [0,1] / p outside [0,1] / n<0 / non-integer n) => NaN. |
| `binornd` | ✅ |  |  |  | OK |  |
| `binostat` | ✅ | 0.008 | 209.42× | 57.77× | OK | Sig: [m, v] = binostat(n, p). Binomial: m=n·p, v=n·p·(1-p). Vectorised. n<0 / non-integer / p<0 / p>1 => NaN. p∈{0,1} are valid (variance becomes 0). |
| `unidpdf` | ✅ | 0.007 | 155.89× | 57.54× | OK | Sig: y = unidpdf(k, N). Discrete uniform PMF on {1..N}: 1/N if k in 1..N integer, else 0. N<=0 or non-integer N -> NaN. NaN N -> NaN. NaN k -> 0 (per MATLAB). tol=0 (integer-stable for discrete). |
| `unidcdf` | ✅ |  |  |  | OK |  |
| `unidinv` | ✅ | 0.008 | 137.04× | 47.62× | OK | Sig: x = unidinv(p, N). Inverse discrete-uniform CDF on {1..N}: x = ceil(p·N), clamped. Edges: p<=0 or p>1 -> NaN (p=0 has no integer pre-image); N<1 or non-integer N -> NaN; NaN p/N -> NaN. tol=0. |
| `unidrnd` | ✅ |  |  |  | OK |  |
| `unidstat` | ✅ | 0.006 | 84.76× | 42.89× | OK | Sig: [m, v] = unidstat(N). Discrete uniform on {1..N}: m = (N+1)/2, v = (N²-1)/12. Vectorised. N<1 or non-integer => NaN. |
| `geopdf` | ✅ |  |  |  | OK | geometric (failures before 1st success) |
| `geocdf` | ✅ |  |  |  | OK | -expm1((⌊k⌋+1)·log1p(-p)) |
| `geoinv` | ✅ |  |  |  | OK |  |
| `geornd` | ✅ |  |  |  | OK |  |
| `geostat` | ✅ | 0.006 | 58.05× | 20.21× | OK | Sig: [m, v] = geostat(p). Geometric (number-of-failures form): m = (1-p)/p, v = (1-p)/p². p=1 → m=v=0. Vectorised. p<=0 or p>1 => NaN. |
| `nbinpdf` | ✅ |  |  |  | OK | negative binomial |
| `nbincdf` | ✅ |  |  |  | OK | I_p(r, ⌊k⌋ + 1) |
| `nbininv` | ✅ |  |  |  | OK |  |
| `nbinrnd` | ✅ |  |  |  | OK | Gamma-Poisson mixture; supports real r |
| `nbinstat` | ✅ | 0.009 | 156.17× | 46.33× | OK | Sig: [m, v] = nbinstat(r, p). Negative binomial (number of failures): m = r·(1-p)/p, v = m/p. Vectorised. r<=0 / p<=0 / p>1 => NaN. r non-integer is OK (Pólya generalisation). p=1 → m=v=0. |
| `hygepdf` | ✅ |  |  |  | OK | hypergeometric (M, K, N) |
| `hygecdf` | ✅ |  |  |  | OK | forward sum via pmf-recurrence |
| `hygeinv` | ✅ |  |  |  | OK |  |
| `hygernd` | ✅ |  |  |  | OK | inverse-cdf walk per draw |
| `hygestat` | ✅ | 0.008 | 187.95× | 68.22× | OK | Sig: [m, v] = hygestat(M, K, N). Hypergeometric: m = N·K/M, v = N·K·(M-K)·(M-N)/(M²(M-1)). Vectorised. K=0 / K=M / N=0 valid; M=0 / K>M / N>M / negative => NaN. |
| `evpdf` | ✅ | 0.004 | 70.18× | 28.19× | OK | Sig: p = evpdf(x[, mu, sigma]). Type-I extreme value (Gumbel min) PDF. Defaults mu=0, sigma=1. Formula: (1/σ)·exp(t)·exp(−exp(t)) where t=(x−μ)/σ. |
| `evcdf` | ✅ | 0.003 | 170.98× | 89.45× | OK | Sig: p = evcdf(x[, mu, sigma]). F(x) = 1 − exp(−exp((x−μ)/σ)). |
| `evinv` | ✅ | 0.003 | 162.03× | 55.86× | OK | Sig: x = evinv(p[, mu, sigma]). x = μ + σ·log(−log1p(−p)). |
| `evrnd` | ✅ |  |  |  |  |  |
| `evstat` | ✅ | 0.009 | 59.72× | 32.29× | OK | Sig: [m, v] = evstat(mu, sigma). Type-I extreme value (Gumbel min): m = mu - sigma·γ (Euler), v = sigma²·π²/6. Vectorised. sigma<=0 => NaN. |
| `gevpdf` | ✅ | 0.004 | 218.14× | 61.53× | OK | Sig: p = gevpdf(x, k, sigma, mu). Generalised extreme value PDF; k=0 is Gumbel-MAX (limit). |
| `gevcdf` | ✅ | 0.003 | 384.47× | 93.98× | OK | Sig: p = gevcdf(x, k, sigma, mu). |
| `gevinv` | ✅ | 0.003 | 273.88× | 69.80× | OK | Sig: x = gevinv(p, k, sigma, mu). |
| `gevrnd` | ✅ |  |  |  |  |  |
| `gevstat` | ✅ | 0.007 | 150.55× | 102.05× | OK | Sig: [m, v] = gevstat(k, sigma, mu). GEV moments: complex by k regime (k>=1 mean=Inf; 0.5<=k<1 mean finite/var=Inf; k<0.5 both finite; k=0 Gumbel limit). Vectorised. sigma<=0 => NaN. |
| `gppdf` | ✅ | 0.003 | 281.24× | 98.79× | OK | Sig: p = gppdf(x, k, sigma, theta). Generalised Pareto. |
| `gpcdf` | ✅ | 0.003 | 405.90× | 55.74× | OK | Sig: p = gpcdf(x, k, sigma, theta). |
| `gpinv` | ✅ | 0.003 | 291.99× | 97.75× | OK | Sig: x = gpinv(p, k, sigma, theta). |
| `gprnd` | ✅ |  |  |  |  |  |
| `gpstat` | ✅ | 0.007 | 124.74× | 72.78× | OK | Sig: [m, v] = gpstat(k, sigma, theta). GP moments by k regime: k≥1 → mean Inf; 0.5≤k<1 → var Inf; k<0.5 → finite. m = theta + sigma/(1-k); v = sigma²/((1-k)²(1-2k)). Vectorised. sigma<=0 => NaN. |
| `nakapdf` | ✅ | 0.004 |  | 64.72× | OK | Sig: y = nakapdf(x, mu, omega). Nakagami PDF: (2μ^μ/Γ(μ)Ω^μ)·x^(2μ−1)·exp(−μx²/Ω). Octave's statistics package has direct names; MATLAB exposes via pdf('Nakagami', ...). Direct numkit + Octave parity. |
| `nakacdf` | ✅ |  |  |  |  |  |
| `nakainv` | ✅ |  |  |  |  |  |
| `nakarnd` | ✅ |  |  |  |  |  |
| `nakastat` | ✅ | 0.007 |  | 48.85× | OK | Sig: [m, v] = nakastat(mu, omega). Nakagami: m = sqrt(omega/mu)·Γ(mu+0.5)/Γ(mu), v = omega·(1 - r²/mu) where r = Γ(mu+0.5)/Γ(mu). Vectorised. mu<=0 / omega<=0 => NaN. MATLAB R2025b does not ship nakastat — Octave statistics package is the reference. |
| `ricepdf` | ✅ | 0.004 |  | 67.79× | OK | Sig: y = ricepdf(x, s, sigma). Rice PDF (x/σ²)·exp(−(x²+s²)/(2σ²))·I_0(x·s/σ²). Octave stats package has direct names; MATLAB exposes via pdf('Rician', ...). |
| `ricecdf` | ✅ |  |  |  |  |  |
| `riceinv` | ✅ |  |  |  |  |  |
| `ricernd` | ✅ |  |  |  |  |  |
| `ricestat` | ✅ | 0.010 |  | 50.55× | OK | Sig: [m, v] = ricestat(s, sigma). Rician (Rice). s=0 reduces to Rayleigh: m = sigma·sqrt(π/2), v = sigma²·(2 - π/2). Vectorised. sigma<=0 / s<0 => NaN. MATLAB R2025b doesn't ship ricestat — Octave statistics package is the reference. |
| `ncfpdf` | ❌ |  |  |  |  | noncentral F |
| `ncfcdf` | ❌ |  |  |  |  |  |
| `ncfinv` | ❌ |  |  |  |  |  |
| `ncfrnd` | ❌ |  |  |  |  |  |
| `ncfstat` | ❌ |  |  |  |  |  |
| `nctpdf` | ❌ |  |  |  |  | noncentral t |
| `nctcdf` | ❌ |  |  |  |  |  |
| `nctinv` | ❌ |  |  |  |  |  |
| `nctrnd` | ❌ |  |  |  |  |  |
| `nctstat` | ❌ |  |  |  |  |  |
| `ncx2pdf` | ✅ | 0.004 | 838.12× | 142.11× | OK | Sig: y = ncx2pdf(x, k, lambda). Noncentral χ²(k, λ) PDF: ½·exp(−(x+λ)/2)·(x/λ)^((k−2)/4)·I_{(k−2)/2}(√(λx)); λ=0 reduces to chi2pdf(x, k). |
| `ncx2cdf` | ✅ | 0.006 | 778.10× | 728.80× | OK | Sig: y = ncx2cdf(x, k, lambda). Poisson-mixture: Σ_j Poisson(j; λ/2)·gammainc(x/2, k/2 + j); truncated when contribution drops below 1e-16 of running sum. |
| `ncx2inv` | ✅ |  |  |  |  |  |
| `ncx2rnd` | ✅ |  |  |  |  |  |
| `ncx2stat` | ✅ | 0.008 | 128.69× | 46.11× | OK | Sig: [m, v] = ncx2stat(k, lambda). Non-central χ²: m = k+λ, v = 2(k+2λ). Vectorised. k<=0 / λ<0 => NaN. λ=0 reduces to central χ²(k). |

### Distribution Fitting (MLE / likelihood)

**Namespace:** `stats.fit.*` — 16 ✅ + 0 ⚠️ / 24 = 67%

OOP `fitdist` / `makedist` family intentionally omitted — only flat
function-form fitters (return `[parmhat, parmci]`) and likelihood evaluators.

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `mle` | ❌ |  |  |  |  | generic MLE for arbitrary pdf |
| `mlecov` | ❌ |  |  |  |  | covariance of MLE estimates |
| `betafit` | ❌ |  |  |  |  |  |
| `betalike` | ✅ | 0.007 | 140.45× | 61.04× | OK | Sig: [nL, AVAR] = betalike([a b], x). NLL for Beta(a, b). AVAR is the 2×2 inverse of the BHHH (outer-product-of-gradients) Fisher info — MATLAB's betalike uses BHHH, not the Hessian (verified by direct probe). Edge: invalid params or x outside (0,1) => NaN. |
| `binofit` | ✅ | 0.019 | 121.88× | 978.39× | OK | Sig: [phat, pci] = binofit(x, n[, alpha]). Clopper-Pearson exact binomial CI. Covers: scalar (k=7,n=10), vector ([3 5 7]'), edges x=0 + x=n, non-default alpha=0.01. No 'Method' kw — MATLAB binofit hard-codes Clopper-Pearson. |
| `evfit` | ❌ |  |  |  |  | extreme value |
| `evlike` | ✅ | 0.008 | 115.75× |  | OK | Sig: nL = evlike([mu sigma], x[, cens, freq]). Type-I extreme value (Gumbel min). Uncensored: log(σ) - z + e^z; censored: e^z; with optional freq weights. Edges: σ<=0 -> NaN (was Inf); empty data -> 0. AVAR (2-output form) deferred — observed Fisher info has nontrivial cross-terms. |
| `expfit` | ✅ | 0.012 | 216.59× | 2165.60× | OK | Sig: [muhat, muci] = expfit(x[, alpha[, censoring[, freq]]]). MLE for exponential: T = Σ(freq·x), D = Σ(freq·(1-cens)), mu = T/D. Exact CI via χ²(2D): [2T/χ²₁₋α/2, 2T/χ²_α/2]. Defaults: cens=0, freq=1. |
| `explike` | ✅ | 0.006 | 175.22× |  | OK | Sig: [nL, avar] = explike(mu, x[, cens, freq]). NLL for Exp(mu). avar (scalar) = 1/I where I = Σ w_i ∂²nL_i/∂μ² (uncens: -1/μ²+2x/μ³; right-cens: 2x/μ³). Edge: mu<=0 => NaN; empty data => 0. |
| `gamfit` | ❌ |  |  |  |  |  |
| `gamlike` | ✅ | 0.005 | 157.37× | 32.86× | OK | Sig: [nL, AVAR] = gamlike([a b], x). NLL for Gamma(a, b). AVAR is the 2×2 inverse observed-Fisher info matrix at [a, b], computed via central-difference Hessian (no in-tree trigamma). Edge: invalid params (a<=0 or b<=0) => NaN. tol=1e-7 reflects FD precision (~5e-8 absolute on basic case). |
| `gevfit` | ❌ |  |  |  |  | generalised extreme value |
| `gevlike` | ✅ | 0.008 | 144.71× | 52.27× | OK | Sig: [nL, ACOV] = gevlike([k sigma mu], x). GEV NLL with Gumbel-MAX limit at k=0. ACOV is the 3×3 inverse observed-Fisher matrix at [k,sigma,mu], computed via central-difference Hessian (tol=1e-6 reflects FD precision). Edge: sigma<=0 or per-point support violation (1+k*z<=0) => NaN. Known gap: at exactly k=0 MATLAB uses an analytical Gumbel-limit Hessian that differs from FD straddling — numkit's FD reproduces the value of FD-on-MATLAB's-own-gevlike (~0.030, 0.098, -1.622), not MATLAB's reported analytical ACOV. |
| `gpfit` | ❌ |  |  |  |  | generalised Pareto |
| `gplike` | ✅ | 0.007 | 146.83× |  | OK | Sig: [nL, acov] = gplike([k sigma], x). GP NLL with implicit theta=0. acov is the 2×2 inverse observed-Fisher matrix at [k, sigma], computed via central-difference Hessian (tol=1e-5 reflects FD precision; k=0 stride is the worst case at ~2e-6). Edges: sigma<=0 or per-point support violation (1+k*x/sigma<=0) => NaN. MATLAB does NOT enforce x>=0 globally — only the per-point support check; numkit matches (e.g. gplike([0.5,1], [-1 1 2]') returns 1.2163...). |
| `lognfit` | ✅ | 0.006 | 482.94× | 1234.73× | OK | Sig: [parm, pci] = lognfit(x[, alpha]). Lognormal MLE: parm=[mu sigma] of log(x). pci is 2x2: column 1 = mu CI, column 2 = sigma CI. |
| `lognlike` | ✅ | 0.010 | 216.66× |  | OK | Sig: [nL, aVar] = lognlike([mu sigma], x[, cens, freq]). NLL for lognormal. Hessian wrt (mu, sigma) is structurally identical to the normal Hessian on log(x). aVar (column-major 2×2) reflects cens/freq weighting; can have negative diagonal entries at non-MLE params (observed Fisher, not expected). Edge: sigma<=0 or x<=0 => NaN; empty data => 0. |
| `nbinfit` | ❌ |  |  |  |  |  |
| `normfit` | ✅ | 0.006 | 466.90× | 1219.64× | OK | Sig: [mu, sd, muci, sdci] = normfit(x[, alpha]). MLE for normal: mu=mean, sd=sample std (N-1). t-CI for mu, chi² CI for sigma. Default alpha=0.05. |
| `normlike` | ✅ | 0.009 | 233.30× |  | OK | Sig: [nL, aVar] = normlike([mu sigma], data[, cens, freq]). Default + censoring (right-censored => -log(S(z))) + freq weights + empty + invalid-sigma (=> NaN). Second output aVar = inverse 2×2 observed-Fisher information matrix at [mu, sigma]; reflects cens/freq weighting. |
| `poissfit` | ✅ | 0.008 | 228.09× |  | OK | Sig: [lhat, lci] = poissfit(x[, alpha]). MLE for Poisson: lambda=mean(x). Exact CI via chi² inversion (Garwood). Edges: all-zero data -> lo=0; non-default alpha; empty input -> NaN. |
| `raylfit` | ✅ | 0.009 | 241.10× |  | OK | Sig: [shat, sci] = raylfit(x[, alpha]). Rayleigh MLE: σ = √(Σx²/(2N)); CI from chi² inversion 2N·σ̂² ~ σ²·χ²(2N). Edges: non-default α; single-element x; empty input -> NaN. |
| `unifit` | ✅ | 0.008 | 74.44× |  | OK | Sig: [a, b, aci, bci] = unifit(x[, alpha]). MLE for U(a,b): a=min, b=max. CI extension delta = (b-a)·(α^(-1/n) − 1). Single-element x: ACI=BCI=[x x] (zero-width). Empty input: numkit returns NaN; MATLAB returns empty arrays — convention difference, not in fingerprint. |
| `wblfit` | ❌ |  |  |  |  |  |
| `wbllike` | ✅ | 0.008 | 276.96× | 58.63× | OK | Sig: nL = wbllike([scale shape], x[, cens, freq]). Weibull(a, b). Uncensored: -log(b) + b·log(a) - (b-1)·log(x) + (x/a)^b. Censored: (x/a)^b. With optional freq weights. Edges: scale<=0 or shape<=0 -> NaN (was Inf); x_i <= 0 -> NaN. Empty data: numkit returns 0 (consistent with our *like family); MATLAB errors `DATA must be a vector` — convention difference, not in fingerprint. AVAR (2-output form) deferred. |

### Multivariate Distributions

**Namespace:** `stats.mvdist.*` — 3 ✅ + 0 ⚠️ / 14 = 21%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `mvncdf` | ❌ |  |  |  |  | multivariate normal |
| `mvnpdf` | ✅ | 0.004 | 206.00× | 44.45× | OK | Sig: p = mvnpdf(X[, mu, Sigma]). Multivariate normal PDF. Default mu=zeros, Sigma=I. Cholesky-based to handle |Σ|^(-1/2) and Σ^(-1) accurately. |
| `mvnrnd` | ❌ |  |  |  |  |  |
| `mvtcdf` | ❌ |  |  |  |  | multivariate t |
| `mvtpdf` | ✅ | 0.004 | 268.85× | 60.17× | OK | Sig: p = mvtpdf(X, C, df). Multivariate t PDF; C is treated as a correlation matrix (input is normalised to correlation form to match MATLAB). Cholesky-based determinant + quadratic form. |
| `mvtrnd` | ❌ |  |  |  |  |  |
| `mnpdf` | ✅ | 0.003 | 196.13× | 49.38× | OK | Sig: p = mnpdf(X, P). Multinomial PMF: n!/(Πx_i!)·Π p_i^x_i. Computed in log-space via lgamma. |
| `mnrnd` | ❌ |  |  |  |  |  |
| `wishrnd` | ❌ |  |  |  |  | Wishart |
| `iwishrnd` | ❌ |  |  |  |  | inverse Wishart |
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
| `randg` | ❌ |  |  |  |  | gamma random utility |

### Empirical / Kernel Distributions

**Namespace:** `stats.empirical.*` — 3 ✅ + 0 ⚠️ / 4 = 75%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `ecdf` | ✅ | 0.003 | 889.16× | 85.32× | OK | Sig: [f, x] = ecdf(y). Empirical CDF — column vectors of length K+1 where K is the number of distinct y values. f(1)=0 at x=min(y); subsequent f(k)=cumcount(k)/N at each unique value. NaN excluded from N. Element-wise SAVE on f. |
| `ecdfhist` | ✅ | 0.003 | 341.03× |  | OK | Sig: [n, c] = ecdfhist(f, x [, m]). Probability-density histogram from ecdf data. Default m=10 bins. Element-wise SAVE on n. |
| `ksdensity` | ✅ | 0.004 | 2040.36× |  | OK | Sig: [f, xi, bw] = ksdensity(x[, pts, 'Bandwidth', bw]). Gaussian kernel density estimate at user-specified pts (or 100-point auto grid). With explicit bw the kernel formula matches MATLAB exactly; the auto-bw heuristic uses Silverman's rule which differs slightly from MATLAB's internal. |
| `mvksdensity` | ❌ |  |  |  |  | multivariate KDE |

### Hypothesis Tests

**Namespace:** `stats.test.*` — 16 ✅ + 0 ⚠️ / 25 = 64%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `adtest` | ❌ |  |  |  |  | Anderson-Darling normality |
| `ansaribradley` | ❌ |  |  |  |  | scale test |
| `barttest` | ❌ |  |  |  |  | Bartlett's sphericity |
| `chi2gof` | ✅ | 0.005 | 570.92× | 219.96× | OK | Sig: [h, p, stats] = chi2gof(x, 'Frequency', O, 'Expected', E[, 'NParams', np]). chi² = Σ(O−E)²/E; df = k−1−np. Auto-binned distribution-fit form intentionally not implemented in this release. |
| `dwtest` | ❌ |  |  |  |  | Durbin-Watson |
| `fishertest` | ✅ | 0.005 | 1128.35× | 100.45× | OK | Sig: [h, p, stats] = fishertest(T[, 'Tail', t, 'Alpha', a]). Fisher's exact test for 2×2 contingency. Two-sided p sums hypergeometric pmf cells with P(X=k) ≤ P(X=obs). OR = a·d/(b·c); CI is the Woolf log-OR ± z·SE. |
| `friedman` | ❌ |  |  |  |  | non-parametric repeated-measures |
| `jbtest` | ✅ |  |  |  | OK | Jarque-Bera, JB ~ χ²(2) |
| `knntest` | ❌ |  |  |  |  | k-NN two-sample test |
| `kruskalwallis` | ✅ | 0.006 | 865.19× | 349.46× | OK | Sig: [p, tbl, stats] = kruskalwallis(y, group[, 'off']). Non-parametric one-way ANOVA: H = (12/(N(N+1)))·Σ R_g²/n_g − 3(N+1), tie-corrected by 1 − Σ(t³−t)/(N³−N). df = k−1; p = 1 − chi2cdf(H, df). |
| `kstest` | ✅ |  |  |  | OK | one-sample KS via asymptotic Smirnov series |
| `kstest2` | ✅ |  |  |  | OK | two-sample KS |
| `lillietest` | ❌ |  |  |  |  | Lilliefors |
| `meanEffectSize` | ❌ |  |  |  |  | Cohen's d, Hedges' g |
| `mmdtest` | ❌ |  |  |  |  | maximum mean discrepancy |
| `multcompare` | ❌ |  |  |  |  | post-hoc multiple comparisons |
| `ranksum` | ✅ | 0.007 | 697.44× | 1064.43× | OK | Sig: [p, h, stats] = ranksum(x, y[, alpha, tail | name-value]). Wilcoxon rank-sum (Mann-Whitney U). Default exact iff both samples have <10 obs (size-k subset-sum DP); else approximate with continuity + tie correction. |
| `runstest` | ✅ | 0.005 | 376.80× | 95.95× | OK | Sig: [h, p, stats] = runstest(x[, v][, alpha, tail | name-value]). Wald-Wolfowitz runs test. Default v=median(x); values == v dropped. Exact dist by default via combinatorial PMF; approximate uses continuity-corrected normal. |
| `sampsizepwr` | ❌ |  |  |  |  | sample-size / power |
| `signrank` | ✅ | 0.004 | 625.70× | 74.62× | OK | Sig: [p, h, stats] = signrank(x[, m | y][, alpha, tail | name-value]). Wilcoxon signed-rank: rank |d_i| with mid-rank tie averaging, W+ = Σ ranks of positive d. Default exact for n_eff ≤ 15 (subset-sum convolution); approximate uses tie-corrected normal. |
| `signtest` | ✅ | 0.004 | 790.44× | 101.31× | OK | Sig: [p, h, stats] = signtest(x[, m | y][, alpha, tail | name-value]). Paired sample test: 5 positives over 5 non-zero diffs, two-sided p = 2·(0.5)^5 = 0.0625 (binomial). |
| `ttest` | ✅ |  |  |  | OK | one-sample, returns (h, p, ci, tstat) |
| `ttest2` | ✅ |  |  |  | OK | Welch (default) or pooled-variance |
| `vartest` | ✅ |  |  |  | OK | chi-squared one-sample variance test |
| `vartest2` | ✅ |  |  |  | OK | F-test for equality of variances |
| `vartestn` | ✅ | 0.005 | 1284.84× | 835.74× | OK | Sig: [p, stats] = vartestn(x, group[, 'Display', 'off']). Bartlett's k-sample variance equality test. Q = (N-k)·ln(S²) - Σ(n_i-1)·ln(s_i²); T = Q/C; p = 1 - chi2cdf(T, k-1). Levene/BrownForsythe TestType deferred. |
| `ztest` | ✅ |  |  |  | OK | known-σ z-test |

### Resampling Techniques

**Namespace:** `stats.resample.*` — 3 ✅ + 0 ⚠️ / 7 = 38%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `bootci` | ❌ |  |  |  |  | bootstrap confidence intervals |
| `bootstrp` | ⚠️ |  |  |  | NYI | needs Engine::call for function handles |
| `combnk` | ✅ |  |  |  | OK | lex-order enumeration; scalar N or vector input |
| `crossval` | ❌ |  |  |  |  | k-fold cross-validation |
| `cvpartition` | ❌ |  |  |  |  | partition object (function-form constructor) |
| `datasample` | ✅ |  |  |  | OK | rows or columns; with/without replacement; weights |
| `jackknife` | ⚠️ |  |  |  | NYI | needs Engine::call for function handles |
| `randsample` | ✅ |  |  |  | OK | uniform or weighted; with/without replacement |

### Quasirandom Sequences and MCMC

**Namespace:** `stats.qmc.*` — 1 ✅ + 0 ⚠️ / 8 = 13%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `haltonset` | ✅ | 0.003 | 232.79× |  | OK | Sig: p = haltonset(d[, 'Skip', s, leap]); X = net(p, n). Halton quasi-random points via radical inverse. Default Skip=0 (origin first); MATLAB convention. |
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
| `anova1` | ✅ | 0.006 | 804.36× | 420.59× | OK | Sig: [p, tbl, stats] = anova1(y, group[, 'off']). One-way ANOVA; F = MS_groups/MS_error; p = 1 - fcdf(F, k-1, N-k). tbl is 4×6 cell array (Source, SS, df, MS, F, Prob>F). |
| `anova2` | ❌ |  |  |  |  | two-way balanced |
| `anovan` | ❌ |  |  |  |  | n-way |
| `manova1` | ❌ |  |  |  |  | one-way MANOVA |
| `canoncorr` | ❌ |  |  |  |  | canonical correlation |
| `dummyvar` | ✅ | 0.003 | 251.23× | 78.07× | OK | Sig: D = dummyvar(group). Indicator-coding: N×K matrix with 1 in column k for samples whose label is the k-th unique value (sorted ascending). |
| `aoctool` | ❌ |  |  |  |  | analysis of covariance (interactive — defer) |
| `mauchly` | ❌ |  |  |  |  | Mauchly's sphericity |
| `epsilon` | ❌ |  |  |  |  | sphericity adjustments |

### Linear Regression (function-form)

**Namespace:** `stats.regress.*` — 3 ✅ + 0 ⚠️ / 13 = 23%

OOP `fitlm` / `fitlme` / `fitglm` / `LinearModel` / etc. intentionally omitted. Only the legacy command-form entry points that return numerics (coeffs, residuals, CIs).

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `regress` | ✅ | 0.006 | 476.12× | 403.15× | OK | Sig: [b, bint, r, rint, stats] = regress(y, X[, alpha]). OLS multiple regression via Cholesky on X'X. stats = [R², F, p_F, sigma²]. The 4th output `rint` (residual-outlier intervals) is currently a placeholder. |
| `robustfit` | ❌ |  |  |  |  | robust (M-estimator) regression |
| `lscov` | ✅ | 0.004 | 201.24× | 10.25× | OK | Sig: [x, stdx, mse, S] = lscov(A, b[, w]). Weighted least squares. With identity weights = OLS. mse = SSR/(N-p); S = mse·(A'WA)^(-1). Full N×N covariance V intentionally not yet supported. |
| `stepwisefit` | ❌ |  |  |  |  | stepwise selection |
| `glmfit` | ❌ |  |  |  |  | generalised linear model |
| `glmval` | ❌ |  |  |  |  | predict from glmfit |
| `mvregress` | ❌ |  |  |  |  | multivariate regression |
| `mvregresslike` | ❌ |  |  |  |  |  |
| `plsregress` | ❌ |  |  |  |  | partial least squares |
| `ridge` | ✅ | 0.004 | 344.06× | 108.76× | OK | Sig: B = ridge(y, X, k[, scaled]). Ridge regression on standardised X (centered + N-1 std). Default scaled=1 returns coefficients in standardised space; scaled=0 returns (p+1)-row matrix with intercept in original units. |
| `lasso` | ❌ |  |  |  |  |  |
| `lassoglm` | ❌ |  |  |  |  |  |
| `polyconf` | ❌ |  |  |  |  | polynomial CI prediction |

### Nonlinear Regression (function-form)

**Namespace:** `stats.nlfit.*` — 0 ✅ + 0 ⚠️ / 5 = 0%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `nlinfit` | ❌ |  |  |  |  | nonlinear LSQ |
| `nlparci` | ❌ |  |  |  |  | parameter CIs |
| `nlpredci` | ❌ |  |  |  |  | predicted-value CIs |
| `statset` | ❌ |  |  |  |  | options struct setter |
| `statget` | ❌ |  |  |  |  | options struct getter |

### Distance Metrics

**Namespace:** `stats.cluster.*` — 4 ✅ + 0 ⚠️ / 4 = 100%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `pdist` | ✅ |  |  |  | OK | euclidean / sqeuclidean / cityblock / chebychev / minkowski / cosine / correlation / hamming / jaccard |
| `pdist2` | ✅ |  |  |  | OK | same metrics |
| `squareform` | ✅ |  |  |  | OK | bidirectional vec ↔ square |
| `mahal` | ✅ |  |  |  | OK | Cholesky-based, throws on non-PSD covariance |

### Hierarchical Clustering

**Namespace:** `stats.cluster.*` — 5 ✅ + 0 ⚠️ / 7 = 71%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `linkage` | ✅ |  |  |  | OK | single/complete/average/weighted/centroid/median/ward |
| `cluster` | ✅ |  |  |  | OK | maxclust + cutoff (distance criterion) |
| `clusterdata` | ✅ |  |  |  | OK | pdist + linkage + cluster one-shot |
| `cophenet` | ✅ |  |  |  | OK | Pearson between Y and cophenetic distances |
| `inconsistent` | ✅ |  |  |  | OK | (mean, std, count, inconsistency) at given depth |
| `dendrogram` | ❌ |  |  |  |  | display |
| `optimalleaforder` | ❌ |  |  |  |  | leaf permutation for visualisation |

### Partitional Clustering

**Namespace:** `stats.cluster.*` — 3 ✅ + 0 ⚠️ / 4 = 75%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `kmeans` | ✅ |  |  |  | OK | Lloyd's + k-means++ init, MaxIter / Replicates options |
| `kmedoids` | ✅ |  |  |  | OK | PAM-style; supports euclidean/sqeuclidean/cityblock/chebychev |
| `dbscan` | ✅ |  |  |  | OK | core-point expansion; noise → label 0 (MATLAB convention) |
| `spectralcluster` | ❌ |  |  |  |  | spectral clustering |

### Cluster Evaluation

**Namespace:** `stats.cluster_eval.*` — 0 ✅ + 0 ⚠️ / 3 = 0%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `silhouette` | ✅ | 0.004 | 984.05× | 19497.99× | OK | Sig: s = silhouette(X, clust). Default metric sqEuclidean. 6 points, 2 well-separated clusters of 3. Element-wise SAVE; values near 0.99 indicating tight clusters with large inter-cluster gap. |
| `evalclusters` | ❌ |  |  |  |  | CalinskiHarabasz / DaviesBouldin / gap / silhouette |
| `manovacluster` | ❌ |  |  |  |  | dendrogram from MANOVA |

### Nearest Neighbors (function-form)

**Namespace:** `stats.knn.*` — 0 ✅ + 0 ⚠️ / 3 = 0%

OOP `KDTreeSearcher` / `ExhaustiveSearcher` / `hnswSearcher` intentionally omitted; flat function form only.

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `knnsearch` | ✅ | 0.004 | 1326.60× | 219.61× | OK | Sig: [Idx, D] = knnsearch(X, Y, 'K', K). Brute-force k-nearest neighbour. 6-point X, 2-query Y, K=3, default Euclidean. Element-wise SAVE on idx (1-based row indices). |
| `rangesearch` | ✅ | 0.005 | 991.24× | 123.10× | OK | Sig: [Idx, D] = rangesearch(X, Y, r). Cell-array output unwrapped to a numeric row in SAVE (idx = idxC{1}). All 3 points in cluster 1 are within r=1.0 of (1.5, 1.5). Explicit fingerprint avoids sum on the cell. |
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
| `pca` | ✅ |  |  |  | OK | Jacobi eigendecomp on cov(X); coeff/score/latent/T²/explained/μ |
| `pcacov` | ✅ |  |  |  | OK | direct eigendecomp on covariance matrix |
| `pcares` | ✅ |  |  |  | OK | residual = X - reconstruct from k PCs |
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
| `classify` | ✅ | 0.006 | 862.14× |  | OK | Sig: [c, err, post, logp] = classify(sample, training, group[, type]). LDA (default 'linear') or QDA ('quadratic'); also 'diaglinear', 'diagquadratic'. Empirical priors n_k/N. Cholesky-factor approach for numerical stability. |

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
| `dwt` | ✅ |  |  |  | OK | single-level DWT, 'sym' boundary |
| `idwt` | ✅ |  |  |  | OK | round-trip ≤ 1e-12 on db/sym/coif |
| `wavedec` | ✅ |  |  |  | OK | multi-level DWT (composes dwt) |
| `waverec` | ✅ |  |  |  | OK | round-trip ≤ 1e-11 over 4 levels |
| `appcoef` | ✅ |  |  |  | OK | level=0 = full reconstruction |
| `detcoef` | ✅ |  |  |  | OK | 1-based level (1=finest, n=coarsest) |
| `wrcoef` | ✅ | 0.037 | 67.45× |  | OK | Sig: y = wrcoef(type, c, l, wname[, n]). Single-band reconstruction. type ∈ {'a','d'}; n is the level kept ('a' allows n=0 = full reconstruction; 'd' requires n in [1, max]). Default n = length(l)-2 for both types. Algorithm: build modified c with off-band coefficients zeroed, run waverec. Verified parity with MATLAB R2025b on HAAR wavelet (where numkit's wavedec matches MATLAB exactly). For db/sym/coif numkit's wavedec uses a slightly different boundary convention (BUGS.md #37) — wrcoef there produces values consistent with numkit's own wavedec/waverec round-trip but does NOT match MATLAB coefficient-for-coefficient. (Lo_R, Hi_R) two-filter form not implemented in this release. |
| `dwtmode` | ❌ |  |  |  |  | extension mode |
| `dyaddown` | ✅ | 0.003 | 205.08× |  | OK | Sig: y = dyaddown(x[, ODD]). Dyadic downsample. ODD = 0 (default) → x(2:2:end); ODD = 1 → x(1:2:end). |
| `dyadup` | ✅ | 0.003 | 350.96× |  | OK | Sig: y = dyadup(x[, ODD]). Zero insertion between samples. Default ODD = 1 → [0 x(1) 0 x(2) 0 ... x(N) 0] (length 2N+1); ODD = 0 → [x(1) 0 x(2) 0 ... x(N)] (length 2N-1). |
| `wkeep` | ✅ | 0.005 | 498.49× |  | OK | Sig: y = wkeep(x, n[, OPT]). Default 'c' central window with start = floor((N-n)/2)+1 (1-based). 'l'/'r' first/last n; numeric FIRST gives x(FIRST:FIRST+n-1). |
| `wextend` | ✅ | 0.006 | 305.97× |  | OK | Sig: y = wextend(1, mode, x, lf[, side]). Modes: sym (whole-point symm with edge), per (periodic with edge-pad on odd N), zpd (zero pad), ppd (true periodic). For odd-N periodic MATLAB pre-pads x with x(end) to even length, then wraps. |
| `wcodemat` | ✅ | 0.003 | 166.80× |  | OK | Sig: Y = wcodemat(X [, nb [, opt [, absol]]]). Quantize/scale to [1, nb] integer codes (default nb=16, mat, absol=1). Wavelet Toolbox helper; Octave-image doesn't have it → correctness=N/A. |
| `haart` | ✅ | 0.013 | 150.78× |  | OK | Sig: [a, d] = haart(x[, level[, integerflag]]). Haar 1-D DWT. Default level = max k such that 2^k divides length(x). 'noninteger' uses 1/sqrt(2) Haar pair; 'integer' uses lifting (a = x[2k] + floor((x[2k+1]-x[2k])/2)). Output is always column for vector input. d is plain when level=1, cell array d{1..L} when level>1 (d{1} finest). Matrix input processes columns independently. Verified: level=1, default-level (cell), integer mode (signed-floor), matrix, complex, row->col coercion, integer+double, N=12 partial level. |
| `ihaart` | ✅ | 0.017 | 279.78× |  | OK | Sig: xrec = ihaart(a, d[, level[, integerflag]]). Inverse Haar 1-D DWT. Default level=0 (lossless reconstruction). When level=K (in [0, Nlevels)) the K finest detail bands d{1..K} are zeroed BEFORE reconstruction (xrec stays full-length). Inverse formulas: noninteger uses (a±d)/sqrt(2); integer uses lifting x[2k]=a[k]-floor(d[k]/2), x[2k+1]=x[2k]+d[k]. d MUST be real even when a is complex (MATLAB validateattributes on D). d may be a plain matrix at level=1 or a length-Nlevels cell array. Vector-shaped a returns column; matrix returns matrix. Verified: level=1, full multi-level, partial reconstruction (zero-out 1 and 2 bands), integer mode + partial, matrix full + partial. |
| `wmaxlev` | ✅ | 0.005 | 407.51× |  | OK | Sig: L = wmaxlev(N, wname). Maximum decomposition level. L = floor(log2(N / (Lf - 1))) where Lf is the wavelet filter length. For a 2-vector N, MATLAB uses min(N). |
| `dwpt` | ❌ |  |  |  |  | discrete wavelet packet transform |
| `idwpt` | ❌ |  |  |  |  | inverse DWPT |

### Discrete Wavelet Transforms (2-D / 3-D)

**Namespace:** `wavelet.dwt2.*` — 2 ✅ + 0 ⚠️ / 15 = 13%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `dwt2` | ✅ |  |  |  | OK | separable: row pass then column pass |
| `idwt2` | ✅ |  |  |  | OK | round-trip ≤ 7e-11 across haar/db2/sym4 |
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
| `swt` | ✅ |  |  |  | OK | stationary (à trous) wavelet transform |
| `iswt` | ✅ |  |  |  | OK | round-trip ≤ 3.2e-12 across haar/db2/sym4 |
| `swt2` | ❌ |  |  |  |  |  |
| `iswt2` | ❌ |  |  |  |  |  |
| `modwt` | ✅ |  |  |  | OK | energy-preserving (h̃ = Lo_D/√2); any N (no pow2 constraint) |
| `imodwt` | ✅ |  |  |  | OK | exact inverse; round-trip ≤ 3e-12; Parseval ratio = 1.0 |
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
| `wdenoise` | ✅ |  |  |  | OK | VisuShrink (universal soft) on default details |
| `wdenoise2` | ❌ |  |  |  |  | 2-D denoising |
| `wden` | ❌ |  |  |  |  | classical denoising |
| `wdencmp` | ❌ |  |  |  |  | denoise / compress |
| `wpdencmp` | ❌ |  |  |  |  | wavelet-packet denoise / compress |
| `wnoisest` | ✅ |  |  |  | OK | per-level σ via MAD/0.6745 |
| `wvarchg` | ❌ |  |  |  |  | variance-change detection |
| `ddencmp` | ❌ |  |  |  |  | default thresholding parameters |
| `thselect` | ❌ |  |  |  |  | threshold selection |
| `wthcoef` | ❌ |  |  |  |  | apply threshold to detail coeffs |
| `wthcoef2` | ❌ |  |  |  |  |  |
| `wthresh` | ✅ |  |  |  | OK | hard / soft threshold |
| `wmulden` | ❌ |  |  |  |  | multivariate denoising |
| `measerr` | ❌ |  |  |  |  | quality measures (PSNR/MSE/MAX/L2) |
| `wnoise` | ❌ |  |  |  |  | noisy test signal |
| `wcompress` | ❌ |  |  |  |  | compression front-end |

### Filter Banks and Wavelet Families

**Namespace:** `wavelet.filt.*` — 7 ✅ + 0 ⚠️ / 22 = 32%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `wfilters` | ✅ |  |  |  | OK | haar / db1..db4 / sym2 / sym4 / coif1; 4-out form + 'd'/'r'/'l'/'h' |
| `orthfilt` | ✅ | 0.004 | 105.74× |  | OK | Sig: [Lo_D, Hi_D, Lo_R, Hi_R] = orthfilt(W). Quadruple from a unit-norm scaling filter W (sum(W)=1). Lo_R = W*sqrt(2); Lo_D = reverse(Lo_R); Hi_R[k] = (-1)^k * Lo_R[N-1-k]; Hi_D = reverse(Hi_R). |
| `qmf` | ✅ | 0.004 | 109.04× |  | OK | Sig: y = qmf(x[, p]). Quadrature mirror filter. y(k) = (-1)^(k-1+p) * x(N-k+1). Default p = 0 (identity-sign on the first element); p = 1 negates. |
| `biorfilt` | ❌ |  |  |  |  | biorthogonal filter quadruple |
| `dbwavf` | ✅ | 0.006 | 130.99× |  | OK | Sig: h = dbwavf(wname). Daubechies scaling filter: Lo_R / sqrt(2). Length 2N for 'dbN'. Sum(h) = 1. |
| `coifwavf` | ✅ | 0.003 | 184.30× |  | OK | Sig: h = coifwavf(wname). Coiflet scaling filter: Lo_R / sqrt(2). Length 6K for 'coifK'. |
| `symwavf` | ✅ | 0.004 | 158.56× |  | OK | Sig: h = symwavf(wname). Symlet scaling filter: Lo_R / sqrt(2). Length 2N for 'symN'. |
| `dbaux` | ❌ |  |  |  |  | Daubechies aux |
| `symaux` | ❌ |  |  |  |  | symlet aux |
| `biorwavf` | ❌ |  |  |  |  | biorthogonal scaling filter |
| `rbiowavf` | ❌ |  |  |  |  | reverse biorthogonal |
| `fejerkorovkin` | ❌ |  |  |  |  | Fejér-Korovkin filters |
| `mbscalf` | ❌ |  |  |  |  | Morris minimum-bandwidth |
| `hanscalf` | ❌ |  |  |  |  | Han scaling filter |
| `blscalf` | ❌ |  |  |  |  | Beylkin |
| `bswfun` | ❌ |  |  |  |  | biorthogonal scaling/wavelet via cascade |
| `wrev` | ✅ | 0.003 | 48.27× | 56.85× | OK | Sig: y = wrev(x). Reverse a vector (Wavelet Toolbox helper, equivalent to fliplr/flipud on a vector). |
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
| `meyeraux` | ✅ | 0.003 | 178.46× | 62.01× | OK | Sig: y = meyeraux(x). Polynomial 35x⁴ − 84x⁵ + 70x⁶ − 20x⁷; element-wise. Endpoints meyeraux(0)=0, meyeraux(1)=1, meyeraux(0.5)=0.5. |
| `mexihat` | ✅ | 0.004 | 199.54× | 27.70× | OK | Sig: [psi, x] = mexihat(LB, UB, N). ψ(t) = (2/√3)·π^(-1/4)·(1-t²)·exp(-t²/2). Even, peaks at 0, zero crossings at ±1. |
| `morlet` | ✅ | 0.005 | 83.85× | 14.95× | OK | Sig: [psi, x] = morlet(LB, UB, N). Real Morlet ψ(t) = exp(-t²/2)·cos(5t). |
| `cgauwavf` | ✅ | 0.007 | 168.64× |  | OK | Sig: [psi, x] = cgauwavf(LB, UB, N[, p]). Complex Gaussian wavelet (-1)^p · H_p(t + i/2) · exp(-t² - i·t). Trapezoidal L² normalization on the requested grid (matches MATLAB's grid-dependent normalization). |
| `cmorwavf` | ✅ | 0.005 | 136.37× | 9.72× | OK | Sig: [psi, x] = cmorwavf(LB, UB, N, fb, fc). Complex Morlet ψ(t) = (1/√(π·fb))·exp(2πi·fc·t)·exp(-t²/fb). |
| `fbspwavf` | ✅ | 0.005 | 154.18× |  | OK | Sig: [psi, x] = fbspwavf(LB, UB, N, m, fb, fc). Frequency B-spline ψ(t) = √fb·(sinc(fb·t/m))^m · exp(2πi·fc·t). |
| `gauswavf` | ✅ | 0.006 | 193.88× |  | OK | Sig: [psi, x] = gauswavf(LB, UB, N[, p]). Real Gaussian wavelet ψ_p = sgn_p · |α_p| · H_p(t)·exp(-t²) with sgn_p = (-1)^ceil(p/2), |α_p|² = 1/((2p-1)!!·sqrt(π/2)). Default p = 1. |
| `intwave` | ❌ |  |  |  |  | wavelet integral |
| `pat2cwav` | ❌ |  |  |  |  | pattern → custom wavelet |
| `shanwavf` | ✅ | 0.007 | 199.53× | 9.67× | OK | Sig: [psi, x] = shanwavf(LB, UB, N, fb, fc). Shannon wavelet ψ(t) = √fb·sinc(fb·t)·exp(2πi·fc·t). |

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
| `impyramid_expand` | — | 0.004 |  | 115.34× | OK | Sig: B = impyramid(A, 'expand'). Output: (2M-1)x(2N-1). Cross-check expand variant separately. |
| `axes2pix` | — | 0.004 |  | 25.07× | OK | Sig: pix = axes2pix(n, extent, axesCoord). World→pixel axis mapping (1-based). Octave-image has axes2pix. |
| `isgray` | — | 0.003 |  | 14.93× | OK | Sig: tf = isgray(I). True for 2-D images of class uint8/uint16/int16 or float in [0,1]. Octave-image has isgray. |
| `imcast` | — | 0.005 |  | 41.75× | OK | Sig: J = imcast(I, type). Dispatch wrapper over im2* helpers (type ∈ double/single/uint8/uint16/int16/logical). Octave-image has imcast. |
| `mmgradm` | — | 0.004 |  | 144.88× | OK | Sig: G = mmgradm(I [, se_dil [, se_ero]]). Morphological gradient = imdilate − imerode (default cross SE). Octave-image has mmgradm. |
| `fchcode` | — | 0.004 |  | 38.44× | OK | Sig: fcc = fchcode(bound). Freeman 8-direction chain code; struct with x0y0, fcc, diff fields. Octave-image has fchcode. |
| `fftconv2` | — | 0.007 |  | 83.79× | OK | Sig: Y = fftconv2(A, B [, shape]). FFT-based 2-D conv; output complex with tiny imag, smoke wraps real(). Octave-image has fftconv2. |
| `wavelength2rgb` | — | 0.004 |  | 144.92× | OK | Sig: rgb = wavelength2rgb(wavelength [, class [, gamma]]). Piecewise visible-light wavelength → RGB (Bruton). Tolerance loose because Octave's gamma=0.8 raises tiny FP noise when raising 0 to 0.8 — final RGB triple to 4 decimals is the right comparison. |
| `imsmooth` | — | 0.005 |  | 140.09× | OK | Sig: J = imsmooth(I, name [, sigma]). Currently Gaussian-only with σ-Gaussian, h=ceil(3σ), symmetric pad; Octave-image has imsmooth (this matches the Gaussian path). |
| `colorgradient` | — | 0.005 |  | 45.97× | OK | Sig: M = colorgradient(C [, w] [, n]). K-by-3 anchor RGB; piecewise linspace; default n=64. Octave-image has colorgradient. Default uses rows(colormap) but we don't have a graphics colormap so we default to n=64. |
| `iscolormap` | — | 0.007 |  | 30.48× | OK | Sig: tf = iscolormap(cmap). Real, float (single/double), 2-D, 3 cols, non-empty. Range [0,1] not enforced. Octave core has iscolormap. |
| `gray` | — | 0.003 |  | 12.94× | OK | Sig: map = gray([n]). N×3 grayscale colormap. Default n=256 (we don't track figure colormap state). n==1 → [0 0 0]; n<=0 → 0×3. Octave core has gray. |
| `hot` | — | 0.004 |  | 11.72× | OK | Sig: map = hot([n]). N×3 black→red→yellow→white colormap. Default n=256. Octave core has hot. |
| `cool` | — | 0.004 |  | 59.77× | OK | Sig: map = cool([n]). N×3 cyan→magenta. r=(0:n-1)/(n-1), g=1-r, b=1. Default n=256. Octave core has cool. |
| `spring` | — | 0.003 | 142.79× | 68.25× | OK | Sig: map = spring([n]). N×3 magenta→yellow. r=1, g=(0:n-1)/(n-1), b=1-g. Default n=256. Octave core has spring. |
| `summer` | — | 0.003 | 204.72× | 64.76× | OK | Sig: map = summer([n]). N×3 green→yellow. r=(0:n-1)/(n-1), g=0.5+r/2, b=0.4. Default n=256. Octave core has summer. |
| `autumn` | — | 0.003 | 131.14× | 25.93× | OK | Sig: map = autumn([n]). N×3 red→yellow. r=1, g=(0:n-1)/(n-1), b=0. Default n=256. MATLAB+Octave both ship autumn. |
| `winter` | — | 0.004 | 119.12× | 52.83× | OK | Sig: map = winter([n]). N×3 blue→cyan-ish. r=0, g=(0:n-1)/(n-1), b=1-g/2. Default n=256. MATLAB+Octave both ship winter. |
| `copper` | — | 0.003 | 212.37× | 38.74× | OK | Sig: map = copper([n]). N×3 black→copper. r=min(5/4*x,1), g=0.7812*x, b=0.4975*x where x=(0:n-1)/(n-1). Default n=256. MATLAB+Octave. |
| `pink` | — | 0.004 | 254.55× | 62.29× | OK | Sig: map = pink([n]). N×3 pastel pink. 3-piece linspace ramps per channel, then sqrt. Default n=256. MATLAB+Octave both ship pink. |
| `hsv` | — | 0.004 | 363.64× | 67.60× | OK | Sig: map = hsv([n]). Hue rotation via hsv2rgb([(0:n-1)'/n, 1, 1]). Default n=256. MATLAB+Octave both ship hsv. |
| `flag` | — | 0.003 | 194.03× | 66.33× | OK | Sig: map = flag([n]). N×3 cycling [1 0 0; 1 1 1; 0 0 1; 0 0 0]. Default n=256. MATLAB+Octave both ship flag. |
| `prism` | — | 0.003 | 186.05× | 30.06× | OK | Sig: map = prism([n]). N×3 cyclic 6-row rainbow [r,o,y,g,b,v]. Default n=256. MATLAB+Octave both ship prism. |
| `lines` | — | 0.003 | 230.31× | 41.74× | OK | Sig: map = lines([n]). Cycles the figure axes colororder. We pin the MATLAB R2025b factory 7-row palette (Octave's older default differs). MATLAB+factory; harness ranks MATLAB as truth so OK is expected. |
| `bone` | — | 0.004 | 246.57× | 69.89× | OK | Sig: map = bone([n]). N×3 grayscale-with-blue-tint colormap. Per Octave's bone.m: idx=floor(3/4·n) for R, idx=floor(3/8·n) for G/B; piecewise linspace ramps; switch on mod(n,8) for base. Default n=256. MATLAB+Octave both match. |
| `white` | — | 0.003 | 117.13× | 61.79× | OK | Sig: map = white([n]). N×3 all-ones colormap. Default n=256. MATLAB+Octave both ship white. |
| `brighten` | — | 0.005 | 77.84× | 21.06× | OK | Sig: rmap = brighten(map, beta). Output = map .^ gamma where gamma = 1-beta if beta>0 else 1/(1+beta). MATLAB+Octave both ship brighten. |
| `contrast` | — | 0.004 | 90.91× | 62.89× | OK | Sig: cmap = contrast(x[, m]). Histogram-equalising gray colormap. Per MATLAB R2025b cleve-moler algorithm: scale to [0,m-1] ints, concat with [0..m], find rising edges. MATLAB+Octave both ship contrast but Octave gives slightly different values; we follow MATLAB. |
| `cdf_upper` | — | 0.014 | 758.79× | 143.49× | OK | Joint 'upper' flag verification across 14 CDFs (closes 14 audit ТЗ in stats.dist). MATLAB R2025b: every *cdf accepts trailing 'upper' string and returns 1 - F(x). normcdf double-checks lower tail unchanged. tol = 1e-9. Closes audit/findings/stats/{normcdf,chi2cdf,tcdf,fcdf,betacdf,gamcdf,expcdf,raylcdf,logncdf,wblcdf,unifcdf,unidcdf,binocdf,poisscdf}.md. |
| `windows_sflag` | — | 0.012 | 360.65× | 33.26× | OK | Joint 'periodic' / 'symmetric' (default) sflag verification across 6 signal.windows that accept it. Implementation trick: periodic(N) = first N samples of symmetric(N+1) — works for any window. The other 6 windows (bartlett/triang/parzenwin/bohmanwin/barthannwin/rectwin) accept ONLY 'double'/'single' typeName and throw on 'periodic' (gtest covers that branch). Closes audit/findings/signal/{hamming,hann,blackman,blackmanharris,flattopwin,nuttallwin,bartlett,triang,parzenwin,bohmanwin,barthannwin,rectwin}.md. |
| `kstest_extras` | — | 0.020 | 151.57× | 70.32× | OK | Sig: kstest2(x, y[, alpha, tail | name-value]). Tail accepts 'unequal' (default), 'larger', 'smaller' (synonyms for 'both', 'right', 'left' from kstest). Name-Value pairs: 'Alpha', 'Tail'. Closes audit/findings/stats/{kstest,kstest2}.md. |
| `ttest_extras` | — | 0.016 | 329.21× | 801.78× | OK | Sig: ttest(x, y[, NV]) paired form; ttest2 default Vartype=equal (pooled). NV pairs: Alpha, Tail, Vartype, Dim (Dim throws). 4th output struct (tstat/df/sd) NOT yet implemented — fingerprints stay on first 3 outputs. Closes audit/findings/stats/{ttest,ttest2}.md (partial — 4th-output struct, matrix input, Dim, n<2 NaN remain as documented gaps in spec comment). |
| `vartest_extras` | — | 0.013 | 361.43× | 1349.01× | OK | Sig: vartest(x, v[, NV]) and vartest2(x, y[, NV]). Both adapters now parse Alpha and Tail Name-Value pairs (case-insensitive). 'Dim' N-V throws (parity gap). 4th output remains scalar T/F (struct deferred). Closes audit/findings/stats/{vartest,vartest2}.md (partial). |
| `ztest_extras` | — | 0.007 | 407.16× | 78.87× | OK | Sig: ztest(x, m, sigma[, NV]). Alpha/Tail Name-Value pairs (case-insensitive). 'Dim' N-V throws (parity gap). 4th output is scalar zval (matches MATLAB which doesn't return a struct here). Closes audit/findings/stats/ztest.md. |
