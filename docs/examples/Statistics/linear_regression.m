% linear_regression.m
% Simple-linear-regression workflow:
%   1. Generate noisy y = 2.5 * x + 1 + N(0, 0.6)
%   2. Fit with polyfit (degree 1)
%   3. Report slope, intercept, R^2
%   4. Plot data + fit line + 95% confidence band

import compat.*;

clear
close all

rng(0);
N = 60;
x = sort(10 * rand(N, 1));
y_true = 2.5 * x + 1.0;
y = y_true + 0.6 * randn(N, 1);

% Fit y ≈ p(1)*x + p(2).
p = polyfit(x, y, 1);
y_fit = polyval(p, x);

% R^2 = 1 - SSres/SStot.
ss_res = sum((y - y_fit).^2);
ss_tot = sum((y - mean(y)).^2);
r2 = 1 - ss_res / ss_tot;

fprintf('True slope     = 2.5,    fitted = %.4f\n', p(1));
fprintf('True intercept = 1.0,    fitted = %.4f\n', p(2));
fprintf('R^2 (coefficient of determination) = %.4f\n', r2);

% Crude confidence band: residual std × 1.96.
s_res = sqrt(ss_res / (N - 2));
band  = 1.96 * s_res;

xfine = linspace(min(x), max(x), 100);
yfine = polyval(p, xfine);

figure;
plot(x, y, 'bo', 'MarkerSize', 5);
hold on;
plot(xfine, yfine, 'r-', 'LineWidth', 1.5);
plot(xfine, yfine + band, 'r--', xfine, yfine - band, 'r--');
hold off;
grid on;
xlabel('x'); ylabel('y');
legend('data', sprintf('fit: y = %.3f x + %.3f', p(1), p(2)), '\pm 1.96 \sigma_{res}', ...
       'location', 'northwest');
title(sprintf('Simple linear regression — R^2 = %.3f', r2));
