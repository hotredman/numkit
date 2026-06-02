# Numkit benchmark

**Performance map** for numkit — per-function wall-clock vs MATLAB R2025b
and Octave, at a **small** and a **large** input. Implementation status and
correctness live in [PROGRESS.md](PROGRESS.md); this file is about *speed
only* and mirrors PROGRESS.md's sections so every function appears here too.

Updated by `tools/parity/run_parity.py` (the SIMD `desktop-fast` build): a
spec timed on large data needs a `bench_setup` template using the variable
`N`, which the harness sets to the spec's two `bench_sizes`. Rows without
one stay **blank = not yet benched**.

**Sizes (the `notes` column states the exact input when non-default):**
- **vectors / elementwise** (default): small = **1000** elements, large =
  **1 000 000** elements.
- **images** (Image Processing): small = **100×100**, large = **1000×1000**
  (N = side length, N×N).
- a few transforms pick a clean size (e.g. `fft` rounds N to a power of two).

**Columns:** `nk small/large (ms)` = numkit mean per-call time; `ML× s/l`
and `OC× s/l` = MATLAB_ms / numkit_ms and Octave_ms / numkit_ms
(**>1× = numkit faster**). Blank ratio = that reference engine not measured.

> **Reading the two sizes.** At the small size, per-call interpreter
> overhead still shows (MATLAB's is large, numkit's tiny → ratios flatter
> numkit). At the large size the kernel dominates → that is the honest
> throughput number, where numkit's single-thread Highway-SIMD kernels are
> usually **slower** than MATLAB's multithreaded ones. The C++
> micro-benchmarks under `benchmarks/` and `libs/*/benchmarks/` remain the
> deepest perf source.
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

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `ans` |  |  |  |  |  |  |  |
| `clc` |  |  |  |  |  |  |  |
| `commandhistory` |  |  |  |  |  |  | not implemented |
| `commandwindow` |  |  |  |  |  |  | not implemented |
| `diary` |  |  |  |  |  |  | not implemented |
| `format` |  |  |  |  |  |  |  |
| `home` |  |  |  |  |  |  |  |
| `iskeyword` |  |  |  |  |  |  |  |
| `more` |  |  |  |  |  |  | not implemented |

### Matrices and Arrays

**Namespace:** builtin — 53 ✅ + 1 ⚠️ / 55 = 98%

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `blkdiag` |  |  |  |  |  |  |  |
| `cat` |  |  |  |  |  |  |  |
| `circshift` |  |  |  |  |  |  |  |
| `colon` |  |  |  |  |  |  | partial |
| `combinations` |  |  |  |  |  |  | not implemented |
| `ctranspose` |  |  |  |  |  |  |  |
| `diag` |  |  |  |  |  |  |  |
| `end` |  |  |  |  |  |  |  |
| `eye` |  |  |  |  |  |  |  |
| `false` |  |  |  |  |  |  |  |
| `flip` |  |  |  |  |  |  |  |
| `fliplr` |  |  |  |  |  |  |  |
| `flipud` |  |  |  |  |  |  |  |
| `freqspace` |  |  |  |  |  |  |  |
| `head` |  |  |  |  |  |  |  |
| `horzcat` |  |  |  |  |  |  |  |
| `ind2sub` |  |  |  |  |  |  |  |
| `ipermute` |  |  |  |  |  |  |  |
| `iscolumn` |  |  |  |  |  |  |  |
| `isempty` |  |  |  |  |  |  |  |
| `ismatrix` |  |  |  |  |  |  |  |
| `isrow` |  |  |  |  |  |  |  |
| `isscalar` |  |  |  |  |  |  |  |
| `issorted` |  |  |  |  |  |  |  |
| `issortedrows` |  |  |  |  |  |  |  |
| `isuniform` |  |  |  |  |  |  |  |
| `isvector` |  |  |  |  |  |  |  |
| `length` |  |  |  |  |  |  |  |
| `linspace` |  |  |  |  |  |  |  |
| `logspace` |  |  |  |  |  |  |  |
| `meshgrid` |  |  |  |  |  |  |  |
| `ndgrid` |  |  |  |  |  |  |  |
| `ndims` |  |  |  |  |  |  |  |
| `numel` |  |  |  |  |  |  |  |
| `ones` |  |  |  |  |  |  |  |
| `paddata` |  |  |  |  |  |  |  |
| `permute` |  |  |  |  |  |  |  |
| `rand` |  |  |  |  |  |  |  |
| `repelem` |  |  |  |  |  |  |  |
| `repmat` |  |  |  |  |  |  |  |
| `reshape` |  |  |  |  |  |  |  |
| `resize` |  |  |  |  |  |  |  |
| `rot90` |  |  |  |  |  |  |  |
| `shiftdim` |  |  |  |  |  |  |  |
| `size` |  |  |  |  |  |  |  |
| `sort` | 0.01937 | 0.70× |  | 38.77 | 0.31× |  |  |
| `sortrows` |  |  |  |  |  |  |  |
| `squeeze` |  |  |  |  |  |  |  |
| `sub2ind` |  |  |  |  |  |  |  |
| `tail` |  |  |  |  |  |  |  |
| `transpose` |  |  |  |  |  |  |  |
| `trimdata` |  |  |  |  |  |  |  |
| `true` |  |  |  |  |  |  |  |
| `vertcat` |  |  |  |  |  |  |  |
| `zeros` |  |  |  |  |  |  |  |

### Control Flow

**Namespace:** builtin (keywords) — 10 ✅ + 0 ⚠️ / 11 = 91%

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `break` |  |  |  |  |  |  |  |
| `continue` |  |  |  |  |  |  |  |
| `end` |  |  |  |  |  |  |  |
| `for` |  |  |  |  |  |  |  |
| `if` |  |  |  |  |  |  |  |
| `parfor` |  |  |  |  |  |  | not implemented |
| `pause` |  |  |  |  |  |  |  |
| `return` |  |  |  |  |  |  |  |
| `switch` |  |  |  |  |  |  |  |
| `try` |  |  |  |  |  |  |  |
| `while` |  |  |  |  |  |  |  |

### Numeric Types

**Namespace:** builtin — 27 ✅ + 0 ⚠️ / 29 = 93%

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `allfinite` |  |  |  |  |  |  |  |
| `anynan` |  |  |  |  |  |  |  |
| `cast` |  |  |  |  |  |  |  |
| `double` |  |  |  |  |  |  |  |
| `eps` |  |  |  |  |  |  |  |
| `flintmax` |  |  |  |  |  |  |  |
| `inf` |  |  |  |  |  |  |  |
| `int16` |  |  |  |  |  |  |  |
| `int32` |  |  |  |  |  |  |  |
| `int64` |  |  |  |  |  |  |  |
| `int8` |  |  |  |  |  |  |  |
| `intmax` |  |  |  |  |  |  |  |
| `intmin` |  |  |  |  |  |  |  |
| `isfinite` |  |  |  |  |  |  |  |
| `isfloat` |  |  |  |  |  |  |  |
| `isinf` |  |  |  |  |  |  |  |
| `isinteger` |  |  |  |  |  |  |  |
| `isnan` |  |  |  |  |  |  |  |
| `isnumeric` |  |  |  |  |  |  |  |
| `isreal` |  |  |  |  |  |  |  |
| `nan` |  |  |  |  |  |  |  |
| `realmax` |  |  |  |  |  |  |  |
| `realmin` |  |  |  |  |  |  |  |
| `single` |  |  |  |  |  |  |  |
| `typecast` |  |  |  |  |  |  |  |
| `uint16` |  |  |  |  |  |  |  |
| `uint32` |  |  |  |  |  |  |  |
| `uint64` |  |  |  |  |  |  |  |
| `uint8` |  |  |  |  |  |  |  |

### Characters and Strings

**Namespace:** builtin — 54 ✅ + 0 ⚠️ / 65 = 83%

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `append` |  |  |  |  |  |  |  |
| `blanks` |  |  |  |  |  |  |  |
| `cellstr` |  |  |  |  |  |  |  |
| `char` |  |  |  |  |  |  |  |
| `compose` |  |  |  |  |  |  |  |
| `contains` |  |  |  |  |  |  |  |
| `convertcharstostrings` |  |  |  |  |  |  |  |
| `convertcontainedstringstochars` |  |  |  |  |  |  |  |
| `convertstringstochars` |  |  |  |  |  |  |  |
| `count` |  |  |  |  |  |  |  |
| `deblank` |  |  |  |  |  |  |  |
| `double` |  |  |  |  |  |  |  |
| `endsWith` |  |  |  |  |  |  |  |
| `erase` |  |  |  |  |  |  |  |
| `erasebetween` |  |  |  |  |  |  |  |
| `extract` |  |  |  |  |  |  |  |
| `extractafter` |  |  |  |  |  |  |  |
| `extractbefore` |  |  |  |  |  |  |  |
| `extractbetween` |  |  |  |  |  |  |  |
| `insertafter` |  |  |  |  |  |  |  |
| `insertbefore` |  |  |  |  |  |  |  |
| `iscellstr` |  |  |  |  |  |  |  |
| `ischar` |  |  |  |  |  |  |  |
| `isletter` |  |  |  |  |  |  |  |
| `isspace` |  |  |  |  |  |  |  |
| `isstring` |  |  |  |  |  |  |  |
| `isstringscalar` |  |  |  |  |  |  |  |
| `isstrprop` |  |  |  |  |  |  |  |
| `join` |  |  |  |  |  |  |  |
| `lower` |  |  |  |  |  |  |  |
| `matches` |  |  |  |  |  |  |  |
| `newline` |  |  |  |  |  |  |  |
| `num2str` |  |  |  |  |  |  |  |
| `pad` |  |  |  |  |  |  |  |
| `plus` |  |  |  |  |  |  |  |
| `regexp` |  |  |  |  |  |  |  |
| `regexpi` |  |  |  |  |  |  |  |
| `regexprep` |  |  |  |  |  |  |  |
| `regexptranslate` |  |  |  |  |  |  |  |
| `replace` |  |  |  |  |  |  |  |
| `replacebetween` |  |  |  |  |  |  |  |
| `reverse` |  |  |  |  |  |  |  |
| `split` |  |  |  |  |  |  |  |
| `splitlines` |  |  |  |  |  |  |  |
| `sprintf` |  |  |  |  |  |  |  |
| `sscanf` |  |  |  |  |  |  |  |
| `startsWith` |  |  |  |  |  |  |  |
| `str2double` |  |  |  |  |  |  |  |
| `strcat` |  |  |  |  |  |  |  |
| `strcmp` |  |  |  |  |  |  |  |
| `strcmpi` |  |  |  |  |  |  |  |
| `strfind` |  |  |  |  |  |  |  |
| `string` |  |  |  |  |  |  |  |
| `strings` |  |  |  |  |  |  |  |
| `strip` |  |  |  |  |  |  |  |
| `strjoin` |  |  |  |  |  |  |  |
| `strjust` |  |  |  |  |  |  |  |
| `strlength` |  |  |  |  |  |  |  |
| `strncmp` |  |  |  |  |  |  |  |
| `strncmpi` |  |  |  |  |  |  |  |
| `strrep` |  |  |  |  |  |  |  |
| `strsplit` |  |  |  |  |  |  |  |
| `strtok` |  |  |  |  |  |  |  |
| `strtrim` |  |  |  |  |  |  |  |
| `upper` |  |  |  |  |  |  |  |

### Structures

**Namespace:** builtin — 12 ✅ + 0 ⚠️ / 14 = 86%

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `arrayfun` |  |  |  |  |  |  |  |
| `cell2struct` |  |  |  |  |  |  |  |
| `fieldnames` |  |  |  |  |  |  |  |
| `getfield` |  |  |  |  |  |  |  |
| `isfield` |  |  |  |  |  |  |  |
| `isstruct` |  |  |  |  |  |  |  |
| `orderfields` |  |  |  |  |  |  |  |
| `rmfield` |  |  |  |  |  |  |  |
| `setfield` |  |  |  |  |  |  |  |
| `struct` |  |  |  |  |  |  |  |
| `struct2cell` |  |  |  |  |  |  |  |
| `struct2table` |  |  |  |  |  |  | not implemented |
| `structfun` |  |  |  |  |  |  |  |
| `table2struct` |  |  |  |  |  |  | not implemented |

### Cell Arrays

**Namespace:** builtin — 12 ✅ + 0 ⚠️ / 17 = 71%

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `cell` |  |  |  |  |  |  |  |
| `cell2mat` |  |  |  |  |  |  |  |
| `cell2struct` |  |  |  |  |  |  |  |
| `cell2table` |  |  |  |  |  |  | not implemented |
| `celldisp` |  |  |  |  |  |  |  |
| `cellfun` |  |  |  |  |  |  |  |
| `cellplot` |  |  |  |  |  |  | not implemented |
| `cellstr` |  |  |  |  |  |  |  |
| `iscell` |  |  |  |  |  |  |  |
| `iscellstr` |  |  |  |  |  |  |  |
| `mat2cell` |  |  |  |  |  |  |  |
| `num2cell` |  |  |  |  |  |  |  |
| `string` |  |  |  |  |  |  |  |
| `struct2cell` |  |  |  |  |  |  |  |
| `table` |  |  |  |  |  |  | not implemented |
| `table2cell` |  |  |  |  |  |  | not implemented |
| `timetable` |  |  |  |  |  |  | not implemented |

### Function Handles

**Namespace:** builtin — 5 ✅ + 0 ⚠️ / 6 = 83%

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `feval` |  |  |  |  |  |  |  |
| `func2str` |  |  |  |  |  |  |  |
| `function_handle` |  |  |  |  |  |  | not implemented |
| `functions` |  |  |  |  |  |  |  |
| `localfunctions` |  |  |  |  |  |  |  |
| `str2func` |  |  |  |  |  |  |  |

### Categorical Arrays

**Namespace:** `categorical.*` (future) — 1 ✅ + 0 ⚠️ / 17 = 5%

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `addcats` |  |  |  |  |  |  | not implemented |
| `categorical` |  |  |  |  |  |  | not implemented |
| `categories` |  |  |  |  |  |  | not implemented |
| `combinations` |  |  |  |  |  |  | not implemented |
| `countcats` |  |  |  |  |  |  | not implemented |
| `discretize` |  |  |  |  |  |  |  |
| `iscategory` |  |  |  |  |  |  | not implemented |
| `isordinal` |  |  |  |  |  |  | not implemented |
| `isprotected` |  |  |  |  |  |  | not implemented |
| `isundefined` |  |  |  |  |  |  | not implemented |
| `mergecats` |  |  |  |  |  |  | not implemented |
| `removecats` |  |  |  |  |  |  | not implemented |
| `renamecats` |  |  |  |  |  |  | not implemented |
| `reordercats` |  |  |  |  |  |  | not implemented |
| `setcats` |  |  |  |  |  |  | not implemented |
| `summary` |  |  |  |  |  |  | not implemented |

### Tables / Timetables

**Namespace:** `table.*` (future) — 6 ✅ + 0 ⚠️ / 66 = 9%

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `addprop` |  |  |  |  |  |  | not implemented |
| `addvars` |  |  |  |  |  |  | not implemented |
| `anymissing` |  |  |  |  |  |  |  |
| `array2table` |  |  |  |  |  |  | not implemented |
| `cell2table` |  |  |  |  |  |  | not implemented |
| `computebygroup` |  |  |  |  |  |  | not implemented |
| `convertvars` |  |  |  |  |  |  | not implemented |
| `fillmissing` |  |  |  |  |  |  | partial |
| `findgroups` |  |  |  |  |  |  |  |
| `groupcounts` |  |  |  |  |  |  |  |
| `groupfilter` |  |  |  |  |  |  |  |
| `groupsummary` |  |  |  |  |  |  |  |
| `grouptransform` |  |  |  |  |  |  |  |
| `head` |  |  |  |  |  |  |  |
| `height` |  |  |  |  |  |  | not implemented |
| `inner2outer` |  |  |  |  |  |  | not implemented |
| `innerjoin` |  |  |  |  |  |  | not implemented |
| `intersect` |  |  |  |  |  |  |  |
| `ismember` |  |  |  |  |  |  |  |
| `ismissing` |  |  |  |  |  |  |  |
| `issortedrows` |  |  |  |  |  |  |  |
| `join` |  |  |  |  |  |  |  |
| `jointables` |  |  |  |  |  |  | not implemented |
| `mergevars` |  |  |  |  |  |  | not implemented |
| `movevars` |  |  |  |  |  |  | not implemented |
| `outerjoin` |  |  |  |  |  |  | not implemented |
| `parquetread` |  |  |  |  |  |  | not implemented |
| `parquetwrite` |  |  |  |  |  |  | not implemented |
| `pivot` |  |  |  |  |  |  | not implemented |
| `pivottable` |  |  |  |  |  |  | not implemented |
| `readtable` |  |  |  |  |  |  | not implemented |
| `removevars` |  |  |  |  |  |  | not implemented |
| `renamevars` |  |  |  |  |  |  | not implemented |
| `rmmissing` |  |  |  |  |  |  |  |
| `rmprop` |  |  |  |  |  |  | not implemented |
| `rowfun` |  |  |  |  |  |  | not implemented |
| `rows2vars` |  |  |  |  |  |  | not implemented |
| `setdiff` |  |  |  |  |  |  |  |
| `setxor` |  |  |  |  |  |  |  |
| `sortrows` |  |  |  |  |  |  |  |
| `splitapply` |  |  |  |  |  |  |  |
| `splitvars` |  |  |  |  |  |  | not implemented |
| `stack` |  |  |  |  |  |  | not implemented |
| `stackedplot` |  |  |  |  |  |  | not implemented |
| `stacktablevariables` |  |  |  |  |  |  | not implemented |
| `standardizemissing` |  |  |  |  |  |  | not implemented |
| `struct2table` |  |  |  |  |  |  | not implemented |
| `summary` |  |  |  |  |  |  | not implemented |
| `table` |  |  |  |  |  |  | not implemented |
| `table2array` |  |  |  |  |  |  | not implemented |
| `table2cell` |  |  |  |  |  |  | not implemented |
| `table2struct` |  |  |  |  |  |  | not implemented |
| `table2timetable` |  |  |  |  |  |  | not implemented |
| `tail` |  |  |  |  |  |  |  |
| `timetable2table` |  |  |  |  |  |  | not implemented |
| `topkrows` |  |  |  |  |  |  | partial |
| `union` |  |  |  |  |  |  |  |
| `unique` |  |  |  |  |  |  |  |
| `unstack` |  |  |  |  |  |  | not implemented |
| `unstacktablevariables` |  |  |  |  |  |  | not implemented |
| `varfun` |  |  |  |  |  |  | not implemented |
| `vartype` |  |  |  |  |  |  | not implemented |
| `width` |  |  |  |  |  |  | not implemented |
| `writetable` |  |  |  |  |  |  | not implemented |

### Bit-wise Operations

**Namespace:** builtin — 7 ✅ + 0 ⚠️ / 8 = 88%

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `bitand` |  |  |  |  |  |  |  |
| `bitcmp` |  |  |  |  |  |  |  |
| `bitget` |  |  |  |  |  |  |  |
| `bitor` |  |  |  |  |  |  |  |
| `bitset` |  |  |  |  |  |  |  |
| `bitshift` |  |  |  |  |  |  |  |
| `bitxor` |  |  |  |  |  |  |  |
| `swapbytes` |  |  |  |  |  |  |  |

### Set Operations

**Namespace:** builtin — 10 ✅ + 0 ⚠️ / 13 = 77%

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `allunique` |  |  |  |  |  |  |  |
| `innerjoin` |  |  |  |  |  |  | not implemented |
| `intersect` |  |  |  |  |  |  |  |
| `ismember` |  |  |  |  |  |  |  |
| `ismembertol` |  |  |  |  |  |  |  |
| `join` |  |  |  |  |  |  |  |
| `numunique` |  |  |  |  |  |  |  |
| `outerjoin` |  |  |  |  |  |  | not implemented |
| `setdiff` |  |  |  |  |  |  |  |
| `setxor` |  |  |  |  |  |  |  |
| `union` |  |  |  |  |  |  |  |
| `unique` |  |  |  |  |  |  |  |
| `uniquetol` |  |  |  |  |  |  |  |

### Arithmetic

**Namespace:** builtin — 28 ✅ + 0 ⚠️ / 34 = 82%

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `bsxfun` |  |  |  |  |  |  |  |
| `ceil` |  |  |  |  |  |  |  |
| `ctranspose` |  |  |  |  |  |  |  |
| `cumprod` |  |  |  |  |  |  |  |
| `cumsum` | 0.00138 | 4.19× |  | 2.068 | 0.71× |  |  |
| `diff` |  |  |  |  |  |  |  |
| `fix` | 0.000356 | 26.65× |  | 2.182 | 0.07× |  |  |
| `floor` | 0.000322 | 26.84× |  | 2.32 | 0.09× |  |  |
| `idivide` |  |  |  |  |  |  |  |
| `ldivide` |  |  |  |  |  |  |  |
| `minus` |  |  |  |  |  |  |  |
| `mldivide` |  |  |  |  |  |  |  |
| `mod` | 0.001814 | 9.07× |  | 3.435 | 0.13× |  |  |
| `movsum` |  |  |  |  |  |  |  |
| `mpower` |  |  |  |  |  |  |  |
| `mrdivide` |  |  |  |  |  |  |  |
| `mtimes` |  |  |  |  |  |  |  |
| `pagectranspose` |  |  |  |  |  |  |  |
| `pagemldivide` |  |  |  |  |  |  |  |
| `pagemrdivide` |  |  |  |  |  |  |  |
| `pagemtimes` |  |  |  |  |  |  |  |
| `pagetranspose` |  |  |  |  |  |  |  |
| `plus` |  |  |  |  |  |  |  |
| `power` |  |  |  |  |  |  |  |
| `prod` | 0.002612 | 2.19× |  | 2.193 | 0.04× |  |  |
| `rdivide` |  |  |  |  |  |  |  |
| `rem` | 0.0035 | 2.93× |  | 5.036 | 0.05× |  |  |
| `round` | 0.00038 | 22.69× |  | 2.14 | 0.09× |  |  |
| `sum` | 0.001656 | 3.81× |  | 1.36 | 0.06× |  |  |
| `tensorprod` |  |  |  |  |  |  | not implemented |
| `times` |  |  |  |  |  |  |  |
| `transpose` |  |  |  |  |  |  |  |
| `uminus` | 0.003492 | 1.28× |  | 4.43 | 0.03× |  |  |
| `uplus` | 0.000378 | 11.05× |  | 0.00062 | 22.28× |  |  |

### Trigonometry

