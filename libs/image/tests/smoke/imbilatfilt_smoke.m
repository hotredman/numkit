clear

import compat.*

% --- Smooths flat regions, preserves a strong step edge ---
% 16-pixel vertical step: left half = 0.2, right half = 0.8.
% Bilateral (with dos < step²) should keep the edge sharp while
% averaging within each half.
H = 8; W = 16;
I = [0.2 * ones(H, W/2) , 0.8 * ones(H, W/2)];
% Add small uniform noise [-0.05, +0.05] within each half.
rng(7);
N = 0.10 * (rand(H, W) - 0.5);
J0 = I + N;

B = imbilatfilt(J0, 0.001, 1);   % small dos: tight range → preserve edge

% Edge sharpness check: B(:, W/2) and B(:, W/2+1) should still be far apart.
edge_left  = mean(B(:, W/2));
edge_right = mean(B(:, W/2 + 1));
fprintf('--- imbilatfilt edge preservation ---\n');
fprintf('  raw step:        0.20 → 0.80 (Δ = 0.60)\n');
fprintf('  noisy mean:      %.3f → %.3f (Δ = %.3f)\n', ...
    mean(J0(:, W/2)), mean(J0(:, W/2 + 1)), ...
    mean(J0(:, W/2 + 1)) - mean(J0(:, W/2)));
fprintf('  bilateral mean:  %.3f → %.3f (Δ = %.3f, expect ≈ 0.60)\n', ...
    edge_left, edge_right, edge_right - edge_left);

% Inside-half variance should drop substantially.
var_left_raw = var(J0(:, 1:W/2 - 1)(:));
var_left_blt = var(B(:, 1:W/2 - 1)(:));
fprintf('  left-half var: raw=%.5f bilateral=%.5f (expect ≪ raw)\n\n', ...
    var_left_raw, var_left_blt);

% --- Large dos = pure spatial Gaussian (range insensitive) ---
% With dos huge, bilateral should approach a pure Gaussian blur.
B_huge = imbilatfilt(J0, 1e6, 1);
G      = imgaussfilt(J0, 1);
fprintf('--- huge dos approaches pure Gaussian blur ---\n');
fprintf('  max|imbilatfilt(I, ∞, σ) − imgaussfilt(I, σ)| = %.4e (expect ~0)\n\n', ...
    max(max(abs(B_huge - G))));

% --- Identity-ish: tiny window via small spatialSigma ---
% spatialSigma → small means very local average; for σ=0.5 the window
% is 2*ceil(2*0.5)+1 = 3, so it's still a 3×3 op. We just check it
% doesn't explode.
B_small = imbilatfilt(J0, 0.01, 0.5);
fprintf('--- tiny spatialSigma sanity ---\n');
fprintf('  size(B_small) = [%d %d] (expect [%d %d])\n', ...
    size(B_small, 1), size(B_small, 2), H, W);
fprintf('  max(B_small(:)) = %.3f, min = %.3f (expect ~[0.15, 0.85])\n', ...
    max(B_small(:)), min(B_small(:)));

% --- Bad dos raises ---
ok = false;
try
    imbilatfilt(J0, -0.01);
catch
    ok = true;
end
fprintf('  negative dos raises = %d (expect 1)\n', ok);
