clear
import compat.*
rng(0);

fprintf('=== glmfit / glmval — logistic regression ===\n');
n = 500;
x = randn(n, 1) * 2;
% True logistic: P(y=1 | x) = sigmoid(0.5 + 1.5 · x)
eta_true = 0.5 + 1.5 * x;
p_true = 1 ./ (1 + exp(-eta_true));
y = double(rand(n, 1) < p_true);

[b, dev] = glmfit(x, y, 'binomial');
fprintf('\nbeta = '); disp(b');
fprintf('true [0.5; 1.5]\n');
fprintf('deviance = %.4f\n', dev);

% Predict over a grid.
xq = (-3:0.5:3)';
yhat = glmval(b, xq, 'logit');
fprintf('\nPredicted probabilities:\n');
for i = 1:numel(xq)
    fprintf('  x = %4.1f → p = %.4f\n', xq(i), yhat(i));
end


fprintf('\n=== Poisson regression ===\n');
% Deterministic exact-mean Poisson surrogate (vector poissrnd not yet
% available in numkit's stats library — KNOWN GAP in poissrnd).
x2 = linspace(-1, 1, 200)';
lambda = exp(0.7 + 0.5 * x2);
y2 = round(lambda);
[b2, dev2] = glmfit(x2, y2, 'poisson');
fprintf('beta = '); disp(b2');
fprintf('true [0.7; 0.5]\n');
fprintf('deviance = %.4f\n', dev2);


fprintf('\n=== Normal GLM ≡ OLS ===\n');
x3 = randn(n, 1);
y3 = 2.0 + 1.5 * x3 + 0.3 * randn(n, 1);
[b3, ~] = glmfit(x3, y3, 'normal');
fprintf('GLM beta = '); disp(b3');
X = [ones(n, 1), x3];
fprintf('OLS beta = '); disp(regress(y3, X, 0.05)');
