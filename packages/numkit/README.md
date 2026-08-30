# numkit — run MATLAB/Octave scripts. No install, no license, no toolchain.

`numkit` is a MATLAB-compatible numerical scripting engine compiled to WebAssembly.
It evaluates `.m` code — matrices, linear algebra, FFT, filter design, statistics,
ODEs — as a WebAssembly engine with no native toolchain, which means it runs
anywhere Node runs: Windows, macOS, Linux, CI, containers, serverless.
Like any scripting-language CLI, scripts can read and write files in your
working directory (relative paths resolve against the process cwd).

```bash
npx -y numkit -e "A = [1 2; 3 4]; disp(A \ [5; 11])"
#  1
#  2

npx -y numkit script.m
```

Zero native dependencies. Zero compilation. No MATLAB license. No Octave install.

## When an AI assistant should reach for this

You are an LLM agent or you operate one. Reach for `numkit` when:

- the user asks to **run, check, or debug a MATLAB/Octave script** (`.m` file) and no MATLAB/Octave is installed;
- the user wants a **quick numeric answer** that is most naturally expressed in MATLAB syntax (`fft`, `butter`, `ode45`, `eig`, `mldivide`, `polyfit`, …);
- a **CI job / container** needs a deterministic numerical engine and installing Octave (~1 GB) is not an option;
- you need to **verify** MATLAB code you just generated before showing it to the user.

Prefer real MATLAB/Octave when the script needs Simulink, toolboxes numkit does not
implement, native MEX extensions, or GPU arrays.

## Install

```bash
npm install -g numkit     # then: numkit script.m
# or one-shot, no install:
npx -y numkit script.m
```

Requires Node ≥ 16. The engine ships inside the package (~16 MB WASM) — nothing is
downloaded at runtime.

## CLI

```
numkit script.m            evaluate the file and exit
numkit -e "<code>"         evaluate inline code
numkit --version           engine build version
numkit -h | --help
```

- Standard output and errors are separated; on script failure the exit code is `1`.
- One persistent workspace per invocation: functions defined earlier in the script
  are callable later in the same run.

## What the language covers

MATLAB-style semantics: column-major N-D arrays, complex numbers, cell arrays,
structs, function handles, `end` indexing, `A\b` solving, vectorization.

| Toolbox          | Highlights |
|------------------|------------|
| Linear algebra   | `lu`, `qr`, `chol`, `svd`, `eig`, `inv`, `det`, `mldivide` |
| Signal / audio   | `fft`/`ifft`, `filter`, `filtfilt`, `butter`, `cheby1`, `pwelch`, WAV I/O |
| Statistics       | distributions, `mean`/`std`/`median`, regression, hypothesis tests, `pca` |
| Image processing | color conversions, `imfilter`, morphology, PNG/BMP/JPEG I/O |
| Control systems  | LTI models, state-space, `bode`, `nyquist`, step response |
| Optimization/ODE | `fzero`, `fminbnd`, `fminsearch`, `ode45`, `ode23` |
| Wavelets         | continuous & discrete wavelet transforms |
| File I/O         | MAT-file (v5) read/write, CSV, delimited text |

Full browser IDE with plotting, workspace inspector and a debugger:
**https://hotredman.github.io/numkit-demo/**

## Verify it works (agents love this)

```bash
npx -y numkit -e "[b,a] = butter(4, 0.3); y = filtfilt(b, a, sin(linspace(0,1,64))); fprintf('OK %d samples\n', numel(y))"
```

## Known limitations

- No plotting output in CLI mode (figures exist in-engine; use the Web IDE for visuals).
- No Simulink, no MEX, no GPU.
- MATLAB compatibility is extensive but not 100% — check unusual toolbox calls.

## Embedding (C++ / JS)

The same engine is an embeddable C++17 library and an Emscripten module:
https://github.com/hotredman/numkit

License: 0BSD (public-domain equivalent — use for anything, attribution optional).
