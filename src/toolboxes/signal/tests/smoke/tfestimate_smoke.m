clear

import compat.*

% --- tfestimate(x, x) = 1 across all freqs (auto-tf) ---
rng(42);
x = randn(1024, 1);
[Txx, F] = tfestimate(x, x);
fprintf('--- tfestimate(x, x) ---\n');
fprintf('  size = %d (expect 129)\n', numel(Txx));
fprintf('  max|Txx - 1| = %.6e (expect 0)\n', max(abs(Txx - 1)));
fprintf('  max(|imag(Txx)|) = %.6e (expect 0)\n\n', max(abs(imag(Txx))));

% --- tfestimate on LTI system y = filter(h, 1, x) recovers H(f) ---
% h is a low-pass moving average of length 5 → H(f) ≈ |sin(5f/2)/(5sin(f/2))|.
h = ones(1, 5) / 5;
y = filter(h, 1, x);
[Txy, F] = tfestimate(x, y);
% Compare |Txy| with the analytic |H(f)| at each F.
% For h = ones(1,L)/L: |H(jw)| = |sin(L*w/2)/(L*sin(w/2))|.
L = 5;
analytic = zeros(numel(F), 1);
for k = 1:numel(F)
    w = F(k);
    if abs(w) < 1e-12
        analytic(k) = 1.0;
    else
        analytic(k) = abs(sin(L*w/2) / (L*sin(w/2)));
    end
end
err = max(abs(abs(Txy) - analytic));
fprintf('--- tfestimate on FIR LTI ---\n');
fprintf('  max | |Txy| - |H_analytic| | = %.4f\n', err);
fprintf('  expect ≤ 0.2 (Welch finite-segment bias on randn input)\n');
fprintf('  Txy(1) = %.4f + %.4fj (expect ≈ 1 + 0j at DC)\n', ...
    real(Txy(1)), imag(Txy(1)));

% --- grayconnected: flood-fill on a 5x5 with two intensity regions ---
I = uint8([
    10 10 10 50 50;
    10 10 10 50 50;
    10 10 10 50 50;
    50 50 50 50 50;
    50 50 50 50 50]);
% Seed at (1, 1) value=10, default tol=32 ⇒ would also include 50s
% (|50-10| = 40 > 32 — actually NOT included). Use tol=20.
BW = grayconnected(I, 1, 1, 20);
fprintf('\n--- grayconnected on 5x5 with two regions, seed (1,1), tol=20 ---\n');
fprintf('  count(BW) = %d (expect 9 — the 3x3 chunk of 10s)\n', sum(sum(BW)));
fprintf('  BW(1,4) = %d (expect 0, 50 differs from seed by 40)\n', BW(1, 4));
fprintf('  BW(3,3) = %d (expect 1, in 10-region)\n', BW(3, 3));
fprintf('  BW(4,4) = %d (expect 0, 50-region)\n', BW(4, 4));

% Big tol → flood entire image.
BW_all = grayconnected(I, 1, 1, 100);
fprintf('  with tol=100, count = %d (expect 25 — all pixels)\n', ...
    sum(sum(BW_all)));
