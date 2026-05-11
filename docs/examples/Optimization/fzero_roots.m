% fzero_roots.m
% Find scalar roots of three different functions with fzero, then
% overlay the result on a plot of each function.

import compat.*;

clear
close all

% Three test functions with known roots:
f1 = @(x) cos(x) - x;            % root ≈ 0.7390851332 (Dottie number)
f2 = @(x) x.^3 - 2*x - 5;        % real root ≈ 2.0945514815
f3 = @(x) sin(2*x) .* exp(-x/3); % infinitely many; bracket near pi/2

x1 = fzero(f1, 0.5);
x2 = fzero(f2, 2.0);
x3 = fzero(f3, [1.0  2.5]);      % bracketed search

fprintf('cos(x) - x       root = %.10f\n', x1);
fprintf('x^3 - 2x - 5     root = %.10f\n', x2);
fprintf('sin(2x)*exp(-x/3) root in [1, 2.5] = %.10f\n', x3);

xs = linspace(-2, 4, 600);
figure;
plot(xs, f1(xs), 'b', xs, f2(xs)/10, 'g', xs, f3(xs), 'r');
hold on;
yline(0, 'k--');
plot(x1, 0, 'bo', x2, 0, 'go', x3, 0, 'ro', 'MarkerSize', 10, 'LineWidth', 2);
hold off;
legend('cos(x)-x', '(x^3-2x-5)/10', 'sin(2x)e^{-x/3}', 'y=0', 'location', 'south');
grid on;
title('fzero on three test functions');
xlabel('x'); ylabel('f(x)');
