% fminsearch_rosenbrock.m
% Nelder-Mead minimization of the classic Rosenbrock banana function:
%   f(x, y) = (1 - x)^2 + 100*(y - x^2)^2
% Global minimum at (1, 1) with f = 0. fminsearch is derivative-free.

import compat.*;

clear
close all

f = @(p) (1 - p(1))^2 + 100 * (p(2) - p(1)^2)^2;

x0 = [-1.2; 1.0];                       % traditional starting point
xopt = fminsearch(f, x0);
fval = f(xopt);

fprintf('Start point :  x0 = [%.4f, %.4f],  f(x0) = %.4f\n', ...
        x0(1), x0(2), f(x0));
fprintf('Solution    :  x  = [%.6f, %.6f]\n', xopt(1), xopt(2));
fprintf('f(x)         =  %.3e\n', fval);

% Visualise the function and the optimum.
[X, Y] = meshgrid(linspace(-1.5, 1.5, 80));
Z = (1 - X).^2 + 100 * (Y - X.^2).^2;

figure;
contourf(X, Y, log10(1 + Z), 20); colorbar;
hold on;
plot(x0(1),  x0(2),  'wo', 'MarkerSize', 10, 'LineWidth', 2);
plot(xopt(1), xopt(2), 'r*', 'MarkerSize', 14, 'LineWidth', 2);
hold off;
xlabel('x'); ylabel('y');
title('Rosenbrock — log_{10}(1+f).  o = start, * = fminsearch optimum');