**Namespace:** builtin — 47 ✅ + 0 ⚠️ / 47 = 100%

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `acos` | 0.001326 | 6.20× |  | 2.692 | 0.47× |  |  |
| `acosd` |  |  |  |  |  |  |  |
| `acosh` | 0.00233 | 6.51× |  | 3.742 | 0.43× |  |  |
| `acot` | 0.001638 | 5.59× |  | 2.93 | 0.08× |  |  |
| `acotd` |  |  |  |  |  |  |  |
| `acoth` |  |  |  |  |  |  |  |
| `acsc` | 0.001366 | 6.21× |  | 2.795 | 0.43× |  |  |
| `acscd` |  |  |  |  |  |  |  |
| `acsch` |  |  |  |  |  |  |  |
| `asec` | 0.001518 | 6.34× |  | 2.969 | 0.40× |  |  |
| `asecd` |  |  |  |  |  |  |  |
| `asech` |  |  |  |  |  |  |  |
| `asin` | 0.001308 | 9.27× |  | 2.642 | 0.53× |  |  |
| `asind` |  |  |  |  |  |  |  |
| `asinh` | 0.00238 | 7.41× |  | 3.709 | 0.29× |  |  |
| `atan` | 0.00158 | 6.73× |  | 2.78 | 0.11× |  |  |
| `atan2` |  |  |  |  |  |  |  |
| `atan2d` |  |  |  |  |  |  |  |
| `atand` |  |  |  |  |  |  |  |
| `atanh` | 0.00175 | 27.14× |  | 2.852 | 0.73× |  |  |
| `cart2pol` |  |  |  |  |  |  |  |
| `cart2sph` |  |  |  |  |  |  |  |
| `cos` | 0.000826 | 13.29× |  | 0.7703 | 0.66× |  |  |
| `cosd` |  |  |  |  |  |  |  |
| `cosh` | 0.002174 | 5.32× |  | 3.726 | 0.14× |  |  |
| `cospi` |  |  |  |  |  |  |  |
| `cot` | 0.002032 | 5.38× |  | 3.412 | 0.21× |  |  |
| `cotd` |  |  |  |  |  |  |  |
| `coth` | 0.00325 | 4.42× |  | 4.512 | 0.33× |  |  |
| `csc` | 0.001334 | 10.52× |  | 2.726 | 0.15× |  |  |
| `cscd` |  |  |  |  |  |  |  |
| `csch` | 0.001816 | 8.79× |  | 3.659 | 0.28× |  |  |
| `deg2rad` | 0.00367 | 2.32× |  | 4.51 | 0.21× |  |  |
| `hypot` | 0.00097 | 16.32× |  | 2.467 | 0.31× |  |  |
| `pol2cart` |  |  |  |  |  |  |  |
| `rad2deg` | 0.003622 | 2.25× |  | 4.549 | 0.22× |  |  |
| `sec` | 0.00139 | 10.02× |  | 2.821 | 0.18× |  |  |
| `secd` |  |  |  |  |  |  |  |
| `sech` | 0.00219 | 5.88× |  | 3.681 | 0.20× |  |  |
| `sin` | 0.000786 | 20.92× |  | 0.7292 | 0.59× |  |  |
| `sind` |  |  |  |  |  |  |  |
| `sinh` | 0.00171 | 7.56× |  | 3.323 | 0.25× |  |  |
| `sinpi` |  |  |  |  |  |  |  |
| `sph2cart` |  |  |  |  |  |  |  |
| `tan` | 0.00175 | 5.58× |  | 3.29 | 0.16× |  |  |
| `tand` |  |  |  |  |  |  |  |
| `tanh` | 0.00178 | 6.58× |  | 3.112 | 0.28× |  |  |

### Exponents and Logarithms

**Namespace:** builtin — 13 ✅ + 0 ⚠️ / 13 = 100%

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `exp` | 0.000862 | 14.55× |  | 0.8328 | 0.34× |  |  |
| `expm1` | 0.006618 | 1.46× |  | 7.983 | 0.03× |  |  |
| `log` | 0.000722 | 18.31× |  | 0.6909 | 2.27× |  |  |
| `log10` | 0.005816 | 2.61× |  | 7.4 | 0.23× |  |  |
| `log1p` | 0.006868 | 1.65× |  | 8.206 | 0.15× |  |  |
| `log2` | 0.00806 | 1.76× |  | 9.712 | 0.18× |  |  |
| `nextpow2` |  |  |  |  |  |  |  |
| `nthroot` | 0.008838 | 16.29× |  | 10.01 | 1.61× |  |  |
| `pow2` | 0.004842 | 6.63× |  | 6.212 | 0.57× |  |  |
| `reallog` | 0.006014 | 7.51× |  | 7.365 | 0.22× |  |  |
| `realpow` |  |  |  |  |  |  |  |
| `realsqrt` | 0.003554 | 4.41× |  | 5.029 | 0.21× |  |  |
| `sqrt` | 0.003516 | 1.37× |  | 4.718 | 0.22× |  |  |

### Special Functions

**Namespace:** builtin — 20 ✅ + 0 ⚠️ / 24 = 83%

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `airy` |  |  |  |  |  |  |  |
| `besselh` |  |  |  |  |  |  |  |
| `besseli` |  |  |  |  |  |  |  |
| `besselj` |  |  |  |  |  |  |  |
| `besselk` |  |  |  |  |  |  |  |
| `bessely` |  |  |  |  |  |  |  |
| `beta` | 0.07595 | 0.54× |  | 71.64 | 0.09× |  |  |
| `betainc` |  |  |  |  |  |  |  |
| `betaincinv` |  |  |  |  |  |  |  |
| `betaln` |  |  |  |  |  |  |  |
| `ellipj` |  |  |  |  |  |  |  |
| `ellipke` |  |  |  |  |  |  |  |
| `erf` | 0.008528 | 1.47× |  | 9.945 | 0.19× |  |  |
| `erfc` | 0.01132 | 1.47× |  | 12.88 | 0.14× |  |  |
| `erfcinv` | 0.04583 | 0.32× |  | 47.78 | 0.07× |  |  |
| `erfcx` |  |  |  |  |  |  |  |
| `erfinv` | 0.04825 | 0.34× |  | 46.8 | 0.06× |  |  |
| `expint` |  |  |  |  |  |  |  |
| `gamma` | 0.01438 | 0.66× |  | 16.14 | 0.08× |  |  |
| `gammainc` |  |  |  |  |  |  |  |
| `gammaincinv` |  |  |  |  |  |  |  |
| `gammaln` | 0.02075 | 0.49× |  | 21.88 | 0.06× |  |  |
| `legendre` |  |  |  |  |  |  |  |
| `psi` |  |  |  |  |  |  |  |

### Discrete Math

**Namespace:** builtin — 10 ✅ + 0 ⚠️ / 11 = 90%

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `factor` |  |  |  |  |  |  |  |
| `factorial` | 0.02961 | 0.50× |  | 30.42 | 0.37× |  |  |
| `gcd` |  |  |  |  |  |  |  |
| `isprime` |  |  |  |  |  |  |  |
| `lcm` |  |  |  |  |  |  |  |
| `matchpairs` |  |  |  |  |  |  |  |
| `nchoosek` |  |  |  |  |  |  |  |
| `perms` |  |  |  |  |  |  |  |
| `primes` |  |  |  |  |  |  |  |
| `rat` |  |  |  |  |  |  |  |
| `rats` |  |  |  |  |  |  |  |

### Polynomials

**Namespace:** builtin — 10 ✅ + 0 ⚠️ / 12 = 83%

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `poly` |  |  |  |  |  |  |  |
| `polyder` |  |  |  |  |  |  |  |
| `polydiv` |  |  |  |  |  |  |  |
| `polyeig` |  |  |  |  |  |  |  |
| `polyfit` |  |  |  |  |  |  |  |
| `polyint` |  |  |  |  |  |  |  |
| `polyval` |  |  |  |  |  |  |  |
| `polyvalm` |  |  |  |  |  |  |  |
| `residue` |  |  |  |  |  |  |  |
| `roots` |  |  |  |  |  |  |  |
| `padecoef` |  |  |  |  |  |  |  |

### Random Number Generation

**Namespace:** builtin — 5 ✅ + 0 ⚠️ / 6 = 83%

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `rand` |  |  |  |  |  |  |  |
| `randi` |  |  |  |  |  |  |  |
| `randn` |  |  |  |  |  |  |  |
| `randperm` |  |  |  |  |  |  |  |
| `randstream` |  |  |  |  |  |  | not implemented |
| `rng` |  |  |  |  |  |  |  |

### Interpolation

**Namespace:** builtin — 11 ✅ + 0 ⚠️ / 18 = 61%

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `griddata` |  |  |  |  |  |  |  |
| `griddatan` |  |  |  |  |  |  |  |
| `griddedinterpolant` |  |  |  |  |  |  | not implemented |
| `interp1` |  |  |  |  |  |  |  |
| `interp2` |  |  |  |  |  |  |  |
| `interp3` |  |  |  |  |  |  |  |
| `interpft` |  |  |  |  |  |  |  |
| `interpn` |  |  |  |  |  |  |  |
| `makima` |  |  |  |  |  |  |  |
| `meshgrid` |  |  |  |  |  |  |  |
| `mkpp` |  |  |  |  |  |  |  |
| `ndgrid` |  |  |  |  |  |  |  |
| `pchip` |  |  |  |  |  |  |  |
| `ppval` |  |  |  |  |  |  |  |
| `scatteredinterpolant` |  |  |  |  |  |  | not implemented |
| `spline` |  |  |  |  |  |  |  |
| `unmkpp` |  |  |  |  |  |  |  |

### Sparse Matrices

**Namespace:** `sparse.*` (future) — 4 ✅ + 0 ⚠️ / 53 = 7%

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `amd` |  |  |  |  |  |  | not implemented |
| `bicg` |  |  |  |  |  |  | not implemented |
| `bicgstab` |  |  |  |  |  |  | not implemented |
| `bicgstabl` |  |  |  |  |  |  | not implemented |
| `cgs` |  |  |  |  |  |  | not implemented |
| `colamd` |  |  |  |  |  |  | not implemented |
| `colperm` |  |  |  |  |  |  |  |
| `condest` |  |  |  |  |  |  |  |
| `dissect` |  |  |  |  |  |  | not implemented |
| `dmperm` |  |  |  |  |  |  | not implemented |
| `eigs` |  |  |  |  |  |  | not implemented |
| `equilibrate` |  |  |  |  |  |  | not implemented |
| `etree` |  |  |  |  |  |  | not implemented |
| `etreeplot` |  |  |  |  |  |  | not implemented |
| `find` |  |  |  |  |  |  |  |
| `full` |  |  |  |  |  |  | not implemented |
| `gmres` |  |  |  |  |  |  | not implemented |
| `gplot` |  |  |  |  |  |  | not implemented |
| `ichol` |  |  |  |  |  |  | not implemented |
| `ilu` |  |  |  |  |  |  | not implemented |
| `issparse` |  |  |  |  |  |  |  |
| `lsqr` |  |  |  |  |  |  | not implemented |
| `minres` |  |  |  |  |  |  | not implemented |
| `nnz` |  |  |  |  |  |  |  |
| `nonzeros` |  |  |  |  |  |  |  |
| `normest` |  |  |  |  |  |  |  |
| `nzmax` |  |  |  |  |  |  | not implemented |
| `pcg` |  |  |  |  |  |  | not implemented |
| `qmr` |  |  |  |  |  |  | not implemented |
| `randperm` |  |  |  |  |  |  |  |
| `spalloc` |  |  |  |  |  |  | not implemented |
| `sparse` |  |  |  |  |  |  | not implemented |
| `spaugment` |  |  |  |  |  |  | not implemented |
| `spconvert` |  |  |  |  |  |  | not implemented |
| `spdiags` |  |  |  |  |  |  | not implemented |
| `speye` |  |  |  |  |  |  | not implemented |
| `spfun` |  |  |  |  |  |  | not implemented |
| `spones` |  |  |  |  |  |  | not implemented |
| `spparms` |  |  |  |  |  |  | not implemented |
| `sprand` |  |  |  |  |  |  | not implemented |
| `sprandn` |  |  |  |  |  |  | not implemented |
| `sprandsym` |  |  |  |  |  |  | not implemented |
| `sprank` |  |  |  |  |  |  | not implemented |
| `spy` |  |  |  |  |  |  | not implemented |
| `svds` |  |  |  |  |  |  | not implemented |
| `symamd` |  |  |  |  |  |  | not implemented |
| `symbfact` |  |  |  |  |  |  | not implemented |
| `symmlq` |  |  |  |  |  |  | not implemented |
| `symrcm` |  |  |  |  |  |  |  |
| `tfqmr` |  |  |  |  |  |  | not implemented |
| `treelayout` |  |  |  |  |  |  | not implemented |
| `treeplot` |  |  |  |  |  |  | not implemented |
| `unmesh` |  |  |  |  |  |  | not implemented |

### Workspace

**Namespace:** builtin — 8 ✅ + 0 ⚠️ / 10 = 80%

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `clear` |  |  |  |  |  |  |  |
| `clearvars` |  |  |  |  |  |  |  |
| `disp` |  |  |  |  |  |  |  |
| `formatteddisplaytext` |  |  |  |  |  |  |  |
| `load` |  |  |  |  |  |  |  |
| `openvar` |  |  |  |  |  |  | not implemented |
| `save` |  |  |  |  |  |  |  |
| `who` |  |  |  |  |  |  |  |
| `whos` |  |  |  |  |  |  |  |
| `workspacebrowser` |  |  |  |  |  |  | not implemented |

### Error Handling (basic)

**Namespace:** builtin — 4 ✅ + 0 ⚠️ / 6 = 66%

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `assert` |  |  |  |  |  |  |  |
| `error` |  |  |  |  |  |  |  |
| `lastwarn` |  |  |  |  |  |  |  |
| `oncleanup` |  |  |  |  |  |  | not implemented |
| `try` |  |  |  |  |  |  |  |
| `warning` |  |  |  |  |  |  |  |

### Exception Handling

**Namespace:** builtin (keyword + class) — 2 ✅ + 0 ⚠️ / 2 = 100%

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `mexception` |  |  |  |  |  |  |  |
| `try` |  |  |  |  |  |  |  |

## Communications

### Modulation

**Namespace:** `comm.mod.*` — 13 ✅ + 0 ⚠️ / 29 = 45%

Function-form modulators / demodulators. The `comm.PSKModulator` /
`comm.QAMModulator` / `comm.OFDMModulator` System Object family is
intentionally omitted, along with `constellation` (object method) and
`showResourceMapping` (display).

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `genqammod` |  |  |  |  |  |  | partial |
| `genqamdemod` |  |  |  |  |  |  | not implemented |
| `modnorm` |  |  |  |  |  |  |  |
| `pammod` |  |  |  |  |  |  |  |
| `pamdemod` |  |  |  |  |  |  |  |
| `qammod` |  |  |  |  |  |  |  |
| `qamdemod` |  |  |  |  |  |  |  |
| `apskmod` |  |  |  |  |  |  | partial |
| `apskdemod` |  |  |  |  |  |  | not implemented |
| `mil188qammod` |  |  |  |  |  |  |  |
| `mil188qamdemod` |  |  |  |  |  |  | not implemented |
| `mskmod` |  |  |  |  |  |  | partial |
| `mskdemod` |  |  |  |  |  |  | not implemented |
| `fskmod` |  |  |  |  |  |  |  |
| `fskdemod` |  |  |  |  |  |  |  |
| `ofdmmod` |  |  |  |  |  |  |  |
| `ofdmdemod` |  |  |  |  |  |  |  |
| `dpskmod` |  |  |  |  |  |  |  |
| `dpskdemod` |  |  |  |  |  |  |  |
| `pskmod` |  |  |  |  |  |  |  |
| `pskdemod` |  |  |  |  |  |  |  |
| `ammod` |  |  |  |  |  |  |  |
| `amdemod` |  |  |  |  |  |  | not implemented |
| `fmmod` |  |  |  |  |  |  |  |
| `fmdemod` |  |  |  |  |  |  | not implemented |
| `pmmod` |  |  |  |  |  |  |  |
| `pmdemod` |  |  |  |  |  |  | not implemented |
| `ssbmod` |  |  |  |  |  |  |  |
| `ssbdemod` |  |  |  |  |  |  | not implemented |

### Sources, Sinks, and Signal Operations

**Namespace:** `comm.signals.*` — 0 ✅ + 0 ⚠️ / 17 = 0%

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `randerr` |  |  |  |  |  |  |  |
| `randsrc` |  |  |  |  |  |  |  |
| `wgn` |  |  |  |  |  |  |  |
| `biterr` |  |  |  |  |  |  |  |
| `symerr` |  |  |  |  |  |  |  |
| `zadoffChuSeq` |  |  |  |  |  |  | not implemented |
| `mask2shift` |  |  |  |  |  |  | not implemented |
| `shift2mask` |  |  |  |  |  |  | not implemented |
| `bit2int` |  |  |  |  |  |  | not implemented |
| `int2bit` |  |  |  |  |  |  | not implemented |
| `bi2de` |  |  |  |  |  |  | not implemented |
| `de2bi` |  |  |  |  |  |  | not implemented |
| `hex2poly` |  |  |  |  |  |  | not implemented |
| `oct2poly` |  |  |  |  |  |  | not implemented |
| `oct2dec` |  |  |  |  |  |  | not implemented |
| `vec2mat` |  |  |  |  |  |  | not implemented |
| `convertSNR` |  |  |  |  |  |  |  |

### Source Coding

**Namespace:** `comm.source_coding.*` — 0 ✅ + 0 ⚠️ / 11 = 0%

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `arithenco` |  |  |  |  |  |  |  |
| `arithdeco` |  |  |  |  |  |  | not implemented |
| `compand` |  |  |  |  |  |  |  |
| `dpcmenco` |  |  |  |  |  |  | partial |
| `dpcmdeco` |  |  |  |  |  |  | not implemented |
| `dpcmopt` |  |  |  |  |  |  |  |
| `huffmandict` |  |  |  |  |  |  |  |
| `huffmanenco` |  |  |  |  |  |  |  |
| `huffmandeco` |  |  |  |  |  |  | not implemented |
| `lloyds` |  |  |  |  |  |  | partial |
| `quantiz` |  |  |  |  |  |  |  |

### Error Detection and Correction

**Namespace:** `comm.fec.*` — 0 ✅ + 0 ⚠️ / 26 = 0%

`crcConfig`, `ldpcEncoderConfig`, `ldpcDecoderConfig`, the System
Objects (`comm.CRCGenerator`, `comm.LDPCEncoder`, etc.) and the `gf`
class are intentionally omitted. Galois-field math is exposed through
the flat `gf*` function family below.

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `crcGenerate` |  |  |  |  |  |  | not implemented |
| `crcDetect` |  |  |  |  |  |  | not implemented |
| `cyclgen` |  |  |  |  |  |  | not implemented |
| `cyclpoly` |  |  |  |  |  |  | not implemented |
| `encode` |  |  |  |  |  |  | not implemented |
| `decode` |  |  |  |  |  |  | not implemented |
| `gfweight` |  |  |  |  |  |  | not implemented |
| `gen2par` |  |  |  |  |  |  | not implemented |
| `hammgen` |  |  |  |  |  |  | not implemented |
| `syndtable` |  |  |  |  |  |  | not implemented |
| `bchenc` |  |  |  |  |  |  | not implemented |
| `bchdec` |  |  |  |  |  |  | not implemented |
| `bchgenpoly` |  |  |  |  |  |  | not implemented |
| `bchnumerr` |  |  |  |  |  |  | not implemented |
| `rsenc` |  |  |  |  |  |  | not implemented |
| `rsdec` |  |  |  |  |  |  | not implemented |
| `rsgenpoly` |  |  |  |  |  |  | not implemented |
| `rsgenpolycoeffs` |  |  |  |  |  |  | not implemented |
| `ldpcEncode` |  |  |  |  |  |  | not implemented |
| `ldpcDecode` |  |  |  |  |  |  | not implemented |
| `ldpcPCM` |  |  |  |  |  |  | not implemented |
| `ldpcQuasiCyclicMatrix` |  |  |  |  |  |  | not implemented |
| `tpcenc` |  |  |  |  |  |  | not implemented |
| `tpcdec` |  |  |  |  |  |  | not implemented |
| `convenc` |  |  |  |  |  |  | not implemented |
| `vitdec` |  |  |  |  |  |  | not implemented |

### Trellis and Galois Field Utilities

**Namespace:** `comm.gf.*` — 0 ✅ + 0 ⚠️ / 22 = 0%

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `distspec` |  |  |  |  |  |  | not implemented |
| `iscatastrophic` |  |  |  |  |  |  | not implemented |
| `istrellis` |  |  |  |  |  |  | not implemented |
| `poly2trellis` |  |  |  |  |  |  | not implemented |
| `cosets` |  |  |  |  |  |  | not implemented |
| `dftmtx` |  |  |  |  |  |  |  |
| `isprimitive` |  |  |  |  |  |  | not implemented |
| `minpol` |  |  |  |  |  |  | not implemented |
| `primpoly` |  |  |  |  |  |  | not implemented |
| `gfadd` |  |  |  |  |  |  | not implemented |
| `gfconv` |  |  |  |  |  |  | not implemented |
| `gfcosets` |  |  |  |  |  |  | not implemented |
| `gfdeconv` |  |  |  |  |  |  | not implemented |
| `gfdiv` |  |  |  |  |  |  | not implemented |
| `gffilter` |  |  |  |  |  |  | not implemented |
| `gflineq` |  |  |  |  |  |  | not implemented |
| `gfminpol` |  |  |  |  |  |  | not implemented |
| `gfmul` |  |  |  |  |  |  | not implemented |
| `gfpretty` |  |  |  |  |  |  | not implemented |
| `gfprimck` |  |  |  |  |  |  | not implemented |
| `gfprimdf` |  |  |  |  |  |  | not implemented |
| `gftuple` |  |  |  |  |  |  | not implemented |

### Interleaving

**Namespace:** `comm.intrlv.*` — 0 ✅ + 0 ⚠️ / 16 = 0%

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `intrlv` |  |  |  |  |  |  | not implemented |
| `deintrlv` |  |  |  |  |  |  | not implemented |
| `algintrlv` |  |  |  |  |  |  | not implemented |
| `algdeintrlv` |  |  |  |  |  |  | not implemented |
| `helscanintrlv` |  |  |  |  |  |  | not implemented |
| `helscandeintrlv` |  |  |  |  |  |  | not implemented |
| `matintrlv` |  |  |  |  |  |  | not implemented |
| `matdeintrlv` |  |  |  |  |  |  | not implemented |
| `randintrlv` |  |  |  |  |  |  | not implemented |
| `randdeintrlv` |  |  |  |  |  |  | not implemented |
| `convintrlv` |  |  |  |  |  |  | not implemented |
| `convdeintrlv` |  |  |  |  |  |  | not implemented |
| `helintrlv` |  |  |  |  |  |  | not implemented |
| `heldeintrlv` |  |  |  |  |  |  | not implemented |
| `muxintrlv` |  |  |  |  |  |  | not implemented |
| `muxdeintrlv` |  |  |  |  |  |  | not implemented |

