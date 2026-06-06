% fminbnd_min1d.m
% Bounded 1-D minimization with fminbnd (golden-section + parabolic
% interpolation). Faster and more robust than fminsearch when you
% have a known interval.

import compat.*;

clear
close all

% A multi-modal function in [0, 6]:
%   f(x) = sin(x) + 0.5 * cos(2x)
% Two minima inside; we go for the one in [3, 5].
f = @(x) sin(x) + 0.5 * cos(2 * x);

xopt = fminbnd(f, 3, 5);
fval = f(xopt);

fprintf('Search interval : [3, 5]\n');
fprintf('Minimum at x   = %.8f\n', xopt);
fprintf('f(x)           = %.8f\n', fval);

xs = linspace(0, 6, 400);
figure;
plot(xs, f(xs), 'b'); grid on;
hold on;
plot([3 3 5 5], [-2 2 2 -2], 'k--');     % search interval markers
plot(xopt, fval, 'r*', 'MarkerSize', 14, 'LineWidth', 2);
hold off;
xlabel('x'); ylabel('f(x)');
title('fminbnd — multi-modal function, bounded search [3, 5]');
legend('f(x) = sin x + 0.5 cos 2x', 'interval', 'minimum', 'location', 'northwest');
