% Polyfit Demo — Polynomial curve fitting
% Fit noisy data with polynomials of different degrees
% and plot the results using polyval.
clear
close all

% Noisy samples of a sine-like curve
x = linspace(0, 2*pi, 20);
y = sin(x) + 0.2 * randn(1, 20);

% Evaluation grid
xq = linspace(0, 2*pi, 200);

% Fit degree 3 and degree 6
p3 = polyfit(x, y, 3);
p6 = polyfit(x, y, 6);

figure
hold on
scatter(x, y)
plot(xq, polyval(p3, xq), 'r-', 'LineWidth', 1.5)
plot(xq, polyval(p6, xq), 'g-', 'LineWidth', 1.5)
hold off
title('Polynomial Fit (degree 3 vs 6)')
xlabel('x')
ylabel('y')
legend('data', 'deg 3', 'deg 6')
grid on