### Pulse Shaping, Equalization, MIMO

**Namespace:** `comm.shape.*` — 1 ✅ + 0 ⚠️ / 8 = 13%

System-Object equalisers (`comm.LinearEqualizer`, `comm.MLSEEqualizer`,
`comm.DecisionFeedbackEqualizer`) are omitted; only the function-form
MLSE entry is exposed.

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `gaussdesign` |  |  |  |  |  |  |  |
| `rcosdesign` |  |  |  |  |  |  |  |
| `rectpulse` |  |  |  |  |  |  |  |
| `intdump` |  |  |  |  |  |  |  |
| `mlseeq` |  |  |  |  |  |  | not implemented |
| `ofdmEqualize` |  |  |  |  |  |  | not implemented |
| `blkdiagbfweights` |  |  |  |  |  |  | not implemented |
| `ofdmPrecode` |  |  |  |  |  |  | not implemented |

### RF and Channel Impairments

**Namespace:** `comm.rf.*` — 4 ✅ + 0 ⚠️ / 10 = 40%

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `awgn` |  |  |  |  |  |  |  |
| `bsc` |  |  |  |  |  |  |  |
| `rayleighchan` |  |  |  |  |  |  |  |
| `ricianchan` |  |  |  |  |  |  |  |
| `stdchan` |  |  |  |  |  |  | not implemented |
| `frequencyOffset` |  |  |  |  |  |  | not implemented |
| `iqimbal` |  |  |  |  |  |  | not implemented |
| `iqcoef2imbal` |  |  |  |  |  |  | not implemented |
| `iqimbal2coef` |  |  |  |  |  |  | not implemented |
| `srmdelay` |  |  |  |  |  |  | not implemented |
| `channelDelay` |  |  |  |  |  |  | not implemented |
| `ofdmChannelResponse` |  |  |  |  |  |  | not implemented |

### Propagation Path Loss and Geometry

**Namespace:** `comm.propagation.*` — 0 ✅ + 0 ⚠️ / 15 = 0%

OOP `propagationModel` family, ray-tracing classes (`raytrace`,
`coverage`, `pattern`, `sinr`, `link`, `sigstrength`) and the antenna /
basemap object hierarchy intentionally omitted — only flat scalar /
vector path-loss models and coordinate transforms.

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `fspl` |  |  |  |  |  |  | not implemented |
| `cranerainpl` |  |  |  |  |  |  | not implemented |
| `rainpl` |  |  |  |  |  |  | not implemented |
| `gaspl` |  |  |  |  |  |  | not implemented |
| `fogpl` |  |  |  |  |  |  | not implemented |
| `raypl` |  |  |  |  |  |  | not implemented |
| `buildingMaterialPermittivity` |  |  |  |  |  |  | not implemented |
| `earthSurfacePermittivity` |  |  |  |  |  |  | not implemented |
| `los` |  |  |  |  |  |  | not implemented |
| `doppler` |  |  |  |  |  |  | not implemented |
| `rangeangle` |  |  |  |  |  |  | not implemented |
| `global2localcoord` |  |  |  |  |  |  | not implemented |
| `local2globalcoord` |  |  |  |  |  |  | not implemented |
| `cart2sphvec` |  |  |  |  |  |  | not implemented |
| `sph2cartvec` |  |  |  |  |  |  | not implemented |

### Performance Analysis

**Namespace:** `comm.perf.*` — 6 ✅ + 0 ⚠️ / 11 = 55%

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `berawgn` |  |  |  |  |  |  |  |
| `bercoding` |  |  |  |  |  |  | not implemented |
| `berconfint` |  |  |  |  |  |  |  |
| `berfading` |  |  |  |  |  |  | not implemented |
| `berfit` |  |  |  |  |  |  | not implemented |
| `bersync` |  |  |  |  |  |  | not implemented |
| `semianalytic` |  |  |  |  |  |  | not implemented |
| `marcumq` |  |  |  |  |  |  |  |
| `qfunc` |  |  |  |  |  |  |  |
| `qfuncinv` |  |  |  |  |  |  |  |
| `noisebw` |  |  |  |  |  |  |  |

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

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `tf` |  |  |  |  |  |  |  |
| `zpk` |  |  |  |  |  |  |  |
| `ss` |  |  |  |  |  |  |  |
| `frd` |  |  |  |  |  |  |  |
| `dss` |  |  |  |  |  |  | not implemented |
| `filt` |  |  |  |  |  |  |  |
| `pid` |  |  |  |  |  |  | not implemented |
| `pid2` |  |  |  |  |  |  | not implemented |
| `pidstd` |  |  |  |  |  |  | not implemented |
| `pidstd2` |  |  |  |  |  |  | not implemented |
| `rss` |  |  |  |  |  |  | not implemented |
| `drss` |  |  |  |  |  |  | not implemented |
| `tfdata` |  |  |  |  |  |  |  |
| `zpkdata` |  |  |  |  |  |  |  |
| `ssdata` |  |  |  |  |  |  |  |
| `frdata` |  |  |  |  |  |  |  |
| `dssdata` |  |  |  |  |  |  | not implemented |
| `piddata` |  |  |  |  |  |  | not implemented |
| `pidstddata` |  |  |  |  |  |  | not implemented |

### Model Properties

**Namespace:** `control.props.*` — 11 ✅ + 0 ⚠️ / 11 = **100%**

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `isct` |  |  |  |  |  |  |  |
| `isdt` |  |  |  |  |  |  |  |
| `isproper` |  |  |  |  |  |  |  |
| `issiso` |  |  |  |  |  |  |  |
| `isstable` |  |  |  |  |  |  |  |
| `isstatic` |  |  |  |  |  |  |  |
| `order` |  |  |  |  |  |  |  |
| `pole` |  |  |  |  |  |  |  |
| `zero` |  |  |  |  |  |  |  |
| `tzero` |  |  |  |  |  |  |  |
| `damp` |  |  |  |  |  |  |  |

### Model Conversion & Reduction

**Namespace:** `control.convert.*` — 3 ✅ + 0 ⚠️ / 18 = 17%

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `c2d` |  |  |  |  |  |  |  |
| `c2dOptions` |  |  |  |  |  |  | not implemented |
| `d2c` |  |  |  |  |  |  |  |
| `d2cOptions` |  |  |  |  |  |  | not implemented |
| `d2d` |  |  |  |  |  |  | not implemented |
| `d2dOptions` |  |  |  |  |  |  | not implemented |
| `ss2ss` |  |  |  |  |  |  |  |
| `canon` |  |  |  |  |  |  | not implemented |
| `balreal` |  |  |  |  |  |  | not implemented |
| `prescale` |  |  |  |  |  |  | not implemented |
| `modalreal` |  |  |  |  |  |  | not implemented |
| `compreal` |  |  |  |  |  |  | not implemented |
| `minreal` |  |  |  |  |  |  | not implemented |
| `sminreal` |  |  |  |  |  |  | not implemented |
| `balred` |  |  |  |  |  |  | not implemented |
| `modred` |  |  |  |  |  |  | not implemented |
| `hsvd` |  |  |  |  |  |  | not implemented |
| `pade` |  |  |  |  |  |  | not implemented |
| `ss2tf` |  |  |  |  |  |  |  |

### Interconnections

**Namespace:** `control.connect.*` — 3 ✅ + 0 ⚠️ / 7 = 43%

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `feedback` |  |  |  |  |  |  |  |
| `series` |  |  |  |  |  |  |  |
| `parallel` |  |  |  |  |  |  |  |
| `connect` |  |  |  |  |  |  | not implemented |
| `append` |  |  |  |  |  |  |  |
| `lft` |  |  |  |  |  |  | not implemented |
| `sumblk` |  |  |  |  |  |  | not implemented |

### Time and Frequency Response

**Namespace:** `control.response.*` — 9 ✅ + 0 ⚠️ / 19 = 47%

`*plot` variants intentionally dropped — they're display-only mirrors
of the numeric functions (which already return data when called with
output args).

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `step` |  |  |  |  |  |  |  |
| `stepinfo` |  |  |  |  |  |  |  |
| `impulse` |  |  |  |  |  |  |  |
| `initial` |  |  |  |  |  |  | not implemented |
| `lsim` |  |  |  |  |  |  |  |
| `lsiminfo` |  |  |  |  |  |  | not implemented |
| `gensig` |  |  |  |  |  |  | not implemented |
| `covar` |  |  |  |  |  |  | not implemented |
| `bode` |  |  |  |  |  |  |  |
| `bodemag` |  |  |  |  |  |  | not implemented |
| `nyquist` |  |  |  |  |  |  |  |
| `nichols` |  |  |  |  |  |  | not implemented |
| `sigma` |  |  |  |  |  |  | not implemented |
| `freqresp` |  |  |  |  |  |  |  |
| `evalfr` |  |  |  |  |  |  |  |
| `dcgain` |  |  |  |  |  |  |  |
| `bandwidth` |  |  |  |  |  |  |  |
| `getPeakGain` |  |  |  |  |  |  | not implemented |
| `getGainCrossover` |  |  |  |  |  |  | not implemented |

### Stability and Margins

**Namespace:** `control.margin.*` — 3 ✅ + 0 ⚠️ / 6 = 50%

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `margin` |  |  |  |  |  |  |  |
| `allmargin` |  |  |  |  |  |  | not implemented |
| `db2mag` |  |  |  |  |  |  |  |
| `mag2db` |  |  |  |  |  |  |  |
| `pzmap` |  |  |  |  |  |  |  |
| `rlocus` |  |  |  |  |  |  |  |

### State-Space Design and Estimation

**Namespace:** `control.design.*` — 4 ✅ + 0 ⚠️ / 18 = 22%

OOP filters (`extendedKalmanFilter`, `unscentedKalmanFilter`,
`particleFilter`) intentionally omitted — they're class-objects with
methods (`correct`, `predict`, etc.). Flat steady-state designs only.

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `lqr` |  |  |  |  |  |  | not implemented |
| `lqry` |  |  |  |  |  |  | not implemented |
| `lqi` |  |  |  |  |  |  | not implemented |
| `dlqr` |  |  |  |  |  |  | not implemented |
| `lqrd` |  |  |  |  |  |  | not implemented |
| `lqg` |  |  |  |  |  |  | not implemented |
| `lqgreg` |  |  |  |  |  |  | not implemented |
| `lqgtrack` |  |  |  |  |  |  | not implemented |
| `place` |  |  |  |  |  |  |  |
| `estim` |  |  |  |  |  |  | not implemented |
| `kalman` |  |  |  |  |  |  | not implemented |
| `kalmd` |  |  |  |  |  |  | not implemented |
| `reg` |  |  |  |  |  |  | not implemented |
| `ctrb` |  |  |  |  |  |  |  |
| `obsv` |  |  |  |  |  |  |  |
| `gram` |  |  |  |  |  |  | not implemented |
| `ctrbf` |  |  |  |  |  |  | not implemented |
| `obsvf` |  |  |  |  |  |  | not implemented |

### Matrix Equations

**Namespace:** `control.matrixeq.*` — 2 ✅ + 0 ⚠️ / 8 = 25%

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `lyap` |  |  |  |  |  |  |  |
| `lyapchol` |  |  |  |  |  |  | not implemented |
| `dlyap` |  |  |  |  |  |  |  |
| `dlyapchol` |  |  |  |  |  |  | not implemented |
| `care` |  |  |  |  |  |  | not implemented |
| `dare` |  |  |  |  |  |  | not implemented |
| `gcare` |  |  |  |  |  |  | not implemented |
| `gdare` |  |  |  |  |  |  | not implemented |

### PID Tuning and Modal Analysis

**Namespace:** `control.tune.*` — 0 ✅ + 0 ⚠️ / 7 = 0%

`pidTuner`, `looptune`, `systune`, `slTuner` and friends intentionally
omitted — interactive / Simulink / OOP.

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `pidtune` |  |  |  |  |  |  | not implemented |
| `pidtuneOptions` |  |  |  |  |  |  | not implemented |
| `getPIDLoopResponse` |  |  |  |  |  |  | not implemented |
| `modalsep` |  |  |  |  |  |  | not implemented |
| `stabsep` |  |  |  |  |  |  | not implemented |
| `freqsep` |  |  |  |  |  |  | not implemented |
| `spectralfact` |  |  |  |  |  |  | not implemented |

## Fitting

### Splines

**Namespace:** `cfit.splines.*` — 15 ✅ + 0 ⚠️ / 49 = 31%

OOP `fittype`/`fit`/`cfit`/`sfit`/`fitoptions`/`excludedata` and the
GUI tools (`sftool`, `bspligui`, `splinetool`, `getcurve`) intentionally
omitted. Curve Fitting's value for a non-OOP runtime sits in the spline
construction / postprocessing primitives — those are all flat functions.

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `bspline` |  |  |  |  |  |  | not implemented |
| `csape` |  |  |  |  |  |  | not implemented |
| `csapi` |  |  |  |  |  |  |  |
| `csaps` |  |  |  |  |  |  | not implemented |
| `cscvn` |  |  |  |  |  |  | not implemented |
| `rscvn` |  |  |  |  |  |  | not implemented |
| `spapi` |  |  |  |  |  |  | not implemented |
| `spaps` |  |  |  |  |  |  | not implemented |
| `spap2` |  |  |  |  |  |  | not implemented |
| `spcrv` |  |  |  |  |  |  | not implemented |
| `tpaps` |  |  |  |  |  |  | not implemented |
| `ppmak` |  |  |  |  |  |  |  |
| `rpmak` |  |  |  |  |  |  | not implemented |
| `rsmak` |  |  |  |  |  |  | not implemented |
| `spmak` |  |  |  |  |  |  | not implemented |
| `stmak` |  |  |  |  |  |  | not implemented |
| `fn2fm` |  |  |  |  |  |  | not implemented |
| `fnbrk` |  |  |  |  |  |  |  |
| `fnchg` |  |  |  |  |  |  | not implemented |
| `fncmb` |  |  |  |  |  |  |  |
| `fnder` |  |  |  |  |  |  |  |
| `fndir` |  |  |  |  |  |  | not implemented |
| `fnint` |  |  |  |  |  |  |  |
| `fnjmp` |  |  |  |  |  |  | not implemented |
| `fnmin` |  |  |  |  |  |  | not implemented |
| `fnplt` |  |  |  |  |  |  | not implemented |
| `fnrfn` |  |  |  |  |  |  | not implemented |
| `fntlr` |  |  |  |  |  |  | not implemented |
| `fnval` |  |  |  |  |  |  |  |
| `fnxtr` |  |  |  |  |  |  | not implemented |
| `fnzeros` |  |  |  |  |  |  | not implemented |
| `bkbrk` |  |  |  |  |  |  | not implemented |
| `slvblk` |  |  |  |  |  |  | not implemented |
| `spcol` |  |  |  |  |  |  | not implemented |
| `stcol` |  |  |  |  |  |  | not implemented |
| `subplus` |  |  |  |  |  |  |  |
| `aptknt` |  |  |  |  |  |  | not implemented |
| `augknt` |  |  |  |  |  |  |  |
| `aveknt` |  |  |  |  |  |  |  |
| `brk2knt` |  |  |  |  |  |  |  |
| `chbpnt` |  |  |  |  |  |  | not implemented |
| `knt2brk` |  |  |  |  |  |  |  |
| `newknt` |  |  |  |  |  |  | not implemented |
| `optknt` |  |  |  |  |  |  | not implemented |
| `smooth` |  |  |  |  |  |  | not implemented |
| `datastats` |  |  |  |  |  |  |  |
| `prepareCurveData` |  |  |  |  |  |  |  |
| `prepareSurfaceData` |  |  |  |  |  |  |  |
| `quad2d` |  |  |  |  |  |  | not implemented |

## Graphics

### Line Plots

**Namespace:** `graphics.line.*` — 2 ✅ + 0 ⚠️ / 12 = 16%

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `area` |  |  |  |  |  |  | not implemented |
| `errorbar` |  |  |  |  |  |  |  |
| `fimplicit` |  |  |  |  |  |  | not implemented |
| `fplot` |  |  |  |  |  |  |  |
| `fplot3` |  |  |  |  |  |  |  |
| `loglog` |  |  |  |  |  |  |  |
| `plot` |  |  |  |  |  |  |  |
| `plot3` |  |  |  |  |  |  | not implemented |
| `semilogx` |  |  |  |  |  |  |  |
| `semilogy` |  |  |  |  |  |  |  |
| `stackedplot` |  |  |  |  |  |  | not implemented |
| `stairs` |  |  |  |  |  |  |  |

### Polar Plots

**Namespace:** `graphics.polar.*` — 3 ✅ + 0 ⚠️ / 19 = 15%

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `compassplot` |  |  |  |  |  |  | not implemented |
| `fpolarplot` |  |  |  |  |  |  | not implemented |
| `polaraxes` |  |  |  |  |  |  | not implemented |
| `polarbubblechart` |  |  |  |  |  |  | not implemented |
| `polarhistogram` |  |  |  |  |  |  | not implemented |
| `polarplot` |  |  |  |  |  |  |  |
| `polarregion` |  |  |  |  |  |  | not implemented |
| `polarscatter` |  |  |  |  |  |  | not implemented |
| `radiusregion` |  |  |  |  |  |  | not implemented |
| `rlim` |  |  |  |  |  |  |  |
| `rtickangle` |  |  |  |  |  |  | not implemented |
| `rtickformat` |  |  |  |  |  |  | not implemented |
| `rticklabels` |  |  |  |  |  |  | not implemented |
| `rticks` |  |  |  |  |  |  | not implemented |
| `thetalim` |  |  |  |  |  |  |  |
| `thetaregion` |  |  |  |  |  |  | not implemented |
| `thetatickformat` |  |  |  |  |  |  | not implemented |
| `thetaticklabels` |  |  |  |  |  |  | not implemented |
| `thetaticks` |  |  |  |  |  |  | not implemented |

### Contour Plots

**Namespace:** `graphics.contour.*` — 2 ✅ + 0 ⚠️ / 7 = 28%

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `clabel` |  |  |  |  |  |  | not implemented |
| `contour` |  |  |  |  |  |  |  |
| `contour3` |  |  |  |  |  |  | not implemented |
| `contourc` |  |  |  |  |  |  | not implemented |
| `contourf` |  |  |  |  |  |  |  |
| `contourslice` |  |  |  |  |  |  | not implemented |
| `fcontour` |  |  |  |  |  |  |  |

### Vector Fields

**Namespace:** `graphics.vector_fields.*` — 0 ✅ + 0 ⚠️ / 6 = 0%

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `compassplot` |  |  |  |  |  |  | not implemented |
| `feather` |  |  |  |  |  |  | not implemented |
| `quiver` |  |  |  |  |  |  | not implemented |
| `quiver3` |  |  |  |  |  |  | not implemented |
| `streamline` |  |  |  |  |  |  | not implemented |
| `streamslice` |  |  |  |  |  |  | not implemented |

### Surface and Mesh Plots

**Namespace:** `graphics.surface.*` — 3 ✅ + 0 ⚠️ / 21 = 14%

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `contour3` |  |  |  |  |  |  | not implemented |
| `cylinder` |  |  |  |  |  |  |  |
| `ellipsoid` |  |  |  |  |  |  |  |
| `fimplicit3` |  |  |  |  |  |  | not implemented |
| `fmesh` |  |  |  |  |  |  |  |
| `fsurf` |  |  |  |  |  |  |  |
| `hidden` |  |  |  |  |  |  | not implemented |
| `mesh` |  |  |  |  |  |  |  |
| `meshc` |  |  |  |  |  |  | not implemented |
| `meshz` |  |  |  |  |  |  | not implemented |
| `pcolor` |  |  |  |  |  |  |  |
| `peaks` |  |  |  |  |  |  |  |
| `ribbon` |  |  |  |  |  |  | not implemented |
| `sphere` |  |  |  |  |  |  |  |
| `surf` |  |  |  |  |  |  |  |
| `surf2patch` |  |  |  |  |  |  | not implemented |
| `surface` |  |  |  |  |  |  | not implemented |
| `surfc` |  |  |  |  |  |  | not implemented |
| `surfl` |  |  |  |  |  |  | not implemented |
| `surfnorm` |  |  |  |  |  |  | not implemented |
| `waterfall` |  |  |  |  |  |  | not implemented |

### Volume Visualization

**Namespace:** `graphics.volume.*` — 0 ✅ + 0 ⚠️ / 24 = 0%

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `coneplot` |  |  |  |  |  |  |  |
| `contourslice` |  |  |  |  |  |  | not implemented |
| `curl` |  |  |  |  |  |  | not implemented |
| `divergence` |  |  |  |  |  |  | not implemented |
| `flow` |  |  |  |  |  |  | not implemented |
| `interpstreamspeed` |  |  |  |  |  |  | not implemented |
| `isocaps` |  |  |  |  |  |  | not implemented |
| `isocolors` |  |  |  |  |  |  | not implemented |
| `isonormals` |  |  |  |  |  |  | not implemented |
| `isosurface` |  |  |  |  |  |  | not implemented |
| `reducepatch` |  |  |  |  |  |  | not implemented |
| `reducevolume` |  |  |  |  |  |  | not implemented |
| `shrinkfaces` |  |  |  |  |  |  | not implemented |
| `slice` |  |  |  |  |  |  | not implemented |
| `smooth3` |  |  |  |  |  |  | not implemented |
| `stream2` |  |  |  |  |  |  | not implemented |
| `stream3` |  |  |  |  |  |  | not implemented |
| `streamline` |  |  |  |  |  |  | not implemented |
| `streamparticles` |  |  |  |  |  |  | not implemented |
| `streamribbon` |  |  |  |  |  |  | not implemented |
| `streamslice` |  |  |  |  |  |  | not implemented |
| `streamtube` |  |  |  |  |  |  | not implemented |
| `subvolume` |  |  |  |  |  |  | not implemented |
| `volumebounds` |  |  |  |  |  |  | not implemented |

### Geographic Plots

**Namespace:** `graphics.geographic.*` — 0 ✅ + 0 ⚠️ / 8 = 0%

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `geoaxes` |  |  |  |  |  |  | not implemented |
| `geobasemap` |  |  |  |  |  |  | not implemented |
| `geobubble` |  |  |  |  |  |  | not implemented |
| `geodensityplot` |  |  |  |  |  |  | not implemented |
| `geolimits` |  |  |  |  |  |  | not implemented |
| `geoplot` |  |  |  |  |  |  | not implemented |
| `geoscatter` |  |  |  |  |  |  | not implemented |
| `geotickformat` |  |  |  |  |  |  | not implemented |

## Image

### Image I/O

**Namespace:** `image.io.*` — 3 ✅ + 0 ⚠️ / 3 = **100%**

Backed by `stb_image` / `stb_image_write` (single-header, public-domain) vendored under `third_party/stb/`.

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `imread` |  |  |  |  |  |  |  |
| `imwrite` |  |  |  |  |  |  |  |
| `imfinfo` |  |  |  |  |  |  |  |

### Image Type Conversion

