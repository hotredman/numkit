% Interp1 Methods — Compare interpolation techniques
% Linear, nearest, spline, and pchip on the same data.
clear
close all

% Sparse samples with a sharp feature
x = [0 1 2 3 3.5 4 5 6 7 8];
y = [0 0.8 0.9 0.2 -0.5 -0.3 0.5 0.9 0.7 0.2];

xq = linspace(0, 8, 200);

y_lin  = interp1(x, y, xq, 'linear');
y_near = interp1(x, y, xq, 'nearest');
y_spl  = interp1(x, y, xq, 'spline');
y_pch  = interp1(x, y, xq, 'pchip');

figure(1)
hold on
scatter(x, y)
plot(xq, y_lin, 'r-', 'LineWidth', 1.5)
plot(xq, y_spl, 'b-', 'LineWidth', 1.5)
plot(xq, y_pch, 'g--', 'LineWidth', 1.5)
hold off
title('interp1: linear vs spline vs pchip')
xlabel('x')
ylabel('y')
legend('data', 'linear', 'spline', 'pchip')
grid on

figure(2)
hold on
scatter(x, y)
plot(xq, y_near, 'm-', 'LineWidth', 1.5)
plot(xq, y_lin, 'r-', 'LineWidth', 1.5)
hold off
title('interp1: nearest vs linear')
xlabel('x')
ylabel('y')
legend('data', 'nearest', 'linear')
grid on
