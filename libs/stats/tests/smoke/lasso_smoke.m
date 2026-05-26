clear
import compat.*
rng(0);

fprintf('=== lasso — sparse signal recovery ===\n');

% True signal: only X1 and X3 contribute.
n = 200;
p = 5;
X = randn(n, p);
beta_true = [2.0; 0.0; 3.0; 0.0; 0.0];
y = X * beta_true + 0.3 * randn(n, 1);

% Lambda path.
lambdas = [0, 0.01, 0.05, 0.1, 0.5];
[B, intercept, lam] = lasso(X, y, lambdas, 1.0);
fprintf('\nCoefficient path (columns = increasing λ):\n');
disp(B);
fprintf('λ:      '); disp(lam);
fprintf('truth:  '); disp(beta_true');
fprintf('(noise columns 2, 4, 5 → exactly zero at λ ≥ 0.05)\n');

fprintf('\n=== Elastic net (α = 0.5) ===\n');
[B2, ~, ~] = lasso(X, y, 0.1, 0.5);
fprintf('coefs:  '); disp(B2');
fprintf('(L2 component → no exact zeros, all coefs shrunk)\n');


fprintf('\n=== lassoglm — logistic ===\n');
n2 = 500;
X2 = randn(n2, 4);
eta = 0.5 * X2(:, 1) + 1.5 * X2(:, 3);   % only X1, X3 matter
y2 = double(rand(n2, 1) < 1 ./ (1 + exp(-eta)));

[B3, intercept3, ~] = lassoglm(X2, y2, 'binomial', [0.001, 0.05, 0.2], 1.0);
fprintf('coefficients per λ:\n');
disp(B3);
fprintf('intercepts: '); disp(intercept3);
fprintf('(true ≈ [0.5, 0, 1.5, 0])\n');