**Namespace:** `image.type.*` — 13 ✅ + 0 ⚠️ / 27 = 48%

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `adaptthresh` |  |  |  |  |  |  |  |
| `cmap2gray` |  |  |  |  |  |  |  |
| `cmunique` |  |  |  |  |  |  |  |
| `getrangefromclass` |  |  |  |  |  |  |  |
| `gray2ind` |  |  |  |  |  |  |  |
| `graythresh` |  |  |  |  |  |  |  |
| `grayslice` |  |  |  |  |  |  |  |
| `im2bw` |  |  |  |  |  |  |  |
| `im2double` |  |  |  |  |  |  |  |
| `im2gray` |  |  |  |  |  |  |  |
| `im2int16` |  |  |  |  |  |  |  |
| `im2single` |  |  |  |  |  |  |  |
| `im2uint16` |  |  |  |  |  |  |  |
| `im2uint8` |  |  |  |  |  |  |  |
| `imbinarize` | 0.08276 | 2.13× |  | 7.858 | 0.09× |  | grayscale double N×N (100×100 / 1000×1000) |
| `imquantize` |  |  |  |  |  |  |  |
| `imsplit` |  |  |  |  |  |  |  |
| `ind2gray` |  |  |  |  |  |  |  |
| `ind2rgb` |  |  |  |  |  |  |  |
| `iptnum2ordinal` |  |  |  |  |  |  |  |
| `label2rgb` |  |  |  |  |  |  |  |
| `mat2gray` | 0.04461 | 2.75× |  | 6.088 | 0.28× |  | grayscale double N×N (100×100 / 1000×1000) |
| `multithresh` |  |  |  |  |  |  |  |
| `otsuthresh` |  |  |  |  |  |  |  |
| `rgb2gray` |  |  |  |  |  |  |  |
| `rgb2ind` |  |  |  |  |  |  | not implemented |
| `rgb2lightness` |  |  |  |  |  |  |  |
| `demosaic` |  |  |  |  |  |  |  |

### Color Space Conversion

**Namespace:** `image.color.*` — 10 ✅ + 0 ⚠️ / 30 = 33%

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `chromadapt` |  |  |  |  |  |  |  |
| `colorangle` |  |  |  |  |  |  |  |
| `deltaE` |  |  |  |  |  |  |  |
| `hsv2rgb` |  |  |  |  |  |  |  |
| `illumgray` |  |  |  |  |  |  |  |
| `illumpca` |  |  |  |  |  |  |  |
| `illumwhite` |  |  |  |  |  |  |  |
| `imapprox` |  |  |  |  |  |  | not implemented |
| `imcolordiff` |  |  |  |  |  |  |  |
| `lab2double` |  |  |  |  |  |  |  |
| `lab2rgb` |  |  |  |  |  |  |  |
| `lab2uint16` |  |  |  |  |  |  |  |
| `lab2uint8` |  |  |  |  |  |  |  |
| `lab2xyz` |  |  |  |  |  |  |  |
| `lin2rgb` |  |  |  |  |  |  |  |
| `ntsc2rgb` |  |  |  |  |  |  |  |
| `rgb2hsv` |  |  |  |  |  |  |  |
| `rgb2lab` |  |  |  |  |  |  |  |
| `rgb2lin` |  |  |  |  |  |  |  |
| `rgb2ntsc` |  |  |  |  |  |  |  |
| `rgb2xyz` |  |  |  |  |  |  |  |
| `rgb2ycbcr` |  |  |  |  |  |  |  |
| `rgbwide2xyz` |  |  |  |  |  |  |  |
| `rgbwide2ycbcr` |  |  |  |  |  |  |  |
| `whitepoint` |  |  |  |  |  |  |  |
| `xyz2double` |  |  |  |  |  |  |  |
| `xyz2lab` |  |  |  |  |  |  |  |
| `xyz2rgb` |  |  |  |  |  |  |  |
| `xyz2rgbwide` |  |  |  |  |  |  |  |
| `xyz2uint16` |  |  |  |  |  |  |  |
| `ycbcr2rgb` |  |  |  |  |  |  |  |
| `ycbcr2rgbwide` |  |  |  |  |  |  |  |

### Synthetic Images and Display

**Namespace:** `image.synth.*` / `image.display.*` — 0 ✅ + 0 ⚠️ / 8 = 0%

Display ones (`imshow`, `montage`, …) need graphics; synthesis is pure algorithm.

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `checkerboard` |  |  |  |  |  |  |  |
| `imnoise` |  |  |  |  |  |  |  |
| `phantom` |  |  |  |  |  |  |  |
| `imshow` |  |  |  |  |  |  | not implemented |
| `imfuse` |  |  |  |  |  |  |  |
| `imshowpair` |  |  |  |  |  |  | not implemented |
| `montage` |  |  |  |  |  |  | not implemented |
| `immovie` |  |  |  |  |  |  | not implemented |

### Geometric Transformations

**Namespace:** `image.geom.*` — 4 ✅ + 0 ⚠️ / 13 = 31%

Class-based affine/rigid/projective transforms (affinetform2d etc.) intentionally omitted; flat function APIs only.

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `findbounds` |  |  |  |  |  |  | not implemented |
| `fitgeotrans` |  |  |  |  |  |  | not implemented |
| `imcrop` |  |  |  |  |  |  |  |
| `imcrop3` |  |  |  |  |  |  |  |
| `impyramid` |  |  |  |  |  |  |  |
| `imresize` | 0.04069 | 14.69× |  | 5.497 | 0.37× |  | grayscale double N×N (100×100 / 1000×1000) |
| `imresize3` |  |  |  |  |  |  |  |
| `imrotate` | 0.1137 | 2.37× |  | 17.71 | 0.38× |  | grayscale double N×N (100×100 / 1000×1000) |
| `imrotate3` |  |  |  |  |  |  |  |
| `imtransform` |  |  |  |  |  |  | not implemented |
| `imtranslate` |  |  |  |  |  |  |  |
| `imwarp` |  |  |  |  |  |  | not implemented |
| `makeresampler` |  |  |  |  |  |  | not implemented |

### Image Registration

**Namespace:** `image.register.*` — 0 ✅ + 0 ⚠️ / 8 = 0%

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `cpcorr` |  |  |  |  |  |  | not implemented |
| `imregconfig` |  |  |  |  |  |  | not implemented |
| `imregcorr` |  |  |  |  |  |  | not implemented |
| `imregdemons` |  |  |  |  |  |  | not implemented |
| `imregister` |  |  |  |  |  |  | not implemented |
| `imregmtb` |  |  |  |  |  |  | not implemented |
| `imregtform` |  |  |  |  |  |  | not implemented |
| `normxcorr2` |  |  |  |  |  |  |  |

### Image Filtering

**Namespace:** `image.filter.*` — 10 ✅ + 0 ⚠️ / 36 = 28%

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `convmtx2` |  |  |  |  |  |  |  |
| `entropyfilt` |  |  |  |  |  |  |  |
| `fibermetric` |  |  |  |  |  |  |  |
| `freqspace` |  |  |  |  |  |  |  |
| `freqz2` |  |  |  |  |  |  |  |
| `fsamp2` |  |  |  |  |  |  |  |
| `fspecial` |  |  |  |  |  |  |  |
| `fspecial3` |  |  |  |  |  |  |  |
| `ftrans2` |  |  |  |  |  |  |  |
| `fwind1` |  |  |  |  |  |  |  |
| `fwind2` |  |  |  |  |  |  |  |
| `gabor` |  |  |  |  |  |  | not implemented |
| `imbilatfilt` |  |  |  |  |  |  |  |
| `imboxfilt` | 0.05437 | 3.62× |  | 8.678 | 0.17× |  | grayscale double N×N (100×100 / 1000×1000) |
| `imboxfilt3` |  |  |  |  |  |  |  |
| `imdiffusefilt` |  |  |  |  |  |  |  |
| `imfilter` | 0.02674 | 1.68× |  | 3.856 | 0.33× |  | grayscale double N×N (100×100 / 1000×1000) |
| `imgaborfilt` |  |  |  |  |  |  |  |
| `imgaussfilt` | 0.165 | 2.83× |  | 8.024 | 0.75× |  | grayscale double N×N (100×100 / 1000×1000) |
| `imgaussfilt3` |  |  |  |  |  |  |  |
| `imguidedfilter` |  |  |  |  |  |  |  |
| `imnlmfilt` |  |  |  |  |  |  |  |
| `integralBoxFilter` |  |  |  |  |  |  |  |
| `integralBoxFilter3` |  |  |  |  |  |  |  |
| `integralImage` |  |  |  |  |  |  |  |
| `integralImage3` |  |  |  |  |  |  |  |
| `medfilt2` | 0.2615 | 2.09× |  | 29.26 | 0.13× |  | grayscale double N×N (100×100 / 1000×1000) |
| `medfilt3` |  |  |  |  |  |  |  |
| `modefilt` |  |  |  |  |  |  |  |
| `nlfilter` |  |  |  |  |  |  |  |
| `ordfilt2` |  |  |  |  |  |  |  |
| `padarray` |  |  |  |  |  |  |  |
| `rangefilt` |  |  |  |  |  |  |  |
| `roifilt2` |  |  |  |  |  |  |  |
| `stdfilt` | 0.1428 | 2.10× |  | 10.94 | 0.52× |  | grayscale double N×N (100×100 / 1000×1000) |
| `wiener2` |  |  |  |  |  |  |  |

### Contrast Adjustment

**Namespace:** `image.contrast.*` — 3 ✅ + 0 ⚠️ / 14 = 21%

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `adapthisteq` |  |  |  |  |  |  |  |
| `decorrstretch` |  |  |  |  |  |  | not implemented |
| `histeq` |  |  |  |  |  |  |  |
| `imadjust` | 0.1727 | 3.80× |  | 14.67 | 0.55× |  | grayscale double N×N (100×100 / 1000×1000) |
| `imadjustn` |  |  |  |  |  |  |  |
| `imflatfield` |  |  |  |  |  |  |  |
| `imhistmatch` |  |  |  |  |  |  |  |
| `imhistmatchn` |  |  |  |  |  |  |  |
| `imlocalbrighten` |  |  |  |  |  |  | not implemented |
| `imreducehaze` |  |  |  |  |  |  |  |
| `imsharpen` |  |  |  |  |  |  |  |
| `intlut` |  |  |  |  |  |  |  |
| `localcontrast` |  |  |  |  |  |  | not implemented |
| `locallapfilt` |  |  |  |  |  |  |  |
| `stretchlim` |  |  |  |  |  |  |  |

### ROI-Based Processing

**Namespace:** `image.roi.*` — 5 ✅ + 0 ⚠️ / 8 = 63%

ROI drawing classes (`Circle`, `Ellipse`, `drawcircle`, `imellipse`, `imrect`, …) intentionally omitted as OOP / interactive.

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `inpaintCoherent` |  |  |  |  |  |  | not implemented |
| `inpaintExemplar` |  |  |  |  |  |  | not implemented |
| `poly2mask` |  |  |  |  |  |  |  |
| `reducepoly` |  |  |  |  |  |  |  |
| `regionfill` |  |  |  |  |  |  |  |
| `roicolor` |  |  |  |  |  |  |  |
| `roifill` |  |  |  |  |  |  | not implemented |
| `roipoly` |  |  |  |  |  |  |  |

### Morphological Operations

**Namespace:** `image.morph.*` — 8 ✅ + 0 ⚠️ / 27 = 30%

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `applylut` |  |  |  |  |  |  |  |
| `bwhitmiss` |  |  |  |  |  |  |  |
| `bwlookup` |  |  |  |  |  |  |  |
| `bwmorph` |  |  |  |  |  |  |  |
| `bwmorph3` |  |  |  |  |  |  |  |
| `bwpack` |  |  |  |  |  |  |  |
| `bwperim` |  |  |  |  |  |  |  |
| `bwskel` |  |  |  |  |  |  | not implemented |
| `bwulterode` |  |  |  |  |  |  | not implemented |
| `bwunpack` |  |  |  |  |  |  | not implemented |
| `conndef` |  |  |  |  |  |  | not implemented |
| `imbothat` |  |  |  |  |  |  |  |
| `imclearborder` |  |  |  |  |  |  |  |
| `imclose` |  |  |  |  |  |  |  |
| `imdilate` |  |  |  |  |  |  |  |
| `imerode` |  |  |  |  |  |  |  |
| `imextendedmax` |  |  |  |  |  |  |  |
| `imextendedmin` |  |  |  |  |  |  |  |
| `imfill` |  |  |  |  |  |  |  |
| `imhmax` |  |  |  |  |  |  |  |
| `imhmin` |  |  |  |  |  |  |  |
| `imimposemin` |  |  |  |  |  |  |  |
| `imkeepborder` |  |  |  |  |  |  |  |
| `imopen` |  |  |  |  |  |  |  |
| `imreconstruct` |  |  |  |  |  |  |  |
| `imregionalmax` |  |  |  |  |  |  |  |
| `imregionalmin` |  |  |  |  |  |  |  |
| `imtophat` |  |  |  |  |  |  |  |
| `makelut` |  |  |  |  |  |  |  |
| `offsetstrel` |  |  |  |  |  |  | not implemented |
| `strel` |  |  |  |  |  |  |  |

### Deblurring

**Namespace:** `image.deblur.*` — 0 ✅ + 0 ⚠️ / 7 = 0%

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `deconvblind` |  |  |  |  |  |  | not implemented |
| `deconvlucy` |  |  |  |  |  |  | not implemented |
| `deconvreg` |  |  |  |  |  |  |  |
| `deconvwnr` |  |  |  |  |  |  |  |
| `edgetaper` |  |  |  |  |  |  |  |
| `otf2psf` |  |  |  |  |  |  |  |
| `psf2otf` |  |  |  |  |  |  |  |

### Neighborhood and Block Processing

**Namespace:** `image.block.*` — 0 ✅ + 0 ⚠️ / 6 = 0%

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `bestblk` |  |  |  |  |  |  |  |
| `blockproc` |  |  |  |  |  |  | not implemented |
| `col2im` |  |  |  |  |  |  |  |
| `colfilt` |  |  |  |  |  |  |  |
| `im2col` |  |  |  |  |  |  |  |
| `nlfilter` |  |  |  |  |  |  |  |

### Image Arithmetic

**Namespace:** `image.arith.*` — 8 ✅ + 0 ⚠️ / 8 = 100%

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `imabsdiff` |  |  |  |  |  |  |  |
| `imadd` |  |  |  |  |  |  |  |
| `imapplymatrix` |  |  |  |  |  |  |  |
| `imcomplement` | 0.02059 | 0.69× |  | 3.629 | 0.31× |  | grayscale double N×N (100×100 / 1000×1000) |
| `imdivide` |  |  |  |  |  |  |  |
| `imlincomb` |  |  |  |  |  |  |  |
| `immultiply` |  |  |  |  |  |  |  |
| `imsubtract` |  |  |  |  |  |  |  |

### Image Segmentation

**Namespace:** `image.segment.*` — 6 ✅ + 0 ⚠️ / 22 = 27%

Deep-learning-based ones (`imsegsam`, `segmentAnythingModel`, …) intentionally omitted.

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `activecontour` |  |  |  |  |  |  | not implemented |
| `bfscore` |  |  |  |  |  |  | not implemented |
| `boundarymask` |  |  |  |  |  |  |  |
| `dice` |  |  |  |  |  |  |  |
| `gradientweight` |  |  |  |  |  |  |  |
| `grabcut` |  |  |  |  |  |  | not implemented |
| `grayconnected` |  |  |  |  |  |  |  |
| `graydiffweight` |  |  |  |  |  |  |  |
| `imoverlay` |  |  |  |  |  |  |  |
| `imseggeodesic` |  |  |  |  |  |  | not implemented |
| `imsegfmm` |  |  |  |  |  |  | not implemented |
| `imsegisodata` |  |  |  |  |  |  | not implemented |
| `imsegkmeans` |  |  |  |  |  |  | not implemented |
| `imsegkmeans3` |  |  |  |  |  |  | not implemented |
| `jaccard` |  |  |  |  |  |  |  |
| `label2idx` |  |  |  |  |  |  |  |
| `labeloverlay` |  |  |  |  |  |  |  |
| `lazysnapping` |  |  |  |  |  |  | not implemented |
| `superpixels` |  |  |  |  |  |  | not implemented |
| `superpixels3` |  |  |  |  |  |  | not implemented |
| `watershed` |  |  |  |  |  |  | not implemented |

### Object Analysis

**Namespace:** `image.object.*` — 4 ✅ + 0 ⚠️ / 18 = 22%

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `bwboundaries` |  |  |  |  |  |  |  |
| `bwtraceboundary` |  |  |  |  |  |  |  |
| `circles2mask` |  |  |  |  |  |  | not implemented |
| `corner` |  |  |  |  |  |  | not implemented |
| `cornermetric` |  |  |  |  |  |  |  |
| `edge` |  |  |  |  |  |  |  |
| `edge3` |  |  |  |  |  |  | not implemented |
| `hough` |  |  |  |  |  |  |  |
| `houghlines` |  |  |  |  |  |  |  |
| `houghpeaks` |  |  |  |  |  |  | not implemented |
| `imfindcircles` |  |  |  |  |  |  | not implemented |
| `imgradient` | 0.2161 | 1.56× |  | 15.89 | 0.24× |  | grayscale double N×N (100×100 / 1000×1000) |
| `imgradientxy` |  |  |  |  |  |  |  |
| `imgradient3` |  |  |  |  |  |  |  |
| `imgradientxyz` |  |  |  |  |  |  |  |
| `iradon` |  |  |  |  |  |  | not implemented |
| `qtdecomp` |  |  |  |  |  |  | not implemented |
| `qtgetblk` |  |  |  |  |  |  | not implemented |
| `qtsetblk` |  |  |  |  |  |  | not implemented |
| `radon` |  |  |  |  |  |  | not implemented |
| `visboundaries` |  |  |  |  |  |  | not implemented |
| `viscircles` |  |  |  |  |  |  | not implemented |

### Region and Image Properties

**Namespace:** `image.region.*` — 8 ✅ + 0 ⚠️ / 28 = 29%

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `bwarea` |  |  |  |  |  |  |  |
| `bwareafilt` |  |  |  |  |  |  |  |
| `bwareaopen` |  |  |  |  |  |  |  |
| `bwconncomp` |  |  |  |  |  |  |  |
| `bwconvhull` |  |  |  |  |  |  | not implemented |
| `bwdist` |  |  |  |  |  |  |  |
| `bwdistgeodesic` |  |  |  |  |  |  |  |
| `bweuler` |  |  |  |  |  |  |  |
| `bwferet` |  |  |  |  |  |  | not implemented |
| `bwlabel` |  |  |  |  |  |  |  |
| `bwlabeln` |  |  |  |  |  |  | not implemented |
| `bwperim` |  |  |  |  |  |  |  |
| `bwpropfilt` |  |  |  |  |  |  |  |
| `bwselect` |  |  |  |  |  |  |  |
| `bwselect3` |  |  |  |  |  |  | not implemented |
| `cc2bw` |  |  |  |  |  |  |  |
| `corr2` |  |  |  |  |  |  |  |
| `graydist` |  |  |  |  |  |  |  |
| `imcontour` |  |  |  |  |  |  | not implemented |
| `imhist` |  |  |  |  |  |  |  |
| `impixel` |  |  |  |  |  |  | not implemented |
| `improfile` |  |  |  |  |  |  | not implemented |
| `labelmatrix` |  |  |  |  |  |  |  |
| `mean2` |  |  |  |  |  |  |  |
| `poly2label` |  |  |  |  |  |  | not implemented |
| `regionprops` |  |  |  |  |  |  |  |
| `regionprops3` |  |  |  |  |  |  | not implemented |
| `std2` |  |  |  |  |  |  |  |

### Texture Analysis

**Namespace:** `image.texture.*` — 0 ✅ + 0 ⚠️ / 6 = 0%

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `entropy` |  |  |  |  |  |  |  |
| `entropyfilt` |  |  |  |  |  |  |  |
| `graycomatrix` |  |  |  |  |  |  |  |
| `graycoprops` |  |  |  |  |  |  |  |
| `rangefilt` |  |  |  |  |  |  |  |
| `stdfilt` | 0.1428 | 2.10× |  | 10.94 | 0.52× |  | grayscale double N×N (100×100 / 1000×1000) |

### Image Quality

**Namespace:** `image.quality.*` — 3 ✅ + 0 ⚠️ / 8 = 38%

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `brisque` |  |  |  |  |  |  | not implemented |
| `immse` |  |  |  |  |  |  |  |
| `multissim` |  |  |  |  |  |  |  |
| `multissim3` |  |  |  |  |  |  |  |
| `niqe` |  |  |  |  |  |  | not implemented |
| `piqe` |  |  |  |  |  |  | not implemented |
| `psnr` |  |  |  |  |  |  |  |
| `ssim` |  |  |  |  |  |  |  |

### Image Transforms

**Namespace:** `image.transform.*` — 7 ✅ + 0 ⚠️ / 11 = 64%

`fft2` / `ifft2` / `fftshift` / `ifftshift` already covered under Signal / Transforms; cross-listed here per MATLAB TOC.

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `dct2` |  |  |  |  |  |  |  |
| `dctmtx` |  |  |  |  |  |  |  |
| `fan2para` |  |  |  |  |  |  | not implemented |
| `fanbeam` |  |  |  |  |  |  | not implemented |
| `fft2` |  |  |  |  |  |  |  |
| `fftshift` |  |  |  |  |  |  |  |
| `idct2` |  |  |  |  |  |  |  |
| `ifanbeam` |  |  |  |  |  |  | not implemented |
| `ifft2` |  |  |  |  |  |  |  |
| `ifftshift` |  |  |  |  |  |  |  |
| `para2fan` |  |  |  |  |  |  | not implemented |

## IO

### Low-Level File I/O

**Namespace:** `io.file_io.*` — 13 ✅ + 0 ⚠️ / 15 = 86%

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `fclose` |  |  |  |  |  |  |  |
| `feof` |  |  |  |  |  |  |  |
| `ferror` |  |  |  |  |  |  |  |
| `fgetl` |  |  |  |  |  |  |  |
| `fgets` |  |  |  |  |  |  |  |
| `fileread` |  |  |  |  |  |  |  |
| `fopen` |  |  |  |  |  |  |  |
| `fprintf` |  |  |  |  |  |  |  |
| `fread` |  |  |  |  |  |  |  |
| `frewind` |  |  |  |  |  |  |  |
| `fscanf` |  |  |  |  |  |  |  |
| `fseek` |  |  |  |  |  |  |  |
| `ftell` |  |  |  |  |  |  |  |
| `fwrite` |  |  |  |  |  |  |  |
| `openedfiles` |  |  |  |  |  |  | not implemented |

### Text Files (CSV / dlm / readtable)

