clear

fprintf('=== partialcorri — semi-partial correlation ===\n');

rng(0);
n = 500;
X = randn(n, 3);
% Y depends only on X(:, 1) and X(:, 2) — column 3 is just noise.
Y = [X(:, 1) + 0.05 * randn(n, 1), ...
     X(:, 2) + 0.05 * randn(n, 1)];

R = partialcorri(Y, X);
fprintf('\npartialcorri(Y, X), pY×pX = %d×%d:\n', size(R, 1), size(R, 2));
disp(R);
fprintf('  R(1, 1), R(2, 2) ≈ 1 — Y(:, i) depends on X(:, i)\n');
fprintf('  R(*, 3), off-diagonal ≈ 0 — no dependency\n');

% With extra controls.
Z = randn(n, 2);
Rz = partialcorri(Y, X, Z);
fprintf('\npartialcorri(Y, X, Z):\n');
disp(Rz);

fprintf('\n=== canoncorr — canonical correlation analysis ===\n');

rng(0);
n = 1000;
% Both X and Y share a latent factor z.
z = randn(n, 1);
X = [z + 0.05 * randn(n, 1), z + 0.10 * randn(n, 1), randn(n, 1)];
Y = [z + 0.07 * randn(n, 1), -z + 0.10 * randn(n, 1)];

[A, B, r] = canoncorr(X, Y);
fprintf('canonical correlations r ='); disp(r');
fprintf('  expect r(1) ≈ 1 (shared latent), r(2) small\n');
fprintf('size(A) = [%d, %d];   size(B) = [%d, %d]\n', ...
        size(A, 1), size(A, 2), size(B, 1), size(B, 2));

% Cross-check.
U = X * A;
V = Y * B;
fprintf('\nVerify corr(U(:, i), V(:, i)) == r(i):\n');
for i = 1:numel(r)
    c = corrcoef([U(:, i), V(:, i)]);
    fprintf('  corr(U, V) at i=%d: %.6f   r(%d) = %.6f\n', i, c(1, 2), i, r(i));
end
