clear
import compat.*

fprintf('=== nlinfit family — nonlinear LS + confidence intervals ===\n');
rng(0);

% Exponential decay: y = a * exp(-b * x).
fprintf('\nModel: y = a · exp(-b · x), true (a, b) = (2.0, 0.5)\n');
x = linspace(0, 5, 50)';
beta_true = [2.0; 0.5];
y = beta_true(1) * exp(-beta_true(2) * x) + 0.05 * randn(size(x));
fun = @(b, x) b(1) * exp(-b(2) * x);

[beta, R, J, CovB, MSE] = nlinfit(x, y, fun, [1.0; 1.0]);
fprintf('  beta = '); disp(beta');
fprintf('  MSE  = %.6f\n', MSE);
fprintf('  ||R|| = %.4f\n', norm(R));

% Parameter CIs.
ci = nlparci(beta, R, J);
fprintf('\n95%% parameter CIs:\n');
for i = 1:numel(beta)
    fprintf('  beta(%d) ∈ [%.4f, %.4f]   (true = %.4f)\n', ...
            i, ci(i, 1), ci(i, 2), beta_true(i));
end

% Prediction CIs.
xq = (0:0.5:5)';
[yp, delta] = nlpredci(fun, xq, beta, R, J, 0.05);
fprintf('\nPredictions ± 95%% delta:\n');
for i = 1:numel(xq)
    fprintf('  x = %.2f:  %.4f ± %.4f\n', xq(i), yp(i), delta(i));
end


fprintf('\n--- Linear model (sanity check) ---\n');
x2 = linspace(0, 1, 100)';
y2 = 2 * x2 + 1 + 0.05 * randn(size(x2));
fun2 = @(b, x) b(1) * x + b(2);
[b2, R2, J2] = nlinfit(x2, y2, fun2, [0.5; 0.5]);
fprintf('  beta = '); disp(b2');
fprintf('  (true slope=2, intercept=1)\n');