**Namespace:** `io.text.*`. Exception: `readtable/writetable/readtimetable/writetimetable` → `table.*` (future) — 1 ✅ + 0 ⚠️ / 16 = 6%

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `fileread` |  |  |  |  |  |  |  |
| `importdatatask` |  |  |  |  |  |  | not implemented |
| `importtool` |  |  |  |  |  |  | not implemented |
| `readcell` |  |  |  |  |  |  | not implemented |
| `readlines` |  |  |  |  |  |  |  |
| `readmatrix` |  |  |  |  |  |  |  |
| `readtable` |  |  |  |  |  |  | not implemented |
| `readtimetable` |  |  |  |  |  |  | not implemented |
| `readvars` |  |  |  |  |  |  | not implemented |
| `textscan` |  |  |  |  |  |  |  |
| `type` |  |  |  |  |  |  |  |
| `writecell` |  |  |  |  |  |  | not implemented |
| `writelines` |  |  |  |  |  |  |  |
| `writematrix` |  |  |  |  |  |  |  |
| `writetable` |  |  |  |  |  |  | not implemented |
| `writetimetable` |  |  |  |  |  |  | not implemented |

### Spreadsheets

**Namespace:** `io.text.*`. Table-shaped readers (`readtable`/`writetable`) → `table.*` (future) — 0 ✅ + 0 ⚠️ / 13 = 0%

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `importdata` |  |  |  |  |  |  | not implemented |
| `importdatatask` |  |  |  |  |  |  | not implemented |
| `importtool` |  |  |  |  |  |  | not implemented |
| `readcell` |  |  |  |  |  |  | not implemented |
| `readmatrix` |  |  |  |  |  |  |  |
| `readtable` |  |  |  |  |  |  | not implemented |
| `readtimetable` |  |  |  |  |  |  | not implemented |
| `readvars` |  |  |  |  |  |  | not implemented |
| `sheetnames` |  |  |  |  |  |  | not implemented |
| `writecell` |  |  |  |  |  |  | not implemented |
| `writematrix` |  |  |  |  |  |  |  |
| `writetable` |  |  |  |  |  |  | not implemented |
| `writetimetable` |  |  |  |  |  |  | not implemented |

### Workspace Save / Load

**Namespace:** `io.workspace.*` — 0 ✅ + 0 ⚠️ / 2 = 0%

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `loadobj` |  |  |  |  |  |  | not implemented |
| `saveobj` |  |  |  |  |  |  | not implemented |

### File Name Construction

**Namespace:** `io.paths.*` — 0 ✅ + 0 ⚠️ / 9 = 0%

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `filemarker` |  |  |  |  |  |  | not implemented |
| `fileparts` |  |  |  |  |  |  |  |
| `filesep` |  |  |  |  |  |  |  |
| `fullfile` |  |  |  |  |  |  |  |
| `matlabdrive` |  |  |  |  |  |  | not implemented |
| `matlabroot` |  |  |  |  |  |  | not implemented |
| `tempdir` |  |  |  |  |  |  |  |
| `tempname` |  |  |  |  |  |  |  |
| `toolboxdir` |  |  |  |  |  |  | not implemented |

## Linear Algebra



**Namespace:** `linalg.*` — 12 ✅ + 0 ⚠️ / 82 = 15%

> Library is live (libs/linalg/, 2026-05-25). User-facing surface migrated
> from libs/builtin — see commits `30b06660`..`d71b472c`. Functions still
> marked **deferred — libs/linalg** below are not-yet-implemented (the
> library is the destination, not the blocker). 22 ❌ on this page wait
> on first-time implementation; the per-function migration is complete
> for everything that was previously shipped.

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `balance` |  |  |  |  |  |  | partial |
| `bandwidth` |  |  |  |  |  |  |  |
| `cdf2rdf` |  |  |  |  |  |  |  |
| `chol` |  |  |  |  |  |  |  |
| `cholupdate` |  |  |  |  |  |  |  |
| `cond` |  |  |  |  |  |  |  |
| `condeig` |  |  |  |  |  |  |  |
| `condest` |  |  |  |  |  |  |  |
| `cross` |  |  |  |  |  |  |  |
| `ctranspose` |  |  |  |  |  |  |  |
| `decomposition` |  |  |  |  |  |  | not implemented |
| `det` |  |  |  |  |  |  |  |
| `dot` |  |  |  |  |  |  |  |
| `eig` |  |  |  |  |  |  | partial |
| `eigs` |  |  |  |  |  |  | not implemented |
| `expm` |  |  |  |  |  |  |  |
| `expmv` |  |  |  |  |  |  |  |
| `funm` |  |  |  |  |  |  | not implemented |
| `gsvd` |  |  |  |  |  |  | not implemented |
| `hess` |  |  |  |  |  |  |  |
| `inv` |  |  |  |  |  |  |  |
| `isbanded` |  |  |  |  |  |  |  |
| `isdiag` |  |  |  |  |  |  |  |
| `ishermitian` |  |  |  |  |  |  |  |
| `issymmetric` |  |  |  |  |  |  |  |
| `istril` |  |  |  |  |  |  |  |
| `istriu` |  |  |  |  |  |  |  |
| `kron` |  |  |  |  |  |  |  |
| `ldl` |  |  |  |  |  |  |  |
| `linsolve` |  |  |  |  |  |  |  |
| `logm` |  |  |  |  |  |  | partial |
| `lscov` |  |  |  |  |  |  |  |
| `lsqminnorm` |  |  |  |  |  |  |  |
| `lsqnonneg` |  |  |  |  |  |  |  |
| `lu` |  |  |  |  |  |  |  |
| `mldivide` |  |  |  |  |  |  |  |
| `mpower` |  |  |  |  |  |  |  |
| `mrdivide` |  |  |  |  |  |  |  |
| `mtimes` |  |  |  |  |  |  |  |
| `norm` |  |  |  |  |  |  |  |
| `normest` |  |  |  |  |  |  |  |
| `null` |  |  |  |  |  |  |  |
| `ordeig` |  |  |  |  |  |  |  |
| `ordqz` |  |  |  |  |  |  | not implemented |
| `ordschur` |  |  |  |  |  |  | not implemented |
| `orth` |  |  |  |  |  |  |  |
| `pagectranspose` |  |  |  |  |  |  |  |
| `pageeig` |  |  |  |  |  |  |  |
| `pageinv` |  |  |  |  |  |  |  |
| `pagelsqminnorm` |  |  |  |  |  |  |  |
| `pagemldivide` |  |  |  |  |  |  |  |
| `pagemrdivide` |  |  |  |  |  |  |  |
| `pagemtimes` |  |  |  |  |  |  |  |
| `pagenorm` |  |  |  |  |  |  |  |
| `pagepinv` |  |  |  |  |  |  |  |
| `pagesvd` |  |  |  |  |  |  |  |
| `pagetranspose` |  |  |  |  |  |  |  |
| `pinv` |  |  |  |  |  |  |  |
| `planerot` |  |  |  |  |  |  |  |
| `polyeig` |  |  |  |  |  |  |  |
| `qr` |  |  |  |  |  |  |  |
| `qrdelete` |  |  |  |  |  |  |  |
| `qrinsert` |  |  |  |  |  |  |  |
| `qrupdate` |  |  |  |  |  |  |  |
| `qz` |  |  |  |  |  |  | not implemented |
| `rank` |  |  |  |  |  |  |  |
| `rcond` |  |  |  |  |  |  |  |
| `rref` |  |  |  |  |  |  |  |
| `rsf2csf` |  |  |  |  |  |  |  |
| `schur` |  |  |  |  |  |  | partial |
| `sqrtm` |  |  |  |  |  |  | partial |
| `subspace` |  |  |  |  |  |  |  |
| `svd` |  |  |  |  |  |  |  |
| `svdappend` |  |  |  |  |  |  | not implemented |
| `svds` |  |  |  |  |  |  | not implemented |
| `svdsketch` |  |  |  |  |  |  | not implemented |
| `sylvester` |  |  |  |  |  |  | partial |
| `trace` |  |  |  |  |  |  |  |
| `transpose` |  |  |  |  |  |  |  |
| `tril` |  |  |  |  |  |  |  |
| `triu` |  |  |  |  |  |  |  |
| `vecnorm` |  |  |  |  |  |  |  |

## ODE



**Namespace:** `ode.*` (future) — 0 ✅ + 0 ⚠️ / 21 = 0%

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `decic` |  |  |  |  |  |  | not implemented |
| `deval` |  |  |  |  |  |  | not implemented |
| `ode` |  |  |  |  |  |  | not implemented |
| `ode113` |  |  |  |  |  |  | not implemented |
| `ode15i` |  |  |  |  |  |  | not implemented |
| `ode15s` |  |  |  |  |  |  | not implemented |
| `ode23` |  |  |  |  |  |  |  |
| `ode23s` |  |  |  |  |  |  | not implemented |
| `ode23t` |  |  |  |  |  |  | not implemented |
| `ode23tb` |  |  |  |  |  |  | not implemented |
| `ode45` |  |  |  |  |  |  |  |
| `ode78` |  |  |  |  |  |  | not implemented |
| `ode89` |  |  |  |  |  |  | not implemented |
| `odeevent` |  |  |  |  |  |  | not implemented |
| `odeget` |  |  |  |  |  |  |  |
| `odejacobian` |  |  |  |  |  |  | not implemented |
| `odemassmatrix` |  |  |  |  |  |  | not implemented |
| `odesensitivity` |  |  |  |  |  |  | not implemented |
| `odeset` |  |  |  |  |  |  |  |
| `odextend` |  |  |  |  |  |  | not implemented |
| `solveode` |  |  |  |  |  |  | not implemented |

## Optimization

### Local

**Namespace:** `optim.*` (top-level promoted: `fzero, fminbnd, fminsearch`) · `optimset/optimget` registered top-level from libs/builtin — 5 ✅ + 0 ⚠️ / 7 = 71%

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `fminbnd` |  |  |  |  |  |  |  |
| `fminsearch` |  |  |  |  |  |  |  |
| `fzero` |  |  |  |  |  |  |  |
| `lsqnonneg` |  |  |  |  |  |  |  |
| `optimget` |  |  |  |  |  |  |  |
| `optimize` |  |  |  |  |  |  | not implemented |
| `optimset` |  |  |  |  |  |  |  |

### Constrained

**Namespace:** `optim.*` — 0 ✅ + 0 ⚠️ / 22 = 0%

The new problem-based API (`optimproblem`, `optimvar`, `optimexpr`,
`optimconstr`, `optimeq`, `optimineq`, `solve`, `evaluate`, `prob2struct`,
`infeasibility`, `findindex`, `issatisfied`, `paretoplot`, `optimvalues`,
the `show*` / `write*` family, `eqnproblem`, `fcn2optimexpr`) is OOP /
expression-tree based and intentionally omitted; we expose only the
solver-based legacy API which is flat function-form.

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `fmincon` |  |  |  |  |  |  | not implemented |
| `fminunc` |  |  |  |  |  |  | not implemented |
| `fseminf` |  |  |  |  |  |  | not implemented |
| `fgoalattain` |  |  |  |  |  |  | not implemented |
| `fminimax` |  |  |  |  |  |  | not implemented |
| `linprog` |  |  |  |  |  |  | not implemented |
| `intlinprog` |  |  |  |  |  |  | not implemented |
| `quadprog` |  |  |  |  |  |  | not implemented |
| `coneprog` |  |  |  |  |  |  | not implemented |
| `secondordercone` |  |  |  |  |  |  | not implemented |
| `lsqlin` |  |  |  |  |  |  | not implemented |
| `lsqcurvefit` |  |  |  |  |  |  | not implemented |
| `lsqnonlin` |  |  |  |  |  |  | not implemented |
| `fsolve` |  |  |  |  |  |  | not implemented |
| `mpsread` |  |  |  |  |  |  | not implemented |
| `optimoptions` |  |  |  |  |  |  | not implemented |
| `resetoptions` |  |  |  |  |  |  | not implemented |
| `checkGradients` |  |  |  |  |  |  | not implemented |
| `optimwarmstart` |  |  |  |  |  |  | not implemented |
| `integerConstraint` |  |  |  |  |  |  | not implemented |
| `mldivide` |  |  |  |  |  |  |  |

### Global

**Namespace:** `gads.*` — 0 ✅ + 0 ⚠️ / 14 = 0%

Problem-based API (`optimproblem`/`optimvar`/etc.), MultiStart class
methods (`createOptimProblem`/`list`/`run`) and `paretoplot` (display)
intentionally omitted — flat solver functions only.

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `ga` |  |  |  |  |  |  | not implemented |
| `gamultiobj` |  |  |  |  |  |  | not implemented |
| `paretosearch` |  |  |  |  |  |  | not implemented |
| `particleswarm` |  |  |  |  |  |  | not implemented |
| `patternsearch` |  |  |  |  |  |  | not implemented |
| `simulannealbnd` |  |  |  |  |  |  | not implemented |
| `surrogateopt` |  |  |  |  |  |  | not implemented |
| `packfcn` |  |  |  |  |  |  | not implemented |
| `gaoptimset` |  |  |  |  |  |  | not implemented |
| `gaoptimget` |  |  |  |  |  |  | not implemented |
| `psoptimset` |  |  |  |  |  |  | not implemented |
| `psoptimget` |  |  |  |  |  |  | not implemented |
| `saoptimset` |  |  |  |  |  |  | not implemented |
| `saoptimget` |  |  |  |  |  |  | not implemented |

## Signal

### Waveform Generation

**Namespace:** `signal.waveform_generation.*` — 5 ✅ + 0 ⚠️ / 21 = 23%

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `buffer` |  |  |  |  |  |  | not implemented |
| `chirp` |  |  |  |  |  |  |  |
| `demod` |  |  |  |  |  |  | not implemented |
| `diric` |  |  |  |  |  |  |  |
| `framelbl` |  |  |  |  |  |  | not implemented |
| `framesig` |  |  |  |  |  |  | not implemented |
| `gauspuls` |  |  |  |  |  |  |  |
| `gmonopuls` |  |  |  |  |  |  |  |
| `marcumq` |  |  |  |  |  |  |  |
| `modulate` |  |  |  |  |  |  | not implemented |
| `pulstran` |  |  |  |  |  |  |  |
| `rectpuls` |  |  |  |  |  |  |  |
| `sawtooth` |  |  |  |  |  |  |  |
| `shiftdata` |  |  |  |  |  |  | not implemented |
| `sinc` |  |  |  |  |  |  |  |
| `square` |  |  |  |  |  |  |  |
| `tripuls` |  |  |  |  |  |  |  |
| `udecode` |  |  |  |  |  |  | not implemented |
| `uencode` |  |  |  |  |  |  | not implemented |
| `unshiftdata` |  |  |  |  |  |  | not implemented |
| `vco` |  |  |  |  |  |  | not implemented |

### Filter Design

**Namespace:** `signal.filter_design.*` — 11 ✅ + 0 ⚠️ / 37 = 30%

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `butter` |  |  |  |  |  |  |  |
| `buttord` |  |  |  |  |  |  |  |
| `cfirpm` |  |  |  |  |  |  | not implemented |
| `cheb1ord` |  |  |  |  |  |  |  |
| `cheb2ord` |  |  |  |  |  |  |  |
| `cheby1` |  |  |  |  |  |  |  |
| `cheby2` |  |  |  |  |  |  |  |
| `designfilt` |  |  |  |  |  |  | not implemented |
| `designfilter` |  |  |  |  |  |  | not implemented |
| `digitalfilter` |  |  |  |  |  |  | not implemented |
| `double` |  |  |  |  |  |  |  |
| `dspfwiz` |  |  |  |  |  |  | not implemented |
| `ellip` |  |  |  |  |  |  |  |
| `ellipord` |  |  |  |  |  |  | partial |
| `filt2block` |  |  |  |  |  |  | not implemented |
| `filteranalyzer` |  |  |  |  |  |  | not implemented |
| `fir1` |  |  |  |  |  |  |  |
| `fir2` |  |  |  |  |  |  |  |
| `fircls` |  |  |  |  |  |  | not implemented |
| `fircls1` |  |  |  |  |  |  | not implemented |
| `firls` |  |  |  |  |  |  | partial |
| `firpm` |  |  |  |  |  |  |  |
| `firpmord` |  |  |  |  |  |  |  |
| `gaussdesign` |  |  |  |  |  |  |  |
| `info` |  |  |  |  |  |  | not implemented |
| `intfilt` |  |  |  |  |  |  |  |
| `isdouble` |  |  |  |  |  |  | not implemented |
| `issingle` |  |  |  |  |  |  |  |
| `kaiserord` |  |  |  |  |  |  |  |
| `maxflat` |  |  |  |  |  |  | not implemented |
| `polyscale` |  |  |  |  |  |  | not implemented |
| `polystab` |  |  |  |  |  |  | not implemented |
| `rcosdesign` |  |  |  |  |  |  |  |
| `scalefiltersections` |  |  |  |  |  |  | not implemented |
| `sgolay` |  |  |  |  |  |  |  |
| `single` |  |  |  |  |  |  |  |
| `yulewalk` |  |  |  |  |  |  | not implemented |

### Analog Filters

**Namespace:** `signal.filter_design.*` — 14 ✅ + 0 ⚠️ / 17 = 82%

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `besselap` |  |  |  |  |  |  |  |
| `besself` |  |  |  |  |  |  |  |
| `bilinear` |  |  |  |  |  |  |  |
| `buttap` |  |  |  |  |  |  |  |
| `butter` |  |  |  |  |  |  |  |
| `cheb1ap` |  |  |  |  |  |  |  |
| `cheb2ap` |  |  |  |  |  |  |  |
| `cheby1` |  |  |  |  |  |  |  |
| `cheby2` |  |  |  |  |  |  |  |
| `ellip` |  |  |  |  |  |  |  |
| `ellipap` |  |  |  |  |  |  |  |
| `freqs` |  |  |  |  |  |  |  |
| `impinvar` |  |  |  |  |  |  |  |
| `lp2bp` |  |  |  |  |  |  |  |
| `lp2bs` |  |  |  |  |  |  |  |
| `lp2hp` |  |  |  |  |  |  |  |
| `lp2lp` |  |  |  |  |  |  |  |

### Digital Filter Analysis

**Namespace:** `signal.filter_analysis.*` — 3 ✅ + 0 ⚠️ / 19 = 15%

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `filteranalyzer` |  |  |  |  |  |  | not implemented |
| `filternorm` |  |  |  |  |  |  |  |
| `filtord` |  |  |  |  |  |  |  |
| `firtype` |  |  |  |  |  |  |  |
| `freqz` |  |  |  |  |  |  |  |
| `grpdelay` |  |  |  |  |  |  |  |
| `impz` |  |  |  |  |  |  |  |
| `impzlength` |  |  |  |  |  |  |  |
| `isallpass` |  |  |  |  |  |  |  |
| `isfir` |  |  |  |  |  |  |  |
| `islinphase` |  |  |  |  |  |  |  |
| `ismaxphase` |  |  |  |  |  |  |  |
| `isminphase` |  |  |  |  |  |  |  |
| `isstable` |  |  |  |  |  |  |  |
| `phasedelay` |  |  |  |  |  |  |  |
| `phasez` |  |  |  |  |  |  |  |
| `stepz` |  |  |  |  |  |  |  |
| `zerophase` |  |  |  |  |  |  |  |
| `zplane` |  |  |  |  |  |  | not implemented |

### Digital Filtering

**Namespace:** `signal.digital_filtering.*` + `signal.filter_implementation.*` (TF/SOS/SS/ZP conversions) — 8 ✅ + 0 ⚠️ / 41 = 19%

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `bandpass` |  |  |  |  |  |  |  |
| `bandstop` |  |  |  |  |  |  |  |
| `cell2sos` |  |  |  |  |  |  | not implemented |
| `convmtx` |  |  |  |  |  |  |  |
| `ctf2zp` |  |  |  |  |  |  | not implemented |
| `ctffilt` |  |  |  |  |  |  | not implemented |
| `dspfwiz` |  |  |  |  |  |  | not implemented |
| `eqtflength` |  |  |  |  |  |  | not implemented |
| `fftfilt` |  |  |  |  |  |  |  |
| `filt2block` |  |  |  |  |  |  | not implemented |
| `filtfilt` |  |  |  |  |  |  |  |
| `filtic` |  |  |  |  |  |  | not implemented |
| `hampel` |  |  |  |  |  |  |  |
| `highpass` |  |  |  |  |  |  |  |
| `latc2tf` |  |  |  |  |  |  | not implemented |
| `latcfilt` |  |  |  |  |  |  | not implemented |
| `lowpass` |  |  |  |  |  |  |  |
| `medfilt1` |  |  |  |  |  |  |  |
| `residuez` |  |  |  |  |  |  |  |
| `scalefiltersections` |  |  |  |  |  |  | not implemented |
| `sgolayfilt` |  |  |  |  |  |  |  |
| `sos2cell` |  |  |  |  |  |  | not implemented |
| `sos2ctf` |  |  |  |  |  |  | not implemented |
| `sos2ss` |  |  |  |  |  |  |  |
| `sos2tf` |  |  |  |  |  |  |  |
| `sos2zp` |  |  |  |  |  |  |  |
| `sosfilt` |  |  |  |  |  |  |  |
| `ss` |  |  |  |  |  |  |  |
| `ss2sos` |  |  |  |  |  |  |  |
| `ss2zp` |  |  |  |  |  |  |  |
| `tf` |  |  |  |  |  |  |  |
| `tf2latc` |  |  |  |  |  |  | not implemented |
| `tf2sos` |  |  |  |  |  |  |  |
| `tf2ss` |  |  |  |  |  |  |  |
| `tf2zp` |  |  |  |  |  |  |  |
| `tf2zpk` |  |  |  |  |  |  |  |
| `zp2ctf` |  |  |  |  |  |  | not implemented |
| `zp2sos` |  |  |  |  |  |  |  |
| `zp2ss` |  |  |  |  |  |  |  |
| `zp2tf` |  |  |  |  |  |  |  |
| `zpk` |  |  |  |  |  |  |  |
| `filter` | 0.006638 | 1.45× |  | 8.986 | 0.14× |  |  |
| `filter2` |  |  |  |  |  |  |  |

### Multirate Signal Processing

**Namespace:** `signal.multirate.*` — 4 ✅ + 0 ⚠️ / 8 = 50%

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `decimate` |  |  |  |  |  |  |  |
| `downsample` |  |  |  |  |  |  |  |
| `fillgaps` |  |  |  |  |  |  | not implemented |
| `interp` |  |  |  |  |  |  |  |
| `intfilt` |  |  |  |  |  |  |  |
| `resample` |  |  |  |  |  |  |  |
| `upfirdn` |  |  |  |  |  |  |  |
| `upsample` |  |  |  |  |  |  |  |

### Signal Modeling

