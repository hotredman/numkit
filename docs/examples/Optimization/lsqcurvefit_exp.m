% lsqcurvefit_exp.m
% Nonlinear least-squares curve fit: recover the parameters of
%   y(t) = a * exp(-b * t) + c
% from noisy samples. Demonstrates fminsearch driven by the residual
% sum-of-squares — the hand-rolled equivalent of lsqcurvefit.

import compat.*;

clear
close all

% Ground truth.
true_p = [2.5, 0.8, 0.2];
model = @(p, t) p(1) * exp(-p(2) * t) + p(3);

% Synthesize noisy data.
rng(0);
t = linspace(0, 5, 60)';
y_clean = model(true_p, t);
y = y_clean + 0.08 * randn(size(t));

% Fit by minimising the residual sum-of-squares with Nelder-Mead.
ssr = @(p) sum((y - model(p, t)).^2);

p0    = [1.0, 1.0, 0.0];
p_fit = fminsearch(ssr, p0);

fprintf('True params  = [%.3f, %.3f, %.3f]\n', true_p);
fprintf('Init guess  = [%.3f, %.3f, %.3f]\n', p0);
fprintf('Fitted     = [%.3f, %.3f, %.3f]\n', p_fit);

resid = y - model(p_fit, t);
fprintf('Residual norm = %.4f\n', norm(resid));

figure;
plot(t, y, 'bo', t, model(true_p, t), 'k--', t, model(p_fit, t), 'r-', ...
     'LineWidth', 1.2, 'MarkerSize', 5);
grid on;
xlabel('t'); ylabel('y');
legend('noisy data', 'ground truth', 'fminsearch fit', 'location', 'northeast');
title('Nonlinear least-squares fit of  y = a e^{-bt} + c');
