% Benchmark SIMD — Library function performance
% Exercises the MATLAB functions that numkit-m vectorised via Google
% Highway in the Phase 8 work: abs, sin, cos, exp, log, + - .* ./,
% matrix multiply, and FFT. Works identically in MATLAB, Octave, and
% the numkit-m IDE — run it in each and compare totals to see where
% the interpreter + library stack stands against the reference
% implementations.
%
% Sizes are tuned so a single kernel call does meaningful work
% (≥ a few ms on typical hardware) and the whole script finishes
% in 10–30 s on a modern desktop.
clear

fprintf('\n=== SIMD Library Benchmark ===\n\n')

N   = 1000000;   % elementwise length
Mm  = 512;       % matrix multiply side
Nf  = 32768;     % FFT length
Re  = 20;        % repetitions for elementwise ops
Rm  = 3;         % repetitions for mtimes
Rf  = 50;        % repetitions for fft

% Inputs reused across rows. randn is comparable across interpreters
% even though RNG streams differ — we only care about wall time, not
% the output values.
x = randn(N, 1);
y = randn(N, 1);

% ── 1. abs ─────────────────────────────────────────────────
tic
for k = 1:Re
    z = abs(x);
end
t_abs = toc;
fprintf(' 1. abs(x)          N=%7d  reps=%d : %.4f s\n', N, Re, t_abs)

% ── 2. sin ─────────────────────────────────────────────────
tic
for k = 1:Re
    z = sin(x);
end
t_sin = toc;
fprintf(' 2. sin(x)          N=%7d  reps=%d : %.4f s\n', N, Re, t_sin)

% ── 3. cos ─────────────────────────────────────────────────
tic
for k = 1:Re
    z = cos(x);
end
t_cos = toc;
fprintf(' 3. cos(x)          N=%7d  reps=%d : %.4f s\n', N, Re, t_cos)

% ── 4. exp ─────────────────────────────────────────────────
tic
for k = 1:Re
    z = exp(x);
end
t_exp = toc;
fprintf(' 4. exp(x)          N=%7d  reps=%d : %.4f s\n', N, Re, t_exp)

% ── 5. log ─────────────────────────────────────────────────
% log needs positive input; shift randn onto the positive half-line.
xp = abs(x) + 1;
tic
for k = 1:Re
    z = log(xp);
end
t_log = toc;
fprintf(' 5. log(x)          N=%7d  reps=%d : %.4f s\n', N, Re, t_log)

% ── 6. plus (x + y) ────────────────────────────────────────
tic
for k = 1:Re
    z = x + y;
end
t_plus = toc;
fprintf(' 6. x + y           N=%7d  reps=%d : %.4f s\n', N, Re, t_plus)

% ── 7. minus (x - y) ───────────────────────────────────────
tic
for k = 1:Re
    z = x - y;
end
t_minus = toc;
fprintf(' 7. x - y           N=%7d  reps=%d : %.4f s\n', N, Re, t_minus)

% ── 8. times (x .* y) ──────────────────────────────────────
tic
for k = 1:Re
    z = x .* y;
end
t_times = toc;
fprintf(' 8. x .* y          N=%7d  reps=%d : %.4f s\n', N, Re, t_times)

% ── 9. rdivide (x ./ y) ────────────────────────────────────
tic
for k = 1:Re
    z = x ./ y;
end
t_rdiv = toc;
fprintf(' 9. x ./ y          N=%7d  reps=%d : %.4f s\n', N, Re, t_rdiv)

% ── 10. mtimes (A * B) ────────────────────────────────────
A = randn(Mm, Mm);
B = randn(Mm, Mm);
tic
for k = 1:Rm
    C = A * B;
end
t_mm = toc;
fprintf('10. A * B           %3dx%d   reps=%d : %.4f s\n', Mm, Mm, Rm, t_mm)

% ── 11. fft ───────────────────────────────────────────────
s = randn(Nf, 1);
tic
for k = 1:Rf
    F = fft(s);
end
t_fft = toc;
fprintf('11. fft(s)          N=%7d  reps=%d : %.4f s\n', Nf, Rf, t_fft)

% ── Summary ───────────────────────────────────────────────
total = t_abs + t_sin + t_cos + t_exp + t_log + t_plus + t_minus + ...
        t_times + t_rdiv + t_mm + t_fft;
fprintf('\n── Total: %.4f s ──\n', total)