**Namespace:** `signal.parametric.*` — 23 ✅ + 0 ⚠️ / 25 = 92%

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `ac2poly` |  |  |  |  |  |  |  |
| `ac2rc` |  |  |  |  |  |  |  |
| `arburg` |  |  |  |  |  |  |  |
| `arcov` |  |  |  |  |  |  |  |
| `armcov` |  |  |  |  |  |  |  |
| `aryule` |  |  |  |  |  |  |  |
| `corrmtx` |  |  |  |  |  |  |  |
| `invfreqs` |  |  |  |  |  |  |  |
| `invfreqz` |  |  |  |  |  |  |  |
| `is2rc` |  |  |  |  |  |  |  |
| `lar2rc` |  |  |  |  |  |  |  |
| `levinson` |  |  |  |  |  |  |  |
| `lpc` |  |  |  |  |  |  |  |
| `lsf2poly` |  |  |  |  |  |  |  |
| `poly2ac` |  |  |  |  |  |  |  |
| `poly2lsf` |  |  |  |  |  |  |  |
| `poly2rc` |  |  |  |  |  |  |  |
| `prony` |  |  |  |  |  |  |  |
| `rc2ac` |  |  |  |  |  |  |  |
| `rc2is` |  |  |  |  |  |  |  |
| `rc2lar` |  |  |  |  |  |  |  |
| `rc2poly` |  |  |  |  |  |  |  |
| `rlevinson` |  |  |  |  |  |  |  |
| `schurrc` |  |  |  |  |  |  |  |
| `stmcb` |  |  |  |  |  |  | not implemented |

### Correlation and Convolution

**Namespace:** `signal.convolution.*` — 0 ✅ + 0 ⚠️ / 9 = 0%

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `alignsignals` |  |  |  |  |  |  |  |
| `cconv` |  |  |  |  |  |  |  |
| `convmtx` |  |  |  |  |  |  |  |
| `corrmtx` |  |  |  |  |  |  |  |
| `dtw` |  |  |  |  |  |  | not implemented |
| `edr` |  |  |  |  |  |  | not implemented |
| `finddelay` |  |  |  |  |  |  |  |
| `findsignal` |  |  |  |  |  |  | not implemented |
| `xcorr2` |  |  |  |  |  |  |  |
| `conv` | 0.003104 | 5.25× |  | 6.509 | 0.30× |  |  |
| `conv2` |  |  |  |  |  |  |  |
| `convn` |  |  |  |  |  |  |  |
| `deconv` |  |  |  |  |  |  |  |

### Transforms

**Namespace:** `signal.transforms.*`. Promotions in core: `fft, ifft, fftshift, ifftshift`. Future wavelet split: `cwt/dwt/modwt/...` → `wavelet.*` — 6 ✅ + 0 ⚠️ / 32 = 18%

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `bitrevorder` |  |  |  |  |  |  |  |
| `cceps` |  |  |  |  |  |  |  |
| `czt` |  |  |  |  |  |  |  |
| `dct` |  |  |  |  |  |  |  |
| `dftmtx` |  |  |  |  |  |  |  |
| `digitrevorder` |  |  |  |  |  |  | not implemented |
| `dlistft` |  |  |  |  |  |  | not implemented |
| `dlstft` |  |  |  |  |  |  | not implemented |
| `emd` |  |  |  |  |  |  | not implemented |
| `envelope` |  |  |  |  |  |  |  |
| `fsst` |  |  |  |  |  |  | not implemented |
| `fwht` |  |  |  |  |  |  |  |
| `goertzel` |  |  |  |  |  |  |  |
| `hht` |  |  |  |  |  |  | not implemented |
| `hilbert` |  |  |  |  |  |  |  |
| `icceps` |  |  |  |  |  |  |  |
| `idct` |  |  |  |  |  |  |  |
| `ifsst` |  |  |  |  |  |  | not implemented |
| `ifwht` |  |  |  |  |  |  |  |
| `instfreq` |  |  |  |  |  |  |  |
| `istft` |  |  |  |  |  |  |  |
| `istftlayer` |  |  |  |  |  |  | not implemented |
| `pspectrum` |  |  |  |  |  |  | not implemented |
| `rceps` |  |  |  |  |  |  |  |
| `spectrogram` |  |  |  |  |  |  |  |
| `stft` |  |  |  |  |  |  |  |
| `stftlayer` |  |  |  |  |  |  | not implemented |
| `stftmag2sig` |  |  |  |  |  |  | not implemented |
| `vmd` |  |  |  |  |  |  | not implemented |
| `wvd` |  |  |  |  |  |  | not implemented |
| `xspectrogram` |  |  |  |  |  |  | not implemented |
| `xwvd` |  |  |  |  |  |  | not implemented |
| `fft` | 0.003832 | 1.09× |  | 16.78 | 0.22× |  |  |
| `fft2` |  |  |  |  |  |  |  |
| `fftn` |  |  |  |  |  |  |  |
| `fftshift` |  |  |  |  |  |  |  |
| `fftw` |  |  |  |  |  |  | not implemented |
| `ifft` |  |  |  |  |  |  |  |
| `ifft2` |  |  |  |  |  |  |  |
| `ifftn` |  |  |  |  |  |  |  |
| `ifftshift` |  |  |  |  |  |  |  |
| `interpft` |  |  |  |  |  |  |  |
| `nextpow2` |  |  |  |  |  |  |  |
| `nufft` |  |  |  |  |  |  | not implemented |
| `nufftn` |  |  |  |  |  |  | not implemented |

### Windows

**Namespace:** `signal.windows.*` — 6 ✅ + 0 ⚠️ / 24 = 25%

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `barthannwin` |  |  |  |  |  |  |  |
| `bartlett` |  |  |  |  |  |  |  |
| `blackman` |  |  |  |  |  |  |  |
| `blackmanharris` |  |  |  |  |  |  |  |
| `bohmanwin` |  |  |  |  |  |  |  |
| `chebwin` |  |  |  |  |  |  |  |
| `dpss` |  |  |  |  |  |  | not implemented |
| `dpssclear` |  |  |  |  |  |  | not implemented |
| `dpssdir` |  |  |  |  |  |  | not implemented |
| `dpssload` |  |  |  |  |  |  | not implemented |
| `dpsssave` |  |  |  |  |  |  | not implemented |
| `enbw` |  |  |  |  |  |  |  |
| `flattopwin` |  |  |  |  |  |  |  |
| `gausswin` |  |  |  |  |  |  |  |
| `hamming` |  |  |  |  |  |  |  |
| `hann` |  |  |  |  |  |  |  |
| `kaiser` |  |  |  |  |  |  |  |
| `nuttallwin` |  |  |  |  |  |  |  |
| `parzenwin` |  |  |  |  |  |  |  |
| `rectwin` |  |  |  |  |  |  |  |
| `taylorwin` |  |  |  |  |  |  |  |
| `triang` |  |  |  |  |  |  |  |
| `tukeywin` |  |  |  |  |  |  |  |
| `wvtool` |  |  |  |  |  |  | not implemented |

### Parametric Spectral Estimation

**Namespace:** `signal.spectral_analysis.*`. Magnitude utils (`db/db2mag/mag2db/pow2db`) → core (cross-cutting math) — 3 ✅ + 0 ⚠️ / 10 = 30%

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `db` |  |  |  |  |  |  |  |
| `db2mag` |  |  |  |  |  |  |  |
| `db2pow` |  |  |  |  |  |  |  |
| `findpeaks` |  |  |  |  |  |  |  |
| `mag2db` |  |  |  |  |  |  |  |
| `pburg` |  |  |  |  |  |  |  |
| `pcov` |  |  |  |  |  |  | not implemented |
| `pmcov` |  |  |  |  |  |  | not implemented |
| `pow2db` |  |  |  |  |  |  |  |
| `pyulear` |  |  |  |  |  |  |  |

### Nonparametric Spectral Estimation

**Namespace:** `signal.spectral_analysis.*` — 6 ✅ + 0 ⚠️ / 17 = 35%

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `cpsd` |  |  |  |  |  |  |  |
| `db` |  |  |  |  |  |  |  |
| `db2mag` |  |  |  |  |  |  |  |
| `db2pow` |  |  |  |  |  |  |  |
| `findpeaks` |  |  |  |  |  |  |  |
| `mag2db` |  |  |  |  |  |  |  |
| `mscohere` |  |  |  |  |  |  |  |
| `periodogram` |  |  |  |  |  |  |  |
| `plomb` |  |  |  |  |  |  | not implemented |
| `pmtm` |  |  |  |  |  |  | not implemented |
| `poctave` |  |  |  |  |  |  | not implemented |
| `pow2db` |  |  |  |  |  |  |  |
| `pspectrum` |  |  |  |  |  |  | not implemented |
| `pwelch` |  |  |  |  |  |  |  |
| `refinepeaks` |  |  |  |  |  |  | not implemented |
| `spectralentropy` |  |  |  |  |  |  |  |
| `tfestimate` |  |  |  |  |  |  |  |

### Spectral Measurements

**Namespace:** `signal.spectral_analysis.*` — 0 ✅ + 0 ⚠️ / 18 = 0%

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `bandpower` |  |  |  |  |  |  |  |
| `enbw` |  |  |  |  |  |  |  |
| `instbw` |  |  |  |  |  |  |  |
| `instfreq` |  |  |  |  |  |  |  |
| `meanfreq` |  |  |  |  |  |  |  |
| `medfreq` |  |  |  |  |  |  |  |
| `obw` |  |  |  |  |  |  |  |
| `powerbw` |  |  |  |  |  |  |  |
| `sfdr` |  |  |  |  |  |  |  |
| `sinad` |  |  |  |  |  |  |  |
| `snr` |  |  |  |  |  |  |  |
| `spectralcrest` |  |  |  |  |  |  |  |
| `spectralentropy` |  |  |  |  |  |  |  |
| `spectralflatness` |  |  |  |  |  |  |  |
| `spectralkurtosis` |  |  |  |  |  |  |  |
| `spectralskewness` |  |  |  |  |  |  |  |
| `thd` |  |  |  |  |  |  |  |
| `toi` |  |  |  |  |  |  | not implemented |

### Time-Frequency Analysis

**Namespace:** `signal.time_frequency.*`. Wavelet/EMD subset (`cwt/wsst/vmd/hht/emd/fsst/ifsst`) → `wavelet.*` (future) — 1 ✅ + 0 ⚠️ / 27 = 3%

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `dlistft` |  |  |  |  |  |  | not implemented |
| `dlstft` |  |  |  |  |  |  | not implemented |
| `emd` |  |  |  |  |  |  | not implemented |
| `fsst` |  |  |  |  |  |  | not implemented |
| `hht` |  |  |  |  |  |  | not implemented |
| `ifsst` |  |  |  |  |  |  | not implemented |
| `instbw` |  |  |  |  |  |  |  |
| `instfreq` |  |  |  |  |  |  |  |
| `iscola` |  |  |  |  |  |  |  |
| `istft` |  |  |  |  |  |  |  |
| `istftlayer` |  |  |  |  |  |  | not implemented |
| `kurtogram` |  |  |  |  |  |  | not implemented |
| `pspectrum` |  |  |  |  |  |  | not implemented |
| `spectralcrest` |  |  |  |  |  |  |  |
| `spectralentropy` |  |  |  |  |  |  |  |
| `spectralflatness` |  |  |  |  |  |  |  |
| `spectralkurtosis` |  |  |  |  |  |  |  |
| `spectralskewness` |  |  |  |  |  |  |  |
| `spectrogram` |  |  |  |  |  |  |  |
| `stft` |  |  |  |  |  |  |  |
| `stftlayer` |  |  |  |  |  |  | not implemented |
| `stftmag2sig` |  |  |  |  |  |  | not implemented |
| `tfridge` |  |  |  |  |  |  | not implemented |
| `vmd` |  |  |  |  |  |  | not implemented |
| `wvd` |  |  |  |  |  |  | not implemented |
| `xspectrogram` |  |  |  |  |  |  | not implemented |
| `xwvd` |  |  |  |  |  |  | not implemented |

### Pulse and Transition Metrics

**Namespace:** `signal.measurements.*` — 0 ✅ + 0 ⚠️ / 12 = 0%

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `dutycycle` |  |  |  |  |  |  |  |
| `falltime` |  |  |  |  |  |  |  |
| `midcross` |  |  |  |  |  |  |  |
| `overshoot` |  |  |  |  |  |  |  |
| `pulseperiod` |  |  |  |  |  |  |  |
| `pulsesep` |  |  |  |  |  |  |  |
| `pulsewidth` |  |  |  |  |  |  |  |
| `risetime` |  |  |  |  |  |  |  |
| `settlingtime` |  |  |  |  |  |  |  |
| `slewrate` |  |  |  |  |  |  |  |
| `statelevels` |  |  |  |  |  |  |  |
| `undershoot` |  |  |  |  |  |  |  |

### Signal Descriptive Statistics

**Namespace:** `signal.measurements.*` — 2 ✅ + 0 ⚠️ / 30 = 6%

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `alignsignals` |  |  |  |  |  |  |  |
| `binmask2sigroi` |  |  |  |  |  |  | not implemented |
| `countlabels` |  |  |  |  |  |  | not implemented |
| `cusum` |  |  |  |  |  |  | not implemented |
| `dtw` |  |  |  |  |  |  | not implemented |
| `edr` |  |  |  |  |  |  | not implemented |
| `envelope` |  |  |  |  |  |  |  |
| `extendsigroi` |  |  |  |  |  |  | not implemented |
| `extractsigroi` |  |  |  |  |  |  | not implemented |
| `filenames2labels` |  |  |  |  |  |  | not implemented |
| `findchangepts` |  |  |  |  |  |  | not implemented |
| `finddelay` |  |  |  |  |  |  |  |
| `findpeaks` |  |  |  |  |  |  |  |
| `findsignal` |  |  |  |  |  |  | not implemented |
| `folders2labels` |  |  |  |  |  |  | not implemented |
| `framelbl` |  |  |  |  |  |  | not implemented |
| `framesig` |  |  |  |  |  |  | not implemented |
| `meanfreq` |  |  |  |  |  |  |  |
| `medfreq` |  |  |  |  |  |  |  |
| `mergesigroi` |  |  |  |  |  |  | not implemented |
| `peak2peak` |  |  |  |  |  |  |  |
| `peak2rms` |  |  |  |  |  |  |  |
| `removesigroi` |  |  |  |  |  |  | not implemented |
| `rssq` |  |  |  |  |  |  |  |
| `seqperiod` |  |  |  |  |  |  | not implemented |
| `shortensigroi` |  |  |  |  |  |  | not implemented |
| `sigrangebinmask` |  |  |  |  |  |  | not implemented |
| `sigroi2binmask` |  |  |  |  |  |  | not implemented |
| `splitlabels` |  |  |  |  |  |  | not implemented |
| `zerocrossrate` |  |  |  |  |  |  | not implemented |

### Smoothing and Denoising

**Namespace:** `signal.smoothing.*` + `signal.digital_filtering.*` (medfilt1, sgolayfilt). `smoothdata` itself → `stats.moving.*` — 3 ✅ + 0 ⚠️ / 4 = 75%

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `hampel` |  |  |  |  |  |  |  |
| `medfilt1` |  |  |  |  |  |  |  |
| `sgolay` |  |  |  |  |  |  |  |
| `sgolayfilt` |  |  |  |  |  |  |  |

### Vibration Analysis

**Namespace:** `signal.vibration.*` — 0 ✅ + 0 ⚠️ / 13 = 0%

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `envspectrum` |  |  |  |  |  |  |  |
| `modalfit` |  |  |  |  |  |  | not implemented |
| `modalfrf` |  |  |  |  |  |  | not implemented |
| `modalsd` |  |  |  |  |  |  | not implemented |
| `orderspectrum` |  |  |  |  |  |  | not implemented |
| `ordertrack` |  |  |  |  |  |  | not implemented |
| `orderwaveform` |  |  |  |  |  |  | not implemented |
| `rainflow` |  |  |  |  |  |  |  |
| `rpmfreqmap` |  |  |  |  |  |  | not implemented |
| `rpmordermap` |  |  |  |  |  |  | not implemented |
| `rpmtrack` |  |  |  |  |  |  | not implemented |
| `tachorpm` |  |  |  |  |  |  |  |
| `tsa` |  |  |  |  |  |  |  |

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

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `spectralCentroid` |  |  |  |  |  |  |  |
| `spectralCrest` |  |  |  |  |  |  |  |
| `spectralDecrease` |  |  |  |  |  |  |  |
| `spectralEntropy` |  |  |  |  |  |  |  |
| `spectralFlatness` |  |  |  |  |  |  |  |
| `spectralFlux` |  |  |  |  |  |  |  |
| `spectralKurtosis` |  |  |  |  |  |  |  |
| `spectralRolloffPoint` |  |  |  |  |  |  |  |
| `spectralSkewness` |  |  |  |  |  |  |  |
| `spectralSlope` |  |  |  |  |  |  |  |
| `spectralSpread` |  |  |  |  |  |  |  |

### Audio Feature Extraction

**Namespace:** `audio.features.*` (planned) — 0 ✅ + 0 ⚠️ / 7 = 0%

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `audioDelta` |  |  |  |  |  |  |  |
| `cepstralCoefficients` |  |  |  |  |  |  |  |
| `gtcc` |  |  |  |  |  |  |  |
| `harmonicRatio` |  |  |  |  |  |  |  |
| `mfcc` |  |  |  |  |  |  |  |
| `pitch` |  |  |  |  |  |  |  |
| `pitchnn` |  |  |  |  |  |  | not implemented |

### Audio Time-Frequency

**Namespace:** `audio.spectrogram.*` (planned) — 0 ✅ + 0 ⚠️ / 1 = 0%

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `melSpectrogram` |  |  |  |  |  |  |  |

### Audio Frequency / Loudness Conversions

**Namespace:** `audio.scale.*` (planned) — 0 ✅ + 0 ⚠️ / 8 = 0%

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `bark2hz` |  |  |  |  |  |  |  |
| `erb2hz` |  |  |  |  |  |  |  |
| `hz2bark` |  |  |  |  |  |  |  |
| `hz2erb` |  |  |  |  |  |  |  |
| `hz2mel` |  |  |  |  |  |  |  |
| `mel2hz` |  |  |  |  |  |  |  |
| `phon2sone` |  |  |  |  |  |  |  |
| `sone2phon` |  |  |  |  |  |  |  |

## Statistics

### Descriptive Statistics

**Namespace:** `stats.descriptive.*` / `stats.moving.*` / `stats.nan.*`. Exception: `xcorr/xcov/rms/rssq/peak2peak/peak2rms` → `signal.*` (signal-side stats) — 14 ✅ + 0 ⚠️ / 33 = 42%

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `bounds` |  |  |  |  |  |  |  |
| `corrcoef` |  |  |  |  |  |  |  |
| `cov` |  |  |  |  |  |  |  |
| `cummax` |  |  |  |  |  |  |  |
| `cummin` |  |  |  |  |  |  |  |
| `iqr` |  |  |  |  |  |  |  |
| `kde` |  |  |  |  |  |  |  |
| `mape` |  |  |  |  |  |  |  |
| `max` | 0.002037 | 2.25× |  | 1.447 | 0.03× |  |  |
| `maxk` |  |  |  |  |  |  |  |
| `mean` | 0.001619 | 14.83× |  | 1.34 | 0.03× |  |  |
| `median` |  |  |  |  |  |  |  |
| `min` | 0.001923 | 2.08× |  | 1.441 | 0.04× |  |  |
| `mink` |  |  |  |  |  |  |  |
| `mode` |  |  |  |  |  |  |  |
| `movmad` |  |  |  |  |  |  |  |
| `movmax` |  |  |  |  |  |  |  |
| `movmean` |  |  |  |  |  |  |  |
| `movmedian` |  |  |  |  |  |  |  |
| `movmin` |  |  |  |  |  |  |  |
| `movprod` |  |  |  |  |  |  |  |
| `movstd` |  |  |  |  |  |  |  |
| `movsum` |  |  |  |  |  |  |  |
| `movvar` |  |  |  |  |  |  |  |
| `prctile` |  |  |  |  |  |  |  |
| `quantile` |  |  |  |  |  |  |  |
| `rms` |  |  |  |  |  |  |  |
| `rmse` |  |  |  |  |  |  |  |
| `std` |  |  |  |  |  |  |  |
| `summary` |  |  |  |  |  |  | not implemented |
| `var` |  |  |  |  |  |  |  |
| `xcorr` |  |  |  |  |  |  |  |
| `xcov` |  |  |  |  |  |  |  |

### Descriptive Statistics — extras

**Namespace:** `stats.descriptive.*` — additions on top of the existing section above. 0 ✅ + 0 ⚠️ / 23 = 0%

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `cholcov` |  |  |  |  |  |  |  |
| `corr` |  |  |  |  |  |  | partial |
| `corrcov` |  |  |  |  |  |  |  |
| `crosstab` |  |  |  |  |  |  |  |
| `geomean` |  |  |  |  |  |  |  |
| `grpstats` |  |  |  |  |  |  |  |
| `harmmean` |  |  |  |  |  |  |  |
| `kurtosis` |  |  |  |  |  |  |  |
| `mad` |  |  |  |  |  |  |  |
| `moment` |  |  |  |  |  |  |  |
| `nearcorr` |  |  |  |  |  |  | partial |
| `partialcorr` |  |  |  |  |  |  |  |
| `partialcorri` |  |  |  |  |  |  |  |
| `range` |  |  |  |  |  |  |  |
| `robustcov` |  |  |  |  |  |  |  |
| `skewness` |  |  |  |  |  |  |  |
| `tabulate` |  |  |  |  |  |  |  |
| `tiedrank` |  |  |  |  |  |  |  |
| `trimmean` |  |  |  |  |  |  |  |
| `zscore` |  |  |  |  |  |  |  |
| `nancov` |  |  |  |  |  |  |  |
| `nansum` |  |  |  |  |  |  |  |
| `nanmean` |  |  |  |  |  |  |  |

### Probability Distributions

**Namespace:** `stats.dist.*` — 115 ✅ + 0 ⚠️ / 130+ = 88%

