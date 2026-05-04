import compat.*

% 2-D anisotropic data along (1, 1) direction
rng(1);
n = 200;
t = linspace(-5, 5, n)';
X = [t + 0.1*randn(n,1), t + 0.1*randn(n,1)];

[coeff, score, latent, ~, explained, mu] = pca(X);
fprintf('--- pca on near-collinear 2-D data ---\n');
fprintf('mu = '); disp(mu);
fprintf('  expect: ~ [0 0]\n');
fprintf('coeff =\n'); disp(coeff);
fprintf('  expect: first column ≈ [0.707; 0.707] or [-0.707;-0.707]\n');
fprintf('latent = '); disp(latent');
fprintf('  expect: first eigenvalue much larger than second\n');
fprintf('explained = '); disp(explained');
fprintf('  expect: ≈ [99.x  0.x] %% — first PC dominates\n\n');

% pcacov
C = cov(X);
[c2, l2, e2] = pcacov(C);
fprintf('--- pcacov(cov(X)) ---\n');
fprintf('coeff = \n'); disp(c2);
fprintf('latent = '); disp(l2');
fprintf('  expect: same as pca() output\n\n');

% pcares — keep 1 PC, residual should be small
res = pcares(X, 1);
fprintf('Residual norm after 1 PC retained: %.4f\n', sqrt(sum(sum(res.^2))));
fprintf('Original norm:                     %.4f\n', sqrt(sum(sum((X - mu).^2))));
fprintf('  expect: residual ≪ original (most variance captured)\n\n');

% Trivial 3-D test
Y = [1 0 0; 2 0 0; 3 0 0; 4 0 0; 5 0 0];   % all along x-axis
[~, ~, ly] = pca(Y);
fprintf('--- pca on x-aligned data: latent = '); disp(ly');
fprintf('  expect: [2.5 0 0] (variance only in first PC)\n');
