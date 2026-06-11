clear

import compat.*

% --- Constant image: T = constant intensity (the local mean is the image itself) ---
I = uint8(100 * ones(20, 20));
T = adaptthresh(I, 0.5);   % default neighborhood
fprintf('--- adaptthresh on uint8 const=100 ---\n');
fprintf('  size = %dx%d\n', size(T, 1), size(T, 2));
fprintf('  mean(T) = %.4f (expect ≈ 100/255 = 0.3922)\n', mean(T(:)));
fprintf('  max(T) - min(T) = %.6e (expect ~ 0 — constant image)\n\n', ...
    max(T(:)) - min(T(:)));

% --- Bright image: T should be > 0.5 in bright region, < 0.5 in dark ---
I = uint8(zeros(40, 40));
I(:, 1:20) = 50;     % dark left half
I(:, 21:40) = 200;   % bright right half
T = adaptthresh(I, 0.5);
fprintf('--- adaptthresh on half-half (dark/bright) ---\n');
fprintf('  T(20, 5)  = %.4f (left/dark, expect ≈ 50/255 = 0.196)\n', T(20, 5));
fprintf('  T(20, 35) = %.4f (right/bright, expect ≈ 200/255 = 0.784)\n', T(20, 35));
fprintf('  T(20, 20) = %.4f (border, expect intermediate)\n\n', T(20, 20));

% --- Sensitivity: higher sens → lower threshold (more foreground) ---
T_lo = adaptthresh(I, 0.1);   % low sens → higher threshold
T_hi = adaptthresh(I, 0.9);   % high sens → lower threshold
fprintf('--- sensitivity sweep ---\n');
fprintf('  sens=0.1: mean(T) = %.4f\n', mean(T_lo(:)));
fprintf('  sens=0.5: mean(T) = %.4f\n', mean(T(:)));
fprintf('  sens=0.9: mean(T) = %.4f\n', mean(T_hi(:)));
fprintf('  expect monotonic: 0.1 → highest, 0.9 → lowest\n\n');

% --- Pixel-wise threshold check on a synthetic gradient ---
[X, ~] = meshgrid(1:50, 1:50);
img = uint8(50 + 4 * X);   % gradient brighter to the right
T = adaptthresh(img, 0.5);
fprintf('--- adaptthresh on gradient ---\n');
fprintf('  T(25, 5)  = %.4f (left, dim)\n', T(25, 5));
fprintf('  T(25, 45) = %.4f (right, bright — should be larger)\n', T(25, 45));
fprintf('  monotonic L→R: %d (expect 1)\n', T(25, 45) > T(25, 5));

% --- Gaussian variant ---
Tg = adaptthresh(I, 0.5, 0, 'gaussian');
fprintf('\n--- gaussian variant ---\n');
fprintf('  size(Tg) = %dx%d\n', size(Tg, 1), size(Tg, 2));
fprintf('  mean(Tg) = %.4f (similar to mean version)\n', mean(Tg(:)));