Each distribution provides 5 entrypoints: `*pdf` / `*cdf` / `*inv` (or `*icdf`) / `*rnd` / `*stat`. All `rnd` functions share `numkit::builtin::sharedEngine()` so `rng(seed)` reseeds them. Discrete `*inv` use one-ULP relative tolerance against the public cdf so `inv(cdf(k))=k` round-trips don't overshoot.

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `normpdf` |  |  |  |  |  |  |  |
| `normcdf` |  |  |  |  |  |  |  |
| `norminv` |  |  |  |  |  |  |  |
| `normrnd` |  |  |  |  |  |  |  |
| `normstat` |  |  |  |  |  |  |  |
| `chi2pdf` |  |  |  |  |  |  |  |
| `chi2cdf` |  |  |  |  |  |  |  |
| `chi2inv` |  |  |  |  |  |  |  |
| `chi2rnd` |  |  |  |  |  |  |  |
| `chi2stat` |  |  |  |  |  |  |  |
| `tpdf` |  |  |  |  |  |  |  |
| `tcdf` |  |  |  |  |  |  |  |
| `tinv` |  |  |  |  |  |  |  |
| `trnd` |  |  |  |  |  |  |  |
| `tstat` |  |  |  |  |  |  |  |
| `fpdf` |  |  |  |  |  |  |  |
| `fcdf` |  |  |  |  |  |  |  |
| `finv` |  |  |  |  |  |  |  |
| `frnd` |  |  |  |  |  |  |  |
| `fstat` |  |  |  |  |  |  |  |
| `betapdf` |  |  |  |  |  |  |  |
| `betacdf` |  |  |  |  |  |  |  |
| `betainv` |  |  |  |  |  |  |  |
| `betarnd` |  |  |  |  |  |  |  |
| `betastat` |  |  |  |  |  |  |  |
| `gampdf` |  |  |  |  |  |  |  |
| `gamcdf` |  |  |  |  |  |  |  |
| `gaminv` |  |  |  |  |  |  |  |
| `gamrnd` |  |  |  |  |  |  |  |
| `gamstat` |  |  |  |  |  |  |  |
| `exppdf` |  |  |  |  |  |  |  |
| `expcdf` |  |  |  |  |  |  |  |
| `expinv` |  |  |  |  |  |  |  |
| `exprnd` |  |  |  |  |  |  |  |
| `expstat` |  |  |  |  |  |  |  |
| `unifpdf` |  |  |  |  |  |  |  |
| `unifcdf` |  |  |  |  |  |  |  |
| `unifinv` |  |  |  |  |  |  |  |
| `unifrnd` |  |  |  |  |  |  |  |
| `unifstat` |  |  |  |  |  |  |  |
| `lognpdf` |  |  |  |  |  |  |  |
| `logncdf` |  |  |  |  |  |  |  |
| `logninv` |  |  |  |  |  |  |  |
| `lognrnd` |  |  |  |  |  |  |  |
| `lognstat` |  |  |  |  |  |  |  |
| `wblpdf` |  |  |  |  |  |  |  |
| `wblcdf` |  |  |  |  |  |  |  |
| `wblinv` |  |  |  |  |  |  |  |
| `wblrnd` |  |  |  |  |  |  |  |
| `wblstat` |  |  |  |  |  |  |  |
| `raylpdf` |  |  |  |  |  |  |  |
| `raylcdf` |  |  |  |  |  |  |  |
| `raylinv` |  |  |  |  |  |  |  |
| `raylrnd` |  |  |  |  |  |  |  |
| `raylstat` |  |  |  |  |  |  |  |
| `poisspdf` |  |  |  |  |  |  |  |
| `poisscdf` |  |  |  |  |  |  |  |
| `poissinv` |  |  |  |  |  |  |  |
| `poissrnd` |  |  |  |  |  |  |  |
| `poisstat` |  |  |  |  |  |  |  |
| `binopdf` |  |  |  |  |  |  |  |
| `binocdf` |  |  |  |  |  |  |  |
| `binoinv` |  |  |  |  |  |  |  |
| `binornd` |  |  |  |  |  |  |  |
| `binostat` |  |  |  |  |  |  |  |
| `unidpdf` |  |  |  |  |  |  |  |
| `unidcdf` |  |  |  |  |  |  |  |
| `unidinv` |  |  |  |  |  |  |  |
| `unidrnd` |  |  |  |  |  |  |  |
| `unidstat` |  |  |  |  |  |  |  |
| `geopdf` |  |  |  |  |  |  |  |
| `geocdf` |  |  |  |  |  |  |  |
| `geoinv` |  |  |  |  |  |  |  |
| `geornd` |  |  |  |  |  |  |  |
| `geostat` |  |  |  |  |  |  |  |
| `nbinpdf` |  |  |  |  |  |  |  |
| `nbincdf` |  |  |  |  |  |  |  |
| `nbininv` |  |  |  |  |  |  |  |
| `nbinrnd` |  |  |  |  |  |  |  |
| `nbinstat` |  |  |  |  |  |  |  |
| `hygepdf` |  |  |  |  |  |  |  |
| `hygecdf` |  |  |  |  |  |  |  |
| `hygeinv` |  |  |  |  |  |  |  |
| `hygernd` |  |  |  |  |  |  |  |
| `hygestat` |  |  |  |  |  |  |  |
| `evpdf` |  |  |  |  |  |  |  |
| `evcdf` |  |  |  |  |  |  |  |
| `evinv` |  |  |  |  |  |  |  |
| `evrnd` |  |  |  |  |  |  |  |
| `evstat` |  |  |  |  |  |  |  |
| `gevpdf` |  |  |  |  |  |  |  |
| `gevcdf` |  |  |  |  |  |  |  |
| `gevinv` |  |  |  |  |  |  |  |
| `gevrnd` |  |  |  |  |  |  |  |
| `gevstat` |  |  |  |  |  |  |  |
| `gppdf` |  |  |  |  |  |  |  |
| `gpcdf` |  |  |  |  |  |  |  |
| `gpinv` |  |  |  |  |  |  |  |
| `gprnd` |  |  |  |  |  |  |  |
| `gpstat` |  |  |  |  |  |  |  |
| `nakapdf` |  |  |  |  |  |  |  |
| `nakacdf` |  |  |  |  |  |  |  |
| `nakainv` |  |  |  |  |  |  |  |
| `nakarnd` |  |  |  |  |  |  |  |
| `nakastat` |  |  |  |  |  |  |  |
| `ricepdf` |  |  |  |  |  |  |  |
| `ricecdf` |  |  |  |  |  |  |  |
| `riceinv` |  |  |  |  |  |  |  |
| `ricernd` |  |  |  |  |  |  |  |
| `ricestat` |  |  |  |  |  |  |  |
| `ncfpdf` |  |  |  |  |  |  |  |
| `ncfcdf` |  |  |  |  |  |  |  |
| `ncfinv` |  |  |  |  |  |  |  |
| `ncfrnd` |  |  |  |  |  |  |  |
| `ncfstat` |  |  |  |  |  |  |  |
| `nctpdf` |  |  |  |  |  |  |  |
| `nctcdf` |  |  |  |  |  |  |  |
| `nctinv` |  |  |  |  |  |  |  |
| `nctrnd` |  |  |  |  |  |  |  |
| `nctstat` |  |  |  |  |  |  |  |
| `ncx2pdf` |  |  |  |  |  |  |  |
| `ncx2cdf` |  |  |  |  |  |  |  |
| `ncx2inv` |  |  |  |  |  |  |  |
| `ncx2rnd` |  |  |  |  |  |  |  |
| `ncx2stat` |  |  |  |  |  |  |  |

### Distribution Fitting (MLE / likelihood)

**Namespace:** `stats.fit.*` — 16 ✅ + 0 ⚠️ / 24 = 67%

OOP `fitdist` / `makedist` family intentionally omitted — only flat
function-form fitters (return `[parmhat, parmci]`) and likelihood evaluators.

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `mle` |  |  |  |  |  |  | partial |
| `mlecov` |  |  |  |  |  |  | not implemented |
| `betafit` |  |  |  |  |  |  |  |
| `betalike` |  |  |  |  |  |  |  |
| `binofit` |  |  |  |  |  |  |  |
| `evfit` |  |  |  |  |  |  |  |
| `evlike` |  |  |  |  |  |  |  |
| `expfit` |  |  |  |  |  |  |  |
| `explike` |  |  |  |  |  |  |  |
| `gamfit` |  |  |  |  |  |  |  |
| `gamlike` |  |  |  |  |  |  |  |
| `gevfit` |  |  |  |  |  |  |  |
| `gevlike` |  |  |  |  |  |  |  |
| `gpfit` |  |  |  |  |  |  |  |
| `gplike` |  |  |  |  |  |  |  |
| `lognfit` |  |  |  |  |  |  |  |
| `lognlike` |  |  |  |  |  |  |  |
| `nbinfit` |  |  |  |  |  |  |  |
| `normfit` |  |  |  |  |  |  |  |
| `normlike` |  |  |  |  |  |  |  |
| `poissfit` |  |  |  |  |  |  |  |
| `raylfit` |  |  |  |  |  |  |  |
| `unifit` |  |  |  |  |  |  |  |
| `wblfit` |  |  |  |  |  |  |  |
| `wbllike` |  |  |  |  |  |  |  |

### Multivariate Distributions

**Namespace:** `stats.mvdist.*` — 3 ✅ + 0 ⚠️ / 14 = 21%

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `mvncdf` |  |  |  |  |  |  |  |
| `mvnpdf` |  |  |  |  |  |  |  |
| `mvnrnd` |  |  |  |  |  |  |  |
| `mvtcdf` |  |  |  |  |  |  |  |
| `mvtpdf` |  |  |  |  |  |  |  |
| `mvtrnd` |  |  |  |  |  |  |  |
| `mnpdf` |  |  |  |  |  |  |  |
| `mnrnd` |  |  |  |  |  |  |  |
| `wishrnd` |  |  |  |  |  |  |  |
| `iwishrnd` |  |  |  |  |  |  |  |
| `copulapdf` |  |  |  |  |  |  |  |
| `copulacdf` |  |  |  |  |  |  |  |
| `copulafit` |  |  |  |  |  |  | not implemented |
| `copulaparam` |  |  |  |  |  |  | not implemented |
| `copulastat` |  |  |  |  |  |  | not implemented |
| `copularnd` |  |  |  |  |  |  | not implemented |

### Pearson / Johnson Distributions

**Namespace:** `stats.pearson.*` / `stats.johnson.*` — 0 ✅ + 0 ⚠️ / 6 = 0%

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `pearspdf` |  |  |  |  |  |  | not implemented |
| `pearscdf` |  |  |  |  |  |  | not implemented |
| `pearsinv` |  |  |  |  |  |  | not implemented |
| `pearsrnd` |  |  |  |  |  |  | not implemented |
| `johnsrnd` |  |  |  |  |  |  | not implemented |
| `randg` |  |  |  |  |  |  |  |

### Empirical / Kernel Distributions

**Namespace:** `stats.empirical.*` — 3 ✅ + 0 ⚠️ / 4 = 75%

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `ecdf` |  |  |  |  |  |  |  |
| `ecdfhist` |  |  |  |  |  |  |  |
| `ksdensity` |  |  |  |  |  |  |  |
| `mvksdensity` |  |  |  |  |  |  | not implemented |

### Hypothesis Tests

**Namespace:** `stats.test.*` — 16 ✅ + 0 ⚠️ / 25 = 64%

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `adtest` |  |  |  |  |  |  |  |
| `ansaribradley` |  |  |  |  |  |  |  |
| `barttest` |  |  |  |  |  |  | not implemented |
| `chi2gof` |  |  |  |  |  |  |  |
| `dwtest` |  |  |  |  |  |  |  |
| `fishertest` |  |  |  |  |  |  |  |
| `friedman` |  |  |  |  |  |  | not implemented |
| `jbtest` |  |  |  |  |  |  |  |
| `knntest` |  |  |  |  |  |  | not implemented |
| `kruskalwallis` |  |  |  |  |  |  |  |
| `kstest` |  |  |  |  |  |  |  |
| `kstest2` |  |  |  |  |  |  |  |
| `lillietest` |  |  |  |  |  |  |  |
| `meanEffectSize` |  |  |  |  |  |  | not implemented |
| `mmdtest` |  |  |  |  |  |  | not implemented |
| `multcompare` |  |  |  |  |  |  |  |
| `ranksum` |  |  |  |  |  |  |  |
| `runstest` |  |  |  |  |  |  |  |
| `sampsizepwr` |  |  |  |  |  |  | not implemented |
| `signrank` |  |  |  |  |  |  |  |
| `signtest` |  |  |  |  |  |  |  |
| `ttest` |  |  |  |  |  |  |  |
| `ttest2` |  |  |  |  |  |  |  |
| `vartest` |  |  |  |  |  |  |  |
| `vartest2` |  |  |  |  |  |  |  |
| `vartestn` |  |  |  |  |  |  |  |
| `ztest` |  |  |  |  |  |  |  |

### Resampling Techniques

**Namespace:** `stats.resample.*` — 3 ✅ + 0 ⚠️ / 7 = 38%

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `bootci` |  |  |  |  |  |  | partial |
| `bootstrp` |  |  |  |  |  |  | partial |
| `combnk` |  |  |  |  |  |  |  |
| `crossval` |  |  |  |  |  |  | partial |
| `cvpartition` |  |  |  |  |  |  | not implemented |
| `datasample` |  |  |  |  |  |  |  |
| `jackknife` |  |  |  |  |  |  | partial |
| `randsample` |  |  |  |  |  |  |  |

### Quasirandom Sequences and MCMC

**Namespace:** `stats.qmc.*` — 1 ✅ + 0 ⚠️ / 8 = 13%

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `haltonset` |  |  |  |  |  |  |  |
| `lhsdesign` |  |  |  |  |  |  |  |
| `lhsnorm` |  |  |  |  |  |  |  |
| `mhsample` |  |  |  |  |  |  | not implemented |
| `qrandstream` |  |  |  |  |  |  | not implemented |
| `slicesample` |  |  |  |  |  |  | not implemented |
| `sobolset` |  |  |  |  |  |  | not implemented |
| `qrand` |  |  |  |  |  |  | not implemented |

### ANOVA / MANOVA / Correlation

**Namespace:** `stats.anova.*` — 2 ✅ + 0 ⚠️ / 9 = 22%

OOP `anova` class and `fitrm` repeated-measures model intentionally omitted; only the legacy function-form entry points (anova1/anova2/anovan) which return F-statistic and p-value tables.

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `anova1` |  |  |  |  |  |  |  |
| `anova2` |  |  |  |  |  |  | partial |
| `anovan` |  |  |  |  |  |  | not implemented |
| `manova1` |  |  |  |  |  |  | not implemented |
| `canoncorr` |  |  |  |  |  |  |  |
| `dummyvar` |  |  |  |  |  |  |  |
| `aoctool` |  |  |  |  |  |  | not implemented |
| `mauchly` |  |  |  |  |  |  | not implemented |
| `epsilon` |  |  |  |  |  |  | not implemented |

### Linear Regression (function-form)

**Namespace:** `stats.regress.*` — 3 ✅ + 0 ⚠️ / 13 = 23%

OOP `fitlm` / `fitlme` / `fitglm` / `LinearModel` / etc. intentionally omitted. Only the legacy command-form entry points that return numerics (coeffs, residuals, CIs).

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `regress` |  |  |  |  |  |  |  |
| `robustfit` |  |  |  |  |  |  |  |
| `lscov` |  |  |  |  |  |  |  |
| `stepwisefit` |  |  |  |  |  |  | not implemented |
| `glmfit` |  |  |  |  |  |  |  |
| `glmval` |  |  |  |  |  |  |  |
| `mvregress` |  |  |  |  |  |  | not implemented |
| `mvregresslike` |  |  |  |  |  |  | not implemented |
| `plsregress` |  |  |  |  |  |  | not implemented |
| `ridge` |  |  |  |  |  |  |  |
| `lasso` |  |  |  |  |  |  |  |
| `lassoglm` |  |  |  |  |  |  |  |
| `polyconf` |  |  |  |  |  |  | not implemented |

### Nonlinear Regression (function-form)

**Namespace:** `stats.nlfit.*` — 0 ✅ + 0 ⚠️ / 5 = 0%

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `nlinfit` |  |  |  |  |  |  |  |
| `nlparci` |  |  |  |  |  |  |  |
| `nlpredci` |  |  |  |  |  |  |  |
| `statset` |  |  |  |  |  |  | not implemented |
| `statget` |  |  |  |  |  |  | not implemented |

### Distance Metrics

**Namespace:** `stats.cluster.*` — 4 ✅ + 0 ⚠️ / 4 = 100%

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `pdist` |  |  |  |  |  |  |  |
| `pdist2` |  |  |  |  |  |  |  |
| `squareform` |  |  |  |  |  |  |  |
| `mahal` |  |  |  |  |  |  |  |

### Hierarchical Clustering

**Namespace:** `stats.cluster.*` — 5 ✅ + 0 ⚠️ / 7 = 71%

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `linkage` |  |  |  |  |  |  |  |
| `cluster` |  |  |  |  |  |  |  |
| `clusterdata` |  |  |  |  |  |  |  |
| `cophenet` |  |  |  |  |  |  |  |
| `inconsistent` |  |  |  |  |  |  |  |
| `dendrogram` |  |  |  |  |  |  | not implemented |
| `optimalleaforder` |  |  |  |  |  |  | not implemented |

### Partitional Clustering

**Namespace:** `stats.cluster.*` — 3 ✅ + 0 ⚠️ / 4 = 75%

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `kmeans` |  |  |  |  |  |  |  |
| `kmedoids` |  |  |  |  |  |  |  |
| `dbscan` |  |  |  |  |  |  |  |
| `spectralcluster` |  |  |  |  |  |  | not implemented |

### Cluster Evaluation

**Namespace:** `stats.cluster_eval.*` — 0 ✅ + 0 ⚠️ / 3 = 0%

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `silhouette` |  |  |  |  |  |  |  |
| `evalclusters` |  |  |  |  |  |  | not implemented |
| `manovacluster` |  |  |  |  |  |  | not implemented |

### Nearest Neighbors (function-form)

**Namespace:** `stats.knn.*` — 0 ✅ + 0 ⚠️ / 3 = 0%

OOP `KDTreeSearcher` / `ExhaustiveSearcher` / `hnswSearcher` intentionally omitted; flat function form only.

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `knnsearch` |  |  |  |  |  |  |  |
| `rangesearch` |  |  |  |  |  |  |  |
| `createns` |  |  |  |  |  |  | not implemented |

### Hidden Markov Models

**Namespace:** `stats.hmm.*` — 0 ✅ + 0 ⚠️ / 5 = 0%

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `hmmdecode` |  |  |  |  |  |  | not implemented |
| `hmmestimate` |  |  |  |  |  |  | not implemented |
| `hmmgenerate` |  |  |  |  |  |  | not implemented |
| `hmmtrain` |  |  |  |  |  |  | not implemented |
| `hmmviterbi` |  |  |  |  |  |  | not implemented |

### Dimensionality Reduction

**Namespace:** `stats.dim.*` — 3 ✅ + 0 ⚠️ / 8 = 38%

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `pca` |  |  |  |  |  |  |  |
| `pcacov` |  |  |  |  |  |  |  |
| `pcares` |  |  |  |  |  |  |  |
| `ppca` |  |  |  |  |  |  | not implemented |
| `factoran` |  |  |  |  |  |  | not implemented |
| `rica` |  |  |  |  |  |  | not implemented |
| `sparsefilt` |  |  |  |  |  |  | not implemented |
| `tsne` |  |  |  |  |  |  | not implemented |

### Feature Selection (function-form)

**Namespace:** `stats.fselect.*` — 0 ✅ + 0 ⚠️ / 9 = 0%

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `fscchi2` |  |  |  |  |  |  | not implemented |
| `fscmrmr` |  |  |  |  |  |  | not implemented |
| `fscnca` |  |  |  |  |  |  | not implemented |
| `fsrftest` |  |  |  |  |  |  | not implemented |
| `fsrmrmr` |  |  |  |  |  |  | not implemented |
| `fsrnca` |  |  |  |  |  |  | not implemented |
| `fsulaplacian` |  |  |  |  |  |  | not implemented |
| `relieff` |  |  |  |  |  |  | not implemented |
| `sequentialfs` |  |  |  |  |  |  | not implemented |

### Linear Discriminant Analysis (function-form)

**Namespace:** `stats.lda.*` — 1 ✅ + 0 ⚠️ / 1 = **100%**

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `classify` |  |  |  |  |  |  |  |

## Wavelet

### Continuous Wavelet Transforms

**Namespace:** `wavelet.cwt.*` — 0 ✅ + 0 ⚠️ / 16 = 0%

`cwtfilterbank` (class) and the deep-learning layer family
(`cwtLayer`/`icwtLayer`/`dlcwt`/etc.) intentionally omitted.

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `cwt` |  |  |  |  |  |  | not implemented |
| `icwt` |  |  |  |  |  |  | not implemented |
| `cwtfreqbounds` |  |  |  |  |  |  | not implemented |
| `centfrq` |  |  |  |  |  |  | not implemented |
| `scal2frq` |  |  |  |  |  |  | not implemented |
| `wcoherence` |  |  |  |  |  |  | not implemented |
| `wsst` |  |  |  |  |  |  | not implemented |
| `iwsst` |  |  |  |  |  |  | not implemented |
| `wsstridge` |  |  |  |  |  |  | not implemented |
| `wtmm` |  |  |  |  |  |  | not implemented |
| `wavefun` |  |  |  |  |  |  | not implemented |
| `wavefun2` |  |  |  |  |  |  | not implemented |
| `wavsupport` |  |  |  |  |  |  | not implemented |
| `qfactor` |  |  |  |  |  |  | not implemented |
| `wavemngr` |  |  |  |  |  |  | not implemented |
| `waveinfo` |  |  |  |  |  |  | not implemented |

### Discrete Wavelet Transforms (1-D)

**Namespace:** `wavelet.dwt.*` — 14 ✅ + 0 ⚠️ / 18 = 78%

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `dwt` |  |  |  |  |  |  |  |
| `idwt` |  |  |  |  |  |  |  |
| `wavedec` |  |  |  |  |  |  |  |
| `waverec` |  |  |  |  |  |  |  |
| `appcoef` |  |  |  |  |  |  |  |
| `detcoef` |  |  |  |  |  |  |  |
| `wrcoef` |  |  |  |  |  |  |  |
| `dwtmode` |  |  |  |  |  |  | not implemented |
| `dyaddown` |  |  |  |  |  |  |  |
| `dyadup` |  |  |  |  |  |  |  |
| `wkeep` |  |  |  |  |  |  |  |
| `wextend` |  |  |  |  |  |  |  |
| `wcodemat` |  |  |  |  |  |  |  |
| `haart` |  |  |  |  |  |  |  |
| `ihaart` |  |  |  |  |  |  |  |
| `wmaxlev` |  |  |  |  |  |  |  |
| `dwpt` |  |  |  |  |  |  | not implemented |
| `idwpt` |  |  |  |  |  |  | not implemented |

### Discrete Wavelet Transforms (2-D / 3-D)

**Namespace:** `wavelet.dwt2.*` — 2 ✅ + 0 ⚠️ / 15 = 13%

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `dwt2` |  |  |  |  |  |  |  |
| `idwt2` |  |  |  |  |  |  |  |
| `wavedec2` |  |  |  |  |  |  | not implemented |
| `waverec2` |  |  |  |  |  |  | not implemented |
| `appcoef2` |  |  |  |  |  |  | not implemented |
| `detcoef2` |  |  |  |  |  |  | not implemented |
| `wrcoef2` |  |  |  |  |  |  | not implemented |
| `wpdec2` |  |  |  |  |  |  | not implemented |
| `wprec2` |  |  |  |  |  |  | not implemented |
| `haart2` |  |  |  |  |  |  | not implemented |
| `ihaart2` |  |  |  |  |  |  | not implemented |
| `wavedec3` |  |  |  |  |  |  | not implemented |
| `waverec3` |  |  |  |  |  |  | not implemented |
| `dwt3` |  |  |  |  |  |  | not implemented |
| `idwt3` |  |  |  |  |  |  | not implemented |

