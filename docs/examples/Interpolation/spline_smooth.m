% Spline Smooth — Smooth interpolation of sparse data
% Uses spline() to create a smooth curve through sample points.
clear
close all

x = 0:0.5:4;
y = [0 1.2 1.8 0.9 0.1 -0.5 0.2 1.0 1.5];

xq = linspace(0, 4, 300);
yq = spline(x, y, xq);

figure
hold on
scatter(x, y)
plot(xq, yq, 'r-', 'LineWidth', 1.5)
hold off
title('Cubic Spline Interpolation')
xlabel('x')
ylabel('y')
legend('samples', 'spline')
grid on
