clear
import compat.*
rng(0);

fprintf('=== robustfit — IRLS regression with outliers ===\n');

n = 100;
x = (1:n)';
X = [x, ones(n, 1)];
y = 2 * x + 1 + 0.5 * randn(n, 1);
y(95:100) = y(95:100) + 50;   % 6 high-leverage outliers

% OLS for comparison.
b_ols = regress(y, X, 0.05);
fprintf('\nOLS coefficients (biased by outliers):\n');
fprintf('   slope     = %.4f  (true = 2.0)\n', b_ols(1));
fprintf('   intercept = %.4f  (true = 1.0)\n', b_ols(2));

% Robust.
[b_r, s] = robustfit(X, y);
fprintf('\nRobust (bisquare):\n');
fprintf('   slope     = %.4f\n', b_r(1));
fprintf('   intercept = %.4f\n', b_r(2));
fprintf('   scale s   = %.4f\n', s);

% Huber variant.
[b_h, ~] = robustfit(X, y, 'huber');
fprintf('\nRobust (Huber):\n');
fprintf('   slope     = %.4f\n', b_h(1));
fprintf('   intercept = %.4f\n', b_h(2));


fprintf('\n=== robustcov — trimmed-MCD covariance ===\n');

% Clean 2D Gaussian + outlier cluster.
n2 = 200;
Xc = randn(n2, 2) + [3 5];
Xc(180:200, :) = 20 * randn(21, 2);

[sig, mu] = robustcov(Xc);
fprintf('\nRobust mu:    '); disp(mu);
fprintf('Robust sigma:\n');  disp(sig);

fprintf('Classical mean(X):  '); disp(mean(Xc));
fprintf('Classical cov(X):\n'); disp(cov(Xc));
