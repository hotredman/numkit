% Benchmark SIMD (inplace) — same kernels as benchmark_simd.m but the
% output buffer `z` is pre-allocated once and written via slice assign
% `z(:) = rhs`. On a sane MATLAB / Octave / numkit-m this should avoid
% the per-iteration output allocation and isolate the cost of the
% actual vectorised op. Cross-check for the hypothesis that the native
% numkit-m slowdown vs WASM on benchmark_simd.m is driven by allocator
% churn around the 8 MB output.
clear

fprintf('\n=== SIMD Library Benchmark (inplace) ===\n\n')

N   = 1000000;
Mm  = 512;
Nf  = 32768;
Re  = 20;
Rm  = 3;
Rf  = 50;

x = randn(N, 1);
y = randn(N, 1);

% Pre-allocated output buffers (real and complex).
z    = zeros(N, 1);
C    = zeros(Mm, Mm);
F    = complex(zeros(Nf, 1), zeros(Nf, 1));

% ── Warm-up — see comment in benchmark_simd.m. JIT / Worker spawn /
% Highway dispatch resolution off the timed clock so the first kernel
% (abs) doesn't get charged with everyone else's first-call cost.
tmp = abs(x);
tmp = sin(x);
tmp = cos(x);
tmp = exp(x);
tmp = log(abs(x) + 1);
tmp = x + y;
tmp = x - y;
tmp = x .* y;
tmp = x ./ y;
A_warm = randn(Mm, Mm);
B_warm = randn(Mm, Mm);
tmp = A_warm * B_warm;
s_warm = randn(Nf, 1);
tmp = fft(s_warm);
clear tmp A_warm B_warm s_warm

% ── 1. abs ─────────────────────────────────────────────────
tic
for k = 1:Re
    z(:) = abs(x);
end
t_abs = toc;
fprintf(' 1. abs(x)          N=%7d  reps=%d : %.4f s\n', N, Re, t_abs)

% ── 2. sin ─────────────────────────────────────────────────
tic
for k = 1:Re
    z(:) = sin(x);
end
t_sin = toc;
fprintf(' 2. sin(x)          N=%7d  reps=%d : %.4f s\n', N, Re, t_sin)

% ── 3. cos ─────────────────────────────────────────────────
tic
for k = 1:Re
    z(:) = cos(x);
end
t_cos = toc;
fprintf(' 3. cos(x)          N=%7d  reps=%d : %.4f s\n', N, Re, t_cos)

% ── 4. exp ─────────────────────────────────────────────────
tic
for k = 1:Re
    z(:) = exp(x);
end
t_exp = toc;
fprintf(' 4. exp(x)          N=%7d  reps=%d : %.4f s\n', N, Re, t_exp)

% ── 5. log ─────────────────────────────────────────────────
xp = abs(x) + 1;
tic
for k = 1:Re
    z(:) = log(xp);
end
t_log = toc;
fprintf(' 5. log(x)          N=%7d  reps=%d : %.4f s\n', N, Re, t_log)

% ── 6. plus ────────────────────────────────────────────────
tic
for k = 1:Re
    z(:) = x + y;
end
t_plus = toc;
fprintf(' 6. x + y           N=%7d  reps=%d : %.4f s\n', N, Re, t_plus)

% ── 7. minus ───────────────────────────────────────────────
tic
for k = 1:Re
    z(:) = x - y;
end
t_minus = toc;
fprintf(' 7. x - y           N=%7d  reps=%d : %.4f s\n', N, Re, t_minus)

% ── 8. times ───────────────────────────────────────────────
tic
for k = 1:Re
    z(:) = x .* y;
end
t_times = toc;
fprintf(' 8. x .* y          N=%7d  reps=%d : %.4f s\n', N, Re, t_times)

% ── 9. rdivide ─────────────────────────────────────────────
tic
for k = 1:Re
    z(:) = x ./ y;
end
t_rdiv = toc;
fprintf(' 9. x ./ y          N=%7d  reps=%d : %.4f s\n', N, Re, t_rdiv)

% ── 10. mtimes ────────────────────────────────────────────
A = randn(Mm, Mm);
B = randn(Mm, Mm);
tic
for k = 1:Rm
    C(:) = A * B;
end
t_mm = toc;
fprintf('10. A * B           %3dx%d   reps=%d : %.4f s\n', Mm, Mm, Rm, t_mm)

% ── 11. fft ───────────────────────────────────────────────
s = randn(Nf, 1);
tic
for k = 1:Rf
    F(:) = fft(s);
end
t_fft = toc;
fprintf('11. fft(s)          N=%7d  reps=%d : %.4f s\n', Nf, Rf, t_fft)

% ── Summary ───────────────────────────────────────────────
total = t_abs + t_sin + t_cos + t_exp + t_log + t_plus + t_minus + ...
        t_times + t_rdiv + t_mm + t_fft;
fprintf('\n── Total: %.4f s ──\n', total)