### Stationary, MODWT, and Wavelet Packets

**Namespace:** `wavelet.swt_modwt.*` — 4 ✅ + 0 ⚠️ / 17 = 24%

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `swt` |  |  |  |  |  |  |  |
| `iswt` |  |  |  |  |  |  |  |
| `swt2` |  |  |  |  |  |  | not implemented |
| `iswt2` |  |  |  |  |  |  | not implemented |
| `modwt` |  |  |  |  |  |  |  |
| `imodwt` |  |  |  |  |  |  |  |
| `modwtmra` |  |  |  |  |  |  | not implemented |
| `modwtcorr` |  |  |  |  |  |  | not implemented |
| `modwtvar` |  |  |  |  |  |  | not implemented |
| `modwtxcorr` |  |  |  |  |  |  | not implemented |
| `modwpt` |  |  |  |  |  |  | not implemented |
| `imodwpt` |  |  |  |  |  |  | not implemented |
| `wpdec` |  |  |  |  |  |  | not implemented |
| `wprec` |  |  |  |  |  |  | not implemented |
| `wpcoef` |  |  |  |  |  |  | not implemented |
| `wprcoef` |  |  |  |  |  |  | not implemented |
| `besttree` |  |  |  |  |  |  | not implemented |

### Denoising and Compression

**Namespace:** `wavelet.denoise.*` — 3 ✅ + 0 ⚠️ / 16 = 19%

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `wdenoise` |  |  |  |  |  |  |  |
| `wdenoise2` |  |  |  |  |  |  | not implemented |
| `wden` |  |  |  |  |  |  | not implemented |
| `wdencmp` |  |  |  |  |  |  | not implemented |
| `wpdencmp` |  |  |  |  |  |  | not implemented |
| `wnoisest` |  |  |  |  |  |  |  |
| `wvarchg` |  |  |  |  |  |  | not implemented |
| `ddencmp` |  |  |  |  |  |  | not implemented |
| `thselect` |  |  |  |  |  |  | not implemented |
| `wthcoef` |  |  |  |  |  |  | not implemented |
| `wthcoef2` |  |  |  |  |  |  | not implemented |
| `wthresh` |  |  |  |  |  |  |  |
| `wmulden` |  |  |  |  |  |  | not implemented |
| `measerr` |  |  |  |  |  |  | not implemented |
| `wnoise` |  |  |  |  |  |  | not implemented |
| `wcompress` |  |  |  |  |  |  | not implemented |

### Filter Banks and Wavelet Families

**Namespace:** `wavelet.filt.*` — 7 ✅ + 0 ⚠️ / 22 = 32%

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `wfilters` |  |  |  |  |  |  |  |
| `orthfilt` |  |  |  |  |  |  |  |
| `qmf` |  |  |  |  |  |  |  |
| `biorfilt` |  |  |  |  |  |  | not implemented |
| `dbwavf` |  |  |  |  |  |  |  |
| `coifwavf` |  |  |  |  |  |  |  |
| `symwavf` |  |  |  |  |  |  |  |
| `dbaux` |  |  |  |  |  |  | not implemented |
| `symaux` |  |  |  |  |  |  | not implemented |
| `biorwavf` |  |  |  |  |  |  | not implemented |
| `rbiowavf` |  |  |  |  |  |  | not implemented |
| `fejerkorovkin` |  |  |  |  |  |  | not implemented |
| `mbscalf` |  |  |  |  |  |  | not implemented |
| `hanscalf` |  |  |  |  |  |  | not implemented |
| `blscalf` |  |  |  |  |  |  | not implemented |
| `bswfun` |  |  |  |  |  |  | not implemented |
| `wrev` |  |  |  |  |  |  |  |
| `isbiorthwfb` |  |  |  |  |  |  | not implemented |
| `isorthwfb` |  |  |  |  |  |  | not implemented |
| `wavelets` |  |  |  |  |  |  | not implemented |
| `waveletfamilies` |  |  |  |  |  |  | not implemented |
| `wavenames` |  |  |  |  |  |  | not implemented |

### Continuous Wavelet Shapes

**Namespace:** `wavelet.shape.*` — 8 ✅ + 0 ⚠️ / 11 = 73%

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `meyer` |  |  |  |  |  |  | not implemented |
| `meyeraux` |  |  |  |  |  |  |  |
| `mexihat` |  |  |  |  |  |  |  |
| `morlet` |  |  |  |  |  |  |  |
| `cgauwavf` |  |  |  |  |  |  |  |
| `cmorwavf` |  |  |  |  |  |  |  |
| `fbspwavf` |  |  |  |  |  |  |  |
| `gauswavf` |  |  |  |  |  |  |  |
| `intwave` |  |  |  |  |  |  | not implemented |
| `pat2cwav` |  |  |  |  |  |  | not implemented |
| `shanwavf` |  |  |  |  |  |  |  |

### Lifting

**Namespace:** `wavelet.lift.*` — 0 ✅ + 0 ⚠️ / 6 = 0%

`liftingScheme` and `liftingStep` are MATLAB classes; we treat lifting
as a pair of flat decomposition / reconstruction functions.

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `lwt` |  |  |  |  |  |  | not implemented |
| `ilwt` |  |  |  |  |  |  | not implemented |
| `lwt2` |  |  |  |  |  |  | not implemented |
| `ilwt2` |  |  |  |  |  |  | not implemented |
| `lwtcoef` |  |  |  |  |  |  | not implemented |
| `lwtcoef2` |  |  |  |  |  |  | not implemented |

### Decomposition Trees and Misc

**Namespace:** `wavelet.misc.*` — 0 ✅ + 0 ⚠️ / 13 = 0%

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `dualtree` |  |  |  |  |  |  | not implemented |
| `idualtree` |  |  |  |  |  |  | not implemented |
| `dualtree2` |  |  |  |  |  |  | not implemented |
| `idualtree2` |  |  |  |  |  |  | not implemented |
| `dddtree` |  |  |  |  |  |  | not implemented |
| `idddtree` |  |  |  |  |  |  | not implemented |
| `tqwt` |  |  |  |  |  |  | not implemented |
| `itqwt` |  |  |  |  |  |  | not implemented |
| `wfbm` |  |  |  |  |  |  | not implemented |
| `wfbmesti` |  |  |  |  |  |  | not implemented |
| `wfusimg` |  |  |  |  |  |  | not implemented |
| `wfusmat` |  |  |  |  |  |  | not implemented |
| `wentropy` |  |  |  |  |  |  | not implemented |

## Misc / not in TODO

Functions benched by the harness that don't appear in any of the MATLAB-doc sections above. Move them into a real section if they correspond to a documented MATLAB function.

| function | nk small (ms) | ML× s | OC× s | nk large (ms) | ML× l | OC× l | notes |
|---|---:|---:|---:|---:|---:|---:|---|
| `impyramid_expand` |  |  |  |  |  |  |  |
| `axes2pix` |  |  |  |  |  |  |  |
| `isgray` |  |  |  |  |  |  |  |
| `imcast` |  |  |  |  |  |  |  |
| `mmgradm` |  |  |  |  |  |  |  |
| `fchcode` |  |  |  |  |  |  |  |
| `fftconv2` |  |  |  |  |  |  |  |
| `wavelength2rgb` |  |  |  |  |  |  |  |
| `imsmooth` |  |  |  |  |  |  |  |
| `colorgradient` |  |  |  |  |  |  |  |
| `iscolormap` |  |  |  |  |  |  |  |
| `gray` |  |  |  |  |  |  |  |
| `hot` |  |  |  |  |  |  |  |
| `cool` |  |  |  |  |  |  |  |
| `spring` |  |  |  |  |  |  |  |
| `summer` |  |  |  |  |  |  |  |
| `autumn` |  |  |  |  |  |  |  |
| `winter` |  |  |  |  |  |  |  |
| `copper` |  |  |  |  |  |  |  |
| `pink` |  |  |  |  |  |  |  |
| `hsv` |  |  |  |  |  |  |  |
| `flag` |  |  |  |  |  |  |  |
| `prism` |  |  |  |  |  |  |  |
| `lines` |  |  |  |  |  |  |  |
| `bone` |  |  |  |  |  |  |  |
| `white` |  |  |  |  |  |  |  |
| `brighten` |  |  |  |  |  |  |  |
| `contrast` |  |  |  |  |  |  |  |
| `cdf_upper` |  |  |  |  |  |  |  |
| `windows_sflag` |  |  |  |  |  |  |  |
| `kstest_extras` |  |  |  |  |  |  |  |
| `ttest_extras` |  |  |  |  |  |  |  |
| `vartest_extras` |  |  |  |  |  |  |  |
| `ztest_extras` |  |  |  |  |  |  |  |
| `logical` |  |  |  |  |  |  |  |
| `islogical` |  |  |  |  |  |  |  |
| `smoothdata` |  |  |  |  |  |  |  |
| `sosfiltfilt` |  |  |  |  |  |  |  |
| `magic` |  |  |  |  |  |  |  |
| `toeplitz` |  |  |  |  |  |  |  |
| `hankel` |  |  |  |  |  |  |  |
| `vander` |  |  |  |  |  |  |  |
| `compan` |  |  |  |  |  |  |  |
| `pascal` |  |  |  |  |  |  |  |
| `hilb` |  |  |  |  |  |  |  |
| `invhilb` |  |  |  |  |  |  |  |
| `wilkinson` |  |  |  |  |  |  |  |
| `hadamard` |  |  |  |  |  |  |  |
| `rosser` |  |  |  |  |  |  |  |
| `cputime` |  |  |  |  |  |  |  |
| `isoutlier` |  |  |  |  |  |  |  |
| `rmoutliers` |  |  |  |  |  |  |  |
| `standardizeMissing` |  |  |  |  |  |  |  |
| `detrend` |  |  |  |  |  |  |  |
| `fitdist` |  |  |  |  |  |  |  |
| `now` |  |  |  |  |  |  |  |
| `datenum` |  |  |  |  |  |  |  |
| `weekday` |  |  |  |  |  |  |  |
| `juliandate` |  |  |  |  |  |  |  |
| `eomday` |  |  |  |  |  |  |  |
| `datevec` |  |  |  |  |  |  |  |
| `yyyymmdd` |  |  |  |  |  |  |  |
| `mjuliandate` |  |  |  |  |  |  |  |
| `predicates` |  |  |  |  |  |  |  |
| `rref_rcond_planerot` |  |  |  |  |  |  |  |
| `lsqminnorm_lsqnonneg` |  |  |  |  |  |  |  |
| `base_conversions` |  |  |  |  |  |  |  |
| `sigroi_utils` |  |  |  |  |  |  |  |
| `color_extras` |  |  |  |  |  |  |  |
| `filter_design` |  |  |  |  |  |  |  |
| `sig_utils` |  |  |  |  |  |  |  |
| `signal_buffer` |  |  |  |  |  |  |  |
| `signal_uquant` |  |  |  |  |  |  |  |
| `signal_polyutils` |  |  |  |  |  |  |  |
| `signal_shiftdata` |  |  |  |  |  |  |  |
| `signal_kaiserord` |  |  |  |  |  |  |  |
| `signal_ellipord` |  |  |  |  |  |  |  |
| `signal_firpmord` |  |  |  |  |  |  |  |
| `signal_vco` |  |  |  |  |  |  |  |
| `signal_fir2` |  |  |  |  |  |  |  |
| `signal_cell2sos` |  |  |  |  |  |  |  |
| `signal_ctfutils` |  |  |  |  |  |  |  |
| `signal_modulate` |  |  |  |  |  |  |  |
| `signal_demod` |  |  |  |  |  |  |  |
| `signal_firpm` |  |  |  |  |  |  |  |
| `signal_fftn` |  |  |  |  |  |  |  |
| `signal_czt` |  |  |  |  |  |  |  |
| `signal_stft` |  |  |  |  |  |  |  |
| `image_adapthisteq` |  |  |  |  |  |  |  |
| `image_graycomatrix` |  |  |  |  |  |  |  |
| `image_bwmorph` |  |  |  |  |  |  |  |
| `signal_polyscale` |  |  |  |  |  |  |  |
| `signal_polystab` |  |  |  |  |  |  |  |
| `signal_scalefiltersections` |  |  |  |  |  |  |  |
| `page_family` |  |  |  |  |  |  |  |
| `schur_convert` |  |  |  |  |  |  |  |
| `cond_pnorm` |  |  |  |  |  |  |  |
| `predicates_sym` |  |  |  |  |  |  |  |
| `predicates_band` |  |  |  |  |  |  |  |
| `animatedline` |  |  |  |  |  |  |  |
| `ode23(f, tspan, y0)            scalar IVP, explicit tspan` |  |  |  |  |  |  |  |
| `ode23(f, tspan, y0, opts)      with odeset() opts` |  |  |  |  |  |  |  |
| `RelTol / AbsTol propagation` |  |  |  |  |  |  |  |
| `cubic Hermite dense-output at explicit tspan points` |  |  |  |  |  |  |  |
| `[t, y] output shape  (n × 1, n × d)` |  |  |  |  |  |  |  |
| `illumwhite(A)              default P = 1` |  |  |  |  |  |  |  |
| `illumwhite(A, 0)           per-channel max` |  |  |  |  |  |  |  |
| `illumwhite(A, 5)           top-5%-by-channel` |  |  |  |  |  |  |  |
| `illumgray(A)               default p_lo = p_hi = 1, Norm = 1` |  |  |  |  |  |  |  |
| `illumgray(A, [pl ph])      vector percentile` |  |  |  |  |  |  |  |
| `illumgray(A, p, 'Norm', n) custom p-norm exponent` |  |  |  |  |  |  |  |
| `illumpca(A)              default p = 3.5` |  |  |  |  |  |  |  |
| `illumpca(A, p)           tail-fraction p in (0, 50]` |  |  |  |  |  |  |  |
| `illumpca(A, 50)          use all pixels` |  |  |  |  |  |  |  |
| `imcolordiff(I1, I2)               CIE94 default, RGB input` |  |  |  |  |  |  |  |
| `imcolordiff(.., 'Standard', .)    CIEDE2000 alternate` |  |  |  |  |  |  |  |
| `imcolordiff(.., 'isInputLab', .)  Lab input skips rgb2lab` |  |  |  |  |  |  |  |
| `imcolordiff with non-default kL/kC/kH/K1/K2 (gtest only)` |  |  |  |  |  |  |  |
| `otf2psf(otf)                no outsize, 3-D / 4-D / odd / even / 1-D` |  |  |  |  |  |  |  |
| `otf2psf(otf, outsize)       crop to top-left after floor(outsize/2) shift` |  |  |  |  |  |  |  |
| `otf2psf(otf, [1 N])         1-D crop` |  |  |  |  |  |  |  |
| `rgbwide2ycbcr(rgb, 10)      BT.2020 10-bit narrow-range` |  |  |  |  |  |  |  |
| `rgbwide2ycbcr(rgb, 12)      BT.2020 12-bit narrow-range` |  |  |  |  |  |  |  |
| `ycbcr2rgbwide(ycbcr, 10)      BT.2020 10-bit narrow-range decode` |  |  |  |  |  |  |  |
| `ycbcr2rgbwide(ycbcr, 12)      BT.2020 12-bit narrow-range decode` |  |  |  |  |  |  |  |
| `H×W×3 image input` |  |  |  |  |  |  |  |
| `rgbwide2ycbcr round-trip (10/12-bit)` |  |  |  |  |  |  |  |
| `ind2gray(X, MAP)            double / single X — 1-based lookup` |  |  |  |  |  |  |  |
| `ind2gray(uint8 X, MAP)      class-preserving uint8 output via 256-LUT` |  |  |  |  |  |  |  |
| `ind2gray(uint16 X, MAP)     class-preserving uint16 output via 65536-LUT` |  |  |  |  |  |  |  |
| `out-of-range index clamping (both float and integer)` |  |  |  |  |  |  |  |
| `imcrop3(V, cuboid)         3-D volume crop` |  |  |  |  |  |  |  |
| `imcrop3 with rounding      non-integer cuboid limits` |  |  |  |  |  |  |  |
| `imcrop3 on 4-D volume      4th dim (channels / time) passes through` |  |  |  |  |  |  |  |
| `class-preserving output    uint8 in → uint8 out` |  |  |  |  |  |  |  |
| `graydiffweight(I, refGrayVal)         scalar reference` |  |  |  |  |  |  |  |
| `graydiffweight(I, MASK)                mean over masked pixels` |  |  |  |  |  |  |  |
| `graydiffweight(I, C, R)                mean over indexed pixels` |  |  |  |  |  |  |  |
| `RolloffFactor (default 0.5)            controls falloff` |  |  |  |  |  |  |  |
| `GrayDifferenceCutoff (default Inf)     hard threshold` |  |  |  |  |  |  |  |
| `3-D volume input` |  |  |  |  |  |  |  |
| `nlfilter(A, [m n], fun)         default zero-padding` |  |  |  |  |  |  |  |
| `nlfilter(A, 'indexed', ...)     padval=1 for double/single, 0 otherwise` |  |  |  |  |  |  |  |
| `Class preservation              output class = first fun() return class` |  |  |  |  |  |  |  |
| `Even neighbourhood              [2 3] with top-left bias for the centre` |  |  |  |  |  |  |  |
| `colfilt(A, [m n], 'sliding', fun)       zero-padding` |  |  |  |  |  |  |  |
| `colfilt(A, [m n], 'distinct', fun)      shape-preserving distinct` |  |  |  |  |  |  |  |
| `even neighbourhood [2 3]                 in sliding mode` |  |  |  |  |  |  |  |
| `'indexed' padding                        padval=1 for double` |  |  |  |  |  |  |  |
| `nlfilter ↔ colfilt equivalence           same output for shared kernels` |  |  |  |  |  |  |  |
| `deconvwnr(I, PSF, NSR=0)             ideal inverse` |  |  |  |  |  |  |  |
| `deconvwnr(I, PSF, NSR=0.01)          regularised` |  |  |  |  |  |  |  |
| `deconvwnr(I, PSF, NSR=0.1)           strongly regularised` |  |  |  |  |  |  |  |
| `deconvwnr(I, PSF, NCORR, ICORR)      scalar NCORR/ICORR ≡ NSR` |  |  |  |  |  |  |  |
| `uint8 input — class-preserving       saturating cast on output` |  |  |  |  |  |  |  |
| `edgetaper(I, PSF)             double image` |  |  |  |  |  |  |  |
| `uint8 input — class-preserving` |  |  |  |  |  |  |  |
| `constant image → J ≡ I        (alpha-symmetric edge case)` |  |  |  |  |  |  |  |
| `tonemap` |  |  |  |  |  |  |  |
| `raw2planar` |  |  |  |  |  |  |  |
| `planar2raw` |  |  |  |  |  |  |  |
| `filloutliers` |  |  |  |  |  |  |  |
| `signal_iscola` |  |  |  |  |  |  |  |
| `signal_fwht` |  |  |  |  |  |  |  |
| `image_integralBoxFilter` |  |  |  |  |  |  |  |
| `image_imread_tiff` |  |  |  |  |  |  |  |
| `image_modefilt` |  |  |  |  |  |  |  |
| `image_fspecial3` |  |  |  |  |  |  |  |
| `image_integralBoxFilter3` |  |  |  |  |  |  |  |
| `image_makelut` |  |  |  |  |  |  |  |
| `image_bwmorph3` |  |  |  |  |  |  |  |
| `image_reducepoly` |  |  |  |  |  |  |  |
| `image_roifilt2` |  |  |  |  |  |  |  |
| `padarray_dir` |  |  |  |  |  |  |  |
| `imrotate_bbox` |  |  |  |  |  |  |  |
| `xcov_maxlag` |  |  |  |  |  |  |  |
| `trapz` |  |  |  |  |  |  |  |
| `normalize` |  |  |  |  |  |  |  |
| `rescale` |  |  |  |  |  |  |  |
| `histc` |  |  |  |  |  |  |  |
| `dec2base` |  |  |  |  |  |  |  |
| `cumtrapz` |  |  |  |  |  |  |  |
| `histcounts_norm` |  |  |  |  |  |  |  |
| `mat2str` |  |  |  |  |  |  |  |
| `nanvar` |  |  |  |  |  |  |  |
| `nanstd` |  |  |  |  |  |  |  |
| `sortrows_complex` |  |  |  |  |  |  |  |
| `abs_integer` |  |  |  |  |  |  |  |
| `sign_integer` |  |  |  |  |  |  |  |
| `sign_complex` |  |  |  |  |  |  |  |
| `wrap_angle` |  |  |  |  |  |  |  |
| `interp1_extrap` |  |  |  |  |  |  |  |
| `sprintf_string` |  |  |  |  |  |  |  |
| `histcounts_binedges` |  |  |  |  |  |  |  |
| `sprintf_width` |  |  |  |  |  |  |  |
| `sprintf_int_fallback` |  |  |  |  |  |  |  |
| `mink_maxk_index` |  |  |  |  |  |  |  |
| `zscore_musigma` |  |  |  |  |  |  |  |
| `find_rowcol` |  |  |  |  |  |  |  |
| `sprintf_inf_nan` |  |  |  |  |  |  |  |
| `unique_orientation` |  |  |  |  |  |  |  |
| `setops_indices` |  |  |  |  |  |  |  |
| `corrcoef_pvalues` |  |  |  |  |  |  |  |
| `regexp_multiout` |  |  |  |  |  |  |  |
| `strsplit_matches` |  |  |  |  |  |  |  |
| `sortrows_direction` |  |  |  |  |  |  |  |
| `num2str_format` |  |  |  |  |  |  |  |
| `int2str` |  |  |  |  |  |  |  |
| `validatestring` |  |  |  |  |  |  |  |
| `match_cell_patterns` |  |  |  |  |  |  |  |
| `isvarname` |  |  |  |  |  |  |  |
| `erase_count_cell` |  |  |  |  |  |  |  |
| `replace_cell` |  |  |  |  |  |  |  |
| `grp2idx` |  |  |  |  |  |  |  |
| `datestr` |  |  |  |  |  |  |  |
| `datenum_string` |  |  |  |  |  |  |  |
| `datevec_string` |  |  |  |  |  |  |  |
| `calendar` |  |  |  |  |  |  |  |
| `etime` |  |  |  |  |  |  |  |
| `weeknum` |  |  |  |  |  |  |  |
| `addtodate` |  |  |  |  |  |  |  |
| `partialcorr_rows` |  |  |  |  |  |  |  |
| `abs` | 0.000128 | 44.06× |  | 0.2497 | 0.42× |  |  |
| `sign` | 0.004368 | 0.92× |  | 5.979 | 0.02× |  |  |
