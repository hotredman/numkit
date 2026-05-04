import compat.*

% Identity case: A == B
A = [1 2 3; 4 5 6; 7 8 9];
fprintf('--- immse(A, A) = %.4f (expect 0) ---\n', immse(A, A));
fprintf('--- psnr(A, A) = %.4f (expect Inf) ---\n', psnr(A, A));
fprintf('--- ssim(A, A) = %.4f (expect 1.0) ---\n\n', ssim(A, A));

% Small perturbation
B = A + 0.5;
fprintf('--- immse(A, B=A+0.5) = %.4f (expect 0.25) ---\n', immse(A, B));
fprintf('--- psnr(A, B, peak=10) = %.4f (expect 26.02) ---\n', psnr(A, B, 10));

% Larger perturbation
C = A + 5;
fprintf('--- psnr(A, C=A+5, peak=10) = %.4f (expect 6.02) ---\n', psnr(A, C, 10));

% SSIM on a noisy version
rng(42);
[X, Y] = meshgrid(1:32, 1:32);
big = double(uint8(128 + 50*sin(2*pi*X/16) + 30*cos(2*pi*Y/16)));
noisy = big + 5 * randn(32, 32);
fprintf('--- ssim(big, noisy) = %.4f ---\n', ssim(uint8(big), uint8(noisy)));
fprintf('  expect: high (≥ 0.85) since noise is small relative to signal\n');
