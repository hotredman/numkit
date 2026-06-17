% iir_filter_ref.m -- cross-engine reference for the biquad interpreter bench.
%
% The SAME 2nd-order IIR (biquad, Direct Form I) as
% benchmarks/interpreter/iir_filter_bench.cpp, written in PORTABLE M-code so the
% identical file can be timed in MATLAB, Octave and numkit to compare the
% scalar-loop cost engine-to-engine. The recursion (y[n] depends on y[n-1],
% y[n-2]) cannot vectorise, so it isolates raw scalar-loop dispatch. Run it in
% each engine:
%   MATLAB : matlab -batch "run('benchmarks/m/iir_filter_ref.m')"
%   Octave : octave-cli benchmarks/m/iir_filter_ref.m
%   numkit : build/desktop-fast/tests/smoke/Release/numkit_smoke.exe \
%                benchmarks/m/iir_filter_ref.m
%
% Reference snapshot (2026-06-17, N = 5e6, ns/sample):
%   biquad loop : MATLAB JIT ~4.0  |  numkit VM ~150 (after loop-opt #1+#2)
%   filter()    : MATLAB ~7.8      |  numkit ~5.5  (numkit beats MATLAB here)
%   native C++ ~1.6  (numkit_bench BM_Biquad_NativeCpp)
% No `import` here so the file stays MATLAB/Octave-portable; filter() is guarded
% by try/catch (numkit resolves it only after `import compat.*`).

clear
N = 5000000;
x = sin(0.01 * (1:N));
b0 = 0.0675; b1 = 0.1349; b2 = 0.0675; a1 = -1.1430; a2 = 0.4128;

% Warm-up run (primes MATLAB's loop JIT / the instruction cache).
y = zeros(1, N); x1 = 0; x2 = 0; y1 = 0; y2 = 0;
for n = 1:N
    xn = x(n);
    yn = b0*xn + b1*x1 + b2*x2 - a1*y1 - a2*y2;
    y(n) = yn;
    x2 = x1; x1 = xn; y2 = y1; y1 = yn;
end

% Timed run.
y = zeros(1, N); x1 = 0; x2 = 0; y1 = 0; y2 = 0;
tic;
for n = 1:N
    xn = x(n);
    yn = b0*xn + b1*x1 + b2*x2 - a1*y1 - a2*y2;
    y(n) = yn;
    x2 = x1; x1 = xn; y2 = y1; y1 = yn;
end
t = toc;
fprintf('biquad loop : %.4f s | %7.2f ns/sample | y(end)=%.6f\n', t, t/N*1e9, y(N));

% filter() builtin -- the C++ kernel doing the same recursion. Built in to
% MATLAB / Octave; in numkit it needs `import compat.*`, so guard it.
try
    b = [b0 b1 b2]; a = [1 a1 a2];
    tic; yf = filter(b, a, x); tf = toc;
    fprintf('filter()    : %.4f s | %7.2f ns/sample | yf(end)=%.6f\n', tf, tf/N*1e9, yf(N));
catch
    fprintf('filter()    : skipped (numkit needs ''import compat.*'' first)\n');
end
